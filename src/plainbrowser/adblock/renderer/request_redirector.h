// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_RENDERER_REQUEST_REDIRECTOR_H_
#define PLAINBROWSER_ADBLOCK_RENDERER_REQUEST_REDIRECTOR_H_

class GURL;

namespace blink {
class WebLocalFrame;
}

namespace url {
class Origin;
}

namespace plainbrowser::adblock {

// $redirect rules: a request a rule would block is answered with one of our
// stand-in resources (rs/src/resources.rs) instead, so a page that waits for,
// say, its analytics script keeps working. Called from
// ChromeContentRendererClient::WillSendRequest (patches/30-adblock/0007), the
// last renderer hook where a subresource's URL can still be replaced; Blink then
// loads the data: URL itself, without the network service.
//
// Only requests to hosts that $redirect rules name are checked (a sync call to
// the browser, GetRedirect); the host list is fetched once per renderer process
// (sync, GetRedirectHosts) and again after a minute, so a list update reaches
// running renderers. Everything else, and a
// redirect the browser declines, goes on to the URL loader throttle as before.
// Main thread. Returns true and sets `new_url` when the request is redirected.
bool MaybeRedirectRequest(blink::WebLocalFrame* frame,
                          const GURL& url,
                          const url::Origin* initiator_origin,
                          GURL* new_url);

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_RENDERER_REQUEST_REDIRECTOR_H_
