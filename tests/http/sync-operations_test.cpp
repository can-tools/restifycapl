// Coverage for src/http/sync-operations.*, against FakeTransport. Offline
// only, same as http-client_test.cpp -- no test here opens a socket.

#include "http/sync-operations.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "core/status.h"
#include "fake-transport.h"

namespace {

const std::vector<HttpHeader> kSampleHeaders = {{"Accept", "application/json"}, {"X-Test", "1"}};

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

}  // namespace

// ---------------------------------------------------------------------------
// A non-empty body on Get, Head or Delete is rejected before the transport
// is ever reached. Head has no dedicated verb helper, so its coverage is a
// hand-built request through Request() directly.
// ---------------------------------------------------------------------------

TEST(Request, GetWithNonEmptyBodyIsInvalidArgumentAndTransportNeverCalled) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  HttpRequest request;
  request.method = HttpMethod::Get;
  request.url = "http://example.invalid/";
  request.body = "unexpected";

  HttpResponse response;
  EXPECT_EQ(Request(client, request, response), Status::InvalidArgument);
  EXPECT_FALSE(transport.HasLastRequest());
}

TEST(Request, HandBuiltHeadWithNonEmptyBodyIsInvalidArgumentAndTransportNeverCalled) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  HttpRequest request;
  request.method = HttpMethod::Head;
  request.url = "http://example.invalid/";
  request.body = "unexpected";

  HttpResponse response;
  EXPECT_EQ(Request(client, request, response), Status::InvalidArgument);
  EXPECT_FALSE(transport.HasLastRequest());
}

TEST(Request, DeleteWithNonEmptyBodyIsInvalidArgumentAndTransportNeverCalled) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  HttpRequest request;
  request.method = HttpMethod::Delete;
  request.url = "http://example.invalid/";
  request.body = "unexpected";

  HttpResponse response;
  EXPECT_EQ(Request(client, request, response), Status::InvalidArgument);
  EXPECT_FALSE(transport.HasLastRequest());
}

TEST(Request, GetWithEmptyBodyReachesTransport) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  HttpRequest request;
  request.method = HttpMethod::Get;
  request.url = "http://example.invalid/";

  HttpResponse response;
  EXPECT_EQ(Request(client, request, response), Status::Ok);
  EXPECT_TRUE(transport.HasLastRequest());
}

TEST(Request, PostWithNonEmptyBodyReachesTransport) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  HttpRequest request;
  request.method = HttpMethod::Post;
  request.url = "http://example.invalid/";
  request.body = "payload";

  HttpResponse response;
  EXPECT_EQ(Request(client, request, response), Status::Ok);
  ASSERT_TRUE(transport.HasLastRequest());
  EXPECT_EQ(transport.LastRequest().body, "payload");
}

// ---------------------------------------------------------------------------
// Empty URL
// ---------------------------------------------------------------------------

TEST(Request, EmptyUrlIsInvalidArgumentAndTransportNeverCalled) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  HttpRequest request;
  request.method = HttpMethod::Get;
  request.url = "";

  HttpResponse response;
  EXPECT_EQ(Request(client, request, response), Status::InvalidArgument);
  EXPECT_FALSE(transport.HasLastRequest());
}

TEST(Get, EmptyUrlIsInvalidArgument) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  HttpResponse response;
  EXPECT_EQ(Get(client, "", kSampleHeaders, RequestOptions{}, response), Status::InvalidArgument);
  EXPECT_FALSE(transport.HasLastRequest());
}

// ---------------------------------------------------------------------------
// Each verb builds the right method, forwards headers intact, and (where
// applicable) forwards a body byte-identical, including embedded NUL
// bytes -- a plain equality check would pass even on a silently truncated
// copy if the body were compared as a C string instead of std::string.
// ---------------------------------------------------------------------------

TEST(Get, SetsMethodGetWithEmptyBodyAndForwardsHeadersIntact) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  HttpResponse response;
  ASSERT_EQ(Get(client, "http://example.invalid/", kSampleHeaders, RequestOptions{}, response),
            Status::Ok);
  const HttpRequest& seen = transport.LastRequest();
  EXPECT_EQ(seen.method, HttpMethod::Get);
  EXPECT_TRUE(seen.body.empty());
  EXPECT_TRUE(HeadersEqual(seen.headers, kSampleHeaders));
}

TEST(Post, SetsMethodPostAndForwardsBodyByteIdenticalAndHeadersIntact) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);
  const std::string body("{\"a\":1}\x00tail", 12);

  HttpResponse response;
  ASSERT_EQ(Post(client, "http://example.invalid/", kSampleHeaders, body, RequestOptions{}, response),
            Status::Ok);
  const HttpRequest& seen = transport.LastRequest();
  EXPECT_EQ(seen.method, HttpMethod::Post);
  EXPECT_EQ(seen.body, body);
  EXPECT_EQ(seen.body.size(), body.size());
  EXPECT_TRUE(HeadersEqual(seen.headers, kSampleHeaders));
}

TEST(Put, SetsMethodPutAndForwardsBodyByteIdenticalAndHeadersIntact) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);
  const std::string body("{\"a\":1}\x00tail", 12);

  HttpResponse response;
  ASSERT_EQ(Put(client, "http://example.invalid/", kSampleHeaders, body, RequestOptions{}, response),
            Status::Ok);
  const HttpRequest& seen = transport.LastRequest();
  EXPECT_EQ(seen.method, HttpMethod::Put);
  EXPECT_EQ(seen.body, body);
  EXPECT_TRUE(HeadersEqual(seen.headers, kSampleHeaders));
}

TEST(Patch, SetsMethodPatchAndForwardsBodyByteIdenticalAndHeadersIntact) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);
  const std::string body("{\"a\":1}\x00tail", 12);

  HttpResponse response;
  ASSERT_EQ(Patch(client, "http://example.invalid/", kSampleHeaders, body, RequestOptions{}, response),
            Status::Ok);
  const HttpRequest& seen = transport.LastRequest();
  EXPECT_EQ(seen.method, HttpMethod::Patch);
  EXPECT_EQ(seen.body, body);
  EXPECT_TRUE(HeadersEqual(seen.headers, kSampleHeaders));
}

TEST(Delete, SetsMethodDeleteWithEmptyBodyAndForwardsHeadersIntact) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  HttpResponse response;
  ASSERT_EQ(Delete(client, "http://example.invalid/", kSampleHeaders, RequestOptions{}, response),
            Status::Ok);
  const HttpRequest& seen = transport.LastRequest();
  EXPECT_EQ(seen.method, HttpMethod::Delete);
  EXPECT_TRUE(seen.body.empty());
  EXPECT_TRUE(HeadersEqual(seen.headers, kSampleHeaders));
}

// ---------------------------------------------------------------------------
// skipTlsVerification: default-false regression, true round-tripped
// through every verb, and neither direction mutated. Whether the transport
// actually applies this to a real TLS handshake is unobservable through a
// fake and is not covered here -- that needs a real peer to verify
// against, which this suite deliberately never contacts.
// ---------------------------------------------------------------------------

TEST(RequestOptions, DefaultConstructedHasSkipTlsVerificationFalse) {
  EXPECT_FALSE(RequestOptions{}.skipTlsVerification);
}

TEST(Get, DefaultOptionsArriveWithSkipTlsVerificationFalse) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);
  HttpResponse response;
  ASSERT_EQ(Get(client, "http://example.invalid/", {}, RequestOptions{}, response), Status::Ok);
  EXPECT_FALSE(transport.LastRequest().options.skipTlsVerification);
}

TEST(Post, DefaultOptionsArriveWithSkipTlsVerificationFalse) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);
  HttpResponse response;
  ASSERT_EQ(Post(client, "http://example.invalid/", {}, "", RequestOptions{}, response), Status::Ok);
  EXPECT_FALSE(transport.LastRequest().options.skipTlsVerification);
}

TEST(Put, DefaultOptionsArriveWithSkipTlsVerificationFalse) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);
  HttpResponse response;
  ASSERT_EQ(Put(client, "http://example.invalid/", {}, "", RequestOptions{}, response), Status::Ok);
  EXPECT_FALSE(transport.LastRequest().options.skipTlsVerification);
}

TEST(Patch, DefaultOptionsArriveWithSkipTlsVerificationFalse) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);
  HttpResponse response;
  ASSERT_EQ(Patch(client, "http://example.invalid/", {}, "", RequestOptions{}, response), Status::Ok);
  EXPECT_FALSE(transport.LastRequest().options.skipTlsVerification);
}

TEST(Delete, DefaultOptionsArriveWithSkipTlsVerificationFalse) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);
  HttpResponse response;
  ASSERT_EQ(Delete(client, "http://example.invalid/", {}, RequestOptions{}, response), Status::Ok);
  EXPECT_FALSE(transport.LastRequest().options.skipTlsVerification);
}

TEST(Get, SkipTlsVerificationTrueRoundTrips) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);
  RequestOptions options;
  options.skipTlsVerification = true;
  HttpResponse response;
  ASSERT_EQ(Get(client, "https://example.invalid/", {}, options, response), Status::Ok);
  EXPECT_TRUE(transport.LastRequest().options.skipTlsVerification);
}

TEST(Post, SkipTlsVerificationTrueRoundTrips) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);
  RequestOptions options;
  options.skipTlsVerification = true;
  HttpResponse response;
  ASSERT_EQ(Post(client, "https://example.invalid/", {}, "", options, response), Status::Ok);
  EXPECT_TRUE(transport.LastRequest().options.skipTlsVerification);
}

TEST(Put, SkipTlsVerificationTrueRoundTrips) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);
  RequestOptions options;
  options.skipTlsVerification = true;
  HttpResponse response;
  ASSERT_EQ(Put(client, "https://example.invalid/", {}, "", options, response), Status::Ok);
  EXPECT_TRUE(transport.LastRequest().options.skipTlsVerification);
}

TEST(Patch, SkipTlsVerificationTrueRoundTrips) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);
  RequestOptions options;
  options.skipTlsVerification = true;
  HttpResponse response;
  ASSERT_EQ(Patch(client, "https://example.invalid/", {}, "", options, response), Status::Ok);
  EXPECT_TRUE(transport.LastRequest().options.skipTlsVerification);
}

TEST(Delete, SkipTlsVerificationTrueRoundTrips) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);
  RequestOptions options;
  options.skipTlsVerification = true;
  HttpResponse response;
  ASSERT_EQ(Delete(client, "https://example.invalid/", {}, options, response), Status::Ok);
  EXPECT_TRUE(transport.LastRequest().options.skipTlsVerification);
}

TEST(Request, SkipTlsVerificationTrueRoundTrips) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  HttpRequest request;
  request.method = HttpMethod::Get;
  request.url = "https://example.invalid/";
  request.options.skipTlsVerification = true;

  HttpResponse response;
  ASSERT_EQ(Request(client, request, response), Status::Ok);
  EXPECT_TRUE(transport.LastRequest().options.skipTlsVerification);
}

TEST(Request, FullyCustomOptionsForwardedUnmodifiedInEitherDirection) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  HttpRequest request;
  request.method = HttpMethod::Post;
  request.url = "https://example.invalid/";
  request.headers = kSampleHeaders;
  request.body = "payload";
  request.options.connectTimeoutMs = 111;
  request.options.totalTimeoutMs = 222;
  request.options.maxResponseBytes = 333;
  request.options.skipTlsVerification = true;

  HttpResponse response;
  ASSERT_EQ(Request(client, request, response), Status::Ok);
  ExpectOptionsEqual(transport.LastRequest().options, request.options);

  // Repeat with the TLS flag at its other value -- proves Request() does
  // not, say, always set it true or always clear it.
  request.options.skipTlsVerification = false;
  ASSERT_EQ(Request(client, request, response), Status::Ok);
  ExpectOptionsEqual(transport.LastRequest().options, request.options);
}

// ---------------------------------------------------------------------------
// Response body size cap. The cap constant's architecture parity is
// checked in http-client_test.cpp; this only confirms maxResponseBytes
// itself is forwarded unresolved (a caller-supplied 0 stays 0 here --
// substituting the real default is the transport's job, not this one's).
// ---------------------------------------------------------------------------

TEST(Get, ZeroMaxResponseBytesIsForwardedAsZeroNotPreResolved) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);
  HttpResponse response;
  ASSERT_EQ(Get(client, "http://example.invalid/", {}, RequestOptions{}, response), Status::Ok);
  EXPECT_EQ(transport.LastRequest().options.maxResponseBytes, 0u);
}

// ---------------------------------------------------------------------------
// Simulated timeouts and error responses, surfaced through a verb rather
// than the raw client.
// ---------------------------------------------------------------------------

TEST(Get, SurfacesTimeoutFromTransport) {
  FakeTransport transport;
  transport.SetResult(Status::Timeout, HttpResponse{});
  HttpClient client(transport);
  HttpResponse response;
  EXPECT_EQ(Get(client, "http://example.invalid/", {}, RequestOptions{}, response), Status::Timeout);
}

TEST(Post, SurfacesNetworkErrorFromTransport) {
  FakeTransport transport;
  transport.SetResult(Status::NetworkError, HttpResponse{});
  HttpClient client(transport);
  HttpResponse response;
  EXPECT_EQ(Post(client, "http://example.invalid/", {}, "body", RequestOptions{}, response),
            Status::NetworkError);
}

TEST(Get, SurfacesHttpStatusCodeFromResponse) {
  FakeTransport transport;
  HttpResponse scripted;
  scripted.statusCode = 404;
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);
  HttpResponse response;
  ASSERT_EQ(Get(client, "http://example.invalid/", {}, RequestOptions{}, response), Status::Ok);
  EXPECT_EQ(response.statusCode, 404);
}

TEST(Get, EmptyResponseBodyWithNoHeadersPassesThrough) {
  FakeTransport transport;
  HttpResponse scripted;
  scripted.statusCode = 204;
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);
  HttpResponse response;
  ASSERT_EQ(Get(client, "http://example.invalid/", {}, RequestOptions{}, response), Status::Ok);
  EXPECT_TRUE(response.body.empty());
  EXPECT_TRUE(response.headers.empty());
}
