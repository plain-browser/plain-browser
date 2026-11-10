// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_RENDERER_ADBLOCK_WEBSOCKET_THROTTLE_H_
#define PLAINBROWSER_ADBLOCK_RENDERER_ADBLOCK_WEBSOCKET_THROTTLE_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "plainbrowser/adblock/common/adblock.mojom.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"
#include "url/gurl.h"

namespace plainbrowser::adblock {

// Delays a WebSocket handshake until the browser-side filter has decided
// ("websocket" request type, so `$websocket` rules apply), then either reports
// an error to Blink (the connection fails like a blocked request) or hands the
// handshake on to `inner`, chrome's own throttle (Safe Browsing for extension
// requests), which may be null. Blink allows one throttle per WebSocket, so
// this one wraps chrome's instead of sitting next to it. Fails open: if the
// browser does not answer within 3 s the handshake goes on.
class AdblockWebSocketHandshakeThrottle
    : public blink::WebSocketHandshakeThrottle {
 public:
  // `filter` is owned by the AdblockThrottleFactory of the provider and is
  // only used inside ThrottleHandshake(). `top_level_url` may be empty, then the
  // creator's origin stands in (per-site exceptions). `frame_token` is the
  // creating frame, absent for shared and service workers.
  AdblockWebSocketHandshakeThrottle(
      mojom::AdblockFilter* filter,
      const GURL& top_level_url,
      std::optional<blink::LocalFrameToken> frame_token,
      std::unique_ptr<blink::WebSocketHandshakeThrottle> inner);
  ~AdblockWebSocketHandshakeThrottle() override;

  // blink::WebSocketHandshakeThrottle:
  void ThrottleHandshake(const blink::WebURL& url,
                         const blink::WebSecurityOrigin& creator_origin,
                         const blink::WebSecurityOrigin& isolated_world_origin,
                         OnCompletion completion) override;

 private:
  // Fail open: if the browser never answers, let the handshake through.
  static constexpr base::TimeDelta kDecisionTimeout = base::Seconds(3);

  // May destroy `this` (it runs the completion callback, which Blink answers
  // by deleting the throttle): nothing may touch members afterwards.
  void OnDecision(bool block);

  raw_ptr<mojom::AdblockFilter> filter_;
  const GURL top_level_url_;
  const std::optional<blink::LocalFrameToken> frame_token_;
  std::unique_ptr<blink::WebSocketHandshakeThrottle> inner_;

  GURL url_;
  blink::WebSecurityOrigin creator_origin_;
  blink::WebSecurityOrigin isolated_world_origin_;
  OnCompletion completion_;
  bool decided_ = false;
  base::OneShotTimer failsafe_timer_;
  base::WeakPtrFactory<AdblockWebSocketHandshakeThrottle> weak_factory_{this};
};

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_RENDERER_ADBLOCK_WEBSOCKET_THROTTLE_H_
