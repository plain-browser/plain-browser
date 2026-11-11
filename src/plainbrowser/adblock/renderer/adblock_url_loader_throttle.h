// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_RENDERER_ADBLOCK_URL_LOADER_THROTTLE_H_
#define PLAINBROWSER_ADBLOCK_RENDERER_ADBLOCK_URL_LOADER_THROTTLE_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "plainbrowser/adblock/common/adblock.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace plainbrowser::adblock {

// Defers each subresource request, asks the browser-side filter, then cancels
// with ERR_BLOCKED_BY_CLIENT or resumes; every redirect target is asked again
// (a request to an allowed URL that redirects to a blocked one is blocked at
// the redirect). Frame (navigation) requests never reach a renderer throttle:
// SubframeNavigationThrottle (browser side) handles those.
class AdblockURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  // `filter` is owned by the AdblockThrottleFactory of the creating
  // URLLoaderThrottleProvider and outlives requests on this sequence (the same
  // contract Chrome's safe_browsing::RendererURLLoaderThrottle relies on).
  // `top_level_url`: origin of the page's top-level frame (per-site
  // exceptions); `frame_token`: the requesting frame, if known (per-tab
  // blocked-request counter).
  AdblockURLLoaderThrottle(mojom::AdblockFilter* filter,
                           const GURL& top_level_url,
                           std::optional<blink::LocalFrameToken> frame_token);
  ~AdblockURLLoaderThrottle() override;

  // blink::URLLoaderThrottle:
  void DetachFromCurrentSequence() override;
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      network::HttpRequestHeadersUpdateParams* headers_update_params) override;
  const char* NameForLoggingWillStartRequest() override;

  // Maps a fetch destination to an adblock-rust request type.
  static std::string RequestTypeFor(const network::ResourceRequest& request);

 private:
  // The filter to ask, or null when there is none that works (then the request
  // goes through).
  mojom::AdblockFilter* Filter();
  // Defers the request and asks the filter about `url`; `*defer` stays false
  // when there is no filter. The request type and the source are those of the
  // request this throttle was started for.
  void Ask(const GURL& url, bool* defer);
  void OnDecision(bool block);

  // Fail open: if the browser never answers, let the request through after this long.
  static constexpr base::TimeDelta kDecisionTimeout = base::Seconds(3);
  bool decided_ = false;
  base::OneShotTimer failsafe_timer_;

  raw_ptr<mojom::AdblockFilter> filter_;
  // Used after DetachFromCurrentSequence(): bound lazily on the new sequence.
  mojo::PendingRemote<mojom::AdblockFilter> pending_filter_;
  mojo::Remote<mojom::AdblockFilter> own_filter_;
  GURL url_;
  GURL source_url_;
  std::string request_type_;
  const GURL top_level_url_;
  const std::optional<blink::LocalFrameToken> frame_token_;
  base::WeakPtrFactory<AdblockURLLoaderThrottle> weak_factory_{this};
};

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_RENDERER_ADBLOCK_URL_LOADER_THROTTLE_H_
