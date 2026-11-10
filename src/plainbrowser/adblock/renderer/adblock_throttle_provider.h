// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_RENDERER_ADBLOCK_THROTTLE_PROVIDER_H_
#define PLAINBROWSER_ADBLOCK_RENDERER_ADBLOCK_THROTTLE_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/types/optional_ref.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/url_loader_throttle_provider.h"

namespace blink {
class ThreadSafeBrowserInterfaceBrokerProxy;
}  // namespace blink

namespace network {
struct ResourceRequest;
}  // namespace network

namespace plainbrowser::adblock {

class AdblockThrottleFactory;

// Wraps chrome's URLLoaderThrottleProvider and puts the adblock throttle in
// front of the throttles it creates for every subresource request. Chrome's
// provider is not touched: ChromeContentRendererClient::
// CreateURLLoaderThrottleProvider returns Wrap(URLLoaderThrottleProviderImpl::
// Create(...)) (patches/20-hooks). Clone() wraps the clone, so worker
// threads and background resource fetch get the throttle too.
class AdblockThrottleProvider : public blink::URLLoaderThrottleProvider {
 public:
  // `broker` is the render thread's interface broker (the one chrome's
  // provider is created with); it is used here, on the main thread.
  static std::unique_ptr<blink::URLLoaderThrottleProvider> Wrap(
      std::unique_ptr<blink::URLLoaderThrottleProvider> inner,
      blink::ThreadSafeBrowserInterfaceBrokerProxy* broker);

  AdblockThrottleProvider(
      std::unique_ptr<blink::URLLoaderThrottleProvider> inner,
      std::unique_ptr<AdblockThrottleFactory> factory);
  AdblockThrottleProvider(const AdblockThrottleProvider&) = delete;
  AdblockThrottleProvider& operator=(const AdblockThrottleProvider&) = delete;
  ~AdblockThrottleProvider() override;

  // blink::URLLoaderThrottleProvider:
  std::unique_ptr<blink::URLLoaderThrottleProvider> Clone() override;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> CreateThrottles(
      base::optional_ref<const blink::LocalFrameToken> local_frame_token,
      const network::ResourceRequest& request) override;
  void SetOnline(bool is_online) override;

 private:
  std::unique_ptr<blink::URLLoaderThrottleProvider> inner_;
  // Null when the filter interface is unavailable.
  std::unique_ptr<AdblockThrottleFactory> factory_;
};

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_RENDERER_ADBLOCK_THROTTLE_PROVIDER_H_
