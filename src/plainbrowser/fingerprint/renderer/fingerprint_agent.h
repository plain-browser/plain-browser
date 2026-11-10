// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_FINGERPRINT_RENDERER_FINGERPRINT_AGENT_H_
#define PLAINBROWSER_FINGERPRINT_RENDERER_FINGERPRINT_AGENT_H_

#include <cstdint>
#include <string>

#include "content/public/renderer/render_frame_observer.h"
#include "v8/include/v8-forward.h"

namespace plainbrowser::fingerprint {

// Per-frame fingerprint protection. When the main-world script context of a
// document is created (before any page script runs) it injects
// fingerprint_script.js with a seed derived from the session key and the
// top-level site (eTLD+1), so one site sees the same noise for the whole
// session and two sites see different noise. Sites on the compatibility list
// (sites that need a stable fingerprint, for example online banking) get
// nothing. Self-owned; deleted in OnDestruct().
class FingerprintAgent : public content::RenderFrameObserver {
 public:
  static void Create(content::RenderFrame* render_frame);

  // A dedicated, shared or service worker's global scope was created (worker
  // thread, before the worker's script runs): injects the same script, seeded
  // with the site of the worker's own origin (`self.origin`). For a worker
  // started by a top-level page that is the page's site, so page and worker
  // agree; a worker started by a third-party frame gets the noise of its own
  // site, not of the page the frame is embedded in (the hook knows no frame).
  // Opaque origins (data: workers) and sites on a compatibility or user
  // exception list get nothing, like documents.
  static void OnWorkerContextCreated(v8::Local<v8::Context> context);

  FingerprintAgent(const FingerprintAgent&) = delete;
  FingerprintAgent& operator=(const FingerprintAgent&) = delete;

  // Seed for `site` under `session_key`; stable for the pair.
  static uint64_t SeedFor(const std::string& session_key,
                          const std::string& site);

 private:
  explicit FingerprintAgent(content::RenderFrame* render_frame);
  ~FingerprintAgent() override;

  // content::RenderFrameObserver:
  void DidCreateScriptContext(v8::Local<v8::Context> context,
                              int32_t world_id) override;
  void OnDestruct() override;

  // eTLD+1 of the top-level frame's origin (falls back to this frame's
  // origin, then empty). Empty means "do not protect".
  std::string TopLevelSite() const;
};

}  // namespace plainbrowser::fingerprint

#endif  // PLAINBROWSER_FINGERPRINT_RENDERER_FINGERPRINT_AGENT_H_
