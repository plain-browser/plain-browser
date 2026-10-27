// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_RENDERER_COSMETIC_FILTER_AGENT_H_
#define PLAINBROWSER_ADBLOCK_RENDERER_COSMETIC_FILTER_AGENT_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "plainbrowser/adblock/common/adblock.mojom.h"
#include "url/gurl.h"

namespace plainbrowser::adblock {

// Per-frame element hiding. When a document element is created, fetch the
// site's hide selectors from the browser and insert them as a user-origin
// stylesheet. Unless the document is under $generichide (or excepted), also
// apply generic class/id rules (adblock-rust hidden_class_id_selectors): the
// class names and ids present in the document are collected at
// DOMContentLoaded, at load and on a few later passes (pages that add ads
// after load), and the ones not sent before go to the browser, whose answer is
// inserted as another user stylesheet. Self-owned; deleted in OnDestruct().
// Not done: scriptlets, procedural filters, and elements added long after load
// (no MutationObserver from C++; the last pass is kRescanDelays' last entry).
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
  void DidDispatchDOMContentLoadedEvent() override;
  void DidFinishLoad() override;
  void OnDestruct() override;

  void OnCss(const GURL& document_url,
             const std::string& css,
             bool generic_hide);
  void OnGenericCss(const GURL& document_url, const std::string& css);
  void InsertCss(const GURL& document_url, const std::string& css);

  // Collects class names and ids not sent before and asks the browser for the
  // generic rules that match them.
  void Scan();
  void ScheduleNextRescan();

  mojo::Remote<mojom::AdblockFilter> filter_;

  // State of the current document; reset in DidCreateDocumentElement().
  GURL document_url_;
  GURL top_level_url_;
  // Generic rules apply: the browser said so and the document is http(s).
  bool generic_ = false;
  // The browser's answer to GetCosmeticCss has arrived (until then, scans
  // wait: the answer may say generic rules do not apply).
  bool resources_known_ = false;
  bool scan_pending_ = false;
  std::set<std::string> sent_classes_;
  std::set<std::string> sent_ids_;
  size_t next_rescan_ = 0;
  base::OneShotTimer rescan_timer_;

  base::WeakPtrFactory<CosmeticFilterAgent> weak_factory_{this};
};

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_RENDERER_COSMETIC_FILTER_AGENT_H_
