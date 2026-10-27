// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_RENDERER_COSMETIC_FILTER_AGENT_H_
#define PLAINBROWSER_ADBLOCK_RENDERER_COSMETIC_FILTER_AGENT_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "plainbrowser/adblock/common/adblock.mojom.h"
#include "url/gurl.h"
#include "v8/include/v8-forward.h"

namespace plainbrowser::adblock {

// Isolated world of our cosmetic script (renderer/cosmetic_script.js). Blink
// wants embedder world ids below 1 << 29; extensions count up from
// ISOLATED_WORLD_ID_EXTENSIONS, a small number, so the top of the range is
// ours.
inline constexpr int32_t kCosmeticWorldId = (1 << 29) - 1;

// Per-frame cosmetic filtering, for http(s) documents:
// - Scriptlets (##+js(...)): when the main world's script context is created,
//   a sync GetScriptlets call fetches them and they run at once, before any of
//   the page's scripts.
// - Hide rules: when the document element is created, GetCosmeticCss returns
//   the site's stylesheet (inserted as a user stylesheet), whether generic
//   class/id rules apply ($generichide, excepted site) and the procedural
//   filters.
// - Generic class/id rules (adblock-rust hidden_class_id_selectors): the class
//   names and ids in the document are collected at DOMContentLoaded and at
//   load; afterwards the isolated-world script's MutationObserver reports new
//   ones through __plainbrowserCosmeticReport (installed by this agent). Names
//   not sent before go to GetGenericCosmeticCss and the answer is inserted as
//   another user stylesheet.
// - Procedural filters (:has-text(), :upward(), :remove(), :style(), ...) are
//   applied by the isolated-world script, also on DOM changes.
// Self-owned; deleted in OnDestruct().
class CosmeticFilterAgent
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<CosmeticFilterAgent> {
 public:
  static void Create(content::RenderFrame* render_frame);

  CosmeticFilterAgent(const CosmeticFilterAgent&) = delete;
  CosmeticFilterAgent& operator=(const CosmeticFilterAgent&) = delete;

  // From the isolated-world script: names that appeared in the document.
  void OnNamesReported(std::vector<std::string> classes,
                       std::vector<std::string> ids);

 private:
  explicit CosmeticFilterAgent(content::RenderFrame* render_frame);
  ~CosmeticFilterAgent() override;

  // content::RenderFrameObserver:
  void DidCreateScriptContext(v8::Local<v8::Context> context,
                              int32_t world_id) override;
  void DidCreateDocumentElement() override;
  void DidDispatchDOMContentLoadedEvent() override;
  void DidFinishLoad() override;
  void OnDestruct() override;

  mojom::AdblockFilter* Filter();
  GURL TopLevelUrl() const;
  void RunScriptlets(v8::Local<v8::Context> context);
  void InstallReportFunction(v8::Local<v8::Context> context);

  void OnCss(const GURL& document_url,
             const std::string& css,
             bool generic_hide,
             const std::vector<std::string>& procedural_actions,
             const std::vector<std::string>& procedural_selectors);
  void OnGenericCss(const GURL& document_url, const std::string& css);
  void InsertCss(const GURL& document_url, const std::string& css);
  void InjectCosmeticScript(
      const std::vector<std::string>& procedural_actions,
      const std::vector<std::string>& procedural_selectors);

  // Collects class names and ids from the whole document.
  void Scan();
  // Sends the names in `classes` and `ids` not sent before for this document.
  void SendNew(std::vector<std::string> classes, std::vector<std::string> ids);

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

  base::WeakPtrFactory<CosmeticFilterAgent> weak_factory_{this};
};

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_RENDERER_COSMETIC_FILTER_AGENT_H_
