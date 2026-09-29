// Coverage for src/http/async-text-api.*, against FakeTransport. Offline
// only, same as the other tests/http files -- no test here opens a socket
// or sleeps; synchronization goes through FakeTransport's gate API and
// AsyncEngine::Await.

#include "http/async-text-api.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "core/status.h"
#include "fake-transport.h"
#include "http/async-operations.h"

namespace {

constexpr std::int32_t kStatusSentinel = -777;
constexpr std::uint32_t kLengthSentinel = 0xDEADBEEFu;
constexpr std::uint32_t kIdSentinel = 0xDEADBEEFu;

// Mirrors what a CAPL caller passes: a NUL-terminated buffer plus the size
// elcount() would report for it (text length + 1 for the terminator).
struct TextParam {
  std::string text;
  explicit TextParam(std::string value) : text(std::move(value)) {}
  const char* Data() const { return text.c_str(); }
  std::uint32_t Size() const { return static_cast<std::uint32_t>(text.size() + 1); }
};

// AsyncEngine's destructor never joins/waits for its workers -- correct for
// the production singleton, which outlives every worker, but not for a
// short-lived per-test instance. This fake gives the fixture an explicit
// JoinAll() to call before teardown instead.
class JoiningWorkerLifetime : public WorkerLifetime {
 public:
  void* AcquireModuleReference() override { return this; }
  void ReleaseModuleReference(void*) override {}

  bool StartThread(AsyncThreadEntry entry, void* param) override {
    std::lock_guard<std::mutex> lock(mutex_);
    threads_.emplace_back(entry, param);
    return true;
  }

  void ExitCurrentThread(void*) override {}

  void JoinAll() {
    std::vector<std::thread> threads;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      threads = std::move(threads_);
    }
    for (auto& t : threads) {
      if (t.joinable()) {
        t.join();
      }
    }
  }

 private:
  std::mutex mutex_;
  std::vector<std::thread> threads_;
};

class AsyncTextApiTest : public ::testing::Test {
 protected:
  FakeTransport transport;
  HttpClient client{transport};
  JoiningWorkerLifetime lifetime;
  AsyncEngine engine{lifetime, DefaultIdSeedSource(), std::chrono::milliseconds(50)};
  std::vector<std::string> gatedUrls_;

  // A failed ASSERT_* can leave a test's gate open (entered but never
  // Release()'d); without this, JoinAll() below would hang the whole binary
  // instead of just failing that one test.
  void TearDown() override {
    for (const auto& url : gatedUrls_) {
      transport.Release(url, Status::RequestCancelled, HttpResponse{});
    }
    lifetime.JoinAll();
  }

  // Blocks (via Await, no sleep) until the given id's slot leaves
  // Pending/Running.
  void AwaitOrDie(std::uint32_t requestId) {
    ASSERT_EQ(engine.Await(requestId, 5000), Status::Ok);
  }

  void ExpectGateTracked(const std::string& url) {
    gatedUrls_.push_back(url);
    transport.ExpectGate(url);
  }
};

}  // namespace

// ---------------------------------------------------------------------------
// DispatchGetAsync
// ---------------------------------------------------------------------------

TEST_F(AsyncTextApiTest, DispatchGetAsyncValidRequestReturnsOkAndAssignsRequestId) {
  HttpResponse scripted;
  scripted.statusCode = 200;
  scripted.body = "hello";
  transport.SetResult(Status::Ok, scripted);

  TextParam url("http://example.invalid/");
  TextParam headers("");
  std::uint32_t requestId = kIdSentinel;

  Status result =
      DispatchGetAsync(client, engine, url.Data(), url.Size(), headers.Data(), headers.Size(), requestId);

  EXPECT_EQ(result, Status::Ok);
  EXPECT_NE(requestId, 0u);

  AwaitOrDie(requestId);
  EXPECT_EQ(engine.Discard(requestId), Status::Ok);
}

TEST_F(AsyncTextApiTest, DispatchGetAsyncUnterminatedUrlTextIsUnterminatedInputTextBeforeDispatch) {
  std::array<char, 4> urlBuffer;
  urlBuffer.fill('x');
  TextParam headers("");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchGetAsync(client, engine, urlBuffer.data(),
                                    static_cast<std::uint32_t>(urlBuffer.size()), headers.Data(),
                                    headers.Size(), requestId);

  EXPECT_EQ(result, Status::UnterminatedInputText);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

TEST_F(AsyncTextApiTest, DispatchGetAsyncZeroSizeUrlIsInvalidArgumentBeforeDispatch) {
  TextParam headers("");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchGetAsync(client, engine, "unused", 0, headers.Data(), headers.Size(), requestId);

  EXPECT_EQ(result, Status::InvalidArgument);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

TEST_F(AsyncTextApiTest, DispatchGetAsyncEmptyUrlAfterBoundIsInvalidArgumentBeforeDispatch) {
  TextParam url("");
  TextParam headers("");
  std::uint32_t requestId = kIdSentinel;

  Status result =
      DispatchGetAsync(client, engine, url.Data(), url.Size(), headers.Data(), headers.Size(), requestId);

  EXPECT_EQ(result, Status::InvalidArgument);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

TEST_F(AsyncTextApiTest, DispatchGetAsyncUnterminatedHeadersTextIsUnterminatedInputTextBeforeDispatch) {
  TextParam url("http://example.invalid/");
  std::array<char, 4> headersBuffer;
  headersBuffer.fill('x');
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchGetAsync(client, engine, url.Data(), url.Size(), headersBuffer.data(),
                                    static_cast<std::uint32_t>(headersBuffer.size()), requestId);

  EXPECT_EQ(result, Status::UnterminatedInputText);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

TEST_F(AsyncTextApiTest, DispatchGetAsyncMalformedHeaderBlockIsRejectedBeforeDispatch) {
  TextParam url("http://example.invalid/");
  TextParam headers("NoColonHere");
  std::uint32_t requestId = kIdSentinel;

  Status result =
      DispatchGetAsync(client, engine, url.Data(), url.Size(), headers.Data(), headers.Size(), requestId);

  EXPECT_EQ(result, Status::MalformedHeaderBlock);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

// ---------------------------------------------------------------------------
// DispatchDeleteAsync -- shares DispatchNoBodyVerb with DispatchGetAsync
// above, so coverage here is limited to method mapping plus one rejection
// path rather than repeating every shared bound.
// ---------------------------------------------------------------------------

TEST_F(AsyncTextApiTest, DispatchDeleteAsyncValidRequestMapsMethodAndReturnsOk) {
  transport.SetResult(Status::Ok, HttpResponse{});

  TextParam url("http://example.invalid/widgets/1");
  TextParam headers("");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchDeleteAsync(client, engine, url.Data(), url.Size(), headers.Data(),
                                       headers.Size(), requestId);

  EXPECT_EQ(result, Status::Ok);
  EXPECT_NE(requestId, 0u);

  AwaitOrDie(requestId);
  EXPECT_EQ(transport.LastRequest().method, HttpMethod::Delete);
  EXPECT_EQ(engine.Discard(requestId), Status::Ok);
}

TEST_F(AsyncTextApiTest, DispatchDeleteAsyncUnterminatedUrlTextIsUnterminatedInputTextBeforeDispatch) {
  std::array<char, 4> urlBuffer;
  urlBuffer.fill('x');
  TextParam headers("");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchDeleteAsync(client, engine, urlBuffer.data(),
                                       static_cast<std::uint32_t>(urlBuffer.size()), headers.Data(),
                                       headers.Size(), requestId);

  EXPECT_EQ(result, Status::UnterminatedInputText);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

// ---------------------------------------------------------------------------
// DispatchPostAsync
// ---------------------------------------------------------------------------

TEST_F(AsyncTextApiTest, DispatchPostAsyncValidRequestForwardsBodyAndReturnsOk) {
  HttpResponse scripted;
  scripted.statusCode = 201;
  scripted.body = "created";
  transport.SetResult(Status::Ok, scripted);

  TextParam url("http://example.invalid/widgets");
  TextParam headers("Content-Type: application/json");
  TextParam body("{\"n\":1}");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchPostAsync(client, engine, url.Data(), url.Size(), headers.Data(),
                                     headers.Size(), body.Data(), body.Size(), requestId);

  EXPECT_EQ(result, Status::Ok);
  EXPECT_NE(requestId, 0u);

  AwaitOrDie(requestId);
  EXPECT_EQ(transport.LastRequest().method, HttpMethod::Post);
  EXPECT_EQ(transport.LastRequest().body, "{\"n\":1}");
  ASSERT_EQ(transport.LastRequest().headers.size(), 1u);
  EXPECT_EQ(transport.LastRequest().headers[0].name, "Content-Type");
  EXPECT_EQ(engine.Discard(requestId), Status::Ok);
}

TEST_F(AsyncTextApiTest, DispatchPostAsyncUnterminatedUrlTextIsUnterminatedInputTextBeforeDispatch) {
  std::array<char, 4> urlBuffer;
  urlBuffer.fill('x');
  TextParam headers("");
  TextParam body("payload");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchPostAsync(client, engine, urlBuffer.data(),
                                     static_cast<std::uint32_t>(urlBuffer.size()), headers.Data(),
                                     headers.Size(), body.Data(), body.Size(), requestId);

  EXPECT_EQ(result, Status::UnterminatedInputText);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

TEST_F(AsyncTextApiTest, DispatchPostAsyncZeroSizeUrlIsInvalidArgumentBeforeDispatch) {
  TextParam headers("");
  TextParam body("payload");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchPostAsync(client, engine, "unused", 0, headers.Data(), headers.Size(),
                                     body.Data(), body.Size(), requestId);

  EXPECT_EQ(result, Status::InvalidArgument);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

TEST_F(AsyncTextApiTest, DispatchPostAsyncUnterminatedHeadersTextIsUnterminatedInputTextBeforeDispatch) {
  TextParam url("http://example.invalid/");
  std::array<char, 4> headersBuffer;
  headersBuffer.fill('x');
  TextParam body("payload");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchPostAsync(client, engine, url.Data(), url.Size(), headersBuffer.data(),
                                     static_cast<std::uint32_t>(headersBuffer.size()), body.Data(),
                                     body.Size(), requestId);

  EXPECT_EQ(result, Status::UnterminatedInputText);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

TEST_F(AsyncTextApiTest, DispatchPostAsyncMalformedHeaderBlockIsRejectedBeforeDispatch) {
  TextParam url("http://example.invalid/");
  TextParam headers("X-Foo:   ");
  TextParam body("payload");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchPostAsync(client, engine, url.Data(), url.Size(), headers.Data(),
                                     headers.Size(), body.Data(), body.Size(), requestId);

  EXPECT_EQ(result, Status::MalformedHeaderBlock);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

TEST_F(AsyncTextApiTest, DispatchPostAsyncUnterminatedBodyTextIsUnterminatedInputTextBeforeDispatch) {
  TextParam url("http://example.invalid/");
  TextParam headers("");
  std::array<char, 4> bodyBuffer;
  bodyBuffer.fill('x');
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchPostAsync(client, engine, url.Data(), url.Size(), headers.Data(),
                                     headers.Size(), bodyBuffer.data(),
                                     static_cast<std::uint32_t>(bodyBuffer.size()), requestId);

  EXPECT_EQ(result, Status::UnterminatedInputText);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

TEST_F(AsyncTextApiTest, DispatchPostAsyncEmptyUrlAfterBoundIsInvalidArgumentBeforeDispatch) {
  TextParam url("");
  TextParam headers("");
  TextParam body("payload");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchPostAsync(client, engine, url.Data(), url.Size(), headers.Data(),
                                     headers.Size(), body.Data(), body.Size(), requestId);

  EXPECT_EQ(result, Status::InvalidArgument);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

// ---------------------------------------------------------------------------
// DispatchPutAsync, DispatchPatchAsync -- share DispatchBodyVerb with
// DispatchPostAsync above, so coverage here is limited to method mapping
// plus one rejection path per function rather than repeating every shared
// bound.
// ---------------------------------------------------------------------------

TEST_F(AsyncTextApiTest, DispatchPutAsyncValidRequestForwardsBodyAndReturnsOk) {
  HttpResponse scripted;
  scripted.statusCode = 200;
  scripted.body = "updated";
  transport.SetResult(Status::Ok, scripted);

  TextParam url("http://example.invalid/widgets/1");
  TextParam headers("");
  TextParam body("{\"n\":2}");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchPutAsync(client, engine, url.Data(), url.Size(), headers.Data(),
                                    headers.Size(), body.Data(), body.Size(), requestId);

  EXPECT_EQ(result, Status::Ok);
  EXPECT_NE(requestId, 0u);

  AwaitOrDie(requestId);
  EXPECT_EQ(transport.LastRequest().method, HttpMethod::Put);
  EXPECT_EQ(transport.LastRequest().body, "{\"n\":2}");
  EXPECT_EQ(engine.Discard(requestId), Status::Ok);
}

TEST_F(AsyncTextApiTest, DispatchPutAsyncMalformedHeaderBlockIsRejectedBeforeDispatch) {
  TextParam url("http://example.invalid/");
  TextParam headers("   : value");
  TextParam body("payload");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchPutAsync(client, engine, url.Data(), url.Size(), headers.Data(),
                                    headers.Size(), body.Data(), body.Size(), requestId);

  EXPECT_EQ(result, Status::MalformedHeaderBlock);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

TEST_F(AsyncTextApiTest, DispatchPatchAsyncValidRequestForwardsBodyAndReturnsOk) {
  HttpResponse scripted;
  scripted.statusCode = 200;
  scripted.body = "patched";
  transport.SetResult(Status::Ok, scripted);

  TextParam url("http://example.invalid/widgets/1");
  TextParam headers("");
  TextParam body("{\"n\":3}");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchPatchAsync(client, engine, url.Data(), url.Size(), headers.Data(),
                                      headers.Size(), body.Data(), body.Size(), requestId);

  EXPECT_EQ(result, Status::Ok);
  EXPECT_NE(requestId, 0u);

  AwaitOrDie(requestId);
  EXPECT_EQ(transport.LastRequest().method, HttpMethod::Patch);
  EXPECT_EQ(transport.LastRequest().body, "{\"n\":3}");
  EXPECT_EQ(engine.Discard(requestId), Status::Ok);
}

TEST_F(AsyncTextApiTest, DispatchPatchAsyncUnterminatedBodyTextIsUnterminatedInputTextBeforeDispatch) {
  TextParam url("http://example.invalid/");
  TextParam headers("");
  std::array<char, 4> bodyBuffer;
  bodyBuffer.fill('x');
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchPatchAsync(client, engine, url.Data(), url.Size(), headers.Data(),
                                      headers.Size(), bodyBuffer.data(),
                                      static_cast<std::uint32_t>(bodyBuffer.size()), requestId);

  EXPECT_EQ(result, Status::UnterminatedInputText);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

// ---------------------------------------------------------------------------
// DispatchRequestAsync -- the generic dispatch, matching
// ExecuteRequestSync's validation case by case, plus timeout/cap forwarding.
// ---------------------------------------------------------------------------

TEST_F(AsyncTextApiTest, DispatchRequestAsyncValidLowercaseMethodMapsCorrectlyAndReturnsOk) {
  HttpResponse scripted;
  scripted.statusCode = 204;
  transport.SetResult(Status::Ok, scripted);

  TextParam method("delete");
  TextParam url("http://example.invalid/widgets/1");
  TextParam headers("");
  TextParam body("");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchRequestAsync(client, engine, method.Data(), method.Size(), url.Data(),
                                        url.Size(), headers.Data(), headers.Size(), body.Data(),
                                        body.Size(), 0, 0, 0, requestId);

  EXPECT_EQ(result, Status::Ok);
  EXPECT_NE(requestId, 0u);

  AwaitOrDie(requestId);
  EXPECT_EQ(transport.LastRequest().method, HttpMethod::Delete);
  EXPECT_EQ(engine.Discard(requestId), Status::Ok);
}

// HEAD has no dedicated verb dispatch (same as sync) -- DispatchRequestAsync
// is the only way to reach it.
TEST_F(AsyncTextApiTest, DispatchRequestAsyncHeadMethodIsOnlyReachableThroughThisFunction) {
  HttpResponse scripted;
  scripted.statusCode = 200;
  transport.SetResult(Status::Ok, scripted);

  TextParam method("HEAD");
  TextParam url("http://example.invalid/");
  TextParam headers("");
  TextParam body("");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchRequestAsync(client, engine, method.Data(), method.Size(), url.Data(),
                                        url.Size(), headers.Data(), headers.Size(), body.Data(),
                                        body.Size(), 0, 0, 0, requestId);

  EXPECT_EQ(result, Status::Ok);
  AwaitOrDie(requestId);
  EXPECT_EQ(transport.LastRequest().method, HttpMethod::Head);
  EXPECT_EQ(engine.Discard(requestId), Status::Ok);
}

TEST_F(AsyncTextApiTest, DispatchRequestAsyncTimeoutsAndCapAreForwardedIntoRequestOptions) {
  transport.SetResult(Status::Ok, HttpResponse{});

  TextParam method("GET");
  TextParam url("http://example.invalid/");
  TextParam headers("");
  TextParam body("");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchRequestAsync(client, engine, method.Data(), method.Size(), url.Data(),
                                        url.Size(), headers.Data(), headers.Size(), body.Data(),
                                        body.Size(), 1234, 5678, 999, requestId);

  ASSERT_EQ(result, Status::Ok);
  AwaitOrDie(requestId);
  const RequestOptions& options = transport.LastRequest().options;
  EXPECT_EQ(options.connectTimeoutMs, 1234u);
  EXPECT_EQ(options.totalTimeoutMs, 5678u);
  EXPECT_EQ(options.maxResponseBytes, 999u);
  EXPECT_FALSE(options.skipTlsVerification);
  EXPECT_EQ(engine.Discard(requestId), Status::Ok);
}

TEST_F(AsyncTextApiTest, DispatchRequestAsyncUnknownMethodIsRejectedBeforeDispatch) {
  TextParam method("OPTIONS");
  TextParam url("http://example.invalid/");
  TextParam headers("");
  TextParam body("");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchRequestAsync(client, engine, method.Data(), method.Size(), url.Data(),
                                        url.Size(), headers.Data(), headers.Size(), body.Data(),
                                        body.Size(), 0, 0, 0, requestId);

  EXPECT_EQ(result, Status::UnknownHttpMethod);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

TEST_F(AsyncTextApiTest, DispatchRequestAsyncMalformedHeaderBlockIsRejectedBeforeDispatch) {
  TextParam method("GET");
  TextParam url("http://example.invalid/");
  TextParam headers("NoColonHere");
  TextParam body("");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchRequestAsync(client, engine, method.Data(), method.Size(), url.Data(),
                                        url.Size(), headers.Data(), headers.Size(), body.Data(),
                                        body.Size(), 0, 0, 0, requestId);

  EXPECT_EQ(result, Status::MalformedHeaderBlock);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

TEST_F(AsyncTextApiTest, DispatchRequestAsyncUnterminatedMethodTextIsRejectedBeforeDispatch) {
  std::array<char, 4> methodBuffer;
  methodBuffer.fill('x');
  TextParam url("http://example.invalid/");
  TextParam headers("");
  TextParam body("");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchRequestAsync(
      client, engine, methodBuffer.data(), static_cast<std::uint32_t>(methodBuffer.size()), url.Data(),
      url.Size(), headers.Data(), headers.Size(), body.Data(), body.Size(), 0, 0, 0, requestId);

  EXPECT_EQ(result, Status::UnterminatedInputText);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

TEST_F(AsyncTextApiTest, DispatchRequestAsyncUnterminatedUrlTextIsRejectedBeforeDispatch) {
  TextParam method("GET");
  std::array<char, 4> urlBuffer;
  urlBuffer.fill('x');
  TextParam headers("");
  TextParam body("");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchRequestAsync(client, engine, method.Data(), method.Size(), urlBuffer.data(),
                                        static_cast<std::uint32_t>(urlBuffer.size()), headers.Data(),
                                        headers.Size(), body.Data(), body.Size(), 0, 0, 0, requestId);

  EXPECT_EQ(result, Status::UnterminatedInputText);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

TEST_F(AsyncTextApiTest, DispatchRequestAsyncUnterminatedHeadersTextIsRejectedBeforeDispatch) {
  TextParam method("GET");
  TextParam url("http://example.invalid/");
  std::array<char, 4> headersBuffer;
  headersBuffer.fill('x');
  TextParam body("");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchRequestAsync(client, engine, method.Data(), method.Size(), url.Data(),
                                        url.Size(), headersBuffer.data(),
                                        static_cast<std::uint32_t>(headersBuffer.size()), body.Data(),
                                        body.Size(), 0, 0, 0, requestId);

  EXPECT_EQ(result, Status::UnterminatedInputText);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

TEST_F(AsyncTextApiTest, DispatchRequestAsyncUnterminatedBodyTextIsRejectedBeforeDispatch) {
  TextParam method("POST");
  TextParam url("http://example.invalid/");
  TextParam headers("");
  std::array<char, 4> bodyBuffer;
  bodyBuffer.fill('x');
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchRequestAsync(client, engine, method.Data(), method.Size(), url.Data(),
                                        url.Size(), headers.Data(), headers.Size(), bodyBuffer.data(),
                                        static_cast<std::uint32_t>(bodyBuffer.size()), 0, 0, 0, requestId);

  EXPECT_EQ(result, Status::UnterminatedInputText);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

TEST_F(AsyncTextApiTest, DispatchRequestAsyncZeroSizeUrlIsInvalidArgumentBeforeDispatch) {
  TextParam method("GET");
  TextParam headers("");
  TextParam body("");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchRequestAsync(client, engine, method.Data(), method.Size(), "unused", 0,
                                        headers.Data(), headers.Size(), body.Data(), body.Size(), 0, 0,
                                        0, requestId);

  EXPECT_EQ(result, Status::InvalidArgument);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

TEST_F(AsyncTextApiTest, DispatchRequestAsyncEmptyUrlAfterBoundIsInvalidArgumentBeforeDispatch) {
  TextParam method("GET");
  TextParam url("");
  TextParam headers("");
  TextParam body("");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchRequestAsync(client, engine, method.Data(), method.Size(), url.Data(),
                                        url.Size(), headers.Data(), headers.Size(), body.Data(),
                                        body.Size(), 0, 0, 0, requestId);

  EXPECT_EQ(result, Status::InvalidArgument);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

TEST_F(AsyncTextApiTest, DispatchRequestAsyncBodyOnGetIsInvalidArgumentBeforeDispatch) {
  TextParam method("GET");
  TextParam url("http://example.invalid/");
  TextParam headers("");
  TextParam body("unexpected");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchRequestAsync(client, engine, method.Data(), method.Size(), url.Data(),
                                        url.Size(), headers.Data(), headers.Size(), body.Data(),
                                        body.Size(), 0, 0, 0, requestId);

  EXPECT_EQ(result, Status::InvalidArgument);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

TEST_F(AsyncTextApiTest, DispatchRequestAsyncBodyOnDeleteIsInvalidArgumentBeforeDispatch) {
  TextParam method("DELETE");
  TextParam url("http://example.invalid/");
  TextParam headers("");
  TextParam body("unexpected");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchRequestAsync(client, engine, method.Data(), method.Size(), url.Data(),
                                        url.Size(), headers.Data(), headers.Size(), body.Data(),
                                        body.Size(), 0, 0, 0, requestId);

  EXPECT_EQ(result, Status::InvalidArgument);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

// Trap: do not relax this expectation to match async-text-api.cpp's local
// ForbidsBody, which currently omits HttpMethod::Head. sync-operations.cpp's
// ForbidsBody -- the function this dispatch path is required to match --
// forbids a body on Head as well as Get/Delete, and HEAD is only reachable
// through this function, so a mismatch here lets a HEAD body silently reach
// the transport.
TEST_F(AsyncTextApiTest, DispatchRequestAsyncBodyOnHeadIsInvalidArgumentBeforeDispatch) {
  TextParam method("HEAD");
  TextParam url("http://example.invalid/");
  TextParam headers("");
  TextParam body("unexpected");
  std::uint32_t requestId = kIdSentinel;

  Status result = DispatchRequestAsync(client, engine, method.Data(), method.Size(), url.Data(),
                                        url.Size(), headers.Data(), headers.Size(), body.Data(),
                                        body.Size(), 0, 0, 0, requestId);

  EXPECT_EQ(result, Status::InvalidArgument);
  EXPECT_EQ(requestId, 0u);
  EXPECT_EQ(transport.CallCount(), 0);
}

// ---------------------------------------------------------------------------
// PollAsyncResponse
// ---------------------------------------------------------------------------

TEST_F(AsyncTextApiTest, PollAsyncResponseNeverIssuedIdReturnsUnknownRequestId) {
  std::int32_t state = kStatusSentinel;
  EXPECT_EQ(PollAsyncResponse(engine, 12345u, state), Status::UnknownRequestId);
  EXPECT_EQ(state, 0);
}

TEST_F(AsyncTextApiTest, PollAsyncResponseReportsPendingThenComplete) {
  const std::string url = "http://example.invalid/poll";
  ExpectGateTracked(url);

  HttpRequest request;
  request.method = HttpMethod::Get;
  request.url = url;
  std::uint32_t requestId = 0;
  ASSERT_EQ(engine.Dispatch(client, request, requestId), Status::Ok);
  ASSERT_TRUE(transport.WaitForGateEntered(url));

  std::int32_t state = kStatusSentinel;
  EXPECT_EQ(PollAsyncResponse(engine, requestId, state), Status::Ok);
  EXPECT_EQ(state, 1);

  transport.Release(url, Status::Ok, HttpResponse{});
  AwaitOrDie(requestId);

  state = kStatusSentinel;
  EXPECT_EQ(PollAsyncResponse(engine, requestId, state), Status::Ok);
  EXPECT_EQ(state, 2);

  EXPECT_EQ(engine.Discard(requestId), Status::Ok);
}

// ---------------------------------------------------------------------------
// AwaitAsyncResponse
// ---------------------------------------------------------------------------

TEST_F(AsyncTextApiTest, AwaitAsyncResponseNeverIssuedIdReturnsUnknownRequestId) {
  EXPECT_EQ(AwaitAsyncResponse(engine, 12345u, 5000), Status::UnknownRequestId);
}

TEST_F(AsyncTextApiTest, AwaitAsyncResponseReturnsOkOnCompletion) {
  transport.SetResult(Status::Ok, HttpResponse{});

  HttpRequest request;
  request.method = HttpMethod::Get;
  request.url = "http://example.invalid/await";
  std::uint32_t requestId = 0;
  ASSERT_EQ(engine.Dispatch(client, request, requestId), Status::Ok);

  EXPECT_EQ(AwaitAsyncResponse(engine, requestId, 5000), Status::Ok);
  EXPECT_EQ(engine.Discard(requestId), Status::Ok);
}

// ---------------------------------------------------------------------------
// ReadAsyncResponse -- D12's write order: zero the three out-parameters on
// entry, then write them (if the slot is Complete) before attempting the
// copy, so a copy failure never erases what a retry needs.
// ---------------------------------------------------------------------------

TEST_F(AsyncTextApiTest, ReadAsyncResponseNeverIssuedIdZeroesOutParamsAndReturnsUnknownRequestId) {
  char buffer[64];
  std::int32_t requestStatus = kStatusSentinel;
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ReadAsyncResponse(engine, 12345u, buffer, sizeof(buffer), requestStatus,
                                     httpStatusCode, responseBodyLength);

  EXPECT_EQ(result, Status::UnknownRequestId);
  EXPECT_EQ(requestStatus, 0);
  EXPECT_EQ(httpStatusCode, 0);
  EXPECT_EQ(responseBodyLength, 0u);
}

TEST_F(AsyncTextApiTest, ReadAsyncResponseWhilePendingZeroesOutParamsAndReturnsRequestNotComplete) {
  const std::string url = "http://example.invalid/read-pending";
  ExpectGateTracked(url);

  HttpRequest request;
  request.method = HttpMethod::Get;
  request.url = url;
  std::uint32_t requestId = 0;
  ASSERT_EQ(engine.Dispatch(client, request, requestId), Status::Ok);
  ASSERT_TRUE(transport.WaitForGateEntered(url));

  char buffer[64];
  std::int32_t requestStatus = kStatusSentinel;
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ReadAsyncResponse(engine, requestId, buffer, sizeof(buffer), requestStatus,
                                     httpStatusCode, responseBodyLength);

  EXPECT_EQ(result, Status::RequestNotComplete);
  EXPECT_EQ(requestStatus, 0);
  EXPECT_EQ(httpStatusCode, 0);
  EXPECT_EQ(responseBodyLength, 0u);

  transport.Release(url, Status::Ok, HttpResponse{});
  AwaitOrDie(requestId);
  EXPECT_EQ(engine.Discard(requestId), Status::Ok);
}

TEST_F(AsyncTextApiTest, ReadAsyncResponseWritesStatusesBeforeCopyEvenWhenBufferTooSmall) {
  HttpResponse scripted;
  scripted.statusCode = 200;
  scripted.body = "0123456789";
  transport.SetResult(Status::Ok, scripted);

  HttpRequest request;
  request.method = HttpMethod::Get;
  request.url = "http://example.invalid/read-too-small";
  std::uint32_t requestId = 0;
  ASSERT_EQ(engine.Dispatch(client, request, requestId), Status::Ok);
  AwaitOrDie(requestId);

  std::array<char, 4> buffer{};
  buffer.fill('Z');
  std::int32_t requestStatus = kStatusSentinel;
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ReadAsyncResponse(engine, requestId, buffer.data(),
                                     static_cast<std::uint32_t>(buffer.size()), requestStatus,
                                     httpStatusCode, responseBodyLength);

  EXPECT_EQ(result, Status::BufferTooSmall);
  EXPECT_EQ(requestStatus, 0);
  EXPECT_EQ(httpStatusCode, 200);
  EXPECT_EQ(responseBodyLength, 10u);
  EXPECT_EQ(buffer[0], '\0');

  // The slot stays Complete after -2, so a bigger-buffer retry still works.
  std::array<char, 64> retryBuffer{};
  EXPECT_EQ(ReadAsyncResponse(engine, requestId, retryBuffer.data(),
                               static_cast<std::uint32_t>(retryBuffer.size()), requestStatus,
                               httpStatusCode, responseBodyLength),
            Status::Ok);
  EXPECT_STREQ(retryBuffer.data(), "0123456789");
}

TEST_F(AsyncTextApiTest, ReadAsyncResponseNullBufferWritesStatusesBeforeCopyFailure) {
  HttpResponse scripted;
  scripted.statusCode = 200;
  scripted.body = "ok";
  transport.SetResult(Status::Ok, scripted);

  HttpRequest request;
  request.method = HttpMethod::Get;
  request.url = "http://example.invalid/read-null";
  std::uint32_t requestId = 0;
  ASSERT_EQ(engine.Dispatch(client, request, requestId), Status::Ok);
  AwaitOrDie(requestId);

  std::int32_t requestStatus = kStatusSentinel;
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result =
      ReadAsyncResponse(engine, requestId, nullptr, 0, requestStatus, httpStatusCode, responseBodyLength);

  EXPECT_EQ(result, Status::InvalidArgument);
  EXPECT_EQ(requestStatus, 0);
  EXPECT_EQ(httpStatusCode, 200);
  EXPECT_EQ(responseBodyLength, 2u);

  EXPECT_EQ(engine.Discard(requestId), Status::Ok);
}

TEST_F(AsyncTextApiTest, ReadAsyncResponseSuccessConsumesSlotSoSecondReadReturnsUnknownRequestId) {
  HttpResponse scripted;
  scripted.statusCode = 200;
  scripted.body = "ok";
  transport.SetResult(Status::Ok, scripted);

  HttpRequest request;
  request.method = HttpMethod::Get;
  request.url = "http://example.invalid/read-consume";
  std::uint32_t requestId = 0;
  ASSERT_EQ(engine.Dispatch(client, request, requestId), Status::Ok);
  AwaitOrDie(requestId);

  char buffer[64];
  std::int32_t requestStatus = kStatusSentinel;
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  ASSERT_EQ(ReadAsyncResponse(engine, requestId, buffer, sizeof(buffer), requestStatus, httpStatusCode,
                               responseBodyLength),
            Status::Ok);
  EXPECT_STREQ(buffer, "ok");

  requestStatus = kStatusSentinel;
  httpStatusCode = kStatusSentinel;
  responseBodyLength = kLengthSentinel;
  Status result = ReadAsyncResponse(engine, requestId, buffer, sizeof(buffer), requestStatus,
                                     httpStatusCode, responseBodyLength);

  EXPECT_EQ(result, Status::UnknownRequestId);
  EXPECT_EQ(requestStatus, 0);
  EXPECT_EQ(httpStatusCode, 0);
  EXPECT_EQ(responseBodyLength, 0u);
}

TEST_F(AsyncTextApiTest, ReadAsyncResponseTransportFailureReportsZeroHttpStatusInRequestStatus) {
  transport.SetResult(Status::Timeout, HttpResponse{});

  HttpRequest request;
  request.method = HttpMethod::Get;
  request.url = "http://example.invalid/read-timeout";
  std::uint32_t requestId = 0;
  ASSERT_EQ(engine.Dispatch(client, request, requestId), Status::Ok);
  AwaitOrDie(requestId);

  char buffer[64];
  std::int32_t requestStatus = kStatusSentinel;
  std::int32_t httpStatusCode = kStatusSentinel;
  std::uint32_t responseBodyLength = kLengthSentinel;

  Status result = ReadAsyncResponse(engine, requestId, buffer, sizeof(buffer), requestStatus,
                                     httpStatusCode, responseBodyLength);

  EXPECT_EQ(result, Status::Ok);
  EXPECT_EQ(requestStatus, static_cast<std::int32_t>(Status::Timeout));
  EXPECT_EQ(httpStatusCode, 0);
  EXPECT_EQ(responseBodyLength, 0u);
}

// ---------------------------------------------------------------------------
// DiscardAsyncResponse, DiscardAllAsyncResponses
// ---------------------------------------------------------------------------

TEST_F(AsyncTextApiTest, DiscardAsyncResponseNeverIssuedIdReturnsUnknownRequestId) {
  EXPECT_EQ(DiscardAsyncResponse(engine, 12345u), Status::UnknownRequestId);
}

TEST_F(AsyncTextApiTest, DiscardAsyncResponseFreesSlotSoSubsequentReadReturnsUnknownRequestId) {
  transport.SetResult(Status::Ok, HttpResponse{});

  HttpRequest request;
  request.method = HttpMethod::Get;
  request.url = "http://example.invalid/discard";
  std::uint32_t requestId = 0;
  ASSERT_EQ(engine.Dispatch(client, request, requestId), Status::Ok);
  AwaitOrDie(requestId);

  EXPECT_EQ(DiscardAsyncResponse(engine, requestId), Status::Ok);

  char buffer[64];
  std::int32_t requestStatus = 0;
  std::int32_t httpStatusCode = 0;
  std::uint32_t responseBodyLength = 0;
  EXPECT_EQ(ReadAsyncResponse(engine, requestId, buffer, sizeof(buffer), requestStatus, httpStatusCode,
                               responseBodyLength),
            Status::UnknownRequestId);
}

TEST_F(AsyncTextApiTest, DiscardAllAsyncResponsesReportsStillRunningForInFlightRequestAndFreesIdle) {
  const std::string runningUrl = "http://example.invalid/discard-all-running";
  ExpectGateTracked(runningUrl);

  HttpRequest runningRequest;
  runningRequest.method = HttpMethod::Get;
  runningRequest.url = runningUrl;
  std::uint32_t runningId = 0;
  ASSERT_EQ(engine.Dispatch(client, runningRequest, runningId), Status::Ok);
  ASSERT_TRUE(transport.WaitForGateEntered(runningUrl));

  std::uint32_t stillRunning = kIdSentinel;
  EXPECT_EQ(DiscardAllAsyncResponses(engine, stillRunning), Status::Ok);
  EXPECT_EQ(stillRunning, 1u);

  // Discard-all moves a Running slot straight to Abandoned under the same
  // lock, so the id already reads as unknown here -- no need to wait for
  // the worker to actually unwind out of the gated Perform call.
  std::int32_t state = kStatusSentinel;
  EXPECT_EQ(PollAsyncResponse(engine, runningId, state), Status::UnknownRequestId);
}
