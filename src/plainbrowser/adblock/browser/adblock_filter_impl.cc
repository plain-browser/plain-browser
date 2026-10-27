// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/browser/adblock_filter_impl.h"

#include <optional>
#include <string>

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
    const bool block = AdBlockService::GetInstance()->ShouldBlock(
        url, source_url, request_type);
    if (block && frame_token) {
      shields::BlockedRequestsCounter::CountBlocked(
          receivers_.current_context(), *frame_token);
    }
    std::move(callback).Run(block);
  }

  void GetCosmeticCss(const GURL& url,
                      const GURL& top_level_url,
                      GetCosmeticCssCallback callback) override {
    if (IsExceptedPage(top_level_url)) {
      std::move(callback).Run(std::string());
      return;
    }
    std::move(callback).Run(AdBlockService::GetInstance()->CosmeticCss(url));
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
