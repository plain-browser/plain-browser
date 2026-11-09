// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/shields/ui/shields_settings_strings.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "plainbrowser/common/version.h"
#include "ui/base/l10n/l10n_util.h"
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

  // About page (chrome://settings/help, patches/60-ui/0012): the upstream
  // versions this build is made from, from versions.toml via version.h.
  html_source->AddString(
      "plainBrowserAboutBase",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_PLAINBROWSER_ABOUT_BASE,
          base::UTF8ToUTF16(PLAINBROWSER_CHROMIUM_VERSION),
          base::UTF8ToUTF16(PLAINBROWSER_UNGOOGLED_VERSION)));
  html_source->AddString(
      "plainBrowserAboutComponents",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_PLAINBROWSER_ABOUT_COMPONENTS,
          base::UTF8ToUTF16(PLAINBROWSER_ADBLOCK_VERSION)));
}

}  // namespace plainbrowser::shields
