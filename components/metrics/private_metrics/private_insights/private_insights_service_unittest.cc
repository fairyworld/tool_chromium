// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_insights/private_insights_service.h"

#include <atomic>

#include "base/files/scoped_temp_dir.h"
#include "base/no_destructor.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_choice_service.h"
#include "components/metrics/private_metrics/private_insights/private_insights_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_insights {

class PrivateInsightsServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(tmp_profile_dir_.CreateUniqueTempDir());
    mock_run_federated_computation_call_count_ = 0;
    GetLastPopulationName() = "";
    PrivateInsightsService::SetRunFederatedComputationForTesting(
        &MockRunFederatedComputation);
    feature_list_.InitAndEnableFeatureWithParameters(
        kPrivateInsightsFeature,
        {{"fcp_server_uri", "https://example.com/test"}});
    test_shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
  }

  void TearDown() override {
    PrivateInsightsService::SetRunFederatedComputationForTesting(nullptr);
  }

  static PrivateInsightsService::FederatedComputationResult
  MockRunFederatedComputation(
      const PrivateInsightsService::FederatedComputationParams& params) {
    mock_run_federated_computation_call_count_++;
    GetLastPopulationName() = params.population_name;
    return {
        .outcome =
            PrivateInsightsService::FederatedComputationOutcome::kSuccess,
        .contributed_task_count = 1,
    };
  }

  static inline std::atomic<int> mock_run_federated_computation_call_count_ = 0;
  static std::string& GetLastPopulationName() {
    static base::NoDestructor<std::string> last_population_name;
    return *last_population_name;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir tmp_profile_dir_;
  base::test::ScopedFeatureList feature_list_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
};

TEST_F(PrivateInsightsServiceTest,
       TriggerUploadSkipsPostingTaskWhenAlreadyRunning) {
  base::HistogramTester histogram_tester;
  TestingPrefServiceSimple local_state;
  PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                 test_shared_url_loader_factory_);

  // First call: should post the task.
  service.TriggerUpload();
  histogram_tester.ExpectUniqueSample(
      kTriggerUploadOutcomeHistogram,
      PrivateInsightsService::TriggerUploadOutcome::kTaskPosted, 1);

  // Second call: while task is running, should be skipped.
  service.TriggerUpload();
  histogram_tester.ExpectBucketCount(
      kTriggerUploadOutcomeHistogram,
      PrivateInsightsService::TriggerUploadOutcome::kSkippedAlreadyRunning, 1);
  histogram_tester.ExpectTotalCount(kTriggerUploadOutcomeHistogram, 2);

  // Wait for task execution to complete.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service.is_upload_running_; }));
  EXPECT_EQ(mock_run_federated_computation_call_count_, 1);

  histogram_tester.ExpectTotalCount(kUploadPendingTimeHistogram, 1);
  histogram_tester.ExpectTotalCount(kUploadTimeHistogram, 1);
  histogram_tester.ExpectUniqueSample(
      kFederatedComputationOutcomeHistogram,
      PrivateInsightsService::FederatedComputationOutcome::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(kContributedTaskCountHistogram, 1, 1);

  // Third call: now that task completed, should post the task again.
  service.TriggerUpload();
  histogram_tester.ExpectBucketCount(
      kTriggerUploadOutcomeHistogram,
      PrivateInsightsService::TriggerUploadOutcome::kTaskPosted, 2);
  histogram_tester.ExpectTotalCount(kTriggerUploadOutcomeHistogram, 3);

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service.is_upload_running_; }));
  EXPECT_EQ(mock_run_federated_computation_call_count_, 2);
  histogram_tester.ExpectUniqueSample(
      kFederatedComputationOutcomeHistogram,
      PrivateInsightsService::FederatedComputationOutcome::kSuccess, 2);
  histogram_tester.ExpectUniqueSample(kContributedTaskCountHistogram, 1, 2);
}

TEST_F(PrivateInsightsServiceTest, MetricsChoiceCoupling) {

  TestingPrefServiceSimple local_state;
  metrics::MetricsReportingChoiceService::RegisterPrefs(local_state.registry());
  local_state.registry()->RegisterBooleanPref(
      metrics::prefs::kMetricsReportingEnabled, false);

  PrivateInsightsMetricsServiceAccessor::
      SetForceIsMetricsReportingEnabledPrefLookupForTesting(true);

  // When Init() is NOT called, UMA choice changes should be ignored.
  PrivateInsightsService uninit_service(&local_state,
                                        tmp_profile_dir_.GetPath(),
                                        test_shared_url_loader_factory_);
  EXPECT_FALSE(uninit_service.upload_timer_.IsRunning());
  local_state.SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);
  EXPECT_FALSE(uninit_service.upload_timer_.IsRunning());

  local_state.SetBoolean(metrics::prefs::kMetricsReportingEnabled, false);

  // When Init() IS called, UMA choice changes should start/stop the service.
  PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                 test_shared_url_loader_factory_);
  service.Init();
  EXPECT_FALSE(service.upload_timer_.IsRunning());

  // Enable UMA metrics reporting.
  local_state.SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);
  EXPECT_TRUE(service.upload_timer_.IsRunning());

  // Disable UMA metrics reporting.
  local_state.SetBoolean(metrics::prefs::kMetricsReportingEnabled, false);
  EXPECT_FALSE(service.upload_timer_.IsRunning());

  PrivateInsightsMetricsServiceAccessor::
      SetForceIsMetricsReportingEnabledPrefLookupForTesting(false);
}

TEST_F(PrivateInsightsServiceTest, MetricsChoiceRespectedOnStartup) {

  PrivateInsightsMetricsServiceAccessor::
      SetForceIsMetricsReportingEnabledPrefLookupForTesting(true);

  // Verify choice is respected when disabled on startup.
  {
    TestingPrefServiceSimple local_state;
    metrics::MetricsReportingChoiceService::RegisterPrefs(
        local_state.registry());
    local_state.registry()->RegisterBooleanPref(
        metrics::prefs::kMetricsReportingEnabled, false);

    PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                   test_shared_url_loader_factory_);
    EXPECT_FALSE(service.upload_timer_.IsRunning());

    service.Init();
    EXPECT_FALSE(service.upload_timer_.IsRunning());
  }

  // Verify choice is respected when enabled on startup.
  {
    TestingPrefServiceSimple local_state;
    metrics::MetricsReportingChoiceService::RegisterPrefs(
        local_state.registry());
    local_state.registry()->RegisterBooleanPref(
        metrics::prefs::kMetricsReportingEnabled, true);

    PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                   test_shared_url_loader_factory_);
    EXPECT_FALSE(service.upload_timer_.IsRunning());

    service.Init();
    EXPECT_TRUE(service.upload_timer_.IsRunning());
  }

  PrivateInsightsMetricsServiceAccessor::
      SetForceIsMetricsReportingEnabledPrefLookupForTesting(false);
}

TEST_F(PrivateInsightsServiceTest, UploadSkippedWhenServerUriEmpty) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kPrivateInsightsFeature, {{"fcp_server_uri", ""}});

  base::HistogramTester histogram_tester;
  TestingPrefServiceSimple local_state;
  PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                 test_shared_url_loader_factory_);

  service.TriggerUpload();

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service.is_upload_running_; }));
  EXPECT_EQ(mock_run_federated_computation_call_count_, 0);

  histogram_tester.ExpectTotalCount(kUploadPendingTimeHistogram, 1);
  histogram_tester.ExpectTotalCount(kUploadTimeHistogram, 0);
  histogram_tester.ExpectUniqueSample(
      kFederatedComputationOutcomeHistogram,
      PrivateInsightsService::FederatedComputationOutcome::kErrorNoServerUri,
      1);
  histogram_tester.ExpectTotalCount(kContributedTaskCountHistogram, 0);
}

TEST_F(PrivateInsightsServiceTest, PopulationNameFinchParam) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kPrivateInsightsFeature,
      {{"fcp_server_uri", "https://example.com/test"},
       {"fcp_population_name_contextual_cues", "custom_population_name"}});

  TestingPrefServiceSimple local_state;
  PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                 test_shared_url_loader_factory_);

  service.TriggerUpload();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service.is_upload_running_; }));
  EXPECT_EQ(mock_run_federated_computation_call_count_, 1);
  EXPECT_EQ(GetLastPopulationName(), "custom_population_name");
}

}  // namespace private_insights
