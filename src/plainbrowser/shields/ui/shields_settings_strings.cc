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
      {"plainBrowserListUpdates", IDS_SETTINGS_PLAINBROWSER_LIST_UPDATES},
      {"plainBrowserListUpdatesDesc",
       IDS_SETTINGS_PLAINBROWSER_LIST_UPDATES_DESC},
      {"plainBrowserBlocking", IDS_SETTINGS_PLAINBROWSER_BLOCKING},
      {"plainBrowserBlockingDesc", IDS_SETTINGS_PLAINBROWSER_BLOCKING_DESC},
      {"plainBrowserListLastCheck", IDS_SETTINGS_PLAINBROWSER_LIST_LAST_CHECK},
      {"plainBrowserListNeverChecked",
       IDS_SETTINGS_PLAINBROWSER_LIST_NEVER_CHECKED},
      {"plainBrowserAdblockStatus", IDS_SETTINGS_PLAINBROWSER_ADBLOCK_STATUS},
      {"plainBrowserAdblockStatusDesc",
       IDS_SETTINGS_PLAINBROWSER_ADBLOCK_STATUS_DESC},
  };
  html_source->AddLocalizedStrings(kStrings);
}

}  // namespace plainbrowser::shields
