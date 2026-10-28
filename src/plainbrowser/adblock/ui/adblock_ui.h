// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_ADBLOCK_UI_ADBLOCK_UI_H_
#define PLAINBROWSER_ADBLOCK_UI_ADBLOCK_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace plainbrowser::adblock {

inline constexpr char kAdblockHost[] = "adblock";

class AdblockUI;

// chrome://adblock, registered in chrome/browser/ui/webui/chrome_web_ui_configs.cc
// (patches/60-ui/0011).
class AdblockUIConfig : public content::DefaultWebUIConfig<AdblockUI> {
 public:
  AdblockUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme, kAdblockHost) {}
};

// The state of the ad blocker: the global switch, requests blocked since
// start-up, the engine process, the lists in use (downloaded or compiled in)
// and the last update check, with a "Check now" button. Page and script are
// adblock_page.{html,js}, embedded at build time (no grit resources).
class AdblockUI : public content::WebUIController {
 public:
  explicit AdblockUI(content::WebUI* web_ui);
  ~AdblockUI() override;

  AdblockUI(const AdblockUI&) = delete;
  AdblockUI& operator=(const AdblockUI&) = delete;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace plainbrowser::adblock

#endif  // PLAINBROWSER_ADBLOCK_UI_ADBLOCK_UI_H_
