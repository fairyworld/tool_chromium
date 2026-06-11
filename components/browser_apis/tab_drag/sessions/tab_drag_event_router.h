// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_EVENT_ROUTER_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_EVENT_ROUTER_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_injector.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_input_listener.h"
#include "components/browser_apis/tab_drag/tab_drag_api.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace tabs_api {

class TabDragWindowAdapter;
class TabDragSession;

// Routes tab drag events to the right DropTarget. It maintains a mapping of
// active drop targets associated with their respective browser windows.
enum class DropTargetEvent {
  kEntered,
  kDrag,
  kLeave,
  kDrop,
  kCancelled,
};

class TabDragEventRouter : public TabDragSessionInputListener,
                           public DropTargetRegistry {
 public:
  TabDragEventRouter();
  TabDragEventRouter(const TabDragEventRouter&) = delete;
  TabDragEventRouter& operator=(const TabDragEventRouter&) = delete;
  ~TabDragEventRouter() override;

  // DropTargetRegistry overrides:
  void RegisterDropTarget(
      TabDragWindowAdapter* window_adapter,
      mojo::PendingAssociatedRemote<mojom::DropTarget> target,
      mojo::PendingAssociatedReceiver<mojom::DropTargetRegistration>
          registration) override;
  void UnregisterDropTarget(TabDragWindowAdapter* window_adapter) override;

  // TabDragSessionInputListener overrides:
  void OnSessionStarted(TabDragSession* session) override;
  void OnSessionEnded() override;
  void OnDragSessionEvent(const TabDragSessionInputEvent& event) override;

  base::WeakPtr<TabDragEventRouter> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  size_t drop_targets_count_for_testing() const { return drop_targets_.size(); }

 private:
  void HandleDragMoved(const gfx::Point& screen_point);
  void HandleDragDropped(const gfx::Point& screen_point);
  void HandleDragCancelled();

  void TransitionToTargetWindow(TabDragWindowAdapter* new_target,
                                const gfx::Point& screen_point);

  void DispatchEvent(TabDragWindowAdapter* window,
                     DropTargetEvent event,
                     const gfx::Point& screen_point = gfx::Point());

  TabDragWindowAdapter* FindTargetWindow(const gfx::Point& screen_point) const;
  mojom::DropTarget* GetDropTarget(TabDragWindowAdapter* window) const;
  std::vector<tabs_api::NodeId> GetDraggedTabs() const;

  std::map<TabDragWindowAdapter*, mojo::AssociatedRemote<mojom::DropTarget>>
      drop_targets_;
  raw_ptr<TabDragSession> active_session_ = nullptr;
  raw_ptr<TabDragWindowAdapter> current_drop_target_window_ = nullptr;
  base::WeakPtrFactory<TabDragEventRouter> weak_factory_{this};
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_EVENT_ROUTER_H_
