// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/shields/ui/shields_button.h"

#include <string>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_prefs/user_prefs.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "plainbrowser/shields/browser/shields_prefs.h"
#include "plainbrowser/shields/common/site_util.h"
#include "plainbrowser/shields/ui/shields_bubble_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"

namespace plainbrowser::shields {

namespace {

content::WebContents* ActiveContents(BrowserWindowInterface* browser) {
  tabs::TabInterface* tab = browser->GetActiveTabInterface();
  return tab ? tab->GetContents() : nullptr;
}

}  // namespace

ShieldsButton::ShieldsButton(BrowserWindowInterface* browser)
    : ToolbarButton(base::BindRepeating(&ShieldsButton::OnPressed,
                                        base::Unretained(this))),
      browser_(browser) {
  SetVectorIcon(vector_icons::kShieldIcon);
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  GetViewAccessibility().SetName(u"Plain Browser protections");
  GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kDialog);
  SetTooltipText(u"Plain Browser protections");
  active_tab_subscription_ = browser_->RegisterActiveTabDidChange(
      base::BindRepeating(&ShieldsButton::OnActiveTabChanged,
                          base::Unretained(this)));
  UpdateState();
}

ShieldsButton::~ShieldsButton() = default;

void ShieldsButton::OnActiveTabChanged(BrowserWindowInterface* browser) {
  UpdateState();
}

void ShieldsButton::PrimaryPageChanged(content::Page& page) {
  UpdateState();
}

void ShieldsButton::UpdateState() {
  content::WebContents* contents = ActiveContents(browser_);
  Observe(contents);
  const std::string site =
      contents ? SiteFor(contents->GetLastCommittedURL()) : std::string();
  bool excepted = false;
  if (!site.empty()) {
    excepted = IsSiteExcepted(
        user_prefs::UserPrefs::Get(contents->GetBrowserContext()), site);
  }
  SetVectorIcon(excepted ? vector_icons::kGppMaybeIcon
                         : vector_icons::kShieldIcon);
  std::u16string tooltip = u"Plain Browser protections";
  if (!site.empty()) {
    tooltip += excepted ? u": off for " : u": on for ";
    tooltip += base::UTF8ToUTF16(site);
  }
  SetTooltipText(tooltip);
}

void ShieldsButton::OnPressed() {
  ShieldsBubbleView::Show(
      this, ActiveContents(browser_),
      base::BindRepeating(&ShieldsButton::UpdateState, base::Unretained(this)));
}

BEGIN_METADATA(ShieldsButton)
END_METADATA

}  // namespace plainbrowser::shields
