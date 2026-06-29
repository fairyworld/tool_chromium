// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_FCP_UTILS_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_FCP_UTILS_H_

#include <atomic>
#include <cstddef>

#include "base/synchronization/waitable_event.h"

namespace private_insights {

// Thread-safe synchronization primitive that allows threads to wait until a
// set of operations decrement the counter to zero.
//
// WHY THIS IS NEEDED:
// The external FCP library interface (`fcp::client::http::HttpClient`) imposes
// a synchronous `PerformRequests` contract, requiring the caller thread to
// block until a batch of HTTP requests completes. Conversely, Chromium's
// network stack (`SharedURLLoaderFactory`) executes requests asynchronously on
// Chrome's UI thread. `CountdownLatch` bridges this synchronous-to-asynchronous
// boundary by allowing the calling thread to block on `Wait()` until all posted
// network request tasks complete on the UI thread and call `CountDown()`.
class CountdownLatch {
 public:
  // Initializes the latch with `count`. If `count` is 0, the latch is
  // initialized in a signaled state so `Wait()` will return immediately.
  explicit CountdownLatch(size_t count);

  CountdownLatch(const CountdownLatch&) = delete;
  CountdownLatch& operator=(const CountdownLatch&) = delete;
  CountdownLatch(CountdownLatch&&) = delete;
  CountdownLatch& operator=(CountdownLatch&&) = delete;

  ~CountdownLatch();

  // Decrements the counter by 1. When the counter reaches 0, signals the
  // underlying event to unblock all threads waiting in `Wait()`.
  void CountDown();

  // Blocks the calling thread until `CountDown()` has been called `count` times
  // and the counter reaches 0.
  void Wait();

 private:
  std::atomic<size_t> count_;
  base::WaitableEvent done_event_;
};

}  // namespace private_insights

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_FCP_UTILS_H_
