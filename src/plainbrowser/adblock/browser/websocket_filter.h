// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_BROWSER_WEBSOCKET_FILTER_H_
#define PLAINBROWSER_ADBLOCK_BROWSER_WEBSOCKET_FILTER_H_

#include <optional>
#include <string>

#include "content/public/browser/content_browser_client.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/websocket.mojom-forward.h"
#include "url/gurl.h"

namespace content {
class RenderFrameHost;
}

// WebSocket connections are decided in the browser process, before the network
// service opens a socket: a blocked tracker never receives the handshake (a
// renderer-side throttle cannot do that, Blink starts the handshake in parallel
// with it). Called from ChromeContentBrowserClient::WillInterceptWebSocket and
// CreateWebSocket through //plainbrowser/hooks (patches/20-hooks/0002); UI thread.
namespace plainbrowser::adblock {

// True when the filter wants to see a WebSocket created by `frame`: blocking is
// on (global switch, kill switch --disable-features=PlainBrowserAdblock) and the
// site of the page is not on the exception list. False means "leave it alone",
// and the connection is made exactly as before. Shared and service workers have
// no frame and are not filtered.
bool WillInterceptWebSocket(content::RenderFrameHost* frame);

// A factory that asks the engine about the URL it is called with (type
// "websocket", the `$websocket` rules) and either runs `factory` or ends the
// handshake with ERR_BLOCKED_BY_CLIENT without connecting. Fails open: no
// engine, or no answer in the engine's own time-outs, means connect. A null
// `frame` returns `factory` unchanged.
content::ContentBrowserClient::WebSocketFactory FilterWebSocketFactory(
    content::RenderFrameHost* frame,
    content::ContentBrowserClient::WebSocketFactory factory);

// Connects the way content::WebSocketConnectorImpl does when nothing intercepts
// (a User-Agent header if the renderer gave one; DevTools' extra headers are
// not added). For when the filter intercepted and no extension proxies it.
void ConnectWebSocket(
    content::ContentBrowserClient::WebSocketFactory factory,
    const GURL& url,
    const std::optional<std::string>& user_agent,
    mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
        handshake_client,
    mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_client);

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_BROWSER_WEBSOCKET_FILTER_H_
