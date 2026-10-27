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

namespace {
AdBlockService* g_instance_for_testing = nullptr;
}  // namespace

// static
AdBlockService* AdBlockService::GetInstance() {
  if (g_instance_for_testing) {
    return g_instance_for_testing;
  }
  static base::NoDestructor<AdBlockService> instance;
  return instance.get();
}

// static
std::unique_ptr<AdBlockService> AdBlockService::CreateForTesting() {
  std::unique_ptr<AdBlockService> service(new AdBlockService());
  g_instance_for_testing = service.get();
  return service;
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

AdBlockService::~AdBlockService() {
  if (g_instance_for_testing == this) {
    g_instance_for_testing = nullptr;
  }
}

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

GURL AdBlockService::RewrittenUrl(const GURL& url,
                                  const GURL& source_url,
                                  const std::string& request_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!engine_ || !url.SchemeIsHTTPOrHTTPS() || !url.has_query()) {
    return GURL();
  }
  const std::string& spec = url.spec();
  const std::string& source =
      source_url.is_valid() ? source_url.spec() : spec;
  BlockResult r = (*engine_)->check_network_request(
      rust::Str(spec), rust::Str(source), rust::Str(request_type));
  if (r.matched || r.rewritten_url.empty()) {
    return GURL();
  }
  GURL rewritten(std::string(r.rewritten_url));
  // Only ever drop query parameters: anything else from the engine is ignored.
  if (!rewritten.is_valid() || rewritten == url ||
      rewritten.GetWithEmptyPath() != url.GetWithEmptyPath() ||
      rewritten.path() != url.path()) {
    return GURL();
  }
  return rewritten;
}

AdBlockService::Cosmetic AdBlockService::CosmeticResources(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!engine_ || !url.SchemeIsHTTPOrHTTPS()) {
    // No engine yet: report generic hiding off too, so the renderer does not
    // collect classes for nothing. Non-web documents never get rules.
    return {std::string(), /*generic_hide=*/true};
  }
  CosmeticResult r = (*engine_)->cosmetic_resources(rust::Str(url.spec()));
  return {std::string(r.css), r.generic_hide};
}

std::string AdBlockService::GenericCosmeticCss(
    const GURL& url,
    const std::vector<std::string>& classes,
    const std::vector<std::string>& ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!engine_ || !url.SchemeIsHTTPOrHTTPS()) {
    return std::string();
  }
  rust::Vec<rust::String> rust_classes;
  rust_classes.reserve(classes.size());
  for (const std::string& c : classes) {
    rust_classes.push_back(rust::String(c));
  }
  rust::Vec<rust::String> rust_ids;
  rust_ids.reserve(ids.size());
  for (const std::string& id : ids) {
    rust_ids.push_back(rust::String(id));
  }
  return std::string((*engine_)->generic_cosmetic_css(
      rust::Str(url.spec()), rust_classes, rust_ids));
}

void AdBlockService::UseLists(ListTexts lists, base::OnceClosure done) {
  const int generation = ++generation_;
  // Building parses every list (about a second for EasyList): off the engine
  // sequence, so queries are answered by the old engine meanwhile. Unretained:
  // the instance is never destroyed (tests run the task environment until
  // idle before destroying theirs).
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          [](AdBlockService* service, int generation, ListTexts lists,
             base::OnceClosure done) {
            rust::Vec<ListText> texts;
            texts.reserve(lists.size());
            for (auto& [name, text] : lists) {
              texts.push_back(ListText{rust::String(name), rust::String(text)});
            }
            rust::Box<AdblockEngine> engine = new_engine_with_lists(texts);
            service->task_runner_->PostTask(
                FROM_HERE,
                base::BindOnce(&AdBlockService::SwapEngine,
                               base::Unretained(service), generation,
                               std::move(engine), std::move(done)));
          },
          base::Unretained(this), generation, std::move(lists),
          std::move(done)));
}

void AdBlockService::SwapEngine(int generation,
                                rust::Box<AdblockEngine> engine,
                                base::OnceClosure done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (generation == generation_) {
    engine_.emplace(std::move(engine));
    VLOG(1) << "Plain Browser adblock engine replaced (lists updated)";
  }
  if (done) {
    std::move(done).Run();
  }
}

}  // namespace plainbrowser::adblock
