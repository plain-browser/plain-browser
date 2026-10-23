// Copyright Plain Browser contributors. MPL-2.0.
#ifndef DRAFTWAY_ADBLOCK_BROWSER_ADBLOCK_FILTER_IMPL_H_
#define DRAFTWAY_ADBLOCK_BROWSER_ADBLOCK_FILTER_IMPL_H_

#include "draftway/adblock/common/adblock.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace draftway::adblock {

// Binds a renderer's AdblockFilter receiver. Must run on
// AdBlockService::GetInstance()->task_runner(); all receivers live in one
// ReceiverSet on that sequence.
void BindAdblockFilter(mojo::PendingReceiver<mojom::AdblockFilter> receiver);

}  // namespace draftway::adblock

#endif  // DRAFTWAY_ADBLOCK_BROWSER_ADBLOCK_FILTER_IMPL_H_
