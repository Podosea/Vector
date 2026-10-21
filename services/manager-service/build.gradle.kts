plugins { alias(libs.plugins.agp.lib) }

android {
    buildFeatures { aidl = true }

    buildTypes { release { isMinifyEnabled = false } }

    namespace = "io.leafcore.managerservice"
}

dependencies { api(libs.rikkax.parcelablelist) }
