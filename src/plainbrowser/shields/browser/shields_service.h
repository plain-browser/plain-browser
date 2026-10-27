// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_SHIELDS_BROWSER_SHIELDS_SERVICE_H_
#define PLAINBROWSER_SHIELDS_BROWSER_SHIELDS_SERVICE_H_

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "plainbrowser/shields/common/shields.mojom.h"

namespace content {
class BrowserContext;
class RenderProcessHost;
}  // namespace content

namespace plainbrowser::shields {

// Process-wide view of the per-site exception lists (prefs::kShieldsExceptions)
// of every loaded profile, for code that runs off the UI thread or in a
// renderer: the adblock filter asks IsExcepted() on the engine sequence, and
// renderer processes get the list pushed over mojom::ShieldsConfig (the
// fingerprint agent needs it synchronously). The lists of all profiles are
// merged: a site excepted in one profile is excepted everywhere. Plain Browser
// is used with one profile; keeping the lists apart would need per-profile
// filter plumbing for no practical gain today.
class ShieldsService {
 public:
  static ShieldsService* GetInstance();

  ShieldsService(const ShieldsService&) = delete;
  ShieldsService& operator=(const ShieldsService&) = delete;

  // Any thread. `site` is an eTLD+1 as returned by SiteFor().
  bool IsExcepted(std::string_view site) const;

  // UI thread. Called when a renderer process is created
  // (ChromeContentBrowserClient::ExposeInterfacesToRenderer, patches/30-adblock):
  // starts watching the process's profile prefs if not yet watched, binds a
  // ShieldsConfig remote in the renderer and sends it the current list.
  void OnRendererProcessCreated(content::RenderProcessHost* host);

 private:
  friend class base::NoDestructor<ShieldsService>;
  class ProfileWatcher;

  ShieldsService();
  ~ShieldsService();

  void EnsureWatching(content::BrowserContext* context);
  void OnWatcherDestroyed(ProfileWatcher* watcher);
  // UI thread. Recomputes the merged list and pushes it to every renderer.
  void Rebuild();

  // UI thread only.
  std::set<ProfileWatcher*> watchers_;
  mojo::RemoteSet<mojom::ShieldsConfig> renderers_;

  mutable base::Lock lock_;
  std::set<std::string> excepted_ GUARDED_BY(lock_);
};

}  // namespace plainbrowser::shields

#endif  // PLAINBROWSER_SHIELDS_BROWSER_SHIELDS_SERVICE_H_
