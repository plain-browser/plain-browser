// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_RENDERER_ADBLOCK_THROTTLE_FACTORY_H_
#define PLAINBROWSER_ADBLOCK_RENDERER_ADBLOCK_THROTTLE_FACTORY_H_

#include <memory>

#include "base/sequence_checker.h"
#include "base/types/optional_ref.h"
#include "plainbrowser/adblock/common/adblock.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace blink {
class ThreadSafeBrowserInterfaceBrokerProxy;
class URLLoaderThrottle;
class WebSocketHandshakeThrottle;
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

  // Returns nullptr for frame (navigation) resources. `local_frame_token` is
  // the requesting frame (absent for workers); on the main thread it gives the
  // top-level frame's origin, which the per-site exception list is keyed by.
  std::unique_ptr<blink::URLLoaderThrottle> MaybeCreateThrottle(
      base::optional_ref<const blink::LocalFrameToken> local_frame_token,
      const network::ResourceRequest& request);

  // For WebSocketHandshakeThrottleProvider::CreateThrottle(): a throttle that
  // asks the filter first and then hands the handshake to `inner` (chrome's
  // throttle, may be null). Returns `inner` itself when there is no working
  // filter, so a missing filter never blocks or delays a WebSocket.
  std::unique_ptr<blink::WebSocketHandshakeThrottle> CreateWebSocketThrottle(
      base::optional_ref<const blink::LocalFrameToken> local_frame_token,
      std::unique_ptr<blink::WebSocketHandshakeThrottle> inner);

 private:
  mojom::AdblockFilter* GetFilter();

  // Origin of the page's top-level frame, for the per-site exception list.
  // Frames may only be touched on the main thread; empty otherwise.
  static GURL TopLevelUrl(
      base::optional_ref<const blink::LocalFrameToken> local_frame_token);

  mojo::PendingRemote<mojom::AdblockFilter> pending_filter_;
  mojo::Remote<mojom::AdblockFilter> filter_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_RENDERER_ADBLOCK_THROTTLE_FACTORY_H_
