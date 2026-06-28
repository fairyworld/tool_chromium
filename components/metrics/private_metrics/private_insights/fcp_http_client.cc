// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_insights/fcp_http_client.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace private_insights {

namespace {

[[maybe_unused]] constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("private_insights_fcp_http", R"(
      semantics {
        sender: "Private Insights Service"
        description:
          "Uploads encrypted metrics to a confidential compute service to be "
          "processed by a TEE-hosted workloads pipeline, which uses "
          "differential privacy to ensure that only anonymized, aggregate "
          "results are released."
        trigger:
          "Reports are automatically generated at intervals while Chrome is "
          "running."
        data:
          "Encrypted metrics about the usage of the contextual cueing feature, "
          "including the information about the user context, and the cue "
          "suggestion. The encryption is bound to the TEE-hosted binary which "
          "applies differential privacy, ensuring that Google has access to "
          "anonymized aggregates only."
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts {
            owners: "//components/metrics/private_metrics/OWNERS"
          }
        }
        user_data {
          type: WEB_CONTENT
          type: USER_CONTENT
          type: SENSITIVE_URL
          type: USAGE_AND_PERFORMANCE_METRICS
        }
        last_reviewed: "2026-06-26"
      }
      policy {
        cookies_allowed: NO
        setting:
          "Users can enable or disable this feature via "
          "\"Help improve Chrome's features and performance\" in Chrome "
          "settings under Sync and Google services > Other Google services. "
          "The feature is enabled by default."
        chrome_policy {
          MetricsReportingEnabled {
            policy_options { mode: MANDATORY }
            MetricsReportingEnabled: false
          }
        }
      })");

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

SharedURLLoaderFactoryProxy::SharedURLLoaderFactoryProxy(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : url_loader_factory_(std::move(url_loader_factory)),
      ui_task_runner_(std::move(ui_task_runner)) {}

SharedURLLoaderFactoryProxy::~SharedURLLoaderFactoryProxy() {
  if (url_loader_factory_) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](scoped_refptr<network::SharedURLLoaderFactory> factory) {
              // Body is intentionally empty; factory will be destructed here.
            },
            std::move(url_loader_factory_)));
  }
}

FcpHttpClient::FcpHttpClient(SharedURLLoaderFactoryProxy* url_loader_factory)
    : url_loader_factory_proxy_(url_loader_factory) {}

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
