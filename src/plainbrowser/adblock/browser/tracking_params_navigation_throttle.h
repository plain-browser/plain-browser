// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_BROWSER_TRACKING_PARAMS_NAVIGATION_THROTTLE_H_
#define PLAINBROWSER_ADBLOCK_BROWSER_TRACKING_PARAMS_NAVIGATION_THROTTLE_H_

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"
#include "url/gurl.h"

namespace content {
class NavigationThrottleRegistry;
}

namespace plainbrowser::adblock {

// Kill switch: --disable-features=PlainBrowserTrackingParams. Also off when
// PlainBrowserAdblock is disabled (same engine, same lists).
BASE_DECLARE_FEATURE(kPlainBrowserTrackingParams);

// Removes tracking parameters (utm_*, fbclid, gclid, ...: the $removeparam
// rules in the engine's lists) from frame navigations. The renderer-side
// subresource throttle cannot rewrite a URL once it has deferred (the request
// may only be edited synchronously), so navigations are handled here: the
// navigation is deferred, the engine is asked on its own sequence, and when
// the URL changes the navigation is cancelled and re-issued with the clean URL,
// so the address bar and history only ever see the clean one.
// Not handled: POST navigations (the body could not be replayed), non-HTTP(S)
// URLs, URLs without a query.
class TrackingParamsNavigationThrottle : public content::NavigationThrottle {
 public:
  // Called from ChromeContentBrowserClient::CreateThrottlesForNavigation
  // (patches/30-adblock). Adds the throttle when the features are enabled.
  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);

  explicit TrackingParamsNavigationThrottle(
      content::NavigationThrottleRegistry& registry);
  ~TrackingParamsNavigationThrottle() override;

  TrackingParamsNavigationThrottle(const TrackingParamsNavigationThrottle&) =
      delete;
  TrackingParamsNavigationThrottle& operator=(
      const TrackingParamsNavigationThrottle&) = delete;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  ThrottleCheckResult Check();
  void OnRewritten(const GURL& original, const GURL& rewritten);

  base::WeakPtrFactory<TrackingParamsNavigationThrottle> weak_factory_{this};
};

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_BROWSER_TRACKING_PARAMS_NAVIGATION_THROTTLE_H_
