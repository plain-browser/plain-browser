// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_HOOKS_BROWSER_HOOKS_H_
#define PLAINBROWSER_HOOKS_BROWSER_HOOKS_H_

#include "base/memory/scoped_refptr.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace content {
class NavigationThrottleRegistry;
class RenderProcessHost;
class WebUIConfigMap;
}  // namespace content

namespace network {
class SharedURLLoaderFactory;
}

namespace service_manager {
class BinderRegistry;
}

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
