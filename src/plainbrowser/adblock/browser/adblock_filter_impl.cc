// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/browser/adblock_filter_impl.h"

#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "plainbrowser/adblock/browser/ad_block_service.h"
#include "plainbrowser/shields/browser/blocked_requests_counter.h"
#include "plainbrowser/shields/browser/shields_service.h"
#include "plainbrowser/shields/common/site_util.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace plainbrowser::adblock {
namespace {

// Upper bound on the class names and ids accepted in one
// GetGenericCosmeticCss call (the renderer sends at most 2000).
constexpr size_t kMaxGenericTokensPerCall = 10000;

bool IsExceptedPage(const GURL& top_level_url) {
  return shields::ShieldsService::GetInstance()->IsExcepted(
      shields::SiteFor(top_level_url));
}

// Receiver context: the renderer process id.
class AdblockFilterImpl : public mojom::AdblockFilter {
 public:
  AdblockFilterImpl() = default;

  void AddReceiver(int process_id,
                   mojo::PendingReceiver<mojom::AdblockFilter> receiver) {
    receivers_.Add(this, std::move(receiver), process_id);
  }

  // mojom::AdblockFilter:
  void Clone(mojo::PendingReceiver<mojom::AdblockFilter> receiver) override {
    AddReceiver(receivers_.current_context(), std::move(receiver));
  }

  void ShouldBlockRequest(
      const GURL& url,
      const GURL& source_url,
      const std::string& request_type,
      const GURL& top_level_url,
      const std::optional<blink::LocalFrameToken>& frame_token,
      ShouldBlockRequestCallback callback) override {
    // Per-site exception: the user turned protections off for the page.
    if (IsExceptedPage(top_level_url)) {
      std::move(callback).Run(false);
      return;
    }
    AdBlockService::GetInstance()->ShouldBlock(
        url, source_url, request_type,
        base::BindOnce(
            [](int process_id, std::optional<blink::LocalFrameToken> frame_token,
               ShouldBlockRequestCallback callback, bool block) {
              if (block && frame_token) {
                shields::BlockedRequestsCounter::CountBlocked(process_id,
                                                              *frame_token);
              }
              std::move(callback).Run(block);
            },
            receivers_.current_context(), frame_token, std::move(callback)));
  }

  void GetCosmeticCss(const GURL& url,
                      const GURL& top_level_url,
                      GetCosmeticCssCallback callback) override {
    if (IsExceptedPage(top_level_url)) {
      std::move(callback).Run(std::string(), /*generic_hide=*/true, {}, {});
      return;
    }
    AdBlockService::GetInstance()->CosmeticResources(
        url, base::BindOnce(
                 [](GetCosmeticCssCallback callback,
                    mojom::CosmeticResultPtr cosmetic) {
                   std::move(callback).Run(
                       std::move(cosmetic->css), cosmetic->generic_hide,
                       std::move(cosmetic->procedural_actions),
                       std::move(cosmetic->procedural_selectors));
                 },
                 std::move(callback)));
  }

  void GetScriptlets(const GURL& url,
                     const GURL& top_level_url,
                     GetScriptletsCallback callback) override {
    if (IsExceptedPage(top_level_url)) {
      std::move(callback).Run(std::string());
      return;
    }
    AdBlockService::GetInstance()->Scriptlets(url, std::move(callback));
  }

  void GetRedirectHosts(GetRedirectHostsCallback callback) override {
    AdBlockService::GetInstance()->RedirectHosts(std::move(callback));
  }

  void GetRedirect(const GURL& url,
                   const GURL& source_url,
                   const GURL& top_level_url,
                   const std::optional<blink::LocalFrameToken>& frame_token,
                   GetRedirectCallback callback) override {
    if (IsExceptedPage(top_level_url)) {
      std::move(callback).Run(std::string());
      return;
    }
    AdBlockService::GetInstance()->RedirectUrl(
        url, source_url,
        base::BindOnce(
            [](int process_id, std::optional<blink::LocalFrameToken> frame_token,
               GetRedirectCallback callback, const std::string& data_url) {
              if (!data_url.empty() && frame_token) {
                shields::BlockedRequestsCounter::CountBlocked(process_id,
                                                              *frame_token);
              }
              std::move(callback).Run(data_url);
            },
            receivers_.current_context(), frame_token, std::move(callback)));
  }

  void GetGenericCosmeticCss(const GURL& url,
                             const GURL& top_level_url,
                             const std::vector<std::string>& classes,
                             const std::vector<std::string>& ids,
                             GetGenericCosmeticCssCallback callback) override {
    // A renderer sends at most kMaxGenericTokens per call (renderer side);
    // anything far beyond that is not a page, so do not spend the engine on it.
    if (IsExceptedPage(top_level_url) ||
        classes.size() + ids.size() > kMaxGenericTokensPerCall) {
      std::move(callback).Run(std::string());
      return;
    }
    AdBlockService::GetInstance()->GenericCosmeticCss(url, classes, ids,
                                                      std::move(callback));
  }

 private:
  mojo::ReceiverSet<mojom::AdblockFilter, int> receivers_;
};

AdblockFilterImpl& GetImpl() {
  static base::NoDestructor<AdblockFilterImpl> impl;
  return *impl;
}

}  // namespace

void BindAdblockFilter(int process_id,
                       mojo::PendingReceiver<mojom::AdblockFilter> receiver) {
  DCHECK(AdBlockService::GetInstance()
             ->task_runner()
             ->RunsTasksInCurrentSequence());
  GetImpl().AddReceiver(process_id, std::move(receiver));
}

}  // namespace plainbrowser::adblock
