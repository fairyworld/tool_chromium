// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_ISOLATED_DATABASE_QUERIES_H_
#define NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_ISOLATED_DATABASE_QUERIES_H_

#include "base/strings/cstring_view.h"

namespace disk_cache_sql_queries {

namespace internal {

// Creates the resources table.
// `hash` is the 32-bit hash of a resource URL, used for fast indexing and
// lookups within the isolated database.
inline constexpr char kSharedCacheIsolatedCreateResourcesTable[] =
    "CREATE TABLE resources ("
    "hash INTEGER NOT NULL,"
    "url TEXT NOT NULL,"
    "headers BLOB NOT NULL,"
    "body BLOB NOT NULL,"
    "is_ready INTEGER NOT NULL)";

inline constexpr char kSharedCacheIsolatedCreateResourcesTableIndex[] =
    "CREATE INDEX index_resources_hash ON resources(hash)";

}  // namespace internal

enum class SharedCacheIsolatedDatabaseQuery {
  kCreateResourcesTable,
  kCreateResourcesTableIndex,
};

inline constexpr base::cstring_view GetSharedCacheIsolatedDatabaseQuery(
    SharedCacheIsolatedDatabaseQuery query) {
  switch (query) {
    case SharedCacheIsolatedDatabaseQuery::kCreateResourcesTable:
      return internal::kSharedCacheIsolatedCreateResourcesTable;
    case SharedCacheIsolatedDatabaseQuery::kCreateResourcesTableIndex:
      return internal::kSharedCacheIsolatedCreateResourcesTableIndex;
  }
}

}  // namespace disk_cache_sql_queries

#endif  // NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_ISOLATED_DATABASE_QUERIES_H_
