// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/fingerprint/renderer/fingerprint_script.h"

#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"

namespace plainbrowser::fingerprint {
namespace {

// The script is embedded by embed_text.py at build time; the agent fills in
// the two seed placeholders before injecting it.
TEST(FingerprintScriptTest, EmbeddedScriptCarriesBothSeedPlaceholders) {
  const std::string_view script(kFingerprintScript);
  EXPECT_GT(script.size(), 1000u);
  EXPECT_NE(std::string_view::npos, script.find("var SEED_HI = __PB_SEED_HI__;"));
  EXPECT_NE(std::string_view::npos, script.find("var SEED_LO = __PB_SEED_LO__;"));
  // Line structure survives embedding (each source line is one chunk).
  EXPECT_NE(std::string_view::npos, script.find('\n'));
  EXPECT_EQ(std::string_view::npos, script.find("\r\n"));
}

}  // namespace
}  // namespace plainbrowser::fingerprint
