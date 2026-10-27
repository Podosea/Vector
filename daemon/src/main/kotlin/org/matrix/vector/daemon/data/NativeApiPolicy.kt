package org.matrix.vector.daemon.data

import java.nio.file.Files
import java.nio.file.Path

/**
 * The daemon's per-process native API policy, read from a root-only config file.
 *
 * The native API installs a Dobby inline hook on the linker's `do_dlopen` the first time a module
 * registers a native library (see the daemon's native counterpart, `native_api.cpp`), and that
 * hook then runs inside every dlopen of the host process. One hardened pack's loader interacting
 * badly with that hook breaks the whole host, so the decision must be reachable without touching
 * app-visible state: this file lives under `/data/adb/lspd/config/`, which plain apps cannot read,
 * and the decision travels to the child in the GET_BINDER reply instead of any system property -
 * a persistent prop is a fingerprint surface and is process-global, neither of which belongs in a
 * durable control.
 *
 * File: `/data/adb/lspd/config/native_api.conf`. One rule per line, `#` comments, blank lines
 * ignored:
 *
 * ```
 * # <mode> [pattern]
 * off com.qidian.*        # hardened pack: no hook at all in any of its processes
 * hook_only com.example   # install the hook, never enter module code
 * full                    # bare mode: applies to everything the earlier rules missed
 * ```
 *
 * `mode` is one of `full`, `hook_only`, `off`; a mode alone is a catch-all. A pattern matches the
 * process name exactly, or as a prefix when it ends with `*`. First matching rule wins, so put
 * specific rules before catch-alls. A missing or unreadable file, or no matching rule, decides
 * nothing and leaves the choice to the native side's fallback (the diagnostic
 * `persist.leaf.native_api` property, then the `full` default).
 *
 * The file is read once per attach rather than cached: attachProcess runs once per process start,
 * the file is a few lines on local storage, and reading per call makes rule edits live without a
 * daemon restart or a reload path to maintain.
 */
object NativeApiPolicy {
  /** Wire values carried in the GET_BINDER reply; mirrored by the constants in native_api.cpp. */
  const val MODE_UNDECIDED = 0
  const val MODE_FULL = 1
  const val MODE_HOOK_ONLY = 2
  const val MODE_OFF = 3

  private val MODES = mapOf("full" to MODE_FULL, "hook_only" to MODE_HOOK_ONLY, "off" to MODE_OFF)

  private val confPath: Path = FileSystem.configDirPath.resolve("native_api.conf")

  /** The mode for [processName], or [MODE_UNDECIDED] when this file decides nothing. */
  fun modeFor(processName: String): Int =
      runCatching {
            if (!Files.exists(confPath)) return MODE_UNDECIDED
            Files.newBufferedReader(confPath).useLines { lines ->
              for (raw in lines) {
                val line = raw.substringBefore('#').trim()
                if (line.isEmpty()) continue
                val parts = line.split(Regex("\\s+"))
                val mode = MODES[parts[0].lowercase()] ?: continue
                // An unreadable mode word is a config typo, not a decision; skip the rule.
                val pattern = parts.getOrNull(1) ?: "*" // a bare mode is a catch-all
                if (matches(pattern, processName)) return mode
              }
            }
            MODE_UNDECIDED
          }
          .getOrDefault(MODE_UNDECIDED)

  private fun matches(pattern: String, processName: String): Boolean =
      if (pattern.endsWith("*")) processName.startsWith(pattern.dropLast(1))
      else processName == pattern
}
