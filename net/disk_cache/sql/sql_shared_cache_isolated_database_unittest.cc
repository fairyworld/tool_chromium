// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_shared_cache_isolated_database.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {

class SqlSharedCacheIsolatedDatabaseTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

TEST_F(SqlSharedCacheIsolatedDatabaseTest, InitSuccess) {
  SqlSharedCacheDbId db_id(1);
  SqlSharedCacheIsolatedDatabase db("nik", temp_dir_.GetPath(), db_id,
                                    task_runner_);
  EXPECT_TRUE(db.Init().has_value());

  // Verify that the isolated database file is created successfully.
  base::FilePath expected_file = temp_dir_.GetPath().AppendASCII("shared_1.db");
  EXPECT_TRUE(base::PathExists(expected_file));
}

TEST_F(SqlSharedCacheIsolatedDatabaseTest, InitFailureForTesting) {
  SqlSharedCacheDbId db_id(1);
  SqlSharedCacheIsolatedDatabase db("nik", temp_dir_.GetPath(), db_id,
                                    task_runner_);
  db.SetSimulateDbFailureForTesting(true);
  EXPECT_EQ(db.Init().error(),
            SqlSharedCacheIsolatedDatabase::Error::kFailedForTesting);
}

TEST_F(SqlSharedCacheIsolatedDatabaseTest, InitFailedToOpenVfsFileSet) {
  constexpr std::string_view kNik = "nik";
  SqlSharedCacheDbId db_id(1);

  {
    SqlSharedCacheIsolatedDatabase db(std::string(kNik), temp_dir_.GetPath(),
                                      db_id, task_runner_);
    EXPECT_TRUE(db.Init().has_value());
  }

  base::FilePath db_path = temp_dir_.GetPath().AppendASCII("shared_1.db");
  ASSERT_TRUE(base::MakeFileUnwritable(db_path));

  {
    SqlSharedCacheIsolatedDatabase db(std::string(kNik), temp_dir_.GetPath(),
                                      db_id, task_runner_);
    EXPECT_EQ(db.Init().error(),
              SqlSharedCacheIsolatedDatabase::Error::kFailedToOpenVfsFileSet);
  }
}

TEST_F(SqlSharedCacheIsolatedDatabaseTest, InitializeAndNikMismatch) {
  constexpr std::string_view kNik1 = "nik1";
  constexpr std::string_view kNik2 = "nik2";
  SqlSharedCacheDbId db_id(1);

  {
    SqlSharedCacheIsolatedDatabase db(std::string(kNik1), temp_dir_.GetPath(),
                                      db_id, task_runner_);
    EXPECT_TRUE(db.Init().has_value());
  }

  {
    // Initialize with the same nik
    SqlSharedCacheIsolatedDatabase db(std::string(kNik1), temp_dir_.GetPath(),
                                      db_id, task_runner_);
    EXPECT_TRUE(db.Init().has_value());
  }

  {
    // Initialize with a different nik. It should wipe the database.
    // TODO(crbug.com/473666511): Check if the data was wiped.
    SqlSharedCacheIsolatedDatabase db(std::string(kNik2), temp_dir_.GetPath(),
                                      db_id, task_runner_);
    EXPECT_TRUE(db.Init().has_value());
  }
}

}  // namespace disk_cache
