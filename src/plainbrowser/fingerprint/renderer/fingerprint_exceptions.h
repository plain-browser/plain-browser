// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_FINGERPRINT_RENDERER_FINGERPRINT_EXCEPTIONS_H_
#define PLAINBROWSER_FINGERPRINT_RENDERER_FINGERPRINT_EXCEPTIONS_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"

namespace plainbrowser::fingerprint {

// True when `site` (an eTLD+1 such as "mbank.pl") must not get fingerprint
// noise: it is on the built-in compatibility list (sites that need a stable
// device fingerprint to work, for example online banking and payment services
// whose fraud checks break on a changing fingerprint) or in the
// --fingerprint-exceptions switch (parsed once per process).
bool IsExceptedSite(std::string_view site);

// Same, with the user's list given explicitly (lower-case eTLD+1 entries, as
// ParseExceptionList() returns them) instead of read from the command line.
bool IsExceptedSite(std::string_view site,
                    base::span<const std::string> user_exceptions);

// Parses a --fingerprint-exceptions value: comma-separated sites, whitespace
// trimmed, lower-cased, empty items dropped.
std::vector<std::string> ParseExceptionList(std::string_view value);

}  // namespace plainbrowser::fingerprint

#endif  // PLAINBROWSER_FINGERPRINT_RENDERER_FINGERPRINT_EXCEPTIONS_H_
