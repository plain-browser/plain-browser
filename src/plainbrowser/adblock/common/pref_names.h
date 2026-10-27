// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_COMMON_PREF_NAMES_H_
#define PLAINBROWSER_ADBLOCK_COMMON_PREF_NAMES_H_

namespace plainbrowser::adblock::prefs {

// Local State, bool, default true: download filter list updates
// (FilterListUpdater). Toggled on chrome://settings/privacy.
inline constexpr char kListUpdatesEnabled[] =
    "plainbrowser.adblock.list_updates.enabled";

// Local State, time: when the last update check finished (successfully or
// not), so a restart does not check again before the interval is over.
inline constexpr char kListUpdatesLastCheck[] =
    "plainbrowser.adblock.list_updates.last_check";

}  // namespace plainbrowser::adblock::prefs

#endif  // PLAINBROWSER_ADBLOCK_COMMON_PREF_NAMES_H_
