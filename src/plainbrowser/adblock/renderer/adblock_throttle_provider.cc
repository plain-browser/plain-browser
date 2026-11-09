// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/renderer/adblock_throttle_provider.h"

#include <utility>

#include "plainbrowser/adblock/renderer/adblock_throttle_factory.h"

namespace plainbrowser::adblock {

// static
std::unique_ptr<blink::URLLoaderThrottleProvider> AdblockThrottleProvider::Wrap(
    std::unique_ptr<blink::URLLoaderThrottleProvider> inner,
    blink::ThreadSafeBrowserInterfaceBrokerProxy* broker) {
  return std::make_unique<AdblockThrottleProvider>(
      std::move(inner), AdblockThrottleFactory::Create(broker));
}

AdblockThrottleProvider::AdblockThrottleProvider(
    std::unique_ptr<blink::URLLoaderThrottleProvider> inner,
    std::unique_ptr<AdblockThrottleFactory> factory)
    : inner_(std::move(inner)), factory_(std::move(factory)) {}

AdblockThrottleProvider::~AdblockThrottleProvider() = default;

std::unique_ptr<blink::URLLoaderThrottleProvider>
AdblockThrottleProvider::Clone() {
  return std::make_unique<AdblockThrottleProvider>(
      inner_->Clone(), factory_ ? factory_->Clone() : nullptr);
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
AdblockThrottleProvider::CreateThrottles(
    base::optional_ref<const blink::LocalFrameToken> local_frame_token,
    const network::ResourceRequest& request) {
  auto throttles = inner_->CreateThrottles(local_frame_token, request);
  if (factory_) {
    // Subresources only (null for frame resources). First, so a request the
    // filter blocks is cancelled before the other throttles look at it.
    if (auto throttle =
            factory_->MaybeCreateThrottle(local_frame_token, request)) {
      throttles.insert(throttles.begin(), std::move(throttle));
    }
  }
  return throttles;
}

void AdblockThrottleProvider::SetOnline(bool is_online) {
  inner_->SetOnline(is_online);
}

}  // namespace plainbrowser::adblock
