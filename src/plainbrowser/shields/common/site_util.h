// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_SHIELDS_COMMON_SITE_UTIL_H_
#define PLAINBROWSER_SHIELDS_COMMON_SITE_UTIL_H_

#include <string>

class GURL;

namespace plainbrowser::shields {

// The site (eTLD+1, e.g. "example.co.uk" for "https://a.b.example.co.uk/")
// the exception list is keyed by. IP literals and single-label hosts, which
// have no registry, give the host itself. Empty for non-http(s) URLs.
std::string SiteFor(const GURL& url);

}  // namespace plainbrowser::shields

#endif  // PLAINBROWSER_SHIELDS_COMMON_SITE_UTIL_H_
