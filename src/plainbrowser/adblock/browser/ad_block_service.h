// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_BROWSER_AD_BLOCK_SERVICE_H_
#define PLAINBROWSER_ADBLOCK_BROWSER_AD_BLOCK_SERVICE_H_

#include <memory>
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

  // Tests only: a service whose engine loads on the current ThreadPool (the
  // test's TaskEnvironment). While it is alive, GetInstance() returns it, so the
  // mojo filter and the throttles under test use it instead of the process-wide
  // instance (whose task runner would outlive the test's thread pool). Run the
  // task environment until idle before using or destroying it.
  static std::unique_ptr<AdBlockService> CreateForTesting();
  ~AdBlockService();

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

  void LoadDefaultEngine();

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::optional<rust::Box<AdblockEngine>> engine_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_BROWSER_AD_BLOCK_SERVICE_H_
