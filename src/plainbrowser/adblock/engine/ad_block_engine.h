// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_ENGINE_AD_BLOCK_ENGINE_H_
#define PLAINBROWSER_ADBLOCK_ENGINE_AD_BLOCK_ENGINE_H_

#include <atomic>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "plainbrowser/adblock/rs/src/lib.rs.h"
#include "third_party/rust/cxx/v1/cxx.h"

class GURL;

namespace plainbrowser::adblock {

// Owner of the adblock-rust engine, in the adblock utility process
// (AdBlockEngineService) and in unit tests. All methods except task_runner()
// and UseLists() must run on task_runner(). The engine is built from the
// compiled-in lists asynchronously; until it is ready, nothing is blocked.
// UseLists() swaps in an engine built from downloaded lists. Lives as long as
// its process; tests run the task environment until idle before destroying it.
class AdBlockEngine {
 public:
  AdBlockEngine();
  ~AdBlockEngine();

  AdBlockEngine(const AdBlockEngine&) = delete;
  AdBlockEngine& operator=(const AdBlockEngine&) = delete;

  scoped_refptr<base::SequencedTaskRunner> task_runner() const {
    return task_runner_;
  }

  // Downloaded list texts by list name (default_list_names()).
  using ListTexts = std::vector<std::pair<std::string, std::string>>;

  // Any thread. Builds an engine from the default lists with the named ones
  // replaced by `lists` (empty: the compiled-in lists only) on the thread pool,
  // then swaps it in on task_runner(); queries keep using the old engine until
  // then. A later call wins over an earlier one still building. `done` runs on
  // task_runner() after the swap.
  void UseLists(ListTexts lists, base::OnceClosure done = base::OnceClosure());

  bool ShouldBlock(const GURL& url,
                   const GURL& source_url,
                   const std::string& request_type);

  // Hide rules for a document: the per-site stylesheet and whether a
  // $generichide exception turns generic (class/id) rules off for it.
  struct Cosmetic {
    Cosmetic();
    Cosmetic(Cosmetic&&);
    Cosmetic& operator=(Cosmetic&&);
    ~Cosmetic();
    std::string css;
    bool generic_hide = false;
    // For the renderer's procedural script: adblock-rust action JSON and
    // selectors with non-CSS operators.
    std::vector<std::string> procedural_actions;
    std::vector<std::string> procedural_selectors;
  };
  Cosmetic CosmeticResources(const GURL& url);

  // The scriptlets for a document, as one script for its main world; empty
  // when none apply.
  std::string Scriptlets(const GURL& url);

  // A data: URL replacing `url` under a $redirect rule, or empty.
  std::string RedirectUrl(const GURL& url, const GURL& source_url);

  // Hosts named by $redirect rules (renderers ask RedirectUrl only for them).
  std::vector<std::string> RedirectHosts();

  // Generic hide rules for classes and ids found in the document at `url`.
  std::string GenericCosmeticCss(const GURL& url,
                                 const std::vector<std::string>& classes,
                                 const std::vector<std::string>& ids);

  // `url` with tracking parameters removed ($removeparam rules), or an empty
  // GURL when nothing was removed or the request would be blocked outright.
  // `request_type` is "main_frame" or "sub_frame" for navigations.
  GURL RewrittenUrl(const GURL& url,
                    const GURL& source_url,
                    const std::string& request_type);

 private:
  void LoadDefaultEngine();
  void SwapEngine(int generation,
                  rust::Box<AdblockEngine> engine,
                  base::OnceClosure done);

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::optional<rust::Box<AdblockEngine>> engine_;
  // Incremented by every UseLists() call (any thread); an engine whose build
  // started before the latest call is dropped instead of swapped in.
  std::atomic<int> generation_{0};
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_ENGINE_AD_BLOCK_ENGINE_H_
