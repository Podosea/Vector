plugins { alias(libs.plugins.agp.lib) }

android {
    buildFeatures { aidl = true }

    buildTypes { release { isMinifyEnabled = false } }

    sourceSets {
        named("main") {
            java.directories.addAll(listOf("src/main/java", "../libxposed/service/src/main"))
            aidl.directories.addAll(listOf("src/main/aidl", "../libxposed/interface/src/main/aidl"))
        }
    }

    aidlPackagedList += "io/leafcore/ipc/LoadedModule.aidl"
    namespace = "io.leafcore.daemonservice"
}

dependencies {
    compileOnly(libs.androidx.annotation)
    compileOnly(libs.libxposed.annotation)
    compileOnly(projects.hiddenapi.stubs)
}
