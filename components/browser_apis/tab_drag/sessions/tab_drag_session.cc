// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/sessions/tab_drag_session.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_session_input_adapter.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_injector.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_input_listener.h"

namespace tabs_api {

TabDragSession::TabDragSession(TabDragSessionParams params,
                               TabDragSessionInjector* injector)
    : dragged_tabs_(std::move(params.source_tab_ids)),
      injector_(CHECK_DEREF(injector)),
      end_callback_(std::move(params.end_callback)),
      start_point_in_screen_(params.start_point),
      last_mouse_screen_point_(params.start_point) {}

base::expected<void, mojo_base::mojom::ErrorPtr> TabDragSession::Start() {
  auto result = injector_->GetInputAdapter().StartInputCapture(
      dragged_tabs_, base::BindRepeating(&TabDragSession::OnInputEvent,
                                         base::Unretained(this)));
  if (result.has_value()) {
    injector_->GetInputListener().OnSessionStarted(this);
  }
  return result;
}

TabDragSession::~TabDragSession() {
  injector_->GetInputAdapter().ReleaseInputCapture();
}

void TabDragSession::EndSession() {
  injector_->GetInputListener().OnSessionEnded();
  if (end_callback_) {
    std::move(end_callback_).Run();
  }
}

void TabDragSession::OnInputEvent(const TabDragInputEvent& event) {
  TabDragSessionInputEvent::Type event_type;
  bool should_end = false;
  switch (event.type) {
    case TabDragInputEvent::Type::kCancelled:
      event_type = TabDragSessionInputEvent::Type::kCancelled;
      should_end = true;
      break;
    case TabDragInputEvent::Type::kDropped:
      event_type = TabDragSessionInputEvent::Type::kDropped;
      last_mouse_screen_point_ = event.screen_point;
      delta_ = event.screen_point - start_point_in_screen_;
      should_end = true;
      break;
    case TabDragInputEvent::Type::kMoved:
      event_type = TabDragSessionInputEvent::Type::kMoved;
      last_mouse_screen_point_ = event.screen_point;
      delta_ = event.screen_point - start_point_in_screen_;
      break;
  }

  TabDragSessionInputEvent session_event{.type = event_type,
                                         .screen_point = event.screen_point};
  injector_->GetInputListener().OnDragSessionEvent(session_event);

  if (should_end) {
    EndSession();
  }
}

}  // namespace tabs_api
