// Copyright Draftway. MPL-2.0.
#include "draftway/adblock/renderer/adblock_throttle_factory.h"

#include "draftway/adblock/renderer/adblock_url_loader_throttle.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"

namespace draftway::adblock {

// static
std::unique_ptr<AdblockThrottleFactory> AdblockThrottleFactory::Create(
    blink::ThreadSafeBrowserInterfaceBrokerProxy* broker) {
  mojo::PendingRemote<mojom::AdblockFilter> pending;
  broker->GetInterface(pending.InitWithNewPipeAndPassReceiver());
  return std::make_unique<AdblockThrottleFactory>(std::move(pending));
}

AdblockThrottleFactory::AdblockThrottleFactory(
    mojo::PendingRemote<mojom::AdblockFilter> pending_filter)
    : pending_filter_(std::move(pending_filter)) {
  // Constructed on the render main thread, used on the provider's sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

AdblockThrottleFactory::~AdblockThrottleFactory() = default;

mojom::AdblockFilter* AdblockThrottleFactory::GetFilter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_filter_) {
    filter_.Bind(std::move(pending_filter_));
  }
  return filter_.is_bound() ? filter_.get() : nullptr;
}

std::unique_ptr<AdblockThrottleFactory> AdblockThrottleFactory::Clone() {
  mojo::PendingRemote<mojom::AdblockFilter> pending;
  if (mojom::AdblockFilter* filter = GetFilter()) {
    filter->Clone(pending.InitWithNewPipeAndPassReceiver());
  }
  return std::make_unique<AdblockThrottleFactory>(std::move(pending));
}

std::unique_ptr<blink::URLLoaderThrottle>
AdblockThrottleFactory::MaybeCreateThrottle(
    const network::ResourceRequest& request) {
  if (blink::IsRequestDestinationFrame(request.destination) ||
      !request.url.SchemeIsHTTPOrHTTPS()) {
    return nullptr;
  }
  mojom::AdblockFilter* filter = GetFilter();
  if (!filter) {
    return nullptr;
  }
  return std::make_unique<AdblockURLLoaderThrottle>(filter);
}

}  // namespace draftway::adblock
