// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_BROWSER_ADBLOCK_FILTER_IMPL_H_
#define PLAINBROWSER_ADBLOCK_BROWSER_ADBLOCK_FILTER_IMPL_H_

#include "plainbrowser/adblock/common/adblock.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace plainbrowser::adblock {

// Binds a renderer's AdblockFilter receiver. `process_id` is the renderer
// process the receiver belongs to (used to resolve frame tokens for the
// per-tab blocked-request counter). Must run on
// AdBlockService::GetInstance()->task_runner(); all receivers live in one
// ReceiverSet on that sequence.
void BindAdblockFilter(int process_id,
                       mojo::PendingReceiver<mojom::AdblockFilter> receiver);

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_BROWSER_ADBLOCK_FILTER_IMPL_H_
