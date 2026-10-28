// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/browser/ad_block_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "plainbrowser/adblock/common/adblock_engine.mojom.h"
#include "plainbrowser/adblock/engine/ad_block_engine_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace plainbrowser::adblock {
namespace {

constexpr char kAdScript[] =
    "https://pagead2.googlesyndication.com/pagead/js/adsbygoogle.js";
constexpr char kControlScript[] = "https://plain-browser.test/app.js";
constexpr char kPage[] = "https://plain-browser.test/";

// The browser side against an in-process engine service, the way the utility
// process would serve it; "crashing" the engine destroys the service, which
// closes the pipe like a dead process.
class AdBlockServiceTest : public testing::Test {
 public:
  // On the service's sequence: asks, then kills the engine before it can see
  // the question.
  void AskThenCrash(base::OnceCallback<void(bool)> done) {
    service_->ShouldBlock(GURL(kAdScript), GURL(kPage), "script",
                          std::move(done));
    engines_.back().reset();
  }

 protected:
  void SetUp() override {
    service_ = AdBlockService::CreateForTesting(base::BindRepeating(
        &AdBlockServiceTest::Launch, base::Unretained(this)));
  }

  void TearDown() override {
    OnServiceSequence(base::BindOnce(
        [](AdBlockServiceTest* test) { test->engines_.clear(); },
        base::Unretained(this)));
    task_environment_.RunUntilIdle();
    service_.reset();
  }

  // Runs on the service's sequence.
  void Launch(mojo::PendingReceiver<mojom::AdblockEngine> receiver) {
    engines_.push_back(
        std::make_unique<AdBlockEngineService>(std::move(receiver)));
  }

  void OnServiceSequence(base::OnceClosure task) {
    base::test::TestFuture<void> done;
    service_->task_runner()->PostTaskAndReply(FROM_HERE, std::move(task),
                                              done.GetCallback());
    ASSERT_TRUE(done.Wait());
  }

  void CrashEngine() {
    OnServiceSequence(base::BindOnce(
        [](AdBlockServiceTest* test) { test->engines_.back().reset(); },
        base::Unretained(this)));
  }

  bool ShouldBlock(AdBlockService* service, const std::string& url) {
    base::test::TestFuture<bool> result;
    service->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&AdBlockService::ShouldBlock, base::Unretained(service),
                       GURL(url), GURL(kPage), std::string("script"),
                       result.GetSequenceBoundCallback()));
    return result.Get();
  }
  bool ShouldBlock(const std::string& url) {
    return ShouldBlock(service_.get(), url);
  }

  mojom::CosmeticResultPtr Cosmetic(AdBlockService* service) {
    base::test::TestFuture<mojom::CosmeticResultPtr> result;
    service->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&AdBlockService::CosmeticResources,
                                  base::Unretained(service), GURL(kPage),
                                  result.GetSequenceBoundCallback()));
    return result.Take();
  }

  void UseLists(AdBlockService::ListTexts lists) {
    base::test::TestFuture<void> done;
    service_->UseLists(std::move(lists), done.GetSequenceBoundCallback());
    ASSERT_TRUE(done.Wait());
  }

  int Starts() {
    int starts = 0;
    OnServiceSequence(base::BindOnce(
        [](AdBlockService* service, int* starts) {
          *starts = service->engine_starts_for_testing();
        },
        base::Unretained(service_.get()), base::Unretained(&starts)));
    return starts;
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<AdBlockService> service_;
  // Touched on the service's sequence only.
  std::vector<std::unique_ptr<AdBlockEngineService>> engines_;
};

TEST_F(AdBlockServiceTest, StartsTheEngineOnFirstUseAndBlocksThroughIt) {
  EXPECT_EQ(0, Starts());
  EXPECT_TRUE(ShouldBlock(kAdScript));
  EXPECT_FALSE(ShouldBlock(kControlScript));
  EXPECT_EQ(1, Starts());
  EXPECT_EQ(service_.get(), AdBlockService::GetInstance());
}

TEST_F(AdBlockServiceTest, WithoutALauncherEverythingFailsOpen) {
  std::unique_ptr<AdBlockService> no_engine =
      AdBlockService::CreateForTesting(AdBlockService::Launcher());
  EXPECT_FALSE(ShouldBlock(no_engine.get(), kAdScript));
  // generic_hide: the renderer then does not collect class names for nothing.
  EXPECT_TRUE(Cosmetic(no_engine.get())->generic_hide);
  task_environment_.RunUntilIdle();
}

TEST_F(AdBlockServiceTest, AnEngineThatWentAwayIsStartedAgain) {
  EXPECT_TRUE(ShouldBlock(kAdScript));
  CrashEngine();
  EXPECT_TRUE(ShouldBlock(kAdScript));
  EXPECT_EQ(2, Starts());
}

TEST_F(AdBlockServiceTest, AQueryInFlightWhenTheEngineGoesAwayFailsOpen) {
  EXPECT_TRUE(ShouldBlock(kAdScript));  // The engine is up.
  base::test::TestFuture<bool> result;
  service_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&AdBlockServiceTest::AskThenCrash, base::Unretained(this),
                     result.GetSequenceBoundCallback()));
  EXPECT_FALSE(result.Get());
}

TEST_F(AdBlockServiceTest, DownloadedListsAreDroppedAfterTheEngineWentAway) {
  UseLists({{"easylist", "[Adblock Plus 2.0]\n||fresh-ads.test^\n"}});
  EXPECT_TRUE(ShouldBlock("https://fresh-ads.test/a.js"));
  EXPECT_FALSE(ShouldBlock(kAdScript));
  // The downloaded list may be what made it crash: the next engine gets the
  // compiled-in lists only.
  CrashEngine();
  EXPECT_TRUE(ShouldBlock(kAdScript));
  EXPECT_FALSE(ShouldBlock("https://fresh-ads.test/a.js"));
}

TEST_F(AdBlockServiceTest, ListsGivenBeforeTheFirstStartReachTheEngine) {
  UseLists({{"easylist", "[Adblock Plus 2.0]\n||fresh-ads.test^\n"}});
  EXPECT_EQ(1, Starts());
  EXPECT_TRUE(ShouldBlock("https://fresh-ads.test/a.js"));
  EXPECT_EQ(1, Starts());
}

TEST_F(AdBlockServiceTest, StatusReportsTheEngineAndTheListsInUse) {
  base::test::TestFuture<AdBlockService::Status> before;
  service_->GetStatus(before.GetSequenceBoundCallback());
  EXPECT_FALSE(before.Get().engine_running);
  EXPECT_EQ(0, before.Get().engine_starts);

  UseLists({{"easylist", "[Adblock Plus 2.0]\n||fresh-ads.test^\n"}});
  base::test::TestFuture<AdBlockService::Status> after;
  service_->GetStatus(after.GetSequenceBoundCallback());
  EXPECT_TRUE(after.Get().engine_running);
  EXPECT_EQ(1, after.Get().engine_starts);
  EXPECT_FALSE(after.Get().gave_up);
  EXPECT_EQ(std::vector<std::string>({"easylist"}),
            after.Get().downloaded_lists);
}

TEST_F(AdBlockServiceTest, BlockingSwitchAndCounterStartOnAndAtZero) {
  // The switch is read by AdblockFilterImpl; FilterListUpdater mirrors the
  // pref into it.
  EXPECT_TRUE(service_->blocking_enabled());
  service_->SetBlockingEnabled(false);
  EXPECT_FALSE(service_->blocking_enabled());
  EXPECT_EQ(0, service_->blocked_total());
  service_->CountBlocked();
  service_->CountBlocked();
  EXPECT_EQ(2, service_->blocked_total());
}

TEST_F(AdBlockServiceTest, StopsStartingAnEngineThatKeepsGoingAway) {
  for (int i = 0; i < AdBlockService::kMaxEngineStarts; ++i) {
    EXPECT_TRUE(ShouldBlock(kAdScript));
    CrashEngine();
  }
  EXPECT_FALSE(ShouldBlock(kAdScript));
  EXPECT_EQ(AdBlockService::kMaxEngineStarts, Starts());
  base::test::TestFuture<AdBlockService::Status> status;
  service_->GetStatus(status.GetSequenceBoundCallback());
  EXPECT_TRUE(status.Get().gave_up);
}

}  // namespace
}  // namespace plainbrowser::adblock
