// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/browser/websocket_filter.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/bind_post_task.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "plainbrowser/adblock/browser/ad_block_service.h"
#include "plainbrowser/adblock/browser/adblock_filter_impl.h"
#include "plainbrowser/adblock/browser/renderer_interfaces.h"
#include "plainbrowser/shields/browser/shields_service.h"
#include "plainbrowser/shields/common/site_util.h"
#include "services/network/public/mojom/websocket.mojom.h"
#include "url/origin.h"

namespace plainbrowser::adblock {

namespace {

using Factory = content::ContentBrowserClient::WebSocketFactory;

// Origin of the page the frame belongs to (outermost main frame), as a URL.
GURL TopLevelUrl(content::RenderFrameHost* frame) {
  return frame->GetOutermostMainFrame()->GetLastCommittedOrigin().GetURL();
}

// UI thread again, with the engine's answer.
void OnDecision(
    Factory factory,
    const GURL& url,
    std::vector<network::mojom::HttpHeaderPtr> headers,
    mojo::PendingRemote<network::mojom::WebSocketHandshakeClient> client,
    mojo::PendingRemote<network::mojom::WebSocketAuthenticationHandler> auth,
    mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_client,
    bool block) {
  if (block) {
    // Never reaches the network service: the page sees a failed connection.
    mojo::Remote<network::mojom::WebSocketHandshakeClient> remote(
        std::move(client));
    remote->OnFailure(
        "WebSocket connection blocked by the Plain Browser ad blocker",
        net::ERR_BLOCKED_BY_CLIENT, /*response_code=*/-1);
    return;
  }
  std::move(factory).Run(url, std::move(headers), std::move(client),
                         std::move(auth), std::move(header_client));
}

// What the wrapped factory does when the browser or an extension proxy calls it.
void RunFiltered(
    Factory factory,
    int process_id,
    blink::LocalFrameToken frame_token,
    GURL source_url,
    GURL top_level_url,
    const GURL& url,
    std::vector<network::mojom::HttpHeaderPtr> headers,
    mojo::PendingRemote<network::mojom::WebSocketHandshakeClient> client,
    mojo::PendingRemote<network::mojom::WebSocketAuthenticationHandler> auth,
    mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_client) {
  AdBlockService* service = AdBlockService::GetInstance();
  base::OnceCallback<void(bool)> on_decision = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&OnDecision, std::move(factory), url, std::move(headers),
                     std::move(client), std::move(auth),
                     std::move(header_client)));
  // The engine's service lives on its own sequence (see AdBlockService).
  service->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DecideRequest, url, std::move(source_url), "websocket",
                     std::move(top_level_url), process_id,
                     std::make_optional(frame_token), std::move(on_decision)));
}

}  // namespace

bool WillInterceptWebSocket(content::RenderFrameHost* frame) {
  if (!frame || !base::FeatureList::IsEnabled(kPlainBrowserAdblock)) {
    return false;
  }
  if (!AdBlockService::GetInstance()->blocking_enabled()) {
    return false;
  }
  return !shields::ShieldsService::GetInstance()->IsExcepted(
      shields::SiteFor(TopLevelUrl(frame)));
}

Factory FilterWebSocketFactory(content::RenderFrameHost* frame, Factory factory) {
  if (!frame) {
    return factory;
  }
  return base::BindOnce(&RunFiltered, std::move(factory),
                        frame->GetProcess()->GetDeprecatedID(),
                        frame->GetFrameToken(),
                        frame->GetLastCommittedOrigin().GetURL(),
                        TopLevelUrl(frame));
}

void ConnectWebSocket(
    Factory factory,
    const GURL& url,
    const std::optional<std::string>& user_agent,
    mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
        handshake_client,
    mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_client) {
  std::vector<network::mojom::HttpHeaderPtr> headers;
  if (user_agent) {
    headers.push_back(network::mojom::HttpHeader::New(
        net::HttpRequestHeaders::kUserAgent, *user_agent));
  }
  std::move(factory).Run(url, std::move(headers), std::move(handshake_client),
                         mojo::NullRemote(), std::move(header_client));
}

}  // namespace plainbrowser::adblock
