// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/renderer/cosmetic_filter_agent.h"

#include <array>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/origin.h"

namespace plainbrowser::adblock {

namespace {

// Passes after load for content added by scripts (ad slots are usually filled
// within seconds).
constexpr std::array<base::TimeDelta, 4> kRescanDelays = {
    base::Seconds(1), base::Seconds(3), base::Seconds(10), base::Seconds(30)};
// Per message to the browser; the rest goes in the next pass.
constexpr size_t kMaxGenericTokens = 2000;
// Per document: pages with more distinct names stop being scanned.
constexpr size_t kMaxTokensPerDocument = 50000;
// Longer names are not class names anyone writes rules for.
constexpr size_t kMaxTokenLength = 256;

bool Usable(const std::string& token) {
  return !token.empty() && token.size() <= kMaxTokenLength;
}

// Class names in a class attribute value.
std::vector<std::string> SplitClasses(const std::string& value) {
  return base::SplitString(value, base::kWhitespaceASCII,
                           base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

}  // namespace

// static
void CosmeticFilterAgent::Create(content::RenderFrame* render_frame) {
  new CosmeticFilterAgent(render_frame);  // Self-owned.
}

CosmeticFilterAgent::CosmeticFilterAgent(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {}

CosmeticFilterAgent::~CosmeticFilterAgent() = default;

void CosmeticFilterAgent::DidCreateDocumentElement() {
  // A new document: drop replies and timers of the previous one.
  weak_factory_.InvalidateWeakPtrs();
  rescan_timer_.Stop();
  generic_ = false;
  resources_known_ = false;
  scan_pending_ = false;
  sent_classes_.clear();
  sent_ids_.clear();
  next_rescan_ = 0;

  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  document_url_ = GURL(frame->GetDocument().Url());
  if (!document_url_.SchemeIsHTTPOrHTTPS()) {
    return;
  }
  if (!filter_.is_bound()) {
    blink::Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        filter_.BindNewPipeAndPassReceiver());
  }
  // The per-site exception list is keyed by the top-level site.
  top_level_url_ = url::Origin(frame->Top()->GetSecurityOrigin()).GetURL();
  filter_->GetCosmeticCss(
      document_url_, top_level_url_,
      base::BindOnce(&CosmeticFilterAgent::OnCss, weak_factory_.GetWeakPtr(),
                     document_url_));
}

void CosmeticFilterAgent::OnCss(const GURL& document_url,
                                const std::string& css,
                                bool generic_hide) {
  InsertCss(document_url, css);
  resources_known_ = true;
  generic_ = !generic_hide;
  if (scan_pending_) {
    scan_pending_ = false;
    Scan();
  }
}

void CosmeticFilterAgent::DidDispatchDOMContentLoadedEvent() {
  Scan();
}

void CosmeticFilterAgent::DidFinishLoad() {
  Scan();
  next_rescan_ = 0;
  ScheduleNextRescan();
}

void CosmeticFilterAgent::ScheduleNextRescan() {
  if (next_rescan_ >= kRescanDelays.size()) {
    return;
  }
  rescan_timer_.Start(FROM_HERE, kRescanDelays[next_rescan_++],
                      base::BindOnce(
                          [](CosmeticFilterAgent* agent) {
                            agent->Scan();
                            agent->ScheduleNextRescan();
                          },
                          base::Unretained(this)));
}

void CosmeticFilterAgent::Scan() {
  if (!document_url_.SchemeIsHTTPOrHTTPS()) {
    return;
  }
  if (!resources_known_) {
    scan_pending_ = true;  // OnCss() scans when the answer arrives.
    return;
  }
  if (!generic_ ||
      sent_classes_.size() + sent_ids_.size() >= kMaxTokensPerDocument) {
    return;
  }
  blink::WebDocument document = render_frame()->GetWebFrame()->GetDocument();
  if (GURL(document.Url()) != document_url_) {
    return;
  }

  std::vector<std::string> classes;
  std::vector<std::string> ids;
  auto full = [&] { return classes.size() + ids.size() >= kMaxGenericTokens; };
  for (const blink::WebElement& element :
       document.QuerySelectorAll(blink::WebString::FromUtf8("[class],[id]"))) {
    std::string id = element.GetIdAttribute().Utf8();
    if (Usable(id) && sent_ids_.insert(id).second) {
      ids.push_back(std::move(id));
    }
    for (std::string& name : SplitClasses(
             element.GetAttribute(blink::WebString::FromUtf8("class"))
                 .Utf8())) {
      if (full()) {
        break;
      }
      if (Usable(name) && sent_classes_.insert(name).second) {
        classes.push_back(std::move(name));
      }
    }
    if (full()) {
      break;  // The next pass picks up the rest.
    }
  }
  if (classes.empty() && ids.empty()) {
    return;
  }
  filter_->GetGenericCosmeticCss(
      document_url_, top_level_url_, std::move(classes), std::move(ids),
      base::BindOnce(&CosmeticFilterAgent::OnGenericCss,
                     weak_factory_.GetWeakPtr(), document_url_));
}

void CosmeticFilterAgent::OnGenericCss(const GURL& document_url,
                                       const std::string& css) {
  InsertCss(document_url, css);
}

void CosmeticFilterAgent::InsertCss(const GURL& document_url,
                                    const std::string& css) {
  if (css.empty()) {
    return;
  }
  blink::WebDocument document = render_frame()->GetWebFrame()->GetDocument();
  // Ignore late replies for a document that has since been replaced.
  if (GURL(document.Url()) != document_url) {
    return;
  }
  document.InsertStyleSheet(blink::WebString::FromUtf8(css), nullptr,
                            blink::WebCssOrigin::kUser);
}

void CosmeticFilterAgent::OnDestruct() {
  delete this;
}

}  // namespace plainbrowser::adblock
