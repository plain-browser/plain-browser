// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_SHIELDS_COMMON_PREF_NAMES_H_
#define PLAINBROWSER_SHIELDS_COMMON_PREF_NAMES_H_

namespace plainbrowser::shields::prefs {

// Profile pref: list of sites (eTLD+1, lower case, e.g. "example.com") on
// which all protections are off: ad blocking, tracking-parameter stripping and
// fingerprint protection. One list shared by the three features. Registered by
// browser/shields_prefs.h, edited from the toolbar bubble (ui/).
inline constexpr char kShieldsExceptions[] = "plainbrowser.shields.exceptions";

}  // namespace plainbrowser::shields::prefs

#endif  // PLAINBROWSER_SHIELDS_COMMON_PREF_NAMES_H_
