// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/utils/web_state_deferred_executor.h"

#import "base/test/test_future.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using WebStateDeferredExecutorTest = PlatformTest;

// Tests that if the web state is already realized and loaded, the completion
// block is called immediately.
TEST_F(WebStateDeferredExecutorTest, AlreadyLoadedCallImmediately) {
  web::FakeWebState web_state;
  web_state.SetIsRealized(true);
  web_state.SetLoading(false);

  WebStateDeferredExecutor* executor = [[WebStateDeferredExecutor alloc] init];

  __block BOOL completion_called = NO;
  [executor webState:&web_state
      executeOnceLoaded:^(BOOL success) {
        EXPECT_TRUE(success);
        completion_called = YES;
      }];

  EXPECT_TRUE(completion_called);
}

// A FakeNavigationManager that simulates starting a page load (setting the
// WebState's loading state to true) when LoadIfNecessary() is called.
class TestNavigationManager : public web::FakeNavigationManager {
 public:
  explicit TestNavigationManager(web::FakeWebState* web_state)
      : web_state_(web_state) {}

  void LoadIfNecessary() override {
    web::FakeNavigationManager::LoadIfNecessary();
    web_state_->SetLoading(true);
  }

 private:
  raw_ptr<web::FakeWebState> web_state_;
};

// Tests that if the web state is unrealized, calling executeOnceLoaded:
// force-realizes the web state, triggers LoadIfNecessary() on the navigation
// manager, and calls completion once loading succeeds.
TEST_F(WebStateDeferredExecutorTest, UnrealizedTriggersLoadAndCompletes) {
  web::FakeWebState web_state;
  web_state.SetIsRealized(false);
  web_state.SetLoading(false);

  auto navigation_manager = std::make_unique<TestNavigationManager>(&web_state);
  TestNavigationManager* navigation_manager_ptr = navigation_manager.get();
  web_state.SetNavigationManager(std::move(navigation_manager));

  WebStateDeferredExecutor* executor = [[WebStateDeferredExecutor alloc] init];

  base::test::TestFuture<BOOL> future;
  auto* future_ptr = &future;
  [executor webState:&web_state
      executeOnceLoaded:^(BOOL success) {
        future_ptr->SetValue(success);
      }];

  // The web state should be force-realized.
  EXPECT_TRUE(web_state.IsRealized());

  // LoadIfNecessary() should have been called on the navigation manager.
  EXPECT_TRUE(navigation_manager_ptr->LoadIfNecessaryWasCalled());

  // The completion block should not have been called yet because the page is
  // not loaded.
  EXPECT_FALSE(future.IsReady());

  // Simulate page load success.
  web_state.SetLoading(true);
  web_state.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  // The completion block should now be called with success = YES.
  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get());
}

}  // namespace
