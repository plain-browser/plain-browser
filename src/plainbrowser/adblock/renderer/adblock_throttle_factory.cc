// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/renderer/adblock_throttle_factory.h"

#include <optional>

#include "content/public/renderer/render_thread.h"
#include "plainbrowser/adblock/renderer/adblock_url_loader_throttle.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/origin.h"

namespace plainbrowser::adblock {

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
  // A disconnected remote (browser side missing, e.g. kill switch, or the service died)
  // silently drops calls and their callbacks never run: do not defer requests on it.
  return filter_.is_bound() && filter_.is_connected() ? filter_.get() : nullptr;
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
    base::optional_ref<const blink::LocalFrameToken> local_frame_token,
    const network::ResourceRequest& request) {
  if (blink::IsRequestDestinationFrame(request.destination) ||
      !request.url.SchemeIsHTTPOrHTTPS()) {
    return nullptr;
  }
  mojom::AdblockFilter* filter = GetFilter();
  if (!filter) {
    return nullptr;
  }
  // Top-level origin of the page: frames may only be touched on the main
  // thread; worker requests fall back to the initiator (first-party workers
  // are then still covered by the exception list).
  GURL top_level_url;
  std::optional<blink::LocalFrameToken> frame_token;
  if (local_frame_token.has_value()) {
    frame_token = *local_frame_token;
    if (content::RenderThread::IsMainThread()) {
      if (blink::WebLocalFrame* frame =
              blink::WebLocalFrame::FromFrameToken(*local_frame_token)) {
        top_level_url = url::Origin(frame->Top()->GetSecurityOrigin()).GetURL();
      }
    }
  }
  if (top_level_url.is_empty() && request.request_initiator) {
    top_level_url = request.request_initiator->GetURL();
  }
  return std::make_unique<AdblockURLLoaderThrottle>(filter, top_level_url,
                                                    frame_token);
}

}  // namespace plainbrowser::adblock
