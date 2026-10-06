// Coverage for src/mapping/json-document-store.*, called directly with JsonEntryType.

#include "mapping/json-document-store.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "core/status.h"
#include "../test-support/status-print.h"
#include "mapping/json-flatten.h"

namespace {

// Stores built from kFirstId mint ids kFirstId..kFirstId+7 in these tests, so kIdSentinel cannot be a real id.
constexpr std::uint32_t kFirstId = 1000;
constexpr std::uint32_t kIdSentinel = 0xDEADBEEFu;
constexpr std::uint32_t kCountSentinel = 0xC0FFEE11u;
// Not an enumerator, so a real entry type can never equal it.
constexpr JsonEntryType kTypeSentinel = static_cast<JsonEntryType>(-1);
constexpr char kFill = 'Z';

using TextBuffer = std::array<char, 16>;

TextBuffer FilledBuffer() {
  TextBuffer buffer;
  buffer.fill(kFill);
  return buffer;
}

bool IsUntouched(const TextBuffer& buffer) {
  for (const char c : buffer) {
    if (c != kFill) {
      return false;
    }
  }
  return true;
}

std::uint32_t SizeOf(const TextBuffer& buffer) { return static_cast<std::uint32_t>(buffer.size()); }

FlattenResult Flat(const std::string& text) {
  FlattenResult result;
  EXPECT_EQ(FlattenJson(text, result), Status::Ok);
  return result;
}

std::uint32_t InsertOrDie(JsonDocumentStore& store, const std::string& text) {
  std::uint32_t id = 0;
  EXPECT_EQ(store.Insert(Flat(text), id), Status::Ok);
  EXPECT_NE(id, 0u);
  return id;
}

std::vector<std::uint32_t> FillStore(JsonDocumentStore& store) {
  std::vector<std::uint32_t> ids;
  for (std::size_t i = 0; i < kJsonDocumentSlotCount; ++i) {
    ids.push_back(InsertOrDie(store, "{\"n\":" + std::to_string(i) + "}"));
  }
  return ids;
}

struct ValueResult {
  Status status;
  std::string text;
  JsonEntryType type;
};

ValueResult ReadValueAt(JsonDocumentStore& store, std::uint32_t id, std::string_view path) {
  TextBuffer value = FilledBuffer();
  JsonEntryType type = kTypeSentinel;
  const Status status = store.ReadValue(id, path, value.data(), SizeOf(value), type);
  return ValueResult{status, status == Status::Ok ? std::string(value.data()) : std::string(),
                     type};
}

// For every status except BufferTooSmall the caller's buffer must stay as it was.
void ExpectReadValueFails(JsonDocumentStore& store, std::uint32_t id, std::string_view path,
                          Status expected) {
  SCOPED_TRACE("path '" + std::string(path) + "'");
  TextBuffer value = FilledBuffer();
  JsonEntryType type = kTypeSentinel;
  EXPECT_EQ(store.ReadValue(id, path, value.data(), SizeOf(value), type), expected);
  EXPECT_EQ(type, kTypeSentinel);
  EXPECT_TRUE(IsUntouched(value));
}

void ExpectUnknownEverywhere(JsonDocumentStore& store, std::uint32_t id) {
  SCOPED_TRACE("id " + std::to_string(id));

  std::uint32_t count = kCountSentinel;
  EXPECT_EQ(store.Count(id, count), Status::UnknownDocumentId);
  EXPECT_EQ(count, kCountSentinel);

  TextBuffer key = FilledBuffer();
  TextBuffer value = FilledBuffer();
  JsonEntryType type = kTypeSentinel;
  EXPECT_EQ(store.ReadEntry(id, 0, key.data(), SizeOf(key), value.data(), SizeOf(value), type),
            Status::UnknownDocumentId);
  EXPECT_EQ(type, kTypeSentinel);
  EXPECT_TRUE(IsUntouched(key));
  EXPECT_TRUE(IsUntouched(value));

  ExpectReadValueFails(store, id, "", Status::UnknownDocumentId);
  EXPECT_EQ(store.Discard(id), Status::UnknownDocumentId);
}

}  // namespace

TEST(JsonDocumentStore, SlotCountIsEight) {
  EXPECT_EQ(kJsonDocumentSlotCount, 8u);
}

// ---------------------------------------------------------------------------
// Insert and Count.
// ---------------------------------------------------------------------------

TEST(JsonDocumentStore, InsertStoresTheDocumentAndCountReportsItsEntries) {
  JsonDocumentStore store(kFirstId);
  std::uint32_t id = kIdSentinel;
  ASSERT_EQ(store.Insert(Flat(R"({"a":1,"b":[2,3]})"), id), Status::Ok);
  EXPECT_EQ(id, kFirstId);

  std::uint32_t count = kCountSentinel;
  EXPECT_EQ(store.Count(id, count), Status::Ok);
  EXPECT_EQ(count, 3u);
}

TEST(JsonDocumentStore, IdsOfAStoreBuiltWithAStartValueCountUpFromIt) {
  JsonDocumentStore store(100);
  EXPECT_EQ(InsertOrDie(store, "[1]"), 100u);
  EXPECT_EQ(InsertOrDie(store, "[1]"), 101u);
  EXPECT_EQ(InsertOrDie(store, "[1]"), 102u);
}

TEST(JsonDocumentStore, StartValueZeroSkipsZeroAndYieldsOne) {
  JsonDocumentStore store(0);
  EXPECT_EQ(InsertOrDie(store, "[1]"), 1u);
  EXPECT_EQ(InsertOrDie(store, "[1]"), 2u);
}

TEST(JsonDocumentStore, InsertIntoAFullStoreKeepsTheResultAndTheIdUntouched) {
  JsonDocumentStore store(kFirstId);
  const std::vector<std::uint32_t> ids = FillStore(store);

  FlattenResult extra = Flat(R"({"a":1,"b":2})");
  std::uint32_t id = kIdSentinel;
  EXPECT_EQ(store.Insert(std::move(extra), id), Status::NoFreeDocumentSlot);
  EXPECT_EQ(id, kIdSentinel);
  EXPECT_EQ(extra.entries.size(), 2u);
  EXPECT_EQ(extra.document.dump(), R"({"a":1,"b":2})");

  for (const std::uint32_t held : ids) {
    std::uint32_t count = kCountSentinel;
    EXPECT_EQ(store.Count(held, count), Status::Ok);
    EXPECT_EQ(count, 1u);
  }
}

TEST(JsonDocumentStore, InsertIntoAFreedSlotSucceedsAndTheRejectedInsertConsumedNoId) {
  JsonDocumentStore store(kFirstId);
  const std::vector<std::uint32_t> ids = FillStore(store);
  ASSERT_EQ(ids.front(), kFirstId);
  ASSERT_EQ(ids.back(), kFirstId + 7);

  std::uint32_t rejected = kIdSentinel;
  ASSERT_EQ(store.Insert(Flat("[2]"), rejected), Status::NoFreeDocumentSlot);

  ASSERT_EQ(store.Discard(ids.front()), Status::Ok);
  std::uint32_t reused = kIdSentinel;
  ASSERT_EQ(store.Insert(Flat("[2,3]"), reused), Status::Ok);
  EXPECT_EQ(reused, kFirstId + 8);

  std::uint32_t count = kCountSentinel;
  EXPECT_EQ(store.Count(reused, count), Status::Ok);
  EXPECT_EQ(count, 2u);
  ExpectUnknownEverywhere(store, ids.front());
}

TEST(JsonDocumentStore, CountOfAnUnknownIdLeavesTheOutParameterUntouched) {
  JsonDocumentStore store(kFirstId);
  InsertOrDie(store, "[1]");
  const std::uint32_t unknownIds[] = {0u, kFirstId - 1, kFirstId + 1, 0xFFFFFFFFu};
  for (const std::uint32_t unknown : unknownIds) {
    SCOPED_TRACE("id " + std::to_string(unknown));
    std::uint32_t count = kCountSentinel;
    EXPECT_EQ(store.Count(unknown, count), Status::UnknownDocumentId);
    EXPECT_EQ(count, kCountSentinel);
  }
}

// ---------------------------------------------------------------------------
// ReadEntry.
// ---------------------------------------------------------------------------

TEST(JsonDocumentStore, ReadEntryFailuresLeaveBuffersAndValueTypeAsTheyWere) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t id = InsertOrDie(store, R"({"key":"value"})");

  TextBuffer key = FilledBuffer();
  TextBuffer value = FilledBuffer();
  JsonEntryType type = kTypeSentinel;

  EXPECT_EQ(store.ReadEntry(0, 0, key.data(), SizeOf(key), value.data(), SizeOf(value), type),
            Status::UnknownDocumentId);
  EXPECT_EQ(type, kTypeSentinel);
  EXPECT_TRUE(IsUntouched(key));
  EXPECT_TRUE(IsUntouched(value));

  EXPECT_EQ(store.ReadEntry(id, 1, key.data(), SizeOf(key), value.data(), SizeOf(value), type),
            Status::IndexOutOfRange);
  EXPECT_EQ(type, kTypeSentinel);
  EXPECT_TRUE(IsUntouched(key));
  EXPECT_TRUE(IsUntouched(value));

  EXPECT_EQ(store.ReadEntry(id, 0, nullptr, SizeOf(key), value.data(), SizeOf(value), type),
            Status::InvalidArgument);
  EXPECT_EQ(type, kTypeSentinel);
  EXPECT_TRUE(IsUntouched(value));

  EXPECT_EQ(store.ReadEntry(id, 0, key.data(), SizeOf(key), value.data(), 0, type),
            Status::InvalidArgument);
  EXPECT_EQ(type, kTypeSentinel);
  EXPECT_TRUE(IsUntouched(key));
  EXPECT_TRUE(IsUntouched(value));
}

TEST(JsonDocumentStore, ReadEntryTooSmallABufferEmptiesBothBuffersAndLeavesTheType) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t id = InsertOrDie(store, R"({"key":"value"})");

  TextBuffer key = FilledBuffer();
  TextBuffer value = FilledBuffer();
  JsonEntryType type = kTypeSentinel;

  EXPECT_EQ(store.ReadEntry(id, 0, key.data(), 2, value.data(), SizeOf(value), type),
            Status::BufferTooSmall);
  EXPECT_EQ(type, kTypeSentinel);
  EXPECT_EQ(key[0], '\0');
  EXPECT_EQ(value[0], '\0');

  key = FilledBuffer();
  value = FilledBuffer();
  EXPECT_EQ(store.ReadEntry(id, 0, key.data(), SizeOf(key), value.data(), 2, type),
            Status::BufferTooSmall);
  EXPECT_EQ(type, kTypeSentinel);
  EXPECT_EQ(key[0], '\0');
  EXPECT_EQ(value[0], '\0');

  key = FilledBuffer();
  value = FilledBuffer();
  EXPECT_EQ(store.ReadEntry(id, 0, key.data(), 2, value.data(), 2, type), Status::BufferTooSmall);
  EXPECT_EQ(type, kTypeSentinel);
  EXPECT_EQ(key[0], '\0');
  EXPECT_EQ(value[0], '\0');
}

TEST(JsonDocumentStore, ReadEntryBufferArgumentErrorOutranksATooSmallOtherBuffer) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t id = InsertOrDie(store, R"({"key":"value"})");

  TextBuffer key = FilledBuffer();
  TextBuffer value = FilledBuffer();
  JsonEntryType type = kTypeSentinel;

  EXPECT_EQ(store.ReadEntry(id, 0, nullptr, 0, value.data(), 2, type), Status::InvalidArgument);
  EXPECT_EQ(type, kTypeSentinel);
  EXPECT_TRUE(IsUntouched(value));

  EXPECT_EQ(store.ReadEntry(id, 0, key.data(), 2, nullptr, 0, type), Status::InvalidArgument);
  EXPECT_EQ(type, kTypeSentinel);
  EXPECT_TRUE(IsUntouched(key));
}

TEST(JsonDocumentStore, ReadEntrySuccessWritesTextAndType) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t id = InsertOrDie(store, R"({"key":"value"})");
  TextBuffer key = FilledBuffer();
  TextBuffer value = FilledBuffer();
  JsonEntryType type = kTypeSentinel;
  ASSERT_EQ(store.ReadEntry(id, 0, key.data(), SizeOf(key), value.data(), SizeOf(value), type),
            Status::Ok);
  EXPECT_STREQ(key.data(), "/key");
  EXPECT_STREQ(value.data(), "value");
  EXPECT_EQ(type, JsonEntryType::String);
}

// ---------------------------------------------------------------------------
// ReadValue.
// ---------------------------------------------------------------------------

TEST(JsonDocumentStore, ReadValueFailuresLeaveValueTypeAndBufferUntouched) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t id = InsertOrDie(store, R"({"o":{"k":1},"a":[1]})");

  ExpectReadValueFails(store, 0, "", Status::UnknownDocumentId);
  ExpectReadValueFails(store, id, "bad", Status::PathSyntaxError);
  ExpectReadValueFails(store, id, "/x", Status::PathNotFound);
  ExpectReadValueFails(store, id, "/a/3", Status::IndexOutOfRange);
  ExpectReadValueFails(store, id, "/o", Status::TypeMismatch);
  ExpectReadValueFails(store, id, "/a/0/x", Status::TypeMismatch);
}

TEST(JsonDocumentStore, ReadValueBufferArgumentsAreCheckedAfterThePath) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t id = InsertOrDie(store, R"({"o":{"k":1},"s":"text"})");
  JsonEntryType type = kTypeSentinel;

  EXPECT_EQ(store.ReadValue(id, "bad", nullptr, 0, type), Status::PathSyntaxError);
  EXPECT_EQ(store.ReadValue(id, "/missing", nullptr, 0, type), Status::PathNotFound);
  EXPECT_EQ(store.ReadValue(id, "/o", nullptr, 0, type), Status::TypeMismatch);
  EXPECT_EQ(type, kTypeSentinel);

  EXPECT_EQ(store.ReadValue(id, "/s", nullptr, 8, type), Status::InvalidArgument);
  TextBuffer value = FilledBuffer();
  EXPECT_EQ(store.ReadValue(id, "/s", value.data(), 0, type), Status::InvalidArgument);
  EXPECT_EQ(type, kTypeSentinel);
  EXPECT_TRUE(IsUntouched(value));
}

TEST(JsonDocumentStore, ReadValueOnANonEmptyContainerIsTypeMismatchButAnEmptyOneIsALeaf) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t id = InsertOrDie(store, R"({"full":{"k":1},"none":{},"list":[]})");

  ExpectReadValueFails(store, id, "/full", Status::TypeMismatch);

  const ValueResult none = ReadValueAt(store, id, "/none");
  EXPECT_EQ(none.status, Status::Ok);
  EXPECT_EQ(none.text, "{}");
  EXPECT_EQ(none.type, JsonEntryType::EmptyObject);

  const ValueResult list = ReadValueAt(store, id, "/list");
  EXPECT_EQ(list.status, Status::Ok);
  EXPECT_EQ(list.text, "[]");
  EXPECT_EQ(list.type, JsonEntryType::EmptyArray);
}

TEST(JsonDocumentStore, ReadValueWithTooSmallABufferEmptiesItAndLeavesTheTypeUntouched) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t id = InsertOrDie(store, R"({"k":"hello"})");
  std::array<char, 5> value;
  value.fill(kFill);
  JsonEntryType type = kTypeSentinel;
  EXPECT_EQ(store.ReadValue(id, "/k", value.data(), static_cast<std::uint32_t>(value.size()), type),
            Status::BufferTooSmall);
  EXPECT_EQ(value[0], '\0');
  EXPECT_EQ(type, kTypeSentinel);
}

TEST(JsonDocumentStore, ReadValueSuccessWritesTextAndType) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t id = InsertOrDie(store, R"({"k":false})");
  const ValueResult read = ReadValueAt(store, id, "/k");
  EXPECT_EQ(read.status, Status::Ok);
  EXPECT_EQ(read.text, "false");
  EXPECT_EQ(read.type, JsonEntryType::Bool);
}

// ---------------------------------------------------------------------------
// Discard and DiscardAll.
// ---------------------------------------------------------------------------

TEST(JsonDocumentStore, DiscardFreesTheDocumentAndRejectsAnyFurtherUse) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t id = InsertOrDie(store, "[1]");
  EXPECT_EQ(store.Discard(id), Status::Ok);
  EXPECT_EQ(store.Discard(id), Status::UnknownDocumentId);
  ExpectUnknownEverywhere(store, id);
}

TEST(JsonDocumentStore, DiscardAllReportsTheNumberFreedAndEmptiesTheStore) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t a = InsertOrDie(store, "[1]");
  const std::uint32_t b = InsertOrDie(store, "[2]");

  std::uint32_t discarded = kCountSentinel;
  EXPECT_EQ(store.DiscardAll(discarded), Status::Ok);
  EXPECT_EQ(discarded, 2u);

  ExpectUnknownEverywhere(store, a);
  ExpectUnknownEverywhere(store, b);

  discarded = kCountSentinel;
  EXPECT_EQ(store.DiscardAll(discarded), Status::Ok);
  EXPECT_EQ(discarded, 0u);
}

TEST(JsonDocumentStore, DiscardAllFreesEverySlotForNewInserts) {
  JsonDocumentStore store(kFirstId);
  FillStore(store);

  std::uint32_t discarded = kCountSentinel;
  ASSERT_EQ(store.DiscardAll(discarded), Status::Ok);
  ASSERT_EQ(discarded, 8u);

  EXPECT_EQ(FillStore(store).front(), kFirstId + 8);
}

TEST(JsonDocumentStore, DefaultStoreIsOneProcessWideInstance) {
  EXPECT_EQ(&DefaultJsonDocumentStore(), &DefaultJsonDocumentStore());
}

// ---------------------------------------------------------------------------
// Document ids.
// ---------------------------------------------------------------------------

TEST(JsonDocumentIds, ZeroNeverIssuedAndDiscardedIdsAreUnknownForEveryOperation) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t discarded = InsertOrDie(store, "[1]");
  ASSERT_EQ(store.Discard(discarded), Status::Ok);

  ExpectUnknownEverywhere(store, 0);
  ExpectUnknownEverywhere(store, 5);
  ExpectUnknownEverywhere(store, 0xFFFFFFFFu);
  ExpectUnknownEverywhere(store, discarded);
}

TEST(JsonDocumentIds, UnknownIdOnAnEmptyStoreIsUnknown) {
  JsonDocumentStore store(kFirstId);
  ExpectUnknownEverywhere(store, 1);
  ExpectUnknownEverywhere(store, 0);
}

TEST(JsonDocumentIds, DiscardedIdStaysUnknownAfterItsSlotIsReused) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t first = InsertOrDie(store, R"({"v":1})");
  ASSERT_EQ(store.Discard(first), Status::Ok);
  const std::uint32_t second = InsertOrDie(store, R"({"v":2})");
  EXPECT_EQ(first, kFirstId);
  EXPECT_EQ(second, kFirstId + 1);

  ExpectUnknownEverywhere(store, first);
  const ValueResult read = ReadValueAt(store, second, "/v");
  EXPECT_EQ(read.status, Status::Ok);
  EXPECT_EQ(read.text, "2");
}

TEST(JsonDocumentIds, StaleIdDoesNotDiscardTheDocumentThatReusedItsSlot) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t first = InsertOrDie(store, "[1]");
  ASSERT_EQ(store.Discard(first), Status::Ok);
  const std::uint32_t second = InsertOrDie(store, "[2]");

  EXPECT_EQ(store.Discard(first), Status::UnknownDocumentId);
  const ValueResult read = ReadValueAt(store, second, "/0");
  EXPECT_EQ(read.status, Status::Ok);
  EXPECT_EQ(read.text, "2");
}

TEST(JsonDocumentIds, CounterWrapSkipsZeroAndContinuesAtOne) {
  JsonDocumentStore store(0xFFFFFFFDu);
  const std::uint32_t expected[] = {0xFFFFFFFDu, 0xFFFFFFFEu, 0xFFFFFFFFu, 1u, 2u, 3u, 4u, 5u};
  std::vector<std::uint32_t> issued;
  for (const std::uint32_t id : expected) {
    const std::uint32_t minted = InsertOrDie(store, "[1]");
    EXPECT_EQ(minted, id);
    issued.push_back(minted);
    EXPECT_EQ(store.Discard(minted), Status::Ok);
  }
  for (const std::uint32_t id : issued) {
    ExpectUnknownEverywhere(store, id);
  }
}

TEST(JsonDocumentIds, DocumentsLiveAcrossTheCounterWrapStayDistinctAndReadable) {
  JsonDocumentStore store(0xFFFFFFFFu);
  const std::uint32_t beforeWrap = InsertOrDie(store, R"({"v":"before"})");
  const std::uint32_t afterWrap = InsertOrDie(store, R"({"v":"after"})");
  EXPECT_EQ(beforeWrap, 0xFFFFFFFFu);
  EXPECT_EQ(afterWrap, 1u);

  const ValueResult before = ReadValueAt(store, beforeWrap, "/v");
  EXPECT_EQ(before.status, Status::Ok);
  EXPECT_EQ(before.text, "before");
  const ValueResult after = ReadValueAt(store, afterWrap, "/v");
  EXPECT_EQ(after.status, Status::Ok);
  EXPECT_EQ(after.text, "after");
}

TEST(JsonDocumentIds, ClockSeededStoreIssuesNonzeroDistinctIds) {
  JsonDocumentStore store;
  std::set<std::uint32_t> seen;
  for (std::size_t i = 0; i < kJsonDocumentSlotCount; ++i) {
    const std::uint32_t id = InsertOrDie(store, "[1]");
    EXPECT_NE(id, 0u);
    EXPECT_TRUE(seen.insert(id).second);
  }
}

// ---------------------------------------------------------------------------
// ReadWith: runs a callback on the stored document under the lock.
// ---------------------------------------------------------------------------

namespace {

Status LongAtPath(JsonDocumentStore& store, std::uint32_t id, std::string_view path,
                  std::int32_t& out, int* calls = nullptr) {
  return store.ReadWith(id, path, [&](const JsonValue& document) {
    if (calls != nullptr) {
      ++*calls;
    }
    out = document.at("n").get<std::int32_t>();
    return Status::Ok;
  });
}

}  // namespace

TEST(JsonDocumentStoreReadWith, CallbackSeesTheStoredDocumentAndItsStatusIsReturned) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t id = InsertOrDie(store, R"({"n":41})");
  std::int32_t out = 0;
  int calls = 0;
  EXPECT_EQ(LongAtPath(store, id, "/n", out, &calls), Status::Ok);
  EXPECT_EQ(out, 41);
  EXPECT_EQ(calls, 1);

  EXPECT_EQ(store.ReadWith(id, "", [](const JsonValue&) { return Status::NullValue; }),
            Status::NullValue);
}

TEST(JsonDocumentStoreReadWith, ZeroUnknownAndDiscardedIdsAreUnknownDocumentIdAndSkipTheCallback) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t id = InsertOrDie(store, R"({"n":1})");
  std::int32_t out = 0;
  int calls = 0;
  EXPECT_EQ(LongAtPath(store, 0, "/n", out, &calls), Status::UnknownDocumentId);
  EXPECT_EQ(LongAtPath(store, id + 500, "/n", out, &calls), Status::UnknownDocumentId);
  EXPECT_EQ(store.Discard(id), Status::Ok);
  EXPECT_EQ(LongAtPath(store, id, "/n", out, &calls), Status::UnknownDocumentId);
  EXPECT_EQ(calls, 0);
}

TEST(JsonDocumentStoreReadWith, PathSyntaxErrorBeatsUnknownDocumentIdAndSkipsTheCallback) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t id = InsertOrDie(store, R"({"n":1})");
  std::int32_t out = 0;
  int calls = 0;
  EXPECT_EQ(LongAtPath(store, 0, "n", out, &calls), Status::PathSyntaxError);
  EXPECT_EQ(LongAtPath(store, id + 500, "n", out, &calls), Status::PathSyntaxError);
  EXPECT_EQ(LongAtPath(store, id, "n", out, &calls), Status::PathSyntaxError);
  EXPECT_EQ(calls, 0);
}

TEST(JsonDocumentStoreReadWith, TwoDocumentsAreReadIndependently) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t first = InsertOrDie(store, R"({"n":1})");
  const std::uint32_t second = InsertOrDie(store, R"({"n":2})");
  std::int32_t a = 0;
  std::int32_t b = 0;
  EXPECT_EQ(LongAtPath(store, first, "", a), Status::Ok);
  EXPECT_EQ(LongAtPath(store, second, "", b), Status::Ok);
  EXPECT_EQ(a, 1);
  EXPECT_EQ(b, 2);

  EXPECT_EQ(store.Discard(first), Status::Ok);
  EXPECT_EQ(LongAtPath(store, first, "", a), Status::UnknownDocumentId);
  EXPECT_EQ(LongAtPath(store, second, "", b), Status::Ok);
  EXPECT_EQ(b, 2);
}

TEST(JsonDocumentStoreReadWith, ADocumentMintedAfterADiscardDoesNotAnswerTheOldId) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t oldId = InsertOrDie(store, R"({"n":1})");
  EXPECT_EQ(store.Discard(oldId), Status::Ok);
  const std::uint32_t newId = InsertOrDie(store, R"({"n":2})");
  EXPECT_NE(newId, oldId);
  std::int32_t out = 0;
  EXPECT_EQ(LongAtPath(store, oldId, "", out), Status::UnknownDocumentId);
  EXPECT_EQ(LongAtPath(store, newId, "", out), Status::Ok);
  EXPECT_EQ(out, 2);
}

// ---------------------------------------------------------------------------
// ReadWithTokens: as ReadWith, plus the parsed tokens reach the callback.
// ---------------------------------------------------------------------------

namespace {

Status TokensAtPath(JsonDocumentStore& store, std::uint32_t id, std::string_view path,
                    PathTokens& seen, int* calls = nullptr) {
  return store.ReadWithTokens(id, path, [&](const JsonValue&, const PathTokens& tokens) {
    if (calls != nullptr) {
      ++*calls;
    }
    seen = tokens;
    return Status::Ok;
  });
}

}  // namespace

TEST(JsonDocumentStoreReadWithTokens, CallbackReceivesTheDocumentAndTheDecodedTokens) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t id = InsertOrDie(store, R"({"a/b":[10,20]})");

  const struct {
    const char* path;
    PathTokens expected;
  } cases[] = {
      {"", {}},
      {"/a~1b", {"a/b"}},
      {"/a~1b/1", {"a/b", "1"}},
      {"/m~0n/~01", {"m~n", "~1"}},
      {"/", {""}},
      {"//x", {"", "x"}},
  };
  for (const auto& c : cases) {
    SCOPED_TRACE(c.path);
    PathTokens seen = {"stale"};
    int calls = 0;
    EXPECT_EQ(TokensAtPath(store, id, c.path, seen, &calls), Status::Ok);
    EXPECT_EQ(seen, c.expected);
    EXPECT_EQ(calls, 1);
  }

  std::int32_t value = 0;
  EXPECT_EQ(store.ReadWithTokens(id, "/a~1b/1",
                                 [&](const JsonValue& document, const PathTokens& tokens) {
                                   const JsonValue* node = nullptr;
                                   const Status status = ResolvePath(document, tokens, node);
                                   if (status == Status::Ok) {
                                     value = node->get<std::int32_t>();
                                   }
                                   return status;
                                 }),
            Status::Ok);
  EXPECT_EQ(value, 20);
}

TEST(JsonDocumentStoreReadWithTokens, CallbackStatusPassesThrough) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t id = InsertOrDie(store, R"({"n":1})");
  const Status statuses[] = {Status::Ok, Status::NullValue, Status::TypeMismatch,
                             Status::NumericOverflow, Status::PathNotFound};
  for (const Status expected : statuses) {
    SCOPED_TRACE(static_cast<int>(expected));
    EXPECT_EQ(store.ReadWithTokens(
                  id, "/n", [expected](const JsonValue&, const PathTokens&) { return expected; }),
              expected);
  }
}

TEST(JsonDocumentStoreReadWithTokens,
     ZeroUnknownAndDiscardedIdsAreUnknownDocumentIdAndSkipTheCallback) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t id = InsertOrDie(store, R"({"n":1})");
  PathTokens seen = {"stale"};
  int calls = 0;
  EXPECT_EQ(TokensAtPath(store, 0, "/n", seen, &calls), Status::UnknownDocumentId);
  EXPECT_EQ(TokensAtPath(store, id + 500, "/n", seen, &calls), Status::UnknownDocumentId);
  EXPECT_EQ(store.Discard(id), Status::Ok);
  EXPECT_EQ(TokensAtPath(store, id, "/n", seen, &calls), Status::UnknownDocumentId);
  EXPECT_EQ(TokensAtPath(store, id, "", seen, &calls), Status::UnknownDocumentId);
  EXPECT_EQ(calls, 0);
  EXPECT_EQ(seen, PathTokens{"stale"});
}

TEST(JsonDocumentStoreReadWithTokens, PathSyntaxErrorBeatsUnknownDocumentIdAndSkipsTheCallback) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t id = InsertOrDie(store, R"({"n":1})");
  const std::uint32_t discarded = InsertOrDie(store, R"({"n":2})");
  EXPECT_EQ(store.Discard(discarded), Status::Ok);

  PathTokens seen = {"stale"};
  int calls = 0;
  const char* const badPaths[] = {"n", "/a~2", "/~"};
  for (const char* path : badPaths) {
    SCOPED_TRACE(path);
    EXPECT_EQ(TokensAtPath(store, 0, path, seen, &calls), Status::PathSyntaxError);
    EXPECT_EQ(TokensAtPath(store, id + 500, path, seen, &calls), Status::PathSyntaxError);
    EXPECT_EQ(TokensAtPath(store, discarded, path, seen, &calls), Status::PathSyntaxError);
    EXPECT_EQ(TokensAtPath(store, id, path, seen, &calls), Status::PathSyntaxError);
  }
  EXPECT_EQ(calls, 0);
  EXPECT_EQ(seen, PathTokens{"stale"});
}

TEST(JsonDocumentStoreReadWithTokens, TwoDocumentsAreReadIndependently) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t first = InsertOrDie(store, R"({"n":1})");
  const std::uint32_t second = InsertOrDie(store, R"({"n":2})");

  const auto readN = [&](std::uint32_t id, std::int32_t& out) {
    return store.ReadWithTokens(id, "/n", [&](const JsonValue& document, const PathTokens& tokens) {
      const JsonValue* node = nullptr;
      const Status status = ResolvePath(document, tokens, node);
      if (status == Status::Ok) {
        out = node->get<std::int32_t>();
      }
      return status;
    });
  };

  std::int32_t a = 0;
  std::int32_t b = 0;
  EXPECT_EQ(readN(first, a), Status::Ok);
  EXPECT_EQ(readN(second, b), Status::Ok);
  EXPECT_EQ(a, 1);
  EXPECT_EQ(b, 2);

  EXPECT_EQ(store.Discard(first), Status::Ok);
  EXPECT_EQ(readN(first, a), Status::UnknownDocumentId);
  EXPECT_EQ(readN(second, b), Status::Ok);
  EXPECT_EQ(b, 2);
}

TEST(JsonDocumentStoreReadWithTokens, AgreesWithReadWithOnIdAndPathErrors) {
  JsonDocumentStore store(kFirstId);
  const std::uint32_t id = InsertOrDie(store, R"({"n":1})");
  const std::uint32_t ids[] = {0, id, id + 500};
  const char* const paths[] = {"", "/n", "n", "/a~2"};
  for (const std::uint32_t docId : ids) {
    for (const char* path : paths) {
      SCOPED_TRACE(std::string(path) + " @ " + std::to_string(docId));
      const Status viaReadWith =
          store.ReadWith(docId, path, [](const JsonValue&) { return Status::Ok; });
      const Status viaTokens = store.ReadWithTokens(
          docId, path, [](const JsonValue&, const PathTokens&) { return Status::Ok; });
      EXPECT_EQ(viaTokens, viaReadWith);
    }
  }
}
