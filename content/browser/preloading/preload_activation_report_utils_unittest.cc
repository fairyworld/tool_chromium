// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preload_activation_report_utils.h"

#include "base/test/scoped_feature_list.h"
#include "content/public/common/content_features.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/origin_trials/scoped_test_origin_trial_policy.h"

namespace content {

namespace {

constexpr char kTestUrl[] = "https://example.com";

// Generated tokens for https://example.com expiring at 2000000000.
constexpr char kPrefetchToken[] =
    "Awg6VP+CNO7gn+pCQ0nDeDbtCCrmdWbpPtSmfA84DlezOFpFPxNdR5vlj7w+"
    "rNW3yJd4kPiBcNjcBLymrMuOqw8AAABieyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLmNvbT"
    "o0NDMiLCAiZmVhdHVyZSI6ICJQcmVmZXRjaEFjdGl2YXRpb25CZWFjb24iLCAiZXhwaXJ5Ijog"
    "MjAwMDAwMDAwMH0=";

constexpr char kPrerenderToken[] =
    "A/zN3029S5g1Wcv3oUJXAE7JZfpgqd5muGpXsm69MUMnrG9JzfQArCj/9/"
    "JRETAifoNT9dGK2XFoF2nxfdwMYQ0AAABjeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLmNv"
    "bTo0NDMiLCAiZmVhdHVyZSI6ICJQcmVyZW5kZXJBY3RpdmF0aW9uQmVhY29uIiwgImV4cGlyeS"
    "I6IDIwMDAwMDAwMDB9";

}  // namespace

class PreloadActivationReportUtilsTest : public testing::Test {
 public:
  PreloadActivationReportUtilsTest() = default;

 private:
  blink::ScopedTestOriginTrialPolicy origin_trial_policy_;
};

TEST_F(PreloadActivationReportUtilsTest,
       PrefetchActivationBeacon_DefaultDisabled) {
  GURL url(kTestUrl);
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");

  // No trial token -> disabled.
  EXPECT_FALSE(IsPrefetchActivationBeaconEnabled(url, headers.get()));

  // With trial token -> enabled.
  headers->AddHeader("Origin-Trial", kPrefetchToken);
  EXPECT_TRUE(IsPrefetchActivationBeaconEnabled(url, headers.get()));
}

TEST_F(PreloadActivationReportUtilsTest,
       PrefetchActivationBeacon_GloballyEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPrefetchActivationBeacon);

  GURL url(kTestUrl);
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");

  // Enabled by default (no token required).
  EXPECT_TRUE(IsPrefetchActivationBeaconEnabled(url, headers.get()));
}

TEST_F(PreloadActivationReportUtilsTest,
       PrefetchActivationBeacon_KillSwitched) {
  base::test::ScopedFeatureList override_feature_list;
  override_feature_list.InitAndDisableFeature(
      features::kPrefetchActivationBeacon);

  GURL url(kTestUrl);
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  headers->AddHeader("Origin-Trial", kPrefetchToken);

  // Even with trial token, the kill-switch (override to disabled) keeps it
  // disabled.
  EXPECT_FALSE(IsPrefetchActivationBeaconEnabled(url, headers.get()));
}

TEST_F(PreloadActivationReportUtilsTest,
       PrerenderActivationBeacon_DefaultDisabled) {
  GURL url(kTestUrl);
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");

  // No trial token -> disabled.
  EXPECT_FALSE(IsPrerenderActivationBeaconEnabled(url, headers.get()));

  // With trial token -> enabled.
  headers->AddHeader("Origin-Trial", kPrerenderToken);
  EXPECT_TRUE(IsPrerenderActivationBeaconEnabled(url, headers.get()));
}

TEST_F(PreloadActivationReportUtilsTest,
       PrerenderActivationBeacon_GloballyEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPrerenderActivationBeacon);

  GURL url(kTestUrl);
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");

  // Enabled by default (no token required).
  EXPECT_TRUE(IsPrerenderActivationBeaconEnabled(url, headers.get()));
}

TEST_F(PreloadActivationReportUtilsTest,
       PrerenderActivationBeacon_KillSwitched) {
  base::test::ScopedFeatureList override_feature_list;
  override_feature_list.InitAndDisableFeature(
      features::kPrerenderActivationBeacon);

  GURL url(kTestUrl);
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  headers->AddHeader("Origin-Trial", kPrerenderToken);

  // Even with trial token, the kill-switch keeps it disabled.
  EXPECT_FALSE(IsPrerenderActivationBeaconEnabled(url, headers.get()));
}

}  // namespace content
