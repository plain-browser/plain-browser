// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/renderer/adblock_url_loader_throttle.h"

#include <memory>
#include <string>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "plainbrowser/adblock/common/adblock.mojom.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace plainbrowser::adblock {
namespace {

using network::mojom::RequestDestination;

// The browser-side filter, scripted per test.
class FakeFilter : public mojom::AdblockFilter {
 public:
  enum class Mode {
    kAllow,
    kBlock,
    kNeverAnswer,  // Keeps the callback: the browser is "hanging".
    kClosePipe,    // Drops every receiver: the browser side went away.
  };

  mojo::PendingRemote<mojom::AdblockFilter> BindNewRemote() {
    mojo::PendingRemote<mojom::AdblockFilter> remote;
    receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }
  void ClosePipes() { receivers_.Clear(); }
  void set_mode(Mode mode) { mode_ = mode; }
  int requests() const { return requests_; }
  int clones() const { return clones_; }
  const GURL& last_url() const { return last_url_; }
  const GURL& last_source_url() const { return last_source_url_; }
  const GURL& last_top_level_url() const { return last_top_level_url_; }
  const std::optional<blink::LocalFrameToken>& last_frame_token() const {
    return last_frame_token_;
  }
  const std::string& last_type() const { return last_type_; }
  ShouldBlockRequestCallback TakeHeldCallback() { return std::move(held_); }

  // mojom::AdblockFilter:
  void Clone(mojo::PendingReceiver<mojom::AdblockFilter> receiver) override {
    ++clones_;
    receivers_.Add(this, std::move(receiver));
  }
  void ShouldBlockRequest(
      const GURL& url,
      const GURL& source_url,
      const std::string& request_type,
      const GURL& top_level_url,
      const std::optional<blink::LocalFrameToken>& frame_token,
      ShouldBlockRequestCallback callback) override {
    ++requests_;
    last_url_ = url;
    last_source_url_ = source_url;
    last_top_level_url_ = top_level_url;
    last_frame_token_ = frame_token;
    last_type_ = request_type;
    switch (mode_) {
      case Mode::kAllow:
        std::move(callback).Run(false);
        break;
      case Mode::kBlock:
        std::move(callback).Run(true);
        break;
      case Mode::kNeverAnswer:
        held_ = std::move(callback);
        break;
      case Mode::kClosePipe:
        // Close first so dropping the callback has no endpoint to report to.
        receivers_.Clear();
        break;
    }
  }
  void GetCosmeticCss(const GURL& url,
                      const GURL& top_level_url,
                      GetCosmeticCssCallback callback) override {
    std::move(callback).Run(std::string(), /*generic_hide=*/true, {}, {});
  }
  void GetScriptlets(const GURL& url,
                     const GURL& top_level_url,
                     GetScriptletsCallback callback) override {
    std::move(callback).Run(std::string());
  }
  void GetRedirectHosts(GetRedirectHostsCallback callback) override {
    std::move(callback).Run({});
  }
  void GetRedirect(const GURL& url,
                   const GURL& source_url,
                   const GURL& top_level_url,
                   const std::optional<blink::LocalFrameToken>& frame_token,
                   GetRedirectCallback callback) override {
    std::move(callback).Run(std::string());
  }
  void GetGenericCosmeticCss(const GURL& url,
                             const GURL& top_level_url,
                             const std::vector<std::string>& classes,
                             const std::vector<std::string>& ids,
                             GetGenericCosmeticCssCallback callback) override {
    std::move(callback).Run(std::string());
  }

 private:
  Mode mode_ = Mode::kAllow;
  int requests_ = 0;
  int clones_ = 0;
  GURL last_url_;
  GURL last_source_url_;
  GURL last_top_level_url_;
  std::optional<blink::LocalFrameToken> last_frame_token_;
  std::string last_type_;
  ShouldBlockRequestCallback held_;
  mojo::ReceiverSet<mojom::AdblockFilter> receivers_;
};

class FakeDelegate : public blink::URLLoaderThrottle::Delegate {
 public:
  bool resumed() const { return resumed_; }
  bool cancelled() const { return cancelled_; }
  int error_code() const { return error_code_; }
  const std::string& reason() const { return reason_; }

  // blink::URLLoaderThrottle::Delegate:
  void CancelWithError(int error_code, std::string_view custom_reason) override {
    cancelled_ = true;
    error_code_ = error_code;
    reason_ = std::string(custom_reason);
  }
  void Resume() override { resumed_ = true; }

 private:
  bool resumed_ = false;
  bool cancelled_ = false;
  int error_code_ = net::OK;
  std::string reason_;
};

class AdblockURLLoaderThrottleTest : public testing::Test {
 protected:
  AdblockURLLoaderThrottleTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  }

  void SetUp() override { remote_.Bind(fake_.BindNewRemote()); }

  std::unique_ptr<AdblockURLLoaderThrottle> MakeThrottle() {
    auto throttle = std::make_unique<AdblockURLLoaderThrottle>(
        remote_.get(), kTopLevelUrl, frame_token_);
    throttle->set_delegate(&delegate_);
    return throttle;
  }

  const GURL kTopLevelUrl{"https://page.test/"};
  const blink::LocalFrameToken frame_token_{base::UnguessableToken::Create()};

  static network::ResourceRequest Request(const char* url,
                                          RequestDestination destination) {
    network::ResourceRequest request;
    request.url = GURL(url);
    request.destination = destination;
    return request;
  }

  // Starts `request` on a fresh throttle and returns whether it was deferred.
  bool Start(network::ResourceRequest request,
             std::unique_ptr<AdblockURLLoaderThrottle>* out = nullptr) {
    std::unique_ptr<AdblockURLLoaderThrottle> throttle = MakeThrottle();
    bool defer = false;
    throttle->WillStartRequest(&request, &defer);
    if (out) {
      *out = std::move(throttle);
    } else {
      // Keep it alive until the answer or the fail-open timer fires.
      throttles_.push_back(std::move(throttle));
    }
    return defer;
  }

  base::test::TaskEnvironment task_environment_;
  FakeFilter fake_;
  FakeDelegate delegate_;
  mojo::Remote<mojom::AdblockFilter> remote_;
  std::vector<std::unique_ptr<AdblockURLLoaderThrottle>> throttles_;
};

TEST(AdblockRequestTypeTest, MapsFetchDestinationsToAdblockRustNames) {
  auto type = [](RequestDestination d) {
    network::ResourceRequest request;
    request.destination = d;
    return AdblockURLLoaderThrottle::RequestTypeFor(request);
  };
  EXPECT_EQ("script", type(RequestDestination::kScript));
  EXPECT_EQ("script", type(RequestDestination::kWorker));
  EXPECT_EQ("script", type(RequestDestination::kServiceWorker));
  EXPECT_EQ("image", type(RequestDestination::kImage));
  EXPECT_EQ("stylesheet", type(RequestDestination::kStyle));
  EXPECT_EQ("font", type(RequestDestination::kFont));
  EXPECT_EQ("media", type(RequestDestination::kVideo));
  EXPECT_EQ("media", type(RequestDestination::kAudio));
  EXPECT_EQ("sub_frame", type(RequestDestination::kIframe));
  EXPECT_EQ("sub_frame", type(RequestDestination::kFencedframe));
  EXPECT_EQ("object", type(RequestDestination::kObject));
  EXPECT_EQ("object", type(RequestDestination::kEmbed));
  // fetch() / XMLHttpRequest / sendBeacon.
  EXPECT_EQ("xmlhttprequest", type(RequestDestination::kEmpty));
  EXPECT_EQ("other", type(RequestDestination::kDocument));
  EXPECT_EQ("other", type(RequestDestination::kReport));
  EXPECT_EQ("other", type(RequestDestination::kManifest));
}

TEST_F(AdblockURLLoaderThrottleTest, BlockedRequestIsCancelled) {
  fake_.set_mode(FakeFilter::Mode::kBlock);
  EXPECT_TRUE(Start(Request("https://ads.test/a.js", RequestDestination::kScript)));
  EXPECT_FALSE(delegate_.cancelled());  // Decided asynchronously.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(delegate_.cancelled());
  EXPECT_FALSE(delegate_.resumed());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, delegate_.error_code());
  EXPECT_EQ("PlainBrowserAdblock", delegate_.reason());
  EXPECT_EQ(GURL("https://ads.test/a.js"), fake_.last_url());
  EXPECT_EQ("script", fake_.last_type());
  // The page's top-level URL (per-site exceptions) and the requesting frame
  // (per-tab counter) travel with the request.
  EXPECT_EQ(kTopLevelUrl, fake_.last_top_level_url());
  ASSERT_TRUE(fake_.last_frame_token().has_value());
  EXPECT_EQ(frame_token_, *fake_.last_frame_token());
}

TEST_F(AdblockURLLoaderThrottleTest, WorkerRequestsCarryNoFrameToken) {
  auto throttle = std::make_unique<AdblockURLLoaderThrottle>(
      remote_.get(), GURL(), std::nullopt);
  throttle->set_delegate(&delegate_);
  network::ResourceRequest request =
      Request("https://cdn.test/w.js", RequestDestination::kWorker);
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(delegate_.resumed());
  EXPECT_EQ("script", fake_.last_type());
  EXPECT_FALSE(fake_.last_frame_token().has_value());
  EXPECT_EQ(GURL(), fake_.last_top_level_url());
}

TEST_F(AdblockURLLoaderThrottleTest, AllowedRequestResumes) {
  fake_.set_mode(FakeFilter::Mode::kAllow);
  EXPECT_TRUE(Start(Request("https://site.test/a.png", RequestDestination::kImage)));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(delegate_.resumed());
  EXPECT_FALSE(delegate_.cancelled());
  EXPECT_EQ("image", fake_.last_type());
}

TEST_F(AdblockURLLoaderThrottleTest, NonHttpRequestIsNotDeferred) {
  fake_.set_mode(FakeFilter::Mode::kBlock);
  EXPECT_FALSE(Start(Request("data:image/png;base64,AA==", RequestDestination::kImage)));
  EXPECT_FALSE(Start(Request("blob:https://site.test/1234", RequestDestination::kImage)));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, fake_.requests());
  EXPECT_FALSE(delegate_.resumed());
  EXPECT_FALSE(delegate_.cancelled());
}

TEST_F(AdblockURLLoaderThrottleTest, SourceUrlIsTheInitiatorOrTheRequestUrl) {
  network::ResourceRequest request =
      Request("https://cdn.test/a.js", RequestDestination::kScript);
  request.request_initiator = url::Origin::Create(GURL("https://page.test/x/y"));
  EXPECT_TRUE(Start(request));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(GURL("https://page.test/"), fake_.last_source_url());

  EXPECT_TRUE(Start(Request("https://cdn.test/b.js", RequestDestination::kScript)));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(GURL("https://cdn.test/b.js"), fake_.last_source_url());
}

TEST_F(AdblockURLLoaderThrottleTest, FailsOpenWhenTheBrowserNeverAnswers) {
  fake_.set_mode(FakeFilter::Mode::kNeverAnswer);
  EXPECT_TRUE(Start(Request("https://ads.test/a.js", RequestDestination::kScript)));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, fake_.requests());
  EXPECT_FALSE(delegate_.resumed());

  task_environment_.FastForwardBy(base::Seconds(3));
  EXPECT_TRUE(delegate_.resumed());
  EXPECT_FALSE(delegate_.cancelled());

  // A late "block" must not cancel a request that already went through.
  fake_.TakeHeldCallback().Run(true);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(delegate_.cancelled());
}

TEST_F(AdblockURLLoaderThrottleTest, FailsOpenAtOnceWhenThePipeCloses) {
  fake_.set_mode(FakeFilter::Mode::kClosePipe);
  EXPECT_TRUE(Start(Request("https://ads.test/a.js", RequestDestination::kScript)));
  task_environment_.RunUntilIdle();  // No time passes: the disconnect decides.
  EXPECT_TRUE(delegate_.resumed());
  EXPECT_FALSE(delegate_.cancelled());
}

TEST_F(AdblockURLLoaderThrottleTest, DetachedThrottleUsesItsOwnClonedPipe) {
  fake_.set_mode(FakeFilter::Mode::kBlock);
  std::unique_ptr<AdblockURLLoaderThrottle> throttle = MakeThrottle();
  throttle->DetachFromCurrentSequence();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, fake_.clones());

  network::ResourceRequest request =
      Request("https://ads.test/a.js", RequestDestination::kScript);
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(delegate_.cancelled());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, delegate_.error_code());
}

TEST_F(AdblockURLLoaderThrottleTest, DetachedThrottleFailsOpenWithoutABrowserSide) {
  fake_.set_mode(FakeFilter::Mode::kBlock);
  std::unique_ptr<AdblockURLLoaderThrottle> throttle = MakeThrottle();
  throttle->DetachFromCurrentSequence();
  task_environment_.RunUntilIdle();
  fake_.ClosePipes();  // Kill switch flipped / browser side gone.
  task_environment_.RunUntilIdle();

  network::ResourceRequest request =
      Request("https://ads.test/a.js", RequestDestination::kScript);
  bool defer = false;
  throttle->WillStartRequest(&request, &defer);
  task_environment_.RunUntilIdle();
  // Either not deferred at all or resumed by the disconnect: never blocked,
  // never hanging.
  EXPECT_EQ(defer, delegate_.resumed());
  EXPECT_FALSE(delegate_.cancelled());
}

}  // namespace
}  // namespace plainbrowser::adblock
