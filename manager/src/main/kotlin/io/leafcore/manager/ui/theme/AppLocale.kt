package io.leafcore.manager.ui.theme

import androidx.compose.runtime.Composable
import java.util.Locale
import kotlinx.coroutines.flow.StateFlow
import io.leafcore.manager.BuildConfig
import io.leafcore.manager.di.ServiceLocator
import io.leafcore.ui.locale.LocaleController
import io.leafcore.ui.locale.Translator
import io.leafcore.ui.locale.LocalizedContent as SharedLocalizedContent
import io.leafcore.ui.locale.LocalizedOverlay as SharedLocalizedOverlay
import io.leafcore.ui.locale.availableLocales as sharedAvailableLocales

/**
 * This app's answer to "which language, and which are on offer".
 *
 * The mechanism -- providing a configuration and a context down the tree so every string
 * re-resolves without an activity restart -- is the shared library's, because both managers need
 * exactly the same one and a second copy of it is a second place for it to be wrong. What stays
 * here is the binding: where the choice is stored, and which languages this app is translated into.
 *
 * The list is taken at build time from the resource folders that carry our own `strings.xml`, so a
 * language appears the moment a translator's folder lands and nobody maintains a list by hand.
 * Deliberately *not* `AssetManager.getLocales()`: it reports every locale any dependency ships a
 * resource for -- AndroidX alone contributes dozens -- along with the pseudo-locales, so the picker
 * would offer Afrikaans, Azerbaijani and "Éñĝļîšĥ" in an app that has none of them.
 */
object LeafLocaleController : LocaleController {

    override val appLocale: StateFlow<String>
        get() = ServiceLocator.settings.appLocale

    override val availableTags: List<String> =
        BuildConfig.TRANSLATIONS.split(',').filter { it.isNotBlank() }

    override fun setAppLocale(tag: String) {
        ServiceLocator.settings.setAppLocale(tag)
    }

    override val translators: Map<String, List<Translator>>
        get() = TRANSLATORS
}

/** The chosen language, applied to everything below. See the shared implementation for how. */
@Composable
fun LocalizedContent(content: @Composable () -> Unit) {
    SharedLocalizedContent(LeafLocaleController, content)
}

/** The same override, for anything drawn in its own window -- a sheet, a dialog, a dropdown. */
@Composable
fun LocalizedOverlay(content: @Composable () -> Unit) {
    SharedLocalizedOverlay(LeafLocaleController, content)
}

/** The languages this app is translated into, sorted by their own display name. */
fun availableLocales(): List<Locale> = sharedAvailableLocales(LeafLocaleController)
