#pragma once

/**
 * @file config.h
 * @brief Compile-time constants, version information, and platform-specific configurations.
 */

namespace vector::native {

[[nodiscard]] constexpr bool IsDebugBuild() {
#ifdef NDEBUG
    return false;
#else
    return true;
#endif
}

/// A compile-time constant indicating if this is a debug build.
inline constexpr bool kIsDebugBuild = IsDebugBuild();

/**
 * @def LP_SELECT(lp32, lp64)
 * @brief A preprocessor macro to select a value based on the architecture.
 * @param lp32 The value to use on 32-bit platforms.
 * @param lp64 The value to use on 64-bit platforms.
 */
#if defined(__LP64__)
#define LP_SELECT(lp32, lp64) lp64
#else
#define LP_SELECT(lp32, lp64) lp32
#endif

/// The filename of the core Android Runtime (ART) library.
inline constexpr auto kArtLibraryName = "libart.so";

/// The filename of the Android Binder library.
inline constexpr auto kBinderLibraryName = "libbinder.so";

/// The filename of the Android Framework library.
inline constexpr auto kFrameworkLibraryName = "libandroidfw.so";

/// The path to the dynamic linker.
inline constexpr auto kLinkerPath = "/linker";

/// The version code of the library, populated by the build system.
const int kVersionCode = VERSION_CODE;

// The build system hands the version over as a bare token (`-DVERSION_NAME=2.2`) instead of as a
// string literal, because the quoting that would make it a literal does not survive every path
// from Gradle to the compiler: POSIX ninja runs each command through a shell that strips the
// quotes, while Windows ninja execs the compiler directly and the quotes reach the preprocessor,
// where `'"2.2"'` becomes a multi-character constant. Stringifying here is equivalent for both.
#define VECTOR_STRINGIFY_IMPL(x) #x
#define VECTOR_STRINGIFY(x) VECTOR_STRINGIFY_IMPL(x)

/// The version name of the library, populated by the build system.
const char *const kVersionName = VECTOR_STRINGIFY(VERSION_NAME);

}  // namespace vector::native
