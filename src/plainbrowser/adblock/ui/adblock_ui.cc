// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/ui/adblock_ui.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "plainbrowser/adblock/browser/ad_block_service.h"
#include "plainbrowser/adblock/browser/filter_list_updater.h"
#include "plainbrowser/adblock/browser/filter_lists.h"
#include "plainbrowser/adblock/common/pref_names.h"
#include "ui/base/webui/web_ui_util.h"

namespace plainbrowser::adblock {

extern const char kAdblockPageHtml[];
extern const char kAdblockPageJs[];

namespace {

constexpr char kPageJsPath[] = "adblock_page.js";

bool IsPagePath(const std::string& path) {
  return path.empty() || path == "index.html" || path == kPageJsPath;
}

void ServePage(const std::string& path,
               content::WebUIDataSource::GotDataCallback callback) {
  std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>(
      std::string(path == kPageJsPath ? kAdblockPageJs : kAdblockPageHtml)));
}

class AdblockPageHandler : public content::WebUIMessageHandler {
 public:
  AdblockPageHandler() = default;
  ~AdblockPageHandler() override = default;

  // content::WebUIMessageHandler:
  void RegisterMessages() override {
    // Unretained: WebUI owns the handler and drops its callbacks with it.
    web_ui()->RegisterMessageCallback(
        "getStatus", base::BindRepeating(&AdblockPageHandler::HandleGetStatus,
                                         base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "checkNow", base::BindRepeating(&AdblockPageHandler::HandleCheckNow,
                                        base::Unretained(this)));
  }

 private:
  void HandleGetStatus(const base::ListValue& args) {
    AllowJavascript();
    std::string callback_id = args[0].GetString();
    base::DictValue status;
    AdBlockService* service = AdBlockService::GetInstance();
    status.Set("blockingEnabled", service->blocking_enabled());
    status.Set("blockedTotal", static_cast<double>(service->blocked_total()));
    FilterListUpdater* updater = FilterListUpdater::GetInstance();
    filter_lists::Manifest stored;
    if (updater) {
      PrefService* local_state = updater->local_state();
      status.Set("updatesEnabled",
                 local_state->GetBoolean(prefs::kListUpdatesEnabled));
      status.Set("lastSuccess",
                 local_state->GetDouble(prefs::kListUpdatesLastSuccess));
      const base::Time last_check =
          local_state->GetTime(prefs::kListUpdatesLastCheck);
      status.Set("lastCheck", last_check.is_null()
                                  ? 0.0
                                  : last_check.InMillisecondsFSinceUnixEpoch());
      status.Set("checking", updater->checking());
      stored = updater->stored();
    } else {
      status.Set("updatesEnabled", false);
      status.Set("lastSuccess", 0.0);
      status.Set("lastCheck", 0.0);
      status.Set("checking", false);
    }
    // The engine's side lives on the service's sequence.
    service->GetStatus(base::BindPostTaskToCurrentDefault(
        base::BindOnce(&AdblockPageHandler::OnEngineStatus,
                       weak_factory_.GetWeakPtr(), std::move(callback_id),
                       std::move(status), std::move(stored))));
  }

  void OnEngineStatus(std::string callback_id,
                      base::DictValue status,
                      filter_lists::Manifest stored,
                      AdBlockService::Status engine) {
    status.Set("engineRunning", engine.engine_running);
    status.Set("engineStarts", engine.engine_starts);
    status.Set("engineGaveUp", engine.gave_up);
    base::ListValue lists;
    for (std::string_view name : filter_lists::kUpdatableLists) {
      const bool downloaded =
          std::ranges::find(engine.downloaded_lists, name) !=
          engine.downloaded_lists.end();
      base::DictValue list;
      list.Set("name", name);
      list.Set("downloaded", downloaded);
      auto it = stored.find(std::string(name));
      if (downloaded && it != stored.end()) {
        list.Set("sha256", it->second.sha256);
        list.Set("size", static_cast<double>(it->second.size));
      }
      lists.Append(std::move(list));
    }
    base::DictValue removeparam;
    removeparam.Set("name", "plainbrowser-removeparam");
    removeparam.Set("downloaded", false);
    lists.Append(std::move(removeparam));
    status.Set("lists", std::move(lists));
    ResolveJavascriptCallback(base::Value(callback_id),
                              base::Value(std::move(status)));
  }

  void HandleCheckNow(const base::ListValue& args) {
    AllowJavascript();
    const bool started = FilterListUpdater::GetInstance() &&
                         FilterListUpdater::GetInstance()->CheckNow();
    ResolveJavascriptCallback(args[0], base::Value(started));
  }

  base::WeakPtrFactory<AdblockPageHandler> weak_factory_{this};
};

}  // namespace

AdblockUI::AdblockUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kAdblockHost);
  static constexpr webui::LocalizedString kStrings[] = {
      {"title", IDS_PLAINBROWSER_ADBLOCK_PAGE_TITLE},
      {"blocking", IDS_PLAINBROWSER_ADBLOCK_BLOCKING},
      {"on", IDS_PLAINBROWSER_ADBLOCK_ON},
      {"off", IDS_PLAINBROWSER_ADBLOCK_OFF},
      {"blocked", IDS_PLAINBROWSER_ADBLOCK_BLOCKED},
      {"engine", IDS_PLAINBROWSER_ADBLOCK_ENGINE},
      {"engineRunning", IDS_PLAINBROWSER_ADBLOCK_ENGINE_RUNNING},
      {"engineIdle", IDS_PLAINBROWSER_ADBLOCK_ENGINE_IDLE},
      {"engineStopped", IDS_PLAINBROWSER_ADBLOCK_ENGINE_STOPPED},
      {"engineStarts", IDS_PLAINBROWSER_ADBLOCK_ENGINE_STARTS},
      {"lists", IDS_PLAINBROWSER_ADBLOCK_LISTS},
      {"listDownloaded", IDS_PLAINBROWSER_ADBLOCK_LIST_DOWNLOADED},
      {"listBundled", IDS_PLAINBROWSER_ADBLOCK_LIST_BUNDLED},
      {"updates", IDS_PLAINBROWSER_ADBLOCK_UPDATES},
      {"updatesOff", IDS_PLAINBROWSER_ADBLOCK_UPDATES_OFF},
      {"lastCheck", IDS_SETTINGS_PLAINBROWSER_LIST_LAST_CHECK},
      {"neverChecked", IDS_SETTINGS_PLAINBROWSER_LIST_NEVER_CHECKED},
      {"lastFailed", IDS_PLAINBROWSER_ADBLOCK_LAST_FAILED},
      {"checkNow", IDS_PLAINBROWSER_ADBLOCK_CHECK_NOW},
      {"checking", IDS_PLAINBROWSER_ADBLOCK_CHECKING},
      {"settings", IDS_PLAINBROWSER_ADBLOCK_SETTINGS},
  };
  source->AddLocalizedStrings(kStrings);
  source->AddString("language", base::i18n::GetConfiguredLocale());
  source->UseStringsJs();
  source->SetRequestFilter(base::BindRepeating(&IsPagePath),
                           base::BindRepeating(&ServePage));
  web_ui->AddMessageHandler(std::make_unique<AdblockPageHandler>());
}

AdblockUI::~AdblockUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(AdblockUI)

}  // namespace plainbrowser::adblock
