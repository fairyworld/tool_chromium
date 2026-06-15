// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/sessions/tab_drag_session.h"

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_session_input_adapter.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_injector.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_input_listener.h"
#include "components/browser_apis/tab_drag/testing/toy_tab_drag_session_input_adapter.h"
#include "components/browser_apis/tab_drag/testing/toy_tab_drag_window_adapter.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"

namespace tabs_api {

class TabDragSessionTest : public ::testing::Test {
 protected:
  TabDragSessionTest() : dummy_window_(gfx::Rect(0, 0, 100, 100)) {}
  ~TabDragSessionTest() override = default;

  ToyTabDragWindowAdapter dummy_window_;
};

class DummyTabDragSessionInputListener : public TabDragSessionInputListener {
 public:
  void OnSessionStarted(TabDragSession* session) override {}
  void OnSessionEnded() override {}
  void OnDragSessionEvent(const TabDragSessionInputEvent& event) override {}
};

class DummyDropTargetRegistry : public DropTargetRegistry {
 public:
  void RegisterDropTarget(
      TabDragWindowAdapter* window_adapter,
      mojo::PendingAssociatedRemote<mojom::DropTarget> target,
      mojo::PendingAssociatedReceiver<mojom::DropTargetRegistration>
          registration) override {}
  void UnregisterDropTarget(TabDragWindowAdapter* window_adapter) override {}
};

class ToyTabDragSessionInputListener : public TabDragSessionInputListener {
 public:
  ToyTabDragSessionInputListener() = default;
  ~ToyTabDragSessionInputListener() override = default;

  // TabDragSessionInputListener overrides:
  void OnSessionStarted(TabDragSession* session) override {
    session_started_ = true;
    session_ = session;
  }
  void OnSessionEnded() override {
    session_ended_ = true;
    session_ = nullptr;
  }
  void OnDragSessionEvent(const TabDragSessionInputEvent& event) override {
    events_.push_back(event);
  }

  bool session_started() const { return session_started_; }
  bool session_ended() const { return session_ended_; }
  TabDragSession* session() const { return session_; }
  const std::vector<TabDragSessionInputEvent>& events() const {
    return events_;
  }

 private:
  bool session_started_ = false;
  bool session_ended_ = false;
  raw_ptr<TabDragSession> session_ = nullptr;
  std::vector<TabDragSessionInputEvent> events_;
};

TEST_F(TabDragSessionTest, StartAndReleaseCapture) {
  ToyTabDragSessionInputAdapter toy_adapter;
  DummyTabDragSessionInputListener dummy_listener;
  DummyDropTargetRegistry dummy_registry;
  ToyTabDragSessionInjector injector(toy_adapter, dummy_listener,
                                     dummy_registry);
  base::MockOnceClosure end_callback;
  ToyTabDragWindowAdapter toy_window(gfx::Rect(0, 0, 100, 100));

  EXPECT_FALSE(toy_adapter.capture_started());
  EXPECT_FALSE(toy_adapter.capture_released());
  EXPECT_FALSE(toy_window.HasCapture());

  {
    TabDragSessionParams params{
        .source_window = &toy_window,
        .source_tab_ids = {NodeId(NodeId::Type::kContent, "tab1")},
        .start_point = gfx::Point(),
        .end_callback = end_callback.Get()};
    TabDragSession session(std::move(params), &injector);
    EXPECT_FALSE(toy_adapter.capture_started());
    EXPECT_TRUE(session.Start().has_value());
    EXPECT_TRUE(toy_adapter.capture_started());
    EXPECT_FALSE(toy_adapter.capture_released());
    EXPECT_TRUE(toy_window.HasCapture());
  }

  EXPECT_TRUE(toy_adapter.capture_released());
  EXPECT_FALSE(toy_window.HasCapture());
}

TEST_F(TabDragSessionTest, InputEventCancelled) {
  ToyTabDragSessionInputAdapter toy_adapter;
  DummyTabDragSessionInputListener dummy_listener;
  DummyDropTargetRegistry dummy_registry;
  ToyTabDragSessionInjector injector(toy_adapter, dummy_listener,
                                     dummy_registry);
  base::MockOnceClosure end_callback;

  TabDragSessionParams params{
      .source_window = &dummy_window_,
      .source_tab_ids = {NodeId(NodeId::Type::kContent, "tab1")},
      .start_point = gfx::Point(),
      .end_callback = end_callback.Get()};
  TabDragSession session(std::move(params), &injector);
  EXPECT_TRUE(session.Start().has_value());

  EXPECT_CALL(end_callback, Run()).Times(1);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kCancelled);
}

TEST_F(TabDragSessionTest, InputEventDropped) {
  ToyTabDragSessionInputAdapter toy_adapter;
  DummyTabDragSessionInputListener dummy_listener;
  DummyDropTargetRegistry dummy_registry;
  ToyTabDragSessionInjector injector(toy_adapter, dummy_listener,
                                     dummy_registry);
  base::MockOnceClosure end_callback;

  TabDragSessionParams params{
      .source_window = &dummy_window_,
      .source_tab_ids = {NodeId(NodeId::Type::kContent, "tab1")},
      .start_point = gfx::Point(),
      .end_callback = end_callback.Get()};
  TabDragSession session(std::move(params), &injector);
  EXPECT_TRUE(session.Start().has_value());

  EXPECT_CALL(end_callback, Run()).Times(1);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kDropped);
}

TEST_F(TabDragSessionTest, CoordinateTracking) {
  ToyTabDragSessionInputAdapter toy_adapter;
  DummyTabDragSessionInputListener dummy_listener;
  DummyDropTargetRegistry dummy_registry;
  ToyTabDragSessionInjector injector(toy_adapter, dummy_listener,
                                     dummy_registry);
  base::MockOnceClosure end_callback;

  gfx::Point start_point(10, 10);
  TabDragSessionParams params{
      .source_window = &dummy_window_,
      .source_tab_ids = {NodeId(NodeId::Type::kContent, "tab1")},
      .start_point = start_point,
      .end_callback = end_callback.Get()};
  TabDragSession session(std::move(params), &injector);
  EXPECT_TRUE(session.Start().has_value());

  EXPECT_EQ(session.start_point_in_screen(), start_point);
  EXPECT_EQ(session.last_mouse_screen_point(), start_point);
  EXPECT_EQ(session.delta(), gfx::Vector2d(0, 0));

  // Move mouse
  gfx::Point move_point(15, 20);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kMoved, move_point);

  EXPECT_EQ(session.last_mouse_screen_point(), move_point);
  EXPECT_EQ(session.delta(), gfx::Vector2d(5, 10));

  // Drop mouse
  gfx::Point drop_point(25, 30);
  EXPECT_CALL(end_callback, Run()).Times(1);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kDropped, drop_point);

  EXPECT_EQ(session.last_mouse_screen_point(), drop_point);
  EXPECT_EQ(session.delta(), gfx::Vector2d(15, 20));
}

TEST_F(TabDragSessionTest, ListenerNotification) {
  ToyTabDragSessionInputAdapter toy_adapter;
  base::MockOnceClosure end_callback;
  ToyTabDragSessionInputListener listener;
  DummyDropTargetRegistry dummy_registry;
  ToyTabDragSessionInjector injector(toy_adapter, listener, dummy_registry);

  TabDragSessionParams params{
      .source_window = &dummy_window_,
      .source_tab_ids = {NodeId(NodeId::Type::kContent, "tab1")},
      .start_point = gfx::Point(),
      .end_callback = end_callback.Get()};
  TabDragSession session(std::move(params), &injector);

  EXPECT_FALSE(listener.session_started());
  EXPECT_TRUE(session.Start().has_value());
  EXPECT_TRUE(listener.session_started());
  EXPECT_EQ(listener.session(), &session);

  // Move
  gfx::Point move_point(10, 20);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kMoved, move_point);

  ASSERT_EQ(listener.events().size(), 1u);
  EXPECT_EQ(listener.events()[0].type, TabDragSessionInputEvent::Type::kMoved);
  EXPECT_EQ(listener.events()[0].screen_point, move_point);

  // Drop
  gfx::Point drop_point(30, 40);
  EXPECT_CALL(end_callback, Run()).Times(1);
  EXPECT_FALSE(listener.session_ended());
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kDropped, drop_point);

  ASSERT_EQ(listener.events().size(), 2u);
  EXPECT_EQ(listener.events()[1].type,
            TabDragSessionInputEvent::Type::kDropped);
  EXPECT_EQ(listener.events()[1].screen_point, drop_point);
  EXPECT_TRUE(listener.session_ended());
}

TEST_F(TabDragSessionTest, CaptureLostExternally) {
  ToyTabDragSessionInputAdapter toy_adapter;
  DummyTabDragSessionInputListener dummy_listener;
  DummyDropTargetRegistry dummy_registry;
  ToyTabDragSessionInjector injector(toy_adapter, dummy_listener,
                                     dummy_registry);
  base::MockOnceClosure end_callback;
  ToyTabDragWindowAdapter toy_window(gfx::Rect(0, 0, 100, 100));

  TabDragSessionParams params{
      .source_window = &toy_window,
      .source_tab_ids = {NodeId(NodeId::Type::kContent, "tab1")},
      .start_point = gfx::Point(),
      .end_callback = end_callback.Get()};
  TabDragSession session(std::move(params), &injector);
  EXPECT_TRUE(session.Start().has_value());
  EXPECT_TRUE(toy_window.HasCapture());

  // Simulate external capture loss.
  toy_window.ReleaseCapture();
  EXPECT_CALL(end_callback, Run()).Times(1);
  toy_adapter.SendToyEvent(TabDragInputEvent::Type::kCaptureChanged);
}

}  // namespace tabs_api
