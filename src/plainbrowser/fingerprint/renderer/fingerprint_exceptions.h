// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_FINGERPRINT_RENDERER_FINGERPRINT_EXCEPTIONS_H_
#define PLAINBROWSER_FINGERPRINT_RENDERER_FINGERPRINT_EXCEPTIONS_H_

#include <string_view>

namespace plainbrowser::fingerprint {

// True when `site` (an eTLD+1 such as "mbank.pl") must not get fingerprint
// noise: it is on the built-in compatibility list (sites that need a stable
// device fingerprint to work, for example online banking and payment services
// whose fraud checks break on a changing fingerprint) or in the
// --fingerprint-exceptions switch.
bool IsExceptedSite(std::string_view site);

}  // namespace plainbrowser::fingerprint

#endif  // PLAINBROWSER_FINGERPRINT_RENDERER_FINGERPRINT_EXCEPTIONS_H_
