// Coverage for src/mapping/json-document-store.*, called directly with JsonEntryType.

#include "mapping/json-document-store.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "core/status.h"
#include "mapping/json-flatten.h"

namespace {

constexpr std::uint32_t kIdSentinel = 0xDEADBEEFu;
constexpr std::uint32_t kCountSentinel = 0xDEADBEEFu;
constexpr JsonEntryType kTypeSentinel = JsonEntryType::Bool;
constexpr char kFill = 'Z';

FlattenResult Flat(const std::string& text) {
  FlattenResult result;
  EXPECT_EQ(FlattenJson(text, result), Status::Ok);
  return result;
}

std::uint32_t InsertOrDie(JsonDocumentStore& store, const std::string& text) {
  std::uint32_t id = kIdSentinel;
  EXPECT_EQ(store.Insert(Flat(text), id), Status::Ok);
  return id;
}

void FillStore(JsonDocumentStore& store) {
  for (std::size_t i = 0; i < kJsonDocumentSlotCount; ++i) {
    EXPECT_NE(InsertOrDie(store, "[1]"), kIdSentinel);
  }
}

}  // namespace

TEST(JsonDocumentStore, SlotCountIsEight) {
  EXPECT_EQ(kJsonDocumentSlotCount, 8u);
}

TEST(JsonDocumentStore, InsertStoresTheDocumentAndCountReportsItsEntries) {
  JsonDocumentStore store;
  std::uint32_t id = kIdSentinel;
  ASSERT_EQ(store.Insert(Flat(R"({"a":1,"b":[2,3]})"), id), Status::Ok);
  EXPECT_NE(id, 0u);
  EXPECT_NE(id, kIdSentinel);

  std::uint32_t count = kCountSentinel;
  EXPECT_EQ(store.Count(id, count), Status::Ok);
  EXPECT_EQ(count, 3u);
}

TEST(JsonDocumentStore, FirstIdOfAStoreBuiltWithAStartValueIsThatValue) {
  JsonDocumentStore store(100);
  EXPECT_EQ(InsertOrDie(store, "[1]"), 100u);
  EXPECT_EQ(InsertOrDie(store, "[1]"), 101u);
}

TEST(JsonDocumentStore, StartValueZeroNeverYieldsIdZero) {
  JsonDocumentStore store(0);
  EXPECT_NE(InsertOrDie(store, "[1]"), 0u);
}

TEST(JsonDocumentStore, InsertIntoAFullStoreKeepsTheResultAndTheIdUntouched) {
  JsonDocumentStore store;
  FillStore(store);

  FlattenResult extra = Flat(R"({"a":1,"b":2})");
  std::uint32_t id = kIdSentinel;
  EXPECT_EQ(store.Insert(std::move(extra), id), Status::NoFreeDocumentSlot);
  EXPECT_EQ(id, kIdSentinel);
  EXPECT_EQ(extra.entries.size(), 2u);
  EXPECT_EQ(extra.document.dump(), R"({"a":1,"b":2})");
}

TEST(JsonDocumentStore, InsertIntoAFreedSlotSucceedsAfterTheStoreWasFull) {
  JsonDocumentStore store;
  std::uint32_t first = InsertOrDie(store, "[1]");
  for (std::size_t i = 1; i < kJsonDocumentSlotCount; ++i) {
    InsertOrDie(store, "[1]");
  }
  std::uint32_t rejected = kIdSentinel;
  ASSERT_EQ(store.Insert(Flat("[2]"), rejected), Status::NoFreeDocumentSlot);

  ASSERT_EQ(store.Discard(first), Status::Ok);
  std::uint32_t reused = kIdSentinel;
  EXPECT_EQ(store.Insert(Flat("[2]"), reused), Status::Ok);
  EXPECT_NE(reused, first);
  EXPECT_NE(reused, 0u);
}

TEST(JsonDocumentStore, CountOfAnUnknownIdLeavesTheOutParameterUntouched) {
  JsonDocumentStore store;
  InsertOrDie(store, "[1]");
  std::uint32_t count = kCountSentinel;
  EXPECT_EQ(store.Count(0, count), Status::UnknownDocumentId);
  EXPECT_EQ(count, kCountSentinel);
}

TEST(JsonDocumentStore, ReadEntryFailuresLeaveValueTypeUntouched) {
  JsonDocumentStore store;
  const std::uint32_t id = InsertOrDie(store, R"({"key":"value"})");
  std::array<char, 16> key;
  std::array<char, 16> value;
  key.fill(kFill);
  value.fill(kFill);
  const auto keySize = static_cast<std::uint32_t>(key.size());
  const auto valueSize = static_cast<std::uint32_t>(value.size());

  JsonEntryType type = kTypeSentinel;
  EXPECT_EQ(store.ReadEntry(0, 0, key.data(), keySize, value.data(), valueSize, type),
            Status::UnknownDocumentId);
  EXPECT_EQ(type, kTypeSentinel);

  EXPECT_EQ(store.ReadEntry(id, 1, key.data(), keySize, value.data(), valueSize, type),
            Status::IndexOutOfRange);
  EXPECT_EQ(type, kTypeSentinel);

  EXPECT_EQ(store.ReadEntry(id, 0, nullptr, keySize, value.data(), valueSize, type),
            Status::InvalidArgument);
  EXPECT_EQ(type, kTypeSentinel);

  EXPECT_EQ(store.ReadEntry(id, 0, key.data(), 2, value.data(), valueSize, type),
            Status::BufferTooSmall);
  EXPECT_EQ(type, kTypeSentinel);
}

TEST(JsonDocumentStore, ReadEntrySuccessWritesTextAndType) {
  JsonDocumentStore store;
  const std::uint32_t id = InsertOrDie(store, R"({"key":"value"})");
  std::array<char, 16> key;
  std::array<char, 16> value;
  JsonEntryType type = kTypeSentinel;
  ASSERT_EQ(store.ReadEntry(id, 0, key.data(), static_cast<std::uint32_t>(key.size()),
                            value.data(), static_cast<std::uint32_t>(value.size()), type),
            Status::Ok);
  EXPECT_STREQ(key.data(), "/key");
  EXPECT_STREQ(value.data(), "value");
  EXPECT_EQ(type, JsonEntryType::String);
}

TEST(JsonDocumentStore, ReadValueFailuresLeaveValueTypeAndBufferUntouched) {
  JsonDocumentStore store;
  const std::uint32_t id = InsertOrDie(store, R"({"o":{"k":1},"a":[1]})");
  std::array<char, 16> value;
  value.fill(kFill);
  const auto valueSize = static_cast<std::uint32_t>(value.size());

  JsonEntryType type = kTypeSentinel;
  EXPECT_EQ(store.ReadValue(0, "", value.data(), valueSize, type), Status::UnknownDocumentId);
  EXPECT_EQ(store.ReadValue(id, "bad", value.data(), valueSize, type), Status::PathSyntaxError);
  EXPECT_EQ(store.ReadValue(id, "/x", value.data(), valueSize, type), Status::PathNotFound);
  EXPECT_EQ(store.ReadValue(id, "/a/3", value.data(), valueSize, type), Status::IndexOutOfRange);
  EXPECT_EQ(store.ReadValue(id, "/o", value.data(), valueSize, type), Status::TypeMismatch);
  EXPECT_EQ(type, kTypeSentinel);
  for (const char c : value) {
    EXPECT_EQ(c, kFill);
  }
}

TEST(JsonDocumentStore, ReadValueOnANonEmptyContainerIsTypeMismatchButAnEmptyOneIsALeaf) {
  JsonDocumentStore store;
  const std::uint32_t id = InsertOrDie(store, R"({"full":{"k":1},"none":{},"list":[]})");
  std::array<char, 16> value;
  JsonEntryType type = kTypeSentinel;
  const auto valueSize = static_cast<std::uint32_t>(value.size());

  EXPECT_EQ(store.ReadValue(id, "/full", value.data(), valueSize, type), Status::TypeMismatch);

  ASSERT_EQ(store.ReadValue(id, "/none", value.data(), valueSize, type), Status::Ok);
  EXPECT_STREQ(value.data(), "{}");
  EXPECT_EQ(type, JsonEntryType::EmptyObject);

  ASSERT_EQ(store.ReadValue(id, "/list", value.data(), valueSize, type), Status::Ok);
  EXPECT_STREQ(value.data(), "[]");
  EXPECT_EQ(type, JsonEntryType::EmptyArray);
}

TEST(JsonDocumentStore, ReadValueWithTooSmallABufferEmptiesItAndLeavesTheTypeUntouched) {
  JsonDocumentStore store;
  const std::uint32_t id = InsertOrDie(store, R"({"k":"hello"})");
  std::array<char, 5> value;
  value.fill(kFill);
  JsonEntryType type = kTypeSentinel;
  EXPECT_EQ(store.ReadValue(id, "/k", value.data(), static_cast<std::uint32_t>(value.size()), type),
            Status::BufferTooSmall);
  EXPECT_EQ(value[0], '\0');
  EXPECT_EQ(type, kTypeSentinel);
}

TEST(JsonDocumentStore, DiscardFreesTheDocumentAndRejectsAnyFurtherUse) {
  JsonDocumentStore store;
  const std::uint32_t id = InsertOrDie(store, "[1]");
  EXPECT_EQ(store.Discard(id), Status::Ok);
  EXPECT_EQ(store.Discard(id), Status::UnknownDocumentId);

  std::uint32_t count = kCountSentinel;
  EXPECT_EQ(store.Count(id, count), Status::UnknownDocumentId);
}

TEST(JsonDocumentStore, DiscardAllReportsTheNumberFreedAndEmptiesTheStore) {
  JsonDocumentStore store;
  const std::uint32_t a = InsertOrDie(store, "[1]");
  const std::uint32_t b = InsertOrDie(store, "[2]");

  std::uint32_t discarded = kCountSentinel;
  EXPECT_EQ(store.DiscardAll(discarded), Status::Ok);
  EXPECT_EQ(discarded, 2u);

  std::uint32_t count = 0;
  EXPECT_EQ(store.Count(a, count), Status::UnknownDocumentId);
  EXPECT_EQ(store.Count(b, count), Status::UnknownDocumentId);

  EXPECT_EQ(store.DiscardAll(discarded), Status::Ok);
  EXPECT_EQ(discarded, 0u);
}

TEST(JsonDocumentStore, DefaultStoreIsOneProcessWideInstance) {
  EXPECT_EQ(&DefaultJsonDocumentStore(), &DefaultJsonDocumentStore());
}
