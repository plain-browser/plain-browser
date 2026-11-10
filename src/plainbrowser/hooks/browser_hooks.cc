// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/hooks/browser_hooks.h"

#include <memory>
#include <utility>

#include "content/public/browser/webui_config_map.h"
#include "plainbrowser/adblock/browser/filter_list_updater.h"
#include "plainbrowser/adblock/browser/renderer_interfaces.h"
#include "plainbrowser/adblock/browser/tracking_params_navigation_throttle.h"
#include "plainbrowser/adblock/browser/websocket_filter.h"
#include "plainbrowser/adblock/ui/adblock_ui.h"
#include "plainbrowser/extensions/ui/recommended_extensions_ui.h"
#include "plainbrowser/fingerprint/browser/renderer_switches.h"
#include "plainbrowser/shields/browser/shields_prefs.h"
#include "plainbrowser/shields/browser/shields_service.h"

namespace plainbrowser::hooks {

void ExposeInterfacesToRenderer(service_manager::BinderRegistry* registry,
                                content::RenderProcessHost* render_process_host) {
  adblock::ExposeInterfacesToRenderer(registry, render_process_host);
  // The per-site exception list for the new renderer.
  shields::ShieldsService::GetInstance()->OnRendererProcessCreated(
      render_process_host);
}

void AppendRendererCommandLineSwitches(base::CommandLine* command_line,
                                       int child_process_id) {
  fingerprint::AppendRendererCommandLineSwitches(command_line,
                                                 child_process_id);
}

void AddNavigationThrottles(content::NavigationThrottleRegistry& registry) {
  adblock::TrackingParamsNavigationThrottle::MaybeCreateAndAdd(registry);
}

bool WillInterceptWebSocket(content::RenderFrameHost* frame) {
  return adblock::WillInterceptWebSocket(frame);
}

content::ContentBrowserClient::WebSocketFactory FilterWebSocketFactory(
    content::RenderFrameHost* frame,
    content::ContentBrowserClient::WebSocketFactory factory) {
  return adblock::FilterWebSocketFactory(frame, std::move(factory));
}

void ConnectWebSocket(
    content::ContentBrowserClient::WebSocketFactory factory,
    const GURL& url,
    const std::optional<std::string>& user_agent,
    mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
        handshake_client,
    mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_client) {
  adblock::ConnectWebSocket(std::move(factory), url, user_agent,
                            std::move(handshake_client),
                            std::move(header_client));
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  adblock::FilterListUpdater::RegisterLocalStatePrefs(registry);
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  shields::RegisterProfilePrefs(registry);
}

void RegisterWebUIConfigs(content::WebUIConfigMap& map) {
  map.AddWebUIConfig(std::make_unique<adblock::AdblockUIConfig>());
  map.AddWebUIConfig(
      std::make_unique<extensions::RecommendedExtensionsUIConfig>());
}

void OnPostBrowserStart(PrefService* local_state,
                        scoped_refptr<network::SharedURLLoaderFactory> factory,
                        const base::FilePath& user_data_dir) {
  adblock::FilterListUpdater::Start(local_state, std::move(factory),
                                    user_data_dir);
}

}  // namespace plainbrowser::hooks
