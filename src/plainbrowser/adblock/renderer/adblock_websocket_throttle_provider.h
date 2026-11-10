// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_RENDERER_ADBLOCK_WEBSOCKET_THROTTLE_PROVIDER_H_
#define PLAINBROWSER_ADBLOCK_RENDERER_ADBLOCK_WEBSOCKET_THROTTLE_PROVIDER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/optional_ref.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle_provider.h"

namespace blink {
class ThreadSafeBrowserInterfaceBrokerProxy;
class WebSocketHandshakeThrottle;
}  // namespace blink

namespace plainbrowser::adblock {

class AdblockThrottleFactory;

// Wraps chrome's WebSocketHandshakeThrottleProviderImpl (as
// AdblockThrottleProvider wraps the URL loader one): every throttle it creates
// is an AdblockWebSocketHandshakeThrottle around chrome's. Chrome's provider is
// not touched; ChromeContentRendererClient::
// CreateWebSocketHandshakeThrottleProvider returns Wrap(...) (patches/20-hooks).
// Clone() wraps the clone, so worker threads are covered too.
class AdblockWebSocketThrottleProvider
    : public blink::WebSocketHandshakeThrottleProvider {
 public:
  // `broker` is the render thread's interface broker; used on the calling
  // (main) thread.
  static std::unique_ptr<blink::WebSocketHandshakeThrottleProvider> Wrap(
      std::unique_ptr<blink::WebSocketHandshakeThrottleProvider> inner,
      blink::ThreadSafeBrowserInterfaceBrokerProxy* broker);

  AdblockWebSocketThrottleProvider(
      std::unique_ptr<blink::WebSocketHandshakeThrottleProvider> inner,
      std::unique_ptr<AdblockThrottleFactory> factory);
  AdblockWebSocketThrottleProvider(const AdblockWebSocketThrottleProvider&) =
      delete;
  AdblockWebSocketThrottleProvider& operator=(
      const AdblockWebSocketThrottleProvider&) = delete;
  ~AdblockWebSocketThrottleProvider() override;

  // blink::WebSocketHandshakeThrottleProvider:
  std::unique_ptr<blink::WebSocketHandshakeThrottleProvider> Clone(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  std::unique_ptr<blink::WebSocketHandshakeThrottle> CreateThrottle(
      base::optional_ref<const blink::LocalFrameToken> local_frame_token,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;

 private:
  std::unique_ptr<blink::WebSocketHandshakeThrottleProvider> inner_;
  std::unique_ptr<AdblockThrottleFactory> factory_;
};

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_RENDERER_ADBLOCK_WEBSOCKET_THROTTLE_PROVIDER_H_
