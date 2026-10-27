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
// the active tab, with a badge counting the requests blocked on the page, and
// opens ShieldsBubbleView to toggle them.
class ShieldsButton : public ToolbarButton,
                      public content::WebContentsObserver {
  METADATA_HEADER(ShieldsButton, ToolbarButton)

 public:
  explicit ShieldsButton(BrowserWindowInterface* browser);
  ~ShieldsButton() override;

  ShieldsButton(const ShieldsButton&) = delete;
  ShieldsButton& operator=(const ShieldsButton&) = delete;

  // Re-reads the active tab's URL, the pref and the blocked count; updates
  // icon, badge, tooltip and accessible description.
  void UpdateState();

  // ToolbarButton: draws the badge over the icon when requests were blocked.
  void UpdateIcon() override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

 private:
  void OnPressed();
  void OnActiveTabChanged(BrowserWindowInterface* browser);
  void OnBlockedCountChanged(int count);
  void OpenSettings();

  const raw_ptr<BrowserWindowInterface> browser_;
  base::CallbackListSubscription active_tab_subscription_;
  base::CallbackListSubscription count_subscription_;

  // State of the active tab, as last read by UpdateState().
  bool applicable_ = false;
  bool excepted_ = false;
  int blocked_count_ = 0;
};

}  // namespace plainbrowser::shields

#endif  // PLAINBROWSER_SHIELDS_UI_SHIELDS_BUTTON_H_
