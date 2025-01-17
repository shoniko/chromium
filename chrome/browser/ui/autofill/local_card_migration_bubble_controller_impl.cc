// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/local_card_migration_bubble_controller_impl.h"

#include <stddef.h>

#include "chrome/browser/ui/autofill/local_card_migration_bubble.h"
#include "chrome/browser/ui/autofill/popup_constants.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/l10n/l10n_util.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    autofill::LocalCardMigrationBubbleControllerImpl);

namespace autofill {

// TODO(crbug.com/862405): Build a base class for this
// and SaveCardBubbleControllerImpl.
LocalCardMigrationBubbleControllerImpl::LocalCardMigrationBubbleControllerImpl(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      local_card_migration_bubble_(nullptr) {}

LocalCardMigrationBubbleControllerImpl::
    ~LocalCardMigrationBubbleControllerImpl() {
  if (local_card_migration_bubble_)
    HideBubble();
}

void LocalCardMigrationBubbleControllerImpl::ShowBubble(
    base::OnceClosure local_card_migration_bubble_closure) {
  // Don't show the bubble if it's already visible.
  if (local_card_migration_bubble_)
    return;

  is_reshow_ = false;
  local_card_migration_bubble_closure_ =
      std::move(local_card_migration_bubble_closure);

  ShowBubbleImplementation();
}

void LocalCardMigrationBubbleControllerImpl::HideBubble() {
  if (local_card_migration_bubble_) {
    local_card_migration_bubble_->Hide();
    local_card_migration_bubble_ = nullptr;
  }
}

void LocalCardMigrationBubbleControllerImpl::ReshowBubble() {
  if (local_card_migration_bubble_)
    return;

  ShowBubbleImplementation();
}

bool LocalCardMigrationBubbleControllerImpl::IsIconVisible() const {
  return !local_card_migration_bubble_closure_.is_null();
}

LocalCardMigrationBubble*
LocalCardMigrationBubbleControllerImpl::local_card_migration_bubble_view()
    const {
  return local_card_migration_bubble_;
}

base::string16 LocalCardMigrationBubbleControllerImpl::GetWindowTitle() const {
  // TODO(crbug.com/859254): Update string once mock is finalized.
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_LOCAL_CARD_MIGRATION_BUBBLE_TITLE);
}

void LocalCardMigrationBubbleControllerImpl::OnConfirmButtonClicked() {
  DCHECK(local_card_migration_bubble_closure_);
  std::move(local_card_migration_bubble_closure_).Run();
}

void LocalCardMigrationBubbleControllerImpl::OnCancelButtonClicked() {
  local_card_migration_bubble_closure_.Reset();
}

void LocalCardMigrationBubbleControllerImpl::OnBubbleClosed() {
  local_card_migration_bubble_ = nullptr;
  UpdateIcon();
}

base::TimeDelta LocalCardMigrationBubbleControllerImpl::Elapsed() const {
  return timer_->Elapsed();
}

void LocalCardMigrationBubbleControllerImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
    return;

  // Nothing to do if there's no bubble available.
  if (!local_card_migration_bubble_closure_)
    return;

  // Don't react to same-document (fragment) navigations.
  if (navigation_handle->IsSameDocument())
    return;

  // Don't do anything if a navigation occurs before a user could reasonably
  // interact with the bubble.
  if (Elapsed() < kCardBubbleSurviveNavigationTime)
    return;

  // Otherwise, get rid of the bubble and icon.
  local_card_migration_bubble_closure_.Reset();
  bool bubble_was_visible = local_card_migration_bubble_;
  if (bubble_was_visible) {
    local_card_migration_bubble_->Hide();
    OnBubbleClosed();
  } else {
    UpdateIcon();
  }
}

void LocalCardMigrationBubbleControllerImpl::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN)
    HideBubble();
}

void LocalCardMigrationBubbleControllerImpl::WebContentsDestroyed() {
  HideBubble();
}

void LocalCardMigrationBubbleControllerImpl::ShowBubbleImplementation() {
  DCHECK(local_card_migration_bubble_closure_);
  DCHECK(!local_card_migration_bubble_);

  // Need to create location bar icon before bubble, otherwise bubble will be
  // unanchored.
  UpdateIcon();

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  local_card_migration_bubble_ =
      browser->window()->ShowLocalCardMigrationBubble(web_contents(), this,
                                                      true);
  DCHECK(local_card_migration_bubble_);
  UpdateIcon();
  timer_.reset(new base::ElapsedTimer());
}

void LocalCardMigrationBubbleControllerImpl::UpdateIcon() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser)
    return;
  LocationBar* location_bar = browser->window()->GetLocationBar();
  if (!location_bar)
    return;
  location_bar->UpdateLocalCardMigrationIcon();
}

}  // namespace autofill
