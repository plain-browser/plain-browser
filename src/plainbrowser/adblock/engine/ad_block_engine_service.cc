// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/engine/ad_block_engine_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "url/gurl.h"

namespace plainbrowser::adblock {

namespace {

// Runs `method` on the engine's sequence and `callback` with its result back on
// the calling (mojo) sequence.
template <typename Callback, typename Method, typename... Args>
void OnEngine(AdBlockEngine* engine,
              Callback callback,
              Method method,
              Args&&... args) {
  engine->task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(method, base::Unretained(engine),
                     std::forward<Args>(args)...),
      std::move(callback));
}

}  // namespace

AdBlockEngineService::AdBlockEngineService(
    mojo::PendingReceiver<mojom::AdblockEngine> receiver)
    : engine_(new AdBlockEngine()), receiver_(this, std::move(receiver)) {}

AdBlockEngineService::~AdBlockEngineService() = default;

void AdBlockEngineService::UseLists(std::vector<mojom::NamedListPtr> lists,
                                    UseListsCallback callback) {
  AdBlockEngine::ListTexts texts;
  texts.reserve(lists.size());
  for (mojom::NamedListPtr& list : lists) {
    texts.emplace_back(std::move(list->name), std::move(list->text));
  }
  // `done` runs on the engine sequence; answer on ours.
  engine_->UseLists(std::move(texts),
                    base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void AdBlockEngineService::ShouldBlock(const GURL& url,
                                       const GURL& source_url,
                                       const std::string& request_type,
                                       ShouldBlockCallback callback) {
  OnEngine(engine_.get(), std::move(callback), &AdBlockEngine::ShouldBlock, url,
           source_url, request_type);
}

void AdBlockEngineService::CosmeticResources(
    const GURL& url,
    CosmeticResourcesCallback callback) {
  OnEngine(engine_.get(),
           base::BindOnce(
               [](CosmeticResourcesCallback callback,
                  AdBlockEngine::Cosmetic cosmetic) {
                 std::move(callback).Run(mojom::CosmeticResult::New(
                     std::move(cosmetic.css), cosmetic.generic_hide,
                     std::move(cosmetic.procedural_actions),
                     std::move(cosmetic.procedural_selectors)));
               },
               std::move(callback)),
           &AdBlockEngine::CosmeticResources, url);
}

void AdBlockEngineService::Scriptlets(const GURL& url,
                                      ScriptletsCallback callback) {
  OnEngine(engine_.get(), std::move(callback), &AdBlockEngine::Scriptlets, url);
}

void AdBlockEngineService::RedirectUrl(const GURL& url,
                                       const GURL& source_url,
                                       RedirectUrlCallback callback) {
  OnEngine(engine_.get(), std::move(callback), &AdBlockEngine::RedirectUrl, url,
           source_url);
}

void AdBlockEngineService::RedirectHosts(RedirectHostsCallback callback) {
  OnEngine(engine_.get(), std::move(callback), &AdBlockEngine::RedirectHosts);
}

void AdBlockEngineService::GenericCosmeticCss(
    const GURL& url,
    const std::vector<std::string>& classes,
    const std::vector<std::string>& ids,
    GenericCosmeticCssCallback callback) {
  OnEngine(engine_.get(), std::move(callback),
           &AdBlockEngine::GenericCosmeticCss, url, classes, ids);
}

void AdBlockEngineService::RewrittenUrl(const GURL& url,
                                        const GURL& source_url,
                                        const std::string& request_type,
                                        RewrittenUrlCallback callback) {
  OnEngine(engine_.get(), std::move(callback), &AdBlockEngine::RewrittenUrl,
           url, source_url, request_type);
}

}  // namespace plainbrowser::adblock
