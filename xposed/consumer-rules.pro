# Preserve the libxposed public API surface for module developers
-keep class io.github.libxposed.** { *; }

# The libxposed annotations are compile-only metadata and are not packaged
-dontwarn io.github.libxposed.annotation.**

# Preserve all native methods (HookBridge, ResourcesHook, NativeAPI, etc.)
-keepclasseswithmembers,includedescriptorclasses class * {
    native <methods>;
}

# Preserve the JNI Hook Trampoline
-keepclassmembers class io.leafcore.impl.hooks.LeafNativeHooker {
    public <init>(java.lang.reflect.Executable);
    public java.lang.Object callback(java.lang.Object[]);
}
