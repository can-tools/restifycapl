// json-document-store.h -- fixed-slot store of flattened JSON documents; see docs/capl-json-surface.md.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string_view>

#include "core/json-path.h"
#include "core/status.h"
#include "mapping/json-flatten.h"

inline constexpr std::size_t kJsonDocumentSlotCount = 8;

// Every method writes its out-parameters only when it returns Ok.
class JsonDocumentStore {
 public:
  JsonDocumentStore() = default;
  explicit JsonDocumentStore(std::uint32_t firstId);  // fixes where id minting starts, for tests

  JsonDocumentStore(const JsonDocumentStore&) = delete;
  JsonDocumentStore& operator=(const JsonDocumentStore&) = delete;

  // `result` is moved from only on Ok; no free slot -> NoFreeDocumentSlot.
  Status Insert(FlattenResult&& result, std::uint32_t& documentId);

  // Zero, unknown or discarded id -> UnknownDocumentId, in every method below.
  Status Count(std::uint32_t documentId, std::uint32_t& entryCount);

  // Index out of range -> IndexOutOfRange; bad buffer -> InvalidArgument; too small -> BufferTooSmall, both buffers emptied.
  Status ReadEntry(std::uint32_t documentId, std::uint32_t entryIndex, char* key,
                   std::uint32_t keySize, char* value, std::uint32_t valueSize,
                   JsonEntryType& valueType);

  // Path errors pass through from ResolvePath; a non-empty container -> TypeMismatch.
  Status ReadValue(std::uint32_t documentId, std::string_view path, char* value,
                   std::uint32_t valueSize, JsonEntryType& valueType);

  // Runs `read` on the stored document under the lock, without a copy. The path is syntax-checked first (PathSyntaxError beats UnknownDocumentId).
  Status ReadWith(std::uint32_t documentId, std::string_view path,
                  const std::function<Status(const JsonValue&)>& read);

  // As ReadWith, but the path is parsed once, outside the lock, and `read` also receives the tokens.
  Status ReadWithTokens(std::uint32_t documentId, std::string_view path,
                        const std::function<Status(const JsonValue&, const PathTokens&)>& read);

  Status Discard(std::uint32_t documentId);

  // Always Ok; `discardedCount` is the number of documents freed.
  Status DiscardAll(std::uint32_t& discardedCount);

 private:
  struct Slot {
    bool occupied = false;
    std::uint32_t id = 0;
    FlattenResult result;
  };

  Slot* FindFreeSlotLocked();
  Slot* FindLiveSlotLocked(std::uint32_t documentId);
  bool IsIdLiveLocked(std::uint32_t id) const;
  std::uint32_t MintIdLocked();

  std::mutex mutex_;
  std::array<Slot, kJsonDocumentSlotCount> slots_;
  bool idSeeded_ = false;
  std::uint32_t nextId_ = 0;
};

JsonDocumentStore& DefaultJsonDocumentStore();
