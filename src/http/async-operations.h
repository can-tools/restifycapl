// async-operations.h -- the async request engine (N-slot table, worker
// pool) built on HttpClient, same level as sync-operations. Normative
// per-operation behavior lives in docs/capl-async-surface.md.
#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>

#include "core/status.h"
#include "http/http-client.h"

inline constexpr std::size_t kAsyncRequestSlotCount = 8;
inline constexpr std::chrono::milliseconds kAsyncDefaultWorkerIdleTimeout{30000};

using AsyncThreadEntry = unsigned long(__stdcall*)(void*);

class WorkerLifetime {
 public:
  virtual ~WorkerLifetime() = default;
  virtual void* AcquireModuleReference() = 0;
  virtual void ReleaseModuleReference(void* moduleReference) = 0;
  virtual bool StartThread(AsyncThreadEntry entry, void* param) = 0;
  virtual void ExitCurrentThread(void* moduleReference) = 0;
};

WorkerLifetime& DefaultWorkerLifetime();

class IdSeedSource {
 public:
  virtual ~IdSeedSource() = default;
  virtual std::uint32_t NextSeed() = 0;
};

IdSeedSource& DefaultIdSeedSource();

class AsyncEngine {
 public:
  explicit AsyncEngine(WorkerLifetime& lifetime = DefaultWorkerLifetime(),
                        IdSeedSource& idSeed = DefaultIdSeedSource(),
                        std::chrono::milliseconds workerIdleTimeout = kAsyncDefaultWorkerIdleTimeout);
  ~AsyncEngine();

  AsyncEngine(const AsyncEngine&) = delete;
  AsyncEngine& operator=(const AsyncEngine&) = delete;

  Status Dispatch(HttpClient& client, const HttpRequest& request, std::uint32_t& requestId);
  Status Poll(std::uint32_t requestId, std::int32_t& state);
  Status Await(std::uint32_t requestId, std::uint32_t waitTimeoutMs);
  Status Read(std::uint32_t requestId, char* responseBody, std::uint32_t responseBodySize,
              std::int32_t& requestStatus, std::int32_t& httpStatusCode,
              std::uint32_t& responseBodyLength);
  Status Discard(std::uint32_t requestId);
  Status DiscardAll(std::uint32_t& stillRunning);

 private:
  enum class SlotState { Free, Pending, Running, Complete, Consumed, Abandoned };

  struct Slot {
    SlotState state = SlotState::Free;
    std::uint32_t id = 0;
    std::optional<HttpClient> client;
    HttpRequest request;
    std::atomic<bool> cancelFlag{false};
    HttpResponse response;
    Status transportStatus = Status::Ok;
  };

  static unsigned long __stdcall WorkerThreadEntry(void* param);
  void WorkerLoop(void* moduleReference);

  Slot* FindSlotForDispatchLocked();
  Slot* FindPendingSlotLocked();
  std::uint32_t CountPendingSlotsLocked() const;
  Slot* FindLiveSlotLocked(std::uint32_t requestId);
  bool IsIdLiveLocked(std::uint32_t id) const;
  std::uint32_t MintIdLocked();
  static void FreeSlotContentsLocked(Slot& slot);
  bool StartWorkerLocked();

  WorkerLifetime& lifetime_;
  IdSeedSource& idSeed_;
  std::chrono::milliseconds workerIdleTimeout_;

  std::mutex mutex_;
  std::condition_variable workAvailable_;
  std::condition_variable slotChanged_;
  std::array<Slot, kAsyncRequestSlotCount> slots_;
  std::uint32_t liveWorkers_ = 0;
  std::uint32_t idleWorkers_ = 0;
  // Bumped under mutex_ at every workAvailable_.notify_all() call site (Dispatch,
  // DiscardAll); lets WorkerLoop's idle wait tell a deliberate wake-with-nothing-
  // pending (exit now, e.g. DiscardAll shrinking the pool) apart from a true
  // OS-level spurious wakeup (keep waiting out the same deadline).
  std::uint64_t wakeGeneration_ = 0;
  bool idSeeded_ = false;
  std::uint32_t nextId_ = 0;
};

AsyncEngine& DefaultAsyncEngine();
