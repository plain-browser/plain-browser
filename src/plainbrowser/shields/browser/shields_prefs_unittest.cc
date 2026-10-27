// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/shields/browser/shields_prefs.h"

#include <string>
#include <vector>

#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "plainbrowser/shields/common/pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plainbrowser::shields {

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

class ShieldsPrefsTest : public ::testing::Test {
 protected:
  ShieldsPrefsTest() {
    // RegisterProfilePrefs() needs a PrefRegistrySyncable; the list pref is
    // all it registers.
    prefs_.registry()->RegisterListPref(prefs::kShieldsExceptions);
  }

  void SetStored(std::vector<std::string> entries) {
    base::ListValue list;
    for (std::string& entry : entries) {
      list.Append(std::move(entry));
    }
    prefs_.SetList(prefs::kShieldsExceptions, std::move(list));
  }

  std::vector<std::string> Stored() const {
    std::vector<std::string> out;
    for (const base::Value& v : prefs_.GetList(prefs::kShieldsExceptions)) {
      out.push_back(v.is_string() ? v.GetString() : "<not a string>");
    }
    return out;
  }

  TestingPrefServiceSimple prefs_;
};

TEST(ShieldsSiteForEntryTest, SitesHostsAndUrlsGiveTheSite) {
  EXPECT_EQ(SiteForEntry("example.com"), "example.com");
  EXPECT_EQ(SiteForEntry("www.example.com"), "example.com");
  EXPECT_EQ(SiteForEntry("  Mail.Example.CO.UK "), "example.co.uk");
  EXPECT_EQ(SiteForEntry("https://www.example.pl/a?b=c"), "example.pl");
  EXPECT_EQ(SiteForEntry("http://sub.example.org:8080"), "example.org");
}

TEST(ShieldsSiteForEntryTest, HostsWithoutRegistryAreKept) {
  EXPECT_EQ(SiteForEntry("localhost"), "localhost");
  EXPECT_EQ(SiteForEntry("192.168.1.33"), "192.168.1.33");
}

TEST(ShieldsSiteForEntryTest, NonWebEntriesAreEmpty) {
  EXPECT_EQ(SiteForEntry(""), "");
  EXPECT_EQ(SiteForEntry("   "), "");
  EXPECT_EQ(SiteForEntry("file:///C:/x.html"), "");
  EXPECT_EQ(SiteForEntry("chrome://settings"), "");
  EXPECT_EQ(SiteForEntry("not a host"), "");
}

TEST_F(ShieldsPrefsTest, GetExceptionsNormalisesAndDropsDuplicates) {
  SetStored({"www.example.com", "example.com", "Bank.PL", "chrome://x", ""});
  EXPECT_THAT(GetExceptions(&prefs_), ElementsAre("example.com", "bank.pl"));
}

TEST_F(ShieldsPrefsTest, IsSiteExceptedMatchesTheSiteOnly) {
  SetStored({"mail.example.com"});
  EXPECT_TRUE(IsSiteExcepted(&prefs_, "example.com"));
  EXPECT_TRUE(IsSiteExcepted(&prefs_, "EXAMPLE.com"));
  EXPECT_FALSE(IsSiteExcepted(&prefs_, "example.org"));
  EXPECT_FALSE(IsSiteExcepted(&prefs_, ""));
}

TEST_F(ShieldsPrefsTest, SetSiteExceptedRemovesEveryEntryForTheSite) {
  SetStored({"www.example.com", "bank.pl", "https://example.com/"});
  SetSiteExcepted(&prefs_, "example.com", /*excepted=*/false);
  EXPECT_THAT(Stored(), ElementsAre("bank.pl"));
  SetSiteExcepted(&prefs_, "Example.com", /*excepted=*/true);
  EXPECT_THAT(Stored(), ElementsAre("bank.pl", "example.com"));
}

TEST_F(ShieldsPrefsTest, NormalizeRewritesTypedEntries) {
  SetStored({"https://www.Example.com/login", "bank.pl", "example.com", "x y"});
  EXPECT_TRUE(NormalizeExceptions(&prefs_));
  EXPECT_THAT(Stored(), ElementsAre("example.com", "bank.pl"));
}

TEST_F(ShieldsPrefsTest, NormalizeLeavesANormalListAlone) {
  SetStored({"example.com", "bank.pl"});
  EXPECT_FALSE(NormalizeExceptions(&prefs_));
  EXPECT_THAT(Stored(), ElementsAre("example.com", "bank.pl"));
  SetStored({});
  EXPECT_FALSE(NormalizeExceptions(&prefs_));
  EXPECT_THAT(Stored(), IsEmpty());
}

}  // namespace

}  // namespace plainbrowser::shields
