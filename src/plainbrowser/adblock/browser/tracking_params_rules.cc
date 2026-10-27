// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/browser/tracking_params_rules.h"

namespace plainbrowser::adblock {

// Feature name "PlainBrowserTrackingParams" (derived from the identifier).
BASE_FEATURE(kPlainBrowserTrackingParams, base::FEATURE_ENABLED_BY_DEFAULT);

namespace tracking_params {

bool IsEligibleNavigation(const GURL& url, bool is_post) {
  return url.SchemeIsHTTPOrHTTPS() && !is_post;
}

bool NeedsCheck(const GURL& url, bool is_post) {
  return IsEligibleNavigation(url, is_post) && url.has_query();
}

GURL SourceUrlFor(const std::optional<url::Origin>& initiator,
                  const GURL& url) {
  return initiator && !initiator->opaque() ? initiator->GetURL() : url;
}

const GURL& ExceptionUrlFor(bool is_in_main_frame,
                            const GURL& url,
                            const GURL& top_level_url) {
  return is_in_main_frame ? url : top_level_url;
}

bool ShouldReissue(const GURL& original,
                   const GURL& rewritten,
                   const GURL& current) {
  return rewritten.is_valid() && rewritten != original && current == original;
}

}  // namespace tracking_params
}  // namespace plainbrowser::adblock
