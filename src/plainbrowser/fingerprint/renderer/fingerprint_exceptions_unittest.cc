// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/fingerprint/renderer/fingerprint_exceptions.h"

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace plainbrowser::fingerprint {
namespace {

const std::vector<std::string> kNoUserList;

TEST(FingerprintExceptionsTest, BuiltInListMatchesTheExactSite) {
  EXPECT_TRUE(IsExceptedSite("mbank.pl", kNoUserList));
  EXPECT_TRUE(IsExceptedSite("paypal.com", kNoUserList));
  EXPECT_TRUE(IsExceptedSite("przelewy24.pl", kNoUserList));
  EXPECT_FALSE(IsExceptedSite("example.com", kNoUserList));
  EXPECT_FALSE(IsExceptedSite("", kNoUserList));
}

TEST(FingerprintExceptionsTest, MatchingIgnoresCase) {
  EXPECT_TRUE(IsExceptedSite("MBank.PL", kNoUserList));
  EXPECT_TRUE(IsExceptedSite("PAYPAL.COM", kNoUserList));
}

TEST(FingerprintExceptionsTest, OnlyWholeSitesMatch) {
  // Callers pass the eTLD+1; neither hosts below it nor the registry match.
  EXPECT_FALSE(IsExceptedSite("www.mbank.pl", kNoUserList));
  EXPECT_FALSE(IsExceptedSite("pl", kNoUserList));
  EXPECT_FALSE(IsExceptedSite("mbank.pl.evil.test", kNoUserList));
  EXPECT_FALSE(IsExceptedSite("mbank", kNoUserList));
}

TEST(FingerprintExceptionsTest, ParseExceptionListNormalises) {
  EXPECT_EQ((std::vector<std::string>{"example.com", "shop.example"}),
            ParseExceptionList(" Example.COM ,, shop.example ,"));
  EXPECT_TRUE(ParseExceptionList("").empty());
  EXPECT_TRUE(ParseExceptionList(" , ").empty());
}

TEST(FingerprintExceptionsTest, UserListAddsToTheBuiltInList) {
  const std::vector<std::string> user = ParseExceptionList("Shop.Example");
  EXPECT_TRUE(IsExceptedSite("shop.example", user));
  EXPECT_TRUE(IsExceptedSite("SHOP.example", user));
  EXPECT_TRUE(IsExceptedSite("mbank.pl", user));
  EXPECT_FALSE(IsExceptedSite("shop.example", kNoUserList));
  EXPECT_FALSE(IsExceptedSite("other.example", user));
}

TEST(FingerprintExceptionsTest, ProcessCommandLineWithoutSwitchIsBuiltInOnly) {
  EXPECT_TRUE(IsExceptedSite("mbank.pl"));
  EXPECT_FALSE(IsExceptedSite("shop.example"));
}

}  // namespace
}  // namespace plainbrowser::fingerprint
