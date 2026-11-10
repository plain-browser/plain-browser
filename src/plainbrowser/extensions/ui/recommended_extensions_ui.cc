// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/extensions/ui/recommended_extensions_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/ref_counted_memory.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "plainbrowser/adblock/browser/filter_list_updater.h"
#include "plainbrowser/adblock/common/pref_names.h"
#include "plainbrowser/extensions/browser/catalog.h"
#include "plainbrowser/extensions/browser/curated_installer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace plainbrowser::extensions {

extern const char kRecommendedExtensionsPageHtml[];
extern const char kRecommendedExtensionsPageJs[];

namespace {

constexpr char kPageJsPath[] = "recommended_extensions_page.js";

bool IsPagePath(const std::string& path) {
  return path.empty() || path == "index.html" || path == kPageJsPath;
}

void ServePage(const std::string& path,
               content::WebUIDataSource::GotDataCallback callback) {
  std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>(
      std::string(path == kPageJsPath ? kRecommendedExtensionsPageJs
                                      : kRecommendedExtensionsPageHtml)));
}

// Local State through the adblock updater, which owns the blocking switch.
PrefService* LocalState() {
  adblock::FilterListUpdater* updater = adblock::FilterListUpdater::GetInstance();
  return updater ? updater->local_state() : nullptr;
}

class RecommendedExtensionsHandler : public content::WebUIMessageHandler {
 public:
  RecommendedExtensionsHandler() = default;
  ~RecommendedExtensionsHandler() override = default;

  // content::WebUIMessageHandler:
  void RegisterMessages() override {
    // Unretained: WebUI owns the handler and drops its callbacks with it.
    web_ui()->RegisterMessageCallback(
        "getState",
        base::BindRepeating(&RecommendedExtensionsHandler::HandleGetState,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "install",
        base::BindRepeating(&RecommendedExtensionsHandler::HandleInstall,
                            base::Unretained(this), /*migrate=*/false));
    web_ui()->RegisterMessageCallback(
        "migrate",
        base::BindRepeating(&RecommendedExtensionsHandler::HandleInstall,
                            base::Unretained(this), /*migrate=*/true));
    web_ui()->RegisterMessageCallback(
        "turnOffBuiltInBlocking",
        base::BindRepeating(
            &RecommendedExtensionsHandler::HandleTurnOffBuiltInBlocking,
            base::Unretained(this)));
  }

 private:
  Profile* profile() { return Profile::FromWebUI(web_ui()); }

  void HandleGetState(const base::ListValue& args) {
    AllowJavascript();
    Profile* profile = this->profile();
    const ::extensions::ExtensionRegistry* registry =
        ::extensions::ExtensionRegistry::Get(profile);
    PrefService* local_state = LocalState();
    const bool blocking =
        local_state && local_state->GetBoolean(adblock::prefs::kBlockingEnabled);
    std::string active_blocker;
    base::ListValue list;
    for (const CuratedExtension& entry : Catalog()) {
      const std::string id(entry.id);
      const std::string store_id(entry.store_id);
      const ::extensions::Extension* ours = registry->GetInstalledExtension(id);
      const ::extensions::Extension* store =
          store_id.empty() ? nullptr
                           : registry->GetInstalledExtension(store_id);
      const bool ours_enabled = registry->enabled_extensions().Contains(id);
      const bool store_enabled =
          store && registry->enabled_extensions().Contains(store_id);
      if (entry.ad_blocker && (ours_enabled || store_enabled) &&
          active_blocker.empty()) {
        active_blocker = std::string(entry.name);
      }
      std::string state = "none";
      std::string version;
      if (ours) {
        state = ours_enabled ? "enabled" : "disabled";
        version = ours->VersionString();
      } else if (store) {
        state = "store";
        version = store->VersionString();
      }
      list.Append(
          base::DictValue()
              .Set("slug", entry.slug)
              .Set("name", entry.name)
              .Set("description",
                   l10n_util::GetStringUTF16(entry.description_id))
              .Set("homepage", entry.homepage)
              .Set("manifestVersion", entry.manifest_version)
              .Set("adBlocker", entry.ad_blocker)
              .Set("unmaintained", entry.unmaintained)
              .Set("state", state)
              .Set("version", version)
              .Set("installing",
                   CuratedInstaller::IsInstalling(profile, entry.slug))
              .Set("error", CuratedInstaller::LastError(profile, entry.slug)));
    }
    ResolveJavascriptCallback(
        args[0], base::Value(base::DictValue()
                                 .Set("extensions", std::move(list))
                                 .Set("builtInBlocking", blocking)
                                 .Set("activeBlocker", active_blocker)));
  }

  // args: callback id, slug.
  void HandleInstall(bool migrate, const base::ListValue& args) {
    AllowJavascript();
    const std::string* slug = args.size() > 1 ? args[1].GetIfString() : nullptr;
    const CuratedExtension* entry = slug ? FindBySlug(*slug) : nullptr;
    if (entry) {
      CuratedInstaller::Start(profile(), web_ui()->GetWebContents(), *entry,
                              migrate);
    }
    ResolveJavascriptCallback(args[0], base::Value(entry != nullptr));
  }

  void HandleTurnOffBuiltInBlocking(const base::ListValue& args) {
    AllowJavascript();
    if (PrefService* local_state = LocalState()) {
      // The same switch as Settings > Privacy and security > "Block ads and
      // trackers"; tracking-parameter stripping stays on.
      local_state->SetBoolean(adblock::prefs::kBlockingEnabled, false);
    }
    ResolveJavascriptCallback(args[0], base::Value());
  }
};

}  // namespace

RecommendedExtensionsUI::RecommendedExtensionsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kRecommendedExtensionsHost);
  static constexpr webui::LocalizedString kStrings[] = {
      {"title", IDS_PLAINBROWSER_EXTENSIONS_TITLE},
      {"intro", IDS_PLAINBROWSER_EXTENSIONS_INTRO},
      {"install", IDS_PLAINBROWSER_EXTENSIONS_INSTALL},
      {"installing", IDS_PLAINBROWSER_EXTENSIONS_INSTALLING},
      {"enabled", IDS_PLAINBROWSER_EXTENSIONS_ENABLED},
      {"disabled", IDS_PLAINBROWSER_EXTENSIONS_DISABLED},
      {"fromStore", IDS_PLAINBROWSER_EXTENSIONS_FROM_STORE},
      {"switch", IDS_PLAINBROWSER_EXTENSIONS_SWITCH},
      {"switchDesc", IDS_PLAINBROWSER_EXTENSIONS_SWITCH_DESC},
      {"unmaintained", IDS_PLAINBROWSER_EXTENSIONS_UNMAINTAINED},
      {"manifestV3", IDS_PLAINBROWSER_EXTENSIONS_MANIFEST_V3},
      {"source", IDS_PLAINBROWSER_EXTENSIONS_SOURCE},
      {"manage", IDS_PLAINBROWSER_EXTENSIONS_MANAGE},
      {"bothBlockers", IDS_PLAINBROWSER_EXTENSIONS_BOTH_BLOCKERS},
      {"turnOff", IDS_PLAINBROWSER_EXTENSIONS_TURN_OFF_BUILT_IN},
      {"keepBoth", IDS_PLAINBROWSER_EXTENSIONS_KEEP_BOTH},
      {"builtInOff", IDS_PLAINBROWSER_EXTENSIONS_BUILT_IN_OFF},
      {"other", IDS_PLAINBROWSER_EXTENSIONS_OTHER},
  };
  source->AddLocalizedStrings(kStrings);
  source->AddString("language", base::i18n::GetConfiguredLocale());
  source->UseStringsJs();
  source->SetRequestFilter(base::BindRepeating(&IsPagePath),
                           base::BindRepeating(&ServePage));
  web_ui->AddMessageHandler(std::make_unique<RecommendedExtensionsHandler>());
}

RecommendedExtensionsUI::~RecommendedExtensionsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(RecommendedExtensionsUI)

}  // namespace plainbrowser::extensions
