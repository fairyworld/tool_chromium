// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_ISOLATED_DATABASE_H_
#define NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_ISOLATED_DATABASE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/pass_key.h"
#include "components/sqlite_vfs/sqlite_sandboxed_vfs.h"
#include "net/base/net_export.h"
#include "net/disk_cache/sql/sql_backend_aliases.h"
#include "sql/database.h"

namespace disk_cache {

class SqlSharedCacheIsolatedDatabaseTest;

// Manages a single SQLite database shard for a specific Network Isolation Key
// (NIK).
//
// To prevent cross-site data leaks, each NIK is assigned an isolated database
// via sqlite_vfs::SandboxedVfs. In the future, a read-only PendingFileSet
// corresponding to this database will be sent to the renderer process,
// allowing direct read access to the cached resources from the renderer.
// This class orchestrates the underlying SQLite database lifecycle, initializes
// the schema and metadata tables, and automatically razes and recreates the
// database upon NIK mismatch or schema version incompatibility.
class NET_EXPORT_PRIVATE SqlSharedCacheIsolatedDatabase {
 public:
  friend class SqlSharedCacheIsolatedDatabaseTest;

  enum class Error {
    kFailedForTesting = 1,
    kFailedToOpenVfsFileSet = 2,
    kFailedToOpenDatabase = 3,
    kFailedToRazeIncompatibleVersionDatabase = 4,
    kFailedToRazeIncompatibleNikDatabase = 5,
    kFailedToStartTransaction = 6,
    kFailedToCreateResourcesTable = 7,
    kFailedToCreateResourcesTableIndex = 8,
    kFailedToInitMetaTable = 9,
    kFailedToSetNikInMetaTable = 10,
    kFailedToCommitTransaction = 11,
  };

  SqlSharedCacheIsolatedDatabase(
      std::string nik_string,
      const base::FilePath& directory,
      SqlSharedCacheDbId shared_cache_db_id,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~SqlSharedCacheIsolatedDatabase();

  base::expected<void, Error> Init();

  void SetSimulateDbFailureForTesting(bool fail);

 private:
  class DatabaseAssets {
   public:
    static std::unique_ptr<DatabaseAssets> MaybeCreate(
        const base::FilePath& directory,
        SqlSharedCacheDbId shared_cache_db_id);

    DatabaseAssets(sqlite_vfs::SqliteVfsFileSet vfs_file_set,
                   base::PassKey<DatabaseAssets>);
    DatabaseAssets(const DatabaseAssets&) = delete;
    DatabaseAssets& operator=(const DatabaseAssets&) = delete;

    ~DatabaseAssets();

    sql::Database& db() { return db_; }
    base::FilePath GetDbVirtualFilePath() const;

   private:
    sqlite_vfs::SqliteVfsFileSet vfs_file_set_;
    sqlite_vfs::SqliteSandboxedVfsDelegate::UnregisterRunner unregister_runner_;
    sql::Database db_;
  };

  std::string nik_string_;
  std::unique_ptr<DatabaseAssets> db_assets_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  bool simulate_db_failure_ = false;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_ISOLATED_DATABASE_H_
