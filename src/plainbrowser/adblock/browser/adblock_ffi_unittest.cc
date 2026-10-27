// Copyright Plain Browser contributors. MPL-2.0.
// The cxx bridge over adblock-rust as seen from C++ (rs/src/lib.rs). The Rust
// side has its own tests; these check the types and semantics that cross.
#include <string>

#include "plainbrowser/adblock/rs/src/lib.rs.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/rust/cxx/v1/cxx.h"

namespace plainbrowser::adblock {
namespace {

constexpr char kRules[] =
    "||ads.example.com^\n"
    "@@||ads.example.com/ok.js\n"
    "||tracker.example.net^$third-party\n"
    "example.org##.banner\n"
    "*$removeparam=utm_source\n";

BlockResult Check(const rust::Box<AdblockEngine>& engine,
                  const char* url,
                  const char* source,
                  const char* type) {
  return engine->check_network_request(rust::Str(url), rust::Str(source),
                                       rust::Str(type));
}

TEST(AdblockFfiTest, BlockRulesAndExceptions) {
  rust::Box<AdblockEngine> engine = new_engine(rust::Str(kRules));
  BlockResult blocked =
      Check(engine, "https://ads.example.com/x.js", "https://site.com/", "script");
  EXPECT_TRUE(blocked.matched);
  EXPECT_FALSE(blocked.exception);

  BlockResult excepted = Check(engine, "https://ads.example.com/ok.js",
                               "https://site.com/", "script");
  EXPECT_FALSE(excepted.matched);
  EXPECT_TRUE(excepted.exception);

  EXPECT_FALSE(
      Check(engine, "https://site.com/app.js", "https://site.com/", "script")
          .matched);
}

TEST(AdblockFfiTest, ThirdPartyRulesUseTheSourceUrl) {
  rust::Box<AdblockEngine> engine = new_engine(rust::Str(kRules));
  EXPECT_TRUE(Check(engine, "https://tracker.example.net/p.gif",
                    "https://site.com/", "image")
                  .matched);
  EXPECT_FALSE(Check(engine, "https://tracker.example.net/p.gif",
                     "https://www.tracker.example.net/", "image")
                   .matched);
}

TEST(AdblockFfiTest, StringsCrossTheBridge) {
  rust::Box<AdblockEngine> engine = new_engine(rust::Str(kRules));
  BlockResult r = Check(engine, "https://site.com/a?utm_source=nl&id=1",
                        "https://other.com/", "main_frame");
  EXPECT_FALSE(r.matched);
  EXPECT_EQ("https://site.com/a?id=1", std::string(r.rewritten_url));
  EXPECT_TRUE(Check(engine, "https://site.com/a?id=1", "https://other.com/",
                    "main_frame")
                  .rewritten_url.empty());

  EXPECT_NE(std::string::npos,
            std::string(
                engine->cosmetic_resources(rust::Str("https://example.org/"))
                    .css)
                .find(".banner"));
  EXPECT_TRUE(
      engine->cosmetic_resources(rust::Str("https://other.org/")).css.empty());

  // Generic class/id rules: C++ vectors cross as rust::Vec<rust::String>.
  rust::Box<AdblockEngine> generic =
      new_engine(rust::Str("##.ad-box\n###top-ad\n"));
  rust::Vec<rust::String> classes;
  classes.push_back(rust::String("ad-box"));
  classes.push_back(rust::String("content"));
  rust::Vec<rust::String> ids;
  ids.push_back(rust::String("top-ad"));
  const std::string css(generic->generic_cosmetic_css(
      rust::Str("https://site.com/"), classes, ids));
  EXPECT_NE(std::string::npos, css.find(".ad-box")) << css;
  EXPECT_NE(std::string::npos, css.find("#top-ad")) << css;
  EXPECT_EQ(std::string::npos, css.find("content")) << css;
}

TEST(AdblockFfiTest, UnparsableUrlIsNotBlocked) {
  rust::Box<AdblockEngine> engine = new_engine(rust::Str(kRules));
  EXPECT_FALSE(Check(engine, "not a url", "", "script").matched);
  EXPECT_FALSE(Check(engine, "https://ads.example.com/x.js", "", "script")
                   .rewritten_url.size() > 0);
}

}  // namespace
}  // namespace plainbrowser::adblock
