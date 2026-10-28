// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/browser/engine_launcher.h"

#include <atomic>
#include <utility>

#include "base/functional/bind.h"
#include "content/public/browser/service_process_host.h"
#include "plainbrowser/adblock/browser/ad_block_service.h"
#include "plainbrowser/adblock/common/adblock_engine.mojom.h"

namespace plainbrowser::adblock {

namespace {

void LaunchEngine(mojo::PendingReceiver<mojom::AdblockEngine> receiver) {
  // The sandbox comes from the interface ([ServiceSandbox=kService]); the name
  // is what Task Manager shows.
  content::ServiceProcessHost::Launch(
      std::move(receiver), content::ServiceProcessHost::Options()
                               .WithDisplayName(u"Plain Browser ad blocker")
                               .Pass());
}

}  // namespace

void EnsureEngineLauncher() {
  static std::atomic<bool> set{false};
  if (!set.exchange(true)) {
    AdBlockService::GetInstance()->SetLauncher(
        base::BindRepeating(&LaunchEngine));
  }
}

}  // namespace plainbrowser::adblock
