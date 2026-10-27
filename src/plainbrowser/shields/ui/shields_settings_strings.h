// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_SHIELDS_UI_SHIELDS_SETTINGS_STRINGS_H_
#define PLAINBROWSER_SHIELDS_UI_SHIELDS_SETTINGS_STRINGS_H_

namespace content {
class WebUIDataSource;
}

namespace plainbrowser::shields {

// Adds the strings of the exception-list section on chrome://settings/privacy
// (settings-plainbrowser-shields-exceptions, patches/60-ui/0009) to the
// settings data source. Called from settings::AddLocalizedStrings.
void AddSettingsStrings(content::WebUIDataSource* html_source);

}  // namespace plainbrowser::shields

#endif  // PLAINBROWSER_SHIELDS_UI_SHIELDS_SETTINGS_STRINGS_H_
