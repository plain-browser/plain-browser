// Copyright Plain Browser contributors. MPL-2.0.
#ifndef DRAFTWAY_ADBLOCK_BROWSER_AD_BLOCK_SERVICE_H_
#define DRAFTWAY_ADBLOCK_BROWSER_AD_BLOCK_SERVICE_H_

#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "draftway/adblock/rs/src/lib.rs.h"
#include "third_party/rust/cxx/v1/cxx.h"

class GURL;

namespace draftway::adblock {

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

 private:
  friend class base::NoDestructor<AdBlockService>;
  AdBlockService();
  ~AdBlockService();

  void LoadDefaultEngine();

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::optional<rust::Box<AdblockEngine>> engine_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace draftway::adblock

#endif  // DRAFTWAY_ADBLOCK_BROWSER_AD_BLOCK_SERVICE_H_
