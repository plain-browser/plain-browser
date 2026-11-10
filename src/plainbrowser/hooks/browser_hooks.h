// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_HOOKS_BROWSER_HOOKS_H_
#define PLAINBROWSER_HOOKS_BROWSER_HOOKS_H_

#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "content/public/browser/content_browser_client.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/binder_registry.h"

class GURL;
class PrefRegistrySimple;
class PrefService;

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace content {
class NavigationThrottleRegistry;
class RenderFrameHost;
class RenderProcessHost;
class WebUIConfigMap;
}  // namespace content

namespace network::mojom {
class TrustedHeaderClient;
class WebSocketHandshakeClient;
}  // namespace network::mojom

namespace user_prefs {
class PrefRegistrySyncable;
}

// The only entry points of Plain Browser into the browser-side Chromium code
// (patches/20-hooks/0002 and 0003). A new browser feature is added to the .cc
// file next to this one, not to a patch. Keep the signatures stable: changing
// one means editing the patches.
namespace plainbrowser::hooks {

// chrome_content_browser_client_receiver_bindings.cc, a new renderer process:
// the adblock filter and the shields exception list.
void ExposeInterfacesToRenderer(service_manager::BinderRegistry* registry,
                                content::RenderProcessHost* render_process_host);

// ChromeContentBrowserClient::AppendExtraCommandLineSwitches, renderer
// processes only: fingerprint session key and the initial exception list.
// `child_process_id` is the RenderProcessHost id.
void AppendRendererCommandLineSwitches(base::CommandLine* command_line,
                                       int child_process_id);

// ChromeContentBrowserClient::CreateThrottlesForNavigation: tracking-parameter
// stripping and debouncing.
void AddNavigationThrottles(content::NavigationThrottleRegistry& registry);

// ChromeContentBrowserClient::WillInterceptWebSocket, CreateWebSocket: WebSocket
// connections are decided in the browser before a socket is opened, with the
// `$websocket` rules. WillInterceptWebSocket: true when the filter wants this
// frame's connections. FilterWebSocketFactory: `factory` wrapped so that it asks
// the engine first (chrome's own code then hands it to the extensions'
// webRequest proxy, if any). ConnectWebSocket: the direct connection, for when
// no extension proxies it.
bool WillInterceptWebSocket(content::RenderFrameHost* frame);
content::ContentBrowserClient::WebSocketFactory FilterWebSocketFactory(
    content::RenderFrameHost* frame,
    content::ContentBrowserClient::WebSocketFactory factory);
void ConnectWebSocket(
    content::ContentBrowserClient::WebSocketFactory factory,
    const GURL& url,
    const std::optional<std::string>& user_agent,
    mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
        handshake_client,
    mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_client);

// chrome::RegisterLocalState / chrome::RegisterProfilePrefs (browser_prefs.cc).
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// ChromeWebUIConfigs: chrome://adblock, chrome://recommended-extensions.
void RegisterWebUIConfigs(content::WebUIConfigMap& map);

// ChromeBrowserMainParts::PostBrowserStart, UI thread, once: the filter list
// updater (loads the stored lists, schedules the daily check).
void OnPostBrowserStart(PrefService* local_state,
                        scoped_refptr<network::SharedURLLoaderFactory> factory,
                        const base::FilePath& user_data_dir);

}  // namespace plainbrowser::hooks

#endif  // PLAINBROWSER_HOOKS_BROWSER_HOOKS_H_
