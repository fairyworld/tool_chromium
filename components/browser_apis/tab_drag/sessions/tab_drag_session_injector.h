// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_INJECTOR_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_INJECTOR_H_

#include "components/browser_apis/tab_drag/tab_drag_api.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace tabs_api {

class TabDragSessionInputAdapter;
class TabDragSessionInputListener;
class TabDragWindowAdapter;

class DropTargetRegistry {
 public:
  virtual ~DropTargetRegistry() = default;
  // Registers a `target` (DropTarget) associated with the given
  // `window_adapter`. The `window_adapter` is used as a key to route events to
  // the correct target based on the window coordinates. The `registration`
  // receiver is used to manage the lifetime of the registration; when the
  // client discards the corresponding remote, the target will be automatically
  // unregistered.
  virtual void RegisterDropTarget(
      TabDragWindowAdapter* window_adapter,
      mojo::PendingAssociatedRemote<mojom::DropTarget> target,
      mojo::PendingAssociatedReceiver<mojom::DropTargetRegistration>
          registration) = 0;

  // Unregisters the drop target associated with the given `window_adapter`.
  // This is typically called automatically when the `DropTargetRegistration`
  // pipe is closed.
  virtual void UnregisterDropTarget(TabDragWindowAdapter* window_adapter) = 0;
};

class TabDragSessionInjector {
 public:
  virtual ~TabDragSessionInjector() = default;

  virtual TabDragSessionInputAdapter& GetInputAdapter() = 0;
  virtual TabDragSessionInputListener& GetInputListener() = 0;
  virtual DropTargetRegistry& GetDropTargetRegistry() = 0;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_INJECTOR_H_
