// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/fingerprint/renderer/fingerprint_agent.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/renderer/render_frame.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "plainbrowser/fingerprint/common/features.h"
#include "plainbrowser/fingerprint/common/switches.h"
#include "plainbrowser/fingerprint/renderer/fingerprint_exceptions.h"
#include "plainbrowser/fingerprint/renderer/fingerprint_script.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-script.h"

namespace plainbrowser::fingerprint {

namespace {

std::string SiteOf(const blink::WebSecurityOrigin& origin) {
  if (origin.IsNull() || origin.IsOpaque()) {
    return std::string();
  }
  const url::Origin o = origin;
  if (o.scheme() != "https" && o.scheme() != "http") {
    return std::string();
  }
  const std::string site =
      net::registry_controlled_domains::GetDomainAndRegistry(
          o.host(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  // IP literals and single-label hosts have no registry: use the host itself.
  return site.empty() ? o.host() : site;
}

}  // namespace

// static
void FingerprintAgent::Create(content::RenderFrame* render_frame) {
  if (!base::FeatureList::IsEnabled(kPlainBrowserFingerprintProtection)) {
    return;
  }
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kFingerprintSessionKey)) {
    return;  // Browser side disabled (feature off there, or not Plain Browser).
  }
  new FingerprintAgent(render_frame);  // Self-owned.
}

// static
uint64_t FingerprintAgent::SeedFor(const std::string& session_key,
                                   const std::string& site) {
  return base::FastHash(session_key + "|" + site);
}

FingerprintAgent::FingerprintAgent(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {}

FingerprintAgent::~FingerprintAgent() = default;

std::string FingerprintAgent::TopLevelSite() const {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  if (!frame) {
    return std::string();
  }
  // Keyed by the top-level site, like a storage partition: an embedded
  // tracker frame sees the noise of the page it is embedded in, not its own.
  std::string site = SiteOf(frame->Top()->GetSecurityOrigin());
  if (site.empty()) {
    site = SiteOf(frame->GetDocument().GetSecurityOrigin());
  }
  return site;
}

void FingerprintAgent::DidCreateScriptContext(v8::Local<v8::Context> context,
                                              int32_t world_id) {
  if (world_id != content::ISOLATED_WORLD_ID_GLOBAL) {
    return;  // Main world only: extensions and our own worlds are left alone.
  }
  const std::string site = TopLevelSite();
  if (site.empty() || IsExceptedSite(site)) {
    return;
  }
  const std::string session_key =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kFingerprintSessionKey);
  const uint64_t seed = SeedFor(session_key, site);
  std::string script(kFingerprintScript);
  base::ReplaceSubstringsAfterOffset(
      &script, 0, "__PB_SEED_HI__",
      base::NumberToString(static_cast<uint32_t>(seed >> 32)));
  base::ReplaceSubstringsAfterOffset(
      &script, 0, "__PB_SEED_LO__",
      base::NumberToString(static_cast<uint32_t>(seed & 0xffffffffu)));

  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::String> source;
  if (!v8::String::NewFromUtf8(isolate, script.data(),
                               v8::NewStringType::kNormal, script.size())
           .ToLocal(&source)) {
    return;
  }
  v8::Local<v8::Script> compiled;
  if (!v8::Script::Compile(context, source).ToLocal(&compiled)) {
    LOG(ERROR) << "fingerprint script failed to compile";
    return;
  }
  v8::Local<v8::Value> unused;
  if (!compiled->Run(context).ToLocal(&unused)) {
    DLOG(ERROR) << "fingerprint script threw";
  }
}

void FingerprintAgent::OnDestruct() {
  delete this;
}

}  // namespace plainbrowser::fingerprint
