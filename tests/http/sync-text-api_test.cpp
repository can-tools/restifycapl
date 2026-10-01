// Coverage for src/http/sync-text-api.*, against FakeTransport. Offline
// only, same as the other tests/http files -- no test here opens a socket.

#include "http/sync-text-api.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "core/status.h"
#include "../test-support/status-print.h"
#include "fake-transport.h"

namespace {

constexpr std::int32_t kStatusSentinel = -777;
constexpr std::uint32_t kLengthSentinel = 0xDEADBEEFu;

// Mirrors what a CAPL caller passes: a NUL-terminated buffer plus the size
// elcount() would report for it (text length + 1 for the terminator).
struct TextParam {
  std::string text;
  explicit TextParam(std::string value) : text(std::move(value)) {}
  const char* Data() const { return text.c_str(); }
  std::uint32_t Size() const { return static_cast<std::uint32_t>(text.size() + 1); }
};

}  // namespace

// ---------------------------------------------------------------------------
// ParseMethodText
// ---------------------------------------------------------------------------

TEST(ParseMethodText, RecognizesGetAcrossCase) {
  HttpMethod out;
  EXPECT_EQ(ParseMethodText("GET", out), Status::Ok);
  EXPECT_EQ(out, HttpMethod::Get);
  EXPECT_EQ(ParseMethodText("get", out), Status::Ok);
  EXPECT_EQ(out, HttpMethod::Get);
  EXPECT_EQ(ParseMethodText("GeT", out), Status::Ok);
  EXPECT_EQ(out, HttpMethod::Get);
}

TEST(ParseMethodText, RecognizesPostAcrossCase) {
  HttpMethod out;
  EXPECT_EQ(ParseMethodText("POST", out), Status::Ok);
  EXPECT_EQ(out, HttpMethod::Post);
  EXPECT_EQ(ParseMethodText("post", out), Status::Ok);
  EXPECT_EQ(out, HttpMethod::Post);
  EXPECT_EQ(ParseMethodText("PoSt", out), Status::Ok);
  EXPECT_EQ(out, HttpMethod::Post);
}

TEST(ParseMethodText, RecognizesPutAcrossCase) {
  HttpMethod out;
  EXPECT_EQ(ParseMethodText("PUT", out), Status::Ok);
  EXPECT_EQ(out, HttpMethod::Put);
  EXPECT_EQ(ParseMethodText("put", out), Status::Ok);
  EXPECT_EQ(out, HttpMethod::Put);
  EXPECT_EQ(ParseMethodText("PuT", out), Status::Ok);
  EXPECT_EQ(out, HttpMethod::Put);
}

TEST(ParseMethodText, RecognizesPatchAcrossCase) {
  HttpMethod out;
  EXPECT_EQ(ParseMethodText("PATCH", out), Status::Ok);
  EXPECT_EQ(out, HttpMethod::Patch);
  EXPECT_EQ(ParseMethodText("patch", out), Status::Ok);
  EXPECT_EQ(out, HttpMethod::Patch);
  EXPECT_EQ(ParseMethodText("PaTcH", out), Status::Ok);
  EXPECT_EQ(out, HttpMethod::Patch);
}

TEST(ParseMethodText, RecognizesDeleteAcrossCase) {
  HttpMethod out;
  EXPECT_EQ(ParseMethodText("DELETE", out), Status::Ok);
  EXPECT_EQ(out, HttpMethod::Delete);
  EXPECT_EQ(ParseMethodText("delete", out), Status::Ok);
  EXPECT_EQ(out, HttpMethod::Delete);
  EXPECT_EQ(ParseMethodText("DeLeTe", out), Status::Ok);
  EXPECT_EQ(out, HttpMethod::Delete);
}

TEST(ParseMethodText, RecognizesHeadAcrossCase) {
  HttpMethod out;
  EXPECT_EQ(ParseMethodText("HEAD", out), Status::Ok);
  EXPECT_EQ(out, HttpMethod::Head);
  EXPECT_EQ(ParseMethodText("head", out), Status::Ok);
  EXPECT_EQ(out, HttpMethod::Head);
  EXPECT_EQ(ParseMethodText("HeAd", out), Status::Ok);
  EXPECT_EQ(out, HttpMethod::Head);
}

TEST(ParseMethodText, UnrecognizedTextIsUnknownHttpMethod) {
  HttpMethod out = HttpMethod::Get;
  EXPECT_EQ(ParseMethodText("OPTIONS", out), Status::UnknownHttpMethod);
}

TEST(ParseMethodText, EmptyTextIsUnknownHttpMethod) {
  HttpMethod out = HttpMethod::Get;
  EXPECT_EQ(ParseMethodText("", out), Status::UnknownHttpMethod);
}

// ---------------------------------------------------------------------------
// ParseHeaderBlock
// ---------------------------------------------------------------------------

TEST(ParseHeaderBlock, EmptyBlockIsOkWithEmptyVector) {
  std::vector<HttpHeader> out;
  EXPECT_EQ(ParseHeaderBlock("", out), Status::Ok);
  EXPECT_TRUE(out.empty());
}

TEST(ParseHeaderBlock, WellFormedMultiHeaderBlockPreservesOrderAndDuplicates) {
  std::vector<HttpHeader> out;
  EXPECT_EQ(ParseHeaderBlock("Accept: application/json\nX-Test: 1\nX-Test: 2", out), Status::Ok);
  ASSERT_EQ(out.size(), 3u);
  EXPECT_EQ(out[0].name, "Accept");
  EXPECT_EQ(out[0].value, "application/json");
  EXPECT_EQ(out[1].name, "X-Test");
  EXPECT_EQ(out[1].value, "1");
  EXPECT_EQ(out[2].name, "X-Test");
  EXPECT_EQ(out[2].value, "2");
}

TEST(ParseHeaderBlock, TrailingCarriageReturnPerLineTolerated) {
  std::vector<HttpHeader> out;
  EXPECT_EQ(ParseHeaderBlock("Accept: application/json\r\nX-Test: 1\r", out), Status::Ok);
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out[0].value, "application/json");
  EXPECT_EQ(out[1].value, "1");
}

TEST(ParseHeaderBlock, LineWithNoColonIsMalformed) {
  std::vector<HttpHeader> out;
  EXPECT_EQ(ParseHeaderBlock("NoColonHere", out), Status::MalformedHeaderBlock);
}

TEST(ParseHeaderBlock, EmptyNameAfterTrimIsMalformed) {
  std::vector<HttpHeader> out;
  EXPECT_EQ(ParseHeaderBlock("   : value", out), Status::MalformedHeaderBlock);
}

// curl_slist_append("X-Foo:") means remove this header, the opposite of
// what an author writing an empty value intends -- rejected, not forwarded.
TEST(ParseHeaderBlock, EmptyValueAfterTrimIsMalformed) {
  std::vector<HttpHeader> out;
  EXPECT_EQ(ParseHeaderBlock("X-Foo:   ", out), Status::MalformedHeaderBlock);
}

// ---------------------------------------------------------------------------
// ExecuteGetSync
// ---------------------------------------------------------------------------

TEST(ExecuteGetSync, ValidRequestReturnsOkWithStatusAndBody) {
  FakeTransport transport;
  HttpResponse scripted;
  scripted.statusCode = 200;
  scripted.body = "hello";
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);

  TextParam url("http://example.invalid/");
  TextParam headers("");
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecuteGetSync(client, url.Data(), url.Size(), headers.Data(), headers.Size(),
                                  responseBody.data(),
                                  static_cast<std::uint32_t>(responseBody.size()), httpStatusCode,
                                  responseBodyLength);

  EXPECT_EQ(result, Status::Ok);
  EXPECT_EQ(httpStatusCode, 200);
  EXPECT_EQ(responseBodyLength, 5u);
  EXPECT_STREQ(responseBody.data(), "hello");
  EXPECT_EQ(transport.CallCount(), 1);
  EXPECT_EQ(transport.LastRequest().method, HttpMethod::Get);
  // No CAPL-visible parameter can set this to true -- MakeOptions always
  // forwards the false default regardless of what the caller passed in.
  EXPECT_FALSE(transport.LastRequest().options.skipTlsVerification);
}

TEST(ExecuteGetSync, NonTwoHundredHttpStatusStillReturnsOkWithCodeReported) {
  FakeTransport transport;
  HttpResponse scripted;
  scripted.statusCode = 500;
  scripted.body = "server error";
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);

  TextParam url("http://example.invalid/");
  TextParam headers("");
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecuteGetSync(client, url.Data(), url.Size(), headers.Data(), headers.Size(),
                                  responseBody.data(),
                                  static_cast<std::uint32_t>(responseBody.size()), httpStatusCode,
                                  responseBodyLength);

  EXPECT_EQ(result, Status::Ok);
  EXPECT_EQ(httpStatusCode, 500);
  EXPECT_STREQ(responseBody.data(), "server error");
}

// The core of the write-ordering rule: a copy failure must not erase the
// status/length the caller needs in order to retry with a bigger buffer.
TEST(ExecuteGetSync, ResponseLargerThanBufferReportsStatusAndLengthDespiteCopyFailure) {
  FakeTransport transport;
  HttpResponse scripted;
  scripted.statusCode = 200;
  scripted.body = "0123456789";
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);

  TextParam url("http://example.invalid/");
  TextParam headers("");
  std::array<char, 4> responseBody{};
  responseBody.fill('Z');
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecuteGetSync(client, url.Data(), url.Size(), headers.Data(), headers.Size(),
                                  responseBody.data(),
                                  static_cast<std::uint32_t>(responseBody.size()), httpStatusCode,
                                  responseBodyLength);

  EXPECT_EQ(result, Status::BufferTooSmall);
  EXPECT_EQ(httpStatusCode, 200);
  EXPECT_EQ(responseBodyLength, 10u);
  EXPECT_EQ(responseBody[0], '\0');
}

TEST(ExecuteGetSync, TransportErrorReportsZeroStatusAndZeroLength) {
  FakeTransport transport;
  transport.SetResult(Status::NetworkError, HttpResponse{});
  HttpClient client(transport);

  TextParam url("http://example.invalid/");
  TextParam headers("");
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecuteGetSync(client, url.Data(), url.Size(), headers.Data(), headers.Size(),
                                  responseBody.data(),
                                  static_cast<std::uint32_t>(responseBody.size()), httpStatusCode,
                                  responseBodyLength);

  EXPECT_EQ(result, Status::NetworkError);
  EXPECT_EQ(httpStatusCode, 0);
  EXPECT_EQ(responseBodyLength, 0u);
}

TEST(ExecuteGetSync, MalformedHeaderBlockIsRejectedBeforeAnyTransportCall) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  TextParam url("http://example.invalid/");
  TextParam headers("NoColonHere");
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecuteGetSync(client, url.Data(), url.Size(), headers.Data(), headers.Size(),
                                  responseBody.data(),
                                  static_cast<std::uint32_t>(responseBody.size()), httpStatusCode,
                                  responseBodyLength);

  EXPECT_EQ(result, Status::MalformedHeaderBlock);
  EXPECT_EQ(transport.CallCount(), 0);
  EXPECT_EQ(httpStatusCode, kStatusSentinel);
  EXPECT_EQ(responseBodyLength, kLengthSentinel);
}

TEST(ExecuteGetSync, UnterminatedUrlTextIsRejectedBeforeAnyTransportCall) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  std::array<char, 4> urlBuffer;
  urlBuffer.fill('x');
  TextParam headers("");
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecuteGetSync(client, urlBuffer.data(),
                                  static_cast<std::uint32_t>(urlBuffer.size()), headers.Data(),
                                  headers.Size(), responseBody.data(),
                                  static_cast<std::uint32_t>(responseBody.size()), httpStatusCode,
                                  responseBodyLength);

  EXPECT_EQ(result, Status::UnterminatedInputText);
  EXPECT_EQ(transport.CallCount(), 0);
  EXPECT_EQ(httpStatusCode, kStatusSentinel);
  EXPECT_EQ(responseBodyLength, kLengthSentinel);
}

TEST(ExecuteGetSync, ZeroSizeUrlIsInvalidArgumentBeforeAnyTransportCall) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  TextParam headers("");
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecuteGetSync(client, "unused", 0, headers.Data(), headers.Size(),
                                  responseBody.data(),
                                  static_cast<std::uint32_t>(responseBody.size()), httpStatusCode,
                                  responseBodyLength);

  EXPECT_EQ(result, Status::InvalidArgument);
  EXPECT_EQ(transport.CallCount(), 0);
  EXPECT_EQ(httpStatusCode, kStatusSentinel);
  EXPECT_EQ(responseBodyLength, kLengthSentinel);
}

// ---------------------------------------------------------------------------
// ExecutePostSync
// ---------------------------------------------------------------------------

TEST(ExecutePostSync, ValidRequestForwardsBodyAndReturnsOk) {
  FakeTransport transport;
  HttpResponse scripted;
  scripted.statusCode = 201;
  scripted.body = "created";
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);

  TextParam url("http://example.invalid/widgets");
  TextParam headers("Content-Type: application/json");
  TextParam body("{\"n\":1}");
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecutePostSync(client, url.Data(), url.Size(), headers.Data(), headers.Size(),
                                   body.Data(), body.Size(), responseBody.data(),
                                   static_cast<std::uint32_t>(responseBody.size()), httpStatusCode,
                                   responseBodyLength);

  EXPECT_EQ(result, Status::Ok);
  EXPECT_EQ(httpStatusCode, 201);
  EXPECT_STREQ(responseBody.data(), "created");
  EXPECT_EQ(transport.LastRequest().method, HttpMethod::Post);
  EXPECT_EQ(transport.LastRequest().body, "{\"n\":1}");
  ASSERT_EQ(transport.LastRequest().headers.size(), 1u);
  EXPECT_EQ(transport.LastRequest().headers[0].name, "Content-Type");
  EXPECT_FALSE(transport.LastRequest().options.skipTlsVerification);
}

TEST(ExecutePostSync, NonTwoHundredHttpStatusStillReturnsOkWithCodeReported) {
  FakeTransport transport;
  HttpResponse scripted;
  scripted.statusCode = 500;
  scripted.body = "server error";
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);

  TextParam url("http://example.invalid/");
  TextParam headers("");
  TextParam body("payload");
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecutePostSync(client, url.Data(), url.Size(), headers.Data(), headers.Size(),
                                   body.Data(), body.Size(), responseBody.data(),
                                   static_cast<std::uint32_t>(responseBody.size()), httpStatusCode,
                                   responseBodyLength);

  EXPECT_EQ(result, Status::Ok);
  EXPECT_EQ(httpStatusCode, 500);
}

TEST(ExecutePostSync, ResponseLargerThanBufferReportsStatusAndLengthDespiteCopyFailure) {
  FakeTransport transport;
  HttpResponse scripted;
  scripted.statusCode = 200;
  scripted.body = "0123456789";
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);

  TextParam url("http://example.invalid/");
  TextParam headers("");
  TextParam body("payload");
  std::array<char, 4> responseBody{};
  responseBody.fill('Z');
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecutePostSync(client, url.Data(), url.Size(), headers.Data(), headers.Size(),
                                   body.Data(), body.Size(), responseBody.data(),
                                   static_cast<std::uint32_t>(responseBody.size()), httpStatusCode,
                                   responseBodyLength);

  EXPECT_EQ(result, Status::BufferTooSmall);
  EXPECT_EQ(httpStatusCode, 200);
  EXPECT_EQ(responseBodyLength, 10u);
  EXPECT_EQ(responseBody[0], '\0');
}

TEST(ExecutePostSync, TransportErrorReportsZeroStatusAndZeroLength) {
  FakeTransport transport;
  transport.SetResult(Status::Timeout, HttpResponse{});
  HttpClient client(transport);

  TextParam url("http://example.invalid/");
  TextParam headers("");
  TextParam body("payload");
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecutePostSync(client, url.Data(), url.Size(), headers.Data(), headers.Size(),
                                   body.Data(), body.Size(), responseBody.data(),
                                   static_cast<std::uint32_t>(responseBody.size()), httpStatusCode,
                                   responseBodyLength);

  EXPECT_EQ(result, Status::Timeout);
  EXPECT_EQ(httpStatusCode, 0);
  EXPECT_EQ(responseBodyLength, 0u);
}

TEST(ExecutePostSync, MalformedHeaderBlockIsRejectedBeforeAnyTransportCall) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  TextParam url("http://example.invalid/");
  TextParam headers("X-Foo:   ");
  TextParam body("payload");
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecutePostSync(client, url.Data(), url.Size(), headers.Data(), headers.Size(),
                                   body.Data(), body.Size(), responseBody.data(),
                                   static_cast<std::uint32_t>(responseBody.size()), httpStatusCode,
                                   responseBodyLength);

  EXPECT_EQ(result, Status::MalformedHeaderBlock);
  EXPECT_EQ(transport.CallCount(), 0);
  EXPECT_EQ(httpStatusCode, kStatusSentinel);
  EXPECT_EQ(responseBodyLength, kLengthSentinel);
}

TEST(ExecutePostSync, UnterminatedBodyTextIsRejectedBeforeAnyTransportCall) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  TextParam url("http://example.invalid/");
  TextParam headers("");
  std::array<char, 4> bodyBuffer;
  bodyBuffer.fill('x');
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecutePostSync(client, url.Data(), url.Size(), headers.Data(), headers.Size(),
                                   bodyBuffer.data(), static_cast<std::uint32_t>(bodyBuffer.size()),
                                   responseBody.data(),
                                   static_cast<std::uint32_t>(responseBody.size()), httpStatusCode,
                                   responseBodyLength);

  EXPECT_EQ(result, Status::UnterminatedInputText);
  EXPECT_EQ(transport.CallCount(), 0);
  EXPECT_EQ(httpStatusCode, kStatusSentinel);
  EXPECT_EQ(responseBodyLength, kLengthSentinel);
}

// ---------------------------------------------------------------------------
// ExecuteRequestSync
// ---------------------------------------------------------------------------

TEST(ExecuteRequestSync, ValidLowercaseMethodMapsCorrectlyAndReturnsOk) {
  FakeTransport transport;
  HttpResponse scripted;
  scripted.statusCode = 204;
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);

  TextParam method("delete");
  TextParam url("http://example.invalid/widgets/1");
  TextParam headers("");
  TextParam body("");
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecuteRequestSync(
      client, method.Data(), method.Size(), url.Data(), url.Size(), headers.Data(), headers.Size(),
      body.Data(), body.Size(), 0, 0, 0, responseBody.data(),
      static_cast<std::uint32_t>(responseBody.size()), httpStatusCode, responseBodyLength);

  EXPECT_EQ(result, Status::Ok);
  EXPECT_EQ(httpStatusCode, 204);
  EXPECT_EQ(transport.LastRequest().method, HttpMethod::Delete);
}

// HEAD has no dedicated verb row -- restifyRequestSync is the only way
// to reach it, and it forbids a body the same as GET/DELETE.
TEST(ExecuteRequestSync, HeadMethodIsOnlyReachableThroughThisFunction) {
  FakeTransport transport;
  HttpResponse scripted;
  scripted.statusCode = 200;
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);

  TextParam method("HEAD");
  TextParam url("http://example.invalid/");
  TextParam headers("");
  TextParam body("");
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecuteRequestSync(
      client, method.Data(), method.Size(), url.Data(), url.Size(), headers.Data(), headers.Size(),
      body.Data(), body.Size(), 0, 0, 0, responseBody.data(),
      static_cast<std::uint32_t>(responseBody.size()), httpStatusCode, responseBodyLength);

  EXPECT_EQ(result, Status::Ok);
  EXPECT_EQ(transport.LastRequest().method, HttpMethod::Head);
}

TEST(ExecuteRequestSync, TimeoutsAndCapAreForwardedIntoRequestOptions) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  TextParam method("GET");
  TextParam url("http://example.invalid/");
  TextParam headers("");
  TextParam body("");
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecuteRequestSync(client, method.Data(), method.Size(), url.Data(), url.Size(),
                                      headers.Data(), headers.Size(), body.Data(), body.Size(), 1234,
                                      5678, 999, responseBody.data(),
                                      static_cast<std::uint32_t>(responseBody.size()), httpStatusCode,
                                      responseBodyLength);

  ASSERT_EQ(result, Status::Ok);
  const RequestOptions& options = transport.LastRequest().options;
  EXPECT_EQ(options.connectTimeoutMs, 1234u);
  EXPECT_EQ(options.totalTimeoutMs, 5678u);
  EXPECT_EQ(options.maxResponseBytes, 999u);
  // No parameter in the CAPL-visible signature can set this true -- it is
  // always the RequestOptions default regardless of the other inputs.
  EXPECT_FALSE(options.skipTlsVerification);
}

TEST(ExecuteRequestSync, ResponseLargerThanBufferReportsStatusAndLengthDespiteCopyFailure) {
  FakeTransport transport;
  HttpResponse scripted;
  scripted.statusCode = 200;
  scripted.body = "0123456789";
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);

  TextParam method("GET");
  TextParam url("http://example.invalid/");
  TextParam headers("");
  TextParam body("");
  std::array<char, 4> responseBody{};
  responseBody.fill('Z');
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecuteRequestSync(
      client, method.Data(), method.Size(), url.Data(), url.Size(), headers.Data(), headers.Size(),
      body.Data(), body.Size(), 0, 0, 0, responseBody.data(),
      static_cast<std::uint32_t>(responseBody.size()), httpStatusCode, responseBodyLength);

  EXPECT_EQ(result, Status::BufferTooSmall);
  EXPECT_EQ(httpStatusCode, 200);
  EXPECT_EQ(responseBodyLength, 10u);
  EXPECT_EQ(responseBody[0], '\0');
}

TEST(ExecuteRequestSync, TransportErrorReportsZeroStatusAndZeroLength) {
  FakeTransport transport;
  transport.SetResult(Status::TlsError, HttpResponse{});
  HttpClient client(transport);

  TextParam method("GET");
  TextParam url("https://example.invalid/");
  TextParam headers("");
  TextParam body("");
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecuteRequestSync(
      client, method.Data(), method.Size(), url.Data(), url.Size(), headers.Data(), headers.Size(),
      body.Data(), body.Size(), 0, 0, 0, responseBody.data(),
      static_cast<std::uint32_t>(responseBody.size()), httpStatusCode, responseBodyLength);

  EXPECT_EQ(result, Status::TlsError);
  EXPECT_EQ(httpStatusCode, 0);
  EXPECT_EQ(responseBodyLength, 0u);
}

TEST(ExecuteRequestSync, UnknownMethodIsRejectedBeforeAnyTransportCall) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  TextParam method("OPTIONS");
  TextParam url("http://example.invalid/");
  TextParam headers("");
  TextParam body("");
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecuteRequestSync(
      client, method.Data(), method.Size(), url.Data(), url.Size(), headers.Data(), headers.Size(),
      body.Data(), body.Size(), 0, 0, 0, responseBody.data(),
      static_cast<std::uint32_t>(responseBody.size()), httpStatusCode, responseBodyLength);

  EXPECT_EQ(result, Status::UnknownHttpMethod);
  EXPECT_EQ(transport.CallCount(), 0);
  EXPECT_EQ(httpStatusCode, kStatusSentinel);
  EXPECT_EQ(responseBodyLength, kLengthSentinel);
}

TEST(ExecuteRequestSync, MalformedHeaderBlockIsRejectedBeforeAnyTransportCall) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  TextParam method("GET");
  TextParam url("http://example.invalid/");
  TextParam headers("NoColonHere");
  TextParam body("");
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecuteRequestSync(
      client, method.Data(), method.Size(), url.Data(), url.Size(), headers.Data(), headers.Size(),
      body.Data(), body.Size(), 0, 0, 0, responseBody.data(),
      static_cast<std::uint32_t>(responseBody.size()), httpStatusCode, responseBodyLength);

  EXPECT_EQ(result, Status::MalformedHeaderBlock);
  EXPECT_EQ(transport.CallCount(), 0);
  EXPECT_EQ(httpStatusCode, kStatusSentinel);
  EXPECT_EQ(responseBodyLength, kLengthSentinel);
}

TEST(ExecuteRequestSync, UnterminatedMethodTextIsRejectedBeforeAnyTransportCall) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  std::array<char, 4> methodBuffer;
  methodBuffer.fill('x');
  TextParam url("http://example.invalid/");
  TextParam headers("");
  TextParam body("");
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecuteRequestSync(client, methodBuffer.data(),
                                      static_cast<std::uint32_t>(methodBuffer.size()), url.Data(),
                                      url.Size(), headers.Data(), headers.Size(), body.Data(),
                                      body.Size(), 0, 0, 0, responseBody.data(),
                                      static_cast<std::uint32_t>(responseBody.size()), httpStatusCode,
                                      responseBodyLength);

  EXPECT_EQ(result, Status::UnterminatedInputText);
  EXPECT_EQ(transport.CallCount(), 0);
  EXPECT_EQ(httpStatusCode, kStatusSentinel);
  EXPECT_EQ(responseBodyLength, kLengthSentinel);
}

// sync-operations::Request rejects a non-empty body on Get/Head/Delete
// before the transport is reached; this layer must not bypass that check.
TEST(ExecuteRequestSync, BodyOnGetIsInvalidArgumentBeforeAnyTransportCall) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  TextParam method("GET");
  TextParam url("http://example.invalid/");
  TextParam headers("");
  TextParam body("unexpected");
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecuteRequestSync(
      client, method.Data(), method.Size(), url.Data(), url.Size(), headers.Data(), headers.Size(),
      body.Data(), body.Size(), 0, 0, 0, responseBody.data(),
      static_cast<std::uint32_t>(responseBody.size()), httpStatusCode, responseBodyLength);

  EXPECT_EQ(result, Status::InvalidArgument);
  EXPECT_EQ(transport.CallCount(), 0);
  EXPECT_EQ(httpStatusCode, kStatusSentinel);
  EXPECT_EQ(responseBodyLength, kLengthSentinel);
}

TEST(ExecuteRequestSync, BodyOnHeadIsInvalidArgumentBeforeAnyTransportCall) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  TextParam method("HEAD");
  TextParam url("http://example.invalid/");
  TextParam headers("");
  TextParam body("unexpected");
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecuteRequestSync(
      client, method.Data(), method.Size(), url.Data(), url.Size(), headers.Data(), headers.Size(),
      body.Data(), body.Size(), 0, 0, 0, responseBody.data(),
      static_cast<std::uint32_t>(responseBody.size()), httpStatusCode, responseBodyLength);

  EXPECT_EQ(result, Status::InvalidArgument);
  EXPECT_EQ(transport.CallCount(), 0);
  EXPECT_EQ(httpStatusCode, kStatusSentinel);
  EXPECT_EQ(responseBodyLength, kLengthSentinel);
}

TEST(ExecuteRequestSync, BodyOnDeleteIsInvalidArgumentBeforeAnyTransportCall) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  TextParam method("DELETE");
  TextParam url("http://example.invalid/");
  TextParam headers("");
  TextParam body("unexpected");
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecuteRequestSync(
      client, method.Data(), method.Size(), url.Data(), url.Size(), headers.Data(), headers.Size(),
      body.Data(), body.Size(), 0, 0, 0, responseBody.data(),
      static_cast<std::uint32_t>(responseBody.size()), httpStatusCode, responseBodyLength);

  EXPECT_EQ(result, Status::InvalidArgument);
  EXPECT_EQ(transport.CallCount(), 0);
  EXPECT_EQ(httpStatusCode, kStatusSentinel);
  EXPECT_EQ(responseBodyLength, kLengthSentinel);
}

// ---------------------------------------------------------------------------
// ExecuteDeleteSync, ExecutePutSync, ExecutePatchSync -- share
// ExecuteNoBodyVerb/ExecuteBodyVerb with ExecuteGetSync/ExecutePostSync
// above, so coverage here is limited to method mapping plus one rejection
// path per function rather than repeating every shared behavior.
// ---------------------------------------------------------------------------

TEST(ExecuteDeleteSync, ValidRequestMapsMethodAndReturnsOk) {
  FakeTransport transport;
  HttpResponse scripted;
  scripted.statusCode = 204;
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);

  TextParam url("http://example.invalid/widgets/1");
  TextParam headers("");
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecuteDeleteSync(client, url.Data(), url.Size(), headers.Data(), headers.Size(),
                                     responseBody.data(),
                                     static_cast<std::uint32_t>(responseBody.size()), httpStatusCode,
                                     responseBodyLength);

  EXPECT_EQ(result, Status::Ok);
  EXPECT_EQ(httpStatusCode, 204);
  EXPECT_EQ(transport.LastRequest().method, HttpMethod::Delete);
}

TEST(ExecuteDeleteSync, UnterminatedHeadersTextIsRejectedBeforeAnyTransportCall) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  TextParam url("http://example.invalid/");
  std::array<char, 4> headersBuffer;
  headersBuffer.fill('x');
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecuteDeleteSync(client, url.Data(), url.Size(), headersBuffer.data(),
                                     static_cast<std::uint32_t>(headersBuffer.size()),
                                     responseBody.data(),
                                     static_cast<std::uint32_t>(responseBody.size()), httpStatusCode,
                                     responseBodyLength);

  EXPECT_EQ(result, Status::UnterminatedInputText);
  EXPECT_EQ(transport.CallCount(), 0);
  EXPECT_EQ(httpStatusCode, kStatusSentinel);
  EXPECT_EQ(responseBodyLength, kLengthSentinel);
}

TEST(ExecutePutSync, ValidRequestForwardsBodyAndReturnsOk) {
  FakeTransport transport;
  HttpResponse scripted;
  scripted.statusCode = 200;
  scripted.body = "updated";
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);

  TextParam url("http://example.invalid/widgets/1");
  TextParam headers("");
  TextParam body("{\"n\":2}");
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecutePutSync(client, url.Data(), url.Size(), headers.Data(), headers.Size(),
                                  body.Data(), body.Size(), responseBody.data(),
                                  static_cast<std::uint32_t>(responseBody.size()), httpStatusCode,
                                  responseBodyLength);

  EXPECT_EQ(result, Status::Ok);
  EXPECT_EQ(httpStatusCode, 200);
  EXPECT_STREQ(responseBody.data(), "updated");
  EXPECT_EQ(transport.LastRequest().method, HttpMethod::Put);
  EXPECT_EQ(transport.LastRequest().body, "{\"n\":2}");
}

TEST(ExecutePutSync, MalformedHeaderBlockIsRejectedBeforeAnyTransportCall) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  TextParam url("http://example.invalid/");
  TextParam headers("   : value");
  TextParam body("payload");
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecutePutSync(client, url.Data(), url.Size(), headers.Data(), headers.Size(),
                                  body.Data(), body.Size(), responseBody.data(),
                                  static_cast<std::uint32_t>(responseBody.size()), httpStatusCode,
                                  responseBodyLength);

  EXPECT_EQ(result, Status::MalformedHeaderBlock);
  EXPECT_EQ(transport.CallCount(), 0);
  EXPECT_EQ(httpStatusCode, kStatusSentinel);
  EXPECT_EQ(responseBodyLength, kLengthSentinel);
}

TEST(ExecutePatchSync, ValidRequestForwardsBodyAndReturnsOk) {
  FakeTransport transport;
  HttpResponse scripted;
  scripted.statusCode = 200;
  scripted.body = "patched";
  transport.SetResult(Status::Ok, scripted);
  HttpClient client(transport);

  TextParam url("http://example.invalid/widgets/1");
  TextParam headers("");
  TextParam body("{\"n\":3}");
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecutePatchSync(client, url.Data(), url.Size(), headers.Data(), headers.Size(),
                                    body.Data(), body.Size(), responseBody.data(),
                                    static_cast<std::uint32_t>(responseBody.size()), httpStatusCode,
                                    responseBodyLength);

  EXPECT_EQ(result, Status::Ok);
  EXPECT_EQ(httpStatusCode, 200);
  EXPECT_STREQ(responseBody.data(), "patched");
  EXPECT_EQ(transport.LastRequest().method, HttpMethod::Patch);
  EXPECT_EQ(transport.LastRequest().body, "{\"n\":3}");
}

TEST(ExecutePatchSync, UnterminatedBodyTextIsRejectedBeforeAnyTransportCall) {
  FakeTransport transport;
  transport.SetResult(Status::Ok, HttpResponse{});
  HttpClient client(transport);

  TextParam url("http://example.invalid/");
  TextParam headers("");
  std::array<char, 4> bodyBuffer;
  bodyBuffer.fill('x');
  std::array<char, 64> responseBody{};
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ExecutePatchSync(client, url.Data(), url.Size(), headers.Data(), headers.Size(),
                                    bodyBuffer.data(), static_cast<std::uint32_t>(bodyBuffer.size()),
                                    responseBody.data(),
                                    static_cast<std::uint32_t>(responseBody.size()), httpStatusCode,
                                    responseBodyLength);

  EXPECT_EQ(result, Status::UnterminatedInputText);
  EXPECT_EQ(transport.CallCount(), 0);
  EXPECT_EQ(httpStatusCode, kStatusSentinel);
  EXPECT_EQ(responseBodyLength, kLengthSentinel);
}
