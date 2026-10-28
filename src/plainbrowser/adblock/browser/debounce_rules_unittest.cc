// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/browser/debounce_rules.h"

#include <optional>

#include "base/feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace plainbrowser::adblock::debounce {
namespace {

std::optional<GURL> Target(const char* url) {
  return TargetFor(GURL(url), BuiltInRules());
}

// The kill switch is a documented command-line name: pin it.
TEST(DebounceRulesTest, KillSwitchNameAndDefault) {
  EXPECT_STREQ("PlainBrowserDebounce", kPlainBrowserDebounce.name);
  EXPECT_EQ(base::FEATURE_ENABLED_BY_DEFAULT,
            kPlainBrowserDebounce.default_state);
}

TEST(DebounceRulesTest, KnownRedirectorsGoToTheTarget) {
  EXPECT_EQ(GURL("https://example.com/a?b=1"),
            Target("https://l.facebook.com/l.php?u=https%3A%2F%2Fexample.com"
                   "%2Fa%3Fb%3D1&h=AT0xyz"));
  EXPECT_EQ(GURL("https://example.com/"),
            Target("https://www.google.com/url?sa=t&url=https%3A%2F%2F"
                   "example.com%2F&ved=2ah"));
  EXPECT_EQ(GURL("https://example.com/"),
            Target("https://www.google.pl/url?q=https://example.com/&sa=U"));
  EXPECT_EQ(GURL("https://example.com/"),
            Target("https://www.youtube.com/redirect?event=video_description"
                   "&q=https%3A%2F%2Fexample.com%2F&v=abc"));
  EXPECT_EQ(GURL("https://example.com/"),
            Target("https://steamcommunity.com/linkfilter/"
                   "?u=https%3A%2F%2Fexample.com%2F"));
  EXPECT_EQ(GURL("http://example.com/"),
            Target("https://vk.com/away.php?to=http%3A%2F%2Fexample.com%2F"));
}

TEST(DebounceRulesTest, SubdomainsOfARuleHostMatch) {
  EXPECT_EQ(GURL("https://example.com/"),
            Target("https://eur01.safelinks.protection.outlook.com/"
                   "?url=https%3A%2F%2Fexample.com%2F&data=05%7C01"));
  // A host that only ends with the same letters does not.
  EXPECT_EQ(std::nullopt,
            Target("https://evilgoogle.com/url?q=https://example.com/"));
}

TEST(DebounceRulesTest, Base64Targets) {
  EXPECT_EQ(GURL("https://example.com/a?b=1"),
            Target("https://www.bing.com/ck/a?!&&p=abc&u="
                   "a1aHR0cHM6Ly9leGFtcGxlLmNvbS9hP2I9MQ&ntb=1"));
  // Padding is accepted as well; a wrong prefix or bad base64 is left alone.
  EXPECT_EQ(GURL("https://example.com/a?b=1"),
            Target("https://www.bing.com/ck/a?u="
                   "a1aHR0cHM6Ly9leGFtcGxlLmNvbS9hP2I9MQ%3D%3D"));
  EXPECT_EQ(std::nullopt,
            Target("https://www.bing.com/ck/a?u=aHR0cHM6Ly9leGFtcGxlLmNvbS8"));
  EXPECT_EQ(std::nullopt, Target("https://www.bing.com/ck/a?u=a1***"));
}

TEST(DebounceRulesTest, OnlyTheRulePath) {
  // google.com/search is the results page itself, not the redirector.
  EXPECT_EQ(std::nullopt,
            Target("https://www.google.com/search?q=https://example.com/"));
  EXPECT_EQ(std::nullopt,
            Target("https://l.facebook.com/l.phpx?u=https://example.com/"));
  // A rule path ending in '/' is a prefix.
  EXPECT_EQ(GURL("https://example.com/"),
            Target("https://steamcommunity.com/linkfilter/x"
                   "?url=https://example.com/"));
}

TEST(DebounceRulesTest, TargetMustBeHttpAndValid) {
  EXPECT_EQ(std::nullopt,
            Target("https://l.facebook.com/l.php?u=javascript%3Aalert(1)"));
  EXPECT_EQ(std::nullopt,
            Target("https://l.facebook.com/l.php?u=file%3A%2F%2F%2Fc%3A%2F"));
  EXPECT_EQ(std::nullopt, Target("https://l.facebook.com/l.php?u=not%20a%20url"));
  EXPECT_EQ(std::nullopt, Target("https://l.facebook.com/l.php?h=AT0xyz"));
  EXPECT_EQ(std::nullopt, Target("https://l.facebook.com/l.php"));
  // Not http(s) itself, or no rule for the host.
  EXPECT_EQ(std::nullopt, Target("ftp://l.facebook.com/l.php?u=https://a.test/"));
  EXPECT_EQ(std::nullopt, Target("https://example.com/l.php?u=https://a.test/"));
}

TEST(DebounceRulesTest, FirstParameterNameWins) {
  // q is tried before url.
  EXPECT_EQ(GURL("https://a.test/"),
            Target("https://www.google.com/url?url=https://b.test/"
                   "&q=https://a.test/"));
  // An unusable first name falls back to the next.
  EXPECT_EQ(GURL("https://b.test/"),
            Target("https://www.google.com/url?q=x&url=https://b.test/"));
}

TEST(DebounceRulesTest, NestedRedirectorsUnwrapOneStepAtATime) {
  // The re-issued navigation goes through the throttle again, so a chain ends
  // at the real target; each step is strictly shorter, so it terminates.
  std::optional<GURL> step = Target(
      "https://www.google.com/url?q=https%3A%2F%2Fl.facebook.com%2Fl.php%3Fu%3D"
      "https%253A%252F%252Fexample.com%252F");
  ASSERT_TRUE(step);
  EXPECT_EQ(GURL("https://l.facebook.com/l.php?u=https%3A%2F%2Fexample.com%2F"),
            *step);
  EXPECT_EQ(GURL("https://example.com/"), TargetFor(*step, BuiltInRules()));
}

TEST(DebounceRulesTest, EveryBuiltInRuleIsWellFormed) {
  for (const Rule& rule : BuiltInRules()) {
    SCOPED_TRACE(rule.host);
    EXPECT_FALSE(rule.host.empty());
    EXPECT_TRUE(rule.path.starts_with('/'));
    EXPECT_FALSE(rule.params.empty());
    EXPECT_EQ(rule.encoding == Encoding::kBase64Url, !rule.value_prefix.empty());
  }
}

}  // namespace
}  // namespace plainbrowser::adblock::debounce
