// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/declarative_performance_observer/declarative_performance_observer_store.h"

#include "base/functional/bind.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "sql/database.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "url/gurl.h"

namespace content {

class DeclarativePerformanceObserverStore::Backend
    : public base::RefCountedDeleteOnSequence<
          DeclarativePerformanceObserverStore::Backend> {
 public:
  Backend(scoped_refptr<base::SequencedTaskRunner> db_task_runner,
          const base::FilePath& db_path)
      : base::RefCountedDeleteOnSequence<
            DeclarativePerformanceObserverStore::Backend>(db_task_runner),
        db_path_(db_path) {
    DETACH_FROM_SEQUENCE(db_sequence_checker_);
  }

  void LoadPoliciesOnDbSequence(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      base::OnceCallback<void(std::vector<url::Origin>)> on_loaded_callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(db_sequence_checker_);
    if (!InitOnDbSequence()) {
      ui_task_runner->PostTask(FROM_HERE,
                               base::BindOnce(std::move(on_loaded_callback),
                                              std::vector<url::Origin>()));
      return;
    }

    std::vector<url::Origin> loaded;
    sql::Statement statement(
        db_->GetUniqueStatement("SELECT origin, capture_early_failures FROM "
                                "declarative_performance_observer_policies"));
    while (statement.Step()) {
      if (statement.ColumnBool(1)) {
        loaded.emplace_back(
            url::Origin::Create(GURL(statement.ColumnString(0))));
      }
    }

    ui_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(on_loaded_callback), std::move(loaded)));
  }

  void SetEarlyFailurePolicyOnDbSequence(url::Origin origin, bool enabled) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(db_sequence_checker_);
    if (!InitOnDbSequence()) {
      return;
    }

    if (enabled) {
      sql::Statement statement(db_->GetCachedStatement(
          SQL_FROM_HERE,
          "INSERT OR REPLACE INTO declarative_performance_observer_policies "
          "(origin, capture_early_failures) VALUES (?, 1)"));
      statement.BindString(0, origin.Serialize());
      statement.Run();
    } else {
      sql::Statement statement(db_->GetCachedStatement(
          SQL_FROM_HERE,
          "DELETE FROM declarative_performance_observer_policies WHERE origin "
          "= ?"));
      statement.BindString(0, origin.Serialize());
      statement.Run();
    }
  }

  void CloseOnDbSequence() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(db_sequence_checker_);
    if (db_ && db_->is_open()) {
      db_->Close();
    }
    db_.reset();
  }

 private:
  friend class base::RefCountedDeleteOnSequence<
      DeclarativePerformanceObserverStore::Backend>;
  friend class base::DeleteHelper<DeclarativePerformanceObserverStore::Backend>;

  ~Backend() { DCHECK_CALLED_ON_VALID_SEQUENCE(db_sequence_checker_); }

  void DatabaseErrorCallback(int extended_error, sql::Statement* stmt) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(db_sequence_checker_);
    if (sql::IsErrorCatastrophic(extended_error)) {
      if (db_ && db_->is_open() && !db_path_.empty()) {
        std::ignore = db_->RazeAndPoison();
      }
    }
  }

  bool InitOnDbSequence() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(db_sequence_checker_);
    if (db_ && db_->is_open()) {
      return true;
    }

    db_ = std::make_unique<sql::Database>(
        sql::DatabaseOptions().set_page_size(4096).set_cache_size(128),
        sql::Database::Tag("DeclarativePerformanceObserver"));
    db_->set_error_callback(base::BindRepeating(&Backend::DatabaseErrorCallback,
                                                base::Unretained(this)));

    if (db_path_.empty()) {
      if (!db_->OpenInMemory()) {
        return false;
      }
    } else {
      if (!db_->Open(db_path_)) {
        return false;
      }
    }

    sql::MetaTable meta_table;
    static constexpr int kVersionNumber = 1;
    static constexpr int kCompatibleVersionNumber = 1;
    if (!meta_table.Init(db_.get(), kVersionNumber, kCompatibleVersionNumber)) {
      return false;
    }

    static constexpr char kCreatePoliciesTable[] =
        "CREATE TABLE IF NOT EXISTS declarative_performance_observer_policies ("
        "origin TEXT PRIMARY KEY NOT NULL, "
        "capture_early_failures BOOLEAN NOT NULL)";
    if (!db_->Execute(kCreatePoliciesTable)) {
      return false;
    }

    return true;
  }

  base::FilePath db_path_;
  std::unique_ptr<sql::Database> db_;
  SEQUENCE_CHECKER(db_sequence_checker_);
};

DeclarativePerformanceObserverStore::DeclarativePerformanceObserverStore(
    const base::FilePath& db_path,
    scoped_refptr<base::SequencedTaskRunner> db_task_runner,
    base::OnceClosure on_loaded_callback)
    : db_task_runner_(
          db_task_runner
              ? db_task_runner
              : base::ThreadPool::CreateSequencedTaskRunner(
                    {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                     base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      backend_(base::MakeRefCounted<Backend>(db_task_runner_, db_path)) {
  db_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Backend::LoadPoliciesOnDbSequence, backend_,
                     base::SequencedTaskRunner::GetCurrentDefault(),
                     base::BindOnce(&DeclarativePerformanceObserverStore::
                                        OnPoliciesLoadedOnUISequence,
                                    weak_factory_.GetWeakPtr(),
                                    std::move(on_loaded_callback))));
}

DeclarativePerformanceObserverStore::~DeclarativePerformanceObserverStore() =
    default;

void DeclarativePerformanceObserverStore::OnPoliciesLoadedOnUISequence(
    base::OnceClosure on_loaded_callback,
    std::vector<url::Origin> loaded) {
  std::erase_if(loaded, [this](const url::Origin& origin) {
    return modified_during_load_.contains(origin);
  });
  cached_policies_.insert(loaded.begin(), loaded.end());
  loaded_ = true;
  modified_during_load_.clear();
  std::move(on_loaded_callback).Run();
}

void DeclarativePerformanceObserverStore::SetEarlyFailurePolicy(
    const url::Origin& origin,
    bool enabled,
    base::OnceClosure callback) {
  if (origin.opaque()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }
  if (!loaded_) {
    modified_during_load_.insert(origin);
  }
  if (enabled) {
    cached_policies_.insert(origin);
  } else {
    cached_policies_.erase(origin);
  }
  db_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&Backend::SetEarlyFailurePolicyOnDbSequence, backend_,
                     origin, enabled),
      std::move(callback));
}

bool DeclarativePerformanceObserverStore::HasEarlyFailurePolicy(
    const url::Origin& origin) {
  return cached_policies_.contains(origin);
}

void DeclarativePerformanceObserverStore::Close(base::OnceClosure callback) {
  db_task_runner_->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&Backend::CloseOnDbSequence, backend_),
      std::move(callback));
}

}  // namespace content
