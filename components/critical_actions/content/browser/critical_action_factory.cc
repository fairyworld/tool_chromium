// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/content/browser/critical_action_factory.h"

#include "base/files/file_path.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/critical_actions/core/browser/critical_action_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace critical_actions {

namespace {

constexpr base::FilePath::CharType kCriticalActionsDatabaseFilename[] =
    FILE_PATH_LITERAL("CriticalActions.db");
}  // namespace

// static
CriticalActionFactory* CriticalActionFactory::GetInstance() {
  static base::NoDestructor<CriticalActionFactory> instance;
  return instance.get();
}

// static
CriticalActionService* CriticalActionFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<CriticalActionService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

CriticalActionFactory::CriticalActionFactory()
    : BrowserContextKeyedServiceFactory(
          "CriticalActionService",
          BrowserContextDependencyManager::GetInstance()) {}

CriticalActionFactory::~CriticalActionFactory() = default;

std::unique_ptr<KeyedService>
CriticalActionFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // Prevent logging and persistent storage for off-the-record/incognito
  // sessions to enforce browser privacy boundaries.
  if (context->IsOffTheRecord()) {
    return nullptr;
  }

  base::FilePath db_path =
      context->GetPath().Append(kCriticalActionsDatabaseFilename);

  // Create a sequenced background runner for SQLite database disk I/O
  // operations that block shutdown to guarantee database consistency.
  auto backend_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

  return std::make_unique<CriticalActionService>(db_path, backend_task_runner);
}

}  // namespace critical_actions
