// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/renderer/adblock_url_loader_throttle.h"

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"

namespace plainbrowser::adblock {

AdblockURLLoaderThrottle::AdblockURLLoaderThrottle(mojom::AdblockFilter* filter)
    : filter_(filter) {}

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

void AdblockURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  if (!request->url.SchemeIsHTTPOrHTTPS()) {
    return;
  }
  if (pending_filter_) {
    own_filter_.Bind(std::move(pending_filter_));
  }
  mojom::AdblockFilter* filter = nullptr;
  if (own_filter_.is_bound()) {
    filter = own_filter_.is_connected() ? own_filter_.get() : nullptr;
  } else {
    filter = filter_.get();
  }
  if (!filter) {
    return;  // No working filter: let the request through.
  }
  url_ = request->url;
  const GURL source_url = request->request_initiator.has_value()
                              ? request->request_initiator->GetURL()
                              : request->url;
  *defer = true;
  failsafe_timer_.Start(FROM_HERE, kDecisionTimeout,
                        base::BindOnce(&AdblockURLLoaderThrottle::OnDecision,
                                       base::Unretained(this), false));
  // If the pipe closes (e.g. the browser has the filter disabled and never
  // binds it), mojo drops the callback; default to "allow" so the deferred
  // request cannot hang.
  filter->ShouldBlockRequest(
      request->url, source_url, RequestTypeFor(*request),
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
