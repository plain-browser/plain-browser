// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_BROWSER_ADBLOCK_FILTER_IMPL_H_
#define PLAINBROWSER_ADBLOCK_BROWSER_ADBLOCK_FILTER_IMPL_H_

#include "plainbrowser/adblock/common/adblock.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace plainbrowser::adblock {

// Binds a renderer's AdblockFilter receiver. Must run on
// AdBlockService::GetInstance()->task_runner(); all receivers live in one
// ReceiverSet on that sequence.
void BindAdblockFilter(mojo::PendingReceiver<mojom::AdblockFilter> receiver);

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_BROWSER_ADBLOCK_FILTER_IMPL_H_
