// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/browser/ad_block_service.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace plainbrowser::adblock {
namespace {

// Blocked by EasyList (||pagead2.googlesyndication.com^); a control that no
// bundled list mentions; a site with site-specific hide rules in EasyList
// Polish (wykop.pl##section.pub-slot-wrapper).
constexpr char kAdScript[] =
    "https://pagead2.googlesyndication.com/pagead/js/adsbygoogle.js";
constexpr char kControlScript[] = "https://plain-browser.test/app.js";
constexpr char kPage[] = "https://plain-browser.test/";
constexpr char kSiteWithHideRules[] = "https://wykop.pl/";

// Runs the service the way production does: engine built from the bundled
// lists on the ThreadPool, every query posted to the engine sequence.
class AdBlockServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    service_ = AdBlockService::CreateForTesting();
    task_environment_.RunUntilIdle();  // Loads the engine.
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();
    service_.reset();
  }

  bool ShouldBlock(const std::string& url,
                   const std::string& source,
                   const std::string& type) {
    base::test::TestFuture<bool> result;
    service_->task_runner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&AdBlockService::ShouldBlock,
                       base::Unretained(service_.get()), GURL(url),
                       GURL(source), type),
        result.GetCallback());
    return result.Get();
  }

  GURL RewrittenUrl(const std::string& url,
                    const std::string& source,
                    const std::string& type) {
    base::test::TestFuture<GURL> result;
    service_->task_runner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&AdBlockService::RewrittenUrl,
                       base::Unretained(service_.get()), GURL(url),
                       GURL(source), type),
        result.GetCallback());
    return result.Get();
  }

  std::string CosmeticCss(const std::string& url) {
    base::test::TestFuture<std::string> result;
    service_->task_runner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&AdBlockService::CosmeticCss,
                       base::Unretained(service_.get()), GURL(url)),
        result.GetCallback());
    return result.Get();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<AdBlockService> service_;
};

TEST_F(AdBlockServiceTest, GetInstanceReturnsTheTestService) {
  EXPECT_EQ(service_.get(), AdBlockService::GetInstance());
}

TEST_F(AdBlockServiceTest, BlocksKnownAdHostAndPassesControl) {
  EXPECT_TRUE(ShouldBlock(kAdScript, kPage, "script"));
  EXPECT_FALSE(ShouldBlock(kControlScript, kPage, "script"));
  EXPECT_FALSE(ShouldBlock(kPage, kPage, "main_frame"));
}

TEST_F(AdBlockServiceTest, SourceUrlDefaultsToTheRequestUrl) {
  // An invalid source (browser-initiated, no initiator) must not disable
  // blocking.
  EXPECT_TRUE(ShouldBlock(kAdScript, "", "script"));
}

TEST_F(AdBlockServiceTest, NonHttpUrlsAreNeverBlocked) {
  EXPECT_FALSE(ShouldBlock("chrome://version/", kPage, "script"));
  EXPECT_FALSE(ShouldBlock("file:///C:/ads/adsbygoogle.js", kPage, "script"));
  EXPECT_FALSE(ShouldBlock("data:text/javascript,1", kPage, "script"));
  EXPECT_EQ(GURL(), RewrittenUrl("chrome://settings/?utm_source=x", kPage,
                                 "main_frame"));
  EXPECT_EQ(std::string(), CosmeticCss("chrome://newtab/"));
}

TEST_F(AdBlockServiceTest, RewrittenUrlStripsTrackingParamsAndKeepsTheRest) {
  EXPECT_EQ(GURL("https://plain-browser.test/p?a=1&b=2"),
            RewrittenUrl("https://plain-browser.test/p?a=1&utm_source=nl&"
                         "utm_campaign=x&b=2&gclid=1&fbclid=abc",
                         kPage, "main_frame"));
  // Subframes use the same rules.
  EXPECT_EQ(GURL("https://plain-browser.test/f"),
            RewrittenUrl("https://plain-browser.test/f?fbclid=1", kPage,
                         "sub_frame"));
}

TEST_F(AdBlockServiceTest, RewrittenUrlIsEmptyWhenNothingToStrip) {
  EXPECT_EQ(GURL(), RewrittenUrl("https://plain-browser.test/p?q=2", kPage,
                                 "main_frame"));
  EXPECT_EQ(GURL(),
            RewrittenUrl("https://plain-browser.test/p", kPage, "main_frame"));
}

TEST_F(AdBlockServiceTest, RewrittenUrlIsEmptyForBlockedRequests) {
  // A request that is blocked outright is not rewritten (the caller blocks).
  EXPECT_EQ(GURL(), RewrittenUrl(std::string(kAdScript) + "?utm_source=x",
                                 kPage, "script"));
}

TEST_F(AdBlockServiceTest, CosmeticCssAddsSiteSpecificRulesToGenericOnes) {
  const std::string site = CosmeticCss(kSiteWithHideRules);
  const std::string other = CosmeticCss(kPage);
  EXPECT_NE(std::string::npos, site.find("section.pub-slot-wrapper")) << site;
  EXPECT_NE(std::string::npos, site.find("{ display: none !important; }"));
  // Generic rules apply everywhere; the site-specific one only on its site.
  EXPECT_NE(std::string::npos, other.find("[data-ad-name]"));
  EXPECT_EQ(std::string::npos, other.find("section.pub-slot-wrapper"));
}

}  // namespace
}  // namespace plainbrowser::adblock
