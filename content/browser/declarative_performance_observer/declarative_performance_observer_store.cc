// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/declarative_performance_observer/declarative_performance_observer_store.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "sql/database.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/gurl.h"

namespace content {

namespace {

constexpr char kReportsTableName[] = "declarative_performance_observer_reports";
constexpr char kReportsIndexName[] = "idx_reports_origin";

constexpr char kHistogramPrefix[] = "Storage.DeclarativePerformanceObserver.";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(DeclarativePerformanceObserverStoreReportResult)
enum class StoreReportResult {
  kSuccess = 0,
  kFailedDbInit = 1,
  kFailedJsonWrite = 2,
  kFailedSqlRun = 3,
  kMaxValue = kFailedSqlRun,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml)

}  // namespace

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

  void StoreEarlyFailureReportOnDbSequence(url::Origin origin,
                                           base::DictValue report) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(db_sequence_checker_);
    if (!InitOnDbSequence()) {
      base::UmaHistogramEnumeration(
          base::StrCat({kHistogramPrefix, "StoreReportResult"}),
          StoreReportResult::kFailedDbInit);
      return;
    }

    std::string payload;
    bool write_success = base::JSONWriter::Write(report, &payload);
    if (!write_success) {
      base::UmaHistogramEnumeration(
          base::StrCat({kHistogramPrefix, "StoreReportResult"}),
          StoreReportResult::kFailedJsonWrite);
      return;
    }

    sql::Statement statement(db_->GetUniqueStatement(
        "INSERT INTO declarative_performance_observer_reports "
        "(origin, payload, created_at) VALUES (?, ?, ?)"));
    statement.BindString(0, origin.Serialize());
    statement.BindString(1, payload);
    statement.BindInt64(
        2, base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

    if (!statement.Run()) {
      base::UmaHistogramEnumeration(
          base::StrCat({kHistogramPrefix, "StoreReportResult"}),
          StoreReportResult::kFailedSqlRun);
      return;
    }

    base::UmaHistogramEnumeration(
        base::StrCat({kHistogramPrefix, "StoreReportResult"}),
        StoreReportResult::kSuccess);
  }

  void TakeEarlyFailureReportsOnDbSequence(
      url::Origin origin,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      base::OnceCallback<void(base::ListValue)> callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(db_sequence_checker_);
    base::ListValue reports;
    if (!InitOnDbSequence()) {
      ui_task_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::move(reports)));
      return;
    }

    sql::Transaction transaction(db_.get());
    if (!transaction.Begin()) {
      ui_task_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::move(reports)));
      return;
    }

    sql::Statement statement(db_->GetCachedStatement(
        SQL_FROM_HERE,
        "SELECT payload FROM declarative_performance_observer_reports WHERE "
        "origin = ? ORDER BY id ASC"));
    statement.BindString(0, origin.Serialize());
    while (statement.Step()) {
      std::optional<base::Value> value = base::JSONReader::Read(
          statement.ColumnStringView(0), base::JSON_PARSE_RFC);
      if (value && value->is_dict()) {
        reports.Append(std::move(*value));
      }
    }

    sql::Statement delete_statement(db_->GetUniqueStatement(
        "DELETE FROM declarative_performance_observer_reports WHERE origin = "
        "?"));
    delete_statement.BindString(0, origin.Serialize());

    if (delete_statement.Run() && transaction.Commit()) {
      ui_task_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::move(reports)));
    } else {
      ui_task_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::ListValue()));
    }
  }

  void CloseOnDbSequence() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(db_sequence_checker_);
    if (db_ && db_->is_open()) {
      db_->Close();
    }
    db_.reset();
  }

  void CheckSchemaOnDbSequence(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      base::OnceCallback<void(bool, bool)> callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(db_sequence_checker_);
    if (!InitOnDbSequence()) {
      ui_task_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), false, false));
      return;
    }
    bool table_ok = db_->DoesTableExist(kReportsTableName);
    bool index_ok = db_->DoesIndexExist(kReportsIndexName);
    ui_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), table_ok, index_ok));
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

    static constexpr char kCreateReportsTable[] =
        "CREATE TABLE IF NOT EXISTS declarative_performance_observer_reports ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "origin TEXT NOT NULL, "
        "payload TEXT NOT NULL, "
        "created_at INTEGER NOT NULL)";
    if (!db_->Execute(kCreateReportsTable)) {
      return false;
    }

    static constexpr char kCreateReportsIndex[] =
        "CREATE INDEX IF NOT EXISTS idx_reports_origin ON "
        "declarative_performance_observer_reports(origin)";
    if (!db_->Execute(kCreateReportsIndex)) {
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

void DeclarativePerformanceObserverStore::StoreEarlyFailureReport(
    const url::Origin& origin,
    base::DictValue report,
    base::OnceClosure callback) {
  db_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&Backend::StoreEarlyFailureReportOnDbSequence, backend_,
                     origin, std::move(report)),
      std::move(callback));
}

void DeclarativePerformanceObserverStore::TakeEarlyFailureReports(
    const url::Origin& origin,
    base::OnceCallback<void(base::ListValue)> callback) {
  db_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Backend::TakeEarlyFailureReportsOnDbSequence, backend_,
                     origin, base::SequencedTaskRunner::GetCurrentDefault(),
                     std::move(callback)));
}

void DeclarativePerformanceObserverStore::Close(base::OnceClosure callback) {
  db_task_runner_->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&Backend::CloseOnDbSequence, backend_),
      std::move(callback));
}

void DeclarativePerformanceObserverStore::CheckSchemaForTesting(  // IN-TEST
    base::OnceCallback<void(bool, bool)> callback) {
  db_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Backend::CheckSchemaOnDbSequence, backend_,
                                base::SequencedTaskRunner::GetCurrentDefault(),
                                std::move(callback)));
}

}  // namespace content
