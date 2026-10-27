// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/shields/browser/shields_service.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "plainbrowser/shields/browser/shields_prefs.h"
#include "plainbrowser/shields/common/pref_names.h"

namespace plainbrowser::shields {

namespace {
const void* const kWatcherKey = &kWatcherKey;
}  // namespace

// Attached to a BrowserContext as user data, so it lives exactly as long as
// the profile and its PrefService: no KeyedService factory and no extra hook in
// Chromium's profile setup.
class ShieldsService::ProfileWatcher : public base::SupportsUserData::Data {
 public:
  ProfileWatcher(ShieldsService* service, PrefService* prefs)
      : service_(service), prefs_(prefs) {
    registrar_.Init(prefs);
    registrar_.Add(prefs::kShieldsExceptions,
                   base::BindRepeating(&ShieldsService::Rebuild,
                                       base::Unretained(service_)));
  }
  ~ProfileWatcher() override { service_->OnWatcherDestroyed(this); }

  const PrefService* prefs() const { return prefs_; }

 private:
  const raw_ptr<ShieldsService> service_;
  const raw_ptr<PrefService> prefs_;
  PrefChangeRegistrar registrar_;
};

// static
ShieldsService* ShieldsService::GetInstance() {
  static base::NoDestructor<ShieldsService> instance;
  return instance.get();
}

ShieldsService::ShieldsService() = default;
ShieldsService::~ShieldsService() = default;

bool ShieldsService::IsExcepted(std::string_view site) const {
  if (site.empty()) {
    return false;
  }
  base::AutoLock auto_lock(lock_);
  return excepted_.find(std::string(site)) != excepted_.end();
}

void ShieldsService::OnRendererProcessCreated(content::RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!host) {
    return;
  }
  EnsureWatching(host->GetBrowserContext());
  mojo::Remote<mojom::ShieldsConfig> remote;
  host->BindReceiver(remote.BindNewPipeAndPassReceiver());
  std::vector<std::string> sites;
  {
    base::AutoLock auto_lock(lock_);
    sites.assign(excepted_.begin(), excepted_.end());
  }
  remote->SetExceptions(sites);
  renderers_.Add(std::move(remote));  // Dropped when the process goes away.
}

void ShieldsService::EnsureWatching(content::BrowserContext* context) {
  if (!context || context->GetUserData(kWatcherKey)) {
    return;
  }
  PrefService* prefs = user_prefs::UserPrefs::Get(context);
  if (!prefs) {
    return;
  }
  auto watcher = std::make_unique<ProfileWatcher>(this, prefs);
  watchers_.insert(watcher.get());
  context->SetUserData(kWatcherKey, std::move(watcher));
  Rebuild();
}

void ShieldsService::OnWatcherDestroyed(ProfileWatcher* watcher) {
  watchers_.erase(watcher);
  Rebuild();
}

void ShieldsService::Rebuild() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::set<std::string> merged;
  for (const ProfileWatcher* watcher : watchers_) {
    for (std::string& site : GetExceptions(watcher->prefs())) {
      merged.insert(std::move(site));
    }
  }
  const std::vector<std::string> sites(merged.begin(), merged.end());
  {
    base::AutoLock auto_lock(lock_);
    excepted_ = std::move(merged);
  }
  for (const auto& remote : renderers_) {
    remote->SetExceptions(sites);
  }
}

}  // namespace plainbrowser::shields
