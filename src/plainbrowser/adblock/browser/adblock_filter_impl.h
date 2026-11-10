// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_BROWSER_ADBLOCK_FILTER_IMPL_H_
#define PLAINBROWSER_ADBLOCK_BROWSER_ADBLOCK_FILTER_IMPL_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "plainbrowser/adblock/common/adblock.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace plainbrowser::adblock {

// Binds a renderer's AdblockFilter receiver. `process_id` is the renderer
// process the receiver belongs to (used to resolve frame tokens for the
// per-tab blocked-request counter). Must run on
// AdBlockService::GetInstance()->task_runner(); all receivers live in one
// ReceiverSet on that sequence.
void BindAdblockFilter(int process_id,
                       mojo::PendingReceiver<mojom::AdblockFilter> receiver);

// The decision for one request, with everything that goes with it: the
// per-site exception list and the global switch (not blocked), then the engine,
// then the counters and the request log for a block. `process_id` and
// `frame_token` name the requesting frame for the per-tab counter. Runs on
// AdBlockService::GetInstance()->task_runner(); `callback` runs there too. Used
// by the renderers' AdblockFilter and by the browser-side WebSocket filter.
void DecideRequest(const GURL& url,
                   const GURL& source_url,
                   const std::string& request_type,
                   const GURL& top_level_url,
                   int process_id,
                   std::optional<blink::LocalFrameToken> frame_token,
                   base::OnceCallback<void(bool)> callback);

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_BROWSER_ADBLOCK_FILTER_IMPL_H_
