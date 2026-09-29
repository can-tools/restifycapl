// Coverage for src/http/async-operations.*, against FakeTransport. Offline
// only, same as sync-operations_test.cpp -- no test here opens a socket or
// sleeps; synchronization goes through FakeTransport's gate API.

#include "http/async-operations.h"

#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "core/status.h"
#include "fake-transport.h"

namespace {

// DefaultWorkerLifetime's real implementation starts threads with the raw
// Win32 CreateThread API, which is documented as unsafe to combine with
// CRT-using thread bodies under the statically linked (/MT) runtime this
// project requires -- confirmed here by a reproducible heap-corruption
// crash in the test immediately following any test that dispatched through
// it. This fake uses std::thread (CRT-safe start/join) instead; its
// ExitCurrentThread deliberately returns rather than terminating the
// thread, matching async-operations.cpp's own "reached only ... under a
// test fake" comment on WorkerThreadEntry's fallthrough return.
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

  // Reaps every thread started so far; each one only returns once its
  // worker has sat idle past the engine's (test-shortened) idle timeout,
  // so this blocks for a bounded, deterministic interval rather than a
  // guessed sleep.
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

class AsyncEngineTest : public ::testing::Test {
 protected:
  FakeTransport transport;
  HttpClient client{transport};
  JoiningWorkerLifetime lifetime;
  AsyncEngine engine{lifetime, DefaultIdSeedSource(), std::chrono::milliseconds(50)};

  void TearDown() override { lifetime.JoinAll(); }

  HttpRequest MakeRequest(const std::string& url) {
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = url;
    return request;
  }

  std::uint32_t DispatchOrDie(const std::string& url) {
    std::uint32_t requestId = 0;
    EXPECT_EQ(engine.Dispatch(client, MakeRequest(url), requestId), Status::Ok);
    return requestId;
  }

  // Blocks (via Await, no sleep) until the given id's slot leaves
  // Pending/Running -- used after a scripted (non-gated) result, where the
  // worker races the test thread to Complete.
  void AwaitOrDie(std::uint32_t requestId) {
    ASSERT_EQ(engine.Await(requestId, 5000), Status::Ok);
  }
};

}  // namespace

// ---------------------------------------------------------------------------
// Case 6: Read on a slot that is still Pending/Running -> RequestNotComplete.
// ---------------------------------------------------------------------------

TEST_F(AsyncEngineTest, ReadBeforeCompletionReturnsRequestNotComplete) {
  const std::string url = "http://example.invalid/case6";
  transport.ExpectGate(url);

  const std::uint32_t requestId = DispatchOrDie(url);
  ASSERT_TRUE(transport.WaitForGateEntered(url));

  char buffer[64];
  std::int32_t requestStatus = 123;
  std::int32_t httpStatusCode = 123;
  std::uint32_t responseBodyLength = 123;
  EXPECT_EQ(engine.Read(requestId, buffer, sizeof(buffer), requestStatus, httpStatusCode,
                         responseBodyLength),
            Status::RequestNotComplete);
  EXPECT_EQ(requestStatus, 0);
  EXPECT_EQ(httpStatusCode, 0);
  EXPECT_EQ(responseBodyLength, 0u);

  transport.Release(url, Status::Ok, HttpResponse{});
  AwaitOrDie(requestId);
  EXPECT_EQ(engine.Discard(requestId), Status::Ok);
}

// ---------------------------------------------------------------------------
// Case 5: a zero-size buffer fails CopyToBuffer's own validation before the
// slot is touched, so the slot stays Complete and a follow-up Read with a
// real buffer still succeeds and consumes it; only then does a further Read
// on the same id see UnknownRequestId.
// ---------------------------------------------------------------------------

TEST_F(AsyncEngineTest, ReadWithZeroSizeBufferLeavesSlotRetryableThenConsumesOnRealRead) {
  const std::string url = "http://example.invalid/case5";
  HttpResponse scripted;
  scripted.statusCode = 200;
  scripted.body = "ok";
  transport.SetResult(Status::Ok, scripted);

  const std::uint32_t requestId = DispatchOrDie(url);
  AwaitOrDie(requestId);

  std::int32_t requestStatus = -1;
  std::int32_t httpStatusCode = -1;
  std::uint32_t responseBodyLength = 0;
  EXPECT_EQ(engine.Read(requestId, nullptr, 0, requestStatus, httpStatusCode, responseBodyLength),
            Status::InvalidArgument);
  EXPECT_EQ(requestStatus, 0);
  EXPECT_EQ(httpStatusCode, 200);
  EXPECT_EQ(responseBodyLength, 2u);

  char buffer[64];
  EXPECT_EQ(engine.Read(requestId, buffer, sizeof(buffer), requestStatus, httpStatusCode,
                         responseBodyLength),
            Status::Ok);
  EXPECT_STREQ(buffer, "ok");

  EXPECT_EQ(engine.Read(requestId, buffer, sizeof(buffer), requestStatus, httpStatusCode,
                         responseBodyLength),
            Status::UnknownRequestId);
}

// ---------------------------------------------------------------------------
// Case 8: transport succeeds but the server responded with an HTTP error --
// requestStatus stays 0 (transport-level Ok), httpStatusCode carries the
// real HTTP code.
// ---------------------------------------------------------------------------

TEST_F(AsyncEngineTest, HttpErrorStatusSurfacesInHttpStatusCodeNotRequestStatus) {
  const std::string url = "http://example.invalid/case8";
  HttpResponse scripted;
  scripted.statusCode = 404;
  scripted.body = "not found";
  transport.SetResult(Status::Ok, scripted);

  const std::uint32_t requestId = DispatchOrDie(url);
  AwaitOrDie(requestId);

  char buffer[64];
  std::int32_t requestStatus = -1;
  std::int32_t httpStatusCode = -1;
  std::uint32_t responseBodyLength = 0;
  EXPECT_EQ(engine.Read(requestId, buffer, sizeof(buffer), requestStatus, httpStatusCode,
                         responseBodyLength),
            Status::Ok);
  EXPECT_EQ(requestStatus, 0);
  EXPECT_EQ(httpStatusCode, 404);
  EXPECT_STREQ(buffer, "not found");
}

TEST_F(AsyncEngineTest, HttpServerErrorStatusAlsoSurfacesInHttpStatusCode) {
  const std::string url = "http://example.invalid/case8-5xx";
  HttpResponse scripted;
  scripted.statusCode = 500;
  scripted.body = "server error";
  transport.SetResult(Status::Ok, scripted);

  const std::uint32_t requestId = DispatchOrDie(url);
  AwaitOrDie(requestId);

  char buffer[64];
  std::int32_t requestStatus = -1;
  std::int32_t httpStatusCode = -1;
  std::uint32_t responseBodyLength = 0;
  EXPECT_EQ(engine.Read(requestId, buffer, sizeof(buffer), requestStatus, httpStatusCode,
                         responseBodyLength),
            Status::Ok);
  EXPECT_EQ(requestStatus, 0);
  EXPECT_EQ(httpStatusCode, 500);
}

// ---------------------------------------------------------------------------
// Case 7: the transport itself fails (a sync-error-range code, -18..-23) --
// requestStatus carries it through and httpStatusCode stays 0, since no
// response was ever received.
// ---------------------------------------------------------------------------

TEST_F(AsyncEngineTest, TransportFailureSurfacesInRequestStatusWithZeroHttpStatusCode) {
  const std::string url = "http://example.invalid/case7";
  transport.SetResult(Status::NetworkError, HttpResponse{});

  const std::uint32_t requestId = DispatchOrDie(url);
  AwaitOrDie(requestId);

  char buffer[64];
  std::int32_t requestStatus = 0;
  std::int32_t httpStatusCode = -1;
  std::uint32_t responseBodyLength = 999;
  EXPECT_EQ(engine.Read(requestId, buffer, sizeof(buffer), requestStatus, httpStatusCode,
                         responseBodyLength),
            Status::Ok);
  EXPECT_EQ(requestStatus, static_cast<std::int32_t>(Status::NetworkError));
  EXPECT_EQ(httpStatusCode, 0);
  EXPECT_EQ(responseBodyLength, 0u);
}

TEST_F(AsyncEngineTest, TransportTimeoutSurfacesInRequestStatusWithZeroHttpStatusCode) {
  const std::string url = "http://example.invalid/case7-timeout";
  transport.SetResult(Status::Timeout, HttpResponse{});

  const std::uint32_t requestId = DispatchOrDie(url);
  AwaitOrDie(requestId);

  char buffer[64];
  std::int32_t requestStatus = 0;
  std::int32_t httpStatusCode = -1;
  std::uint32_t responseBodyLength = 999;
  EXPECT_EQ(engine.Read(requestId, buffer, sizeof(buffer), requestStatus, httpStatusCode,
                         responseBodyLength),
            Status::Ok);
  EXPECT_EQ(requestStatus, static_cast<std::int32_t>(Status::Timeout));
  EXPECT_EQ(httpStatusCode, 0);
}

// ---------------------------------------------------------------------------
// Case 11: DiscardAll frees every non-Running slot immediately and reports
// the Running count (now Abandoned, cancelled but not yet actually stopped)
// in stillRunning.
// ---------------------------------------------------------------------------

TEST_F(AsyncEngineTest, DiscardAllFreesCompleteAndConsumedSlotsAndReportsStillRunning) {
  const std::string runningUrl = "http://example.invalid/case11-running";
  transport.ExpectGate(runningUrl);
  const std::uint32_t runningId = DispatchOrDie(runningUrl);
  ASSERT_TRUE(transport.WaitForGateEntered(runningUrl));

  transport.SetResult(Status::Ok, HttpResponse{});
  const std::uint32_t completeId = DispatchOrDie("http://example.invalid/case11-complete");
  AwaitOrDie(completeId);

  const std::uint32_t consumedId = DispatchOrDie("http://example.invalid/case11-consumed");
  AwaitOrDie(consumedId);
  char buffer[64];
  std::int32_t requestStatus = 0;
  std::int32_t httpStatusCode = 0;
  std::uint32_t responseBodyLength = 0;
  ASSERT_EQ(engine.Read(consumedId, buffer, sizeof(buffer), requestStatus, httpStatusCode,
                         responseBodyLength),
            Status::Ok);

  std::uint32_t stillRunning = 999;
  EXPECT_EQ(engine.DiscardAll(stillRunning), Status::Ok);
  EXPECT_EQ(stillRunning, 1u);

  std::int32_t pollState = -1;
  EXPECT_EQ(engine.Poll(completeId, pollState), Status::UnknownRequestId);
  EXPECT_EQ(engine.Poll(consumedId, pollState), Status::UnknownRequestId);
  EXPECT_EQ(engine.Poll(runningId, pollState), Status::UnknownRequestId);

  transport.Release(runningUrl, Status::Ok, HttpResponse{});
}
