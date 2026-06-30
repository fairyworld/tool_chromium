// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_SERVICE_H_
#define COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_SERVICE_H_

#include <optional>
#include <string_view>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "components/keyed_service/core/keyed_service.h"

namespace critical_actions {

class CriticalActionBackend;

// UI thread service for recording and retrieving critical action history.
class CriticalActionService : public KeyedService {
 public:
  CriticalActionService(
      const base::FilePath& db_path,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner);
  CriticalActionService(const CriticalActionService&) = delete;
  CriticalActionService& operator=(const CriticalActionService&) = delete;
  ~CriticalActionService() override;

  // KeyedService:
  void Shutdown() override;

  // UI thread entry point to add a new critical action.
  void AddCriticalAction(const CriticalActionEntry& entry);

  // UI thread entry point to retrieve a critical action record by ID.
  void GetCriticalAction(
      std::string_view critical_action_id,
      base::OnceCallback<void(std::optional<CriticalActionEntry>)> callback);

  // UI thread entry point to delete a critical action record by ID.
  void DeleteCriticalAction(std::string_view critical_action_id);

  // UI thread entry point to delete all critical action records in given range.
  void DeleteCriticalActionsInTimeRange(base::Time start_time,
                                        base::Time end_time);

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  base::SequenceBound<CriticalActionBackend> backend_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace critical_actions

#endif  // COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_SERVICE_H_
