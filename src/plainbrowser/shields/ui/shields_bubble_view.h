// Copyright Plain Browser contributors. MPL-2.0.
#ifndef PLAINBROWSER_SHIELDS_UI_SHIELDS_BUBBLE_VIEW_H_
#define PLAINBROWSER_SHIELDS_UI_SHIELDS_BUBBLE_VIEW_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class PrefService;

namespace content {
class WebContents;
}

namespace views {
class Label;
class ToggleButton;
}  // namespace views

namespace plainbrowser::shields {

// Bubble under the toolbar button: the current site, a toggle "Protections on
// for this site" (edits prefs::kShieldsExceptions and reloads the tab) and the
// number of requests blocked on the page.
//
// A BubbleDialogDelegate, not a BubbleDialogDelegateView (deprecated, private
// constructor). The contents are a separate View set with SetContentsView();
// Show() owns the delegate and the widget until the bubble is closed.
class ShieldsBubbleView : public views::BubbleDialogDelegate {
 public:
  // Closes an open bubble (second click on the button) or opens one anchored
  // to `anchor` for `web_contents` (may be null: no tab). `on_changed` runs
  // after the toggle changed the pref.
  static void Show(views::View* anchor,
                   content::WebContents* web_contents,
                   base::RepeatingClosure on_changed);
  static bool IsShowing();

  // Use Show(). Public for std::make_unique.
  ShieldsBubbleView(views::View* anchor,
                    content::WebContents* web_contents,
                    base::RepeatingClosure on_changed);
  ~ShieldsBubbleView() override;

 private:
  void OnToggled();
  void UpdateStatus();

  base::WeakPtr<content::WebContents> web_contents_;
  raw_ptr<PrefService> prefs_ = nullptr;
  const std::string site_;
  const base::RepeatingClosure on_changed_;
  bool reloaded_ = false;

  raw_ptr<views::ToggleButton> toggle_ = nullptr;
  raw_ptr<views::Label> status_ = nullptr;
};

}  // namespace plainbrowser::shields

#endif  // PLAINBROWSER_SHIELDS_UI_SHIELDS_BUBBLE_VIEW_H_
