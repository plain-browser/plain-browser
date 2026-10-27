// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_SHIELDS_BROWSER_SHIELDS_PREFS_H_
#define PLAINBROWSER_SHIELDS_BROWSER_SHIELDS_PREFS_H_

#include <string>
#include <string_view>
#include <vector>

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace plainbrowser::shields {

// Called from chrome::RegisterProfilePrefs (patches/60-ui). Registers
// prefs::kShieldsExceptions.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// The exception list of one profile: lower-case eTLD+1 sites, in pref order,
// invalid entries skipped.
std::vector<std::string> GetExceptions(const PrefService* prefs);

bool IsSiteExcepted(const PrefService* prefs, std::string_view site);

// Adds `site` to or removes it from the profile's list. No-op for an empty
// site. Observers of the pref (ShieldsService) pick up the change.
void SetSiteExcepted(PrefService* prefs, std::string_view site, bool excepted);

}  // namespace plainbrowser::shields

#endif  // PLAINBROWSER_SHIELDS_BROWSER_SHIELDS_PREFS_H_
