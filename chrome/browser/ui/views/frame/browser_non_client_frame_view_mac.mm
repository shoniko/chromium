// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_mac.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_view_layout.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/hit_test.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"

namespace {

constexpr int kTabstripTopInset = 8;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, public:

BrowserNonClientFrameViewMac::BrowserNonClientFrameViewMac(
    BrowserFrame* frame,
    BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view) {
  pref_registrar_.Init(browser_view->GetProfile()->GetPrefs());
  pref_registrar_.Add(
      prefs::kShowFullscreenToolbar,
      base::BindRepeating(&BrowserNonClientFrameViewMac::UpdateFullscreenTopUI,
                          base::Unretained(this), false));
}

BrowserNonClientFrameViewMac::~BrowserNonClientFrameViewMac() {
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, BrowserNonClientFrameView implementation:

bool BrowserNonClientFrameViewMac::CaptionButtonsOnLeadingEdge() const {
  return true;
}

gfx::Rect BrowserNonClientFrameViewMac::GetBoundsForTabStrip(
    views::View* tabstrip) const {
  DCHECK(tabstrip);

  gfx::Rect bounds = gfx::Rect(0, kTabstripTopInset, width(),
                               tabstrip->GetPreferredSize().height());
  bounds.Inset(GetTabStripLeftInset(), 0, GetAfterTabstripItemWidth() + 4, 0);
  return bounds;
}

int BrowserNonClientFrameViewMac::GetTopInset(bool restored) const {
  return browser_view()->IsTabStripVisible() ? kTabstripTopInset : 0;
}

int BrowserNonClientFrameViewMac::GetAfterTabstripItemWidth() const {
  int item_width;
  views::View* profile_switcher_button = GetProfileSwitcherButton();
  if (profile_indicator_icon() && browser_view()->IsTabStripVisible())
    item_width = profile_indicator_icon()->width();
  else if (profile_switcher_button)
    item_width = profile_switcher_button->GetPreferredSize().width();
  else
    return 0;
  return item_width + GetAvatarIconPadding();
}

int BrowserNonClientFrameViewMac::GetThemeBackgroundXInset() const {
  return 0;
}

void BrowserNonClientFrameViewMac::UpdateFullscreenTopUI(
    bool is_exiting_fullscreen) {
  FullscreenToolbarStyle old_style = toolbar_style_;

  // Update to the new toolbar style if needed.
  FullscreenController* controller =
      browser_view()->GetExclusiveAccessManager()->fullscreen_controller();
  if ((controller->IsWindowFullscreenForTabOrPending() ||
       controller->IsExtensionFullscreenOrPending()) &&
      !is_exiting_fullscreen) {
    toolbar_style_ = FullscreenToolbarStyle::kToolbarNone;
  } else {
    PrefService* prefs = browser_view()->GetProfile()->GetPrefs();
    toolbar_style_ = prefs->GetBoolean(prefs::kShowFullscreenToolbar)
                         ? FullscreenToolbarStyle::kToolbarPresent
                         : FullscreenToolbarStyle::kToolbarHidden;
  }

  if (old_style != toolbar_style_) {
    // Notify browser that top ui state has been changed so that we can update
    // the bookmark bar state as well.
    browser_view()->browser()->FullscreenTopUIStateChanged();

    // Re-layout if toolbar style changes in fullscreen mode.
    if (frame()->IsFullscreen())
      browser_view()->Layout();

    UMA_HISTOGRAM_ENUMERATION(
        "OSX.Fullscreen.ToolbarStyle", toolbar_style_,
        static_cast<int>(FullscreenToolbarStyle::kToolbarLast) + 1);
  }
}

bool BrowserNonClientFrameViewMac::ShouldHideTopUIForFullscreen() const {
  return frame()->IsFullscreen()
             ? toolbar_style_ != FullscreenToolbarStyle::kToolbarPresent
             : false;
}

void BrowserNonClientFrameViewMac::UpdateThrobber(bool running) {
}

int BrowserNonClientFrameViewMac::GetTabStripLeftInset() const {
  constexpr int kTabstripLeftInset = 70;  // Make room for caption buttons.
  return kTabstripLeftInset;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, views::NonClientFrameView implementation:

gfx::Rect BrowserNonClientFrameViewMac::GetBoundsForClientView() const {
  return bounds();
}

gfx::Rect BrowserNonClientFrameViewMac::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return client_bounds;
}

int BrowserNonClientFrameViewMac::NonClientHitTest(const gfx::Point& point) {
  views::View* profile_switcher_view = GetProfileSwitcherButton();
  if (profile_switcher_view) {
    gfx::Point point_in_switcher(point);
    views::View::ConvertPointToTarget(this, profile_switcher_view,
                                      &point_in_switcher);
    if (profile_switcher_view->HitTestPoint(point_in_switcher)) {
      return HTCLIENT;
    }
  }
  int component = frame()->client_view()->NonClientHitTest(point);

  // BrowserView::NonClientHitTest will return HTNOWHERE for points that hit
  // the native title bar. On Mac, we need to explicitly return HTCAPTION for
  // those points.
  if (component == HTNOWHERE && bounds().Contains(point))
    return HTCAPTION;

  return component;
}

void BrowserNonClientFrameViewMac::GetWindowMask(const gfx::Size& size,
                                                 gfx::Path* window_mask) {
}

void BrowserNonClientFrameViewMac::ResetWindowControls() {
}

void BrowserNonClientFrameViewMac::UpdateWindowIcon() {
}

void BrowserNonClientFrameViewMac::UpdateWindowTitle() {
}

void BrowserNonClientFrameViewMac::SizeConstraintsChanged() {
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, views::View implementation:

gfx::Size BrowserNonClientFrameViewMac::GetMinimumSize() const {
  gfx::Size size = browser_view()->GetMinimumSize();
  constexpr gfx::Size kMinTabbedWindowSize(400, 272);
  constexpr gfx::Size kMinPopupWindowSize(100, 122);
  size.SetToMax(browser_view()->browser()->is_type_tabbed()
                    ? kMinTabbedWindowSize
                    : kMinPopupWindowSize);
  return size;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, protected:

// views::View:

void BrowserNonClientFrameViewMac::Layout() {
  DCHECK(browser_view());
  views::View* profile_switcher_button = GetProfileSwitcherButton();
  if (profile_indicator_icon() && browser_view()->IsTabStripVisible()) {
    LayoutIncognitoButton();
    // Mac lays out the incognito icon on the right, as the stoplight
    // buttons live in its Windows/Linux location.
    profile_indicator_icon()->SetX(width() - GetAfterTabstripItemWidth());
  } else if (profile_switcher_button) {
    gfx::Size button_size = profile_switcher_button->GetPreferredSize();
    int button_x = width() - GetAfterTabstripItemWidth();
    int button_y = 0;
    TabStrip* tabstrip = browser_view()->tabstrip();
    if (tabstrip && browser_view()->IsTabStripVisible()) {
      int new_tab_button_bottom =
          tabstrip->bounds().y() + tabstrip->new_tab_button_bounds().height();
      // Align the switcher's bottom to bottom of the new tab button;
      button_y = new_tab_button_bottom - button_size.height();
    }
    profile_switcher_button->SetBounds(button_x, button_y, button_size.width(),
                                       button_size.height());
  }
  BrowserNonClientFrameView::Layout();
}

void BrowserNonClientFrameViewMac::OnPaint(gfx::Canvas* canvas) {
  if (!browser_view()->IsBrowserTypeNormal())
    return;

  canvas->DrawColor(GetFrameColor());

  if (!GetThemeProvider()->UsingSystemTheme())
    PaintThemedFrame(canvas);

  if (browser_view()->IsToolbarVisible())
    PaintToolbarTopStroke(canvas);
}

// BrowserNonClientFrameView:
AvatarButtonStyle BrowserNonClientFrameViewMac::GetAvatarButtonStyle() const {
  return AvatarButtonStyle::NATIVE;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, private:

void BrowserNonClientFrameViewMac::PaintThemedFrame(gfx::Canvas* canvas) {
  gfx::ImageSkia image = GetFrameImage();
  canvas->TileImageInt(image, 0, 0, width(), image.height());
  gfx::ImageSkia overlay = GetFrameOverlayImage();
  canvas->DrawImageInt(overlay, 0, 0);
}
