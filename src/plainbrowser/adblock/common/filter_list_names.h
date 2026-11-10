// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_COMMON_FILTER_LIST_NAMES_H_
#define PLAINBROWSER_ADBLOCK_COMMON_FILTER_LIST_NAMES_H_

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "plainbrowser/adblock/common/pref_names.h"

// The filter lists compiled into the browser (rs/src/lib.rs, lists/), by the
// names the Rust bridge, the engine process, the list updater and the mirror
// (infra/filter-lists) use.
namespace plainbrowser::adblock {

// Always on (unless blocking as a whole is off). plainbrowser-removeparam is
// ours and ships with the browser only, so it is not listed here.
inline constexpr std::array<std::string_view, 3> kDefaultLists = {
    "easylist", "easyprivacy", "easylistpolish"};

// Lists the user turns on or off in Settings > Privacy and security, each
// with its Local State pref (registered by FilterListUpdater).
struct OptionalList {
  std::string_view name;
  const char* pref;
  // On until the user turns it off. Must match the Rust bridge's
  // OPTIONAL_LISTS (adblock_ffi_unittest.cc checks).
  bool default_on;
};
inline constexpr std::array<OptionalList, 3> kOptionalLists = {{
    // EasyList Cookie List: cookie consent banners.
    {"easylistcookie", prefs::kCookieListEnabled, true},
    // Fanboy's Annoyance List: pop-ups, newsletter and notification prompts,
    // social widgets (contains the cookie list).
    {"fanboyannoyance", prefs::kAnnoyanceListEnabled, false},
    // uBlock filters, uBlock Origin's own list.
    {"ublockfilters", prefs::kUblockFiltersEnabled, false},
}};

// The names of the optional lists that are on by default, in kOptionalLists
// order: what the engine process uses before the browser configures it.
inline std::vector<std::string> DefaultOptionalLists() {
  std::vector<std::string> names;
  for (const OptionalList& list : kOptionalLists) {
    if (list.default_on) {
      names.emplace_back(list.name);
    }
  }
  return names;
}

// Upper bound on the user's own filters (prefs::kCustomFilters), in bytes. The
// Rust bridge ignores longer text (MAX_CUSTOM_FILTERS_BYTES).
inline constexpr size_t kMaxCustomFiltersBytes = 1024 * 1024;

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_COMMON_FILTER_LIST_NAMES_H_
