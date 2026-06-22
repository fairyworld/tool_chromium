// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/session_controller.h"

#include <memory>
#include <ostream>

#include "base/no_destructor.h"
#include "base/state_transitions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/dictation/session_controller_delegate.h"
#include "chrome/browser/dictation/session_ui.h"
#include "chrome/browser/dictation/stream_provider.h"
#include "chrome/browser/dictation/target.h"

namespace dictation {

SessionController::SessionController(SessionControllerDelegate& delegate)
    : delegate_(delegate) {}

SessionController::~SessionController() {
  CHECK(state_ != SessionState::kInactive || !attached_stream_provider_);
  if (state_ != SessionState::kInactive) {
    EndDictationStream();
  }
}

void SessionController::Initialize() {
  ui_ = delegate_->CreateUi(*this);
}

void SessionController::StartDictationStream(std::unique_ptr<Target> target) {
  CHECK_EQ(state_, SessionState::kInactive);

  std::unique_ptr<StreamProvider> stream_provider =
      delegate_->CreateStreamProvider(*this);
  stream_provider->BindToTargetAndConnect(std::move(target));
  attached_stream_provider_ = std::move(stream_provider);

  MoveToState(SessionState::kStreamInitializing);
}

void SessionController::EndDictationStream() {
  CHECK_NE(state_, SessionState::kInactive);
  attached_stream_provider_->Stop();
  // TODO(b/525943882): The stream needs to live on until the transcript is
  // complete.
  attached_stream_provider_.reset();
  MoveToState(SessionState::kInactive);
}

void SessionController::UiRequestEndSession() {
  // EndSession will destroy `this` which owns other objects that call into here
  // so PostTask to avoid destroying objects in the callstack.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<SessionController> this_ptr) {
                       if (!this_ptr) {
                         return;
                       }
                       this_ptr->delegate_->EndSession();
                       CHECK(!this_ptr);
                     },
                     weak_ptr_factory_.GetWeakPtr()));
}

void SessionController::DidUpdateStreamProviderState(
    StreamProvider& stream_provider,
    StreamProvider::StreamState old_state) {
  CHECK_EQ(&stream_provider, attached_stream_provider_.get());

  switch (stream_provider.GetState()) {
    case StreamProvider::StreamState::kInitializing:
      MoveToState(SessionState::kStreamInitializing);
      break;
    case StreamProvider::StreamState::kTranscribing:
      MoveToState(SessionState::kTranscribing);
      break;
    case StreamProvider::StreamState::kComplete:
    case StreamProvider::StreamState::kFailed:
      // Post since we're resetting the stream but it's also the caller so
      // still on the stack.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(
                         [](base::WeakPtr<SessionController> this_ptr) {
                           if (!this_ptr) {
                             return;
                           }
                           this_ptr->attached_stream_provider_.reset();
                           this_ptr->MoveToState(SessionState::kInactive);
                         },
                         weak_ptr_factory_.GetWeakPtr()));
      break;
  }
}

base::CallbackListSubscription
SessionController::AddSessionStateChangedCallback(
    SessionStateChangedCallback callback) {
  return session_state_changed_callback_list_.Add(std::move(callback));
}

void SessionController::MoveToState(SessionState new_state) {
  using enum SessionState;
#if DCHECK_IS_ON()
  static const base::NoDestructor<base::StateTransitions<SessionState>>
      allowed_transitions(base::StateTransitions<SessionState>(
          {{kInactive, {kStreamInitializing}},
           {kStreamInitializing, {kInactive, kTranscribing}},
           {kTranscribing, {kInactive, kFinalizing}},
           {kFinalizing, {kInactive}}}));
  if (new_state != state_) {
    DCHECK_STATE_TRANSITION(allowed_transitions, /*old_state=*/state_,
                            /*new_state=*/new_state);
  }
#endif  // DCHECK_IS_ON()
  state_ = new_state;
  session_state_changed_callback_list_.Notify(new_state);
}

}  // namespace dictation
