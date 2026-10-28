// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_BROWSER_AD_BLOCK_SERVICE_H_
#define PLAINBROWSER_ADBLOCK_BROWSER_AD_BLOCK_SERVICE_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
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
// restarts.
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

  // Tests: how many times an engine process was started.
  int engine_starts_for_testing() const { return engine_starts_; }

 private:
  friend class base::NoDestructor<AdBlockService>;
  AdBlockService();

  void SetLauncherOnSequence(Launcher launcher);
  void UseListsOnSequence(ListTexts lists, base::OnceClosure done);
  bool CanStartEngine() const;
  // Starts an engine process and gives it lists_; `lists_done` runs when it
  // has swapped them in.
  void StartEngine(base::OnceClosure lists_done);
  // The engine, started if needed; null when it cannot be started.
  mojom::AdblockEngine* Engine();
  void OnEngineGone();

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  Launcher launcher_;
  mojo::Remote<mojom::AdblockEngine> engine_;
  // The lists of the last UseLists(), given to every new engine process;
  // nullopt: the compiled-in lists.
  std::optional<ListTexts> lists_;
  int engine_starts_ = 0;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_BROWSER_AD_BLOCK_SERVICE_H_
