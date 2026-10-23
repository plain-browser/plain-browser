// Copyright Draftway. MPL-2.0.
#include "draftway/adblock/browser/renderer_interfaces.h"

#include "base/functional/bind.h"
#include "draftway/adblock/browser/ad_block_service.h"
#include "draftway/adblock/browser/adblock_filter_impl.h"
#include "draftway/adblock/common/adblock.mojom.h"

namespace draftway::adblock {

// Feature name "DraftwayAdblock" (derived from the identifier).
BASE_FEATURE(kDraftwayAdblock, base::FEATURE_ENABLED_BY_DEFAULT);

void ExposeInterfacesToRenderer(service_manager::BinderRegistry* registry) {
  if (!base::FeatureList::IsEnabled(kDraftwayAdblock)) {
    return;
  }
  // The binder runs on the engine sequence, so filter calls never touch the
  // UI or IO thread.
  registry->AddInterface<mojom::AdblockFilter>(
      base::BindRepeating(&BindAdblockFilter),
      AdBlockService::GetInstance()->task_runner());
}

}  // namespace draftway::adblock
