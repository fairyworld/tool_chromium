// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_insights/fcp_utils.h"

#include <atomic>

#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_insights {

TEST(GetMethodString, MethodMapping) {
  EXPECT_EQ(GetMethodString(fcp::client::http::HttpRequest::Method::kGet),
            "GET");
  EXPECT_EQ(GetMethodString(fcp::client::http::HttpRequest::Method::kPost),
            "POST");
  EXPECT_EQ(GetMethodString(fcp::client::http::HttpRequest::Method::kPut),
            "PUT");
  EXPECT_EQ(GetMethodString(fcp::client::http::HttpRequest::Method::kDelete),
            "DELETE");
  EXPECT_EQ(GetMethodString(fcp::client::http::HttpRequest::Method::kHead),
            "HEAD");
  EXPECT_EQ(GetMethodString(fcp::client::http::HttpRequest::Method::kPatch),
            "PATCH");
}

TEST(ConvertNetErrorToFcpStatus, ErrorMapping) {
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::OK).code(), absl::StatusCode::kOk);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_ABORTED).code(),
            absl::StatusCode::kCancelled);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_TIMED_OUT).code(),
            absl::StatusCode::kDeadlineExceeded);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_CONNECTION_TIMED_OUT).code(),
            absl::StatusCode::kDeadlineExceeded);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_INTERNET_DISCONNECTED).code(),
            absl::StatusCode::kUnavailable);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_NETWORK_CHANGED).code(),
            absl::StatusCode::kUnavailable);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_NAME_NOT_RESOLVED).code(),
            absl::StatusCode::kUnavailable);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_NAME_RESOLUTION_FAILED).code(),
            absl::StatusCode::kUnavailable);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_CONNECTION_REFUSED).code(),
            absl::StatusCode::kUnavailable);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_CONNECTION_RESET).code(),
            absl::StatusCode::kUnavailable);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_CONNECTION_CLOSED).code(),
            absl::StatusCode::kUnavailable);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_CONNECTION_ABORTED).code(),
            absl::StatusCode::kUnavailable);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_ADDRESS_UNREACHABLE).code(),
            absl::StatusCode::kUnavailable);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_TOO_MANY_REDIRECTS).code(),
            absl::StatusCode::kOutOfRange);
  EXPECT_EQ(ConvertNetErrorToFcpStatus(net::ERR_FAILED).code(),
            absl::StatusCode::kInternal);
}

TEST(ProcessRequestHeadersTest, DefaultValues) {
  fcp::client::http::HeaderList headers;
  ProcessedRequestHeaders result = ProcessFcpRequestHeaders(headers);
  EXPECT_FALSE(result.has_explicit_accept_encoding);
  EXPECT_EQ(result.upload_content_type, "application/octet-stream");
  EXPECT_TRUE(result.headers.IsEmpty());
}

TEST(ProcessRequestHeadersTest, HeaderFilteringAndExtraction) {
  fcp::client::http::HeaderList headers = {
      {"Host", "example.com"},
      {"Content-Length", "123"},
      {"Transfer-Encoding", "chunked"},
      {"Accept-Encoding", "gzip, deflate"},
      {"Content-Type", "application/x-protobuf"},
      {"X-Custom-Header", "custom_value"},
  };

  ProcessedRequestHeaders result = ProcessFcpRequestHeaders(headers);
  EXPECT_TRUE(result.has_explicit_accept_encoding);
  EXPECT_EQ(result.upload_content_type, "application/x-protobuf");

  // Unsafe headers (Host, Content-Length, Transfer-Encoding) must be stripped.
  EXPECT_FALSE(result.headers.HasHeader("Host"));
  EXPECT_FALSE(result.headers.HasHeader("Content-Length"));
  EXPECT_FALSE(result.headers.HasHeader("Transfer-Encoding"));

  // Allowed headers must be present in net::HttpRequestHeaders.
  EXPECT_EQ(result.headers.GetHeader("Accept-Encoding"), "gzip, deflate");
  EXPECT_EQ(result.headers.GetHeader("Content-Type"), "application/x-protobuf");
  EXPECT_EQ(result.headers.GetHeader("X-Custom-Header"), "custom_value");
}

TEST(ProcessRequestHeadersTest, MultipleHeaderValues) {
  fcp::client::http::HeaderList headers = {
      {"X-Multi-Header", "value1"},
      {"X-Multi-Header-2", "valueA, valueB"},
      {"X-Multi-Header", "value2"},
  };

  ProcessedRequestHeaders result = ProcessFcpRequestHeaders(headers);
  EXPECT_EQ(result.headers.GetHeader("X-Multi-Header"), "value1, value2");
  EXPECT_EQ(result.headers.GetHeader("X-Multi-Header-2"), "valueA, valueB");
}

TEST(ConvertResponseHeadersToFcp, WithoutExplicitAcceptEncoding) {
  auto headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/json\r\n"
      "Transfer-Encoding: chunked\r\n"
      "Content-Length: 100\r\n"
      "Content-Encoding: gzip\r\n"
      "X-Custom-Header: foo\r\n\r\n");
  ASSERT_TRUE(headers);

  // Without explicit accept encoding, Transfer-Encoding, Content-Length, and
  // Content-Encoding are stripped.
  fcp::client::http::HeaderList result = ConvertResponseHeadersToFcp(
      headers.get(), /*request_had_explicit_accept_encoding=*/false);
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0].first, "Content-Type");
  EXPECT_EQ(result[0].second, "application/json");
  EXPECT_EQ(result[1].first, "X-Custom-Header");
  EXPECT_EQ(result[1].second, "foo");
}

TEST(ConvertResponseHeadersToFcp, WithExplicitAcceptEncoding) {
  auto headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/json\r\n"
      "Transfer-Encoding: chunked\r\n"
      "Content-Length: 100\r\n"
      "Content-Encoding: gzip\r\n"
      "X-Custom-Header: foo\r\n\r\n");
  ASSERT_TRUE(headers);

  // With explicit accept encoding, Content-Length and Content-Encoding are
  // kept, Transfer-Encoding is stripped.
  fcp::client::http::HeaderList result = ConvertResponseHeadersToFcp(
      headers.get(),
      /*request_had_explicit_accept_encoding=*/true);
  ASSERT_EQ(result.size(), 4u);
  EXPECT_EQ(result[0].first, "Content-Type");
  EXPECT_EQ(result[0].second, "application/json");
  EXPECT_EQ(result[1].first, "Content-Length");
  EXPECT_EQ(result[1].second, "100");
  EXPECT_EQ(result[2].first, "Content-Encoding");
  EXPECT_EQ(result[2].second, "gzip");
  EXPECT_EQ(result[3].first, "X-Custom-Header");
  EXPECT_EQ(result[3].second, "foo");
}

TEST(ConvertResponseHeadersToFcp, CaseInsensitiveHeaderMatching) {
  auto headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\r\n"
      "content-type: application/json\r\n"
      "transfer-encoding: chunked\r\n"
      "CONTENT-LENGTH: 100\r\n"
      "Content-encoding: gzip\r\n"
      "x-custom-header: foo\r\n\r\n");
  ASSERT_TRUE(headers);

  // Header matching is case-insensitive, so transfer-encoding, CONTENT-LENGTH,
  // and Content-encoding are correctly stripped even with varied casing.
  fcp::client::http::HeaderList result = ConvertResponseHeadersToFcp(
      headers.get(), /*request_had_explicit_accept_encoding=*/false);
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0].first, "content-type");
  EXPECT_EQ(result[0].second, "application/json");
  EXPECT_EQ(result[1].first, "x-custom-header");
  EXPECT_EQ(result[1].second, "foo");
}

TEST(ConvertResponseHeadersToFcp, MultipleHeaderValues) {
  auto headers = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\r\n"
      "X-Multi-Header: value1\r\n"
      "X-Multi-Header-2: valueA, valueB\r\n"
      "X-Multi-Header: value2\r\n\r\n");
  ASSERT_TRUE(headers);

  fcp::client::http::HeaderList result = ConvertResponseHeadersToFcp(
      headers.get(), /*request_had_explicit_accept_encoding=*/false);
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0].first, "X-Multi-Header");
  EXPECT_EQ(result[0].second, "value1");
  EXPECT_EQ(result[1].first, "X-Multi-Header-2");
  EXPECT_EQ(result[1].second, "valueA, valueB");
  EXPECT_EQ(result[2].first, "X-Multi-Header");
  EXPECT_EQ(result[2].second, "value2");
}

TEST(ConvertResponseHeadersToFcp, NullHeaders) {
  fcp::client::http::HeaderList result = ConvertResponseHeadersToFcp(
      nullptr, /*request_had_explicit_accept_encoding=*/false);
  EXPECT_TRUE(result.empty());
}

TEST(CountdownLatchTest, ZeroInitialCount) {
  CountdownLatch latch(0);
  // Wait() should return immediately for count 0.
  latch.Wait();
}

TEST(CountdownLatchTest, SingleThreadCountDown) {
  CountdownLatch latch(1);
  latch.CountDown();
  latch.Wait();
}

TEST(CountdownLatchTest, MultiThreadedCountDown) {
  base::test::TaskEnvironment task_environment;

  constexpr size_t kNumTasks = 5;
  CountdownLatch latch(kNumTasks);
  std::atomic<bool> wait_completed{false};
  base::RunLoop run_loop;

  // Post a background task that calls latch.Wait().
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](CountdownLatch* l, std::atomic<bool>* completed) {
            base::ScopedAllowBaseSyncPrimitivesForTesting allow_sync;
            l->Wait();
            completed->store(true);
          },
          base::Unretained(&latch), base::Unretained(&wait_completed)),
      run_loop.QuitClosure());

  // Count down 4 times.
  for (size_t i = 0; i < kNumTasks - 1; ++i) {
    latch.CountDown();
  }

  // Verify that after 4 countdowns, the waiting task is NOT completed yet.
  EXPECT_FALSE(wait_completed.load());

  // Perform the 5th and final countdown.
  latch.CountDown();

  // Wait for the background task to complete and reply.
  run_loop.Run();

  // Verify that after the 5th countdown, the waiting task completed.
  EXPECT_TRUE(wait_completed.load());
}

}  // namespace private_insights
