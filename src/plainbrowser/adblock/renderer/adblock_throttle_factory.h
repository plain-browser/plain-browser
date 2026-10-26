// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_RENDERER_ADBLOCK_THROTTLE_FACTORY_H_
#define PLAINBROWSER_ADBLOCK_RENDERER_ADBLOCK_THROTTLE_FACTORY_H_

#include <memory>

#include "base/sequence_checker.h"
#include "plainbrowser/adblock/common/adblock.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace blink {
class ThreadSafeBrowserInterfaceBrokerProxy;
class URLLoaderThrottle;
}  // namespace blink

namespace network {
struct ResourceRequest;
}  // namespace network

namespace plainbrowser::adblock {

// Owned by chrome's URLLoaderThrottleProviderImpl (one per provider, i.e. per
// thread). Mirrors how the provider handles the SafeBrowsing remote.
class AdblockThrottleFactory {
 public:
  // Must be called where `broker` may be used (provider Create()).
  static std::unique_ptr<AdblockThrottleFactory> Create(
      blink::ThreadSafeBrowserInterfaceBrokerProxy* broker);

  explicit AdblockThrottleFactory(
      mojo::PendingRemote<mojom::AdblockFilter> pending_filter);
  ~AdblockThrottleFactory();

  // For URLLoaderThrottleProvider::Clone(): a factory for another sequence.
  std::unique_ptr<AdblockThrottleFactory> Clone();

  // Returns nullptr for frame (navigation) resources.
  std::unique_ptr<blink::URLLoaderThrottle> MaybeCreateThrottle(
      const network::ResourceRequest& request);

 private:
  mojom::AdblockFilter* GetFilter();

  mojo::PendingRemote<mojom::AdblockFilter> pending_filter_;
  mojo::Remote<mojom::AdblockFilter> filter_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_RENDERER_ADBLOCK_THROTTLE_FACTORY_H_
