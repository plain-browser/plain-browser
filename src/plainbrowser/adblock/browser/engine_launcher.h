// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_BROWSER_ENGINE_LAUNCHER_H_
#define PLAINBROWSER_ADBLOCK_BROWSER_ENGINE_LAUNCHER_H_

namespace plainbrowser::adblock {

// Any thread, idempotent. Gives AdBlockService the launcher for the engine's
// utility process (content::ServiceProcessHost; the service is registered in
// chrome/utility/services.cc). Called wherever the browser first needs the
// engine: binding a renderer's filter, adding the navigation throttle.
void EnsureEngineLauncher();

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_BROWSER_ENGINE_LAUNCHER_H_
