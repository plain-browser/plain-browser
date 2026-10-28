// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_CRASH_COMMON_PREF_NAMES_H_
#define PLAINBROWSER_CRASH_COMMON_PREF_NAMES_H_

namespace plainbrowser::crash::prefs {

// Local State, bool, default false: upload crash reports to
// crash.plain-browser.app. Toggled on chrome://settings/privacy; CrashConsent
// mirrors it into the registry (for the next start) and into the running
// Crashpad. Dumps are written locally either way (chrome://crashes).
inline constexpr char kCrashReportsEnabled[] =
    "plainbrowser.crash_reports.enabled";

}  // namespace plainbrowser::crash::prefs

#endif  // PLAINBROWSER_CRASH_COMMON_PREF_NAMES_H_
