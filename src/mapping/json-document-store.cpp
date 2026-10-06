#include "mapping/json-document-store.h"

#include <chrono>
#include <limits>
#include <string>
#include <utility>

#include "core/buffer-copy.h"
#include "core/json-path.h"

namespace {

static_assert(kMaxFlatEntries <= std::numeric_limits<std::uint32_t>::max(),
              "entry counts must fit a CAPL dword");

Status CopyEntryTexts(const FlatEntry& entry, char* key, std::uint32_t keySize, char* value,
                      std::uint32_t valueSize) {
  if (key == nullptr || keySize == 0 || value == nullptr || valueSize == 0) {
    return Status::InvalidArgument;
  }

  const Status keyStatus = CopyToBuffer(entry.key, key, keySize);
  const Status valueStatus = CopyToBuffer(entry.value, value, valueSize);
  if (keyStatus == Status::Ok && valueStatus == Status::Ok) {
    return Status::Ok;
  }

  key[0] = '\0';
  value[0] = '\0';
  return Status::BufferTooSmall;
}

}  // namespace

JsonDocumentStore::JsonDocumentStore(std::uint32_t firstId) : idSeeded_(true), nextId_(firstId) {}

JsonDocumentStore::Slot* JsonDocumentStore::FindFreeSlotLocked() {
  for (Slot& slot : slots_) {
    if (!slot.occupied) {
      return &slot;
    }
  }
  return nullptr;
}

JsonDocumentStore::Slot* JsonDocumentStore::FindLiveSlotLocked(std::uint32_t documentId) {
  if (documentId == 0) {
    return nullptr;
  }
  for (Slot& slot : slots_) {
    if (slot.occupied && slot.id == documentId) {
      return &slot;
    }
  }
  return nullptr;
}

bool JsonDocumentStore::IsIdLiveLocked(std::uint32_t id) const {
  for (const Slot& slot : slots_) {
    if (slot.occupied && slot.id == id) {
      return true;
    }
  }
  return false;
}

// Ids count up from a clock-derived start, skipping 0 and live ids; a discarded id recurs only on 32-bit wrap.
std::uint32_t JsonDocumentStore::MintIdLocked() {
  if (!idSeeded_) {
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    nextId_ = static_cast<std::uint32_t>(static_cast<std::uint64_t>(ticks) & 0xFFFFFFFFull);
    idSeeded_ = true;
  }
  for (;;) {
    const std::uint32_t candidate = nextId_++;
    if (candidate != 0 && !IsIdLiveLocked(candidate)) {
      return candidate;
    }
  }
}

Status JsonDocumentStore::Insert(FlattenResult&& result, std::uint32_t& documentId) {
  std::lock_guard<std::mutex> lock(mutex_);
  Slot* const slot = FindFreeSlotLocked();
  if (slot == nullptr) {
    return Status::NoFreeDocumentSlot;
  }

  const std::uint32_t id = MintIdLocked();
  slot->result = std::move(result);
  slot->id = id;
  slot->occupied = true;
  documentId = id;
  return Status::Ok;
}

Status JsonDocumentStore::Count(std::uint32_t documentId, std::uint32_t& entryCount) {
  std::lock_guard<std::mutex> lock(mutex_);
  Slot* const slot = FindLiveSlotLocked(documentId);
  if (slot == nullptr) {
    return Status::UnknownDocumentId;
  }
  entryCount = static_cast<std::uint32_t>(slot->result.entries.size());
  return Status::Ok;
}

Status JsonDocumentStore::ReadEntry(std::uint32_t documentId, std::uint32_t entryIndex, char* key,
                                    std::uint32_t keySize, char* value, std::uint32_t valueSize,
                                    JsonEntryType& valueType) {
  std::lock_guard<std::mutex> lock(mutex_);
  Slot* const slot = FindLiveSlotLocked(documentId);
  if (slot == nullptr) {
    return Status::UnknownDocumentId;
  }

  const std::vector<FlatEntry>& entries = slot->result.entries;
  if (static_cast<std::size_t>(entryIndex) >= entries.size()) {
    return Status::IndexOutOfRange;
  }

  const FlatEntry& entry = entries[entryIndex];
  const Status copyStatus = CopyEntryTexts(entry, key, keySize, value, valueSize);
  if (copyStatus == Status::Ok) {
    valueType = entry.type;
  }
  return copyStatus;
}

Status JsonDocumentStore::ReadValue(std::uint32_t documentId, std::string_view path, char* value,
                                    std::uint32_t valueSize, JsonEntryType& valueType) {
  std::lock_guard<std::mutex> lock(mutex_);
  Slot* const slot = FindLiveSlotLocked(documentId);
  if (slot == nullptr) {
    return Status::UnknownDocumentId;
  }

  const JsonValue* node = nullptr;
  Status status = ResolvePath(slot->result.document, path, node);
  if (status != Status::Ok) {
    return status;
  }

  std::string text;
  JsonEntryType type = JsonEntryType::None;
  status = DescribeLeaf(*node, text, type);
  if (status != Status::Ok) {
    return status;
  }

  status = CopyToBuffer(text, value, valueSize);
  if (status == Status::Ok) {
    valueType = type;
  }
  return status;
}

Status JsonDocumentStore::ReadWith(std::uint32_t documentId, std::string_view path,
                                   const std::function<Status(const JsonValue&)>& read) {
  return ReadWithTokens(documentId, path,
                        [&read](const JsonValue& document, const PathTokens&) { return read(document); });
}

Status JsonDocumentStore::ReadWithTokens(
    std::uint32_t documentId, std::string_view path,
    const std::function<Status(const JsonValue&, const PathTokens&)>& read) {
  PathTokens tokens;
  const Status pathStatus = ParsePath(path, tokens);
  if (pathStatus != Status::Ok) {
    return pathStatus;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  Slot* const slot = FindLiveSlotLocked(documentId);
  if (slot == nullptr) {
    return Status::UnknownDocumentId;
  }
  return read(slot->result.document, tokens);
}

Status JsonDocumentStore::Discard(std::uint32_t documentId) {
  // The document is destroyed after the lock is released because freeing a large one under the mutex would stall concurrent readers.
  FlattenResult doomed;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    Slot* const slot = FindLiveSlotLocked(documentId);
    if (slot == nullptr) {
      return Status::UnknownDocumentId;
    }
    doomed = std::move(slot->result);
    slot->occupied = false;
    slot->id = 0;
  }
  return Status::Ok;
}

Status JsonDocumentStore::DiscardAll(std::uint32_t& discardedCount) {
  std::array<FlattenResult, kJsonDocumentSlotCount> doomed;
  std::uint32_t count = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (std::size_t i = 0; i < slots_.size(); ++i) {
      Slot& slot = slots_[i];
      if (!slot.occupied) {
        continue;
      }
      doomed[i] = std::move(slot.result);
      slot.occupied = false;
      slot.id = 0;
      ++count;
    }
  }
  discardedCount = count;
  return Status::Ok;
}

JsonDocumentStore& DefaultJsonDocumentStore() {
  static JsonDocumentStore store;
  return store;
}
