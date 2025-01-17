// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"

#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/geometry/size.h"

class PictureInPictureWindowManager::WebContentsDestroyedObserver
    : public content::WebContentsObserver {
 public:
  WebContentsDestroyedObserver(PictureInPictureWindowManager* owner,
                               content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents), owner_(owner) {}

  ~WebContentsDestroyedObserver() final = default;

  void WebContentsDestroyed() final { owner_->CloseWindowInternal(); }

 private:
  // Owns |this|.
  PictureInPictureWindowManager* owner_ = nullptr;
};

PictureInPictureWindowManager* PictureInPictureWindowManager::GetInstance() {
  return base::Singleton<PictureInPictureWindowManager>::get();
}

gfx::Size PictureInPictureWindowManager::EnterPictureInPicture(
    content::WebContents* web_contents,
    const viz::SurfaceId& surface_id,
    const gfx::Size& natural_size) {
  // If there was already a controller, close the existing window before
  // creating the next one.
  if (pip_window_controller_)
    CloseWindowInternal();

  // Create or update |pip_window_controller_| for the current WebContents.
  if (!pip_window_controller_ ||
      pip_window_controller_->GetInitiatorWebContents() != web_contents) {
    CreateWindowInternal(web_contents);
  }

  pip_window_controller_->EmbedSurface(surface_id, natural_size);
  return pip_window_controller_->Show();
}

void PictureInPictureWindowManager::ExitPictureInPicture() {
  if (pip_window_controller_)
    CloseWindowInternal();
}

void PictureInPictureWindowManager::CreateWindowInternal(
    content::WebContents* web_contents) {
  destroyed_observer_ =
      std::make_unique<WebContentsDestroyedObserver>(this, web_contents);
  pip_window_controller_ =
      content::PictureInPictureWindowController::GetOrCreateForWebContents(
          web_contents);
}

void PictureInPictureWindowManager::CloseWindowInternal() {
  DCHECK(destroyed_observer_);
  DCHECK(pip_window_controller_);

  destroyed_observer_.reset();
  pip_window_controller_->Close(false /* should_pause_video */);
  pip_window_controller_ = nullptr;
}

PictureInPictureWindowManager::PictureInPictureWindowManager() = default;

PictureInPictureWindowManager::~PictureInPictureWindowManager() = default;
