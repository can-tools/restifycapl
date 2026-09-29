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
