// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_service.h"

#include <memory>
#include <optional>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace critical_actions {

class CriticalActionServiceTest : public testing::Test {
 public:
  CriticalActionServiceTest() = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("TestCriticalActions.db");
    backend_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
    service_ =
        std::make_unique<CriticalActionService>(db_path_, backend_task_runner_);
  }

  void TearDown() override {
    if (service_) {
      service_->Shutdown();
      service_.reset();
    }
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  std::unique_ptr<CriticalActionService> service_;
};

// Verifies end-to-end integration and that callbacks run on the main thread.
TEST_F(CriticalActionServiceTest, AddAndGetActionRunsOnMainThread) {
  CriticalActionEntry entry;
  entry.critical_action_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry.timestamp = base::Time::Now();
  entry.visit_id = base::RandIntInclusive(1, 1000000);
  entry.conversation_id = "test-conv-1";
  entry.actor_task_id = "test-task-1";
  entry.action_type = ActionType::kCredentialAccess;
  entry.url = GURL("https://example.com/oauth");
  entry.metadata = "{\"scopes\": [\"profile\"]}";

  // AddCriticalAction does not have a callback (it is asynchronous
  // fire-and-forget). Because backend operations run on a sequenced task
  // runner, the subsequent GetCriticalAction is guaranteed to run after
  // AddCriticalAction completes.
  service_->AddCriticalAction(entry);

  base::test::TestFuture<std::optional<CriticalActionEntry>> get_future;
  scoped_refptr<base::SequencedTaskRunner> original_runner =
      base::SequencedTaskRunner::GetCurrentDefault();

  service_->GetCriticalAction(
      entry.critical_action_id,
      base::BindOnce(
          [](scoped_refptr<base::SequencedTaskRunner> original_runner,
             base::OnceCallback<void(std::optional<CriticalActionEntry>)>
                 callback,
             std::optional<CriticalActionEntry> retrieved) {
            EXPECT_TRUE(original_runner->RunsTasksInCurrentSequence());
            std::move(callback).Run(retrieved);
          },
          original_runner, get_future.GetCallback()));
  auto retrieved = get_future.Get();
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(*retrieved, entry);
}

// Verifies that service APIs behave gracefully after Service Shutdown.
TEST_F(CriticalActionServiceTest, CallsAfterShutdownGracefullyFail) {
  service_->Shutdown();

  CriticalActionEntry entry;
  entry.critical_action_id = "after-shutdown";
  entry.action_type = ActionType::kFormFill;

  // The following calls should be safe no-ops and not crash.
  service_->AddCriticalAction(entry);
  service_->DeleteCriticalAction("after-shutdown");
  service_->DeleteCriticalActionsInTimeRange(base::Time(), base::Time());

  base::test::TestFuture<std::optional<CriticalActionEntry>> get_future;
  service_->GetCriticalAction("after-shutdown", get_future.GetCallback());
  EXPECT_FALSE(get_future.Get().has_value());

  task_environment_.RunUntilIdle();
}

}  // namespace critical_actions
