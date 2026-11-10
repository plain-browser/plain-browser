// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/hooks/renderer_hooks.h"

#include <utility>

#include "plainbrowser/adblock/renderer/adblock_throttle_provider.h"
#include "plainbrowser/adblock/renderer/adblock_websocket_throttle_provider.h"
#include "plainbrowser/adblock/renderer/cosmetic_filter_agent.h"
#include "plainbrowser/adblock/renderer/request_redirector.h"
#include "plainbrowser/fingerprint/renderer/fingerprint_agent.h"
#include "plainbrowser/shields/renderer/shields_config_impl.h"

namespace plainbrowser::hooks {

void ExposeInterfacesToBrowser(mojo::BinderMap* binders) {
  shields::ExposeRendererInterfacesToBrowser(binders);
}

void OnRenderFrameCreated(content::RenderFrame* render_frame) {
  // Order as before the hooks existed: element hiding, then fingerprint noise.
  adblock::CosmeticFilterAgent::Create(render_frame);
  fingerprint::FingerprintAgent::Create(render_frame);
}

void OnWorkerContextCreated(v8::Local<v8::Context> context) {
  fingerprint::FingerprintAgent::OnWorkerContextCreated(context);
}

bool MaybeReplaceRequestUrl(blink::WebLocalFrame* frame,
                            const GURL& url,
                            const url::Origin* initiator_origin,
                            GURL* new_url) {
  return adblock::MaybeRedirectRequest(frame, url, initiator_origin, new_url);
}

std::unique_ptr<blink::URLLoaderThrottleProvider> WrapURLLoaderThrottleProvider(
    std::unique_ptr<blink::URLLoaderThrottleProvider> chrome_provider,
    blink::ThreadSafeBrowserInterfaceBrokerProxy* broker) {
  return adblock::AdblockThrottleProvider::Wrap(std::move(chrome_provider),
                                                broker);
}

std::unique_ptr<blink::WebSocketHandshakeThrottleProvider>
WrapWebSocketHandshakeThrottleProvider(
    std::unique_ptr<blink::WebSocketHandshakeThrottleProvider> chrome_provider,
    blink::ThreadSafeBrowserInterfaceBrokerProxy* broker) {
  return adblock::AdblockWebSocketThrottleProvider::Wrap(
      std::move(chrome_provider), broker);
}

}  // namespace plainbrowser::hooks
