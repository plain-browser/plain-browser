// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_BROWSER_SUBFRAME_NAVIGATION_THROTTLE_H_
#define PLAINBROWSER_ADBLOCK_BROWSER_SUBFRAME_NAVIGATION_THROTTLE_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationThrottleRegistry;
}

namespace plainbrowser::adblock {

// Applies the network rules to the documents of frames (<iframe>, <frame>).
// A frame's document is a navigation, loaded by the browser process, so the
// renderer-side AdblockURLLoaderThrottle never sees it: without this an ad
// served as an iframe document (`||ads.example^$subdocument`, or any rule
// without a type) loads. Each navigation of a subframe, and each redirect on
// its way, is shown to the engine as a "sub_frame" request exactly like a
// subresource: the navigation is deferred, the engine is asked on its own
// sequence (DecideRequest, with the per-site exceptions, counters and request
// log), and a blocked one is cancelled with ERR_BLOCKED_BY_CLIENT, which makes
// the renderer collapse the frame. Main-frame navigations are not touched
// (`$document` blocking is not implemented). Kill switch kPlainBrowserAdblock.
class SubframeNavigationThrottle : public content::NavigationThrottle {
 public:
  // Called from hooks::AddNavigationThrottles
  // (ChromeContentBrowserClient::CreateThrottlesForNavigation).
  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);

  explicit SubframeNavigationThrottle(
      content::NavigationThrottleRegistry& registry);
  ~SubframeNavigationThrottle() override;

  SubframeNavigationThrottle(const SubframeNavigationThrottle&) = delete;
  SubframeNavigationThrottle& operator=(const SubframeNavigationThrottle&) =
      delete;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  ThrottleCheckResult Check();
  void OnDecision(bool block);

  // Fail open, like the renderer throttle: if the engine never answers, the
  // frame loads after this long.
  static constexpr base::TimeDelta kDecisionTimeout = base::Seconds(3);
  bool decided_ = false;
  base::OneShotTimer failsafe_timer_;

  base::WeakPtrFactory<SubframeNavigationThrottle> weak_factory_{this};
};

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_BROWSER_SUBFRAME_NAVIGATION_THROTTLE_H_
