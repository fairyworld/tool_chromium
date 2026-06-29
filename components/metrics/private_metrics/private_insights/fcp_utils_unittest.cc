// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_insights/fcp_utils.h"

#include <atomic>

#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_insights {

TEST(CountdownLatchTest, ZeroInitialCount) {
  CountdownLatch latch(0);
  // Wait() should return immediately for count 0.
  latch.Wait();
}

TEST(CountdownLatchTest, SingleThreadCountDown) {
  CountdownLatch latch(1);
  latch.CountDown();
  latch.Wait();
}

TEST(CountdownLatchTest, MultiThreadedCountDown) {
  base::test::TaskEnvironment task_environment;

  constexpr size_t kNumTasks = 5;
  CountdownLatch latch(kNumTasks);
  std::atomic<bool> wait_completed{false};
  base::RunLoop run_loop;

  // Post a background task that calls latch.Wait().
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](CountdownLatch* l, std::atomic<bool>* completed) {
            base::ScopedAllowBaseSyncPrimitivesForTesting allow_sync;
            l->Wait();
            completed->store(true);
          },
          base::Unretained(&latch), base::Unretained(&wait_completed)),
      run_loop.QuitClosure());

  // Count down 4 times.
  for (size_t i = 0; i < kNumTasks - 1; ++i) {
    latch.CountDown();
  }

  // Verify that after 4 countdowns, the waiting task is NOT completed yet.
  EXPECT_FALSE(wait_completed.load());

  // Perform the 5th and final countdown.
  latch.CountDown();

  // Wait for the background task to complete and reply.
  run_loop.Run();

  // Verify that after the 5th countdown, the waiting task completed.
  EXPECT_TRUE(wait_completed.load());
}

}  // namespace private_insights
