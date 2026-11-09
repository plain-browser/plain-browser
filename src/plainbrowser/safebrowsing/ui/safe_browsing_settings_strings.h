// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_SAFEBROWSING_UI_SAFE_BROWSING_SETTINGS_STRINGS_H_
#define PLAINBROWSER_SAFEBROWSING_UI_SAFE_BROWSING_SETTINGS_STRINGS_H_

namespace content {
class WebUIDataSource;
}

namespace plainbrowser::safebrowsing {

// Settings strings of Safe Browsing through the Plain Browser proxy
// (patches/70-safebrowsing/0005): the toggle on the Google services page and
// the Standard / No protection descriptions on the Security page, which
// replace Chromium's (same $i18n keys, added later, so they win). Called at
// the end of settings::AddLocalizedStrings, after AddPrivacyStrings.
void AddSettingsStrings(content::WebUIDataSource* html_source);

}  // namespace plainbrowser::safebrowsing

#endif  // PLAINBROWSER_SAFEBROWSING_UI_SAFE_BROWSING_SETTINGS_STRINGS_H_
