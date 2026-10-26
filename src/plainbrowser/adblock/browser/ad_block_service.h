// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_BROWSER_AD_BLOCK_SERVICE_H_
#define PLAINBROWSER_ADBLOCK_BROWSER_AD_BLOCK_SERVICE_H_

#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "plainbrowser/adblock/rs/src/lib.rs.h"
#include "third_party/rust/cxx/v1/cxx.h"

class GURL;

namespace plainbrowser::adblock {

// Process-wide owner of the adblock-rust engine. Created on first use; all
// methods except GetInstance() and task_runner() must run on task_runner().
// The engine is built from the compiled-in lists asynchronously; until it is
// ready, nothing is blocked.
class AdBlockService {
 public:
  static AdBlockService* GetInstance();

  AdBlockService(const AdBlockService&) = delete;
  AdBlockService& operator=(const AdBlockService&) = delete;

  scoped_refptr<base::SequencedTaskRunner> task_runner() const {
    return task_runner_;
  }

  bool ShouldBlock(const GURL& url,
                   const GURL& source_url,
                   const std::string& request_type);
  std::string CosmeticCss(const GURL& url);

  // `url` with tracking parameters removed ($removeparam rules), or an empty
  // GURL when nothing was removed or the request would be blocked outright.
  // `request_type` is "main_frame" or "sub_frame" for navigations.
  GURL RewrittenUrl(const GURL& url,
                    const GURL& source_url,
                    const std::string& request_type);

 private:
  friend class base::NoDestructor<AdBlockService>;
  AdBlockService();
  ~AdBlockService();

  void LoadDefaultEngine();

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::optional<rust::Box<AdblockEngine>> engine_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_BROWSER_AD_BLOCK_SERVICE_H_
