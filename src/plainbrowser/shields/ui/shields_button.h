// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_SHIELDS_UI_SHIELDS_BUTTON_H_
#define PLAINBROWSER_SHIELDS_UI_SHIELDS_BUTTON_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"

class BrowserWindowInterface;

namespace plainbrowser::shields {

// Toolbar button right of the omnibox (added in ToolbarView::Init,
// patches/60-ui). Shows whether Plain Browser's protections (ad blocking,
// tracking-parameter stripping, fingerprint protection) are on for the site in
// the active tab and opens ShieldsBubbleView to toggle them.
class ShieldsButton : public ToolbarButton,
                      public content::WebContentsObserver {
  METADATA_HEADER(ShieldsButton, ToolbarButton)

 public:
  explicit ShieldsButton(BrowserWindowInterface* browser);
  ~ShieldsButton() override;

  ShieldsButton(const ShieldsButton&) = delete;
  ShieldsButton& operator=(const ShieldsButton&) = delete;

  // Re-reads the active tab's URL and the pref; updates icon and tooltip.
  void UpdateState();

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

 private:
  void OnPressed();
  void OnActiveTabChanged(BrowserWindowInterface* browser);

  const raw_ptr<BrowserWindowInterface> browser_;
  base::CallbackListSubscription active_tab_subscription_;
};

}  // namespace plainbrowser::shields

#endif  // PLAINBROWSER_SHIELDS_UI_SHIELDS_BUTTON_H_
