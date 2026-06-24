// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/declarative_performance_observer/declarative_performance_observer_store.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class DeclarativePerformanceObserverStoreTest : public testing::Test {
 public:
  DeclarativePerformanceObserverStoreTest() = default;
  ~DeclarativePerformanceObserverStoreTest() override = default;

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  std::unique_ptr<DeclarativePerformanceObserverStore> CreateStore() {
    base::RunLoop run_loop;
    auto store = std::make_unique<DeclarativePerformanceObserverStore>(
        temp_dir_.GetPath().AppendASCII("TestStore"), nullptr,
        run_loop.QuitClosure());
    run_loop.Run();
    return store;
  }

  std::unique_ptr<DeclarativePerformanceObserverStore> CreateStoreInMemory() {
    base::RunLoop run_loop;
    auto store = std::make_unique<DeclarativePerformanceObserverStore>(
        base::FilePath(), nullptr, run_loop.QuitClosure());
    run_loop.Run();
    return store;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(DeclarativePerformanceObserverStoreTest, StoragePolicyOptInAndOut) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com/"));
  auto store = CreateStore();

  EXPECT_FALSE(store->HasEarlyFailurePolicy(kOrigin));

  base::RunLoop run_loop1;
  store->SetEarlyFailurePolicy(kOrigin, true, run_loop1.QuitClosure());
  run_loop1.Run();

  EXPECT_TRUE(store->HasEarlyFailurePolicy(kOrigin));

  base::RunLoop run_loop2;
  store->SetEarlyFailurePolicy(kOrigin, false, run_loop2.QuitClosure());
  run_loop2.Run();

  EXPECT_FALSE(store->HasEarlyFailurePolicy(kOrigin));
}

TEST_F(DeclarativePerformanceObserverStoreTest, IncognitoModeInMemoryOnly) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com/"));
  auto store = CreateStoreInMemory();

  base::RunLoop run_loop1;
  store->SetEarlyFailurePolicy(kOrigin, true, run_loop1.QuitClosure());
  run_loop1.Run();

  EXPECT_TRUE(store->HasEarlyFailurePolicy(kOrigin));
}

TEST_F(DeclarativePerformanceObserverStoreTest, PersistsAcrossRestarts) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com/"));
  base::FilePath db_path = temp_dir_.GetPath().AppendASCII("TestStore");

  // 1. Create store and set policy.
  {
    base::RunLoop run_loop;
    auto store = std::make_unique<DeclarativePerformanceObserverStore>(
        db_path, nullptr, run_loop.QuitClosure());
    run_loop.Run();

    base::RunLoop run_loop2;
    store->SetEarlyFailurePolicy(kOrigin, true, run_loop2.QuitClosure());
    run_loop2.Run();

    EXPECT_TRUE(store->HasEarlyFailurePolicy(kOrigin));

    base::RunLoop run_loop3;
    store->Close(run_loop3.QuitClosure());
    run_loop3.Run();
  }

  // 2. Re-create store with the same db_path and check persistence.
  {
    base::RunLoop run_loop;
    auto store = std::make_unique<DeclarativePerformanceObserverStore>(
        db_path, nullptr, run_loop.QuitClosure());
    run_loop.Run();

    EXPECT_TRUE(store->HasEarlyFailurePolicy(kOrigin));
  }
}

TEST_F(DeclarativePerformanceObserverStoreTest, RaceConditionDuringLoad) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com/"));
  base::FilePath db_path = temp_dir_.GetPath().AppendASCII("TestStore2");

  // Pre-populate DB with the policy.
  {
    base::RunLoop run_loop;
    auto store = std::make_unique<DeclarativePerformanceObserverStore>(
        db_path, nullptr, run_loop.QuitClosure());
    run_loop.Run();

    base::RunLoop run_loop2;
    store->SetEarlyFailurePolicy(kOrigin, true, run_loop2.QuitClosure());
    run_loop2.Run();

    base::RunLoop run_loop3;
    store->Close(run_loop3.QuitClosure());
    run_loop3.Run();
  }

  // Start new store but DON'T wait for loading to complete yet.
  auto store = std::make_unique<DeclarativePerformanceObserverStore>(
      db_path, nullptr, base::DoNothing());

  // Instantly disable the policy before loading completes.
  base::RunLoop run_loop_set;
  store->SetEarlyFailurePolicy(kOrigin, false, run_loop_set.QuitClosure());

  // Wait for the SetEarlyFailurePolicy DB task (and the preceding load task) to
  // finish.
  run_loop_set.Run();

  // Now, cache should remain false even after the Load task completes,
  // preventing the old load results (which has kOrigin = true) from overwriting
  // it.
  EXPECT_FALSE(store->HasEarlyFailurePolicy(kOrigin));
}

TEST_F(DeclarativePerformanceObserverStoreTest, OpaqueOriginIgnored) {
  const url::Origin kOpaqueOrigin = url::Origin();
  ASSERT_TRUE(kOpaqueOrigin.opaque());
  auto store = CreateStore();

  base::RunLoop run_loop;
  store->SetEarlyFailurePolicy(kOpaqueOrigin, true, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(store->HasEarlyFailurePolicy(kOpaqueOrigin));
}

TEST_F(DeclarativePerformanceObserverStoreTest, DiskPersistence) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com/"));
  auto store = CreateStore();
  base::HistogramTester histogram_tester;

  base::DictValue report;
  report.Set("test_key", "test_value");
  base::RunLoop run_loop_store;
  store->StoreEarlyFailureReport(kOrigin, report.Clone(),
                                 run_loop_store.QuitClosure());
  run_loop_store.Run();

  histogram_tester.ExpectUniqueSample(
      "Storage.DeclarativePerformanceObserver.StoreReportResult",
      /*sample=*/0, /*expected_bucket_count=*/1);

  base::ListValue taken_reports;
  base::RunLoop run_loop_take;
  store->TakeEarlyFailureReports(
      kOrigin,
      base::BindOnce(
          [](base::OnceClosure quit, base::ListValue* out_taken_reports,
             base::ListValue result) {
            *out_taken_reports = std::move(result);
            std::move(quit).Run();
          },
          run_loop_take.QuitClosure(), &taken_reports));
  run_loop_take.Run();

  ASSERT_EQ(taken_reports.size(), 1u);
  const base::DictValue* dict = taken_reports[0].GetIfDict();
  ASSERT_TRUE(dict);
  EXPECT_EQ(*(dict->FindString("test_key")), "test_value");

  // Taking reports a second time should return an empty list:
  base::ListValue taken_reports_empty;
  base::RunLoop run_loop_take2;
  store->TakeEarlyFailureReports(
      kOrigin,
      base::BindOnce(
          [](base::OnceClosure quit, base::ListValue* out_taken_reports,
             base::ListValue result) {
            *out_taken_reports = std::move(result);
            std::move(quit).Run();
          },
          run_loop_take2.QuitClosure(), &taken_reports_empty));
  run_loop_take2.Run();

  EXPECT_TRUE(taken_reports_empty.empty());
}

TEST_F(DeclarativePerformanceObserverStoreTest, DatabaseSchemaConfigured) {
  auto store = CreateStore();
  bool table_ok = false;
  bool index_ok = false;
  base::RunLoop run_loop;
  store->CheckSchemaForTesting(  // IN-TEST
      base::BindOnce(
          [](base::OnceClosure quit, bool* out_table, bool* out_index,
             bool table_res, bool index_res) {
            *out_table = table_res;
            *out_index = index_res;
            std::move(quit).Run();
          },
          run_loop.QuitClosure(), &table_ok, &index_ok));
  run_loop.Run();
  EXPECT_TRUE(table_ok);
  EXPECT_TRUE(index_ok);
}

}  // namespace content
