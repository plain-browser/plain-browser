// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/crash/browser/crash_consent.h"

#include <windows.h>

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "components/prefs/testing_pref_service.h"
#include "plainbrowser/crash/common/crash_constants.h"
#include "plainbrowser/crash/common/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plainbrowser::crash {
namespace {

constexpr wchar_t kPath[] = L"Software\\PlainBrowserTest";

class CrashConsentTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_.OverrideRegistry(HKEY_CURRENT_USER));
    CrashConsent::RegisterLocalStatePrefs(local_state_.registry());
  }

  // The registry value, or -1 when it is missing.
  int Registry() {
    base::win::RegKey key(HKEY_CURRENT_USER, kPath,
                          KEY_QUERY_VALUE | KEY_WOW64_32KEY);
    DWORD value = 0;
    return key.ReadValueDW(kConsentRegValue, &value) == ERROR_SUCCESS
               ? static_cast<int>(value)
               : -1;
  }

  std::unique_ptr<CrashConsent> Start() {
    return std::make_unique<CrashConsent>(
        &local_state_, kPath,
        base::BindRepeating(
            [](std::vector<bool>* calls, bool v) { calls->push_back(v); },
            &calls_));
  }

  base::test::TaskEnvironment task_environment_;
  registry_util::RegistryOverrideManager registry_override_;
  TestingPrefServiceSimple local_state_;
  std::vector<bool> calls_;
};

TEST_F(CrashConsentTest, OffByDefaultAndWrittenAtStart) {
  auto consent = Start();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(calls_, std::vector<bool>({false}));
  EXPECT_EQ(Registry(), 0);
}

TEST_F(CrashConsentTest, FollowsThePref) {
  auto consent = Start();
  local_state_.SetBoolean(prefs::kCrashReportsEnabled, true);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(Registry(), 1);
  local_state_.SetBoolean(prefs::kCrashReportsEnabled, false);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(Registry(), 0);
  EXPECT_EQ(calls_, std::vector<bool>({false, true, false}));
}

TEST_F(CrashConsentTest, StaleRegistryIsCorrectedAtStart) {
  ASSERT_TRUE(CrashConsent::WriteRegistry(kPath, true));
  ASSERT_EQ(Registry(), 1);
  auto consent = Start();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(Registry(), 0);
}

}  // namespace
}  // namespace plainbrowser::crash
