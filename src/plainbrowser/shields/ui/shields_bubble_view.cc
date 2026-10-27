// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/shields/ui/shields_bubble_view.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "plainbrowser/shields/browser/blocked_requests_counter.h"
#include "plainbrowser/shields/browser/shields_prefs.h"
#include "plainbrowser/shields/common/site_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
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
                             base::RepeatingClosure on_changed,
                             base::RepeatingClosure on_manage) {
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
                                     std::move(on_changed),
                                     std::move(on_manage));
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
                                     base::RepeatingClosure on_changed,
                                     base::RepeatingClosure on_manage)
    : views::BubbleDialogDelegate(anchor, views::BubbleBorder::TOP_RIGHT),
      web_contents_(web_contents ? web_contents->GetWeakPtr()
                                 : base::WeakPtr<content::WebContents>()),
      prefs_(web_contents
                 ? user_prefs::UserPrefs::Get(web_contents->GetBrowserContext())
                 : nullptr),
      site_(web_contents ? SiteFor(web_contents->GetLastCommittedURL())
                         : std::string()),
      on_changed_(std::move(on_changed)),
      on_manage_(std::move(on_manage)) {
  SetTitle(u"Plain Browser");  // Product name, not translated.
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
      applicable ? base::UTF8ToUTF16(site_)
                 : l10n_util::GetStringUTF16(IDS_PLAINBROWSER_SHIELDS_THIS_PAGE),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY));
  site_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Row 2: label + toggle.
  auto* row = contents->AddChildView(std::make_unique<views::View>());
  auto* row_layout = row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  row_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  const std::u16string toggle_text =
      l10n_util::GetStringUTF16(IDS_PLAINBROWSER_SHIELDS_TOGGLE);
  auto* toggle_label = row->AddChildView(std::make_unique<views::Label>(
      toggle_text, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_PRIMARY));
  toggle_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  row_layout->SetFlexForView(toggle_label, 1);
  toggle_ = row->AddChildView(std::make_unique<views::ToggleButton>(
      base::BindRepeating(&ShieldsBubbleView::OnToggled,
                          base::Unretained(this))));
  toggle_->SetIsOn(applicable && !excepted);
  toggle_->SetEnabled(applicable);
  toggle_->GetViewAccessibility().SetName(toggle_text);

  // Row 3: status line.
  status_ = contents->AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY));
  status_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  status_->SetMultiLine(true);

  // Row 4: link to the full list in Settings.
  if (on_manage_) {
    auto* manage = contents->AddChildView(std::make_unique<views::Link>(
        l10n_util::GetStringUTF16(IDS_PLAINBROWSER_SHIELDS_MANAGE)));
    manage->SetCallback(base::BindRepeating(
        &ShieldsBubbleView::OnManageClicked, base::Unretained(this)));
    manage->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }
  SetContentsView(std::move(contents));

  // Keep the count current while the page is still loading.
  if (applicable && web_contents) {
    BlockedRequestsCounter::CreateForWebContents(web_contents);
    count_subscription_ =
        BlockedRequestsCounter::FromWebContents(web_contents)
            ->AddCountChangedCallback(base::IgnoreArgs<int>(base::BindRepeating(
                &ShieldsBubbleView::UpdateStatus, base::Unretained(this))));
    // The widget, and with it status_, is deleted before this delegate
    // (OnBubbleClosed): stop the updates when the window closes.
    RegisterWindowClosingCallback(base::BindOnce(
        [](ShieldsBubbleView* view) { view->count_subscription_ = {}; },
        base::Unretained(this)));
  }
  UpdateStatus();
}

ShieldsBubbleView::~ShieldsBubbleView() = default;

void ShieldsBubbleView::UpdateStatus() {
  int id;
  if (site_.empty() || !prefs_) {
    id = IDS_PLAINBROWSER_SHIELDS_NOT_APPLICABLE;
  } else if (reloaded_) {
    id = toggle_->GetIsOn() ? IDS_PLAINBROWSER_SHIELDS_RELOADING_ON
                            : IDS_PLAINBROWSER_SHIELDS_RELOADING_OFF;
  } else if (IsSiteExcepted(prefs_, site_)) {
    id = IDS_PLAINBROWSER_SHIELDS_OFF_STATUS;
  } else {
    status_->SetText(l10n_util::GetPluralStringFUTF16(
        IDS_PLAINBROWSER_SHIELDS_BLOCKED_COUNT,
        BlockedRequestsCounter::CountFor(web_contents_.get())));
    return;
  }
  status_->SetText(l10n_util::GetStringUTF16(id));
}

void ShieldsBubbleView::OnManageClicked() {
  // Copy first: closing the widget deletes this delegate (posted, but the
  // callback must not depend on that).
  base::RepeatingClosure on_manage = on_manage_;
  if (GetWidget()) {
    GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
  on_manage.Run();
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
