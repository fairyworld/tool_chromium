// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_FCP_HTTP_CLIENT_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_FCP_HTTP_CLIENT_H_

#include <memory>
#include <vector>

#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/federated_compute/src/fcp/client/http/http_client.h"

namespace private_insights {

class FcpHttpClient : public fcp::client::http::HttpClient {
 public:
  FcpHttpClient();
  ~FcpHttpClient() override;

  FcpHttpClient(const FcpHttpClient&) = delete;
  FcpHttpClient& operator=(const FcpHttpClient&) = delete;

  // HttpClient overrides:
  std::unique_ptr<fcp::client::http::HttpRequestHandle> EnqueueRequest(
      std::unique_ptr<fcp::client::http::HttpRequest> request) override;

  absl::Status PerformRequests(
      std::vector<std::pair<fcp::client::http::HttpRequestHandle*,
                            fcp::client::http::HttpRequestCallback*>> requests)
      override;
};

}  // namespace private_insights

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_FCP_HTTP_CLIENT_H_
