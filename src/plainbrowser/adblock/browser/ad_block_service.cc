// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/browser/ad_block_service.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace plainbrowser::adblock {

// static
AdBlockService* AdBlockService::GetInstance() {
  static base::NoDestructor<AdBlockService> instance;
  return instance.get();
}

AdBlockService::AdBlockService()
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  // Unretained is safe: the instance is never destroyed (NoDestructor).
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&AdBlockService::LoadDefaultEngine,
                                        base::Unretained(this)));
}

AdBlockService::~AdBlockService() = default;

void AdBlockService::LoadDefaultEngine() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::TimeTicks start = base::TimeTicks::Now();
  engine_.emplace(new_default_engine());
  VLOG(1) << "Plain Browser adblock engine ready in "
          << (base::TimeTicks::Now() - start).InMilliseconds() << " ms";
}

bool AdBlockService::ShouldBlock(const GURL& url,
                                 const GURL& source_url,
                                 const std::string& request_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!engine_ || !url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  const std::string& spec = url.spec();
  const std::string& source =
      source_url.is_valid() ? source_url.spec() : spec;
  BlockResult r = (*engine_)->check_network_request(
      rust::Str(spec), rust::Str(source), rust::Str(request_type));
  // `matched` is adblock-rust's should_block(): important, or a block rule
  // without an exception. Redirect resources are not wired yet, so a
  // $redirect match is treated as a plain block.
  return r.matched;
}

std::string AdBlockService::CosmeticCss(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!engine_ || !url.SchemeIsHTTPOrHTTPS()) {
    return std::string();
  }
  return std::string((*engine_)->cosmetic_css(rust::Str(url.spec())));
}

}  // namespace plainbrowser::adblock
