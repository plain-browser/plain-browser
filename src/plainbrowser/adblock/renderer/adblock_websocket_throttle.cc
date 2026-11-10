// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/renderer/adblock_websocket_throttle.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "url/origin.h"

namespace plainbrowser::adblock {

AdblockWebSocketHandshakeThrottle::AdblockWebSocketHandshakeThrottle(
    mojom::AdblockFilter* filter,
    const GURL& top_level_url,
    std::optional<blink::LocalFrameToken> frame_token,
    std::unique_ptr<blink::WebSocketHandshakeThrottle> inner)
    : filter_(filter),
      top_level_url_(top_level_url),
      frame_token_(std::move(frame_token)),
      inner_(std::move(inner)) {}

AdblockWebSocketHandshakeThrottle::~AdblockWebSocketHandshakeThrottle() =
    default;

void AdblockWebSocketHandshakeThrottle::ThrottleHandshake(
    const blink::WebURL& url,
    const blink::WebSecurityOrigin& creator_origin,
    const blink::WebSecurityOrigin& isolated_world_origin,
    OnCompletion completion) {
  url_ = GURL(url);
  creator_origin_ = creator_origin;
  isolated_world_origin_ = isolated_world_origin;
  completion_ = std::move(completion);

  const GURL source_url = url::Origin(creator_origin).GetURL();
  const GURL& top_level_url =
      top_level_url_.is_empty() ? source_url : top_level_url_;
  failsafe_timer_.Start(
      FROM_HERE, kDecisionTimeout,
      base::BindOnce(&AdblockWebSocketHandshakeThrottle::OnDecision,
                     base::Unretained(this), false));
  // A dropped callback (pipe closed) answers "allow", so the handshake cannot
  // hang on a filter that went away.
  filter_->ShouldBlockRequest(
      url_, source_url, "websocket", top_level_url, frame_token_,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&AdblockWebSocketHandshakeThrottle::OnDecision,
                         weak_factory_.GetWeakPtr()),
          /*block=*/false));
}

void AdblockWebSocketHandshakeThrottle::OnDecision(bool block) {
  if (decided_) {
    return;  // Answer and fail-open timer can both fire; act once.
  }
  decided_ = true;
  failsafe_timer_.Stop();
  if (block) {
    std::move(completion_).Run(blink::WebString::FromUTF8(
        "WebSocket connection blocked by the Plain Browser ad blocker"));
    return;  // `this` is gone.
  }
  if (!inner_) {
    std::move(completion_).Run(std::nullopt);
    return;  // `this` is gone.
  }
  // Chrome's throttle takes over and calls the completion itself.
  inner_->ThrottleHandshake(blink::WebURL(url_), creator_origin_,
                            isolated_world_origin_, std::move(completion_));
}

}  // namespace plainbrowser::adblock
