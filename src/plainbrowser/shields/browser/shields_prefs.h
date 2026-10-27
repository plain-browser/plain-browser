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

// The site an exception-list entry stands for. The toolbar bubble stores sites
// as SiteFor() returns them; the settings page stores what the user typed, so
// an entry may also be a host ("mail.example.com"), a URL
// ("https://www.example.com/a") or mixed case. Empty when the entry names no
// http(s) host.
std::string SiteForEntry(std::string_view entry);

// The exception list of one profile: sites (SiteForEntry() of each entry), in
// pref order, without duplicates and invalid entries.
std::vector<std::string> GetExceptions(const PrefService* prefs);

bool IsSiteExcepted(const PrefService* prefs, std::string_view site);

// Adds `site` to or removes it from the profile's list; removing drops every
// entry that stands for `site`. No-op for an empty site. Observers of the pref
// (ShieldsService) pick up the change.
void SetSiteExcepted(PrefService* prefs, std::string_view site, bool excepted);

// Rewrites the stored list as GetExceptions() returns it, so the settings page
// shows the sites the list applies to. Returns false, and writes nothing, when
// the list is already in that form.
bool NormalizeExceptions(PrefService* prefs);

}  // namespace plainbrowser::shields

#endif  // PLAINBROWSER_SHIELDS_BROWSER_SHIELDS_PREFS_H_
