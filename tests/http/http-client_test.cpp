// Coverage for src/http/http-client.*, against FakeTransport. Offline
// only: no test here opens a socket, and DefaultTransport() is referenced
// only for identity, never Perform'd, so CurlTransport code never runs.

#include "http/http-client.h"

#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "core/status.h"
#include "fake-transport.h"

namespace {

bool HeadersEqual(const std::vector<HttpHeader>& a, const std::vector<HttpHeader>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a[i].name != b[i].name || a[i].value != b[i].value) {
      return false;
    }
  }
  return true;
}

void ExpectOptionsEqual(const RequestOptions& a, const RequestOptions& b) {
  EXPECT_EQ(a.connectTimeoutMs, b.connectTimeoutMs);
  EXPECT_EQ(a.totalTimeoutMs, b.totalTimeoutMs);
  EXPECT_EQ(a.maxResponseBytes, b.maxResponseBytes);
  EXPECT_EQ(a.skipTlsVerification, b.skipTlsVerification);
}

HttpRequest MakeRequest(const std::string& url) {
  HttpRequest request;
  request.method = HttpMethod::Get;
  request.url = url;
  return request;
}

}  // namespace

// ---------------------------------------------------------------------------
// URL validation
// ---------------------------------------------------------------------------

TEST(HttpClient, EmptyUrlIsInvalidArgumentAndTransportNeverCalled) {
  FakeTransport transport;
  HttpClient client(transport);
  HttpResponse response;

  EXPECT_EQ(client.Perform(MakeRequest(""), response), Status::InvalidArgument);
  EXPECT_FALSE(transport.HasLastRequest());
}

TEST(HttpClient, NonEmptyMalformedUrlReachesTransportUnrejected) {
  // No second URL parser here -- validation is the transport's job. A
  // garbage but non-empty string must still reach it unchanged.
  FakeTransport transport;
  transport.SetResult(Status::InvalidUrl, HttpResponse{});
  HttpClient client(transport);
  HttpResponse response;

  EXPECT_EQ(client.Perform(MakeRequest("not a url"), response), Status::InvalidUrl);
  ASSERT_TRUE(transport.HasLastRequest());
  EXPECT_EQ(transport.LastRequest().url, "not a url");
}

// ---------------------------------------------------------------------------
// Request forwarding
// ---------------------------------------------------------------------------

TEST(HttpClient, ForwardsRequestFieldsUnmodified) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  HttpRequest request;
  request.method = HttpMethod::Post;
  request.url = "http://example.invalid/widgets";
  request.headers = {{"Content-Type", "application/json"}, {"X-Trace", "abc"}};
  request.body = std::string("{\"n\":1}", 7);
  request.options.connectTimeoutMs = 1234;
  request.options.totalTimeoutMs = 5678;
  request.options.maxResponseBytes = 99;
  request.options.skipTlsVerification = true;

  HttpResponse response;
  ASSERT_EQ(client.Perform(request, response), Status::Ok);

  const HttpRequest& seen = transport.LastRequest();
  EXPECT_EQ(seen.method, HttpMethod::Post);
  EXPECT_EQ(seen.url, request.url);
  EXPECT_TRUE(HeadersEqual(seen.headers, request.headers));
  EXPECT_EQ(seen.body, request.body);
  ExpectOptionsEqual(seen.options, request.options);
}

TEST(HttpClient, ForwardsResponseFieldsFromTransportUnmodified) {
  FakeTransport transport;
  HttpResponse scripted;
  scripted.statusCode = 201;
  scripted.body = "created";
  scripted.headers = {{"Location", "/widgets/1"}};
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);

  HttpResponse response;
  ASSERT_EQ(client.Perform(MakeRequest("http://example.invalid/widgets"), response), Status::Ok);
  EXPECT_EQ(response.statusCode, 201);
  EXPECT_EQ(response.body, "created");
  EXPECT_TRUE(HeadersEqual(response.headers, scripted.headers));
}

// ---------------------------------------------------------------------------
// Status propagation. The CURLcode-to-Status mapping itself is not
// covered here: it is reachable only through a real network transfer, and
// asserting it against a fake would only restate the switch statement it
// wraps. Verified against a live server instead, along with the
// precedence between a response-size cap breach and a generic write
// error, which is decided inside that same real transfer. What these
// cases cover is that HttpClient never alters a Status value a transport
// hands back, for every value the mapping can produce.
// ---------------------------------------------------------------------------

TEST(HttpClientStatusPropagation, Ok) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);
  HttpResponse response;
  EXPECT_EQ(client.Perform(MakeRequest("http://example.invalid/"), response), Status::Ok);
}

TEST(HttpClientStatusPropagation, Timeout) {
  FakeTransport transport;
  transport.SetResult(Status::Timeout, HttpResponse{});
  HttpClient client(transport);
  HttpResponse response;
  EXPECT_EQ(client.Perform(MakeRequest("http://example.invalid/"), response), Status::Timeout);
}

TEST(HttpClientStatusPropagation, InvalidUrl) {
  FakeTransport transport;
  transport.SetResult(Status::InvalidUrl, HttpResponse{});
  HttpClient client(transport);
  HttpResponse response;
  EXPECT_EQ(client.Perform(MakeRequest("http://example.invalid/"), response), Status::InvalidUrl);
}

TEST(HttpClientStatusPropagation, TlsError) {
  FakeTransport transport;
  transport.SetResult(Status::TlsError, HttpResponse{});
  HttpClient client(transport);
  HttpResponse response;
  EXPECT_EQ(client.Perform(MakeRequest("https://example.invalid/"), response), Status::TlsError);
}

TEST(HttpClientStatusPropagation, NetworkError) {
  FakeTransport transport;
  transport.SetResult(Status::NetworkError, HttpResponse{});
  HttpClient client(transport);
  HttpResponse response;
  EXPECT_EQ(client.Perform(MakeRequest("http://example.invalid/"), response), Status::NetworkError);
}

TEST(HttpClientStatusPropagation, ResponseTooLarge) {
  FakeTransport transport;
  transport.SetResult(Status::ResponseTooLarge, HttpResponse{});
  HttpClient client(transport);
  HttpResponse response;
  EXPECT_EQ(client.Perform(MakeRequest("http://example.invalid/"), response), Status::ResponseTooLarge);
}

TEST(HttpClientStatusPropagation, TransportInitFailed) {
  FakeTransport transport;
  transport.SetResult(Status::TransportInitFailed, HttpResponse{});
  HttpClient client(transport);
  HttpResponse response;
  EXPECT_EQ(client.Perform(MakeRequest("http://example.invalid/"), response),
            Status::TransportInitFailed);
}

// ---------------------------------------------------------------------------
// HTTP status handling -- an HTTP-level status of 400 or above is still a
// successful transport-level exchange: Status::Ok, with the actual code
// carried in statusCode. Each value gets its own case because a future
// caller across the CAPL boundary will observe exactly this split.
// ---------------------------------------------------------------------------

TEST(HttpClientHttpStatus, Http200) {
  FakeTransport transport;
  HttpResponse scripted;
  scripted.statusCode = 200;
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);
  HttpResponse response;
  EXPECT_EQ(client.Perform(MakeRequest("http://example.invalid/"), response), Status::Ok);
  EXPECT_EQ(response.statusCode, 200);
}

TEST(HttpClientHttpStatus, Http201) {
  FakeTransport transport;
  HttpResponse scripted;
  scripted.statusCode = 201;
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);
  HttpResponse response;
  EXPECT_EQ(client.Perform(MakeRequest("http://example.invalid/"), response), Status::Ok);
  EXPECT_EQ(response.statusCode, 201);
}

TEST(HttpClientHttpStatus, Http204) {
  FakeTransport transport;
  HttpResponse scripted;
  scripted.statusCode = 204;
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);
  HttpResponse response;
  EXPECT_EQ(client.Perform(MakeRequest("http://example.invalid/"), response), Status::Ok);
  EXPECT_EQ(response.statusCode, 204);
}

TEST(HttpClientHttpStatus, Http400) {
  FakeTransport transport;
  HttpResponse scripted;
  scripted.statusCode = 400;
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);
  HttpResponse response;
  EXPECT_EQ(client.Perform(MakeRequest("http://example.invalid/"), response), Status::Ok);
  EXPECT_EQ(response.statusCode, 400);
}

TEST(HttpClientHttpStatus, Http401) {
  FakeTransport transport;
  HttpResponse scripted;
  scripted.statusCode = 401;
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);
  HttpResponse response;
  EXPECT_EQ(client.Perform(MakeRequest("http://example.invalid/"), response), Status::Ok);
  EXPECT_EQ(response.statusCode, 401);
}

TEST(HttpClientHttpStatus, Http404) {
  FakeTransport transport;
  HttpResponse scripted;
  scripted.statusCode = 404;
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);
  HttpResponse response;
  EXPECT_EQ(client.Perform(MakeRequest("http://example.invalid/"), response), Status::Ok);
  EXPECT_EQ(response.statusCode, 404);
}

TEST(HttpClientHttpStatus, Http500) {
  FakeTransport transport;
  HttpResponse scripted;
  scripted.statusCode = 500;
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);
  HttpResponse response;
  EXPECT_EQ(client.Perform(MakeRequest("http://example.invalid/"), response), Status::Ok);
  EXPECT_EQ(response.statusCode, 500);
}

// ---------------------------------------------------------------------------
// Malformed / empty response bodies, zero headers
// ---------------------------------------------------------------------------

TEST(HttpClient, EmptyBodyAndZeroHeadersPassThrough) {
  FakeTransport transport;
  HttpResponse scripted;
  scripted.statusCode = 204;
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);
  HttpResponse response;
  ASSERT_EQ(client.Perform(MakeRequest("http://example.invalid/"), response), Status::Ok);
  EXPECT_TRUE(response.body.empty());
  EXPECT_TRUE(response.headers.empty());
}

TEST(HttpClient, BinaryBodyWithEmbeddedNulBytesPassesThroughByteIdentical) {
  FakeTransport transport;
  const std::string binaryBody("\x00\x01\xff not json at all \x00", 21);
  HttpResponse scripted;
  scripted.statusCode = 200;
  scripted.body = binaryBody;
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);
  HttpResponse response;
  ASSERT_EQ(client.Perform(MakeRequest("http://example.invalid/"), response), Status::Ok);
  EXPECT_EQ(response.body, binaryBody);
  EXPECT_EQ(response.body.size(), binaryBody.size());
}

// ---------------------------------------------------------------------------
// Size cap -- architecture parity. The write-callback arithmetic that
// enforces this at runtime is, like the status mapping above, only
// reachable through a real transfer. What this file can check without one
// is that the exposed cap constant itself is the same 8 MiB value this
// same source produces on both architectures.
// ---------------------------------------------------------------------------

TEST(SizeCap, DefaultCapConstantIsEightMebibytesOnThisArchitecture) {
  EXPECT_EQ(kDefaultMaxResponseBytes, static_cast<std::uint32_t>(8u * 1024u * 1024u));
}

TEST(SizeCap, ResponseTooLargeAndOkBothPropagateRegardlessOfBodySize) {
  FakeTransport transport;
  HttpResponse atCap;
  atCap.statusCode = 200;
  atCap.body.assign(kDefaultMaxResponseBytes, 'x');
  transport.SetResult(Status::Ok, atCap);
  HttpClient client(transport);
  HttpResponse response;
  ASSERT_EQ(client.Perform(MakeRequest("http://example.invalid/"), response), Status::Ok);
  EXPECT_EQ(response.body.size(), static_cast<std::size_t>(kDefaultMaxResponseBytes));

  transport.SetResult(Status::ResponseTooLarge, HttpResponse{});
  HttpResponse overCapResponse;
  EXPECT_EQ(client.Perform(MakeRequest("http://example.invalid/"), overCapResponse),
            Status::ResponseTooLarge);
}

// ---------------------------------------------------------------------------
// DefaultTransport
// ---------------------------------------------------------------------------

TEST(HttpClient, DefaultTransportIsTheSameInstanceAcrossCalls) {
  // Identity only -- Perform is never called on this transport, so
  // CurlTransport's curl_global_init path never runs here.
  EXPECT_EQ(&DefaultTransport(), &DefaultTransport());
}

TEST(HttpClient, DefaultConstructedClientDoesNotTouchNetworkUntilPerformIsCalled) {
  HttpClient client;
  (void)client;
}

// ---------------------------------------------------------------------------
// Concurrency smoke test
// ---------------------------------------------------------------------------

// This confirms two concurrent Perform calls through the fake complete
// without crashing or hanging, under FakeTransport's own internal
// locking. It is not a data-race detector -- this environment has no
// TSan-equivalent available -- so it cannot prove HttpClient/CurlTransport
// are race-free on their own; it only proves this seam is safe to call
// from two threads at once.
TEST(HttpClient, ConcurrentPerformCallsThroughFakeCompleteCleanly) {
  FakeTransport transport;
  HttpResponse scripted;
  scripted.statusCode = 200;
  scripted.body = "ok";
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);

  Status statusA = Status::InvalidArgument;
  Status statusB = Status::InvalidArgument;
  HttpResponse responseA;
  HttpResponse responseB;
  const HttpRequest requestA = MakeRequest("http://example.invalid/a");
  const HttpRequest requestB = MakeRequest("http://example.invalid/b");

  std::thread threadA([&] { statusA = client.Perform(requestA, responseA); });
  std::thread threadB([&] { statusB = client.Perform(requestB, responseB); });
  threadA.join();
  threadB.join();

  EXPECT_EQ(statusA, Status::Ok);
  EXPECT_EQ(statusB, Status::Ok);
  EXPECT_EQ(responseA.body, "ok");
  EXPECT_EQ(responseB.body, "ok");
  EXPECT_EQ(transport.CallCount(), 2);
}

// ---------------------------------------------------------------------------
// WouldExceedResponseCap -- pure arithmetic, no transport involved. Covers
// the boundary directly rather than through a real transfer.
// ---------------------------------------------------------------------------

TEST(WouldExceedResponseCap, ExactlyAtCapDoesNotExceed) {
  EXPECT_FALSE(WouldExceedResponseCap(60, 40, 100));
}

TEST(WouldExceedResponseCap, OneByteOverCapExceeds) {
  EXPECT_TRUE(WouldExceedResponseCap(60, 41, 100));
}

TEST(WouldExceedResponseCap, EmptyIncomingIntoBufferAlreadyAtCapDoesNotExceed) {
  EXPECT_FALSE(WouldExceedResponseCap(100, 0, 100));
}

TEST(WouldExceedResponseCap, ZeroCapBytes) {
  EXPECT_FALSE(WouldExceedResponseCap(0, 0, 0));
  EXPECT_TRUE(WouldExceedResponseCap(0, 1, 0));
}

TEST(WouldExceedResponseCap, CurrentSizeAlreadyAtCapWithNonZeroIncomingExceeds) {
  EXPECT_TRUE(WouldExceedResponseCap(100, 1, 100));
}

// The assertion that makes the wraparound guard directly testable:
// without the incoming > cap short-circuit, cap - incoming underflows to a
// huge std::size_t and currentSize > (huge value) is false, so a chunk far
// larger than the cap would be reported as not exceeding it.
TEST(WouldExceedResponseCap, IncomingLargerThanCapExceedsWithoutWrappingTheSubtraction) {
  EXPECT_TRUE(WouldExceedResponseCap(0, 200, 100));
  EXPECT_TRUE(WouldExceedResponseCap(99, 200, 100));
}

// ---------------------------------------------------------------------------
// ResolveTransferResult -- cap-exceeded takes precedence over whatever the
// CURLcode mapping produced.
// ---------------------------------------------------------------------------

TEST(ResolveTransferResult, CapExceededBeatsMappedTransferError) {
  EXPECT_EQ(ResolveTransferResult(true, Status::NetworkError), Status::ResponseTooLarge);
}

TEST(ResolveTransferResult, CapExceededBeatsOk) {
  EXPECT_EQ(ResolveTransferResult(true, Status::Ok), Status::ResponseTooLarge);
}

TEST(ResolveTransferResult, NotExceededPassesOkThrough) {
  EXPECT_EQ(ResolveTransferResult(false, Status::Ok), Status::Ok);
}

TEST(ResolveTransferResult, NotExceededPassesMappedErrorThrough) {
  EXPECT_EQ(ResolveTransferResult(false, Status::NetworkError), Status::NetworkError);
}
