// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/browser/subframe_navigation_throttle.h"

#include <memory>
#include <optional>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/bind_post_task.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "net/base/net_errors.h"
#include "plainbrowser/adblock/browser/ad_block_service.h"
#include "plainbrowser/adblock/browser/adblock_filter_impl.h"
#include "plainbrowser/adblock/browser/engine_launcher.h"
#include "plainbrowser/adblock/browser/renderer_interfaces.h"
#include "plainbrowser/shields/browser/shields_service.h"
#include "plainbrowser/shields/common/site_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace plainbrowser::adblock {

namespace {

// The document that asked for the frame: the initiator when it is a real
// origin, else (browser-initiated, e.g. a restored session, or a sandboxed
// opaque initiator) the parent frame, so `$third-party` still compares the
// frame with the page that embeds it.
GURL SourceUrlFor(const std::optional<url::Origin>& initiator,
                  content::RenderFrameHost* parent) {
  if (initiator && !initiator->opaque()) {
    return initiator->GetURL();
  }
  return parent->GetLastCommittedOrigin().GetURL();
}

}  // namespace

// static
void SubframeNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  if (!base::FeatureList::IsEnabled(kPlainBrowserAdblock)) {
    return;
  }
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  if (handle.IsInMainFrame() || !handle.GetParentFrame()) {
    return;
  }
  EnsureEngineLauncher();
  registry.AddThrottle(std::make_unique<SubframeNavigationThrottle>(registry));
}

SubframeNavigationThrottle::SubframeNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

SubframeNavigationThrottle::~SubframeNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
SubframeNavigationThrottle::WillStartRequest() {
  return Check();
}

content::NavigationThrottle::ThrottleCheckResult
SubframeNavigationThrottle::WillRedirectRequest() {
  return Check();
}

const char* SubframeNavigationThrottle::GetNameForLogging() {
  return "PlainBrowserSubframeNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
SubframeNavigationThrottle::Check() {
  content::NavigationHandle* handle = navigation_handle();
  const GURL& url = handle->GetURL();
  content::RenderFrameHost* parent = handle->GetParentFrame();
  AdBlockService* service = AdBlockService::GetInstance();
  if (!parent || !url.SchemeIsHTTPOrHTTPS() || !service->blocking_enabled()) {
    return PROCEED;
  }
  // The page's top-level origin, like the renderer throttle's and the
  // WebSocket filter's: the per-site exception list is keyed by it.
  const GURL top_level_url =
      parent->GetOutermostMainFrame()->GetLastCommittedOrigin().GetURL();
  if (shields::ShieldsService::GetInstance()->IsExcepted(
          shields::SiteFor(top_level_url))) {
    return PROCEED;
  }
  const GURL source_url = SourceUrlFor(handle->GetInitiatorOrigin(), parent);
  decided_ = false;
  failsafe_timer_.Start(
      FROM_HERE, kDecisionTimeout,
      base::BindOnce(&SubframeNavigationThrottle::OnDecision,
                     base::Unretained(this), false));
  // The requesting frame for the per-tab counter is the embedding one. The
  // service lives on its own sequence and answers false when the engine is not
  // there; the answer comes back here.
  service->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DecideRequest, url, source_url, "sub_frame", top_level_url,
          parent->GetProcess()->GetDeprecatedID(),
          std::make_optional(parent->GetFrameToken()),
          base::BindPostTaskToCurrentDefault(base::BindOnce(
              &SubframeNavigationThrottle::OnDecision,
              weak_factory_.GetWeakPtr()))));
  return DEFER;
}

void SubframeNavigationThrottle::OnDecision(bool block) {
  if (decided_) {
    return;  // The answer and the fail-open timer can both fire; act once.
  }
  decided_ = true;
  failsafe_timer_.Stop();
  if (block) {
    CancelDeferredNavigation(
        ThrottleCheckResult(CANCEL, net::ERR_BLOCKED_BY_CLIENT));
    return;
  }
  Resume();
}

}  // namespace plainbrowser::adblock
