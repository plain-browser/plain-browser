// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/shields/browser/shields_prefs.h"

#include <algorithm>

#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "plainbrowser/shields/common/pref_names.h"
#include "plainbrowser/shields/common/site_util.h"
#include "url/gurl.h"

namespace plainbrowser::shields {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kShieldsExceptions);
}

std::string SiteForEntry(std::string_view entry) {
  const std::string trimmed(
      base::TrimWhitespaceASCII(entry, base::TRIM_ALL));
  // GURL would accept "not a host" as the host "not%20a%20host".
  if (trimmed.empty() ||
      std::any_of(trimmed.begin(), trimmed.end(), base::IsAsciiWhitespace<char>)) {
    return std::string();
  }
  // GURL canonicalises the host: lower case, punycode for IDN.
  return SiteFor(GURL(trimmed.find("://") == std::string::npos
                          ? "https://" + trimmed
                          : trimmed));
}

std::vector<std::string> GetExceptions(const PrefService* prefs) {
  std::vector<std::string> out;
  if (!prefs) {
    return out;
  }
  for (const base::Value& item : prefs->GetList(prefs::kShieldsExceptions)) {
    if (!item.is_string()) {
      continue;
    }
    std::string site = SiteForEntry(item.GetString());
    if (!site.empty() && std::find(out.begin(), out.end(), site) == out.end()) {
      out.push_back(std::move(site));
    }
  }
  return out;
}

bool IsSiteExcepted(const PrefService* prefs, std::string_view site) {
  if (site.empty()) {
    return false;
  }
  const std::string lower = base::ToLowerASCII(site);
  const std::vector<std::string> list = GetExceptions(prefs);
  return std::find(list.begin(), list.end(), lower) != list.end();
}

void SetSiteExcepted(PrefService* prefs, std::string_view site, bool excepted) {
  if (!prefs || site.empty()) {
    return;
  }
  const std::string lower = base::ToLowerASCII(site);
  ScopedListPrefUpdate update(prefs, prefs::kShieldsExceptions);
  base::ListValue& list = update.Get();
  list.EraseIf([&lower](const base::Value& v) {
    return v.is_string() && SiteForEntry(v.GetString()) == lower;
  });
  if (excepted) {
    list.Append(lower);
  }
}

bool NormalizeExceptions(PrefService* prefs) {
  if (!prefs) {
    return false;
  }
  const std::vector<std::string> sites = GetExceptions(prefs);
  const base::ListValue& stored = prefs->GetList(prefs::kShieldsExceptions);
  bool same = stored.size() == sites.size();
  for (size_t i = 0; same && i < sites.size(); ++i) {
    same = stored[i].is_string() && stored[i].GetString() == sites[i];
  }
  if (same) {
    return false;
  }
  base::ListValue list;
  for (const std::string& site : sites) {
    list.Append(site);
  }
  prefs->SetList(prefs::kShieldsExceptions, std::move(list));
  return true;
}

}  // namespace plainbrowser::shields
