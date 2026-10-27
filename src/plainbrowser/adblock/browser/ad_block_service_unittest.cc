// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/browser/ad_block_service.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
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

  AdBlockService::Cosmetic CosmeticResources(const std::string& url) {
    base::test::TestFuture<AdBlockService::Cosmetic> result;
    service_->task_runner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&AdBlockService::CosmeticResources,
                       base::Unretained(service_.get()), GURL(url)),
        result.GetCallback());
    return result.Take();
  }
  std::string CosmeticCss(const std::string& url) {
    return CosmeticResources(url).css;
  }

  std::string GenericCss(const std::string& url,
                         std::vector<std::string> classes,
                         std::vector<std::string> ids) {
    base::test::TestFuture<std::string> result;
    service_->task_runner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&AdBlockService::GenericCosmeticCss,
                       base::Unretained(service_.get()), GURL(url),
                       std::move(classes), std::move(ids)),
        result.GetCallback());
    return result.Take();
  }

  template <typename R, typename... Args>
  R OnEngine(R (AdBlockService::*method)(Args...), Args... args) {
    base::test::TestFuture<R> result;
    service_->task_runner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(method, base::Unretained(service_.get()),
                       std::move(args)...),
        result.GetCallback());
    return result.Take();
  }

  // UseLists() and wait for the swap.
  void UseLists(AdBlockService::ListTexts lists) {
    base::test::TestFuture<void> swapped;
    service_->UseLists(std::move(lists), swapped.GetCallback());
    ASSERT_TRUE(swapped.Wait());
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
  EXPECT_TRUE(CosmeticResources("chrome://newtab/").generic_hide);
  EXPECT_EQ(std::string(), GenericCss("chrome://newtab/", {"a-sponsor"}, {}));
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

// EasyList: ##.a-sponsor and ###ad-top are generic; techymedies.com#@##ad-top
// turns the id rule off on that one site.
TEST_F(AdBlockServiceTest, GenericClassAndIdRulesForTheDocument) {
  EXPECT_FALSE(CosmeticResources(kPage).generic_hide);
  const std::string css =
      GenericCss(kPage, {"a-sponsor", "article-body"}, {"ad-top", "main"});
  EXPECT_NE(std::string::npos, css.find(".a-sponsor")) << css;
  EXPECT_NE(std::string::npos, css.find("#ad-top")) << css;
  EXPECT_EQ(std::string::npos, css.find("article-body")) << css;
  EXPECT_EQ(std::string::npos, css.find("#main")) << css;
  EXPECT_EQ(std::string(), GenericCss(kPage, {"article-body"}, {"main"}));
  EXPECT_EQ(std::string::npos,
            GenericCss("https://techymedies.com/", {}, {"ad-top"}).find("#ad-top"));
}

TEST_F(AdBlockServiceTest, UseListsReplacesTheNamedListOnly) {
  UseLists({{"easylist", "[Adblock Plus 2.0]\n||fresh-ads.test^\n"}});
  EXPECT_TRUE(ShouldBlock("https://fresh-ads.test/a.js", kPage, "script"));
  // EasyList's own host rule is gone with the replaced list...
  EXPECT_FALSE(ShouldBlock(kAdScript, kPage, "script"));
  // ...while EasyList Polish and the parameter list are still the bundled ones.
  EXPECT_NE(std::string::npos,
            CosmeticCss(kSiteWithHideRules).find("section.pub-slot-wrapper"));
  EXPECT_EQ(GURL("https://plain-browser.test/p?q=1"),
            RewrittenUrl("https://plain-browser.test/p?utm_source=x&q=1", kPage,
                         "main_frame"));
  // No downloaded lists: back to the compiled-in engine.
  UseLists({});
  EXPECT_TRUE(ShouldBlock(kAdScript, kPage, "script"));
  EXPECT_FALSE(ShouldBlock("https://fresh-ads.test/a.js", kPage, "script"));
}

TEST_F(AdBlockServiceTest, TheLatestUseListsCallWins) {
  base::test::TestFuture<void> first;
  base::test::TestFuture<void> second;
  service_->UseLists({{"easylist", "||first.test^\n"}}, first.GetCallback());
  service_->UseLists({{"easylist", "||second.test^\n"}}, second.GetCallback());
  ASSERT_TRUE(first.Wait());
  ASSERT_TRUE(second.Wait());
  EXPECT_TRUE(ShouldBlock("https://second.test/", kPage, "script"));
  EXPECT_FALSE(ShouldBlock("https://first.test/", kPage, "script"));
}

// EasyPrivacy: ||static.chartbeat.com/js/chartbeat.js$redirect=chartbeat.js,script
TEST_F(AdBlockServiceTest, RedirectRulesServeOurStandIns) {
  const std::string data = OnEngine<std::string, const GURL&, const GURL&>(
      &AdBlockService::RedirectUrl,
      GURL("https://static.chartbeat.com/js/chartbeat.js"), GURL(kPage));
  EXPECT_TRUE(base::StartsWith(data, "data:application/javascript;base64,"))
      << data;
  EXPECT_EQ(std::string(),
            (OnEngine<std::string, const GURL&, const GURL&>(
                &AdBlockService::RedirectUrl, GURL(kControlScript),
                GURL(kPage))));
  const std::vector<std::string> hosts =
      OnEngine<std::vector<std::string>>(&AdBlockService::RedirectHosts);
  EXPECT_NE(hosts.end(), std::ranges::find(hosts, "static.chartbeat.com"));
}

// EasyPrivacy: madewell.com##+js(cookie-remover, dns_cookie)
TEST_F(AdBlockServiceTest, ScriptletsComeFromOurResources) {
  const std::string script = OnEngine<std::string, const GURL&>(
      &AdBlockService::Scriptlets, GURL("https://www.madewell.com/"));
  EXPECT_NE(std::string::npos, script.find("function pbCookieRemover(")) << script;
  EXPECT_NE(std::string::npos, script.find("dns_cookie")) << script;
  EXPECT_EQ(std::string(), (OnEngine<std::string, const GURL&>(
                               &AdBlockService::Scriptlets, GURL(kPage))));
}

// EasyList: cjleo.com#?#.alt-widget-title:has-text(advertisement)
TEST_F(AdBlockServiceTest, ProceduralSelectorsStayOutOfTheStylesheet) {
  const AdBlockService::Cosmetic c = CosmeticResources("https://cjleo.com/");
  EXPECT_NE(c.procedural_selectors.end(),
            std::ranges::find(c.procedural_selectors,
                              ".alt-widget-title:has-text(advertisement)"));
  EXPECT_EQ(std::string::npos, c.css.find(":has-text(")) << c.css;
}

}  // namespace
}  // namespace plainbrowser::adblock
