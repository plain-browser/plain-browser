// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/shields/browser/shields_service.h"

#include <atomic>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/supports_user_data.h"
#include "base/task/sequenced_task_runner.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "plainbrowser/shields/browser/shields_prefs.h"
#include "plainbrowser/shields/common/pref_names.h"

namespace plainbrowser::shields {

namespace {
const void* const kWatcherKey = &kWatcherKey;

// Profile ids start at 1: 0 means "no profile".
uint64_t NextProfileId() {
  static std::atomic<uint64_t> next{1};
  return next++;
}
}  // namespace

// Attached to a BrowserContext as user data, so it lives exactly as long as
// the profile and its PrefService: no KeyedService factory and no extra hook in
// Chromium's profile setup.
class ShieldsService::ProfileWatcher : public base::SupportsUserData::Data {
 public:
  ProfileWatcher(ShieldsService* service, PrefService* prefs)
      : service_(service), prefs_(prefs), id_(NextProfileId()) {
    registrar_.Init(prefs);
    registrar_.Add(prefs::kShieldsExceptions,
                   base::BindRepeating(&ProfileWatcher::OnChanged,
                                       base::Unretained(this)));
    PostNormalize();
  }
  ~ProfileWatcher() override { service_->OnWatcherDestroyed(this); }

  const PrefService* prefs() const { return prefs_; }
  uint64_t id() const { return id_; }

 private:
  void OnChanged() {
    service_->Rebuild();
    PostNormalize();
  }

  // The settings page stores entries as typed ("www.example.com"); the list
  // is read through SiteForEntry() anyway, but rewriting it shows the user the
  // sites it applies to. Posted so that the pref is not written from inside
  // its own change notification; the rewrite notifies again and then finds
  // nothing to do.
  void PostNormalize() {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ProfileWatcher::Normalize,
                                  weak_factory_.GetWeakPtr()));
  }
  void Normalize() { NormalizeExceptions(prefs_); }

  const raw_ptr<ShieldsService> service_;
  const raw_ptr<PrefService> prefs_;
  const uint64_t id_;
  PrefChangeRegistrar registrar_;
  base::WeakPtrFactory<ProfileWatcher> weak_factory_{this};
};

// Tells the service when a renderer process goes away, to forget its profile.
class ShieldsService::ProcessObserver
    : public content::RenderProcessHostObserver {
 public:
  ProcessObserver(ShieldsService* service,
                  content::RenderProcessHost* host,
                  int process_id)
      : service_(service), process_id_(process_id) {
    observation_.Observe(host);
  }

  // content::RenderProcessHostObserver:
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override {
    observation_.Reset();
    // Destroys this object: nothing may follow.
    service_->OnProcessGone(process_id_);
  }

 private:
  const raw_ptr<ShieldsService> service_;
  const int process_id_;
  base::ScopedObservation<content::RenderProcessHost,
                          content::RenderProcessHostObserver>
      observation_{this};
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

uint64_t ShieldsService::ProfileIdForProcess(int process_id) const {
  base::AutoLock auto_lock(lock_);
  auto it = process_profiles_.find(process_id);
  return it == process_profiles_.end() ? 0 : it->second;
}

uint64_t ShieldsService::ProfileIdFor(content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!context || context->IsOffTheRecord()) {
    return 0;
  }
  EnsureWatching(context);
  auto* watcher =
      static_cast<ProfileWatcher*>(context->GetUserData(kWatcherKey));
  return watcher ? watcher->id() : 0;
}

void ShieldsService::OnProcessGone(int process_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  {
    base::AutoLock auto_lock(lock_);
    process_profiles_.erase(process_id);
  }
  process_observers_.erase(process_id);
}

void ShieldsService::OnRendererProcessCreated(content::RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!host) {
    return;
  }
  EnsureWatching(host->GetBrowserContext());
  const int process_id = host->GetDeprecatedID();
  const uint64_t profile = ProfileIdFor(host->GetBrowserContext());
  {
    base::AutoLock auto_lock(lock_);
    process_profiles_[process_id] = profile;
  }
  if (!process_observers_.contains(process_id)) {
    process_observers_[process_id] =
        std::make_unique<ProcessObserver>(this, host, process_id);
  }
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
