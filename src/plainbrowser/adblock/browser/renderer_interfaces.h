// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_BROWSER_RENDERER_INTERFACES_H_
#define PLAINBROWSER_ADBLOCK_BROWSER_RENDERER_INTERFACES_H_

#include "base/feature_list.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace content {
class RenderProcessHost;
}

namespace plainbrowser::adblock {

// Kill switch: --disable-features=PlainBrowserAdblock.
BASE_DECLARE_FEATURE(kPlainBrowserAdblock);

// Called from ChromeContentBrowserClient::ExposeInterfacesToRenderer
// (patches/30-adblock) with the registry of `render_process_host`. Registers
// mojom::AdblockFilter for that renderer process; the process id lets the
// filter count blocked requests per tab.
void ExposeInterfacesToRenderer(service_manager::BinderRegistry* registry,
                                content::RenderProcessHost* render_process_host);

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_BROWSER_RENDERER_INTERFACES_H_
