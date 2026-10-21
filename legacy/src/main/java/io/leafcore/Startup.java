package io.leafcore;

import io.leafcore.ipc.IFrameworkService;
import io.leafcore.util.Utils;
import io.leafcore.impl.core.LeafStartup;
import io.leafcore.impl.di.LeafBootstrap;
import io.leafcore.legacy.LegacyDelegateImpl;

import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedInit;

public class Startup {

    public static void bootstrapXposed(boolean systemServerStarted) {
        try {
            LeafStartup.bootstrap(XposedInit.startsSystemServer, systemServerStarted);
            XposedInit.loadLegacyModules();
        } catch (Throwable t) {
            Utils.logE("Error during framework initialization", t);
        }
    }

    /**
     * Registers a pre-built {@code LoadedApk} (the LSPatch rootless target) with the modern hooks so
     * its package lifecycle is dispatched when its class loader is realized. See
     * {@link LeafStartup#trackLoadedApk(Object)}.
     */
    public static void trackLoadedApk(Object loadedApk) {
        LeafStartup.trackLoadedApk(loadedApk);
    }

    public static void initXposed(boolean isSystem, String processName, String appDir, IFrameworkService service) {
        // Establish the Dependency Injection contract
        LeafBootstrap.INSTANCE.init(new LegacyDelegateImpl());

        // Initialize legacy resources and state
        XposedBridge.initXResources();
        XposedInit.startsSystemServer = isSystem;

        // Hand off execution to the modern framework initialization
        LeafStartup.init(isSystem, processName, appDir, service);
    }
}
