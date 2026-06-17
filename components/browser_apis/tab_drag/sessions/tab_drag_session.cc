// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/sessions/tab_drag_session.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_session_input_adapter.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_injector.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_listener.h"

namespace tabs_api {

TabDragSession::TabDragSession(TabDragSessionParams params,
                               TabDragSessionInjector* injector)
    : dragged_tabs_(std::move(params.source_tab_ids)),
      injector_(CHECK_DEREF(injector)),
      end_callback_(std::move(params.end_callback)),
      start_point_in_screen_(params.start_point),
      last_mouse_screen_point_(params.start_point),
      dragged_window_(params.source_window) {
  CHECK(dragged_window_);
}

base::expected<void, mojo_base::mojom::ErrorPtr> TabDragSession::Start() {
  auto result =
      injector_->GetInputAdapter().StartInputCapture(base::BindRepeating(
          &TabDragSession::OnInputEvent, base::Unretained(this)));
  if (result.has_value()) {
    dragged_window_->SetCapture();
    injector_->GetSessionListener().OnSessionStarted(
        dragged_tabs_, dragged_window_, start_point_in_screen_);
  }
  return result;
}

TabDragSession::~TabDragSession() {
  dragged_window_->ReleaseCapture();
  injector_->GetInputAdapter().ReleaseInputCapture();
}

void TabDragSession::EndSession() {
  if (end_callback_) {
    std::move(end_callback_).Run();
  }
}

void TabDragSession::UpdateDraggedWindow(TabDragWindowAdapter* new_window) {
  CHECK(new_window);
  dragged_window_->ReleaseCapture();
  dragged_window_ = new_window;
  dragged_window_->SetCapture();
}

void TabDragSession::OnInputEvent(const TabDragInputEvent& event) {
  if (event.type == TabDragInputEvent::Type::kMoved ||
      event.type == TabDragInputEvent::Type::kDropped) {
    last_mouse_screen_point_ = event.screen_point;
    delta_ = event.screen_point - start_point_in_screen_;
  }

  switch (event.type) {
    case TabDragInputEvent::Type::kCancelled:
      injector_->GetSessionListener().OnSessionCancelled();
      EndSession();
      break;
    case TabDragInputEvent::Type::kCaptureChanged:
      if (dragged_window_->HasCapture()) {
        break;
      }
      injector_->GetSessionListener().OnSessionCancelled();
      EndSession();
      break;
    case TabDragInputEvent::Type::kDropped:
      injector_->GetSessionListener().OnSessionDropped(event.screen_point);
      EndSession();
      break;
    case TabDragInputEvent::Type::kMoved:
      HandleMovedEvent(event.screen_point);
      break;
  }
}

void TabDragSession::HandleMovedEvent(const gfx::Point& screen_point) {
  switch (drag_mode_) {
    case DragMode::kAttachedToWindow:
      HandleAttachedMove(screen_point);
      break;
    case DragMode::kDetachedWindow:
      HandleDetachedMove(screen_point);
      break;
  }
}

void TabDragSession::HandleAttachedMove(const gfx::Point& screen_point) {
  CHECK(dragged_window_);
  gfx::Rect bounds = dragged_window_->GetBoundsInScreen();
  constexpr int kTearThreshold = 15;
  bounds.Inset(-kTearThreshold);

  if (!bounds.Contains(screen_point)) {
    drag_mode_ = DragMode::kDetachedWindow;
  } else {
    injector_->GetSessionListener().OnDragMoved(screen_point);
  }
}

void TabDragSession::HandleDetachedMove(const gfx::Point& screen_point) {
  auto new_target = injector_->GetDropTargetRegistry().FindTargetWindow(
      screen_point, dragged_window_);

  if (new_target) {
    TabDragWindowAdapter& target_window = new_target->get();
    if (target_window.GetBoundsInScreen().Contains(screen_point)) {
      drag_mode_ = DragMode::kAttachedToWindow;
      dragged_window_ = &target_window;
      injector_->GetSessionListener().OnTargetWindowChanged(dragged_window_,
                                                            screen_point);
      return;
    }
  }
}

}  // namespace tabs_api
