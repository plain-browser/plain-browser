// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/renderer/adblock_url_loader_throttle.h"

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/base/net_errors.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"

namespace plainbrowser::adblock {

AdblockURLLoaderThrottle::AdblockURLLoaderThrottle(
    mojom::AdblockFilter* filter,
    const GURL& top_level_url,
    std::optional<blink::LocalFrameToken> frame_token)
    : filter_(filter),
      top_level_url_(top_level_url),
      frame_token_(std::move(frame_token)) {}

AdblockURLLoaderThrottle::~AdblockURLLoaderThrottle() = default;

void AdblockURLLoaderThrottle::DetachFromCurrentSequence() {
  // `filter_` is bound to the current sequence; get our own pipe instead.
  if (filter_) {
    filter_->Clone(pending_filter_.InitWithNewPipeAndPassReceiver());
    filter_ = nullptr;
  }
}

// static
std::string AdblockURLLoaderThrottle::RequestTypeFor(
    const network::ResourceRequest& request) {
  using network::mojom::RequestDestination;
  switch (request.destination) {
    case RequestDestination::kScript:
    case RequestDestination::kWorker:
    case RequestDestination::kSharedWorker:
    case RequestDestination::kServiceWorker:
    case RequestDestination::kAudioWorklet:
    case RequestDestination::kPaintWorklet:
      return "script";
    case RequestDestination::kImage:
      return "image";
    case RequestDestination::kStyle:
    case RequestDestination::kXslt:
      return "stylesheet";
    case RequestDestination::kFont:
      return "font";
    case RequestDestination::kAudio:
    case RequestDestination::kVideo:
    case RequestDestination::kTrack:
      return "media";
    case RequestDestination::kFrame:
    case RequestDestination::kIframe:
    case RequestDestination::kFencedframe:
      return "sub_frame";
    case RequestDestination::kObject:
    case RequestDestination::kEmbed:
      return "object";
    case RequestDestination::kEmpty:
      // fetch() / XMLHttpRequest / sendBeacon.
      return "xmlhttprequest";
    default:
      return "other";
  }
}

mojom::AdblockFilter* AdblockURLLoaderThrottle::Filter() {
  if (pending_filter_) {
    own_filter_.Bind(std::move(pending_filter_));
  }
  if (own_filter_.is_bound()) {
    return own_filter_.is_connected() ? own_filter_.get() : nullptr;
  }
  return filter_.get();
}

void AdblockURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  // Kept for the redirects: they are asked about with the same source and type.
  source_url_ = request->request_initiator.has_value()
                    ? request->request_initiator->GetURL()
                    : request->url;
  request_type_ = RequestTypeFor(*request);
  if (!request->url.SchemeIsHTTPOrHTTPS()) {
    return;
  }
  Ask(request->url, defer);
}

void AdblockURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    network::HttpRequestHeadersUpdateParams* headers_update_params) {
  // The first URL was allowed (or the filter was off); the redirect target is
  // a new URL the rules may name.
  if (!redirect_info->new_url.SchemeIsHTTPOrHTTPS()) {
    return;
  }
  Ask(redirect_info->new_url, defer);
}

void AdblockURLLoaderThrottle::Ask(const GURL& url, bool* defer) {
  mojom::AdblockFilter* filter = Filter();
  if (!filter) {
    return;  // No working filter: let the request through.
  }
  url_ = url;
  decided_ = false;
  *defer = true;
  failsafe_timer_.Start(FROM_HERE, kDecisionTimeout,
                        base::BindOnce(&AdblockURLLoaderThrottle::OnDecision,
                                       base::Unretained(this), false));
  // If the pipe closes (e.g. the browser has the filter disabled and never
  // binds it), mojo drops the callback; default to "allow" so the deferred
  // request cannot hang.
  filter->ShouldBlockRequest(
      url, source_url_, request_type_, top_level_url_, frame_token_,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&AdblockURLLoaderThrottle::OnDecision,
                         weak_factory_.GetWeakPtr()),
          /*block=*/false));
}

const char* AdblockURLLoaderThrottle::NameForLoggingWillStartRequest() {
  return "PlainBrowserAdblockThrottle";
}

void AdblockURLLoaderThrottle::OnDecision(bool block) {
  if (decided_) {
    return;  // Answer and fail-open timer can both fire; act once.
  }
  decided_ = true;
  failsafe_timer_.Stop();
  if (block) {
    delegate_->CancelWithError(net::ERR_BLOCKED_BY_CLIENT, "PlainBrowserAdblock");
    return;
  }
  delegate_->Resume();
}

}  // namespace plainbrowser::adblock
