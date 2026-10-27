// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/shields/ui/shields_bubble_view.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "plainbrowser/shields/browser/blocked_requests_counter.h"
#include "plainbrowser/shields/browser/shields_prefs.h"
#include "plainbrowser/shields/common/site_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace plainbrowser::shields {

namespace {

// The open bubble, if any. Raw pointers: a static unique_ptr would need an
// exit-time destructor. Both are released in OnBubbleClosed().
ShieldsBubbleView* g_delegate = nullptr;
views::Widget* g_widget = nullptr;
// When the bubble last closed because it lost activation. A press on the
// toolbar button deactivates the bubble before the button is notified, so that
// press finds no bubble: it must close, not reopen (views::MenuButtonController
// uses the same minimum interval between clicks).
base::TimeTicks g_closed_by_deactivation;

constexpr int kBubbleWidth = 320;
constexpr base::TimeDelta kReopenAfterDeactivation = base::Milliseconds(200);

// Runs synchronously when the widget closes. The delegate must outlive the
// widget and the callback owns the widget, so both are deleted after the
// close has finished.
void OnBubbleClosed(views::Widget::ClosedReason reason) {
  if (reason == views::Widget::ClosedReason::kLostFocus) {
    g_closed_by_deactivation = base::TimeTicks::Now();
  }
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  runner->DeleteSoon(FROM_HERE, g_widget);
  runner->DeleteSoon(FROM_HERE, g_delegate);
  g_widget = nullptr;
  g_delegate = nullptr;
}

}  // namespace

// static
void ShieldsBubbleView::Show(views::View* anchor,
                             content::WebContents* web_contents,
                             base::RepeatingClosure on_changed) {
  if (g_widget) {
    g_widget->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
    return;
  }
  if (!g_closed_by_deactivation.is_null() &&
      base::TimeTicks::Now() - g_closed_by_deactivation <
          kReopenAfterDeactivation) {
    // This press is what deactivated (and closed) the bubble.
    g_closed_by_deactivation = base::TimeTicks();
    return;
  }
  g_delegate = new ShieldsBubbleView(anchor, web_contents,
                                     std::move(on_changed));
  g_widget = views::BubbleDialogDelegate::CreateBubble(
                 g_delegate, base::BindOnce(&OnBubbleClosed))
                 .release();
  g_widget->Show();
}

// static
bool ShieldsBubbleView::IsShowing() {
  return g_widget != nullptr;
}

ShieldsBubbleView::ShieldsBubbleView(views::View* anchor,
                                     content::WebContents* web_contents,
                                     base::RepeatingClosure on_changed)
    : views::BubbleDialogDelegate(anchor, views::BubbleBorder::TOP_RIGHT),
      web_contents_(web_contents ? web_contents->GetWeakPtr()
                                 : base::WeakPtr<content::WebContents>()),
      prefs_(web_contents
                 ? user_prefs::UserPrefs::Get(web_contents->GetBrowserContext())
                 : nullptr),
      site_(web_contents ? SiteFor(web_contents->GetLastCommittedURL())
                         : std::string()),
      on_changed_(std::move(on_changed)) {
  SetTitle(u"Plain Browser");
  SetShowCloseButton(true);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_fixed_width(kBubbleWidth);

  auto contents = std::make_unique<views::View>();
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  contents->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  const bool applicable = !site_.empty() && prefs_;
  const bool excepted = applicable && IsSiteExcepted(prefs_, site_);

  // Row 1: the site.
  auto* site_label = contents->AddChildView(std::make_unique<views::Label>(
      applicable ? base::UTF8ToUTF16(site_) : u"This page",
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY));
  site_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Row 2: label + toggle.
  auto* row = contents->AddChildView(std::make_unique<views::View>());
  auto* row_layout = row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  row_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  auto* toggle_label = row->AddChildView(std::make_unique<views::Label>(
      u"Protections on for this site", views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_PRIMARY));
  toggle_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  row_layout->SetFlexForView(toggle_label, 1);
  toggle_ = row->AddChildView(std::make_unique<views::ToggleButton>(
      base::BindRepeating(&ShieldsBubbleView::OnToggled,
                          base::Unretained(this))));
  toggle_->SetIsOn(applicable && !excepted);
  toggle_->SetEnabled(applicable);
  toggle_->GetViewAccessibility().SetName(u"Protections on for this site");

  // Row 3: status line.
  status_ = contents->AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY));
  status_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  status_->SetMultiLine(true);
  SetContentsView(std::move(contents));
  UpdateStatus();
}

ShieldsBubbleView::~ShieldsBubbleView() = default;

void ShieldsBubbleView::UpdateStatus() {
  std::u16string text;
  if (site_.empty() || !prefs_) {
    text = u"Protections do not apply to this page.";
  } else if (reloaded_) {
    text = toggle_->GetIsOn()
               ? u"Protections on. The page is reloading."
               : u"Protections off for this site. The page is reloading.";
  } else if (IsSiteExcepted(prefs_, site_)) {
    text =
        u"Ad blocking, tracking-parameter stripping and fingerprint "
        u"protection are off for this site.";
  } else {
    const int blocked =
        BlockedRequestsCounter::CountFor(web_contents_.get());
    text = base::NumberToString16(blocked) +
           (blocked == 1 ? u" request blocked on this page."
                         : u" requests blocked on this page.");
  }
  status_->SetText(text);
}

void ShieldsBubbleView::OnToggled() {
  if (site_.empty() || !prefs_) {
    return;
  }
  SetSiteExcepted(prefs_, site_, /*excepted=*/!toggle_->GetIsOn());
  if (web_contents_) {
    // A change applies to new requests only; reload so the page is loaded
    // with the new state from the first byte.
    web_contents_->GetController().Reload(content::ReloadType::NORMAL,
                                          /*check_for_repost=*/false);
  }
  reloaded_ = true;
  UpdateStatus();
  if (on_changed_) {
    on_changed_.Run();
  }
}

}  // namespace plainbrowser::shields
