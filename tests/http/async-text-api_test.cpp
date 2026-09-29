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

  void TearDown() override { lifetime.JoinAll(); }

  // Blocks (via Await, no sleep) until the given id's slot leaves
  // Pending/Running.
  void AwaitOrDie(std::uint32_t requestId) {
    ASSERT_EQ(engine.Await(requestId, 5000), Status::Ok);
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
