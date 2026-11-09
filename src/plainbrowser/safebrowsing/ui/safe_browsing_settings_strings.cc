// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/safebrowsing/ui/safe_browsing_settings_strings.h"

#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace plainbrowser::safebrowsing {

void AddSettingsStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kStrings[] = {
      // Google services page: the on/off toggle.
      {"plainBrowserSafeBrowsingProxy",
       IDS_SETTINGS_PLAINBROWSER_SAFEBROWSING_PROXY},
      {"plainBrowserSafeBrowsingProxyDesc",
       IDS_SETTINGS_PLAINBROWSER_SAFEBROWSING_PROXY_DESC},
      // Security page: Chromium's keys, our texts (where the requests go).
      {"safeBrowsingStandardDesc",
       IDS_SETTINGS_PLAINBROWSER_SAFEBROWSING_STANDARD_DESC},
      {"safeBrowsingNoneDesc", IDS_SETTINGS_PLAINBROWSER_SAFEBROWSING_NONE_DESC},
  };
  html_source->AddLocalizedStrings(kStrings);
}

}  // namespace plainbrowser::safebrowsing
