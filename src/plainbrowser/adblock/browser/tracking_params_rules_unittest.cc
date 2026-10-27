// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/browser/tracking_params_rules.h"

#include <optional>

#include "base/feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace plainbrowser::adblock::tracking_params {
namespace {

// The kill switch is a documented command-line name: pin it.
TEST(TrackingParamsRulesTest, KillSwitchNameAndDefault) {
  EXPECT_STREQ("PlainBrowserTrackingParams", kPlainBrowserTrackingParams.name);
  EXPECT_EQ(base::FEATURE_ENABLED_BY_DEFAULT,
            kPlainBrowserTrackingParams.default_state);
  EXPECT_TRUE(base::FeatureList::IsEnabled(kPlainBrowserTrackingParams));
}

TEST(TrackingParamsRulesTest, EligibleOnlyForHttpNonPost) {
  EXPECT_TRUE(IsEligibleNavigation(GURL("https://a.test/"), false));
  EXPECT_TRUE(IsEligibleNavigation(GURL("http://a.test/?utm_source=x"), false));
  // The body of a POST could not be replayed by the re-issued navigation.
  EXPECT_FALSE(IsEligibleNavigation(GURL("https://a.test/?utm_source=x"), true));
  EXPECT_FALSE(IsEligibleNavigation(GURL("chrome://settings/?utm_source=x"),
                                    false));
  EXPECT_FALSE(IsEligibleNavigation(GURL("file:///C:/a.html?utm_source=x"),
                                    false));
  EXPECT_FALSE(IsEligibleNavigation(GURL("data:text/html,x"), false));
  EXPECT_FALSE(IsEligibleNavigation(GURL("about:blank"), false));
  EXPECT_FALSE(IsEligibleNavigation(GURL(), false));
}

TEST(TrackingParamsRulesTest, NeedsCheckRequiresAQuery) {
  EXPECT_TRUE(NeedsCheck(GURL("https://a.test/p?utm_source=x"), false));
  EXPECT_TRUE(NeedsCheck(GURL("https://a.test/p?q=1"), false));
  EXPECT_FALSE(NeedsCheck(GURL("https://a.test/p"), false));
  EXPECT_FALSE(NeedsCheck(GURL("https://a.test/p#utm_source=x"), false));
  EXPECT_FALSE(NeedsCheck(GURL("https://a.test/p?utm_source=x"), true));
  EXPECT_FALSE(NeedsCheck(GURL("chrome://flags/?utm_source=x"), false));
}

TEST(TrackingParamsRulesTest, SourceUrlIsTheInitiatorOrTheUrlItself) {
  const GURL url("https://dest.test/p?utm_source=x");
  EXPECT_EQ(GURL("https://page.test/"),
            SourceUrlFor(url::Origin::Create(GURL("https://page.test/a/b")),
                         url));
  // Browser-initiated (typed, bookmark): first-party to itself.
  EXPECT_EQ(url, SourceUrlFor(std::nullopt, url));
  // Opaque initiators (sandboxed frames, data: URLs) have no usable URL.
  EXPECT_EQ(url, SourceUrlFor(url::Origin(), url));
}

TEST(TrackingParamsRulesTest, ExceptionSiteIsTheTopLevelPage) {
  const GURL destination("https://dest.test/p?utm_source=x");
  const GURL page("https://page.test/");
  // A main-frame navigation becomes the top-level page itself.
  EXPECT_EQ(destination, ExceptionUrlFor(true, destination, page));
  // A subframe is excepted together with the page that hosts it.
  EXPECT_EQ(page, ExceptionUrlFor(false, destination, page));
}

TEST(TrackingParamsRulesTest, ReissueOnlyForAChangedUrlStillBeingNavigated) {
  const GURL original("https://a.test/p?utm_source=x&q=1");
  const GURL clean("https://a.test/p?q=1");
  EXPECT_TRUE(ShouldReissue(original, clean, original));
  // The engine had nothing to strip.
  EXPECT_FALSE(ShouldReissue(original, GURL(), original));
  EXPECT_FALSE(ShouldReissue(original, original, original));
  // Redirected again while the engine was asked: the answer is stale.
  EXPECT_FALSE(ShouldReissue(original, clean, GURL("https://b.test/")));
}

}  // namespace
}  // namespace plainbrowser::adblock::tracking_params
