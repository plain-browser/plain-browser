// Copyright Draftway. MPL-2.0.
#include "draftway/adblock/renderer/cosmetic_filter_agent.h"

#include "base/functional/bind.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace draftway::adblock {

// static
void CosmeticFilterAgent::Create(content::RenderFrame* render_frame) {
  new CosmeticFilterAgent(render_frame);  // Self-owned.
}

CosmeticFilterAgent::CosmeticFilterAgent(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {}

CosmeticFilterAgent::~CosmeticFilterAgent() = default;

void CosmeticFilterAgent::DidCreateDocumentElement() {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  const GURL url(frame->GetDocument().Url());
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return;
  }
  if (!filter_.is_bound()) {
    blink::Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        filter_.BindNewPipeAndPassReceiver());
  }
  filter_->GetCosmeticCss(
      url, base::BindOnce(&CosmeticFilterAgent::OnCss,
                          weak_factory_.GetWeakPtr(), url));
}

void CosmeticFilterAgent::OnCss(const GURL& document_url,
                                const std::string& css) {
  if (css.empty()) {
    return;
  }
  blink::WebDocument document = render_frame()->GetWebFrame()->GetDocument();
  // Ignore late replies for a document that has since been replaced.
  if (GURL(document.Url()) != document_url) {
    return;
  }
  document.InsertStyleSheet(blink::WebString::FromUTF8(css), nullptr,
                            blink::WebCssOrigin::kUser);
}

void CosmeticFilterAgent::OnDestruct() {
  delete this;
}

}  // namespace draftway::adblock
