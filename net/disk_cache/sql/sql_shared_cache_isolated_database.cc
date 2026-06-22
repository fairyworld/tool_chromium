// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_shared_cache_isolated_database.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/expected_macros.h"
#include "components/sqlite_vfs/client.h"
#include "components/sqlite_vfs/sqlite_sandboxed_vfs.h"
#include "components/sqlite_vfs/vfs_utils.h"
#include "net/disk_cache/sql/sql_backend_constants.h"
#include "net/disk_cache/sql/sql_shared_cache_isolated_database_queries.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/transaction.h"

using disk_cache_sql_queries::GetSharedCacheIsolatedDatabaseQuery;
using disk_cache_sql_queries::SharedCacheIsolatedDatabaseQuery;

namespace disk_cache {

namespace {

constexpr int kSharedCacheIsolatedDatabaseCurrentVersion = 1;
constexpr int kSharedCacheIsolatedDatabaseCompatibleVersion = 1;

}  // namespace

// static
std::unique_ptr<SqlSharedCacheIsolatedDatabase::DatabaseAssets>
SqlSharedCacheIsolatedDatabase::DatabaseAssets::MaybeCreate(
    const base::FilePath& directory,
    SqlSharedCacheDbId shared_cache_db_id) {
  ASSIGN_OR_RETURN(auto pending_file_set,
                   sqlite_vfs::MakePendingFileSet(
                       sqlite_vfs::Client::kSharedCacheIsolated, directory,
                       base::FilePath::FromASCII(base::StrCat(
                           {kSqlBackendSharedCacheIsolatedFileNamePrefix,
                            base::NumberToString(*shared_cache_db_id)})),
                       /*single_connection=*/false,
                       /*journal_mode_wal=*/false),
                   [](auto) { return nullptr; });
  ASSIGN_OR_RETURN(auto vfs_file_set,
                   sqlite_vfs::SqliteVfsFileSet::Bind(
                       sqlite_vfs::Client::kSharedCacheIsolated,
                       std::move(pending_file_set)),
                   [] { return nullptr; });
  return std::make_unique<DatabaseAssets>(std::move(vfs_file_set),
                                          base::PassKey<DatabaseAssets>());
}

SqlSharedCacheIsolatedDatabase::DatabaseAssets::DatabaseAssets(
    sqlite_vfs::SqliteVfsFileSet vfs_file_set,
    base::PassKey<DatabaseAssets>)
    : vfs_file_set_(std::move(vfs_file_set)),
      unregister_runner_(sqlite_vfs::SqliteSandboxedVfsDelegate::GetInstance()
                             ->RegisterSandboxedFiles(vfs_file_set_)),
      db_(sql::DatabaseOptions()
              .set_read_only(vfs_file_set_.read_only())
              .set_exclusive_locking(vfs_file_set_.is_single_connection())
              .set_wal_mode(vfs_file_set_.wal_mode())
              .set_vfs_name_discouraged(
                  sqlite_vfs::SqliteSandboxedVfsDelegate::kSqliteVfsName)
              .set_mmap_enabled(false),
          sql::Database::Tag("SharedCacheIsolated")) {}

SqlSharedCacheIsolatedDatabase::DatabaseAssets::~DatabaseAssets() = default;

base::FilePath
SqlSharedCacheIsolatedDatabase::DatabaseAssets::GetDbVirtualFilePath() const {
  return vfs_file_set_.GetDbVirtualFilePath();
}

SqlSharedCacheIsolatedDatabase::SqlSharedCacheIsolatedDatabase(
    std::string nik_string,
    const base::FilePath& directory,
    SqlSharedCacheDbId shared_cache_db_id,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : nik_string_(std::move(nik_string)),
      db_assets_(DatabaseAssets::MaybeCreate(directory, shared_cache_db_id)),
      task_runner_(std::move(task_runner)) {}

SqlSharedCacheIsolatedDatabase::~SqlSharedCacheIsolatedDatabase() = default;

void SqlSharedCacheIsolatedDatabase::SetSimulateDbFailureForTesting(bool fail) {
  simulate_db_failure_ = fail;
}

base::expected<void, SqlSharedCacheIsolatedDatabase::Error>
SqlSharedCacheIsolatedDatabase::Init() {
  if (simulate_db_failure_) {
    return base::unexpected(Error::kFailedForTesting);
  }
  if (!db_assets_) {
    return base::unexpected(Error::kFailedToOpenVfsFileSet);
  }
  sql::Database& db = db_assets_->db();
  if (db.is_open()) {
    return base::ok();
  }
  if (!db.Open(db_assets_->GetDbVirtualFilePath())) {
    return base::unexpected(Error::kFailedToOpenDatabase);
  }

  if (sql::MetaTable::RazeIfIncompatible(
          &db, kSharedCacheIsolatedDatabaseCompatibleVersion,
          kSharedCacheIsolatedDatabaseCurrentVersion) ==
      sql::RazeIfIncompatibleResult::kFailed) {
    return base::unexpected(Error::kFailedToRazeIncompatibleVersionDatabase);
  }

  sql::MetaTable meta_table;
  if (sql::MetaTable::DoesTableExist(&db)) {
    std::string saved_nik;
    if (!meta_table.Init(&db, kSharedCacheIsolatedDatabaseCurrentVersion,
                         kSharedCacheIsolatedDatabaseCompatibleVersion) ||
        !meta_table.GetValue(kSqlBackendSharedCacheIsolatedMetaTableKeyNik,
                             &saved_nik) ||
        saved_nik != nik_string_) {
      if (!db.Raze()) {
        return base::unexpected(Error::kFailedToRazeIncompatibleNikDatabase);
      }
      meta_table.Reset();
    } else {
      return base::ok();
    }
  }

  sql::Transaction transaction(&db);
  if (!transaction.Begin()) {
    return base::unexpected(Error::kFailedToStartTransaction);
  }

  if (!db.Execute(GetSharedCacheIsolatedDatabaseQuery(
          SharedCacheIsolatedDatabaseQuery::kCreateResourcesTable))) {
    return base::unexpected(Error::kFailedToCreateResourcesTable);
  }

  if (!db.Execute(GetSharedCacheIsolatedDatabaseQuery(
          SharedCacheIsolatedDatabaseQuery::kCreateResourcesTableIndex))) {
    return base::unexpected(Error::kFailedToCreateResourcesTableIndex);
  }

  if (!meta_table.Init(&db, kSharedCacheIsolatedDatabaseCurrentVersion,
                       kSharedCacheIsolatedDatabaseCompatibleVersion)) {
    return base::unexpected(Error::kFailedToInitMetaTable);
  }

  if (!meta_table.SetValue(kSqlBackendSharedCacheIsolatedMetaTableKeyNik,
                           nik_string_)) {
    return base::unexpected(Error::kFailedToSetNikInMetaTable);
  }

  if (!transaction.Commit()) {
    return base::unexpected(Error::kFailedToCommitTransaction);
  }

  return base::ok();
}

}  // namespace disk_cache
