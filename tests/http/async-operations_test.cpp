// Coverage for src/http/async-operations.*, against FakeTransport. Offline
// only, same as sync-operations_test.cpp -- no test here opens a socket or
// sleeps; synchronization goes through FakeTransport's gate API.

#include "http/async-operations.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "core/status.h"
#include "fake-transport.h"

namespace {

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

class FixedIdSeedSource : public IdSeedSource {
 public:
  explicit FixedIdSeedSource(std::uint32_t seed) : seed_(seed) {}

  std::uint32_t NextSeed() override { return seed_; }

 private:
  std::uint32_t seed_;
};

class AsyncEngineTest : public ::testing::Test {
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

  HttpRequest MakeRequest(const std::string& url) {
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = url;
    return request;
  }

  void ExpectGateTracked(const std::string& url) {
    gatedUrls_.push_back(url);
    transport.ExpectGate(url);
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
  ExpectGateTracked(url);

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
// Case 1: all 8 slots occupied by in-flight requests -> a 9th Dispatch gets
// NoFreeRequestSlot.
// ---------------------------------------------------------------------------

TEST_F(AsyncEngineTest, DispatchBeyondSlotCountReturnsNoFreeRequestSlot) {
  std::vector<std::string> urls;
  for (std::size_t i = 0; i < kAsyncRequestSlotCount; ++i) {
    urls.push_back("http://example.invalid/case1-" + std::to_string(i));
  }

  std::vector<std::uint32_t> requestIds;
  for (const auto& url : urls) {
    ExpectGateTracked(url);
    requestIds.push_back(DispatchOrDie(url));
  }
  for (const auto& url : urls) {
    ASSERT_TRUE(transport.WaitForGateEntered(url));
  }

  std::uint32_t extraRequestId = 0;
  HttpRequest extraRequest = MakeRequest("http://example.invalid/case1-extra");
  EXPECT_EQ(engine.Dispatch(client, extraRequest, extraRequestId), Status::NoFreeRequestSlot);
  EXPECT_EQ(extraRequestId, 0u);

  for (std::size_t i = 0; i < urls.size(); ++i) {
    transport.Release(urls[i], Status::Ok, HttpResponse{});
    AwaitOrDie(requestIds[i]);
  }
}

// ---------------------------------------------------------------------------
// Case 10: DiscardAll frees every non-Running slot immediately and reports
// the Running count (now Abandoned, cancelled but not yet actually stopped)
// in stillRunning.
// ---------------------------------------------------------------------------

TEST_F(AsyncEngineTest, DiscardAllFreesCompleteAndConsumedSlotsAndReportsStillRunning) {
  const std::string runningUrl = "http://example.invalid/case11-running";
  ExpectGateTracked(runningUrl);
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

// ---------------------------------------------------------------------------
// Case 2: completions released in an order different from dispatch order
// still reach the correct request id -- no id/response mixup.
// ---------------------------------------------------------------------------

TEST_F(AsyncEngineTest, OutOfOrderCompletionsReachTheCorrectRequestId) {
  const std::string urlA = "http://example.invalid/case2-a";
  const std::string urlB = "http://example.invalid/case2-b";
  const std::string urlC = "http://example.invalid/case2-c";
  ExpectGateTracked(urlA);
  ExpectGateTracked(urlB);
  ExpectGateTracked(urlC);

  const std::uint32_t idA = DispatchOrDie(urlA);
  const std::uint32_t idB = DispatchOrDie(urlB);
  const std::uint32_t idC = DispatchOrDie(urlC);
  ASSERT_TRUE(transport.WaitForGateEntered(urlA));
  ASSERT_TRUE(transport.WaitForGateEntered(urlB));
  ASSERT_TRUE(transport.WaitForGateEntered(urlC));

  HttpResponse responseA;
  responseA.statusCode = 201;
  responseA.body = "body-a";
  HttpResponse responseB;
  responseB.statusCode = 202;
  responseB.body = "body-b";
  HttpResponse responseC;
  responseC.statusCode = 203;
  responseC.body = "body-c";

  // Released out of dispatch order: C, then A, then B.
  transport.Release(urlC, Status::Ok, responseC);
  AwaitOrDie(idC);
  transport.Release(urlA, Status::Ok, responseA);
  AwaitOrDie(idA);
  transport.Release(urlB, Status::Ok, responseB);
  AwaitOrDie(idB);

  char buffer[64];
  std::int32_t requestStatus = -1;
  std::int32_t httpStatusCode = -1;
  std::uint32_t responseBodyLength = 0;

  ASSERT_EQ(engine.Read(idA, buffer, sizeof(buffer), requestStatus, httpStatusCode,
                         responseBodyLength),
            Status::Ok);
  EXPECT_EQ(httpStatusCode, 201);
  EXPECT_STREQ(buffer, "body-a");

  ASSERT_EQ(engine.Read(idB, buffer, sizeof(buffer), requestStatus, httpStatusCode,
                         responseBodyLength),
            Status::Ok);
  EXPECT_EQ(httpStatusCode, 202);
  EXPECT_STREQ(buffer, "body-b");

  ASSERT_EQ(engine.Read(idC, buffer, sizeof(buffer), requestStatus, httpStatusCode,
                         responseBodyLength),
            Status::Ok);
  EXPECT_EQ(httpStatusCode, 203);
  EXPECT_STREQ(buffer, "body-c");
}

// ---------------------------------------------------------------------------
// Case 3: reading a Complete slot to full completion frees it (Consumed
// counts as available for dispatch, same as Free) -- a subsequent Dispatch
// reclaims it instead of failing with NoFreeRequestSlot.
// ---------------------------------------------------------------------------

TEST_F(AsyncEngineTest, ReadThenDispatchReclaimsConsumedSlot) {
  std::vector<std::string> runningUrls;
  for (std::size_t i = 0; i < kAsyncRequestSlotCount - 1; ++i) {
    runningUrls.push_back("http://example.invalid/case3-running-" + std::to_string(i));
  }
  std::vector<std::uint32_t> runningIds;
  for (const auto& url : runningUrls) {
    ExpectGateTracked(url);
    runningIds.push_back(DispatchOrDie(url));
  }
  for (const auto& url : runningUrls) {
    ASSERT_TRUE(transport.WaitForGateEntered(url));
  }

  HttpResponse scripted;
  scripted.statusCode = 200;
  scripted.body = "consume-me";
  transport.SetResult(Status::Ok, scripted);
  const std::uint32_t completeId = DispatchOrDie("http://example.invalid/case3-complete");
  AwaitOrDie(completeId);

  // All 8 slots are now occupied (7 Running + 1 Complete) -- confirm dispatch
  // is blocked before reclaiming any slot.
  std::uint32_t blockedRequestId = 0;
  HttpRequest blockedRequest = MakeRequest("http://example.invalid/case3-blocked");
  EXPECT_EQ(engine.Dispatch(client, blockedRequest, blockedRequestId), Status::NoFreeRequestSlot);

  char buffer[64];
  std::int32_t requestStatus = -1;
  std::int32_t httpStatusCode = -1;
  std::uint32_t responseBodyLength = 0;
  ASSERT_EQ(engine.Read(completeId, buffer, sizeof(buffer), requestStatus, httpStatusCode,
                         responseBodyLength),
            Status::Ok);
  EXPECT_STREQ(buffer, "consume-me");

  const std::string reclaimedUrl = "http://example.invalid/case3-reclaimed";
  ExpectGateTracked(reclaimedUrl);
  const std::uint32_t reclaimedId = DispatchOrDie(reclaimedUrl);
  EXPECT_NE(reclaimedId, 0u);
  ASSERT_TRUE(transport.WaitForGateEntered(reclaimedUrl));

  // All 8 slots are occupied again -- a further dispatch is blocked once more.
  std::uint32_t stillBlockedRequestId = 0;
  EXPECT_EQ(engine.Dispatch(client, blockedRequest, stillBlockedRequestId),
            Status::NoFreeRequestSlot);

  for (std::size_t i = 0; i < runningUrls.size(); ++i) {
    transport.Release(runningUrls[i], Status::Ok, HttpResponse{});
    AwaitOrDie(runningIds[i]);
  }
  transport.Release(reclaimedUrl, Status::Ok, HttpResponse{});
  AwaitOrDie(reclaimedId);
}

// ---------------------------------------------------------------------------
// Case 4: an undersized (but non-zero) buffer on Read gives BufferTooSmall
// and leaves the slot retryable -- a follow-up Read with a big-enough buffer
// on the same request id succeeds and returns the full body.
// ---------------------------------------------------------------------------

TEST_F(AsyncEngineTest, ReadWithUndersizedBufferIsRetryableWithLargerBuffer) {
  const std::string url = "http://example.invalid/case4";
  HttpResponse scripted;
  scripted.statusCode = 200;
  scripted.body = "this body is longer than the small buffer";
  transport.SetResult(Status::Ok, scripted);

  const std::uint32_t requestId = DispatchOrDie(url);
  AwaitOrDie(requestId);

  char smallBuffer[8];
  std::int32_t requestStatus = -1;
  std::int32_t httpStatusCode = -1;
  std::uint32_t responseBodyLength = 0;
  EXPECT_EQ(engine.Read(requestId, smallBuffer, sizeof(smallBuffer), requestStatus, httpStatusCode,
                         responseBodyLength),
            Status::BufferTooSmall);
  EXPECT_EQ(httpStatusCode, 200);
  EXPECT_EQ(responseBodyLength, scripted.body.size());

  char bigBuffer[128];
  EXPECT_EQ(engine.Read(requestId, bigBuffer, sizeof(bigBuffer), requestStatus, httpStatusCode,
                         responseBodyLength),
            Status::Ok);
  EXPECT_EQ(httpStatusCode, 200);
  EXPECT_STREQ(bigBuffer, scripted.body.c_str());

  EXPECT_EQ(engine.Read(requestId, bigBuffer, sizeof(bigBuffer), requestStatus, httpStatusCode,
                         responseBodyLength),
            Status::UnknownRequestId);
}

// ---------------------------------------------------------------------------
// Case 9: Pending is not exercised here -- the instant a slot is marked
// Pending, an idle or freshly-started worker claims it before any test hook
// can observe or gate that state, so only Running and Complete are covered.
// ---------------------------------------------------------------------------

TEST_F(AsyncEngineTest, DiscardOnRunningSlotDropsResultAndEventuallyFreesSlot) {
  std::vector<std::string> otherUrls;
  for (std::size_t i = 0; i < kAsyncRequestSlotCount - 1; ++i) {
    otherUrls.push_back("http://example.invalid/case9-running-other-" + std::to_string(i));
  }
  std::vector<std::uint32_t> otherIds;
  for (const auto& url : otherUrls) {
    ExpectGateTracked(url);
    otherIds.push_back(DispatchOrDie(url));
  }
  for (const auto& url : otherUrls) {
    ASSERT_TRUE(transport.WaitForGateEntered(url));
  }

  const std::string discardUrl = "http://example.invalid/case9-running-discard";
  ExpectGateTracked(discardUrl);
  const std::uint32_t discardId = DispatchOrDie(discardUrl);
  ASSERT_TRUE(transport.WaitForGateEntered(discardUrl));

  EXPECT_EQ(engine.Discard(discardId), Status::Ok);

  std::int32_t pollState = -1;
  EXPECT_EQ(engine.Poll(discardId, pollState), Status::UnknownRequestId);
  char buffer[64];
  std::int32_t requestStatus = 0;
  std::int32_t httpStatusCode = 0;
  std::uint32_t responseBodyLength = 0;
  EXPECT_EQ(engine.Read(discardId, buffer, sizeof(buffer), requestStatus, httpStatusCode,
                         responseBodyLength),
            Status::UnknownRequestId);

  // The cancelled worker frees its slot asynchronously (once FakeTransport's
  // own poll notices the cancel flag) -- bounded retry instead of a fixed
  // sleep, matching FakeTransport's own gate-wait style.
  const std::string reclaimUrl = "http://example.invalid/case9-running-reclaimed";
  ExpectGateTracked(reclaimUrl);
  std::uint32_t reclaimedId = 0;
  bool reclaimed = false;
  for (int attempt = 0; attempt < 200 && !reclaimed; ++attempt) {
    const Status dispatchStatus = engine.Dispatch(client, MakeRequest(reclaimUrl), reclaimedId);
    if (dispatchStatus == Status::Ok) {
      reclaimed = true;
      break;
    }
    ASSERT_EQ(dispatchStatus, Status::NoFreeRequestSlot);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  ASSERT_TRUE(reclaimed) << "discarded Running slot never became reusable";
  ASSERT_TRUE(transport.WaitForGateEntered(reclaimUrl));

  for (std::size_t i = 0; i < otherUrls.size(); ++i) {
    transport.Release(otherUrls[i], Status::Ok, HttpResponse{});
    AwaitOrDie(otherIds[i]);
  }
  transport.Release(reclaimUrl, Status::Ok, HttpResponse{});
  AwaitOrDie(reclaimedId);
}

// ---------------------------------------------------------------------------
// Case 9 (Complete): a Complete-but-unread slot discards the same way a
// consumed Read does (case 3) -- Discard alone makes it reusable, no worker
// involvement needed.
// ---------------------------------------------------------------------------

TEST_F(AsyncEngineTest, DiscardOnCompleteSlotDropsResultAndFreesSlotImmediately) {
  const std::string url = "http://example.invalid/case9-complete";
  HttpResponse scripted;
  scripted.statusCode = 200;
  scripted.body = "unread-result";
  transport.SetResult(Status::Ok, scripted);

  const std::uint32_t requestId = DispatchOrDie(url);
  AwaitOrDie(requestId);

  std::int32_t pollState = -1;
  ASSERT_EQ(engine.Poll(requestId, pollState), Status::Ok);
  ASSERT_EQ(pollState, 2);

  EXPECT_EQ(engine.Discard(requestId), Status::Ok);

  EXPECT_EQ(engine.Poll(requestId, pollState), Status::UnknownRequestId);
  char buffer[64];
  std::int32_t requestStatus = 0;
  std::int32_t httpStatusCode = 0;
  std::uint32_t responseBodyLength = 0;
  EXPECT_EQ(engine.Read(requestId, buffer, sizeof(buffer), requestStatus, httpStatusCode,
                         responseBodyLength),
            Status::UnknownRequestId);

  const std::string reclaimUrl = "http://example.invalid/case9-complete-reclaimed";
  ExpectGateTracked(reclaimUrl);
  const std::uint32_t reclaimedId = DispatchOrDie(reclaimUrl);
  EXPECT_NE(reclaimedId, 0u);
  ASSERT_TRUE(transport.WaitForGateEntered(reclaimUrl));

  transport.Release(reclaimUrl, Status::Ok, HttpResponse{});
  AwaitOrDie(reclaimedId);
}

// ---------------------------------------------------------------------------
// Case 10: DiscardAll also wakes a worker that has gone idle -- it rechecks
// for pending work right away and exits instead of sitting out the rest of
// its idle timeout. A long idle timeout here is what makes the difference
// between "woken" and "just happened to time out" observable.
// ---------------------------------------------------------------------------

TEST_F(AsyncEngineTest, DiscardAllWakesIdleWorkerToExitPromptly) {
  AsyncEngine longIdleEngine(lifetime, DefaultIdSeedSource(), std::chrono::milliseconds(2000));

  const std::string url = "http://example.invalid/case10-idle-wake";
  ExpectGateTracked(url);
  std::uint32_t requestId = 0;
  ASSERT_EQ(longIdleEngine.Dispatch(client, MakeRequest(url), requestId), Status::Ok);
  ASSERT_TRUE(transport.WaitForGateEntered(url));

  transport.Release(url, Status::Ok, HttpResponse{});
  // Await cannot return until the worker has moved past Complete and back to
  // its own idle wait -- both sides serialize on the same engine mutex.
  ASSERT_EQ(longIdleEngine.Await(requestId, 5000), Status::Ok);

  const auto start = std::chrono::steady_clock::now();
  std::uint32_t stillRunning = 999;
  EXPECT_EQ(longIdleEngine.DiscardAll(stillRunning), Status::Ok);
  EXPECT_EQ(stillRunning, 0u);

  lifetime.JoinAll();
  const auto elapsed = std::chrono::steady_clock::now() - start;
  EXPECT_LT(elapsed, std::chrono::milliseconds(1000))
      << "worker exited only after its idle timeout instead of being woken by DiscardAll";
}

// ---------------------------------------------------------------------------
// Case 12 (0 on completion): Await unblocks with Ok once the request
// transitions to Complete.
// ---------------------------------------------------------------------------

TEST_F(AsyncEngineTest, AwaitReturnsOkWhenRequestCompletesBeforeDeadline) {
  const std::string url = "http://example.invalid/case12-ok";
  ExpectGateTracked(url);
  const std::uint32_t requestId = DispatchOrDie(url);
  ASSERT_TRUE(transport.WaitForGateEntered(url));

  transport.Release(url, Status::Ok, HttpResponse{});
  EXPECT_EQ(engine.Await(requestId, 5000), Status::Ok);

  EXPECT_EQ(engine.Discard(requestId), Status::Ok);
}

// ---------------------------------------------------------------------------
// Case 12 (-28 at the deadline): Await gives up and reports WaitTimeout once
// its bound elapses while the request is still Running.
// ---------------------------------------------------------------------------

TEST_F(AsyncEngineTest, AwaitReturnsWaitTimeoutAtDeadlineWhileStillRunning) {
  const std::string url = "http://example.invalid/case12-timeout";
  ExpectGateTracked(url);
  const std::uint32_t requestId = DispatchOrDie(url);
  ASSERT_TRUE(transport.WaitForGateEntered(url));

  EXPECT_EQ(engine.Await(requestId, 50), Status::WaitTimeout);
}

// ---------------------------------------------------------------------------
// Case 12 (-27 promptly on discard): a concurrent Discard ends a blocked
// Await well before its own deadline -- discard's wake-up, not the timeout,
// is what ends the wait.
// ---------------------------------------------------------------------------

TEST_F(AsyncEngineTest, AwaitReturnsUnknownRequestIdPromptlyOnDiscard) {
  const std::string url = "http://example.invalid/case12-discard";
  ExpectGateTracked(url);
  const std::uint32_t requestId = DispatchOrDie(url);
  ASSERT_TRUE(transport.WaitForGateEntered(url));

  std::future<Status> awaitResult =
      std::async(std::launch::async, [&] { return engine.Await(requestId, 5000); });
  EXPECT_EQ(engine.Discard(requestId), Status::Ok);

  ASSERT_EQ(awaitResult.wait_for(std::chrono::milliseconds(500)), std::future_status::ready);
  EXPECT_EQ(awaitResult.get(), Status::UnknownRequestId);
}

// ---------------------------------------------------------------------------
// Case 12 (-27 promptly on discard-all): same as above, but the request is
// swept up by DiscardAll instead of a single Discard.
// ---------------------------------------------------------------------------

TEST_F(AsyncEngineTest, AwaitReturnsUnknownRequestIdPromptlyOnDiscardAll) {
  const std::string url = "http://example.invalid/case12-discardall";
  ExpectGateTracked(url);
  const std::uint32_t requestId = DispatchOrDie(url);
  ASSERT_TRUE(transport.WaitForGateEntered(url));

  std::future<Status> awaitResult =
      std::async(std::launch::async, [&] { return engine.Await(requestId, 5000); });
  std::uint32_t stillRunning = 0;
  EXPECT_EQ(engine.DiscardAll(stillRunning), Status::Ok);
  EXPECT_EQ(stillRunning, 1u);

  ASSERT_EQ(awaitResult.wait_for(std::chrono::milliseconds(500)), std::future_status::ready);
  EXPECT_EQ(awaitResult.get(), Status::UnknownRequestId);

  transport.Release(url, Status::Ok, HttpResponse{});
}

// ---------------------------------------------------------------------------
// Case 12 (-27 after reuse under a new id): once a slot has been reclaimed by
// a different request, Await on the stale id must not be satisfied by the
// new occupant.
// ---------------------------------------------------------------------------

TEST_F(AsyncEngineTest, AwaitReturnsUnknownRequestIdAfterSlotReusedUnderNewId) {
  transport.SetResult(Status::Ok, HttpResponse{});
  const std::uint32_t staleId = DispatchOrDie("http://example.invalid/case12-stale");
  AwaitOrDie(staleId);
  EXPECT_EQ(engine.Discard(staleId), Status::Ok);

  const std::string reclaimUrl = "http://example.invalid/case12-reclaimed";
  ExpectGateTracked(reclaimUrl);
  const std::uint32_t newId = DispatchOrDie(reclaimUrl);
  ASSERT_TRUE(transport.WaitForGateEntered(reclaimUrl));
  ASSERT_NE(newId, staleId);

  EXPECT_EQ(engine.Await(staleId, 2000), Status::UnknownRequestId);

  transport.Release(reclaimUrl, Status::Ok, HttpResponse{});
  AwaitOrDie(newId);
}

// ---------------------------------------------------------------------------
// Case 12 (waitTimeoutMs = 0 follows the request's own total timeout): 0
// means "use the request's totalTimeoutMs as the bound", not "don't wait".
// ---------------------------------------------------------------------------

TEST_F(AsyncEngineTest, AwaitWithZeroTimeoutUsesRequestsOwnTotalTimeout) {
  const std::string url = "http://example.invalid/case12-zero-timeout";
  ExpectGateTracked(url);

  HttpRequest request = MakeRequest(url);
  request.options.totalTimeoutMs = 50;
  std::uint32_t requestId = 0;
  ASSERT_EQ(engine.Dispatch(client, request, requestId), Status::Ok);
  ASSERT_TRUE(transport.WaitForGateEntered(url));

  const auto start = std::chrono::steady_clock::now();
  EXPECT_EQ(engine.Await(requestId, 0), Status::WaitTimeout);
  const auto elapsed = std::chrono::steady_clock::now() - start;

  EXPECT_GE(elapsed, std::chrono::milliseconds(20));
  EXPECT_LT(elapsed, std::chrono::milliseconds(5000));
}

TEST_F(AsyncEngineTest, PollTransitionsFromStateOneToStateTwoToUnknownRequestId) {
  const std::string url = "http://example.invalid/poll-state-transitions";
  ExpectGateTracked(url);
  const std::uint32_t requestId = DispatchOrDie(url);
  ASSERT_TRUE(transport.WaitForGateEntered(url));

  std::int32_t pollState = -1;
  EXPECT_EQ(engine.Poll(requestId, pollState), Status::Ok);
  EXPECT_EQ(pollState, 1);

  transport.Release(url, Status::Ok, HttpResponse{});
  AwaitOrDie(requestId);

  pollState = -1;
  EXPECT_EQ(engine.Poll(requestId, pollState), Status::Ok);
  EXPECT_EQ(pollState, 2);

  char buffer[64];
  std::int32_t requestStatus = 0;
  std::int32_t httpStatusCode = 0;
  std::uint32_t responseBodyLength = 0;
  ASSERT_EQ(engine.Read(requestId, buffer, sizeof(buffer), requestStatus, httpStatusCode,
                         responseBodyLength),
            Status::Ok);

  pollState = -1;
  EXPECT_EQ(engine.Poll(requestId, pollState), Status::UnknownRequestId);
}

TEST_F(AsyncEngineTest, IdSeedSourceValueBecomesTheFirstMintedRequestId) {
  constexpr std::uint32_t seed = 42;
  FixedIdSeedSource seedSource(seed);
  AsyncEngine seededEngine(lifetime, seedSource, std::chrono::milliseconds(50));
  transport.SetResult(Status::Ok, HttpResponse{});

  std::uint32_t idA = 0;
  ASSERT_EQ(seededEngine.Dispatch(client, MakeRequest("http://example.invalid/case13-seed-a"), idA),
            Status::Ok);
  ASSERT_EQ(seededEngine.Await(idA, 5000), Status::Ok);
  EXPECT_EQ(idA, seed);

  std::uint32_t idB = 0;
  ASSERT_EQ(seededEngine.Dispatch(client, MakeRequest("http://example.invalid/case13-seed-b"), idB),
            Status::Ok);
  ASSERT_EQ(seededEngine.Await(idB, 5000), Status::Ok);
  EXPECT_EQ(idB, seed + 1);

  lifetime.JoinAll();
}

TEST_F(AsyncEngineTest, MintedIdsWrapAroundSkippingZeroAndStayUniqueAcrossTheWrap) {
  // Chosen so the slot count's worth of dispatches crosses the uint32_t
  // wraparound boundary partway through, without needing a full lap.
  constexpr std::uint32_t seed = 0xFFFFFFFDu;
  FixedIdSeedSource seedSource(seed);
  AsyncEngine seededEngine(lifetime, seedSource, std::chrono::milliseconds(50));

  std::vector<std::string> urls;
  std::vector<std::uint32_t> ids;
  for (std::size_t i = 0; i < kAsyncRequestSlotCount; ++i) {
    const std::string url = "http://example.invalid/case13-wrap-" + std::to_string(i);
    urls.push_back(url);
    ExpectGateTracked(url);

    std::uint32_t requestId = 0;
    ASSERT_EQ(seededEngine.Dispatch(client, MakeRequest(url), requestId), Status::Ok);
    ASSERT_TRUE(transport.WaitForGateEntered(url));
    ids.push_back(requestId);
  }

  std::uint32_t expected = seed;
  for (const std::uint32_t id : ids) {
    if (expected == 0) {
      ++expected;
    }
    EXPECT_NE(id, 0u);
    EXPECT_EQ(id, expected);
    ++expected;
  }

  for (std::size_t i = 0; i < ids.size(); ++i) {
    for (std::size_t j = i + 1; j < ids.size(); ++j) {
      EXPECT_NE(ids[i], ids[j]);
    }
  }

  for (std::size_t i = 0; i < urls.size(); ++i) {
    transport.Release(urls[i], Status::Ok, HttpResponse{});
    ASSERT_EQ(seededEngine.Await(ids[i], 5000), Status::Ok);
  }

  lifetime.JoinAll();
}

TEST(ShouldCancelTransferTest, NullFlagNeverCancels) {
  EXPECT_FALSE(ShouldCancelTransfer(nullptr));
}

TEST(ShouldCancelTransferTest, ReflectsFlagsCurrentValue) {
  std::atomic<bool> flag{false};
  EXPECT_FALSE(ShouldCancelTransfer(&flag));

  flag.store(true);
  EXPECT_TRUE(ShouldCancelTransfer(&flag));

  flag.store(false);
  EXPECT_FALSE(ShouldCancelTransfer(&flag));
}
