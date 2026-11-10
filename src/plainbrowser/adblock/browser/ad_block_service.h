// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_BROWSER_AD_BLOCK_SERVICE_H_
#define PLAINBROWSER_ADBLOCK_BROWSER_AD_BLOCK_SERVICE_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "plainbrowser/adblock/common/adblock_engine.mojom.h"

class GURL;

namespace plainbrowser::adblock {

// The browser's handle on the adblock engine, which runs in its own sandboxed
// utility process (engine/, mojom::AdblockEngine): filter lists come from the
// network and are parsed there, not in the browser process. Process-wide,
// created on first use. Methods other than GetInstance(), task_runner(),
// SetLauncher() and UseLists() must run on task_runner(); their callbacks run
// there too.
//
// The process is started on first use and kept running. Every query fails
// open: until the engine is built, without a launcher, or when the process
// goes away, the answer is "nothing to block". A process that went away is
// started again on the next query, with the lists it had, except downloaded
// lists that were in use when it went away (they may be what made it crash):
// those are dropped until the updater hands over new ones. After
// kMaxEngineStarts starts the service stops starting it until the browser
// restarts. The optional lists and the user's own filters (SetUserConfig())
// are kept across restarts of the process.
class AdBlockService {
 public:
  using Launcher = base::RepeatingCallback<void(
      mojo::PendingReceiver<mojom::AdblockEngine>)>;
  // Downloaded list texts by list name ("easylist", ...).
  using ListTexts = std::vector<std::pair<std::string, std::string>>;

  static constexpr int kMaxEngineStarts = 4;

  static AdBlockService* GetInstance();

  // Tests only: a service that starts engines with `launcher` (for example an
  // in-process AdBlockEngineService). While it is alive, GetInstance() returns
  // it. Run the task environment until idle before destroying it.
  static std::unique_ptr<AdBlockService> CreateForTesting(Launcher launcher);
  ~AdBlockService();

  AdBlockService(const AdBlockService&) = delete;
  AdBlockService& operator=(const AdBlockService&) = delete;

  scoped_refptr<base::SequencedTaskRunner> task_runner() const {
    return task_runner_;
  }

  // Any thread. How to start the engine process (browser/engine_launcher.cc,
  // content::ServiceProcessHost); the first call wins.
  void SetLauncher(Launcher launcher);

  // Any thread. Builds an engine from the compiled-in lists with the named
  // ones replaced by `lists` (empty: compiled-in only); queries are answered by
  // the old engine until it is swapped in. `done` runs on task_runner() after
  // the swap (or when there is no engine process to swap in).
  void UseLists(ListTexts lists, base::OnceClosure done = base::OnceClosure());

  // Any thread. The optional lists that are on (kOptionalLists names, in that
  // order) and the user's own filters, from the Local State prefs
  // (FilterListUpdater). A running engine is rebuilt; one not started yet gets
  // them when it starts. Until the first call: DefaultOptionalLists() and no
  // filters. `done` as for UseLists().
  void SetUserConfig(std::vector<std::string> optional_lists,
                     std::string custom_filters,
                     base::OnceClosure done = base::OnceClosure());

  void ShouldBlock(const GURL& url,
                   const GURL& source_url,
                   const std::string& request_type,
                   base::OnceCallback<void(bool)> callback);
  // Hide rules for a document. generic_hide is true without an engine, so the
  // renderer does not collect class names for nothing.
  void CosmeticResources(
      const GURL& url,
      base::OnceCallback<void(mojom::CosmeticResultPtr)> callback);
  // The scriptlets for a document's main world, or empty.
  void Scriptlets(const GURL& url,
                  base::OnceCallback<void(const std::string&)> callback);
  // A data: URL replacing `url` under a $redirect rule, or empty.
  void RedirectUrl(const GURL& url,
                   const GURL& source_url,
                   base::OnceCallback<void(const std::string&)> callback);
  // Hosts named by $redirect rules.
  void RedirectHosts(
      base::OnceCallback<void(const std::vector<std::string>&)> callback);
  // Generic hide rules for classes and ids found in the document at `url`.
  void GenericCosmeticCss(
      const GURL& url,
      const std::vector<std::string>& classes,
      const std::vector<std::string>& ids,
      base::OnceCallback<void(const std::string&)> callback);
  // `url` with tracking parameters removed ($removeparam rules), or an empty
  // GURL. `request_type` is "main_frame" or "sub_frame" for navigations.
  void RewrittenUrl(const GURL& url,
                    const GURL& source_url,
                    const std::string& request_type,
                    base::OnceCallback<void(const GURL&)> callback);

  // Any thread. The global switch (prefs::kBlockingEnabled), mirrored here by
  // FilterListUpdater so the filter can read it off the UI thread.
  void SetBlockingEnabled(bool enabled) { blocking_enabled_ = enabled; }
  bool blocking_enabled() const { return blocking_enabled_; }

  // Any thread. Requests blocked or replaced ($redirect) since start-up, for
  // chrome://adblock; AdblockFilterImpl counts them.
  void CountBlocked() { ++blocked_total_; }
  int64_t blocked_total() const { return blocked_total_; }

  // The request log of chrome://adblock: what the blocker did, newest last.
  // Kept in memory only, and only while a chrome://adblock page is open
  // (AddLogViewer() / RemoveLogViewer()); emptied when the last one closes, so
  // nothing about the pages a user visits stays around otherwise.
  enum class LogAction {
    kBlocked,     // A network rule blocked the request.
    kRedirected,  // A $redirect rule replaced the response.
    kParamsRemoved,  // Tracking parameters were removed from a navigation.
    kRedirectSkipped,  // A tracking redirector was skipped (debouncing).
  };
  struct LogEntry {
    base::Time time;
    LogAction action;
    // The request (blocked, redirected) or the navigation's original URL.
    std::string url;
    // The page: the top-level origin, or for navigations the URL navigated to
    // instead.
    std::string page;
    // adblock-rust request type ("script", "image", ...); empty for
    // navigations and redirects.
    std::string type;
  };
  static constexpr size_t kMaxLogEntries = 500;
  // Any thread.
  void AddLogViewer();
  void RemoveLogViewer();
  bool logging() const { return log_viewers_ > 0; }
  // Any thread. Dropped unless logging().
  void Log(LogAction action,
           std::string url,
           std::string page,
           std::string type = std::string());
  std::vector<LogEntry> GetLog() const;
  void ClearLog();

  // For chrome://adblock.
  struct Status {
    Status();
    Status(const Status&);
    Status& operator=(const Status&);
    ~Status();
    bool engine_running = false;
    int engine_starts = 0;
    // No more starts this session (the process kept going away).
    bool gave_up = false;
    // Names of the downloaded lists the engine uses; the others are the
    // compiled-in ones.
    std::vector<std::string> downloaded_lists;
    // The optional lists that are on, and the size of the user's filters.
    std::vector<std::string> optional_lists;
    size_t custom_filters_bytes = 0;
  };
  // `callback` runs on task_runner().
  void GetStatus(base::OnceCallback<void(Status)> callback);

  // Tests: how many times an engine process was started.
  int engine_starts_for_testing() const { return engine_starts_; }

 private:
  friend class base::NoDestructor<AdBlockService>;
  AdBlockService();

  void SetLauncherOnSequence(Launcher launcher);
  void GetStatusOnSequence(base::OnceCallback<void(Status)> callback);
  void UseListsOnSequence(ListTexts lists, base::OnceClosure done);
  void SetUserConfigOnSequence(std::vector<std::string> optional_lists,
                               std::string custom_filters,
                               base::OnceClosure done);
  bool CanStartEngine() const;
  // Whether the engine must be told anything: the process starts with the
  // compiled-in lists and the default optional lists by itself.
  bool NeedsConfig() const;
  // Sends the whole configuration to the running engine.
  void SendConfig(base::OnceClosure done);
  // Starts an engine process and gives it the configuration; `config_done`
  // runs when it has swapped it in.
  void StartEngine(base::OnceClosure config_done);
  // The engine, started if needed; null when it cannot be started.
  mojom::AdblockEngine* Engine();
  void OnEngineGone();

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  Launcher launcher_;
  mojo::Remote<mojom::AdblockEngine> engine_;
  // The lists of the last UseLists(), given to every new engine process;
  // nullopt: the compiled-in lists.
  std::optional<ListTexts> lists_;
  // From the last SetUserConfig().
  std::vector<std::string> optional_lists_;
  std::string custom_filters_;
  int engine_starts_ = 0;
  std::atomic<bool> blocking_enabled_{true};
  std::atomic<int64_t> blocked_total_{0};
  std::atomic<int> log_viewers_{0};
  mutable base::Lock log_lock_;
  base::circular_deque<LogEntry> log_ GUARDED_BY(log_lock_);
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_BROWSER_AD_BLOCK_SERVICE_H_
