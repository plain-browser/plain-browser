// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/adblock/renderer/cosmetic_filter_agent.h"

#include <algorithm>
#include <string>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/renderer/render_frame.h"
#include "plainbrowser/adblock/renderer/cosmetic_script.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "url/origin.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-script.h"

namespace plainbrowser::adblock {

namespace {

// Per message to the browser.
constexpr size_t kMaxGenericTokens = 2000;
// Per document: pages with more distinct names stop being looked at.
constexpr size_t kMaxTokensPerDocument = 50000;
// Longer names are not class names anyone writes rules for.
constexpr size_t kMaxTokenLength = 256;

constexpr char kReportFunction[] = "__plainbrowserCosmeticReport";

bool Usable(const std::string& token) {
  return !token.empty() && token.size() <= kMaxTokenLength;
}

// Class names in a class attribute value.
std::vector<std::string> SplitClasses(const std::string& value) {
  return base::SplitString(value, base::kWhitespaceASCII,
                           base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

// Runs `source` in `context`; errors are swallowed (a scriptlet must never
// break the page).
void RunInContext(v8::Local<v8::Context> context, const std::string& source) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::String> v8_source;
  v8::Local<v8::Script> script;
  v8::Local<v8::Value> unused;
  if (!v8::String::NewFromUtf8(isolate, source.data(),
                               v8::NewStringType::kNormal, source.size())
           .ToLocal(&v8_source) ||
      !v8::Script::Compile(context, v8_source).ToLocal(&script) ||
      !script->Run(context).ToLocal(&unused)) {
    DVLOG(1) << "Plain Browser scriptlets threw";
  }
}

std::vector<std::string> ToStrings(v8::Isolate* isolate,
                                   v8::Local<v8::Context> context,
                                   v8::Local<v8::Value> value) {
  std::vector<std::string> out;
  if (!value->IsArray()) {
    return out;
  }
  v8::Local<v8::Array> array = value.As<v8::Array>();
  const uint32_t n = std::min<uint32_t>(
      array->Length(), static_cast<uint32_t>(kMaxGenericTokens));
  for (uint32_t i = 0; i < n; ++i) {
    v8::Local<v8::Value> item;
    if (!array->Get(context, i).ToLocal(&item) || !item->IsString()) {
      continue;
    }
    v8::String::Utf8Value utf8(isolate, item);
    if (*utf8 && static_cast<size_t>(utf8.length()) <= kMaxTokenLength) {
      out.emplace_back(*utf8, utf8.length());
    }
  }
  return out;
}

// __plainbrowserCosmeticReport(classes, ids) in our isolated world.
void ReportNames(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  blink::WebLocalFrame* web_frame =
      blink::WebLocalFrame::FrameForContext(context);
  content::RenderFrame* render_frame =
      web_frame ? content::RenderFrame::FromWebFrame(web_frame) : nullptr;
  CosmeticFilterAgent* agent =
      render_frame ? CosmeticFilterAgent::Get(render_frame) : nullptr;
  if (!agent || info.Length() < 2) {
    return;
  }
  agent->OnNamesReported(ToStrings(isolate, context, info[0]),
                         ToStrings(isolate, context, info[1]));
}

}  // namespace

// static
void CosmeticFilterAgent::Create(content::RenderFrame* render_frame) {
  new CosmeticFilterAgent(render_frame);  // Self-owned.
}

CosmeticFilterAgent::CosmeticFilterAgent(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<CosmeticFilterAgent>(render_frame) {}

CosmeticFilterAgent::~CosmeticFilterAgent() = default;

mojom::AdblockFilter* CosmeticFilterAgent::Filter() {
  if (!filter_.is_bound()) {
    blink::Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        filter_.BindNewPipeAndPassReceiver());
  }
  return filter_.get();
}

GURL CosmeticFilterAgent::TopLevelUrl() const {
  // The per-site exception list is keyed by the top-level site.
  return url::Origin(render_frame()->GetWebFrame()->Top()->GetSecurityOrigin())
      .GetURL();
}

void CosmeticFilterAgent::DidCreateScriptContext(
    v8::Local<v8::Context> context,
    int32_t world_id) {
  if (world_id == content::ISOLATED_WORLD_ID_GLOBAL) {
    RunScriptlets(context);
  } else if (world_id == kCosmeticWorldId) {
    InstallReportFunction(context);
  }
}

void CosmeticFilterAgent::RunScriptlets(v8::Local<v8::Context> context) {
  const GURL url(render_frame()->GetWebFrame()->GetDocument().Url());
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return;
  }
  std::string script;
  // Sync on purpose: the page's own scripts must not run first. One call per
  // document; the browser answers from its engine without blocking.
  if (!Filter()->GetScriptlets(url, TopLevelUrl(), &script) || script.empty()) {
    return;
  }
  // Wrapped, so the helpers adblock-rust puts in front of the calls do not
  // become globals the page can see or replace.
  RunInContext(context, "(function () {\n" + script + "\n})();");
}

void CosmeticFilterAgent::InstallReportFunction(
    v8::Local<v8::Context> context) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Function> function;
  if (!v8::Function::New(context, &ReportNames).ToLocal(&function)) {
    return;
  }
  std::ignore = context->Global()->Set(
      context, v8::String::NewFromUtf8Literal(isolate, kReportFunction),
      function);
}

void CosmeticFilterAgent::DidCreateDocumentElement() {
  // A new document: drop replies of the previous one.
  weak_factory_.InvalidateWeakPtrs();
  generic_ = false;
  resources_known_ = false;
  scan_pending_ = false;
  sent_classes_.clear();
  sent_ids_.clear();

  document_url_ = GURL(render_frame()->GetWebFrame()->GetDocument().Url());
  if (!document_url_.SchemeIsHTTPOrHTTPS()) {
    return;
  }
  top_level_url_ = TopLevelUrl();
  Filter()->GetCosmeticCss(
      document_url_, top_level_url_,
      base::BindOnce(&CosmeticFilterAgent::OnCss, weak_factory_.GetWeakPtr(),
                     document_url_));
}

void CosmeticFilterAgent::OnCss(
    const GURL& document_url,
    const std::string& css,
    bool generic_hide,
    const std::vector<std::string>& procedural_actions,
    const std::vector<std::string>& procedural_selectors) {
  InsertCss(document_url, css);
  resources_known_ = true;
  generic_ = !generic_hide;
  if (generic_ || !procedural_actions.empty() ||
      !procedural_selectors.empty()) {
    InjectCosmeticScript(procedural_actions, procedural_selectors);
  }
  if (scan_pending_) {
    scan_pending_ = false;
    Scan();
  }
}

void CosmeticFilterAgent::InjectCosmeticScript(
    const std::vector<std::string>& procedural_actions,
    const std::vector<std::string>& procedural_selectors) {
  base::ListValue actions;
  for (const std::string& a : procedural_actions) {
    actions.Append(a);
  }
  base::ListValue selectors;
  for (const std::string& s : procedural_selectors) {
    selectors.Append(s);
  }
  std::optional<std::string> json =
      base::WriteJson(base::DictValue()
                          .Set("generic", generic_)
                          .Set("actions", std::move(actions))
                          .Set("selectors", std::move(selectors)));
  if (!json) {
    return;
  }
  std::string source(kCosmeticScript);
  base::ReplaceFirstSubstringAfterOffset(&source, 0, "__PB_COSMETIC_CONFIG__",
                                         base::GetQuotedJSONString(*json));
  render_frame()->GetWebFrame()->ExecuteScriptInIsolatedWorld(
      kCosmeticWorldId,
      blink::WebScriptSource(blink::WebString::FromUtf8(source)),
      blink::BackForwardCacheAware::kAllow);
}

void CosmeticFilterAgent::DidDispatchDOMContentLoadedEvent() {
  Scan();
}

void CosmeticFilterAgent::DidFinishLoad() {
  Scan();
}

void CosmeticFilterAgent::Scan() {
  if (!document_url_.SchemeIsHTTPOrHTTPS()) {
    return;
  }
  if (!resources_known_) {
    scan_pending_ = true;  // OnCss() scans when the answer arrives.
    return;
  }
  if (!generic_) {
    return;
  }
  blink::WebDocument document = render_frame()->GetWebFrame()->GetDocument();
  if (GURL(document.Url()) != document_url_) {
    return;
  }
  std::vector<std::string> classes;
  std::vector<std::string> ids;
  for (const blink::WebElement& element : document.QuerySelectorAll(
           blink::WebString::FromUtf8("[class],[id]"))) {
    std::string id = element.GetIdAttribute().Utf8();
    if (Usable(id)) {
      ids.push_back(std::move(id));
    }
    for (std::string& name : SplitClasses(
             element.GetAttribute(blink::WebString::FromUtf8("class"))
                 .Utf8())) {
      if (Usable(name)) {
        classes.push_back(std::move(name));
      }
    }
  }
  SendNew(std::move(classes), std::move(ids));
}

void CosmeticFilterAgent::OnNamesReported(std::vector<std::string> classes,
                                          std::vector<std::string> ids) {
  if (generic_) {
    SendNew(std::move(classes), std::move(ids));
  }
}

void CosmeticFilterAgent::SendNew(std::vector<std::string> classes,
                                  std::vector<std::string> ids) {
  std::vector<std::string> new_classes;
  std::vector<std::string> new_ids;
  auto flush = [&] {
    if (new_classes.empty() && new_ids.empty()) {
      return;
    }
    Filter()->GetGenericCosmeticCss(
        document_url_, top_level_url_, std::move(new_classes),
        std::move(new_ids),
        base::BindOnce(&CosmeticFilterAgent::OnGenericCss,
                       weak_factory_.GetWeakPtr(), document_url_));
    new_classes.clear();
    new_ids.clear();
  };
  auto room = [&] {
    return sent_classes_.size() + sent_ids_.size() < kMaxTokensPerDocument;
  };
  for (std::string& id : ids) {
    if (!room()) {
      break;
    }
    if (Usable(id) && sent_ids_.insert(id).second) {
      new_ids.push_back(std::move(id));
      if (new_ids.size() + new_classes.size() >= kMaxGenericTokens) {
        flush();
      }
    }
  }
  for (std::string& name : classes) {
    if (!room()) {
      break;
    }
    if (Usable(name) && sent_classes_.insert(name).second) {
      new_classes.push_back(std::move(name));
      if (new_ids.size() + new_classes.size() >= kMaxGenericTokens) {
        flush();
      }
    }
  }
  flush();
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
