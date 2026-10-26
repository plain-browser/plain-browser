// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/browser/renderer_interfaces.h"

#include "base/functional/bind.h"
#include "plainbrowser/adblock/browser/ad_block_service.h"
#include "plainbrowser/adblock/browser/adblock_filter_impl.h"
#include "plainbrowser/adblock/common/adblock.mojom.h"

namespace plainbrowser::adblock {

// Feature name "PlainBrowserAdblock" (derived from the identifier).
BASE_FEATURE(kPlainBrowserAdblock, base::FEATURE_ENABLED_BY_DEFAULT);

void ExposeInterfacesToRenderer(service_manager::BinderRegistry* registry) {
  if (!base::FeatureList::IsEnabled(kPlainBrowserAdblock)) {
    return;
  }
  // The binder runs on the engine sequence, so filter calls never touch the
  // UI or IO thread.
  registry->AddInterface<mojom::AdblockFilter>(
      base::BindRepeating(&BindAdblockFilter),
      AdBlockService::GetInstance()->task_runner());
}

}  // namespace plainbrowser::adblock
