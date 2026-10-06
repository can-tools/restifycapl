// Coverage for src/mapping/json-text-api.*, each test on its own JsonDocumentStore.

#include "mapping/json-text-api.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "core/status.h"
#include "../test-support/status-print.h"
#include "mapping/json-document-store.h"
#include "mapping/json-flatten.h"

namespace {

// While a FailAllocations is alive, every operator new on this thread throws std::bad_alloc.
thread_local bool t_failAllocations = false;
thread_local int t_failedAllocations = 0;

class FailAllocations {
 public:
  FailAllocations() {
    t_failedAllocations = 0;
    t_failAllocations = true;
  }
  ~FailAllocations() { t_failAllocations = false; }
  FailAllocations(const FailAllocations&) = delete;
  FailAllocations& operator=(const FailAllocations&) = delete;
};

}  // namespace

void* operator new(std::size_t size) {
  if (t_failAllocations) {
    ++t_failedAllocations;
    throw std::bad_alloc();
  }
  if (void* const memory = std::malloc(size != 0 ? size : 1)) {
    return memory;
  }
  throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }

void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

namespace {

constexpr char kFill = 'Z';
// Sentinels no successful call can produce: types are 0..6, counts are at most 10000.
constexpr std::int32_t kTypeSentinel = -777;
constexpr std::uint32_t kIdSentinel = 0xDEADBEEFu;
constexpr std::uint32_t kCountSentinel = 0xC0FFEE11u;

constexpr std::int32_t kString = 1;
constexpr std::int32_t kNumber = 2;
constexpr std::int32_t kBool = 3;
constexpr std::int32_t kNull = 4;
constexpr std::int32_t kEmptyObject = 5;
constexpr std::int32_t kEmptyArray = 6;

constexpr const char* kRichDocument =
    R"({"s":"text","n":-12,"f":2.5,"t":true,"z":null,"e":"","o":{},"r":[],"a":[1,{"k":"v"}]})";

struct ExpectedEntry {
  const char* key;
  const char* value;
  std::int32_t type;
};

const std::vector<ExpectedEntry>& RichEntries() {
  static const std::vector<ExpectedEntry> entries = {
      {"/s", "text", kString},      {"/n", "-12", kNumber},      {"/f", "2.5", kNumber},
      {"/t", "true", kBool},        {"/z", "null", kNull},       {"/e", "", kString},
      {"/o", "{}", kEmptyObject},   {"/r", "[]", kEmptyArray},   {"/a/0", "1", kNumber},
      {"/a/1/k", "v", kString},
  };
  return entries;
}

class Buffer {
 public:
  explicit Buffer(std::size_t size) : bytes_(size, kFill) {}

  char* Data() { return bytes_.data(); }
  std::uint32_t Size() const { return static_cast<std::uint32_t>(bytes_.size()); }

  std::string Text() const {
    const void* nul = std::memchr(bytes_.data(), '\0', bytes_.size());
    const std::size_t length =
        nul != nullptr ? static_cast<std::size_t>(static_cast<const char*>(nul) - bytes_.data())
                       : bytes_.size();
    return std::string(bytes_.data(), length);
  }

  std::string Raw(std::size_t length) const { return std::string(bytes_.data(), length); }

  bool Untouched() const {
    return std::all_of(bytes_.begin(), bytes_.end(), [](char c) { return c == kFill; });
  }

  bool HoldsEmptyText() const { return !bytes_.empty() && bytes_[0] == '\0'; }

 private:
  std::vector<char> bytes_;
};

Status ParseText(JsonDocumentStore& store, const std::string& json, std::uint32_t& id) {
  return ParseJsonDocument(store, json.c_str(), static_cast<std::uint32_t>(json.size() + 1), id);
}

std::uint32_t MustParse(JsonDocumentStore& store, const std::string& json) {
  std::uint32_t id = kIdSentinel;
  EXPECT_EQ(ParseText(store, json, id), Status::Ok);
  EXPECT_NE(id, 0u);
  return id;
}

void ExpectParseFails(JsonDocumentStore& store, const std::string& json, Status expected) {
  SCOPED_TRACE(json.substr(0, 80));
  std::uint32_t id = kIdSentinel;
  EXPECT_EQ(ParseText(store, json, id), expected);
  EXPECT_EQ(id, 0u);
}

std::uint32_t CountOf(JsonDocumentStore& store, std::uint32_t id) {
  std::uint32_t count = kCountSentinel;
  EXPECT_EQ(CountJsonEntries(store, id, count), Status::Ok);
  return count;
}

std::vector<std::uint32_t> FillPool(JsonDocumentStore& store) {
  std::vector<std::uint32_t> ids;
  for (std::size_t i = 0; i < kJsonDocumentSlotCount; ++i) {
    ids.push_back(MustParse(store, "{\"n\":" + std::to_string(i) + "}"));
  }
  return ids;
}

struct EntryRead {
  Status status;
  std::string key;
  std::string value;
  std::int32_t type;
};

EntryRead ReadEntryWith(JsonDocumentStore& store, std::uint32_t id, std::uint32_t index,
                        std::size_t keySize = 256, std::size_t valueSize = 256) {
  Buffer key(keySize);
  Buffer value(valueSize);
  std::int32_t type = kTypeSentinel;
  const Status status =
      ReadJsonEntry(store, id, index, key.Data(), key.Size(), value.Data(), value.Size(), type);
  return EntryRead{status, key.Text(), value.Text(), type};
}

void ExpectEntry(JsonDocumentStore& store, std::uint32_t id, std::uint32_t index,
                 const ExpectedEntry& expected) {
  SCOPED_TRACE("entry " + std::to_string(index));
  const EntryRead read = ReadEntryWith(store, id, index);
  EXPECT_EQ(read.status, Status::Ok);
  EXPECT_EQ(read.key, expected.key);
  EXPECT_EQ(read.value, expected.value);
  EXPECT_EQ(read.type, expected.type);
}

struct ValueRead {
  Status status;
  std::string value;
  std::int32_t type;
};

ValueRead ReadValueAt(JsonDocumentStore& store, std::uint32_t id, const std::string& path,
                      std::size_t valueSize = 256) {
  Buffer value(valueSize);
  std::int32_t type = kTypeSentinel;
  const Status status =
      ReadJsonValue(store, id, path.c_str(), static_cast<std::uint32_t>(path.size() + 1),
                    value.Data(), value.Size(), type);
  return ValueRead{status, value.Text(), type};
}

void ExpectValue(JsonDocumentStore& store, std::uint32_t id, const std::string& path,
                 const std::string& expectedValue, std::int32_t expectedType) {
  SCOPED_TRACE("path '" + path + "'");
  const ValueRead read = ReadValueAt(store, id, path);
  EXPECT_EQ(read.status, Status::Ok);
  EXPECT_EQ(read.value, expectedValue);
  EXPECT_EQ(read.type, expectedType);
}

// For every status except BufferTooSmall: the value buffer must stay as the caller left it.
void ExpectValueRejected(JsonDocumentStore& store, std::uint32_t id, const std::string& path,
                         Status expected) {
  SCOPED_TRACE("path '" + path + "'");
  Buffer value(16);
  std::int32_t type = kTypeSentinel;
  EXPECT_EQ(ReadJsonValue(store, id, path.c_str(), static_cast<std::uint32_t>(path.size() + 1),
                          value.Data(), value.Size(), type),
            expected);
  EXPECT_EQ(type, 0);
  EXPECT_TRUE(value.Untouched());
}

void ExpectUnknownEverywhere(JsonDocumentStore& store, std::uint32_t id) {
  SCOPED_TRACE("id " + std::to_string(id));

  std::uint32_t count = kCountSentinel;
  EXPECT_EQ(CountJsonEntries(store, id, count), Status::UnknownDocumentId);
  EXPECT_EQ(count, 0u);

  Buffer key(16);
  Buffer value(16);
  std::int32_t type = kTypeSentinel;
  EXPECT_EQ(ReadJsonEntry(store, id, 0, key.Data(), key.Size(), value.Data(), value.Size(), type),
            Status::UnknownDocumentId);
  EXPECT_EQ(type, 0);
  EXPECT_TRUE(key.Untouched());
  EXPECT_TRUE(value.Untouched());

  ExpectValueRejected(store, id, "", Status::UnknownDocumentId);
  EXPECT_EQ(DiscardJsonDocument(store, id), Status::UnknownDocumentId);
}

std::string PaddedToSize(std::string text, std::size_t size) {
  text.resize(size, ' ');
  return text;
}

std::string NestedArrays(std::size_t depth) {
  return std::string(depth, '[') + std::string(depth, ']');
}

std::string FlatArrayOf(const std::string& element, std::size_t count) {
  std::string text = "[";
  for (std::size_t i = 0; i < count; ++i) {
    if (i != 0) {
      text += ',';
    }
    text += element;
  }
  text += ']';
  return text;
}

// 63 nested objects with a 15000-byte key each, innermost value an array of 9000 zeros: 64 levels,
// 9000 leaves, 963316 bytes, and every leaf key is over 945000 bytes long.
std::string DeepLongKeyDocument() {
  const std::string key(15000, 'k');
  std::string text;
  for (int i = 0; i < 63; ++i) {
    text += "{\"" + key + "\":";
  }
  text += FlatArrayOf("0", 9000);
  text.append(63, '}');
  return text;
}

}  // namespace

TEST(TextApiStatus, InternalErrorHasTheDocumentedNumber) {
  EXPECT_EQ(static_cast<int>(Status::InternalError), -35);
}

TEST(TextApiStatus, KeyTextTooLargeHasTheDocumentedNumber) {
  EXPECT_EQ(static_cast<int>(Status::KeyTextTooLarge), -36);
}

// ---------------------------------------------------------------------------
// ParseJsonDocument: success and input bounds.
// ---------------------------------------------------------------------------

TEST(ParseJsonDocument, ValidDocumentReturnsOkAndANonzeroId) {
  JsonDocumentStore store;
  std::uint32_t id = 0;
  EXPECT_EQ(ParseText(store, R"({"a":1})", id), Status::Ok);
  EXPECT_NE(id, 0u);
}

TEST(ParseJsonDocument, SuccessOverwritesAPresetDocumentId) {
  JsonDocumentStore store(100);
  std::uint32_t id = kIdSentinel;
  ASSERT_EQ(ParseText(store, "[1]", id), Status::Ok);
  EXPECT_EQ(id, 100u);
}

TEST(ParseJsonDocument, ScalarAndEmptyContainerRootsAreAccepted) {
  JsonDocumentStore store;
  const char* const documents[] = {"42", "\"text\"", "\"\"", "true", "null", "{}", "[]"};
  for (const char* document : documents) {
    SCOPED_TRACE(document);
    const std::uint32_t id = MustParse(store, document);
    EXPECT_EQ(CountOf(store, id), 1u);
    EXPECT_EQ(DiscardJsonDocument(store, id), Status::Ok);
  }
}

TEST(ParseJsonDocument, LeadingUtf8ByteOrderMarkIsSkippedAndNotPartOfTheDocument) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, std::string("\xEF\xBB\xBF") + "[1]");
  EXPECT_EQ(CountOf(store, id), 1u);
  ExpectEntry(store, id, 0, {"/0", "1", kNumber});
}

TEST(ParseJsonDocument, NullTextIsInvalidArgumentAndZeroesTheId) {
  JsonDocumentStore store;
  std::uint32_t id = kIdSentinel;
  EXPECT_EQ(ParseJsonDocument(store, nullptr, 8, id), Status::InvalidArgument);
  EXPECT_EQ(id, 0u);
}

TEST(ParseJsonDocument, ZeroSizeIsInvalidArgumentAndZeroesTheId) {
  JsonDocumentStore store;
  std::uint32_t id = kIdSentinel;
  EXPECT_EQ(ParseJsonDocument(store, "[1]", 0, id), Status::InvalidArgument);
  EXPECT_EQ(id, 0u);
}

TEST(ParseJsonDocument, TextWithoutANulInsideTheSizeIsUnterminatedInputText) {
  JsonDocumentStore store;
  const std::array<char, 3> noNul = {'[', '1', ']'};
  std::uint32_t id = kIdSentinel;
  EXPECT_EQ(ParseJsonDocument(store, noNul.data(), static_cast<std::uint32_t>(noNul.size()), id),
            Status::UnterminatedInputText);
  EXPECT_EQ(id, 0u);
}

TEST(ParseJsonDocument, SizeExactlyCoveringTheNulIsAcceptedAndOneShortIsNot) {
  JsonDocumentStore store;
  const std::string text = "[1,2]";
  std::uint32_t id = kIdSentinel;

  EXPECT_EQ(ParseJsonDocument(store, text.c_str(), 5, id), Status::UnterminatedInputText);
  EXPECT_EQ(id, 0u);

  id = kIdSentinel;
  EXPECT_EQ(ParseJsonDocument(store, text.c_str(), 6, id), Status::Ok);
  EXPECT_EQ(CountOf(store, id), 2u);
}

TEST(ParseJsonDocument, SizeSmallerThanTheTextWithTheNulBeyondItIsUnterminatedInputText) {
  JsonDocumentStore store;
  const char text[] = "[1]\0tail";
  std::uint32_t id = kIdSentinel;
  EXPECT_EQ(ParseJsonDocument(store, text, 2, id), Status::UnterminatedInputText);
  EXPECT_EQ(id, 0u);
}

TEST(ParseJsonDocument, SizeLargerThanTheTextUsesOnlyTheBytesBeforeTheNul) {
  JsonDocumentStore store;
  const char text[] = "[1]\0this is not json";
  std::uint32_t id = kIdSentinel;
  ASSERT_EQ(ParseJsonDocument(store, text, sizeof(text), id), Status::Ok);
  EXPECT_EQ(CountOf(store, id), 1u);
  ExpectValue(store, id, "/0", "1", kNumber);
}

TEST(ParseJsonDocument, FailedInputChecksDoNotConsumeASlot) {
  JsonDocumentStore store;
  std::uint32_t id = 0;
  EXPECT_EQ(ParseJsonDocument(store, nullptr, 4, id), Status::InvalidArgument);
  EXPECT_EQ(ParseJsonDocument(store, "[1]", 0, id), Status::InvalidArgument);
  const std::array<char, 3> noNul = {'[', '1', ']'};
  EXPECT_EQ(ParseJsonDocument(store, noNul.data(), 3, id), Status::UnterminatedInputText);
  EXPECT_EQ(FillPool(store).size(), kJsonDocumentSlotCount);
}

// ---------------------------------------------------------------------------
// ParseJsonDocument: malformed text.
// ---------------------------------------------------------------------------

TEST(ParseJsonDocument, InvalidJsonIsParseError) {
  JsonDocumentStore store;
  const char* const texts[] = {"{",       "[1,",       "{\"a\":}", "{'a':1",
                               "{'a':'it's'}", "{'a\":1}",
                               "[1,]",    "tru",       "01",       "NaN",
                               "{} x",    "1 2",       "abc",      "{\"a\":1,}"};
  for (const char* text : texts) {
    ExpectParseFails(store, text, Status::ParseError);
  }
}

TEST(ParseJsonDocument, ApostropheObjectIsAcceptedNotAParseError) {
  JsonDocumentStore store;
  EXPECT_NE(MustParse(store, "{'a':1}"), 0u);
}

TEST(ParseJsonDocument, EmptyAndWhitespaceOnlyTextIsParseError) {
  JsonDocumentStore store;
  const char* const texts[] = {"", " ", "\t", "\r\n", " \t\r\n "};
  for (const char* text : texts) {
    ExpectParseFails(store, text, Status::ParseError);
  }
}

TEST(ParseJsonDocument, InvalidUtf8IsParseError) {
  JsonDocumentStore store;
  const std::string texts[] = {"\"\xC3\x28\"", "\"\xFF\"", "{\"\xC3\x28\":1}", "\xFF",
                               "[\"ok\",\"\xE2\x82\"]"};
  for (const std::string& text : texts) {
    ExpectParseFails(store, text, Status::ParseError);
  }
}

TEST(ParseJsonDocument, NulInAnObjectKeyIsParseErrorZeroesTheIdAndConsumesNoSlot) {
  JsonDocumentStore store;
  ExpectParseFails(store, R"({"a\u0000b":"v"})", Status::ParseError);
  ExpectParseFails(store, R"({"x":[{"\u0000":1}]})", Status::ParseError);
  EXPECT_EQ(FillPool(store).size(), kJsonDocumentSlotCount);
}

TEST(ParseJsonDocument, NulAsTheFirstByteLeavesEmptyTextAndIsParseError) {
  JsonDocumentStore store;
  const char text[] = "\0[1]";
  std::uint32_t id = kIdSentinel;
  EXPECT_EQ(ParseJsonDocument(store, text, sizeof(text), id), Status::ParseError);
  EXPECT_EQ(id, 0u);
  EXPECT_EQ(FillPool(store).size(), kJsonDocumentSlotCount);
}

TEST(ParseJsonDocument, MalformedTextDoesNotConsumeASlot) {
  JsonDocumentStore store;
  ExpectParseFails(store, "{", Status::ParseError);
  ExpectParseFails(store, "", Status::ParseError);
  EXPECT_EQ(FillPool(store).size(), kJsonDocumentSlotCount);
}

// ---------------------------------------------------------------------------
// ParseJsonDocument: limits, each just below and just above.
// ---------------------------------------------------------------------------

TEST(ParseJsonDocument, InputOfExactlyOneMiBIsAccepted) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, PaddedToSize("[0]", kMaxJsonInputBytes));
  EXPECT_EQ(CountOf(store, id), 1u);
}

TEST(ParseJsonDocument, InputOneByteOverOneMiBIsDocumentTooLarge) {
  JsonDocumentStore store;
  ExpectParseFails(store, PaddedToSize("[0]", kMaxJsonInputBytes + 1), Status::DocumentTooLarge);
}

TEST(ParseJsonDocument, SizeLimitCountsBytesBeforeTheNulNotTheStatedSize) {
  JsonDocumentStore store;
  std::string text = PaddedToSize("[0]", kMaxJsonInputBytes);
  text.append(1001, '\0');
  std::uint32_t id = kIdSentinel;
  EXPECT_EQ(ParseJsonDocument(store, text.data(), static_cast<std::uint32_t>(text.size()), id),
            Status::Ok);
  EXPECT_NE(id, 0u);
}

TEST(ParseJsonDocument, NestingAtDepthSixtyFourIsAcceptedAndSixtyFiveIsNestingTooDeep) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, NestedArrays(kMaxJsonDepth));
  EXPECT_EQ(CountOf(store, id), 1u);
  ExpectParseFails(store, NestedArrays(kMaxJsonDepth + 1), Status::NestingTooDeep);
}

TEST(ParseJsonDocument, TenThousandEntriesAreAcceptedAndOneMoreIsTooManyEntries) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, FlatArrayOf("0", kMaxFlatEntries));
  EXPECT_EQ(CountOf(store, id), kMaxFlatEntries);
  ExpectParseFails(store, FlatArrayOf("0", kMaxFlatEntries + 1), Status::TooManyEntries);
}

TEST(ParseJsonDocument, LimitFailuresDoNotConsumeASlot) {
  JsonDocumentStore store;
  ExpectParseFails(store, PaddedToSize("[0]", kMaxJsonInputBytes + 1), Status::DocumentTooLarge);
  ExpectParseFails(store, NestedArrays(kMaxJsonDepth + 1), Status::NestingTooDeep);
  ExpectParseFails(store, FlatArrayOf("0", kMaxFlatEntries + 1), Status::TooManyEntries);
  ExpectParseFails(store, DeepLongKeyDocument(), Status::KeyTextTooLarge);
  EXPECT_EQ(FillPool(store).size(), kJsonDocumentSlotCount);
}

TEST(ParseJsonDocument, DeepLongKeyDocumentIsKeyTextTooLargeNotInternalErrorAndZeroesTheId) {
  JsonDocumentStore store;
  const std::string text = DeepLongKeyDocument();
  ASSERT_LE(text.size(), kMaxJsonInputBytes);
  std::uint32_t id = kIdSentinel;
  EXPECT_EQ(ParseText(store, text, id), Status::KeyTextTooLarge);
  EXPECT_EQ(id, 0u);
}

TEST(ParseJsonDocument, KeyTextTooLargeIsReportedEvenWhenThePoolIsFull) {
  JsonDocumentStore store;
  FillPool(store);
  ExpectParseFails(store, DeepLongKeyDocument(), Status::KeyTextTooLarge);
}

// ---------------------------------------------------------------------------
// ParseJsonDocument: the pool of 8 slots.
// ---------------------------------------------------------------------------

TEST(ParseJsonDocument, EightDocumentsAreAcceptedWithDistinctIds) {
  JsonDocumentStore store;
  const std::vector<std::uint32_t> ids = FillPool(store);
  ASSERT_EQ(ids.size(), 8u);
  EXPECT_EQ(std::set<std::uint32_t>(ids.begin(), ids.end()).size(), 8u);
  for (const std::uint32_t id : ids) {
    EXPECT_EQ(CountOf(store, id), 1u);
  }
}

TEST(ParseJsonDocument, NinthDocumentIsNoFreeDocumentSlotAndZeroesTheId) {
  JsonDocumentStore store;
  const std::vector<std::uint32_t> ids = FillPool(store);
  ExpectParseFails(store, "[1]", Status::NoFreeDocumentSlot);
  for (std::size_t i = 0; i < ids.size(); ++i) {
    ExpectValue(store, ids[i], "/n", std::to_string(i), kNumber);
  }
}

TEST(ParseJsonDocument, DiscardingOneDocumentFreesASlotUnderANewId) {
  JsonDocumentStore store;
  const std::vector<std::uint32_t> ids = FillPool(store);
  ASSERT_EQ(DiscardJsonDocument(store, ids[3]), Status::Ok);

  const std::uint32_t replacement = MustParse(store, R"({"n":99})");
  EXPECT_NE(replacement, 0u);
  EXPECT_EQ(std::count(ids.begin(), ids.end(), replacement), 0);

  ExpectValue(store, replacement, "/n", "99", kNumber);
  ExpectParseFails(store, "[1]", Status::NoFreeDocumentSlot);
}

TEST(ParseJsonDocument, DiscardedIdIsNotHandedOutAgainByTheNextParses) {
  JsonDocumentStore store;
  const std::uint32_t discarded = MustParse(store, "[1]");
  ASSERT_EQ(DiscardJsonDocument(store, discarded), Status::Ok);
  for (int i = 0; i < 20; ++i) {
    const std::uint32_t id = MustParse(store, "[2]");
    EXPECT_NE(id, discarded);
    EXPECT_EQ(DiscardJsonDocument(store, id), Status::Ok);
  }
}

TEST(ParseJsonDocument, MalformedTextReportsItsOwnErrorEvenWhenThePoolIsFull) {
  JsonDocumentStore store;
  FillPool(store);
  ExpectParseFails(store, "{", Status::ParseError);
  ExpectParseFails(store, "", Status::ParseError);
  ExpectParseFails(store, "\"\xC3\x28\"", Status::ParseError);
  ExpectParseFails(store, PaddedToSize("[0]", kMaxJsonInputBytes + 1), Status::DocumentTooLarge);
  ExpectParseFails(store, NestedArrays(kMaxJsonDepth + 1), Status::NestingTooDeep);
  ExpectParseFails(store, FlatArrayOf("0", kMaxFlatEntries + 1), Status::TooManyEntries);
}

TEST(ParseJsonDocument, InputChecksComeBeforeThePoolCheckWhenThePoolIsFull) {
  JsonDocumentStore store;
  FillPool(store);
  std::uint32_t id = kIdSentinel;
  EXPECT_EQ(ParseJsonDocument(store, nullptr, 4, id), Status::InvalidArgument);
  EXPECT_EQ(id, 0u);
  id = kIdSentinel;
  EXPECT_EQ(ParseJsonDocument(store, "[1]", 0, id), Status::InvalidArgument);
  EXPECT_EQ(id, 0u);
  id = kIdSentinel;
  const std::array<char, 3> noNul = {'[', '1', ']'};
  EXPECT_EQ(ParseJsonDocument(store, noNul.data(), 3, id), Status::UnterminatedInputText);
  EXPECT_EQ(id, 0u);
}

TEST(ParseJsonDocument, ValidDocumentIsNoFreeDocumentSlotOnlyWhenThePoolIsFull) {
  JsonDocumentStore store;
  const std::vector<std::uint32_t> ids = FillPool(store);
  ExpectParseFails(store, R"({"a":1})", Status::NoFreeDocumentSlot);
  ASSERT_EQ(DiscardJsonDocument(store, ids[0]), Status::Ok);
  EXPECT_NE(MustParse(store, R"({"a":1})"), 0u);
}

// ---------------------------------------------------------------------------
// CountJsonEntries.
// ---------------------------------------------------------------------------

TEST(CountJsonEntries, CountsLeavesInTheFlattenedDocument) {
  JsonDocumentStore store;
  EXPECT_EQ(CountOf(store, MustParse(store, R"({"a":{"b":[10,20]}})")), 2u);
  EXPECT_EQ(CountOf(store, MustParse(store, "42")), 1u);
  EXPECT_EQ(CountOf(store, MustParse(store, "{}")), 1u);
  EXPECT_EQ(CountOf(store, MustParse(store, "[]")), 1u);
  EXPECT_EQ(CountOf(store, MustParse(store, kRichDocument)), RichEntries().size());
}

TEST(CountJsonEntries, EmptyContainersAndNullAndEmptyStringCountAsEntries) {
  JsonDocumentStore store;
  EXPECT_EQ(CountOf(store, MustParse(store, R"([{},[],null,""])")), 4u);
}

TEST(CountJsonEntries, DuplicateKeysCountOncePerSurvivingKey) {
  JsonDocumentStore store;
  EXPECT_EQ(CountOf(store, MustParse(store, R"({"a":1,"a":2})")), 1u);
  EXPECT_EQ(CountOf(store, MustParse(store, R"({"a":{"x":1},"b":0,"a":{"y":2}})")), 2u);
}

TEST(CountJsonEntries, MatchesTheFlattenResultForTheMaximumEntryCount) {
  JsonDocumentStore store;
  const std::string text = FlatArrayOf("0", kMaxFlatEntries);
  FlattenResult flattened;
  ASSERT_EQ(FlattenJson(text, flattened), Status::Ok);
  EXPECT_EQ(CountOf(store, MustParse(store, text)), flattened.entries.size());
}

TEST(CountJsonEntries, SuccessOverwritesAPresetCount) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, "[1,2,3]");
  std::uint32_t count = kCountSentinel;
  EXPECT_EQ(CountJsonEntries(store, id, count), Status::Ok);
  EXPECT_EQ(count, 3u);
}

TEST(CountJsonEntries, UnknownIdIsUnknownDocumentIdAndZeroesTheCount) {
  JsonDocumentStore store;
  MustParse(store, "[1]");
  std::uint32_t count = kCountSentinel;
  EXPECT_EQ(CountJsonEntries(store, 0, count), Status::UnknownDocumentId);
  EXPECT_EQ(count, 0u);
}

// ---------------------------------------------------------------------------
// ReadJsonEntry: content.
// ---------------------------------------------------------------------------

TEST(ReadJsonEntry, EntriesComeByIndexInDocumentOrderWithKeyValueAndType) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, kRichDocument);
  const std::vector<ExpectedEntry>& expected = RichEntries();
  ASSERT_EQ(CountOf(store, id), expected.size());
  for (std::size_t i = 0; i < expected.size(); ++i) {
    ExpectEntry(store, id, static_cast<std::uint32_t>(i), expected[i]);
  }
}

TEST(ReadJsonEntry, EveryEntryValueTypeIsReported) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"(["s",1,true,null,{},[]])");
  const std::int32_t expectedTypes[] = {kString, kNumber, kBool, kNull, kEmptyObject, kEmptyArray};
  for (std::uint32_t i = 0; i < 6; ++i) {
    SCOPED_TRACE("entry " + std::to_string(i));
    const EntryRead read = ReadEntryWith(store, id, i);
    EXPECT_EQ(read.status, Status::Ok);
    EXPECT_EQ(read.type, expectedTypes[i]);
  }
}

TEST(ReadJsonEntry, NestedDocumentIsReadPreOrderInDocumentOrder) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"b":{"y":1,"x":2},"a":[3,{"z":4}],"c":{}})");
  const std::vector<ExpectedEntry> expected = {
      {"/b/y", "1", kNumber}, {"/b/x", "2", kNumber},   {"/a/0", "3", kNumber},
      {"/a/1/z", "4", kNumber}, {"/c", "{}", kEmptyObject},
  };
  ASSERT_EQ(CountOf(store, id), expected.size());
  for (std::size_t i = 0; i < expected.size(); ++i) {
    ExpectEntry(store, id, static_cast<std::uint32_t>(i), expected[i]);
  }
}

TEST(ReadJsonEntry, KeysAreJsonPointersWithEscapesAndEmptyTokens) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"a/b":1,"m~n":2,"x":{"":3},"":{"0":4}})");
  const std::vector<ExpectedEntry> expected = {
      {"/a~1b", "1", kNumber},
      {"/m~0n", "2", kNumber},
      {"/x/", "3", kNumber},
      {"//0", "4", kNumber},
  };
  ASSERT_EQ(CountOf(store, id), expected.size());
  for (std::size_t i = 0; i < expected.size(); ++i) {
    ExpectEntry(store, id, static_cast<std::uint32_t>(i), expected[i]);
  }
}

TEST(ReadJsonEntry, DuplicateKeyEntryHoldsTheLastValueAtTheFirstPosition) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"a":{"x":1},"b":0,"a":{"y":2}})");
  ExpectEntry(store, id, 0, {"/a/y", "2", kNumber});
  ExpectEntry(store, id, 1, {"/b", "0", kNumber});
}

TEST(ReadJsonEntry, ScalarRootHasOneEntryWithTheEmptyKeyFittingAOneByteBuffer) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, "42");
  const EntryRead read = ReadEntryWith(store, id, 0, 1, 3);
  EXPECT_EQ(read.status, Status::Ok);
  EXPECT_EQ(read.key, "");
  EXPECT_EQ(read.value, "42");
  EXPECT_EQ(read.type, kNumber);
}

TEST(ReadJsonEntry, EmptyStringValueIsAStringEntryFittingAOneByteBuffer) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"("")");
  const EntryRead read = ReadEntryWith(store, id, 0, 1, 1);
  EXPECT_EQ(read.status, Status::Ok);
  EXPECT_EQ(read.key, "");
  EXPECT_EQ(read.value, "");
  EXPECT_EQ(read.type, kString);
}

TEST(ReadJsonEntry, LastEntryOfAMaximumSizeDocumentIsReadable) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, FlatArrayOf("0", kMaxFlatEntries));
  ExpectEntry(store, id, static_cast<std::uint32_t>(kMaxFlatEntries - 1),
              {"/9999", "0", kNumber});
  EXPECT_EQ(ReadEntryWith(store, id, static_cast<std::uint32_t>(kMaxFlatEntries)).status,
            Status::IndexOutOfRange);
}

TEST(ReadJsonEntry, SuccessOverwritesAPresetValueType) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, "[true]");
  Buffer key(8);
  Buffer value(8);
  std::int32_t type = kTypeSentinel;
  ASSERT_EQ(ReadJsonEntry(store, id, 0, key.Data(), key.Size(), value.Data(), value.Size(), type),
            Status::Ok);
  EXPECT_EQ(type, kBool);
}

// ---------------------------------------------------------------------------
// ReadJsonEntry: failures.
// ---------------------------------------------------------------------------

TEST(ReadJsonEntry, IndexEqualToTheCountIsIndexOutOfRange) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, "[1,2,3]");
  Buffer key(8);
  Buffer value(8);
  std::int32_t type = kTypeSentinel;
  EXPECT_EQ(ReadJsonEntry(store, id, 3, key.Data(), key.Size(), value.Data(), value.Size(), type),
            Status::IndexOutOfRange);
  EXPECT_EQ(type, 0);
  EXPECT_TRUE(key.Untouched());
  EXPECT_TRUE(value.Untouched());
  EXPECT_EQ(ReadEntryWith(store, id, 2).status, Status::Ok);
}

TEST(ReadJsonEntry, VeryLargeIndexIsIndexOutOfRange) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, "[1]");
  EXPECT_EQ(ReadEntryWith(store, id, 0xFFFFFFFFu).status, Status::IndexOutOfRange);
  EXPECT_EQ(ReadEntryWith(store, id, 0x80000000u).status, Status::IndexOutOfRange);
}

TEST(ReadJsonEntry, UnknownIdIsCheckedBeforeTheIndexAndTheBuffers) {
  JsonDocumentStore store;
  MustParse(store, "[1]");
  std::int32_t type = kTypeSentinel;
  EXPECT_EQ(ReadJsonEntry(store, 0, 99, nullptr, 0, nullptr, 0, type), Status::UnknownDocumentId);
  EXPECT_EQ(type, 0);
}

TEST(ReadJsonEntry, IndexIsCheckedBeforeTheBuffers) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, "[1]");
  std::int32_t type = kTypeSentinel;
  EXPECT_EQ(ReadJsonEntry(store, id, 1, nullptr, 0, nullptr, 0, type), Status::IndexOutOfRange);
  EXPECT_EQ(type, 0);
}

TEST(ReadJsonEntry, NullOrZeroSizeBufferIsInvalidArgumentAndNeitherBufferIsWritten) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"k":"v"})");

  {
    Buffer value(8);
    std::int32_t type = kTypeSentinel;
    EXPECT_EQ(ReadJsonEntry(store, id, 0, nullptr, 8, value.Data(), value.Size(), type),
              Status::InvalidArgument);
    EXPECT_EQ(type, 0);
    EXPECT_TRUE(value.Untouched());
  }
  {
    Buffer key(8);
    std::int32_t type = kTypeSentinel;
    EXPECT_EQ(ReadJsonEntry(store, id, 0, key.Data(), key.Size(), nullptr, 8, type),
              Status::InvalidArgument);
    EXPECT_EQ(type, 0);
    EXPECT_TRUE(key.Untouched());
  }
  {
    Buffer key(8);
    Buffer value(8);
    std::int32_t type = kTypeSentinel;
    EXPECT_EQ(ReadJsonEntry(store, id, 0, key.Data(), 0, value.Data(), value.Size(), type),
              Status::InvalidArgument);
    EXPECT_EQ(type, 0);
    EXPECT_TRUE(key.Untouched());
    EXPECT_TRUE(value.Untouched());
  }
  {
    Buffer key(8);
    Buffer value(8);
    std::int32_t type = kTypeSentinel;
    EXPECT_EQ(ReadJsonEntry(store, id, 0, key.Data(), key.Size(), value.Data(), 0, type),
              Status::InvalidArgument);
    EXPECT_EQ(type, 0);
    EXPECT_TRUE(key.Untouched());
    EXPECT_TRUE(value.Untouched());
  }
}

TEST(ReadJsonEntry, NullOrZeroSizeBufferOutranksATooSmallOtherBuffer) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"key":"value"})");

  {
    Buffer value(2);
    std::int32_t type = kTypeSentinel;
    EXPECT_EQ(ReadJsonEntry(store, id, 0, nullptr, 64, value.Data(), value.Size(), type),
              Status::InvalidArgument);
    EXPECT_EQ(type, 0);
    EXPECT_TRUE(value.Untouched());
  }
  {
    Buffer key(2);
    std::int32_t type = kTypeSentinel;
    EXPECT_EQ(ReadJsonEntry(store, id, 0, key.Data(), key.Size(), nullptr, 64, type),
              Status::InvalidArgument);
    EXPECT_EQ(type, 0);
    EXPECT_TRUE(key.Untouched());
  }
  {
    Buffer key(64);
    Buffer value(2);
    std::int32_t type = kTypeSentinel;
    EXPECT_EQ(ReadJsonEntry(store, id, 0, key.Data(), 0, value.Data(), value.Size(), type),
              Status::InvalidArgument);
    EXPECT_EQ(type, 0);
    EXPECT_TRUE(key.Untouched());
    EXPECT_TRUE(value.Untouched());
  }
}

TEST(ReadJsonEntry, BothBuffersTooSmallIsBufferTooSmallAndBothAreEmptied) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"key":"value"})");
  Buffer key(2);
  Buffer value(2);
  std::int32_t type = kTypeSentinel;
  EXPECT_EQ(ReadJsonEntry(store, id, 0, key.Data(), key.Size(), value.Data(), value.Size(), type),
            Status::BufferTooSmall);
  EXPECT_EQ(type, 0);
  EXPECT_TRUE(key.HoldsEmptyText());
  EXPECT_TRUE(value.HoldsEmptyText());
}

TEST(ReadJsonEntry, BooleanFalseIsReadAsBoolWithTheTextFalse) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"on":true,"off":false})");
  ExpectEntry(store, id, 0, {"/on", "true", kBool});
  ExpectEntry(store, id, 1, {"/off", "false", kBool});
}

TEST(ReadJsonEntry, NulInsideAStringValueIsCopiedWithItsFullLength) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"k":"a\u0000b"})");

  Buffer key(8);
  Buffer value(4);
  std::int32_t type = kTypeSentinel;
  ASSERT_EQ(ReadJsonEntry(store, id, 0, key.Data(), key.Size(), value.Data(), value.Size(), type),
            Status::Ok);
  EXPECT_EQ(type, kString);
  EXPECT_EQ(value.Raw(4), std::string("a\0b\0", 4));
  EXPECT_EQ(value.Text(), "a");

  Buffer tightKey(8);
  Buffer tightValue(3);
  type = kTypeSentinel;
  EXPECT_EQ(ReadJsonEntry(store, id, 0, tightKey.Data(), tightKey.Size(), tightValue.Data(),
                          tightValue.Size(), type),
            Status::BufferTooSmall);
  EXPECT_TRUE(tightValue.HoldsEmptyText());
}

TEST(ReadJsonEntry, NonAsciiKeyIsReadBackAsItsUtf8Bytes) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, "{\"\xC3\xA9\":1}");
  ExpectEntry(store, id, 0, {"/\xC3\xA9", "1", kNumber});
}

TEST(ReadJsonEntry, KeyBufferExactFitSucceedsAndOneByteShortIsBufferTooSmall) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"abc":"v"})");

  const EntryRead exact = ReadEntryWith(store, id, 0, 5, 8);
  EXPECT_EQ(exact.status, Status::Ok);
  EXPECT_EQ(exact.key, "/abc");

  Buffer key(4);
  Buffer value(8);
  std::int32_t type = kTypeSentinel;
  EXPECT_EQ(ReadJsonEntry(store, id, 0, key.Data(), key.Size(), value.Data(), value.Size(), type),
            Status::BufferTooSmall);
  EXPECT_EQ(type, 0);
  EXPECT_TRUE(key.HoldsEmptyText());
  EXPECT_TRUE(value.HoldsEmptyText());
}

TEST(ReadJsonEntry, ValueBufferExactFitSucceedsAndOneByteShortIsBufferTooSmall) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"k":"hello"})");

  const EntryRead exact = ReadEntryWith(store, id, 0, 8, 6);
  EXPECT_EQ(exact.status, Status::Ok);
  EXPECT_EQ(exact.value, "hello");

  Buffer key(8);
  Buffer value(5);
  std::int32_t type = kTypeSentinel;
  EXPECT_EQ(ReadJsonEntry(store, id, 0, key.Data(), key.Size(), value.Data(), value.Size(), type),
            Status::BufferTooSmall);
  EXPECT_EQ(type, 0);
  EXPECT_TRUE(key.HoldsEmptyText());
  EXPECT_TRUE(value.HoldsEmptyText());
}

TEST(ReadJsonEntry, TruncationIsAllOrNothingEvenForTheBufferThatWouldHaveFitted) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"key":"value"})");

  {
    Buffer key(64);
    Buffer value(2);
    std::int32_t type = kTypeSentinel;
    EXPECT_EQ(ReadJsonEntry(store, id, 0, key.Data(), key.Size(), value.Data(), value.Size(), type),
              Status::BufferTooSmall);
    EXPECT_EQ(key.Text(), "");
    EXPECT_EQ(value.Text(), "");
    EXPECT_EQ(type, 0);
  }
  {
    Buffer key(2);
    Buffer value(64);
    std::int32_t type = kTypeSentinel;
    EXPECT_EQ(ReadJsonEntry(store, id, 0, key.Data(), key.Size(), value.Data(), value.Size(), type),
              Status::BufferTooSmall);
    EXPECT_EQ(key.Text(), "");
    EXPECT_EQ(value.Text(), "");
    EXPECT_EQ(type, 0);
  }
}

TEST(ReadJsonEntry, RetryWithLargerBuffersOnTheSameDocumentSucceedsWithCorrectData) {
  JsonDocumentStore store;
  const std::string longValue(300, 'q');
  const std::uint32_t id = MustParse(store, R"({"a-fairly-long-key-name":")" + longValue + R"("})");

  std::size_t size = 4;
  EntryRead read = ReadEntryWith(store, id, 0, size, size);
  int attempts = 0;
  while (read.status == Status::BufferTooSmall && attempts < 16) {
    EXPECT_EQ(read.key, "");
    EXPECT_EQ(read.value, "");
    EXPECT_EQ(read.type, 0);
    size *= 2;
    read = ReadEntryWith(store, id, 0, size, size);
    ++attempts;
  }
  EXPECT_EQ(read.status, Status::Ok);
  EXPECT_EQ(read.key, "/a-fairly-long-key-name");
  EXPECT_EQ(read.value, longValue);
  EXPECT_EQ(read.type, kString);
  EXPECT_GT(attempts, 0);
}

TEST(ReadJsonEntry, BufferSizesAreCountedInBytesNotCharacters) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, "{\"k\":\"\xC3\xA9\"}");
  EXPECT_EQ(ReadEntryWith(store, id, 0, 8, 2).status, Status::BufferTooSmall);
  const EntryRead fitted = ReadEntryWith(store, id, 0, 8, 3);
  EXPECT_EQ(fitted.status, Status::Ok);
  EXPECT_EQ(fitted.value, "\xC3\xA9");
}

// ---------------------------------------------------------------------------
// ReadJsonValue: hits.
// ---------------------------------------------------------------------------

TEST(ReadJsonValue, ScalarNullAndEmptyContainerLeavesAreReadByPath) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, kRichDocument);
  ExpectValue(store, id, "/s", "text", kString);
  ExpectValue(store, id, "/n", "-12", kNumber);
  ExpectValue(store, id, "/f", "2.5", kNumber);
  ExpectValue(store, id, "/t", "true", kBool);
  ExpectValue(store, id, "/z", "null", kNull);
  ExpectValue(store, id, "/e", "", kString);
  ExpectValue(store, id, "/o", "{}", kEmptyObject);
  ExpectValue(store, id, "/r", "[]", kEmptyArray);
  ExpectValue(store, id, "/a/0", "1", kNumber);
  ExpectValue(store, id, "/a/1/k", "v", kString);
}

TEST(ReadJsonValue, EveryKeyFromReadEntryResolvesToTheSameValueAndType) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(
      store, R"({"a/b":1,"m~n":"x","":{"0":[true,null]},"q":{"":{}},"u":")"
             "\xC3\xA9"
             R"(","big":[[],{}]})");
  const std::uint32_t count = CountOf(store, id);
  ASSERT_GT(count, 0u);
  for (std::uint32_t i = 0; i < count; ++i) {
    const EntryRead entry = ReadEntryWith(store, id, i);
    ASSERT_EQ(entry.status, Status::Ok);
    SCOPED_TRACE("key '" + entry.key + "'");
    const ValueRead value = ReadValueAt(store, id, entry.key);
    EXPECT_EQ(value.status, Status::Ok);
    EXPECT_EQ(value.value, entry.value);
    EXPECT_EQ(value.type, entry.type);
  }
}

TEST(ReadJsonValue, BooleanFalseIsReadAsBoolWithTheTextFalse) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"on":true,"off":false})");
  ExpectValue(store, id, "/on", "true", kBool);
  ExpectValue(store, id, "/off", "false", kBool);
}

TEST(ReadJsonValue, NonAsciiPathAddressesTheNonAsciiKey) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, "{\"\xC3\xA9\":1,\"e\":2}");
  ExpectValue(store, id, "/\xC3\xA9", "1", kNumber);
  ExpectValue(store, id, "/e", "2", kNumber);
  ExpectValueRejected(store, id, "/\xC3\xA8", Status::PathNotFound);
  ExpectValueRejected(store, id, "/\xFF", Status::PathNotFound);
}

TEST(ReadJsonValue, NulInsideAStringValueIsCopiedWithItsFullLength) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"k":"a\u0000b"})");

  Buffer value(4);
  std::int32_t type = kTypeSentinel;
  ASSERT_EQ(ReadJsonValue(store, id, "/k", 3, value.Data(), value.Size(), type), Status::Ok);
  EXPECT_EQ(type, kString);
  EXPECT_EQ(value.Raw(4), std::string("a\0b\0", 4));
  EXPECT_EQ(value.Text(), "a");

  EXPECT_EQ(ReadValueAt(store, id, "/k", 3).status, Status::BufferTooSmall);
}

TEST(ReadJsonValue, EscapedAndEmptyTokensAreAddressable) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"a/b":1,"m~n":2,"x":{"":3},"":{"0":4},"~1":5})");
  ExpectValue(store, id, "/a~1b", "1", kNumber);
  ExpectValue(store, id, "/m~0n", "2", kNumber);
  ExpectValue(store, id, "/x/", "3", kNumber);
  ExpectValue(store, id, "//0", "4", kNumber);
  ExpectValue(store, id, "/~01", "5", kNumber);
}

TEST(ReadJsonValue, EmptyPathIsTheWholeDocumentForScalarAndEmptyContainerRoots) {
  JsonDocumentStore store;
  ExpectValue(store, MustParse(store, "42"), "", "42", kNumber);
  ExpectValue(store, MustParse(store, R"("text")"), "", "text", kString);
  ExpectValue(store, MustParse(store, "null"), "", "null", kNull);
  ExpectValue(store, MustParse(store, "true"), "", "true", kBool);
  ExpectValue(store, MustParse(store, "{}"), "", "{}", kEmptyObject);
  ExpectValue(store, MustParse(store, "[]"), "", "[]", kEmptyArray);
}

TEST(ReadJsonValue, EmptyPathOnANonEmptyContainerRootIsTypeMismatch) {
  JsonDocumentStore store;
  ExpectValueRejected(store, MustParse(store, R"({"a":1})"), "", Status::TypeMismatch);
  ExpectValueRejected(store, MustParse(store, "[1]"), "", Status::TypeMismatch);
}

TEST(ReadJsonValue, SingleSlashAddressesTheEmptyKeyAtTheRoot) {
  JsonDocumentStore store;
  ExpectValue(store, MustParse(store, R"({"":"empty-key"})"), "/", "empty-key", kString);
  ExpectValueRejected(store, MustParse(store, R"({"a":1})"), "/", Status::PathNotFound);
}

TEST(ReadJsonValue, DuplicateKeyResolvesToTheLastValue) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"a":{"x":1},"b":0,"a":{"y":2}})");
  ExpectValue(store, id, "/a/y", "2", kNumber);
  ExpectValueRejected(store, id, "/a/x", Status::PathNotFound);
}

TEST(ReadJsonValue, SuccessOverwritesAPresetValueType) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"k":null})");
  Buffer value(8);
  std::int32_t type = kTypeSentinel;
  ASSERT_EQ(ReadJsonValue(store, id, "/k", 3, value.Data(), value.Size(), type), Status::Ok);
  EXPECT_EQ(type, kNull);
}

// ---------------------------------------------------------------------------
// ReadJsonValue: path errors and non-leaf nodes.
// ---------------------------------------------------------------------------

TEST(ReadJsonValue, NonEmptyObjectOrArrayIsTypeMismatch) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"o":{"k":1},"a":[1],"n":{"x":{}}})");
  ExpectValueRejected(store, id, "/o", Status::TypeMismatch);
  ExpectValueRejected(store, id, "/a", Status::TypeMismatch);
  ExpectValueRejected(store, id, "/n", Status::TypeMismatch);
  ExpectValue(store, id, "/n/x", "{}", kEmptyObject);
}

TEST(ReadJsonValue, PathSyntaxErrorsArePathSyntaxError) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"a":[1,2],"o":{"k":1},"s":"str","z":null})");
  const char* const paths[] = {"a", "data.items[0].name", "a/b", "[0]", "/a~2", "/o~", "~0", "~1"};
  for (const char* path : paths) {
    ExpectValueRejected(store, id, path, Status::PathSyntaxError);
  }
}

TEST(ReadJsonValue, AbsentObjectKeyIsPathNotFound) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"a":[1,2],"o":{"k":1}})");
  ExpectValueRejected(store, id, "/missing", Status::PathNotFound);
  ExpectValueRejected(store, id, "/o/missing", Status::PathNotFound);
  ExpectValueRejected(store, id, "/o/0", Status::PathNotFound);
}

TEST(ReadJsonValue, ArrayIndexOutOfRangeDashAndHugeNumbersAreIndexOutOfRange) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"a":[1,2]})");
  ExpectValueRejected(store, id, "/a/2", Status::IndexOutOfRange);
  ExpectValueRejected(store, id, "/a/5", Status::IndexOutOfRange);
  ExpectValueRejected(store, id, "/a/-", Status::IndexOutOfRange);
  ExpectValueRejected(store, id, "/a/4294967295", Status::IndexOutOfRange);
  ExpectValueRejected(store, id, "/a/4294967296", Status::IndexOutOfRange);
  ExpectValue(store, id, "/a/1", "2", kNumber);
}

TEST(ReadJsonValue, NonCanonicalArrayTokensAndScalarsMetMidPathAreTypeMismatch) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"a":[1,2],"s":"str","z":null,"n":5})");
  const char* const paths[] = {"/a/01", "/a/x", "/a/", "/a/-1", "/a/+1", "/s/x", "/s/0",
                               "/z/x",  "/n/x", "/n/"};
  for (const char* path : paths) {
    ExpectValueRejected(store, id, path, Status::TypeMismatch);
  }
}

TEST(ReadJsonValue, NumericObjectKeyIsAKeyNotAnIndex) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"0":"zero","a":["x"]})");
  ExpectValue(store, id, "/0", "zero", kString);
  ExpectValue(store, id, "/a/0", "x", kString);
}

TEST(ReadJsonValue, OldDotAndBracketSyntaxIsRejected) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"data":{"items":[{"name":"n"}]}})");
  ExpectValueRejected(store, id, "data.items[0].name", Status::PathSyntaxError);
  ExpectValue(store, id, "/data/items/0/name", "n", kString);
}

// ---------------------------------------------------------------------------
// ReadJsonValue: input bounds and failure precedence.
// ---------------------------------------------------------------------------

TEST(ReadJsonValue, NullPathIsInvalidArgument) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"a":1})");
  Buffer value(8);
  std::int32_t type = kTypeSentinel;
  EXPECT_EQ(ReadJsonValue(store, id, nullptr, 3, value.Data(), value.Size(), type),
            Status::InvalidArgument);
  EXPECT_EQ(type, 0);
  EXPECT_TRUE(value.Untouched());
}

TEST(ReadJsonValue, ZeroPathSizeIsInvalidArgument) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"a":1})");
  Buffer value(8);
  std::int32_t type = kTypeSentinel;
  EXPECT_EQ(ReadJsonValue(store, id, "/a", 0, value.Data(), value.Size(), type),
            Status::InvalidArgument);
  EXPECT_EQ(type, 0);
  EXPECT_TRUE(value.Untouched());
}

TEST(ReadJsonValue, PathWithoutANulInsideTheSizeIsUnterminatedInputText) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"a":1})");
  const std::array<char, 3> noNul = {'/', 'a', 'b'};
  Buffer value(8);
  std::int32_t type = kTypeSentinel;
  EXPECT_EQ(ReadJsonValue(store, id, noNul.data(), static_cast<std::uint32_t>(noNul.size()),
                          value.Data(), value.Size(), type),
            Status::UnterminatedInputText);
  EXPECT_EQ(type, 0);
  EXPECT_TRUE(value.Untouched());
}

TEST(ReadJsonValue, PathSizeExactlyCoveringTheNulIsAcceptedAndOneShortIsNot) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"a":1})");
  Buffer value(8);
  std::int32_t type = kTypeSentinel;

  EXPECT_EQ(ReadJsonValue(store, id, "/a", 2, value.Data(), value.Size(), type),
            Status::UnterminatedInputText);
  EXPECT_EQ(type, 0);

  type = kTypeSentinel;
  EXPECT_EQ(ReadJsonValue(store, id, "/a", 3, value.Data(), value.Size(), type), Status::Ok);
  EXPECT_EQ(type, kNumber);
  EXPECT_EQ(value.Text(), "1");
}

TEST(ReadJsonValue, PathSizeLargerThanTheTextUsesOnlyTheBytesBeforeTheNul) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"a":1,"b":2})");
  const char path[] = "/a\0/b";
  Buffer value(8);
  std::int32_t type = kTypeSentinel;
  EXPECT_EQ(ReadJsonValue(store, id, path, sizeof(path), value.Data(), value.Size(), type),
            Status::Ok);
  EXPECT_EQ(value.Text(), "1");
  EXPECT_EQ(type, kNumber);
}

TEST(ReadJsonValue, PathInputIsCheckedBeforeTheDocumentId) {
  JsonDocumentStore store;
  Buffer value(8);
  std::int32_t type = kTypeSentinel;
  EXPECT_EQ(ReadJsonValue(store, 0, nullptr, 0, value.Data(), value.Size(), type),
            Status::InvalidArgument);
  EXPECT_EQ(type, 0);

  type = kTypeSentinel;
  const std::array<char, 2> noNul = {'/', 'a'};
  EXPECT_EQ(ReadJsonValue(store, 12345, noNul.data(), 2, value.Data(), value.Size(), type),
            Status::UnterminatedInputText);
  EXPECT_EQ(type, 0);
  EXPECT_TRUE(value.Untouched());
}

TEST(ReadJsonValue, DocumentIdIsCheckedBeforeThePathIsResolved) {
  JsonDocumentStore store;
  ExpectValueRejected(store, 12345, "not-a-pointer", Status::UnknownDocumentId);
  ExpectValueRejected(store, 12345, "/a", Status::UnknownDocumentId);
}

TEST(ReadJsonValue, PathResolutionIsCheckedBeforeTheValueBuffer) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"o":{"k":1}})");
  std::int32_t type = kTypeSentinel;
  EXPECT_EQ(ReadJsonValue(store, id, "bad", 4, nullptr, 0, type), Status::PathSyntaxError);
  EXPECT_EQ(type, 0);
  type = kTypeSentinel;
  EXPECT_EQ(ReadJsonValue(store, id, "/missing", 9, nullptr, 0, type), Status::PathNotFound);
  EXPECT_EQ(type, 0);
  type = kTypeSentinel;
  EXPECT_EQ(ReadJsonValue(store, id, "/o", 3, nullptr, 0, type), Status::TypeMismatch);
  EXPECT_EQ(type, 0);
}

TEST(ReadJsonValue, NullOrZeroSizeValueBufferIsInvalidArgument) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"a":1})");
  std::int32_t type = kTypeSentinel;
  EXPECT_EQ(ReadJsonValue(store, id, "/a", 3, nullptr, 8, type), Status::InvalidArgument);
  EXPECT_EQ(type, 0);

  Buffer value(8);
  type = kTypeSentinel;
  EXPECT_EQ(ReadJsonValue(store, id, "/a", 3, value.Data(), 0, type), Status::InvalidArgument);
  EXPECT_EQ(type, 0);
  EXPECT_TRUE(value.Untouched());
}

// ---------------------------------------------------------------------------
// ReadJsonValue: buffer sizes and retry.
// ---------------------------------------------------------------------------

TEST(ReadJsonValue, ValueBufferExactFitSucceedsAndOneByteShortIsBufferTooSmall) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"k":"hello"})");

  const ValueRead exact = ReadValueAt(store, id, "/k", 6);
  EXPECT_EQ(exact.status, Status::Ok);
  EXPECT_EQ(exact.value, "hello");
  EXPECT_EQ(exact.type, kString);

  Buffer value(5);
  std::int32_t type = kTypeSentinel;
  EXPECT_EQ(ReadJsonValue(store, id, "/k", 3, value.Data(), value.Size(), type),
            Status::BufferTooSmall);
  EXPECT_EQ(type, 0);
  EXPECT_TRUE(value.HoldsEmptyText());
}

TEST(ReadJsonValue, EmptyValueFitsAOneByteBuffer) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"e":""})");
  const ValueRead read = ReadValueAt(store, id, "/e", 1);
  EXPECT_EQ(read.status, Status::Ok);
  EXPECT_EQ(read.value, "");
  EXPECT_EQ(read.type, kString);
}

TEST(ReadJsonValue, EmptyContainerTextNeedsThreeBytes) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"o":{},"r":[]})");
  EXPECT_EQ(ReadValueAt(store, id, "/o", 2).status, Status::BufferTooSmall);
  EXPECT_EQ(ReadValueAt(store, id, "/r", 2).status, Status::BufferTooSmall);
  EXPECT_EQ(ReadValueAt(store, id, "/o", 3).value, "{}");
  EXPECT_EQ(ReadValueAt(store, id, "/r", 3).value, "[]");
}

TEST(ReadJsonValue, RetryWithALargerBufferOnTheSameDocumentSucceedsWithCorrectData) {
  JsonDocumentStore store;
  const std::string longValue(500, 'w');
  const std::uint32_t id = MustParse(store, R"({"k":")" + longValue + R"("})");

  std::size_t size = 2;
  ValueRead read = ReadValueAt(store, id, "/k", size);
  int attempts = 0;
  while (read.status == Status::BufferTooSmall && attempts < 16) {
    EXPECT_EQ(read.value, "");
    EXPECT_EQ(read.type, 0);
    size *= 2;
    read = ReadValueAt(store, id, "/k", size);
    ++attempts;
  }
  EXPECT_EQ(read.status, Status::Ok);
  EXPECT_EQ(read.value, longValue);
  EXPECT_EQ(read.type, kString);
  EXPECT_GT(attempts, 0);
}

TEST(ReadJsonValue, ValueBufferSizeIsCountedInBytesNotCharacters) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, "{\"k\":\"\xE2\x82\xAC\"}");
  EXPECT_EQ(ReadValueAt(store, id, "/k", 3).status, Status::BufferTooSmall);
  const ValueRead fitted = ReadValueAt(store, id, "/k", 4);
  EXPECT_EQ(fitted.status, Status::Ok);
  EXPECT_EQ(fitted.value, "\xE2\x82\xAC");
}

// ---------------------------------------------------------------------------
// DiscardJsonDocument.
// ---------------------------------------------------------------------------

TEST(DiscardJsonDocument, ValidIdIsDiscardedOnceAndThenUnknown) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, "[1]");
  EXPECT_EQ(DiscardJsonDocument(store, id), Status::Ok);
  EXPECT_EQ(DiscardJsonDocument(store, id), Status::UnknownDocumentId);
  ExpectUnknownEverywhere(store, id);
}

TEST(DiscardJsonDocument, ZeroAndNeverIssuedIdsAreUnknownAndDisturbNothing) {
  JsonDocumentStore store(500);
  const std::uint32_t id = MustParse(store, "[1]");
  EXPECT_EQ(DiscardJsonDocument(store, 0), Status::UnknownDocumentId);
  EXPECT_EQ(DiscardJsonDocument(store, 7), Status::UnknownDocumentId);
  EXPECT_EQ(CountOf(store, id), 1u);
}

TEST(DiscardJsonDocument, DiscardingOneDocumentLeavesTheOthersReadable) {
  JsonDocumentStore store;
  const std::uint32_t first = MustParse(store, R"({"v":"first"})");
  const std::uint32_t second = MustParse(store, R"({"v":"second"})");
  ASSERT_EQ(DiscardJsonDocument(store, first), Status::Ok);
  ExpectValue(store, second, "/v", "second", kString);
  EXPECT_EQ(CountOf(store, second), 1u);
}

TEST(DiscardJsonDocument, FreedSlotIsUsableAgainUntilThePoolIsFull) {
  JsonDocumentStore store;
  const std::vector<std::uint32_t> ids = FillPool(store);
  for (std::size_t round = 0; round < 3; ++round) {
    ASSERT_EQ(DiscardJsonDocument(store, ids[round]), Status::Ok);
    MustParse(store, "[1]");
    ExpectParseFails(store, "[2]", Status::NoFreeDocumentSlot);
  }
}

// ---------------------------------------------------------------------------
// DiscardAllJsonDocuments.
// ---------------------------------------------------------------------------

TEST(DiscardAllJsonDocuments, EmptyStoreReportsZeroAndOverwritesAPresetCount) {
  JsonDocumentStore store;
  std::uint32_t discarded = kCountSentinel;
  EXPECT_EQ(DiscardAllJsonDocuments(store, discarded), Status::Ok);
  EXPECT_EQ(discarded, 0u);
}

TEST(DiscardAllJsonDocuments, ReportsHowManyDocumentsWereFreedAndTheyBecomeUnknown) {
  JsonDocumentStore store;
  const std::uint32_t a = MustParse(store, "[1]");
  const std::uint32_t b = MustParse(store, "[2]");
  const std::uint32_t c = MustParse(store, "[3]");

  std::uint32_t discarded = kCountSentinel;
  EXPECT_EQ(DiscardAllJsonDocuments(store, discarded), Status::Ok);
  EXPECT_EQ(discarded, 3u);
  ExpectUnknownEverywhere(store, a);
  ExpectUnknownEverywhere(store, b);
  ExpectUnknownEverywhere(store, c);
}

TEST(DiscardAllJsonDocuments, FullPoolIsReportedAsEightAndFreedCompletely) {
  JsonDocumentStore store;
  const std::vector<std::uint32_t> oldIds = FillPool(store);

  std::uint32_t discarded = kCountSentinel;
  EXPECT_EQ(DiscardAllJsonDocuments(store, discarded), Status::Ok);
  EXPECT_EQ(discarded, 8u);

  const std::vector<std::uint32_t> newIds = FillPool(store);
  for (const std::uint32_t id : newIds) {
    EXPECT_EQ(std::count(oldIds.begin(), oldIds.end(), id), 0);
  }
}

TEST(DiscardAllJsonDocuments, CountsOnlyTheDocumentsStillHeld) {
  JsonDocumentStore store;
  const std::vector<std::uint32_t> ids = FillPool(store);
  ASSERT_EQ(DiscardJsonDocument(store, ids[1]), Status::Ok);
  ASSERT_EQ(DiscardJsonDocument(store, ids[6]), Status::Ok);

  std::uint32_t discarded = kCountSentinel;
  EXPECT_EQ(DiscardAllJsonDocuments(store, discarded), Status::Ok);
  EXPECT_EQ(discarded, 6u);
}

TEST(DiscardAllJsonDocuments, SecondCallInARowReportsZero) {
  JsonDocumentStore store;
  MustParse(store, "[1]");
  std::uint32_t discarded = 0;
  ASSERT_EQ(DiscardAllJsonDocuments(store, discarded), Status::Ok);
  ASSERT_EQ(discarded, 1u);

  discarded = kCountSentinel;
  EXPECT_EQ(DiscardAllJsonDocuments(store, discarded), Status::Ok);
  EXPECT_EQ(discarded, 0u);
}

// ---------------------------------------------------------------------------
// Independent documents and the process-wide store.
// ---------------------------------------------------------------------------

TEST(JsonDocuments, TwoDocumentsDoNotInterfere) {
  JsonDocumentStore store;
  const std::uint32_t a = MustParse(store, R"({"a":1,"b":2})");
  const std::uint32_t b = MustParse(store, R"(["x"])");

  EXPECT_EQ(CountOf(store, a), 2u);
  EXPECT_EQ(CountOf(store, b), 1u);
  ExpectEntry(store, a, 1, {"/b", "2", kNumber});
  ExpectEntry(store, b, 0, {"/0", "x", kString});
  EXPECT_EQ(ReadEntryWith(store, b, 1).status, Status::IndexOutOfRange);
  ExpectValue(store, a, "/a", "1", kNumber);
  ExpectValueRejected(store, b, "/a", Status::TypeMismatch);
}

TEST(JsonDocuments, DiscardingOneLeavesTheOtherFullyReadable) {
  JsonDocumentStore store;
  const std::uint32_t a = MustParse(store, R"({"a":1})");
  const std::uint32_t b = MustParse(store, kRichDocument);
  ASSERT_EQ(DiscardJsonDocument(store, a), Status::Ok);

  const std::vector<ExpectedEntry>& expected = RichEntries();
  ASSERT_EQ(CountOf(store, b), expected.size());
  for (std::size_t i = 0; i < expected.size(); ++i) {
    ExpectEntry(store, b, static_cast<std::uint32_t>(i), expected[i]);
  }
  ExpectUnknownEverywhere(store, a);
}

TEST(JsonDocuments, FailedReadsOnOneDocumentDoNotAffectAnother) {
  JsonDocumentStore store;
  const std::uint32_t a = MustParse(store, R"({"k":"a-value"})");
  const std::uint32_t b = MustParse(store, R"({"k":"b-value"})");
  EXPECT_EQ(ReadEntryWith(store, a, 0, 2, 2).status, Status::BufferTooSmall);
  EXPECT_EQ(ReadEntryWith(store, a, 5).status, Status::IndexOutOfRange);
  ExpectValue(store, b, "/k", "b-value", kString);
  ExpectValue(store, a, "/k", "a-value", kString);
}

TEST(ProcessWideJsonStore, TextLayerWorksAgainstTheDefaultStore) {
  JsonDocumentStore& store = DefaultJsonDocumentStore();

  std::uint32_t discardedBefore = kCountSentinel;
  ASSERT_EQ(DiscardAllJsonDocuments(store, discardedBefore), Status::Ok);
  EXPECT_LE(discardedBefore, kJsonDocumentSlotCount);

  const std::uint32_t id = MustParse(store, R"({"k":[1,2]})");
  EXPECT_EQ(CountOf(store, id), 2u);
  ExpectEntry(store, id, 1, {"/k/1", "2", kNumber});
  ExpectValue(store, id, "/k/0", "1", kNumber);
  EXPECT_EQ(DiscardJsonDocument(store, id), Status::Ok);
  EXPECT_EQ(DiscardJsonDocument(store, id), Status::UnknownDocumentId);

  std::uint32_t discardedAfter = kCountSentinel;
  EXPECT_EQ(DiscardAllJsonDocuments(store, discardedAfter), Status::Ok);
  EXPECT_EQ(discardedAfter, 0u);
}

// ---------------------------------------------------------------------------
// InternalError: allocation failure inside the DLL (Count, ReadEntry, Discard and
// DiscardAll allocate nothing, so only Parse and ReadValue can be driven here).
// ---------------------------------------------------------------------------

TEST(TextApiInternalError, ParseReportsInternalErrorWhenAllocationFailsAndConsumesNothing) {
  JsonDocumentStore store;
  const std::string json = R"({"a":[1,2,3],"b":{"c":"text"}})";
  std::uint32_t id = kIdSentinel;
  Status status = Status::Ok;
  int failedAllocations = 0;
  {
    FailAllocations failAllocations;
    status = ParseJsonDocument(store, json.c_str(), static_cast<std::uint32_t>(json.size() + 1), id);
    failedAllocations = t_failedAllocations;
  }
  EXPECT_EQ(status, Status::InternalError);
  EXPECT_GT(failedAllocations, 0);
  EXPECT_EQ(id, 0u);

  EXPECT_EQ(FillPool(store).size(), kJsonDocumentSlotCount);
}

TEST(TextApiInternalError, ReadValueReportsInternalErrorWhenAllocationFailsAndStaysUsable) {
  JsonDocumentStore store;
  const std::uint32_t id = MustParse(store, R"({"k":"v"})");
  Buffer value(8);
  std::int32_t type = kTypeSentinel;
  Status status = Status::Ok;
  int failedAllocations = 0;
  {
    FailAllocations failAllocations;
    status = ReadJsonValue(store, id, "/k", 3, value.Data(), value.Size(), type);
    failedAllocations = t_failedAllocations;
  }
  EXPECT_EQ(status, Status::InternalError);
  EXPECT_GT(failedAllocations, 0);
  EXPECT_EQ(type, 0);
  EXPECT_TRUE(value.Untouched());

  ExpectValue(store, id, "/k", "v", kString);
  EXPECT_EQ(CountOf(store, id), 1u);
}

// ---------------------------------------------------------------------------
// Concurrency: one store shared by several threads.
// ---------------------------------------------------------------------------

TEST(JsonTextApiConcurrency, ParallelParseCountReadAndDiscardStayConsistent) {
  constexpr int kThreads = 6;
  constexpr int kIterations = 200;

  JsonDocumentStore store;
  std::atomic<std::uint32_t> published{0};
  std::atomic<int> badStatuses{0};
  std::atomic<int> badValues{0};
  std::atomic<int> firstBadStatus{0};

  const auto noteBadStatus = [&](Status status) {
    int expected = 0;
    firstBadStatus.compare_exchange_strong(expected, static_cast<int>(status));
    ++badStatuses;
  };

  const auto worker = [&](int threadIndex) {
    const std::string json = "{\"t\":" + std::to_string(threadIndex) + ",\"list\":[1,2]}";
    for (int iteration = 0; iteration < kIterations; ++iteration) {
      std::uint32_t ids[2] = {0, 0};
      for (std::uint32_t& id : ids) {
        const Status status = ParseText(store, json, id);
        if (status == Status::Ok) {
          if (id == 0) {
            ++badValues;
          }
          published.store(id);
        } else if (status != Status::NoFreeDocumentSlot || id != 0) {
          noteBadStatus(status);
        }
      }

      for (const std::uint32_t id : ids) {
        if (id == 0) {
          continue;
        }
        std::uint32_t count = 0;
        const Status countStatus = CountJsonEntries(store, id, count);
        if (countStatus != Status::Ok) {
          noteBadStatus(countStatus);
        } else if (count != 3) {
          ++badValues;
        }

        char key[32] = {};
        char value[32] = {};
        std::int32_t type = 0;
        const Status entryStatus = ReadJsonEntry(store, id, 2, key, sizeof(key), value,
                                                 sizeof(value), type);
        if (entryStatus != Status::Ok) {
          noteBadStatus(entryStatus);
        } else if (std::string(key) != "/list/1" || std::string(value) != "2" ||
                   type != kNumber) {
          ++badValues;
        }
      }

      const std::uint32_t foreign = published.load();
      if (foreign != 0) {
        std::uint32_t count = 0;
        const Status status = CountJsonEntries(store, foreign, count);
        if (status == Status::Ok) {
          if (count != 3) {
            ++badValues;
          }
        } else if (status != Status::UnknownDocumentId) {
          noteBadStatus(status);
        }
      }

      for (const std::uint32_t id : ids) {
        if (id != 0) {
          const Status status = DiscardJsonDocument(store, id);
          if (status != Status::Ok) {
            noteBadStatus(status);
          }
        }
      }
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < kThreads; ++i) {
    threads.emplace_back(worker, i);
  }
  for (std::thread& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(badStatuses.load(), 0) << "first unexpected status: "
                                   << StatusName(static_cast<Status>(firstBadStatus.load()));
  EXPECT_EQ(badValues.load(), 0);

  std::uint32_t leftover = kCountSentinel;
  EXPECT_EQ(DiscardAllJsonDocuments(store, leftover), Status::Ok);
  EXPECT_EQ(leftover, 0u);
}

// ---------------------------------------------------------------------------
// NormalizeJsonText: apostrophe strings to standard JSON, no store involved.
// ---------------------------------------------------------------------------

namespace {

Status NormalizeText(const std::string& json, Buffer& out) {
  return NormalizeJsonText(json.c_str(), static_cast<std::uint32_t>(json.size() + 1), out.Data(),
                           out.Size());
}

void ExpectNormalizes(const std::string& json, const std::string& expected) {
  SCOPED_TRACE(json.substr(0, 80));
  Buffer out(expected.size() + 1);
  EXPECT_EQ(NormalizeText(json, out), Status::Ok);
  EXPECT_EQ(out.Text(), expected);
}

void ExpectNormalizeFails(const std::string& json, Status expected, std::size_t bufferSize = 64) {
  SCOPED_TRACE(json.substr(0, 80));
  Buffer out(bufferSize);
  EXPECT_EQ(NormalizeText(json, out), expected);
  EXPECT_TRUE(out.HoldsEmptyText());
}

std::string PaddedWithSpaces(std::string text, std::size_t size) {
  EXPECT_LE(text.size(), size);
  text.resize(size, ' ');
  return text;
}

}  // namespace

TEST(NormalizeJsonText, ApostropheObjectBecomesStandardJson) {
  ExpectNormalizes("{'a':'b'}", R"({"a":"b"})");
  ExpectNormalizes("{'a':{'b':['c',1,true,null]},'d':''}", R"({"a":{"b":["c",1,true,null]},"d":""})");
  ExpectNormalizes("'text'", R"("text")");
}

TEST(NormalizeJsonText, FormsAAndBGiveTheSameNormalizedText) {
  const std::string expected = R"({"name":"restify","note":"it's ok"})";
  ExpectNormalizes(R"({'name':'restify','note':"it's ok"})", expected);
  const std::string formB = "{'name':'restify','note':'it\\'s ok'}";
  ASSERT_NE(formB.find("\\'"), std::string::npos);
  ExpectNormalizes(formB, expected);
}

TEST(NormalizeJsonText, NormalizedTextParsesToTheSameValueAsTheOriginal) {
  JsonDocumentStore store;
  Buffer out(64);
  ASSERT_EQ(NormalizeText("{'note':'it\\'s ok'}", out), Status::Ok);
  const std::uint32_t id = MustParse(store, out.Text());
  ExpectValue(store, id, "/note", "it's ok", kString);
}

TEST(NormalizeJsonText, JsonUnicodeEscapeInsideAnApostropheStringIsKeptUnchanged) {
  const std::string json = "['\\u0041']";
  ASSERT_NE(json.find("\\u0041"), std::string::npos);
  ExpectNormalizes(json, "[\"\\u0041\"]");
}

TEST(NormalizeJsonText, OutputCanBeLongerThanTheInput) {
  const std::string json = "['\"\"']";
  const std::string expected = "[\"\\\"\\\"\"]";
  ASSERT_GT(expected.size(), json.size());
  Buffer tooSmall(json.size() + 1);
  EXPECT_EQ(NormalizeText(json, tooSmall), Status::BufferTooSmall);
  EXPECT_TRUE(tooSmall.HoldsEmptyText());
  ExpectNormalizes(json, expected);
}

TEST(NormalizeJsonText, ValidDoubleQuotedInputIsReturnedByteIdentical) {
  const char* const documents[] = {
      R"({"a":[1,2.5,-3e2,true,false,null,"s"],"b":{"c":""}})",
      R"({"note":"it's ok","k's":"'quoted'"})",
      R"("it's")",
      " \t{ \"a\" :\r\n [ 1 , 2 ] } ",
      R"({"line":"a\nb \"q\" \\ \/ \u0041"})",
      "{\"s\":\"\xC3\xA9\xE6\x97\xA5\"}",
      "42",
      "true",
      "null",
  };
  for (const char* document : documents) {
    ExpectNormalizes(document, document);
  }
}

TEST(NormalizeJsonText, NormalizingTheOutputAgainChangesNothing) {
  Buffer first(64);
  ASSERT_EQ(NormalizeText("{'a':'it\\'s \"x\"','b':\"it's\"}", first), Status::Ok);
  Buffer second(64);
  ASSERT_EQ(NormalizeText(first.Text(), second), Status::Ok);
  EXPECT_EQ(second.Text(), first.Text());
}

TEST(NormalizeJsonText, InputSizeLargerThanTheTextUsesOnlyTheBytesBeforeTheNul) {
  const char json[] = "{'a':1}\0{'b':2}";
  Buffer out(32);
  EXPECT_EQ(NormalizeJsonText(json, sizeof(json), out.Data(), out.Size()), Status::Ok);
  EXPECT_EQ(out.Text(), R"({"a":1})");
}

TEST(NormalizeJsonText, SizeExactlyCoveringTheNulIsAcceptedAndOneShortIsNot) {
  const char json[] = "['a']";
  Buffer out(16);
  EXPECT_EQ(NormalizeJsonText(json, 5, out.Data(), out.Size()), Status::UnterminatedInputText);
  EXPECT_TRUE(out.HoldsEmptyText());
  EXPECT_EQ(NormalizeJsonText(json, 6, out.Data(), out.Size()), Status::Ok);
  EXPECT_EQ(out.Text(), R"(["a"])");
}

TEST(NormalizeJsonText, NullJsonIsInvalidArgumentAndNothingIsWritten) {
  Buffer out(16);
  EXPECT_EQ(NormalizeJsonText(nullptr, 8, out.Data(), out.Size()), Status::InvalidArgument);
  EXPECT_TRUE(out.Untouched());
}

TEST(NormalizeJsonText, ZeroJsonSizeIsInvalidArgumentAndNothingIsWritten) {
  Buffer out(16);
  EXPECT_EQ(NormalizeJsonText("{'a':1}", 0, out.Data(), out.Size()), Status::InvalidArgument);
  EXPECT_TRUE(out.Untouched());
}

TEST(NormalizeJsonText, NullJsonAndNullNormalizedIsInvalidArgument) {
  EXPECT_EQ(NormalizeJsonText(nullptr, 0, nullptr, 0), Status::InvalidArgument);
}

TEST(NormalizeJsonText, TextWithoutANulInsideTheSizeIsUnterminatedInputTextAndNormalizedIsEmpty) {
  const std::array<char, 5> noNul = {'[', '\'', 'a', '\'', ']'};
  Buffer out(16);
  EXPECT_EQ(NormalizeJsonText(noNul.data(), static_cast<std::uint32_t>(noNul.size()), out.Data(),
                              out.Size()),
            Status::UnterminatedInputText);
  EXPECT_TRUE(out.HoldsEmptyText());
}

TEST(NormalizeJsonText, UnterminatedInputTextIsReportedEvenWithAnUnusableOutputBuffer) {
  const std::array<char, 3> noNul = {'[', '1', ']'};
  const std::uint32_t size = static_cast<std::uint32_t>(noNul.size());
  EXPECT_EQ(NormalizeJsonText(noNul.data(), size, nullptr, 0), Status::UnterminatedInputText);
  Buffer out(16);
  EXPECT_EQ(NormalizeJsonText(noNul.data(), size, out.Data(), 0), Status::UnterminatedInputText);
  EXPECT_TRUE(out.Untouched());
}

TEST(NormalizeJsonText, NullNormalizedIsInvalidArgument) {
  const std::string json = "{'a':1}";
  EXPECT_EQ(NormalizeJsonText(json.c_str(), static_cast<std::uint32_t>(json.size() + 1), nullptr,
                              16),
            Status::InvalidArgument);
}

TEST(NormalizeJsonText, ZeroNormalizedSizeIsInvalidArgumentAndNothingIsWritten) {
  const std::string json = "{'a':1}";
  Buffer out(16);
  EXPECT_EQ(NormalizeJsonText(json.c_str(), static_cast<std::uint32_t>(json.size() + 1), out.Data(),
                              0),
            Status::InvalidArgument);
  EXPECT_TRUE(out.Untouched());
}

TEST(NormalizeJsonText, UnusableOutputBufferIsReportedBeforeTheTextIsChecked) {
  const std::string tooLarge(kMaxJsonInputBytes + 1, 'a');
  const std::uint32_t size = static_cast<std::uint32_t>(tooLarge.size() + 1);
  EXPECT_EQ(NormalizeJsonText(tooLarge.c_str(), size, nullptr, 0), Status::InvalidArgument);
  const std::string invalid = "not json";
  Buffer out(16);
  EXPECT_EQ(NormalizeJsonText(invalid.c_str(), static_cast<std::uint32_t>(invalid.size() + 1),
                              out.Data(), 0),
            Status::InvalidArgument);
  EXPECT_TRUE(out.Untouched());
}

TEST(NormalizeJsonText, InputOneByteOverTheMaximumIsDocumentTooLargeAndNormalizedIsEmpty) {
  ExpectNormalizeFails(PaddedWithSpaces("['a']", kMaxJsonInputBytes + 1), Status::DocumentTooLarge);
  ExpectNormalizeFails(std::string(kMaxJsonInputBytes + 1, 'x'), Status::DocumentTooLarge);
}

TEST(NormalizeJsonText, InputOfExactlyTheMaximumSizeIsNormalized) {
  const std::string json = PaddedWithSpaces("['a']", kMaxJsonInputBytes);
  const std::string expected = PaddedWithSpaces(R"(["a"])", kMaxJsonInputBytes);
  Buffer out(kMaxJsonInputBytes + 1);
  EXPECT_EQ(NormalizeText(json, out), Status::Ok);
  EXPECT_TRUE(out.Text() == expected);
}

TEST(NormalizeJsonText, MaximumSizeIsCountedBeforeConversionEvenWhenTheOutputIsLonger) {
  constexpr std::size_t kQuotes = kMaxJsonInputBytes - 4;
  const std::string json = "['" + std::string(kQuotes, '"') + "']";
  ASSERT_EQ(json.size(), kMaxJsonInputBytes);
  Buffer out(2 * kMaxJsonInputBytes + 8);
  EXPECT_EQ(NormalizeText(json, out), Status::Ok);
  EXPECT_EQ(out.Text().size(), 2 * kQuotes + 4);
}

TEST(NormalizeJsonText, EmptyTextIsParseErrorAndNormalizedIsEmpty) {
  ExpectNormalizeFails("", Status::ParseError);
  const char leadingNul[] = "\0['a']";
  Buffer out(16);
  EXPECT_EQ(NormalizeJsonText(leadingNul, sizeof(leadingNul), out.Data(), out.Size()),
            Status::ParseError);
  EXPECT_TRUE(out.HoldsEmptyText());
}

TEST(NormalizeJsonText, TextThatIsNotJsonAfterConversionIsParseErrorAndNormalizedIsEmpty) {
  const char* const invalid[] = {
      "{'note':'it's'}",
      R"({"note":"it\'s"})",
      R"({'a':'a\\'b'})",
      "{'a':'b",
      "{'a':",
      "{'a':1,'b'}",
      "{'a' 1}",
      "not json",
      "   ",
      "[1,]",
      "{'a':1} trailing",
      "{'a':1}{'b':2}",
      "{a:1}",
  };
  for (const char* text : invalid) {
    ExpectNormalizeFails(text, Status::ParseError);
  }
}

TEST(NormalizeJsonText, InvalidUtf8AfterConversionIsParseErrorAndNormalizedIsEmpty) {
  ExpectNormalizeFails("['\xC3']", Status::ParseError);
  ExpectNormalizeFails("{\"a\":\"\xFF\"}", Status::ParseError);
}

TEST(NormalizeJsonText, OutputOneByteTooSmallIsBufferTooSmallAndARetryGivesTheExactText) {
  const std::string json = "{'a':'b'}";
  const std::string expected = R"({"a":"b"})";
  Buffer tooSmall(expected.size());
  EXPECT_EQ(NormalizeText(json, tooSmall), Status::BufferTooSmall);
  EXPECT_TRUE(tooSmall.HoldsEmptyText());

  Buffer exact(expected.size() + 1);
  EXPECT_EQ(NormalizeText(json, exact), Status::Ok);
  EXPECT_EQ(exact.Text(), expected);

  Buffer larger(64);
  EXPECT_EQ(NormalizeText(json, larger), Status::Ok);
  EXPECT_EQ(larger.Text(), expected);
}

TEST(NormalizeJsonText, OneByteBufferIsBufferTooSmallAndEmpty) {
  ExpectNormalizeFails("{'a':1}", Status::BufferTooSmall, 1);
}

TEST(NormalizeJsonText, NormalizedHoldsTheTextOnlyOnSuccessAndOverwritesAPreviousResult) {
  Buffer out(32);
  ASSERT_EQ(NormalizeText("['x']", out), Status::Ok);
  ASSERT_EQ(out.Text(), R"(["x"])");
  EXPECT_EQ(NormalizeText("['x'", out), Status::ParseError);
  EXPECT_TRUE(out.HoldsEmptyText());
}

TEST(NormalizeJsonText, DocumentDeeperThanTheStoreLimitIsNormalized) {
  const std::size_t depth = kMaxJsonDepth + 1;
  const std::string json = std::string(depth, '[') + "'x'" + std::string(depth, ']');
  const std::string expected = std::string(depth, '[') + "\"x\"" + std::string(depth, ']');
  ExpectNormalizes(json, expected);

  JsonDocumentStore store;
  ExpectParseFails(store, json, Status::NestingTooDeep);
}

TEST(NormalizeJsonText, DocumentWithMoreLeavesThanTheStoreLimitIsNormalized) {
  std::string json = "[";
  std::string expected = "[";
  for (std::size_t i = 0; i < kMaxFlatEntries + 1; ++i) {
    json += i == 0 ? "'a'" : ",'a'";
    expected += i == 0 ? "\"a\"" : ",\"a\"";
  }
  json += "]";
  expected += "]";
  ExpectNormalizes(json, expected);

  JsonDocumentStore store;
  ExpectParseFails(store, json, Status::TooManyEntries);
}

TEST(NormalizeJsonText, DoesNotTouchTheDocumentStore) {
  JsonDocumentStore store;
  const std::vector<std::uint32_t> ids = FillPool(store);
  ASSERT_EQ(ids.size(), kJsonDocumentSlotCount);
  Buffer out(32);
  EXPECT_EQ(NormalizeText("{'a':1}", out), Status::Ok);
  EXPECT_EQ(out.Text(), R"({"a":1})");
  for (const std::uint32_t id : ids) {
    EXPECT_EQ(CountOf(store, id), 1u);
  }
}

TEST(TextApiInternalError, NormalizeReportsInternalErrorWhenAllocationFailsAndNormalizedIsEmpty) {
  const std::string json = "{'a':[1,2,3],'b':{'c':'" + std::string(200, 'x') + "'}}";
  Buffer out(512);
  Status status = Status::Ok;
  int failedAllocations = 0;
  {
    FailAllocations failAllocations;
    status = NormalizeText(json, out);
    failedAllocations = t_failedAllocations;
  }
  EXPECT_EQ(status, Status::InternalError);
  EXPECT_GT(failedAllocations, 0);
  EXPECT_TRUE(out.HoldsEmptyText());

  EXPECT_EQ(NormalizeText(json, out), Status::Ok);
  EXPECT_EQ(out.Text(), R"({"a":[1,2,3],"b":{"c":")" + std::string(200, 'x') + R"("}})");
}
