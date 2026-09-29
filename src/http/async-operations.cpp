#include "http/async-operations.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <memory>
#include <utility>

#include "core/buffer-copy.h"

// Trap: the table mutex below is never held across a call to
// HttpClient::Perform -- only WorkerLoop's own unlock/relock window crosses
// it. Trap: nothing thread-related runs before the first Dispatch call, and
// ~AsyncEngine only releases memory -- it never waits, joins, or signals.
// Trap: a worker thread exits only through ExitCurrentThread, while still
// holding the module reference it acquired at creation -- it must never
// return out of its thread-start function normally. Trap: Poll, Await, Read
// and Discard never allocate or free; only Dispatch and DiscardAll touch
// the heap on the caller's thread.

namespace {

constexpr std::uint32_t kDefaultAwaitTimeoutMs = 30000;  // mirrors http-client.cpp's own default

// Any address inside this translation unit identifies the module to
// GetModuleHandleExW; a data symbol avoids the function-pointer-to-data-
// pointer cast warning a code address would need.
const int kAsyncOperationsAnchor = 0;

class Win32WorkerLifetime : public WorkerLifetime {
 public:
  void* AcquireModuleReference() override {
    HMODULE moduleHandle = nullptr;
    const BOOL ok = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                                        reinterpret_cast<LPCWSTR>(&kAsyncOperationsAnchor),
                                        &moduleHandle);
    return ok ? static_cast<void*>(moduleHandle) : nullptr;
  }

  void ReleaseModuleReference(void* moduleReference) override {
    if (moduleReference != nullptr) {
      FreeLibrary(static_cast<HMODULE>(moduleReference));
    }
  }

  bool StartThread(AsyncThreadEntry entry, void* param) override {
    HANDLE threadHandle = CreateThread(nullptr, 0, entry, param, 0, nullptr);
    if (threadHandle == nullptr) {
      return false;
    }
    CloseHandle(threadHandle);
    return true;
  }

  void ExitCurrentThread(void* moduleReference) override {
    FreeLibraryAndExitThread(static_cast<HMODULE>(moduleReference), 0);
  }
};

class QueryPerformanceCounterIdSeedSource : public IdSeedSource {
 public:
  std::uint32_t NextSeed() override {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    const auto bits = static_cast<std::uint64_t>(counter.QuadPart);
    return static_cast<std::uint32_t>(bits & 0xFFFFFFFFull);
  }
};

struct WorkerStartContext {
  AsyncEngine* engine;
  void* moduleReference;
};

}  // namespace

WorkerLifetime& DefaultWorkerLifetime() {
  static Win32WorkerLifetime instance;
  return instance;
}

IdSeedSource& DefaultIdSeedSource() {
  static QueryPerformanceCounterIdSeedSource instance;
  return instance;
}

AsyncEngine::AsyncEngine(WorkerLifetime& lifetime, IdSeedSource& idSeed,
                          std::chrono::milliseconds workerIdleTimeout)
    : lifetime_(lifetime), idSeed_(idSeed), workerIdleTimeout_(workerIdleTimeout) {}

AsyncEngine::~AsyncEngine() = default;

AsyncEngine::Slot* AsyncEngine::FindSlotForDispatchLocked() {
  for (Slot& slot : slots_) {
    if (slot.state == SlotState::Free || slot.state == SlotState::Consumed) {
      return &slot;
    }
  }
  return nullptr;
}

AsyncEngine::Slot* AsyncEngine::FindPendingSlotLocked() {
  for (Slot& slot : slots_) {
    if (slot.state == SlotState::Pending) {
      return &slot;
    }
  }
  return nullptr;
}

std::uint32_t AsyncEngine::CountPendingSlotsLocked() const {
  std::uint32_t count = 0;
  for (const Slot& slot : slots_) {
    if (slot.state == SlotState::Pending) {
      ++count;
    }
  }
  return count;
}

AsyncEngine::Slot* AsyncEngine::FindLiveSlotLocked(std::uint32_t requestId) {
  if (requestId == 0) {
    return nullptr;
  }
  for (Slot& slot : slots_) {
    if (slot.id == requestId &&
        (slot.state == SlotState::Pending || slot.state == SlotState::Running ||
         slot.state == SlotState::Complete)) {
      return &slot;
    }
  }
  return nullptr;
}

bool AsyncEngine::IsIdLiveLocked(std::uint32_t id) const {
  for (const Slot& slot : slots_) {
    if (slot.id == id && slot.state != SlotState::Free && slot.state != SlotState::Consumed) {
      return true;
    }
  }
  return false;
}

// The seed itself becomes the very first minted id; every id after that is
// nextId_ pre-incremented, skipping 0 and any id currently occupying a slot.
std::uint32_t AsyncEngine::MintIdLocked() {
  if (!idSeeded_) {
    nextId_ = idSeed_.NextSeed();
    idSeeded_ = true;
  }
  for (;;) {
    if (nextId_ == 0) {
      ++nextId_;
      continue;
    }
    if (!IsIdLiveLocked(nextId_)) {
      const std::uint32_t id = nextId_;
      ++nextId_;
      return id;
    }
    ++nextId_;
  }
}

void AsyncEngine::FreeSlotContentsLocked(Slot& slot) {
  slot.client.reset();
  slot.request = HttpRequest();
  slot.response = HttpResponse();
  slot.transportStatus = Status::Ok;
  slot.cancelFlag.store(false);
  slot.id = 0;
}

bool AsyncEngine::StartWorkerLocked() {
  void* const moduleReference = lifetime_.AcquireModuleReference();
  if (moduleReference == nullptr) {
    return false;
  }

  auto context = std::make_unique<WorkerStartContext>(WorkerStartContext{this, moduleReference});
  if (!lifetime_.StartThread(&AsyncEngine::WorkerThreadEntry, context.get())) {
    lifetime_.ReleaseModuleReference(moduleReference);
    return false;
  }
  context.release();  // ownership now belongs to the new thread
  ++liveWorkers_;
  return true;
}

unsigned long __stdcall AsyncEngine::WorkerThreadEntry(void* param) {
  std::unique_ptr<WorkerStartContext> context(static_cast<WorkerStartContext*>(param));
  AsyncEngine* const engine = context->engine;
  void* const moduleReference = context->moduleReference;
  context.reset();
  engine->WorkerLoop(moduleReference);
  return 0;  // reached only if ExitCurrentThread returns, i.e. under a test fake
}

void AsyncEngine::WorkerLoop(void* moduleReference) {
  std::unique_lock<std::mutex> lock(mutex_);
  for (;;) {
    Slot* pending = FindPendingSlotLocked();
    if (pending == nullptr) {
      ++idleWorkers_;
      const auto deadline = std::chrono::steady_clock::now() + workerIdleTimeout_;
      std::uint64_t observedGeneration = wakeGeneration_;
      for (;;) {
        const std::cv_status waitResult = workAvailable_.wait_until(lock, deadline);
        pending = FindPendingSlotLocked();
        if (pending != nullptr || waitResult == std::cv_status::timeout ||
            wakeGeneration_ != observedGeneration) {
          break;
        }
      }
      --idleWorkers_;
      if (pending == nullptr) {
        --liveWorkers_;
        lock.unlock();
        lifetime_.ExitCurrentThread(moduleReference);
        return;
      }
    }

    pending->state = SlotState::Running;
    lock.unlock();

    HttpResponse response;
    const Status result = pending->client->Perform(pending->request, response);

    lock.lock();
    if (pending->state == SlotState::Abandoned) {
      FreeSlotContentsLocked(*pending);
      pending->state = SlotState::Free;
    } else {
      pending->response = std::move(response);
      pending->transportStatus = result;
      pending->state = SlotState::Complete;
    }
    slotChanged_.notify_all();
  }
}

Status AsyncEngine::Dispatch(HttpClient& client, const HttpRequest& request,
                              std::uint32_t& requestId) {
  requestId = 0;

  std::unique_lock<std::mutex> lock(mutex_);
  Slot* const slot = FindSlotForDispatchLocked();
  if (slot == nullptr) {
    return Status::NoFreeRequestSlot;
  }

  FreeSlotContentsLocked(*slot);

  const std::uint32_t id = MintIdLocked();
  slot->id = id;
  slot->client.emplace(client);
  slot->request = request;
  slot->request.cancelFlag = &slot->cancelFlag;
  slot->state = SlotState::Pending;

  // idleWorkers_ only ever undercounts (a notified worker may not have
  // reacquired the lock yet), so comparing it against the pending count
  // taken fresh right here -- rather than a bare idleWorkers_ == 0 check --
  // closes a race where two back-to-back dispatches both see the same
  // stale idle count and both skip starting a worker, stranding one of
  // them behind a single busy worker despite pool headroom.
  if (CountPendingSlotsLocked() > idleWorkers_ && liveWorkers_ < kAsyncRequestSlotCount) {
    if (!StartWorkerLocked()) {
      FreeSlotContentsLocked(*slot);
      slot->state = SlotState::Free;
      return Status::AsyncStartFailed;
    }
  }

  ++wakeGeneration_;
  workAvailable_.notify_all();
  requestId = id;
  return Status::Ok;
}

Status AsyncEngine::Poll(std::uint32_t requestId, std::int32_t& state) {
  state = 0;
  std::lock_guard<std::mutex> lock(mutex_);
  Slot* const slot = FindLiveSlotLocked(requestId);
  if (slot == nullptr) {
    return Status::UnknownRequestId;
  }
  state = slot->state == SlotState::Complete ? 2 : 1;
  return Status::Ok;
}

Status AsyncEngine::Await(std::uint32_t requestId, std::uint32_t waitTimeoutMs) {
  std::unique_lock<std::mutex> lock(mutex_);
  Slot* slot = FindLiveSlotLocked(requestId);
  if (slot == nullptr) {
    return Status::UnknownRequestId;
  }
  if (slot->state == SlotState::Complete) {
    return Status::Ok;
  }

  const std::uint32_t resolvedTimeoutMs =
      waitTimeoutMs != 0 ? waitTimeoutMs
      : slot->request.options.totalTimeoutMs != 0 ? slot->request.options.totalTimeoutMs
                                                    : kDefaultAwaitTimeoutMs;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(resolvedTimeoutMs);

  for (;;) {
    const std::cv_status waitResult = slotChanged_.wait_until(lock, deadline);

    slot = FindLiveSlotLocked(requestId);
    if (slot == nullptr) {
      return Status::UnknownRequestId;
    }
    if (slot->state == SlotState::Complete) {
      return Status::Ok;
    }
    if (waitResult == std::cv_status::timeout) {
      return Status::WaitTimeout;
    }
  }
}

Status AsyncEngine::Read(std::uint32_t requestId, char* responseBody, std::uint32_t responseBodySize,
                          std::int32_t& requestStatus, std::int32_t& httpStatusCode,
                          std::uint32_t& responseBodyLength) {
  requestStatus = 0;
  httpStatusCode = 0;
  responseBodyLength = 0;

  std::lock_guard<std::mutex> lock(mutex_);
  Slot* const slot = FindLiveSlotLocked(requestId);
  if (slot == nullptr) {
    return Status::UnknownRequestId;
  }
  if (slot->state != SlotState::Complete) {
    return Status::RequestNotComplete;
  }

  requestStatus = static_cast<std::int32_t>(slot->transportStatus);
  httpStatusCode = slot->response.statusCode;
  responseBodyLength = static_cast<std::uint32_t>(slot->response.body.size());

  const Status copyStatus = CopyToBuffer(slot->response.body, responseBody, responseBodySize);
  if (copyStatus == Status::Ok) {
    slot->state = SlotState::Consumed;
  }
  return copyStatus;
}

Status AsyncEngine::Discard(std::uint32_t requestId) {
  std::lock_guard<std::mutex> lock(mutex_);
  Slot* const slot = FindLiveSlotLocked(requestId);
  if (slot == nullptr) {
    return Status::UnknownRequestId;
  }

  if (slot->state == SlotState::Running) {
    slot->state = SlotState::Abandoned;
    slot->cancelFlag.store(true);
  } else {
    slot->state = SlotState::Consumed;
  }
  slotChanged_.notify_all();
  return Status::Ok;
}

Status AsyncEngine::DiscardAll(std::uint32_t& stillRunning) {
  stillRunning = 0;
  std::lock_guard<std::mutex> lock(mutex_);
  for (Slot& slot : slots_) {
    switch (slot.state) {
      case SlotState::Pending:
      case SlotState::Complete:
      case SlotState::Consumed:
        FreeSlotContentsLocked(slot);
        slot.state = SlotState::Free;
        break;
      case SlotState::Running:
        slot.cancelFlag.store(true);
        slot.state = SlotState::Abandoned;
        ++stillRunning;
        break;
      default:
        break;
    }
  }
  slotChanged_.notify_all();
  ++wakeGeneration_;
  workAvailable_.notify_all();
  return Status::Ok;
}

AsyncEngine& DefaultAsyncEngine() {
  static AsyncEngine engine;
  return engine;
}
