// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_EXTENSIONS_UI_RECOMMENDED_EXTENSIONS_UI_H_
#define PLAINBROWSER_EXTENSIONS_UI_RECOMMENDED_EXTENSIONS_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace plainbrowser::extensions {

inline constexpr char kRecommendedExtensionsHost[] = "recommended-extensions";

class RecommendedExtensionsUI;

// chrome://recommended-extensions, registered in
// chrome/browser/ui/webui/chrome_web_ui_configs.cc (patches/20-hooks).
class RecommendedExtensionsUIConfig
    : public content::DefaultWebUIConfig<RecommendedExtensionsUI> {
 public:
  RecommendedExtensionsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           kRecommendedExtensionsHost) {}
};

// The curated extensions (browser/catalog.h) with an Install button each, or
// their state when installed; "Switch" for a copy installed from the Chrome
// Web Store; and, while one of the ad blockers runs next to the built-in one,
// an offer to turn the built-in one off (both would filter every request).
// Page and script are embedded at build time like chrome://adblock's.
class RecommendedExtensionsUI : public content::WebUIController {
 public:
  explicit RecommendedExtensionsUI(content::WebUI* web_ui);
  ~RecommendedExtensionsUI() override;

  RecommendedExtensionsUI(const RecommendedExtensionsUI&) = delete;
  RecommendedExtensionsUI& operator=(const RecommendedExtensionsUI&) = delete;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace plainbrowser::extensions

#endif  // PLAINBROWSER_EXTENSIONS_UI_RECOMMENDED_EXTENSIONS_UI_H_
