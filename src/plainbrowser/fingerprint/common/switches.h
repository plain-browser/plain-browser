// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_FINGERPRINT_COMMON_SWITCHES_H_
#define PLAINBROWSER_FINGERPRINT_COMMON_SWITCHES_H_

namespace plainbrowser::fingerprint::switches {

// Browser -> renderer: hex session key the per-site seeds derive from. Set by
// the browser process for every renderer; never read from the user's command
// line.
inline constexpr char kFingerprintSessionKey[] =
    "plainbrowser-fingerprint-session-key";

// User-settable, forwarded to renderers: comma-separated list of sites
// (eTLD+1, e.g. "mbank.pl,ing.pl") that get no fingerprint noise, in addition
// to the built-in list (renderer/fingerprint_exceptions.cc).
inline constexpr char kFingerprintExceptions[] = "fingerprint-exceptions";

}  // namespace plainbrowser::fingerprint::switches

#endif  // PLAINBROWSER_FINGERPRINT_COMMON_SWITCHES_H_
