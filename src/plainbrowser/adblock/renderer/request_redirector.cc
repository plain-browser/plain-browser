// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/renderer/request_redirector.h"

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "plainbrowser/adblock/common/adblock.mojom.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace plainbrowser::adblock {

namespace {

// Short, so a list update (a new engine in the browser) reaches running
// renderers soon; one sync call a minute per process at most, and only while
// pages make requests.
constexpr base::TimeDelta kRefreshInterval = base::Minutes(1);
// While the browser's engine is still loading the list is empty; ask again
// soon instead of waiting the full interval.
constexpr base::TimeDelta kRetryWhenEmpty = base::Seconds(5);

class Redirector {
 public:
  static Redirector& Get() {
    static base::NoDestructor<Redirector> instance;
    return *instance;
  }

  bool HostListed(std::string_view host) {
    const base::TimeTicks now = base::TimeTicks::Now();
    if (fetched_.is_null() ||
        now - fetched_ > (hosts_.empty() ? kRetryWhenEmpty : kRefreshInterval)) {
      fetched_ = now;
      std::vector<std::string> hosts;
      if (Filter()->GetRedirectHosts(&hosts)) {
        hosts_ = std::set<std::string>(hosts.begin(), hosts.end());
      }
      VLOG(1) << "Plain Browser redirect hosts: " << hosts_.size();
    }
    // The host or any parent domain of it.
    while (!host.empty()) {
      if (hosts_.contains(std::string(host))) {
        return true;
      }
      const size_t dot = host.find('.');
      if (dot == std::string_view::npos) {
        break;
      }
      host.remove_prefix(dot + 1);
    }
    return false;
  }

  mojom::AdblockFilter* Filter() {
    if (!filter_.is_bound()) {
      blink::Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
          filter_.BindNewPipeAndPassReceiver());
    }
    return filter_.get();
  }

 private:
  friend class base::NoDestructor<Redirector>;
  Redirector() = default;

  mojo::Remote<mojom::AdblockFilter> filter_;
  std::set<std::string> hosts_;
  base::TimeTicks fetched_;
};

}  // namespace

bool MaybeRedirectRequest(blink::WebLocalFrame* frame,
                          const GURL& url,
                          const url::Origin* initiator_origin,
                          GURL* new_url) {
  if (!frame || !new_url || !url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  Redirector& redirector = Redirector::Get();
  if (!redirector.HostListed(url.host())) {
    return false;
  }
  const GURL source =
      initiator_origin ? initiator_origin->GetURL()
                       : GURL(frame->GetDocument().Url());
  const GURL top_level = url::Origin(frame->Top()->GetSecurityOrigin()).GetURL();
  std::string data_url;
  if (!redirector.Filter()->GetRedirect(url, source, top_level,
                                        frame->GetLocalFrameToken(),
                                        &data_url) ||
      data_url.empty()) {
    VLOG(1) << "Plain Browser: no redirect for " << url.spec();
    return false;
  }
  VLOG(1) << "Plain Browser: redirected " << url.spec();
  GURL replacement(data_url);
  if (!replacement.is_valid() || !replacement.SchemeIs("data")) {
    return false;
  }
  *new_url = std::move(replacement);
  return true;
}

}  // namespace plainbrowser::adblock
