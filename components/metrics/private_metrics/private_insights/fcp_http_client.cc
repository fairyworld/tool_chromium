// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_insights/fcp_http_client.h"

namespace private_insights {

namespace {

class FcpHttpRequestHandle : public fcp::client::http::HttpRequestHandle {
 public:
  explicit FcpHttpRequestHandle(
      std::unique_ptr<fcp::client::http::HttpRequest> request)
      : request_(std::move(request)) {}

  ~FcpHttpRequestHandle() override = default;

  FcpHttpRequestHandle(const FcpHttpRequestHandle&) = delete;
  FcpHttpRequestHandle& operator=(const FcpHttpRequestHandle&) = delete;

  // HttpRequestHandle overrides:
  SentReceivedBytes TotalSentReceivedBytes() const override { return {0, 0}; }

  void Cancel() override {}

 private:
  std::unique_ptr<fcp::client::http::HttpRequest> request_;
};

}  // namespace

FcpHttpClient::FcpHttpClient() = default;
FcpHttpClient::~FcpHttpClient() = default;

std::unique_ptr<fcp::client::http::HttpRequestHandle>
FcpHttpClient::EnqueueRequest(
    std::unique_ptr<fcp::client::http::HttpRequest> request) {
  return std::make_unique<FcpHttpRequestHandle>(std::move(request));
}

absl::Status FcpHttpClient::PerformRequests(
    std::vector<std::pair<fcp::client::http::HttpRequestHandle*,
                          fcp::client::http::HttpRequestCallback*>> requests) {
  return absl::OkStatus();
}

}  // namespace private_insights
