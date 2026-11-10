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
#include "plainbrowser/adblock/common/filter_list_names.h"
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

const char* LogActionName(AdBlockService::LogAction action) {
  switch (action) {
    case AdBlockService::LogAction::kBlocked:
      return "blocked";
    case AdBlockService::LogAction::kRedirected:
      return "redirected";
    case AdBlockService::LogAction::kParamsRemoved:
      return "params";
    case AdBlockService::LogAction::kRedirectSkipped:
      return "debounced";
  }
  return "";
}

// Local State, through the updater that owns the adblock prefs; null in tests
// and before start-up finished.
PrefService* LocalState() {
  FilterListUpdater* updater = FilterListUpdater::GetInstance();
  return updater ? updater->local_state() : nullptr;
}

class AdblockPageHandler : public content::WebUIMessageHandler {
 public:
  // The request log records only while a chrome://adblock page is open.
  AdblockPageHandler() { AdBlockService::GetInstance()->AddLogViewer(); }
  ~AdblockPageHandler() override {
    AdBlockService::GetInstance()->RemoveLogViewer();
  }

  // content::WebUIMessageHandler:
  void RegisterMessages() override {
    // Unretained: WebUI owns the handler and drops its callbacks with it.
    web_ui()->RegisterMessageCallback(
        "getStatus", base::BindRepeating(&AdblockPageHandler::HandleGetStatus,
                                         base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "checkNow", base::BindRepeating(&AdblockPageHandler::HandleCheckNow,
                                        base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "getCustomFilters",
        base::BindRepeating(&AdblockPageHandler::HandleGetCustomFilters,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "setCustomFilters",
        base::BindRepeating(&AdblockPageHandler::HandleSetCustomFilters,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "getLog", base::BindRepeating(&AdblockPageHandler::HandleGetLog,
                                      base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "clearLog", base::BindRepeating(&AdblockPageHandler::HandleClearLog,
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
    // Every list is "on" except optional ones the user did not turn on.
    base::ListValue off;
    for (const OptionalList& list : kOptionalLists) {
      if (std::ranges::find(engine.optional_lists, list.name) ==
          engine.optional_lists.end()) {
        off.Append(list.name);
      }
    }
    status.Set("listsOff", std::move(off));
    status.Set("customFiltersBytes",
               static_cast<double>(engine.custom_filters_bytes));
    ResolveJavascriptCallback(base::Value(callback_id),
                              base::Value(std::move(status)));
  }

  void HandleCheckNow(const base::ListValue& args) {
    AllowJavascript();
    const bool started = FilterListUpdater::GetInstance() &&
                         FilterListUpdater::GetInstance()->CheckNow();
    ResolveJavascriptCallback(args[0], base::Value(started));
  }

  void HandleGetCustomFilters(const base::ListValue& args) {
    AllowJavascript();
    PrefService* local_state = LocalState();
    ResolveJavascriptCallback(
        args[0], base::Value(local_state
                                 ? local_state->GetString(prefs::kCustomFilters)
                                 : std::string()));
  }

  // args: callback id, the text. Resolves with false when it was not stored
  // (too long, or no Local State yet).
  void HandleSetCustomFilters(const base::ListValue& args) {
    AllowJavascript();
    PrefService* local_state = LocalState();
    const std::string* text = args.size() > 1 ? args[1].GetIfString() : nullptr;
    const bool ok =
        local_state && text && text->size() <= kMaxCustomFiltersBytes;
    if (ok) {
      // FilterListUpdater observes the pref and rebuilds the engine.
      local_state->SetString(prefs::kCustomFilters, *text);
    }
    ResolveJavascriptCallback(args[0], base::Value(ok));
  }

  void HandleGetLog(const base::ListValue& args) {
    AllowJavascript();
    base::ListValue entries;
    for (const AdBlockService::LogEntry& e :
         AdBlockService::GetInstance()->GetLog()) {
      entries.Append(base::DictValue()
                         .Set("time", e.time.InMillisecondsFSinceUnixEpoch())
                         .Set("action", LogActionName(e.action))
                         .Set("url", e.url)
                         .Set("page", e.page)
                         .Set("type", e.type));
    }
    ResolveJavascriptCallback(args[0], base::Value(std::move(entries)));
  }

  void HandleClearLog(const base::ListValue& args) {
    AllowJavascript();
    AdBlockService::GetInstance()->ClearLog();
    ResolveJavascriptCallback(args[0], base::Value());
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
      {"listOff", IDS_PLAINBROWSER_ADBLOCK_LIST_OFF},
      {"listsNote", IDS_PLAINBROWSER_ADBLOCK_LISTS_NOTE},
      {"customFilters", IDS_PLAINBROWSER_ADBLOCK_CUSTOM_FILTERS},
      {"customFiltersDesc", IDS_PLAINBROWSER_ADBLOCK_CUSTOM_FILTERS_DESC},
      {"save", IDS_PLAINBROWSER_ADBLOCK_SAVE},
      {"saved", IDS_PLAINBROWSER_ADBLOCK_SAVED},
      {"saveFailed", IDS_PLAINBROWSER_ADBLOCK_SAVE_FAILED},
      {"log", IDS_PLAINBROWSER_ADBLOCK_LOG},
      {"logDesc", IDS_PLAINBROWSER_ADBLOCK_LOG_DESC},
      {"logEmpty", IDS_PLAINBROWSER_ADBLOCK_LOG_EMPTY},
      {"logFilter", IDS_PLAINBROWSER_ADBLOCK_LOG_FILTER},
      {"logClear", IDS_PLAINBROWSER_ADBLOCK_LOG_CLEAR},
      {"logBlocked", IDS_PLAINBROWSER_ADBLOCK_LOG_BLOCKED},
      {"logRedirected", IDS_PLAINBROWSER_ADBLOCK_LOG_REDIRECTED},
      {"logParams", IDS_PLAINBROWSER_ADBLOCK_LOG_PARAMS},
      {"logDebounced", IDS_PLAINBROWSER_ADBLOCK_LOG_DEBOUNCED},
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
