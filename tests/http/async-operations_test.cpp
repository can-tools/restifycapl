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

struct RecordedThreadStart {
  AsyncThreadEntry entry;
  void* param;
};

// StartThread only records (entry, param) here instead of starting anything,
// which is what lets a test catch a slot in Pending before any worker claims
// it. Trap: production code increments liveWorkers_ the moment StartThread
// returns true, and its param owns a heap allocation freed only when the
// deferred entry point actually runs -- every test using this fake must call
// StartAllPending() (then JoinAll()) before it ends, or it leaks that
// allocation and desyncs the worker/thread-join accounting.
class DeferredStartWorkerLifetime : public WorkerLifetime {
 public:
  void* AcquireModuleReference() override { return this; }
  void ReleaseModuleReference(void*) override {}

  bool StartThread(AsyncThreadEntry entry, void* param) override {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.push_back(RecordedThreadStart{entry, param});
    return true;
  }

  void ExitCurrentThread(void*) override {}

  void StartAllPending() {
    std::vector<RecordedThreadStart> pending;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      pending = std::move(pending_);
    }
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& start : pending) {
      threads_.emplace_back(start.entry, start.param);
    }
  }

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
  std::vector<RecordedThreadStart> pending_;
  std::vector<std::thread> threads_;
};

// Same immediate-start behavior as JoiningWorkerLifetime, plus call counts on
// the reference/thread lifecycle so a test can confirm a module reference is
// acquired once per worker and only ever given up when that worker's entry
// point actually returns.
class CountingWorkerLifetime : public WorkerLifetime {
 public:
  void* AcquireModuleReference() override {
    ++acquireCount_;
    return this;
  }
  void ReleaseModuleReference(void*) override { ++releaseCount_; }

  bool StartThread(AsyncThreadEntry entry, void* param) override {
    std::lock_guard<std::mutex> lock(mutex_);
    ++startCount_;
    threads_.emplace_back(entry, param);
    return true;
  }

  void ExitCurrentThread(void*) override { ++exitCount_; }

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

  int AcquireCount() const { return acquireCount_.load(); }
  int ReleaseCount() const { return releaseCount_.load(); }
  int StartCount() const { return startCount_.load(); }
  int ExitCount() const { return exitCount_.load(); }

 private:
  std::mutex mutex_;
  std::vector<std::thread> threads_;
  std::atomic<int> acquireCount_{0};
  std::atomic<int> releaseCount_{0};
  std::atomic<int> startCount_{0};
  std::atomic<int> exitCount_{0};
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
// Read on a slot that is still Pending/Running -> RequestNotComplete.
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
// A zero-size buffer fails CopyToBuffer's own validation before the
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
// Transport succeeds but the server responded with an HTTP error --
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
// The transport itself fails (a sync-error-range code, -18..-23) --
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
// All 8 slots occupied by in-flight requests -> a 9th Dispatch gets
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
// DiscardAll frees every non-Running slot immediately and reports
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
// Completions released in an order different from dispatch order
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
// Reading a Complete slot to full completion frees it (Consumed
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
// An undersized (but non-zero) buffer on Read gives BufferTooSmall
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
// Running: discarding a slot the worker has already claimed drops the
// result and frees the slot only once the worker notices the cancel flag
// and returns.
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
// Complete: a Complete-but-unread slot discards the same way an
// already-consumed slot does -- Discard alone makes it reusable, no worker
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
// Pending: a fresh engine with zero idle workers starts a worker on
// Dispatch but that worker never actually runs until the test says so, so
// Discard must act on the slot while it is still Pending -- proven by the
// transport never being called at all.
// ---------------------------------------------------------------------------

TEST_F(AsyncEngineTest, DiscardOnPendingSlotBypassesTransportAndFreesSlotImmediately) {
  DeferredStartWorkerLifetime deferredLifetime;
  AsyncEngine freshEngine(deferredLifetime, DefaultIdSeedSource(), std::chrono::milliseconds(50));

  const std::string url = "http://example.invalid/pending-discard";
  std::uint32_t requestId = 0;
  ASSERT_EQ(freshEngine.Dispatch(client, MakeRequest(url), requestId), Status::Ok);

  std::int32_t pollState = -1;
  ASSERT_EQ(freshEngine.Poll(requestId, pollState), Status::Ok);
  EXPECT_EQ(pollState, 1);

  EXPECT_EQ(freshEngine.Discard(requestId), Status::Ok);
  EXPECT_EQ(transport.CallCount(), 0)
      << "Discard on a Pending slot must bypass the transport entirely";

  EXPECT_EQ(freshEngine.Poll(requestId, pollState), Status::UnknownRequestId);

  // The trap in DeferredStartWorkerLifetime's own comment: the deferred
  // thread must still be started and joined before the test ends.
  deferredLifetime.StartAllPending();
  deferredLifetime.JoinAll();
  EXPECT_EQ(transport.CallCount(), 0);
}

// ---------------------------------------------------------------------------
// DiscardAll also wakes a worker that has gone idle -- it rechecks
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
// Await unblocks with Ok once the request transitions to Complete.
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
// Await gives up and reports WaitTimeout once its bound elapses while the
// request is still Running.
// ---------------------------------------------------------------------------

TEST_F(AsyncEngineTest, AwaitReturnsWaitTimeoutAtDeadlineWhileStillRunning) {
  const std::string url = "http://example.invalid/case12-timeout";
  ExpectGateTracked(url);
  const std::uint32_t requestId = DispatchOrDie(url);
  ASSERT_TRUE(transport.WaitForGateEntered(url));

  EXPECT_EQ(engine.Await(requestId, 50), Status::WaitTimeout);
}

// ---------------------------------------------------------------------------
// A concurrent Discard ends a blocked Await well before its own deadline --
// discard's wake-up, not the timeout, is what ends the wait.
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
// Same as the single-discard case above, but the request is swept up by
// DiscardAll instead of a single Discard.
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
// Once a slot has been reclaimed by a different request, Await on the stale
// id must not be satisfied by the new occupant.
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
// waitTimeoutMs = 0 means "use the request's totalTimeoutMs as the bound",
// not "don't wait".
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

// ---------------------------------------------------------------------------
// An idle worker exits once its (test-shortened) idle timeout elapses, and
// the next Dispatch after that starts a brand new worker rather than
// reusing the exited one -- shown by StartThread/AcquireModuleReference
// call counts advancing across the exit.
// ---------------------------------------------------------------------------

TEST_F(AsyncEngineTest, IdleWorkerExitsAfterInjectedTimeoutAndNextDispatchRecreatesOne) {
  CountingWorkerLifetime countingLifetime;
  AsyncEngine shortIdleEngine(countingLifetime, DefaultIdSeedSource(), std::chrono::milliseconds(20));

  const std::string firstUrl = "http://example.invalid/lifecycle-first";
  std::uint32_t firstId = 0;
  ASSERT_EQ(shortIdleEngine.Dispatch(client, MakeRequest(firstUrl), firstId), Status::Ok);
  ASSERT_EQ(shortIdleEngine.Await(firstId, 5000), Status::Ok);
  EXPECT_EQ(shortIdleEngine.Discard(firstId), Status::Ok);
  EXPECT_EQ(countingLifetime.StartCount(), 1);
  EXPECT_EQ(countingLifetime.AcquireCount(), 1);

  // Blocks only until the now-idle worker actually exits past its
  // (test-shortened) idle timeout -- same bounded-wait technique as
  // DiscardAllWakesIdleWorkerToExitPromptly, not a guessed sleep.
  countingLifetime.JoinAll();
  EXPECT_EQ(countingLifetime.ExitCount(), 1);

  const std::string secondUrl = "http://example.invalid/lifecycle-second";
  std::uint32_t secondId = 0;
  ASSERT_EQ(shortIdleEngine.Dispatch(client, MakeRequest(secondUrl), secondId), Status::Ok);
  EXPECT_EQ(countingLifetime.StartCount(), 2)
      << "the exited worker must be recreated, not reused";
  EXPECT_EQ(countingLifetime.AcquireCount(), 2)
      << "a fresh module reference is acquired per worker, never carried over from the exited one";

  ASSERT_EQ(shortIdleEngine.Await(secondId, 5000), Status::Ok);
  EXPECT_EQ(shortIdleEngine.Discard(secondId), Status::Ok);
  countingLifetime.JoinAll();
}

// ---------------------------------------------------------------------------
// Dispatch starts a second worker as soon as the one existing worker is
// genuinely busy (Running), not only once idleWorkers_ has already dropped
// to zero through some other means -- a request must not wait behind a
// single worker that is still occupied with an earlier one while pool
// headroom remains.
// ---------------------------------------------------------------------------

TEST_F(AsyncEngineTest,
       DispatchStartsAdditionalWorkerWhenExistingWorkerIsBusyNotJustWhenNoneAreIdle) {
  CountingWorkerLifetime countingLifetime;
  AsyncEngine busyEngine(countingLifetime, DefaultIdSeedSource(), std::chrono::milliseconds(5000));

  // Await and the worker's own idle-wait entry serialize on the same mutex,
  // so by the time Await returns Ok here the one worker it woke for is
  // already sitting idle (idleWorkers_ incremented, blocked in its wait) --
  // no extra synchronization is needed to reach a known idle starting state.
  const std::string warmupUrl = "http://example.invalid/serialization-warmup";
  transport.SetResult(Status::Ok, HttpResponse{});
  std::uint32_t warmupId = 0;
  ASSERT_EQ(busyEngine.Dispatch(client, MakeRequest(warmupUrl), warmupId), Status::Ok);
  ASSERT_EQ(busyEngine.Await(warmupId, 5000), Status::Ok);
  ASSERT_EQ(busyEngine.Discard(warmupId), Status::Ok);
  ASSERT_EQ(countingLifetime.StartCount(), 1);

  const std::string urlA = "http://example.invalid/serialization-a";
  const std::string urlB = "http://example.invalid/serialization-b";
  ExpectGateTracked(urlA);
  ExpectGateTracked(urlB);

  std::uint32_t idA = 0;
  ASSERT_EQ(busyEngine.Dispatch(client, MakeRequest(urlA), idA), Status::Ok);
  ASSERT_TRUE(transport.WaitForGateEntered(urlA));
  EXPECT_EQ(countingLifetime.StartCount(), 1)
      << "the already-idle worker should pick up A directly, no new worker needed yet";

  std::uint32_t idB = 0;
  ASSERT_EQ(busyEngine.Dispatch(client, MakeRequest(urlB), idB), Status::Ok);
  ASSERT_TRUE(transport.WaitForGateEntered(urlB))
      << "B must reach Running via a second worker instead of waiting on A's busy one";
  EXPECT_EQ(countingLifetime.StartCount(), 2);

  HttpResponse responseA;
  responseA.statusCode = 201;
  responseA.body = "body-a";
  HttpResponse responseB;
  responseB.statusCode = 202;
  responseB.body = "body-b";

  // Released out of dispatch order, same as OutOfOrderCompletionsReachTheCorrectRequestId.
  transport.Release(urlB, Status::Ok, responseB);
  ASSERT_EQ(busyEngine.Await(idB, 5000), Status::Ok);
  transport.Release(urlA, Status::Ok, responseA);
  ASSERT_EQ(busyEngine.Await(idA, 5000), Status::Ok);

  char buffer[64];
  std::int32_t requestStatus = -1;
  std::int32_t httpStatusCode = -1;
  std::uint32_t responseBodyLength = 0;
  ASSERT_EQ(busyEngine.Read(idA, buffer, sizeof(buffer), requestStatus, httpStatusCode,
                             responseBodyLength),
            Status::Ok);
  EXPECT_EQ(httpStatusCode, 201);
  EXPECT_STREQ(buffer, "body-a");

  ASSERT_EQ(busyEngine.Read(idB, buffer, sizeof(buffer), requestStatus, httpStatusCode,
                             responseBodyLength),
            Status::Ok);
  EXPECT_EQ(httpStatusCode, 202);
  EXPECT_STREQ(buffer, "body-b");

  // DiscardAll wakes the now-idle workers immediately (see
  // DiscardAllWakesIdleWorkerToExitPromptly) so JoinAll below does not have
  // to wait out busyEngine's 5-second idle timeout.
  std::uint32_t stillRunning = 999;
  EXPECT_EQ(busyEngine.DiscardAll(stillRunning), Status::Ok);
  countingLifetime.JoinAll();
}

// ---------------------------------------------------------------------------
// A burst of dispatches against a cold engine starts exactly as many workers
// as there are concurrently-blocked requests, up to kAsyncRequestSlotCount,
// and every one of them reaches Running without needing to wait out any
// idle timeout -- none is left stranded behind a single worker no matter how
// dispatch calls and worker-thread scheduling happen to interleave.
// ---------------------------------------------------------------------------

TEST_F(AsyncEngineTest, BurstOfDispatchesStartsWorkersUpToCapAndAllReachRunningPromptly) {
  CountingWorkerLifetime countingLifetime;
  AsyncEngine burstEngine(countingLifetime, DefaultIdSeedSource(), std::chrono::milliseconds(5000));

  std::vector<std::string> urls;
  std::vector<std::uint32_t> ids;
  for (std::size_t i = 0; i < kAsyncRequestSlotCount; ++i) {
    urls.push_back("http://example.invalid/burst-" + std::to_string(i));
    ExpectGateTracked(urls.back());
  }

  for (const auto& url : urls) {
    std::uint32_t requestId = 0;
    ASSERT_EQ(burstEngine.Dispatch(client, MakeRequest(url), requestId), Status::Ok);
    ids.push_back(requestId);
  }

  for (const auto& url : urls) {
    EXPECT_TRUE(transport.WaitForGateEntered(url))
        << "request for '" << url << "' never reached Running";
  }

  // Every one of the kAsyncRequestSlotCount requests is blocked on its own
  // gate at this point, so none can have been serviced by a worker looping
  // back to pick up a second one -- exactly this many workers must exist,
  // which is also the cap itself (the slot table and the worker cap share
  // the same constant).
  EXPECT_EQ(countingLifetime.StartCount(), static_cast<int>(kAsyncRequestSlotCount));

  for (std::size_t i = 0; i < urls.size(); ++i) {
    transport.Release(urls[i], Status::Ok, HttpResponse{});
    ASSERT_EQ(burstEngine.Await(ids[i], 5000), Status::Ok);
  }

  std::uint32_t stillRunning = 999;
  EXPECT_EQ(burstEngine.DiscardAll(stillRunning), Status::Ok);
  countingLifetime.JoinAll();
}

// WorkerLoop decrements idleWorkers_ and commits to exiting under the same
// lock Dispatch's idle check uses, so Dispatch only ever sees a worker still
// idle and reusable, or already gone -- never one idle-counted mid-exit.

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
