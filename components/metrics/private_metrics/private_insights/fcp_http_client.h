// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_FCP_HTTP_CLIENT_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_FCP_HTTP_CLIENT_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/federated_compute/src/fcp/client/http/http_client.h"

namespace base {
class SequencedTaskRunner;
}

namespace private_insights {

// A thread-compatible proxy wrapper around
// scoped_refptr<network::SharedURLLoaderFactory>.
//
// Since SharedURLLoaderFactory is not thread-safe and must be accessed and
// released on the sequence it was created on (typically the UI thread), this
// class ensures that:
// 1. The factory is safely released back to the UI thread when this proxy is
//    destroyed on the background thread.
// 2. Network requests initiated on the background thread are safely routed
//    and executed on the UI thread.
//
class FcpHttpRequestManager {
 public:
  FcpHttpRequestManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  FcpHttpRequestManager(const FcpHttpRequestManager&) = delete;
  FcpHttpRequestManager& operator=(const FcpHttpRequestManager&) = delete;

  ~FcpHttpRequestManager();

 private:
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_
      GUARDED_BY_CONTEXT(ui_sequence_checker_);

  SEQUENCE_CHECKER(ui_sequence_checker_);
};

class FcpHttpClient : public fcp::client::http::HttpClient {
 public:
  // The caller must ensure that `request_manager` outlives `this`.
  explicit FcpHttpClient(FcpHttpRequestManager* request_manager);
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

 private:
  raw_ptr<FcpHttpRequestManager> request_manager_;
};

}  // namespace private_insights

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_FCP_HTTP_CLIENT_H_
