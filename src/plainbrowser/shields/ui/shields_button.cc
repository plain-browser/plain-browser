// Copyright Plain Browser contributors. MPL-2.0.
#include "plainbrowser/shields/ui/shields_button.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_prefs/user_prefs.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "plainbrowser/shields/browser/blocked_requests_counter.h"
#include "plainbrowser/shields/browser/shields_prefs.h"
#include "plainbrowser/shields/common/site_util.h"
#include "plainbrowser/shields/ui/shields_bubble_view.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"

namespace plainbrowser::shields {

namespace {

// Counts above this show as "99+".
constexpr int kMaxBadgeCount = 99;
// Badge in the bottom-right corner of the icon, in DIP.
constexpr int kBadgeHeight = 11;
constexpr int kBadgePadding = 2;

content::WebContents* ActiveContents(BrowserWindowInterface* browser) {
  tabs::TabInterface* tab = browser->GetActiveTabInterface();
  return tab ? tab->GetContents() : nullptr;
}

// The toolbar icon with a rounded badge holding the blocked count, drawn the
// way extension action badges are: a 1 DIP transparent gap separates the badge
// from the icon under it.
class BadgedIconSource : public gfx::CanvasImageSource {
 public:
  BadgedIconSource(const gfx::VectorIcon& icon,
                   int size,
                   SkColor icon_color,
                   std::u16string text,
                   SkColor badge_color,
                   SkColor text_color)
      : gfx::CanvasImageSource(gfx::Size(size, size)),
        icon_(icon),
        icon_color_(icon_color),
        text_(std::move(text)),
        badge_color_(badge_color),
        text_color_(text_color) {}

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    gfx::PaintVectorIcon(canvas, *icon_, size().width(), icon_color_);

    const gfx::FontList font = gfx::FontList()
                                   .DeriveWithWeight(gfx::Font::Weight::BOLD)
                                   .DeriveWithHeightUpperBound(kBadgeHeight);
    const int width =
        std::clamp(gfx::GetStringWidth(text_, font) + 2 * kBadgePadding,
                   kBadgeHeight, size().width());
    const gfx::Rect badge(size().width() - width,
                          size().height() - kBadgeHeight, width, kBadgeHeight);

    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);
    cc::PaintFlags gap_flags = flags;
    gap_flags.setBlendMode(SkBlendMode::kClear);
    gfx::Rect gap(badge);
    gap.Inset(-1);
    canvas->DrawRoundRect(gap, gap.height() / 2, gap_flags);
    flags.setColor(badge_color_);
    canvas->DrawRoundRect(badge, badge.height() / 2, flags);
    canvas->DrawStringRectWithFlags(
        text_, font, text_color_, badge,
        gfx::Canvas::TEXT_ALIGN_CENTER | gfx::Canvas::NO_SUBPIXEL_RENDERING);
  }

 private:
  const raw_ref<const gfx::VectorIcon> icon_;
  const SkColor icon_color_;
  const std::u16string text_;
  const SkColor badge_color_;
  const SkColor text_color_;
};

}  // namespace

ShieldsButton::ShieldsButton(BrowserWindowInterface* browser)
    : ToolbarButton(base::BindRepeating(&ShieldsButton::OnPressed,
                                        base::Unretained(this))),
      browser_(browser) {
  SetVectorIcon(vector_icons::kShieldIcon);
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  const std::u16string name =
      l10n_util::GetStringUTF16(IDS_PLAINBROWSER_SHIELDS_BUTTON_NAME);
  GetViewAccessibility().SetName(name);
  GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kDialog);
  SetTooltipText(name);
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
  if (contents != web_contents()) {
    Observe(contents);
    count_subscription_ = {};
    if (contents) {
      BlockedRequestsCounter::CreateForWebContents(contents);
      count_subscription_ =
          BlockedRequestsCounter::FromWebContents(contents)
              ->AddCountChangedCallback(
                  base::BindRepeating(&ShieldsButton::OnBlockedCountChanged,
                                      base::Unretained(this)));
    }
  }
  const std::string site =
      contents ? SiteFor(contents->GetLastCommittedURL()) : std::string();
  applicable_ = !site.empty();
  excepted_ = applicable_ &&
              IsSiteExcepted(
                  user_prefs::UserPrefs::Get(contents->GetBrowserContext()),
                  site);

  SetTooltipText(
      applicable_
          ? l10n_util::GetStringFUTF16(excepted_
                                           ? IDS_PLAINBROWSER_SHIELDS_TOOLTIP_OFF
                                           : IDS_PLAINBROWSER_SHIELDS_TOOLTIP_ON,
                                       base::UTF8ToUTF16(site))
          : l10n_util::GetStringUTF16(IDS_PLAINBROWSER_SHIELDS_BUTTON_NAME));
  OnBlockedCountChanged(BlockedRequestsCounter::CountFor(contents));
  // Repaints with the badge too (UpdateIcon() below) once the button is in a
  // widget.
  SetVectorIcon(excepted_ ? vector_icons::kGppMaybeIcon
                          : vector_icons::kShieldIcon);
}

void ShieldsButton::OnBlockedCountChanged(int count) {
  const int shown = applicable_ && !excepted_ ? count : 0;
  if (applicable_ && !excepted_) {
    GetViewAccessibility().SetDescription(l10n_util::GetPluralStringFUTF16(
        IDS_PLAINBROWSER_SHIELDS_BLOCKED_COUNT, shown));
  } else {
    GetViewAccessibility().RemoveDescription();
  }
  if (shown == blocked_count_) {
    return;
  }
  blocked_count_ = shown;
  if (GetColorProvider()) {
    UpdateIcon();
  }
}

void ShieldsButton::UpdateIcon() {
  const ui::ColorProvider* colors = GetColorProvider();
  if (blocked_count_ <= 0 || !colors) {
    ToolbarButton::UpdateIcon();
    return;
  }
  const gfx::VectorIcon& icon =
      excepted_ ? vector_icons::kGppMaybeIcon : vector_icons::kShieldIcon;
  const std::u16string text =
      blocked_count_ > kMaxBadgeCount
          ? base::NumberToString16(kMaxBadgeCount) + u"+"
          : base::NumberToString16(blocked_count_);
  const SkColor badge_color = colors->GetColor(ui::kColorBadgeBackground);
  const SkColor text_color = colors->GetColor(ui::kColorBadgeForeground);
  for (ButtonState state :
       {ButtonState::STATE_NORMAL, ButtonState::STATE_HOVERED,
        ButtonState::STATE_PRESSED, ButtonState::STATE_DISABLED}) {
    SetImageModel(state,
                  ui::ImageModel::FromImageSkia(
                      gfx::CanvasImageSource::MakeImageSkia<BadgedIconSource>(
                          icon, GetIconSize(), GetForegroundColor(state), text,
                          badge_color, text_color)));
  }
}

void ShieldsButton::OnPressed() {
  ShieldsBubbleView::Show(
      this, ActiveContents(browser_),
      base::BindRepeating(&ShieldsButton::UpdateState, base::Unretained(this)),
      base::BindRepeating(&ShieldsButton::OpenSettings,
                          base::Unretained(this)));
}

void ShieldsButton::OpenSettings() {
  // The exception list is a section of the Privacy and security page
  // (patches/60-ui/0009).
  chrome::ShowSettingsSubPage(browser_, chrome::kPrivacySubPage);
}

BEGIN_METADATA(ShieldsButton)
END_METADATA

}  // namespace plainbrowser::shields
