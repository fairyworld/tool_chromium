// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_insights/private_insights_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_choice_service.h"
#include "components/metrics/private_metrics/private_insights/private_insights_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_insights {

class PrivateInsightsServiceTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(PrivateInsightsServiceTest,
       TriggerUploadSkipsPostingTaskWhenAlreadyRunning) {
  base::HistogramTester histogram_tester;
  TestingPrefServiceSimple local_state;
  PrivateInsightsService service(&local_state);

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

  histogram_tester.ExpectTotalCount(kUploadPendingTimeHistogram, 1);
  histogram_tester.ExpectTotalCount(kUploadTimeHistogram, 1);

  // Third call: now that task completed, should post the task again.
  service.TriggerUpload();
  histogram_tester.ExpectBucketCount(
      kTriggerUploadOutcomeHistogram,
      PrivateInsightsService::TriggerUploadOutcome::kTaskPosted, 2);
  histogram_tester.ExpectTotalCount(kTriggerUploadOutcomeHistogram, 3);
}

TEST_F(PrivateInsightsServiceTest, MetricsChoiceCoupling) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kPrivateInsightsFeature);

  TestingPrefServiceSimple local_state;
  metrics::MetricsReportingChoiceService::RegisterPrefs(local_state.registry());
  local_state.registry()->RegisterBooleanPref(
      metrics::prefs::kMetricsReportingEnabled, false);

  PrivateInsightsMetricsServiceAccessor::
      SetForceIsMetricsReportingEnabledPrefLookupForTesting(true);

  // When Init() is NOT called, UMA choice changes should be ignored.
  PrivateInsightsService uninit_service(&local_state);
  EXPECT_FALSE(uninit_service.upload_timer_.IsRunning());
  local_state.SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);
  EXPECT_FALSE(uninit_service.upload_timer_.IsRunning());

  local_state.SetBoolean(metrics::prefs::kMetricsReportingEnabled, false);

  // When Init() IS called, UMA choice changes should start/stop the service.
  PrivateInsightsService service(&local_state);
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
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kPrivateInsightsFeature);

  PrivateInsightsMetricsServiceAccessor::
      SetForceIsMetricsReportingEnabledPrefLookupForTesting(true);

  // Verify choice is respected when disabled on startup.
  {
    TestingPrefServiceSimple local_state;
    metrics::MetricsReportingChoiceService::RegisterPrefs(
        local_state.registry());
    local_state.registry()->RegisterBooleanPref(
        metrics::prefs::kMetricsReportingEnabled, false);

    PrivateInsightsService service(&local_state);
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

    PrivateInsightsService service(&local_state);
    EXPECT_FALSE(service.upload_timer_.IsRunning());

    service.Init();
    EXPECT_TRUE(service.upload_timer_.IsRunning());
  }

  PrivateInsightsMetricsServiceAccessor::
      SetForceIsMetricsReportingEnabledPrefLookupForTesting(false);
}

}  // namespace private_insights
