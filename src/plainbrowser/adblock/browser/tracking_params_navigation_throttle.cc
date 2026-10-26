// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/browser/tracking_params_navigation_throttle.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "plainbrowser/adblock/browser/ad_block_service.h"
#include "plainbrowser/adblock/browser/renderer_interfaces.h"
#include "url/origin.h"

namespace plainbrowser::adblock {

// Feature name "PlainBrowserTrackingParams" (derived from the identifier).
BASE_FEATURE(kPlainBrowserTrackingParams, base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

void OpenCleanUrl(base::WeakPtr<content::WebContents> web_contents,
                  content::OpenURLParams params) {
  if (web_contents) {
    web_contents->OpenURL(params, /*navigation_handle_callback=*/{});
  }
}

}  // namespace

// static
void TrackingParamsNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  if (!base::FeatureList::IsEnabled(kPlainBrowserAdblock) ||
      !base::FeatureList::IsEnabled(kPlainBrowserTrackingParams)) {
    return;
  }
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  if (!handle.GetURL().SchemeIsHTTPOrHTTPS() || handle.IsPost() ||
      !handle.GetWebContents()) {
    return;
  }
  registry.AddThrottle(
      std::make_unique<TrackingParamsNavigationThrottle>(registry));
}

TrackingParamsNavigationThrottle::TrackingParamsNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

TrackingParamsNavigationThrottle::~TrackingParamsNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
TrackingParamsNavigationThrottle::WillStartRequest() {
  return Check();
}

content::NavigationThrottle::ThrottleCheckResult
TrackingParamsNavigationThrottle::WillRedirectRequest() {
  return Check();
}

const char* TrackingParamsNavigationThrottle::GetNameForLogging() {
  return "PlainBrowserTrackingParamsNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
TrackingParamsNavigationThrottle::Check() {
  content::NavigationHandle* handle = navigation_handle();
  const GURL& url = handle->GetURL();
  if (!url.SchemeIsHTTPOrHTTPS() || !url.has_query() || handle->IsPost()) {
    return PROCEED;
  }
  const std::optional<url::Origin>& initiator = handle->GetInitiatorOrigin();
  const GURL source_url =
      initiator && !initiator->opaque() ? initiator->GetURL() : url;
  AdBlockService* service = AdBlockService::GetInstance();
  // Unretained: the service is never destroyed.
  service->task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AdBlockService::RewrittenUrl, base::Unretained(service),
                     url, source_url,
                     handle->IsInMainFrame() ? std::string("main_frame")
                                             : std::string("sub_frame")),
      base::BindOnce(&TrackingParamsNavigationThrottle::OnRewritten,
                     weak_factory_.GetWeakPtr(), url));
  return DEFER;
}

void TrackingParamsNavigationThrottle::OnRewritten(const GURL& original,
                                                   const GURL& rewritten) {
  content::NavigationHandle* handle = navigation_handle();
  // The navigation may have been redirected again while we were waiting; only
  // act on an answer for the URL we asked about.
  if (!rewritten.is_valid() || rewritten == original ||
      handle->GetURL() != original) {
    Resume();
    return;
  }
  content::WebContents* web_contents = handle->GetWebContents();
  if (!web_contents) {
    Resume();
    return;
  }
  content::OpenURLParams params =
      content::OpenURLParams::FromNavigationHandle(handle);
  params.url = rewritten;
  // Cancel first (this deletes the NavigationRequest and this throttle), then
  // start the clean navigation from a fresh task. The clean URL has nothing
  // left to strip, so the new navigation proceeds through this throttle.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&OpenCleanUrl, web_contents->GetWeakPtr(),
                                std::move(params)));
  CancelDeferredNavigation(CANCEL);
}

}  // namespace plainbrowser::adblock
