// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_SHIELDS_RENDERER_SHIELDS_CONFIG_IMPL_H_
#define PLAINBROWSER_SHIELDS_RENDERER_SHIELDS_CONFIG_IMPL_H_

#include <string_view>

namespace mojo {
class BinderMap;
}

namespace plainbrowser::shields {

// Called from ChromeContentRendererClient::ExposeInterfacesToBrowser
// (patches/60-ui). Registers the renderer's mojom::ShieldsConfig receiver.
void ExposeRendererInterfacesToBrowser(mojo::BinderMap* binders);

// True when `site` (eTLD+1, lower case) is on the exception list the browser
// pushed to this renderer. Renderer main thread or any thread.
bool IsExceptedSite(std::string_view site);

}  // namespace plainbrowser::shields

#endif  // PLAINBROWSER_SHIELDS_RENDERER_SHIELDS_CONFIG_IMPL_H_
