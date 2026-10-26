// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_RENDERER_COSMETIC_FILTER_AGENT_H_
#define PLAINBROWSER_ADBLOCK_RENDERER_COSMETIC_FILTER_AGENT_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "content/public/renderer/render_frame_observer.h"
#include "plainbrowser/adblock/common/adblock.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

namespace plainbrowser::adblock {

// Per-frame element hiding: when a document element is created, fetch the
// site's hide selectors from the browser and insert them as a user-origin
// stylesheet. Self-owned; deleted in OnDestruct(). Scriptlets and generic
// class/id hiding (adblock-rust hidden_class_id_selectors) are not done yet.
class CosmeticFilterAgent : public content::RenderFrameObserver {
 public:
  static void Create(content::RenderFrame* render_frame);

  CosmeticFilterAgent(const CosmeticFilterAgent&) = delete;
  CosmeticFilterAgent& operator=(const CosmeticFilterAgent&) = delete;

 private:
  explicit CosmeticFilterAgent(content::RenderFrame* render_frame);
  ~CosmeticFilterAgent() override;

  // content::RenderFrameObserver:
  void DidCreateDocumentElement() override;
  void OnDestruct() override;

  void OnCss(const GURL& document_url, const std::string& css);

  mojo::Remote<mojom::AdblockFilter> filter_;
  base::WeakPtrFactory<CosmeticFilterAgent> weak_factory_{this};
};

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_RENDERER_COSMETIC_FILTER_AGENT_H_
