// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_HOOKS_RENDERER_HOOKS_H_
#define PLAINBROWSER_HOOKS_RENDERER_HOOKS_H_

#include <memory>

#include "v8/include/v8-forward.h"

class GURL;

namespace blink {
class ThreadSafeBrowserInterfaceBrokerProxy;
class URLLoaderThrottleProvider;
class WebLocalFrame;
class WebSocketHandshakeThrottleProvider;
}  // namespace blink

namespace content {
class RenderFrame;
}

namespace mojo {
class BinderMap;
}

namespace url {
class Origin;
}

// The only entry points of Plain Browser into the renderer-side Chromium code
// (patches/20-hooks/0001). ChromeContentRendererClient calls these
// functions; a new renderer feature is added to the .cc file next to this one,
// not to a patch. Keep the signatures stable: changing one means editing the
// patch.
namespace plainbrowser::hooks {

// ChromeContentRendererClient::ExposeInterfacesToBrowser: our mojo receivers
// (the shields exception list pushed by the browser).
void ExposeInterfacesToBrowser(mojo::BinderMap* binders);

// ChromeContentRendererClient::RenderFrameCreated: the per-frame agents
// (cosmetic filtering, fingerprint protection).
void OnRenderFrameCreated(content::RenderFrame* render_frame);

// ChromeContentRendererClient::WillSendRequest, the last hook that may replace
// a subresource URL. Returns true and sets `new_url` to replace the request
// ($redirect stand-ins). Main thread.
bool MaybeReplaceRequestUrl(blink::WebLocalFrame* frame,
                            const GURL& url,
                            const url::Origin* initiator_origin,
                            GURL* new_url);

// ChromeContentRendererClient::CreateURLLoaderThrottleProvider: wraps chrome's
// provider so that our throttles run first for every subresource request.
std::unique_ptr<blink::URLLoaderThrottleProvider> WrapURLLoaderThrottleProvider(
    std::unique_ptr<blink::URLLoaderThrottleProvider> chrome_provider,
    blink::ThreadSafeBrowserInterfaceBrokerProxy* broker);

// ChromeContentRendererClient::DidInitializeWorkerContextOnWorkerThread: a
// dedicated, shared or service worker's global scope exists, its script has not
// run. Worker thread. Fingerprint protection for workers.
void OnWorkerContextCreated(v8::Local<v8::Context> context);

// ChromeContentRendererClient::CreateWebSocketHandshakeThrottleProvider: wraps
// chrome's provider so that WebSocket handshakes are checked against the
// `$websocket` rules first.
std::unique_ptr<blink::WebSocketHandshakeThrottleProvider>
WrapWebSocketHandshakeThrottleProvider(
    std::unique_ptr<blink::WebSocketHandshakeThrottleProvider> chrome_provider,
    blink::ThreadSafeBrowserInterfaceBrokerProxy* broker);

}  // namespace plainbrowser::hooks

#endif  // PLAINBROWSER_HOOKS_RENDERER_HOOKS_H_
