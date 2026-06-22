// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/session_controller.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace dictation {

namespace {

class DictationSessionControllerTest : public testing::Test {
 public:
  DictationSessionControllerTest() {
    scoped_feature_list_.InitAndEnableFeature(kDictation);
    controller_ = std::make_unique<SessionController>(mock_delegate_);
  }
  ~DictationSessionControllerTest() override = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  testing::NiceMock<MockSessionControllerDelegate> mock_delegate_;
  std::unique_ptr<SessionController> controller_;
};

TEST_F(DictationSessionControllerTest, StartsInactive) {
  EXPECT_EQ(controller_->GetState(), SessionState::kInactive);
}

// Test that starting and stopping a stream moves the controller into the
// appropriate state.
TEST_F(DictationSessionControllerTest, StreamAffectsState) {
  controller_->StartDictationStream(std::make_unique<MockTarget>());
  EXPECT_EQ(controller_->GetState(), SessionState::kStreamInitializing);
  EXPECT_NE(controller_->attached_stream_provider(), nullptr);

  controller_->EndDictationStream();
  EXPECT_EQ(controller_->GetState(), SessionState::kInactive);
  EXPECT_EQ(controller_->attached_stream_provider(), nullptr);
}

// Test that starting a stream initializes the stream provider and binds it to
// the given target.
TEST_F(DictationSessionControllerTest, StartStreamInitializesStreamProvider) {
  auto target = std::make_unique<MockTarget>();
  MockTarget* target_ptr = target.get();
  auto mock_stream_provider =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_ptr = mock_stream_provider.get();

  // Starting a stream should create a stream provider and bind it to the given
  // target.
  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider)));
  EXPECT_CALL(*stream_provider_ptr, BindToTargetAndConnect(_))
      .WillOnce([target_ptr](std::unique_ptr<Target> passed_target) {
        EXPECT_EQ(passed_target.get(), target_ptr);
      });
  controller_->StartDictationStream(std::move(target));
}

// Test that ending a stream notifies the stream provider to stop.
TEST_F(DictationSessionControllerTest, EndStream) {
  auto mock_stream_provider =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_ptr = mock_stream_provider.get();

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider)));
  controller_->StartDictationStream(std::make_unique<MockTarget>());

  EXPECT_CALL(*stream_provider_ptr, Stop());
  controller_->EndDictationStream();
}

// Test that calling EndDictationStream while the controller is in the
// kStreamInitializing state correctly transitions the controller to kInactive.
TEST_F(DictationSessionControllerTest, EndStreamDuringInitialization) {
  auto mock_stream_provider =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_ptr = mock_stream_provider.get();

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider)));
  controller_->StartDictationStream(std::make_unique<MockTarget>());
  ASSERT_EQ(controller_->GetState(), SessionState::kStreamInitializing);

  EXPECT_CALL(*stream_provider_ptr, Stop());
  controller_->EndDictationStream();
  EXPECT_EQ(controller_->GetState(), SessionState::kInactive);
  EXPECT_EQ(controller_->attached_stream_provider(), nullptr);
}

// Test that registering a callback receives state updates when the controller
// transitions states.
TEST_F(DictationSessionControllerTest, StateChangedCallback) {
  std::vector<SessionState> states;
  base::CallbackListSubscription subscription =
      controller_->AddSessionStateChangedCallback(base::BindLambdaForTesting(
          [&](SessionState state) { states.push_back(state); }));

  controller_->StartDictationStream(std::make_unique<MockTarget>());
  controller_->EndDictationStream();

  EXPECT_THAT(states, testing::ElementsAre(SessionState::kStreamInitializing,
                                           SessionState::kInactive));
}

// Test that propagating state changes from the stream provider updates the
// controller's state accordingly.
TEST_F(DictationSessionControllerTest, StreamProviderStatePropagates) {
  auto mock_stream_provider =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_ptr = mock_stream_provider.get();

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider)));
  controller_->StartDictationStream(std::make_unique<MockTarget>());
  EXPECT_EQ(controller_->GetState(), SessionState::kStreamInitializing);

  // Transition to transcribing.
  EXPECT_CALL(*stream_provider_ptr, GetState())
      .WillRepeatedly(Return(StreamProvider::StreamState::kTranscribing));
  controller_->DidUpdateStreamProviderState(
      *stream_provider_ptr, StreamProvider::StreamState::kInitializing);
  EXPECT_EQ(controller_->GetState(), SessionState::kTranscribing);

  // Transition to complete.
  EXPECT_CALL(*stream_provider_ptr, GetState())
      .WillRepeatedly(Return(StreamProvider::StreamState::kComplete));

  // Transitions to inactive are asynchronous.
  base::RunLoop run_loop;
  base::CallbackListSubscription subscription =
      controller_->AddSessionStateChangedCallback(
          base::BindLambdaForTesting([&](SessionState state) {
            if (state == SessionState::kInactive) {
              run_loop.Quit();
            }
          }));
  controller_->DidUpdateStreamProviderState(
      *stream_provider_ptr, StreamProvider::StreamState::kTranscribing);
  run_loop.Run();

  EXPECT_EQ(controller_->GetState(), SessionState::kInactive);
}

// Test that propagating state changes for a failure
TEST_F(DictationSessionControllerTest, StreamProviderStatePropagatesFailure) {
  auto mock_stream_provider =
      std::make_unique<testing::NiceMock<MockStreamProvider>>();
  MockStreamProvider* stream_provider_ptr = mock_stream_provider.get();

  EXPECT_CALL(mock_delegate_, CreateStreamProvider(_))
      .WillOnce(Return(std::move(mock_stream_provider)));
  controller_->StartDictationStream(std::make_unique<MockTarget>());
  EXPECT_EQ(controller_->GetState(), SessionState::kStreamInitializing);

  // Transition to transcribing.
  EXPECT_CALL(*stream_provider_ptr, GetState())
      .WillRepeatedly(Return(StreamProvider::StreamState::kTranscribing));
  controller_->DidUpdateStreamProviderState(
      *stream_provider_ptr, StreamProvider::StreamState::kInitializing);
  EXPECT_EQ(controller_->GetState(), SessionState::kTranscribing);

  // Transition to failure.
  EXPECT_CALL(*stream_provider_ptr, GetState())
      .WillRepeatedly(Return(StreamProvider::StreamState::kFailed));

  // Transitions to inactive are asynchronous.
  base::RunLoop run_loop;
  base::CallbackListSubscription subscription =
      controller_->AddSessionStateChangedCallback(
          base::BindLambdaForTesting([&](SessionState state) {
            if (state == SessionState::kInactive) {
              run_loop.Quit();
            }
          }));
  controller_->DidUpdateStreamProviderState(
      *stream_provider_ptr, StreamProvider::StreamState::kTranscribing);
  run_loop.Run();

  EXPECT_EQ(controller_->GetState(), SessionState::kInactive);
}

}  // namespace

}  // namespace dictation
