// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/renderer/adblock_websocket_throttle_provider.h"

#include <utility>

#include "plainbrowser/adblock/renderer/adblock_throttle_factory.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"

namespace plainbrowser::adblock {

// static
std::unique_ptr<blink::WebSocketHandshakeThrottleProvider>
AdblockWebSocketThrottleProvider::Wrap(
    std::unique_ptr<blink::WebSocketHandshakeThrottleProvider> inner,
    blink::ThreadSafeBrowserInterfaceBrokerProxy* broker) {
  return std::make_unique<AdblockWebSocketThrottleProvider>(
      std::move(inner), AdblockThrottleFactory::Create(broker));
}

AdblockWebSocketThrottleProvider::AdblockWebSocketThrottleProvider(
    std::unique_ptr<blink::WebSocketHandshakeThrottleProvider> inner,
    std::unique_ptr<AdblockThrottleFactory> factory)
    : inner_(std::move(inner)), factory_(std::move(factory)) {}

AdblockWebSocketThrottleProvider::~AdblockWebSocketThrottleProvider() = default;

std::unique_ptr<blink::WebSocketHandshakeThrottleProvider>
AdblockWebSocketThrottleProvider::Clone(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return std::make_unique<AdblockWebSocketThrottleProvider>(
      inner_->Clone(task_runner), factory_ ? factory_->Clone() : nullptr);
}

std::unique_ptr<blink::WebSocketHandshakeThrottle>
AdblockWebSocketThrottleProvider::CreateThrottle(
    base::optional_ref<const blink::LocalFrameToken> local_frame_token,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  std::unique_ptr<blink::WebSocketHandshakeThrottle> chrome_throttle =
      inner_->CreateThrottle(local_frame_token, std::move(task_runner));
  if (!factory_) {
    return chrome_throttle;
  }
  return factory_->CreateWebSocketThrottle(local_frame_token,
                                           std::move(chrome_throttle));
}

}  // namespace plainbrowser::adblock
