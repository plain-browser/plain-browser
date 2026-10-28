// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_COMMON_PREF_NAMES_H_
#define PLAINBROWSER_ADBLOCK_COMMON_PREF_NAMES_H_

namespace plainbrowser::adblock::prefs {

// Local State, bool, default true: block ads and trackers (network rules,
// element hiding, scriptlets, $redirect). Off: AdblockFilterImpl answers
// "nothing to do" for every page; tracking-parameter stripping and debouncing
// stay on (they have their own switches). Toggled on
// chrome://settings/privacy; mirrored into AdBlockService by
// FilterListUpdater.
inline constexpr char kBlockingEnabled[] = "plainbrowser.adblock.enabled";

// Local State, bool, default true: download filter list updates
// (FilterListUpdater). Toggled on chrome://settings/privacy.
inline constexpr char kListUpdatesEnabled[] =
    "plainbrowser.adblock.list_updates.enabled";

// Local State, time: when the last update check finished (successfully or
// not), so a restart does not check again before the interval is over.
inline constexpr char kListUpdatesLastCheck[] =
    "plainbrowser.adblock.list_updates.last_check";

// Local State, double, milliseconds since the Unix epoch (0: never): when a
// check last finished successfully, i.e. the lists in use were verified
// current. A number rather than a time pref so the settings page can read it
// through settingsPrivate (shown as "Last checked").
inline constexpr char kListUpdatesLastSuccess[] =
    "plainbrowser.adblock.list_updates.last_success";

}  // namespace plainbrowser::adblock::prefs

#endif  // PLAINBROWSER_ADBLOCK_COMMON_PREF_NAMES_H_
