// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_FINGERPRINT_RENDERER_FINGERPRINT_EXCEPTIONS_H_
#define PLAINBROWSER_FINGERPRINT_RENDERER_FINGERPRINT_EXCEPTIONS_H_

#include <string_view>

namespace plainbrowser::fingerprint {

// True when `site` (an eTLD+1 such as "mbank.pl") must not get fingerprint
// noise: it is in the built-in list (banks and payment services whose fraud
// checks break on a changing device fingerprint) or in the
// --fingerprint-exceptions switch.
bool IsExceptedSite(std::string_view site);

}  // namespace plainbrowser::fingerprint

#endif  // PLAINBROWSER_FINGERPRINT_RENDERER_FINGERPRINT_EXCEPTIONS_H_
