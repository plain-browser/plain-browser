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

// Local State, bool: the optional filter lists (common/filter_list_names.h,
// kOptionalLists), toggled on chrome://settings/privacy. The cookie list is on
// by default, the other two off.
inline constexpr char kCookieListEnabled[] =
    "plainbrowser.adblock.lists.easylistcookie";
inline constexpr char kAnnoyanceListEnabled[] =
    "plainbrowser.adblock.lists.fanboyannoyance";
inline constexpr char kUblockFiltersEnabled[] =
    "plainbrowser.adblock.lists.ublockfilters";

// Local State, string, default empty: the user's own filters in Adblock Plus /
// uBlock Origin syntax, one per line, edited on chrome://adblock. Added to the
// engine with the lists: an exception (@@...) here undoes a list's block rule
// unless that rule is $important. At most kMaxCustomFiltersBytes.
inline constexpr char kCustomFilters[] = "plainbrowser.adblock.custom_filters";

}  // namespace plainbrowser::adblock::prefs

#endif  // PLAINBROWSER_ADBLOCK_COMMON_PREF_NAMES_H_
