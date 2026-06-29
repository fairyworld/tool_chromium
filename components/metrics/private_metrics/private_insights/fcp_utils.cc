// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_insights/fcp_utils.h"

namespace private_insights {

CountdownLatch::CountdownLatch(size_t count)
    : count_(count),
      done_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                  base::WaitableEvent::InitialState::NOT_SIGNALED) {
  if (count_ == 0) {
    done_event_.Signal();
  }
}

CountdownLatch::~CountdownLatch() = default;

void CountdownLatch::CountDown() {
  if (--count_ == 0) {
    done_event_.Signal();
  }
}

void CountdownLatch::Wait() {
  done_event_.Wait();
}

}  // namespace private_insights
