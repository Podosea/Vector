#include "core/native_api.h"

#include <sys/mman.h>
#include <sys/system_properties.h>

#include <algorithm>
#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <mutex>

#include "common/logging.h"
#include "elf/elf_image.h"
#include "elf/symbol_cache.h"

/**
 * @file native_api.cpp
 * @brief Implementation of the native module loading and API provisioning system.
 */

using lsplant::operator""_sym;
/*
 * ===========================================================================================
 * LSPLANT HOOKING DSL (DOMAIN SPECIFIC LANGUAGE) DOCUMENTATION
 * ===========================================================================================
 *
 * This source file utilizes the 'lsplant' library, which implements a C++20 Hooking DSL.
 * Unlike traditional C-style hooking (which relies on void* casting, manual trampolines,
 * and global function pointers), this DSL uses compile-time metaprogramming to ensure
 * type safety and encapsulate hooking logic.
 *
 * -------------------------------------------------------------------------------------------
 * 1. SYNTAX ANATOMY
 * -------------------------------------------------------------------------------------------
 * The hooking syntax follows this pattern:
 *    "SYMBOL_NAME"_sym .hook ->* [] <auto backup> (args...) { ...body... };
 *
 *  A. "SYMBOL_NAME"_sym
 *     - This is a C++ User-Defined Literal (UDL). It converts the string literal into a
 *       compile-time 'Symbol' type.
 *     - For C++ mangled names (common in Android system libs), you must provide the full
 *       mangled signature (e.g., "__dl__Z9do_dlopen...").
 *
 *  B. Multi-Architecture Support (| Operator)
 *     - Android often requires different symbol names for 32-bit (ARM) and 64-bit (ARM64).
 *     - The DSL supports the pipe operator '|' to select the correct symbol at compile time:
 *       ("Sym32"_sym | "Sym64"_sym)
 *
 *  C. .hook ->*
 *     - '.hook' accesses the hook injection mechanism.
 *     - '->*' (Member Pointer Operator) is overloaded to bind the symbol to the lambda.
 *
 *  D. The Template Lambda (The Replacement)
 *     - Syntax: [] <lsplant::Backup auto backup> (Type arg1, Type arg2...) { ... }
 *     - This is a C++20 Template Lambda.
 *     - 'backup': Represents the ORIGINAL function (trampoline).
 *                 You call this to execute the original system logic.
 *     - 'args...': Must match the signature of the target function exactly.
 *
 * -------------------------------------------------------------------------------------------
 * 2. EXAMPLE USAGE
 * -------------------------------------------------------------------------------------------
 * inline static auto my_hook =
 *     "__open"_sym.hook ->* []<auto backup>(const char* path, int flags) {
 *         // 1. Pre-processing (Before original)
 *         LOGD("Opening file: %s", path);
 *
 *         // 2. Call Original (The "Backup")
 *         int result = backup(path, flags);
 *
 *         // 3. Post-processing (After original)
 *         return result;
 *     };
 *
 * -------------------------------------------------------------------------------------------
 * 3. REGISTRATION
 * -------------------------------------------------------------------------------------------
 * Defining the hook variable does not apply it.
 * You must pass the variable to the HookHandler to modify memory: handler(my_hook).
 * ===========================================================================================
 */

namespace vector::native {

namespace {
// Selects how much of the native API is active in this process. The API installs a Dobby inline
// hook on the linker's `do_dlopen` the first time a module registers a native library, and that
// hook runs inside every dlopen of the process, so one module can change loader behaviour for
// every other library the app loads. Two delivery paths, checked in this order:
//
// 1. daemon-delivered decision (the durable, root-only path). The daemon reads
//    /data/adb/lspd/config/native_api.conf and answers the specialization-time GET_BINDER
//    handshake with a per-process mode, which lands here via SetDeliveredMode() before any module
//    code runs. No persistent system property, nothing a plain app can read, and per-package
//    granularity, which is what the harden-pack degradation needs.
// 2. `persist.leaf.native_api` (the diagnostic path, from the native-api-scope bisection build).
//    Read last, so the durable config outranks it; still useful because setprop takes effect
//    without a rebuild or reinstall. Kept for bisection only - it is a fingerprint surface.
//
//   full (default) - call `native_init` and dispatch module callbacks
//   hook_only      - install the hook, never enter module code (isolates the patch itself)
//   off            - never install the hook; modules keep their Java hooks only
enum class NativeApiMode { kFull, kHookOnly, kOff };

// Wire values sent by the daemon in the GET_BINDER reply (see NativeApiPolicy.kt). 0 means "no
// decision" - it is what an absent reply field or an unreadable conf resolves to, so an old
// system_server path that writes nothing after the binder degrades to the prop fallback instead
// of to a silently wrong mode.
constexpr int kModeUndecided = 0;
constexpr int kModeFull = 1;
constexpr int kModeHookOnly = 2;
constexpr int kModeOff = 3;

// The daemon's decision for this process, or kModeUndecided. Set once during preAppSpecialize,
// long before any module can call RegisterNativeLib, so no synchronization beyond atomic is
// needed and it is never written again.
std::atomic<int> g_delivered_mode{kModeUndecided};
}  // namespace

void SetDeliveredMode(int mode) {
    g_delivered_mode.store(mode, std::memory_order_relaxed);
    LOGD("Native API mode delivered by daemon: {}", mode);
}

namespace {
NativeApiMode ResolveNativeApiMode() {
    switch (g_delivered_mode.load(std::memory_order_relaxed)) {
        case kModeOff:
            return NativeApiMode::kOff;
        case kModeHookOnly:
            return NativeApiMode::kHookOnly;
        case kModeFull:
            return NativeApiMode::kFull;
        default:
            break;
    }
    char value[PROP_VALUE_MAX] = {};
    const int len = __system_property_get("persist.leaf.native_api", value);
    if (len <= 0) return NativeApiMode::kFull;
    const std::string mode(value, static_cast<size_t>(len));
    if (mode == "off") return NativeApiMode::kOff;
    if (mode == "hook_only") return NativeApiMode::kHookOnly;
    return NativeApiMode::kFull;
}

NativeApiMode ActiveNativeApiMode() {
    static const NativeApiMode mode = ResolveNativeApiMode();
    return mode;
}

// Mutex to protect access to the global module lists.
std::mutex g_module_registry_mutex;
// List of callback functions provided by loaded native modules.
std::list<NativeOnModuleLoaded> g_module_loaded_callbacks;
// List of native library filenames that are registered as modules.
std::list<std::string> g_module_native_libs;

// A smart pointer to a memory page that will hold the NativeAPIEntries struct.
std::unique_ptr<void, std::function<void(void *)>> g_api_page(
    mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0), [](void *ptr) {
        if (ptr != MAP_FAILED) {
            munmap(ptr, 4096);
        }
    });
}  // namespace

// The read-only, statically available Native API entry points for modules.
const NativeAPIEntries *g_native_api_entries = nullptr;

/**
 * @brief Initializes the Native API entries struct and makes it read-only.
 */
void InitializeApiEntries() {
    if (g_api_page.get() == MAP_FAILED) {
        LOGF("Failed to allocate memory for native API entries.");
        LOGD("Release the memory page pointer %p", g_api_page.release());
        return;
    }
    auto *entries = new (g_api_page.get()) NativeAPIEntries{
        .version = 2,
        .hookFunc = &HookInline,
        .unhookFunc = &UnhookInline,
    };
    if (mprotect(g_api_page.get(), 4096, PROT_READ) != 0) {
        PLOGE("Failed to mprotect API page to read-only");
    }
    g_native_api_entries = entries;
    LOGI("Native API entries initialized and protected.");
}

void RegisterNativeLib(const std::string &library_name) {
    if (ActiveNativeApiMode() == NativeApiMode::kOff) {
        LOGW("Native API disabled (daemon conf or persist.leaf.native_api=off), ignoring '{}'.",
             library_name.c_str());
        return;
    }
    static bool is_initialized = []() {
        InitializeApiEntries();
        return InstallNativeAPI(lsplant::InitInfo{
            .inline_hooker =
                [](void *target, void *replacement) {
                    void *backup = nullptr;
                    return HookInline(target, replacement, &backup) == 0 ? backup : nullptr;
                },
            .art_symbol_resolver =
                [](auto symbol) { return ElfSymbolCache::GetLinker()->getSymbAddress(symbol); },
        });
    }();

    if (!is_initialized) {
        LOGE("Cannot register module '{}' because native API failed to initialize.",
             library_name.c_str());
        return;
    }

    std::lock_guard<std::mutex> lock(g_module_registry_mutex);
    // The list is walked on every dlopen in the process and never shrinks - there is no
    // unregistration, and hot reload records a module's names again for each new generation - so
    // without this it grows without bound and every dlopen pays for the duplicates.
    if (std::find(g_module_native_libs.begin(), g_module_native_libs.end(), library_name) !=
        g_module_native_libs.end()) {
        LOGD("Native module library '{}' is already registered.", library_name.c_str());
        return;
    }
    g_module_native_libs.push_back(library_name);
    LOGD("Native module library '{}' has been registered.", library_name.c_str());
}

bool HasEnding(std::string_view fullString, std::string_view ending) {
    if (fullString.length() >= ending.length()) {
        return (fullString.compare(fullString.length() - ending.length(), std::string_view::npos,
                                   ending) == 0);
    }
    return false;
}

inline static auto do_dlopen_hook =
    "__dl__Z9do_dlopenPKciPK17android_dlextinfoPKv"_sym.hook->*
    []<lsplant::Backup auto backup>(const char *name, int flags, const void *extinfo,
                                    const void *caller_addr) static -> void * {
    void *handle = backup(name, flags, extinfo, caller_addr);
    const std::string lib_name = (name != nullptr) ? name : "null";
    LOGV("do_dlopen hook triggered for library: '{}'", lib_name.c_str());

    if (handle == nullptr) return nullptr;
    // Passthrough-only mode: the hook stays installed, module code is never entered.
    if (ActiveNativeApiMode() == NativeApiMode::kHookOnly) return handle;

    std::lock_guard<std::mutex> lock(g_module_registry_mutex);

    for (std::string_view module_lib : g_module_native_libs) {
        if (HasEnding(lib_name, module_lib)) {
            LOGI("Detected registered native module being loaded: '{}'", lib_name.c_str());
            void *init_sym = dlsym(handle, "native_init");
            if (init_sym == nullptr) {
                LOGW("Library '{}' matches a module name but does not export 'native_init'.",
                     lib_name.c_str());
                break;
            }
            auto native_init = reinterpret_cast<NativeInit>(init_sym);
            if (auto callback = native_init(g_native_api_entries)) {
                g_module_loaded_callbacks.push_back(callback);
                LOGI("Initialized native module '{}' and registered its callback.",
                     lib_name.c_str());
            }
            break;
        }
    }

    for (const auto &callback : g_module_loaded_callbacks) {
        callback(name, handle);
    }

    return handle;
};

bool InstallNativeAPI(const lsplant::HookHandler &handler) { return handler(do_dlopen_hook); }

}  // namespace vector::native
