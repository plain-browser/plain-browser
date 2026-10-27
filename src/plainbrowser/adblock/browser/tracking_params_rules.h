// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_BROWSER_TRACKING_PARAMS_RULES_H_
#define PLAINBROWSER_ADBLOCK_BROWSER_TRACKING_PARAMS_RULES_H_

#include <optional>

#include "base/feature_list.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace plainbrowser::adblock {

// Kill switch: --disable-features=PlainBrowserTrackingParams. Also off when
// PlainBrowserAdblock is disabled (same engine, same lists).
BASE_DECLARE_FEATURE(kPlainBrowserTrackingParams);

namespace tracking_params {

// Decision logic of TrackingParamsNavigationThrottle, kept free of
// content::NavigationHandle so it can be unit-tested.

// Navigations the throttle handles at all: http(s), not POST (the body could
// not be replayed by the re-issued navigation).
bool IsEligibleNavigation(const GURL& url, bool is_post);

// Whether a request or redirect URL has to be shown to the engine: eligible
// and carrying a query (there is nothing to strip otherwise).
bool NeedsCheck(const GURL& url, bool is_post);

// The document URL handed to the engine as the request's source: the initiator
// origin when there is a non-opaque one, otherwise the navigation URL itself
// (browser-initiated navigations count as first-party).
GURL SourceUrlFor(const std::optional<url::Origin>& initiator,
                  const GURL& url);

// The URL whose site (shields::SiteFor) the per-site exception list is
// consulted for, keyed like the other protections by the top-level page: the
// destination itself for a main-frame navigation (at a redirect, the redirect
// target), the hosting page for a subframe.
const GURL& ExceptionUrlFor(bool is_in_main_frame,
                            const GURL& url,
                            const GURL& top_level_url);

// Whether the engine's answer for `original` means cancel-and-reissue: a valid
// URL that differs from `original`, and the navigation is still at `original`
// (it may have been redirected again while the engine was asked).
bool ShouldReissue(const GURL& original,
                   const GURL& rewritten,
                   const GURL& current);

}  // namespace tracking_params
}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_BROWSER_TRACKING_PARAMS_RULES_H_
