// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/fingerprint/common/features.h"

#include "base/feature_list.h"
#include "plainbrowser/fingerprint/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plainbrowser::fingerprint {
namespace {

// Kill switch and switches are documented command-line names: pin them.
TEST(FingerprintFeaturesTest, KillSwitchNameAndDefault) {
  EXPECT_STREQ("PlainBrowserFingerprintProtection",
               kPlainBrowserFingerprintProtection.name);
  EXPECT_EQ(base::FEATURE_ENABLED_BY_DEFAULT,
            kPlainBrowserFingerprintProtection.default_state);
  EXPECT_TRUE(base::FeatureList::IsEnabled(kPlainBrowserFingerprintProtection));
}

TEST(FingerprintFeaturesTest, SwitchNames) {
  EXPECT_STREQ("plainbrowser-fingerprint-session-key",
               switches::kFingerprintSessionKey);
  EXPECT_STREQ("fingerprint-exceptions", switches::kFingerprintExceptions);
}

}  // namespace
}  // namespace plainbrowser::fingerprint
