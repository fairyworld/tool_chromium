// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_insights/fcp_http_client.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace private_insights {

class FcpHttpClientTest : public testing::Test {
 public:
  FcpHttpClientTest() = default;
  ~FcpHttpClientTest() override = default;
};

TEST_F(FcpHttpClientTest, CanInstantiate) {
  FcpHttpClient client(nullptr);
  EXPECT_NE(&client, nullptr);
}

}  // namespace private_insights
