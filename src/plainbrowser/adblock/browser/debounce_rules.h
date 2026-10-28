// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_BROWSER_DEBOUNCE_RULES_H_
#define PLAINBROWSER_ADBLOCK_BROWSER_DEBOUNCE_RULES_H_

#include <optional>
#include <string_view>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "url/gurl.h"

namespace plainbrowser::adblock {

// Kill switch: --disable-features=PlainBrowserDebounce. Also off when
// PlainBrowserAdblock is disabled.
BASE_DECLARE_FEATURE(kPlainBrowserDebounce);

namespace debounce {

// "Debouncing": a main-frame navigation to a known tracking redirector
// (l.facebook.com/l.php?u=<target>, www.google.com/url?q=<target>, ...) goes
// straight to <target>, so the redirector never sees the click.
// TrackingParamsNavigationThrottle applies it; this is the pure URL logic.

enum class Encoding {
  kPlain,      // The parameter is the (percent-encoded) target URL.
  kBase64Url,  // `value_prefix` followed by the target in base64url (Bing).
};

struct Rule {
  // The redirector's host; subdomains match too (eur01.safelinks....).
  std::string_view host;
  // Path the URL's path must equal, or start with when it ends in '/'.
  std::string_view path;
  // Query parameters holding the target, tried in order; '|'-separated.
  std::string_view params;
  Encoding encoding = Encoding::kPlain;
  std::string_view value_prefix;
};

// The built-in rules (debounce_rules.cc).
base::span<const Rule> BuiltInRules();

// The target `url` redirects to when it matches one of `rules` and the target
// is a valid http(s) URL other than `url`; nullopt otherwise. Only GET
// navigations should be debounced (the caller checks).
std::optional<GURL> TargetFor(const GURL& url, base::span<const Rule> rules);

}  // namespace debounce
}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_BROWSER_DEBOUNCE_RULES_H_
