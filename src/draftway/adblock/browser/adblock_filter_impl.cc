// Copyright Plain Browser contributors. MPL-2.0.
#include "draftway/adblock/browser/adblock_filter_impl.h"

#include <string>

#include "base/no_destructor.h"
#include "draftway/adblock/browser/ad_block_service.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "url/gurl.h"

namespace draftway::adblock {
namespace {

class AdblockFilterImpl : public mojom::AdblockFilter {
 public:
  AdblockFilterImpl() = default;

  void AddReceiver(mojo::PendingReceiver<mojom::AdblockFilter> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  // mojom::AdblockFilter:
  void Clone(mojo::PendingReceiver<mojom::AdblockFilter> receiver) override {
    AddReceiver(std::move(receiver));
  }

  void ShouldBlockRequest(const GURL& url,
                          const GURL& source_url,
                          const std::string& request_type,
                          ShouldBlockRequestCallback callback) override {
    std::move(callback).Run(AdBlockService::GetInstance()->ShouldBlock(
        url, source_url, request_type));
  }

  void GetCosmeticCss(const GURL& url,
                      GetCosmeticCssCallback callback) override {
    std::move(callback).Run(AdBlockService::GetInstance()->CosmeticCss(url));
  }

 private:
  mojo::ReceiverSet<mojom::AdblockFilter> receivers_;
};

AdblockFilterImpl& GetImpl() {
  static base::NoDestructor<AdblockFilterImpl> impl;
  return *impl;
}

}  // namespace

void BindAdblockFilter(mojo::PendingReceiver<mojom::AdblockFilter> receiver) {
  DCHECK(AdBlockService::GetInstance()
             ->task_runner()
             ->RunsTasksInCurrentSequence());
  GetImpl().AddReceiver(std::move(receiver));
}

}  // namespace draftway::adblock
