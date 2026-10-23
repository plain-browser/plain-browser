// Copyright Plain Browser contributors. MPL-2.0.
#ifndef DRAFTWAY_ADBLOCK_BROWSER_RENDERER_INTERFACES_H_
#define DRAFTWAY_ADBLOCK_BROWSER_RENDERER_INTERFACES_H_

#include "base/feature_list.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace draftway::adblock {

// Kill switch: --disable-features=DraftwayAdblock.
BASE_DECLARE_FEATURE(kDraftwayAdblock);

// Called from ChromeContentBrowserClient::ExposeInterfacesToRenderer
// (patches/30-adblock). Registers mojom::AdblockFilter for renderer processes.
void ExposeInterfacesToRenderer(service_manager::BinderRegistry* registry);

}  // namespace draftway::adblock

#endif  // DRAFTWAY_ADBLOCK_BROWSER_RENDERER_INTERFACES_H_
