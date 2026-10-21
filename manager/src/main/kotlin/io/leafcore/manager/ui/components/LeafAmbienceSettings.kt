package io.leafcore.manager.ui.components

import io.leafcore.manager.di.ServiceLocator
import io.leafcore.ui.ambience.AmbienceSettings

/**
 * Bridges the shared [io.leafcore.ui.ambience.AmbientSurface] to Leaf's persisted settings,
 * so the header comes back the size and speed it was left at. LSPatch, which does not persist these,
 * simply lets the surface fall back to its ephemeral default.
 */
object LeafAmbienceSettings : AmbienceSettings {
    private val settings
        get() = ServiceLocator.settings

    override fun scale(key: String) = settings.ambienceScale(key)

    override fun speed(key: String) = settings.ambienceSpeed(key)

    override fun variant(key: String) = settings.ambienceVariant(key)

    override fun setScale(key: String, value: Float) = settings.setAmbienceScale(key, value)

    override fun setSpeed(key: String, value: Float) = settings.setAmbienceSpeed(key, value)

    override fun setVariant(key: String, value: Int) = settings.setAmbienceVariant(key, value)
}
