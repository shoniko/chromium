// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_CAST_SYSTEM_GESTURE_EVENT_HANDLER_H_
#define CHROMECAST_GRAPHICS_CAST_SYSTEM_GESTURE_EVENT_HANDLER_H_

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/timer/elapsed_timer.h"
#include "chromecast/graphics/cast_gesture_handler.h"
#include "ui/events/event_handler.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace chromecast {

// An event handler for detecting system-wide gestures performed on the screen.
// Recognizes swipe gestures that originate from the top, left, bottom, and
// right of the root window.
class CastSystemGestureEventHandler : public ui::EventHandler {
 public:
  explicit CastSystemGestureEventHandler(aura::Window* root_window);

  ~CastSystemGestureEventHandler() override;

  // Register a new handler for gesture events, including side swipe and tap.
  void AddGestureHandler(CastGestureHandler* handler);

  // Remove the registration of a gesture handler.
  void RemoveGestureHandler(CastGestureHandler* handler);

  CastSideSwipeOrigin GetDragPosition(const gfx::Point& point,
                                      const gfx::Rect& screen_bounds) const;

  void ProcessPressedEvent(ui::GestureEvent* event);

  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  const int gesture_start_width_;
  const int gesture_start_height_;

  aura::Window* root_window_;
  CastSideSwipeOrigin current_swipe_;
  base::ElapsedTimer current_swipe_time_;

  base::flat_set<CastGestureHandler*> gesture_handlers_;

  DISALLOW_COPY_AND_ASSIGN(CastSystemGestureEventHandler);
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_CAST_SYSTEM_GESTURE_EVENT_HANDLER_H_
