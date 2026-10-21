package io.leafcore.impl.hookers

import io.github.libxposed.api.XposedInterface
import io.leafcore.impl.di.LeafBootstrap

/**
 * Intercepts the early ApplicationThread attachment phase. Triggers the legacy compatibility layer
 * to load modern modules into the process.
 */
object AppAttachHooker : XposedInterface.Hooker {
    override fun intercept(chain: XposedInterface.Chain): Any? {
        // Execute the actual attach() method first
        val result = chain.proceed()

        // Delegate legacy module loading via DI
        val activityThread = chain.thisObject
        if (activityThread != null) {
            LeafBootstrap.withLegacy { delegate -> delegate.loadModules(activityThread) }
        }

        return result
    }
}
