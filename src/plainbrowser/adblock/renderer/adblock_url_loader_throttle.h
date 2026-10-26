// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_RENDERER_ADBLOCK_URL_LOADER_THROTTLE_H_
#define PLAINBROWSER_ADBLOCK_RENDERER_ADBLOCK_URL_LOADER_THROTTLE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "plainbrowser/adblock/common/adblock.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/gurl.h"

namespace plainbrowser::adblock {

// Defers each subresource request, asks the browser-side filter, then cancels
// with ERR_BLOCKED_BY_CLIENT or resumes. Frame (navigation) requests are not
// handled here.
class AdblockURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  // `filter` is owned by the AdblockThrottleFactory of the creating
  // URLLoaderThrottleProvider and outlives requests on this sequence (the same
  // contract Chrome's safe_browsing::RendererURLLoaderThrottle relies on).
  explicit AdblockURLLoaderThrottle(mojom::AdblockFilter* filter);
  ~AdblockURLLoaderThrottle() override;

  // blink::URLLoaderThrottle:
  void DetachFromCurrentSequence() override;
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  const char* NameForLoggingWillStartRequest() override;

  // Maps a fetch destination to an adblock-rust request type.
  static std::string RequestTypeFor(const network::ResourceRequest& request);

 private:
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
  base::WeakPtrFactory<AdblockURLLoaderThrottle> weak_factory_{this};
};

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_RENDERER_ADBLOCK_URL_LOADER_THROTTLE_H_
