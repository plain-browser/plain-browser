// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/shields/renderer/shields_config_impl.h"

#include <set>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "plainbrowser/shields/common/shields.mojom.h"

namespace plainbrowser::shields {

namespace {

class ShieldsConfigImpl : public mojom::ShieldsConfig {
 public:
  static ShieldsConfigImpl& Get() {
    static base::NoDestructor<ShieldsConfigImpl> impl;
    return *impl;
  }

  void Bind(mojo::PendingReceiver<mojom::ShieldsConfig> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  bool IsExcepted(std::string_view site) const {
    base::AutoLock auto_lock(lock_);
    return sites_.find(std::string(site)) != sites_.end();
  }

  // mojom::ShieldsConfig:
  void SetExceptions(const std::vector<std::string>& sites) override {
    std::set<std::string> next(sites.begin(), sites.end());
    base::AutoLock auto_lock(lock_);
    sites_ = std::move(next);
  }

 private:
  friend class base::NoDestructor<ShieldsConfigImpl>;
  ShieldsConfigImpl() = default;

  mojo::ReceiverSet<mojom::ShieldsConfig> receivers_;
  mutable base::Lock lock_;
  std::set<std::string> sites_ GUARDED_BY(lock_);
};

void BindShieldsConfig(mojo::PendingReceiver<mojom::ShieldsConfig> receiver) {
  ShieldsConfigImpl::Get().Bind(std::move(receiver));
}

}  // namespace

void ExposeRendererInterfacesToBrowser(mojo::BinderMap* binders) {
  binders->Add<mojom::ShieldsConfig>(
      base::BindRepeating(&BindShieldsConfig),
      base::SequencedTaskRunner::GetCurrentDefault());
}

bool IsExceptedSite(std::string_view site) {
  return !site.empty() && ShieldsConfigImpl::Get().IsExcepted(site);
}

}  // namespace plainbrowser::shields
