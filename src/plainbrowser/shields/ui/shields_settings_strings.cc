// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/shields/ui/shields_settings_strings.h"

#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace plainbrowser::shields {

void AddSettingsStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kStrings[] = {
      {"plainBrowserShieldsTitle", IDS_SETTINGS_PLAINBROWSER_SHIELDS_TITLE},
      {"plainBrowserShieldsDesc", IDS_SETTINGS_PLAINBROWSER_SHIELDS_DESC},
      {"plainBrowserShieldsEmpty", IDS_SETTINGS_PLAINBROWSER_SHIELDS_EMPTY},
      {"plainBrowserShieldsAdd", IDS_SETTINGS_PLAINBROWSER_SHIELDS_ADD},
      {"plainBrowserShieldsAddTitle",
       IDS_SETTINGS_PLAINBROWSER_SHIELDS_ADD_TITLE},
      {"plainBrowserShieldsSiteLabel",
       IDS_SETTINGS_PLAINBROWSER_SHIELDS_SITE_LABEL},
      {"plainBrowserShieldsInvalid", IDS_SETTINGS_PLAINBROWSER_SHIELDS_INVALID},
      {"plainBrowserShieldsRemove", IDS_SETTINGS_PLAINBROWSER_SHIELDS_REMOVE},
  };
  html_source->AddLocalizedStrings(kStrings);
}

}  // namespace plainbrowser::shields
