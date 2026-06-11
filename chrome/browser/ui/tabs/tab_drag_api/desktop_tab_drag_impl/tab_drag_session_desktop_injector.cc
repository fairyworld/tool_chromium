// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_drag_api/desktop_tab_drag_impl/tab_drag_session_desktop_injector.h"

#include "chrome/browser/ui/tabs/tab_drag_api/desktop_tab_drag_impl/tab_drag_session_input_adapter_impl.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_session_input_adapter.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_event_router.h"

namespace tabs_api {

TabDragSessionDesktopInjector::TabDragSessionDesktopInjector()
    : adapter_(std::make_unique<TabDragSessionInputAdapterImpl>()),
      event_router_(std::make_unique<TabDragEventRouter>()) {}

TabDragSessionDesktopInjector::~TabDragSessionDesktopInjector() = default;

TabDragSessionInputAdapter& TabDragSessionDesktopInjector::GetInputAdapter() {
  return *adapter_;
}

TabDragSessionInputListener& TabDragSessionDesktopInjector::GetInputListener() {
  return *event_router_;
}

DropTargetRegistry& TabDragSessionDesktopInjector::GetDropTargetRegistry() {
  return *event_router_;
}

}  // namespace tabs_api
