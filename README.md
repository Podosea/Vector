# Leaf Core

A Zygisk module providing an ART hooking framework that keeps API compatibility with the
original Xposed API.

## Requirements

- Android 8.1 or later
- A root solution with Zygisk support (Magisk or KernelSU with a Zygisk implementation)

## Installation

1. Flash the release zip in the root manager and reboot.
2. The manager ships inside the module directory and is loaded from there; no launcher icon is
   installed on purpose, so the manager does not appear in the app list.
3. Modules that target the Xposed API work unchanged.

## This build

Private build. No update source is configured — newer builds are flashed manually. The
metadata that upstream builds publish (update channel, badge links, activity feed) is
intentionally absent.

## License

GPL-3.0. Derived from a GPL-3.0 project; the full license text is in `LICENSE` in the source
tree.
