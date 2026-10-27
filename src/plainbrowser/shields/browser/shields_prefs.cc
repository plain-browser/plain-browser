// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/shields/browser/shields_prefs.h"

#include <algorithm>

#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "plainbrowser/shields/common/pref_names.h"

namespace plainbrowser::shields {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kShieldsExceptions);
}

std::vector<std::string> GetExceptions(const PrefService* prefs) {
  std::vector<std::string> out;
  if (!prefs) {
    return out;
  }
  for (const base::Value& item : prefs->GetList(prefs::kShieldsExceptions)) {
    if (item.is_string() && !item.GetString().empty()) {
      out.push_back(base::ToLowerASCII(item.GetString()));
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
    return v.is_string() && base::ToLowerASCII(v.GetString()) == lower;
  });
  if (excepted) {
    list.Append(lower);
  }
}

}  // namespace plainbrowser::shields
