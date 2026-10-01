// Coverage for src/mapping/json-flatten.*.

#include "mapping/json-flatten.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "core/json-path.h"
#include "core/json-value.h"
#include "core/status.h"
#include "../test-support/status-print.h"
#include "core/type-conversion.h"

namespace {

// Not an enumerator, so no successful call can produce it.
constexpr JsonEntryType kNoType = static_cast<JsonEntryType>(-1);

int TypeNumber(JsonEntryType type) { return static_cast<int>(type); }

FlattenResult MakeSentinel() {
  FlattenResult result;
  result.document = JsonValue(std::string("sentinel"));
  result.entries.push_back(FlatEntry{"sentinel-key", "sentinel-value", JsonEntryType::String});
  return result;
}

// A fatal ASSERT_* inside a void helper below ends only that helper; the calling test carries on.
void ExpectSentinel(const FlattenResult& result) {
  EXPECT_TRUE(result.document.is_string());
  EXPECT_EQ(result.document.get<std::string>(), "sentinel");
  ASSERT_EQ(result.entries.size(), 1u);
  EXPECT_EQ(result.entries[0].key, "sentinel-key");
  EXPECT_EQ(result.entries[0].value, "sentinel-value");
  EXPECT_EQ(TypeNumber(result.entries[0].type), TypeNumber(JsonEntryType::String));
}

void ExpectEntries(const FlattenResult& result, const std::vector<FlatEntry>& expected) {
  ASSERT_EQ(result.entries.size(), expected.size());
  for (std::size_t i = 0; i < expected.size(); ++i) {
    SCOPED_TRACE("entry " + std::to_string(i));
    EXPECT_EQ(result.entries[i].key, expected[i].key);
    EXPECT_EQ(result.entries[i].value, expected[i].value);
    EXPECT_EQ(TypeNumber(result.entries[i].type), TypeNumber(expected[i].type));
  }
}

void ExpectFlattens(std::string_view text, const std::vector<FlatEntry>& expected) {
  SCOPED_TRACE(std::string(text));
  FlattenResult result;
  ASSERT_EQ(FlattenJson(text, result), Status::Ok);
  ExpectEntries(result, expected);
}

void ExpectFails(std::string_view text, Status expected) {
  FlattenResult result = MakeSentinel();
  EXPECT_EQ(FlattenJson(text, result), expected);
  ExpectSentinel(result);
}

std::size_t CountLeaves(const JsonValue& node) {
  if ((node.is_object() || node.is_array()) && !node.empty()) {
    std::size_t total = 0;
    for (const auto& child : node) {
      total += CountLeaves(child);
    }
    return total;
  }
  return 1;
}

// The expected text is rebuilt from the node's own accessors, not from ValueToText, which is what
// produced the entry text under test.
void ExpectNodeMatchesEntry(const JsonValue& node, const FlatEntry& entry) {
  switch (entry.type) {
    case JsonEntryType::String:
      ASSERT_TRUE(node.is_string());
      EXPECT_EQ(node.get<std::string>(), entry.value);
      return;
    case JsonEntryType::Number:
      ASSERT_TRUE(node.is_number());
      if (node.is_number_unsigned()) {
        EXPECT_EQ(std::to_string(node.get<std::uint64_t>()), entry.value);
      } else if (node.is_number_integer()) {
        EXPECT_EQ(std::to_string(node.get<std::int64_t>()), entry.value);
      } else {
        EXPECT_DOUBLE_EQ(std::strtod(entry.value.c_str(), nullptr), node.get<double>());
      }
      return;
    case JsonEntryType::Bool:
      ASSERT_TRUE(node.is_boolean());
      EXPECT_EQ(node.get<bool>() ? "true" : "false", entry.value);
      return;
    case JsonEntryType::Null:
      EXPECT_TRUE(node.is_null());
      EXPECT_EQ(entry.value, "null");
      return;
    case JsonEntryType::EmptyObject:
      EXPECT_TRUE(node.is_object() && node.empty());
      EXPECT_EQ(entry.value, "{}");
      return;
    case JsonEntryType::EmptyArray:
      EXPECT_TRUE(node.is_array() && node.empty());
      EXPECT_EQ(entry.value, "[]");
      return;
    case JsonEntryType::None:
      ADD_FAILURE() << "an entry must never carry type None";
      return;
  }
}

void ExpectRoundTrips(std::string_view text) {
  SCOPED_TRACE(std::string(text));
  FlattenResult result;
  ASSERT_EQ(FlattenJson(text, result), Status::Ok);
  ASSERT_FALSE(result.entries.empty());
  EXPECT_EQ(result.entries.size(), CountLeaves(result.document));

  std::set<std::string> seenKeys;
  for (const FlatEntry& entry : result.entries) {
    SCOPED_TRACE("key '" + entry.key + "'");
    EXPECT_TRUE(seenKeys.insert(entry.key).second);
    const JsonValue* node = nullptr;
    ASSERT_EQ(ResolvePath(result.document, entry.key, node), Status::Ok);
    ASSERT_NE(node, nullptr);
    ExpectNodeMatchesEntry(*node, entry);
  }
}

std::string NestedArrays(std::size_t depth) {
  return std::string(depth, '[') + std::string(depth, ']');
}

std::string NestedObjects(std::size_t depth) {
  if (depth == 0) {
    ADD_FAILURE() << "depth counts the root container and must be at least 1";
    return "{}";
  }
  std::string text;
  for (std::size_t i = 1; i < depth; ++i) {
    text += "{\"a\":";
  }
  text += "{}";
  text.append(depth - 1, '}');
  return text;
}

std::string Repeated(const std::string& unit, std::size_t count) {
  std::string text;
  text.reserve(unit.size() * count);
  for (std::size_t i = 0; i < count; ++i) {
    text += unit;
  }
  return text;
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

std::string FlatObjectUniqueKeys(std::size_t count) {
  std::string text = "{";
  for (std::size_t i = 0; i < count; ++i) {
    if (i != 0) {
      text += ',';
    }
    text += "\"k" + std::to_string(i) + "\":0";
  }
  text += '}';
  return text;
}

std::string ObjectRepeatingKey(std::size_t count) {
  std::string text = "{";
  for (std::size_t i = 0; i < count; ++i) {
    if (i != 0) {
      text += ',';
    }
    text += "\"a\":0";
  }
  text += '}';
  return text;
}

std::string PaddedToSize(std::string text, std::size_t size) {
  EXPECT_LE(text.size(), size) << "padding must not truncate the text";
  text.resize(size, ' ');
  return text;
}

// Key-text budget fixture: 4000 leaves under a 1022-byte key, each entry key "/A/<i>" being
// 1 + 1022 + 1 + digits(i) = 1024 + digits(i) bytes, plus one tail leaf keyed "/<tailKey>".
constexpr std::size_t kBudgetLeafCount = 4000;
constexpr std::size_t kBudgetLongKeyLength = 1022;
constexpr std::size_t kBudgetDigitSum = 14890;  // digits of 0..3999: 10*1 + 90*2 + 900*3 + 3000*4
constexpr std::size_t kBudgetTailKeyLength = 83413;
static_assert(kBudgetLeafCount * (1 + kBudgetLongKeyLength + 1) + kBudgetDigitSum + 1 +
                      kBudgetTailKeyLength ==
                  4194304,
              "the fixture must land exactly on the documented key-text limit");

std::size_t DigitSumBelow(std::size_t count) {
  std::size_t sum = 0;
  for (std::size_t i = 0; i < count; ++i) {
    sum += std::to_string(i).size();
  }
  return sum;
}

std::size_t TotalKeyBytes(const FlattenResult& result) {
  std::size_t total = 0;
  for (const FlatEntry& entry : result.entries) {
    total += entry.key.size();
  }
  return total;
}

std::string ZeroArrayMember(const std::string& key, std::size_t count) {
  return "\"" + key + "\":" + FlatArrayOf("0", count);
}

// Everything up to and including the tail member's value, without the closing brace, so a test can
// append further members.
std::string KeyBudgetPrefix(const std::string& tailKey, const std::string& tailValue) {
  return "{" + ZeroArrayMember(std::string(kBudgetLongKeyLength, 'a'), kBudgetLeafCount) + ",\"" +
         tailKey + "\":" + tailValue;
}

std::string KeyBudgetDocument(const std::string& tailKey, const std::string& tailValue = "0") {
  return KeyBudgetPrefix(tailKey, tailValue) + "}";
}

void ExpectKeyBudgetAtTheLimit(const std::string& text, std::size_t expectedEntries) {
  FlattenResult result;
  ASSERT_EQ(FlattenJson(text, result), Status::Ok);
  EXPECT_EQ(result.entries.size(), expectedEntries);
  EXPECT_EQ(TotalKeyBytes(result), kMaxFlatKeyBytes);
}

}  // namespace

// ---------------------------------------------------------------------------
// Round-trip: every emitted key resolves back through ResolvePath.
// ---------------------------------------------------------------------------

TEST(FlattenJson, EveryKeyResolvesBackToItsValue) {
  const char* const documents[] = {
      R"({"a":{"b":[10,20]}})",
      R"({"b":{"y":1,"x":2},"a":[3,{"z":4}],"c":{}})",
      R"([[],{},[1,[2,[]]],{"a":{}}])",
      R"({"s":"text","n":-12,"f":2.5,"t":true,"u":false,"z":null,"e":"","o":{},"r":[]})",
      R"({"line":"a\nb \"q\" \\ \/"})",
      R"({"deep":{"deeper":{"deepest":[{"k":[1,2,{"m":"v"}]}]}}})",
  };
  for (const char* document : documents) {
    ExpectRoundTrips(document);
  }
}

TEST(FlattenJson, KeysWithSpecialCharactersResolveBackToTheirValues) {
  const char* const documents[] = {
      R"({"a/b":1,"m~n":2})",
      R"({"~1":1,"~0":2,"~01":3,"~10":4})",
      R"({"/":1,"~":2,"//":3,"~~":4,"~/":5,"/~":6,"a~":7,"~a":8})",
      R"({"a.b":1,"a":{"b":2}})",
      R"({"[0]":1,"a[1]":2,"[":3,"]":4,"a[":5})",
      R"({" ":1,"a b":2," lead":3,"trail ":4})",
      R"({"":1})",
      R"({"x":{"":1}})",
      R"({"":{"0":1}})",
      R"({"":[1,2]})",
      R"({"":{"":{"":1}}})",
      R"({"\u00e9":"\u00fc","\u65e5\u672c":"\ud83d\ude00"})",
      R"({"a\/b":{"c~d":[{"e/f":null}]}})",
      R"({"0":{"1":[{"2":3}]}})",
      R"({"k":"c\u0000d"})",
  };
  for (const char* document : documents) {
    ExpectRoundTrips(document);
  }
}

TEST(FlattenJson, ScalarAndEmptyRootsResolveBackThroughTheEmptyPath) {
  const char* const documents[] = {"42",   "-7",    "2.5",  "\"text\"", "\"\"",
                                   "true", "false", "null", "{}",       "[]"};
  for (const char* document : documents) {
    ExpectRoundTrips(document);
  }
}

TEST(FlattenJson, DuplicateKeyDocumentsStillRoundTrip) {
  ExpectRoundTrips(R"({"a":1,"a":2})");
  ExpectRoundTrips(R"({"a":{"x":1},"b":0,"a":{"y":2}})");
}

TEST(FlattenJson, DocumentIsTheParsedInput) {
  FlattenResult result;
  ASSERT_EQ(FlattenJson(R"({"z":1,"a":[2,3]})", result), Status::Ok);
  EXPECT_EQ(result.document.dump(), R"({"z":1,"a":[2,3]})");
}

// ---------------------------------------------------------------------------
// Entry order.
// ---------------------------------------------------------------------------

TEST(FlattenJson, ObjectFieldsFollowDocumentOrderNotAlphabeticalOrder) {
  ExpectFlattens(R"({"z":1,"a":2,"m":3})", {
                                               {"/z", "1", JsonEntryType::Number},
                                               {"/a", "2", JsonEntryType::Number},
                                               {"/m", "3", JsonEntryType::Number},
                                           });
}

TEST(FlattenJson, NumericLookingKeysFollowDocumentOrder) {
  ExpectFlattens(R"({"10":1,"9":2,"2":3})", {
                                                {"/10", "1", JsonEntryType::Number},
                                                {"/9", "2", JsonEntryType::Number},
                                                {"/2", "3", JsonEntryType::Number},
                                            });
}

TEST(FlattenJson, NestedDocumentIsTraversedPreOrderDepthFirst) {
  ExpectFlattens(R"({"b":{"y":1,"x":2},"a":[3,{"z":4}],"c":{}})",
                 {
                     {"/b/y", "1", JsonEntryType::Number},
                     {"/b/x", "2", JsonEntryType::Number},
                     {"/a/0", "3", JsonEntryType::Number},
                     {"/a/1/z", "4", JsonEntryType::Number},
                     {"/c", "{}", JsonEntryType::EmptyObject},
                 });
}

TEST(FlattenJson, ContainerEntriesComeBeforeTheNextSibling) {
  ExpectFlattens(R"({"a":{"x":1,"y":2},"b":3,"c":{"z":4}})",
                 {
                     {"/a/x", "1", JsonEntryType::Number},
                     {"/a/y", "2", JsonEntryType::Number},
                     {"/b", "3", JsonEntryType::Number},
                     {"/c/z", "4", JsonEntryType::Number},
                 });
}

TEST(FlattenJson, ArrayElementsFollowIndexOrderPastTen) {
  ExpectFlattens("[0,1,2,3,4,5,6,7,8,9,10,11]", {
                                                    {"/0", "0", JsonEntryType::Number},
                                                    {"/1", "1", JsonEntryType::Number},
                                                    {"/2", "2", JsonEntryType::Number},
                                                    {"/3", "3", JsonEntryType::Number},
                                                    {"/4", "4", JsonEntryType::Number},
                                                    {"/5", "5", JsonEntryType::Number},
                                                    {"/6", "6", JsonEntryType::Number},
                                                    {"/7", "7", JsonEntryType::Number},
                                                    {"/8", "8", JsonEntryType::Number},
                                                    {"/9", "9", JsonEntryType::Number},
                                                    {"/10", "10", JsonEntryType::Number},
                                                    {"/11", "11", JsonEntryType::Number},
                                                });
}

TEST(FlattenJson, ObjectsInsideArraysKeepTheirOwnDocumentOrder) {
  ExpectFlattens(R"([{"b":1,"a":2},{"b":3}])", {
                                                   {"/0/b", "1", JsonEntryType::Number},
                                                   {"/0/a", "2", JsonEntryType::Number},
                                                   {"/1/b", "3", JsonEntryType::Number},
                                               });
}

TEST(FlattenJson, SameTextTwiceGivesTheSameEntryList) {
  const std::string text = R"({"q":[1,{"y":"a","b":null}],"c":{},"a":true})";
  FlattenResult first;
  FlattenResult second;
  ASSERT_EQ(FlattenJson(text, first), Status::Ok);
  ASSERT_EQ(FlattenJson(text, second), Status::Ok);
  ASSERT_EQ(first.entries.size(), second.entries.size());
  for (std::size_t i = 0; i < first.entries.size(); ++i) {
    SCOPED_TRACE("entry " + std::to_string(i));
    EXPECT_EQ(first.entries[i].key, second.entries[i].key);
    EXPECT_EQ(first.entries[i].value, second.entries[i].value);
    EXPECT_EQ(TypeNumber(first.entries[i].type), TypeNumber(second.entries[i].type));
  }
}

// ---------------------------------------------------------------------------
// Duplicate object keys: last value wins, first position kept.
// ---------------------------------------------------------------------------

TEST(FlattenJson, RepeatedKeyKeepsOnlyTheLastValue) {
  ExpectFlattens(R"({"a":1,"a":2})", {{"/a", "2", JsonEntryType::Number}});
}

TEST(FlattenJson, RepeatedObjectKeyReplacesTheWholeValueAtTheFirstPosition) {
  ExpectFlattens(R"({"a":{"x":1},"b":0,"a":{"y":2}})", {
                                                           {"/a/y", "2", JsonEntryType::Number},
                                                           {"/b", "0", JsonEntryType::Number},
                                                       });
}

TEST(FlattenJson, RepeatedKeyBetweenOtherKeysKeepsFirstPosition) {
  ExpectFlattens(R"({"a":1,"b":2,"a":3})", {
                                               {"/a", "3", JsonEntryType::Number},
                                               {"/b", "2", JsonEntryType::Number},
                                           });
}

TEST(FlattenJson, RepeatedKeyMayChangeTheValueKind) {
  ExpectFlattens(R"({"a":{"x":1,"y":2},"a":0})", {{"/a", "0", JsonEntryType::Number}});
  ExpectFlattens(R"({"a":1,"a":{"x":2}})", {{"/a/x", "2", JsonEntryType::Number}});
  ExpectFlattens(R"({"a":"s","a":{}})", {{"/a", "{}", JsonEntryType::EmptyObject}});
}

TEST(FlattenJson, DuplicateKeysInsideArrayElementsAreResolvedPerObject) {
  ExpectFlattens(R"([{"a":1,"a":2},{"a":3}])", {
                                                   {"/0/a", "2", JsonEntryType::Number},
                                                   {"/1/a", "3", JsonEntryType::Number},
                                               });
}

TEST(FlattenJson, DocumentReflectsDuplicateResolution) {
  FlattenResult result;
  ASSERT_EQ(FlattenJson(R"({"a":{"x":1},"b":0,"a":{"y":2}})", result), Status::Ok);
  EXPECT_EQ(result.document.dump(), R"({"a":{"y":2},"b":0})");
}

// ---------------------------------------------------------------------------
// Leaves and value types.
// ---------------------------------------------------------------------------

TEST(FlattenJson, EachLeafKindGetsItsTypeAndText) {
  ExpectFlattens(R"({"s":"text","n":42,"t":true,"f":false,"z":null,"o":{},"r":[]})",
                 {
                     {"/s", "text", JsonEntryType::String},
                     {"/n", "42", JsonEntryType::Number},
                     {"/t", "true", JsonEntryType::Bool},
                     {"/f", "false", JsonEntryType::Bool},
                     {"/z", "null", JsonEntryType::Null},
                     {"/o", "{}", JsonEntryType::EmptyObject},
                     {"/r", "[]", JsonEntryType::EmptyArray},
                 });
}

TEST(FlattenJson, EntryTypeNumbersAreFixed) {
  EXPECT_EQ(TypeNumber(JsonEntryType::None), 0);
  EXPECT_EQ(TypeNumber(JsonEntryType::String), 1);
  EXPECT_EQ(TypeNumber(JsonEntryType::Number), 2);
  EXPECT_EQ(TypeNumber(JsonEntryType::Bool), 3);
  EXPECT_EQ(TypeNumber(JsonEntryType::Null), 4);
  EXPECT_EQ(TypeNumber(JsonEntryType::EmptyObject), 5);
  EXPECT_EQ(TypeNumber(JsonEntryType::EmptyArray), 6);
}

TEST(FlattenJson, NullAndTheStringNullAreDistinguishedByType) {
  ExpectFlattens(R"({"a":null,"b":"null"})", {
                                                 {"/a", "null", JsonEntryType::Null},
                                                 {"/b", "null", JsonEntryType::String},
                                             });
}

TEST(FlattenJson, NumberAndNumericStringAreDistinguishedByType) {
  ExpectFlattens(R"({"a":5,"b":"5"})", {
                                           {"/a", "5", JsonEntryType::Number},
                                           {"/b", "5", JsonEntryType::String},
                                       });
}

TEST(FlattenJson, BoolAndBoolLookingStringAreDistinguishedByType) {
  ExpectFlattens(R"({"a":true,"b":"true","c":false,"d":"false"})",
                 {
                     {"/a", "true", JsonEntryType::Bool},
                     {"/b", "true", JsonEntryType::String},
                     {"/c", "false", JsonEntryType::Bool},
                     {"/d", "false", JsonEntryType::String},
                 });
}

TEST(FlattenJson, EmptyContainerAndContainerLookingStringAreDistinguishedByType) {
  ExpectFlattens(R"({"a":"{}","b":{},"c":"[]","d":[]})",
                 {
                     {"/a", "{}", JsonEntryType::String},
                     {"/b", "{}", JsonEntryType::EmptyObject},
                     {"/c", "[]", JsonEntryType::String},
                     {"/d", "[]", JsonEntryType::EmptyArray},
                 });
}

TEST(FlattenJson, EmptyStringIsAStringEntryWithEmptyText) {
  ExpectFlattens(R"({"a":"","b":null})", {
                                             {"/a", "", JsonEntryType::String},
                                             {"/b", "null", JsonEntryType::Null},
                                         });
}

TEST(FlattenJson, StringTextIsUnquotedAndUnescaped) {
  ExpectFlattens(R"({"a":"x\"y\\z\n\tq\u0041\/"})",
                 {{"/a", "x\"y\\z\n\tqA/", JsonEntryType::String}});
}

TEST(FlattenJson, MultiByteUtf8TextIsKeptVerbatim) {
  ExpectFlattens(R"({"\u00e9":"\u00fc","k":"\ud83d\ude00"})",
                 {
                     {std::string("/\xC3\xA9"), std::string("\xC3\xBC"), JsonEntryType::String},
                     {"/k", std::string("\xF0\x9F\x98\x80"), JsonEntryType::String},
                 });
  ExpectFlattens("{\"\xC3\xA9\":\"\xE2\x82\xAC\"}",
                 {{std::string("/\xC3\xA9"), std::string("\xE2\x82\xAC"), JsonEntryType::String}});
}

TEST(FlattenJson, NumbersUseTheDocumentedText) {
  ExpectFlattens(R"([42,-7,0,5.0,2.5,-2.5,0.1,1e-5,1e300])",
                 {
                     {"/0", "42", JsonEntryType::Number},
                     {"/1", "-7", JsonEntryType::Number},
                     {"/2", "0", JsonEntryType::Number},
                     {"/3", "5.0", JsonEntryType::Number},
                     {"/4", "2.5", JsonEntryType::Number},
                     {"/5", "-2.5", JsonEntryType::Number},
                     {"/6", "0.1", JsonEntryType::Number},
                     {"/7", "1e-05", JsonEntryType::Number},
                     {"/8", "1e+300", JsonEntryType::Number},
                 });
}

TEST(FlattenJson, IntegersWithinSixtyFourBitsKeepAllDigits) {
  ExpectFlattens(
      "[9223372036854775807,9223372036854775808,18446744073709551615,-9223372036854775808]",
      {
          {"/0", "9223372036854775807", JsonEntryType::Number},
          {"/1", "9223372036854775808", JsonEntryType::Number},
          {"/2", "18446744073709551615", JsonEntryType::Number},
          {"/3", "-9223372036854775808", JsonEntryType::Number},
      });
}

TEST(FlattenJson, IntegersBeyondSixtyFourBitsAreNumbersThatLosePrecision) {
  FlattenResult result;
  ASSERT_EQ(
      FlattenJson("[18446744073709551616,-9223372036854775809,123456789012345678901234567890]",
                  result),
      Status::Ok);
  ASSERT_EQ(result.entries.size(), 3u);
  for (const FlatEntry& entry : result.entries) {
    EXPECT_EQ(TypeNumber(entry.type), TypeNumber(JsonEntryType::Number));
  }
  EXPECT_DOUBLE_EQ(std::strtod(result.entries[0].value.c_str(), nullptr), 18446744073709551616.0);
  EXPECT_DOUBLE_EQ(std::strtod(result.entries[1].value.c_str(), nullptr), -9223372036854775809.0);
  EXPECT_NE(result.entries[2].value, "123456789012345678901234567890");
  EXPECT_DOUBLE_EQ(std::strtod(result.entries[2].value.c_str(), nullptr),
                   123456789012345678901234567890.0);
}

TEST(FlattenJson, ExponentAndZeroFormsUseTheLibrarySerializerText) {
  ExpectFlattens("[1e2,1E5,100.0,0.0,-0.0,-0,1.5e+10]",
                 {
                     {"/0", "100.0", JsonEntryType::Number},
                     {"/1", "100000.0", JsonEntryType::Number},
                     {"/2", "100.0", JsonEntryType::Number},
                     {"/3", "0.0", JsonEntryType::Number},
                     {"/4", "-0.0", JsonEntryType::Number},
                     {"/5", "0", JsonEntryType::Number},
                     {"/6", "15000000000.0", JsonEntryType::Number},
                 });
}

TEST(FlattenJson, DecodedNulInAStringValueIsKeptInTheEntryText) {
  ExpectFlattens(R"({"k":"a\u0000b"})", {{"/k", std::string("a\0b", 3), JsonEntryType::String}});
}

TEST(FlattenJson, DecodedNulInAKeyIsParseError) {
  ExpectFails(R"({"a\u0000b":1})", Status::ParseError);
  ExpectFails(R"({"\u0000":1})", Status::ParseError);
  ExpectFails(R"({"a\u0000":1})", Status::ParseError);
  ExpectFails(R"({"x":{"a\u0000b":1}})", Status::ParseError);
  ExpectFails(R"([{"a\u0000b":1}])", Status::ParseError);
  ExpectFails(R"({"ok":1,"a\u0000b":{}})", Status::ParseError);
}

TEST(FlattenJson, DecodedNulInAKeyIsReportedAtTheKeyInTextOrder) {
  const std::string manyLeaves = ZeroArrayMember("A", kMaxFlatEntries + 1);
  ExpectFails("{" + manyLeaves + R"(,"k\u0000":1})", Status::TooManyEntries);
  ExpectFails(R"({"k\u0000":1,)" + manyLeaves + "}", Status::ParseError);

  ExpectFails(KeyBudgetPrefix(std::string(kBudgetTailKeyLength + 1, 'b'), "0") + R"(,"k\u0000":1})",
              Status::KeyTextTooLarge);
}

TEST(FlattenJson, NonEmptyContainersProduceNoEntryOfTheirOwn) {
  ExpectFlattens("[[1],[2]]", {
                                  {"/0/0", "1", JsonEntryType::Number},
                                  {"/1/0", "2", JsonEntryType::Number},
                              });
  ExpectFlattens(R"({"a":{"b":[true]}})", {{"/a/b/0", "true", JsonEntryType::Bool}});
}

TEST(FlattenJson, EmptyContainersAreEntriesEvenWhenNested) {
  ExpectFlattens("[[],{}]", {
                                {"/0", "[]", JsonEntryType::EmptyArray},
                                {"/1", "{}", JsonEntryType::EmptyObject},
                            });
  ExpectFlattens("[[[]]]", {{"/0/0", "[]", JsonEntryType::EmptyArray}});
  ExpectFlattens(R"({"a":{"b":{}}})", {{"/a/b", "{}", JsonEntryType::EmptyObject}});
  ExpectFlattens(R"({"a":[{}],"b":[[]]})", {
                                               {"/a/0", "{}", JsonEntryType::EmptyObject},
                                               {"/b/0", "[]", JsonEntryType::EmptyArray},
                                           });
}

TEST(FlattenJson, ScalarAndNullRootsHaveExactlyOneEntryWithTheEmptyKey) {
  ExpectFlattens("42", {{"", "42", JsonEntryType::Number}});
  ExpectFlattens("-7", {{"", "-7", JsonEntryType::Number}});
  ExpectFlattens("2.5", {{"", "2.5", JsonEntryType::Number}});
  ExpectFlattens(R"("abc")", {{"", "abc", JsonEntryType::String}});
  ExpectFlattens(R"("")", {{"", "", JsonEntryType::String}});
  ExpectFlattens("true", {{"", "true", JsonEntryType::Bool}});
  ExpectFlattens("false", {{"", "false", JsonEntryType::Bool}});
  ExpectFlattens("null", {{"", "null", JsonEntryType::Null}});
}

TEST(FlattenJson, EmptyContainerRootsHaveOneEntryWithTheEmptyKey) {
  ExpectFlattens("{}", {{"", "{}", JsonEntryType::EmptyObject}});
  ExpectFlattens("[]", {{"", "[]", JsonEntryType::EmptyArray}});
}

TEST(FlattenJson, SurroundingWhitespaceIsAccepted) {
  ExpectFlattens(" \t\r\n[1]\n ", {{"/0", "1", JsonEntryType::Number}});
  ExpectFlattens("  42  ", {{"", "42", JsonEntryType::Number}});
}

TEST(FlattenJson, LeadingUtf8ByteOrderMarkIsSkipped) {
  ExpectFlattens(std::string("\xEF\xBB\xBF") + "[1]", {{"/0", "1", JsonEntryType::Number}});
}

TEST(FlattenJson, ByteOrderMarkWithoutTextOrWithAPartialMarkIsParseError) {
  ExpectFails("\xEF\xBB\xBF", Status::ParseError);
  ExpectFails("\xEF\xBB", Status::ParseError);
}

TEST(FlattenJson, StringViewSizeDecidesTheTextNotAnyLaterBytes) {
  ExpectFlattens(std::string_view("[1]garbage", 3), {{"/0", "1", JsonEntryType::Number}});
}

TEST(FlattenJson, NulAfterTheDocumentIsParseError) {
  ExpectFails(std::string("[1]\0", 4), Status::ParseError);
  ExpectFails(std::string("[1]\0garbage", 11), Status::ParseError);
  ExpectFails(std::string("{\"a\":1}\0", 8), Status::ParseError);
}

TEST(FlattenJson, NulInTheTextIsCheckedBeforeScanningSoItBeatsLaterLimitFailures) {
  ExpectFails(NestedArrays(kMaxJsonDepth + 1) + std::string(1, '\0'), Status::ParseError);
  ExpectFails(FlatArrayOf("0", kMaxFlatEntries + 1) + std::string(1, '\0'), Status::ParseError);
  ExpectFails(KeyBudgetDocument(std::string(kBudgetTailKeyLength + 1, 'b')) + std::string(1, '\0'),
              Status::ParseError);
}

TEST(FlattenJson, SuccessReplacesThePreviousResultCompletely) {
  FlattenResult result = MakeSentinel();
  ASSERT_EQ(FlattenJson(R"({"a":1})", result), Status::Ok);
  ExpectEntries(result, {{"/a", "1", JsonEntryType::Number}});
  EXPECT_EQ(result.document.dump(), R"({"a":1})");
}

// ---------------------------------------------------------------------------
// Keys.
// ---------------------------------------------------------------------------

TEST(FlattenJson, EmptyKeyUnderAnObjectGivesATrailingSlash) {
  ExpectFlattens(R"({"x":{"":1}})", {{"/x/", "1", JsonEntryType::Number}});
}

TEST(FlattenJson, EmptyKeyAtTheRootGivesASingleSlash) {
  ExpectFlattens(R"({"":1})", {{"/", "1", JsonEntryType::Number}});
}

TEST(FlattenJson, EmptyKeyContainingAnObjectOrArrayGivesDoubleSlash) {
  ExpectFlattens(R"({"":{"0":1}})", {{"//0", "1", JsonEntryType::Number}});
  ExpectFlattens(R"({"":[1]})", {{"//0", "1", JsonEntryType::Number}});
}

TEST(FlattenJson, ConsecutiveEmptyKeysGiveConsecutiveSlashes) {
  ExpectFlattens(R"({"":{"":{"":1}}})", {{"///", "1", JsonEntryType::Number}});
}

TEST(FlattenJson, SlashInAKeyIsEscapedAsTildeOne) {
  ExpectFlattens(R"({"a/b":1})", {{"/a~1b", "1", JsonEntryType::Number}});
  ExpectFlattens(R"({"/":1})", {{"/~1", "1", JsonEntryType::Number}});
}

TEST(FlattenJson, TildeInAKeyIsEscapedAsTildeZero) {
  ExpectFlattens(R"({"m~n":1})", {{"/m~0n", "1", JsonEntryType::Number}});
  ExpectFlattens(R"({"~":1})", {{"/~0", "1", JsonEntryType::Number}});
}

TEST(FlattenJson, KeyWithBothTildeAndSlashIsEscapedPerCharacter) {
  ExpectFlattens(R"({"~/":1,"/~":2})", {
                                           {"/~0~1", "1", JsonEntryType::Number},
                                           {"/~1~0", "2", JsonEntryType::Number},
                                       });
}

TEST(FlattenJson, KeyContainingTildeOneLiterallyBecomesTildeZeroOne) {
  ExpectFlattens(R"({"~1":1,"~0":2})", {
                                           {"/~01", "1", JsonEntryType::Number},
                                           {"/~00", "2", JsonEntryType::Number},
                                       });
}

TEST(FlattenJson, KeysMadeOnlyOfTildesAndSlashesAreEscapedPerCharacter) {
  ExpectFlattens(R"({"~~":1,"//":2,"~/~":3})", {
                                                   {"/~0~0", "1", JsonEntryType::Number},
                                                   {"/~1~1", "2", JsonEntryType::Number},
                                                   {"/~0~1~0", "3", JsonEntryType::Number},
                                               });
}

TEST(FlattenJson, DotKeyAndNestedKeyProduceDifferentPaths) {
  ExpectFlattens(R"({"a.b":1,"a":{"b":2}})", {
                                                 {"/a.b", "1", JsonEntryType::Number},
                                                 {"/a/b", "2", JsonEntryType::Number},
                                             });
}

TEST(FlattenJson, BracketsDotsAndSpacesInKeysNeedNoEscaping) {
  ExpectFlattens(R"({"a[0]":1,"[":2,"@odata.context":3," a b ":4})",
                 {
                     {"/a[0]", "1", JsonEntryType::Number},
                     {"/[", "2", JsonEntryType::Number},
                     {"/@odata.context", "3", JsonEntryType::Number},
                     {"/ a b ", "4", JsonEntryType::Number},
                 });
}

TEST(FlattenJson, EscapedSlashInJsonTextIsTheSameKeyAsALiteralSlash) {
  ExpectFlattens(R"({"a\/b":1})", {{"/a~1b", "1", JsonEntryType::Number}});
}

TEST(FlattenJson, NumericObjectKeysAreKeysNotIndices) {
  ExpectFlattens(R"({"0":{"1":2}})", {{"/0/1", "2", JsonEntryType::Number}});
}

TEST(FlattenJson, ArrayIndicesAreDecimalWithoutLeadingZeros) {
  const std::string text = FlatArrayOf("0", 101);
  FlattenResult result;
  ASSERT_EQ(FlattenJson(text, result), Status::Ok);
  ASSERT_EQ(result.entries.size(), 101u);
  EXPECT_EQ(result.entries[99].key, "/99");
  EXPECT_EQ(result.entries[100].key, "/100");
}

// ---------------------------------------------------------------------------
// Errors: ParseError and the result left untouched.
// ---------------------------------------------------------------------------

TEST(FlattenJson, FailureStatusesHaveTheDocumentedNumbers) {
  EXPECT_EQ(static_cast<int>(Status::ParseError), -10);
  EXPECT_EQ(static_cast<int>(Status::DocumentTooLarge), -32);
  EXPECT_EQ(static_cast<int>(Status::NestingTooDeep), -33);
  EXPECT_EQ(static_cast<int>(Status::TooManyEntries), -34);
  EXPECT_EQ(static_cast<int>(Status::KeyTextTooLarge), -36);
}

TEST(FlattenJson, LimitConstantsHaveTheDocumentedValues) {
  EXPECT_EQ(kMaxJsonInputBytes, 1048576u);
  EXPECT_EQ(kMaxJsonDepth, 64u);
  EXPECT_EQ(kMaxFlatEntries, 10000u);
  EXPECT_EQ(kMaxFlatKeyBytes, 4194304u);
}

TEST(FlattenJson, EmptyTextIsParseError) {
  ExpectFails("", Status::ParseError);
  ExpectFails(std::string_view(), Status::ParseError);
}

TEST(FlattenJson, WhitespaceOnlyTextIsParseError) {
  const char* const texts[] = {" ", "   ", "\t", "\r\n", " \t\r\n "};
  for (const char* text : texts) {
    SCOPED_TRACE(text);
    ExpectFails(text, Status::ParseError);
  }
}

TEST(FlattenJson, InvalidJsonIsParseError) {
  const char* const texts[] = {
      "{",
      "}",
      "[",
      "]",
      "{\"a\":}",
      "{\"a\" 1}",
      "{\"a\":1,}",
      "[1,]",
      "[,1]",
      "[1 2]",
      "{a:1}",
      "{'a':1}",
      "{\"a\":1 \"b\":2}",
      "tru",
      "nul",
      "nullx",
      "True",
      "01",
      "+1",
      ".5",
      "1.",
      "-",
      "NaN",
      "Infinity",
      "'a'",
      "abc",
      "\"\\x\"",
      "\"\\u12\"",
      "{1:2}",
      ":",
      ",",
  };
  for (const char* text : texts) {
    SCOPED_TRACE(text);
    ExpectFails(text, Status::ParseError);
  }
}

TEST(FlattenJson, TruncatedJsonIsParseError) {
  const char* const texts[] = {
      "{\"a\":1", "{\"a\":", "{\"a\"",           "{\"a",           "[1,2", "[1,", "{\"a\":[1,2",
      "\"abc",    "[\"abc",  "{\"a\":{\"b\":1}", "[[[[1,2],[3]]]",
  };
  for (const char* text : texts) {
    SCOPED_TRACE(text);
    ExpectFails(text, Status::ParseError);
  }
}

TEST(FlattenJson, TrailingGarbageIsParseError) {
  const char* const texts[] = {
      "{} x",  "1 2",         "[]]",  "{\"a\":1}}", "true false",
      "[1] ,", "\"a\" \"b\"", "{}{}", "[][]",       "null null",
  };
  for (const char* text : texts) {
    SCOPED_TRACE(text);
    ExpectFails(text, Status::ParseError);
  }
}

TEST(FlattenJson, InvalidUtf8InAStringValueIsParseError) {
  const std::string texts[] = {
      "\"\xC3\x28\"",     "\"\xFF\"",
      "\"\xC0\x80\"",     "\"\xE2\x82\"",
      "\"\xED\xA0\x80\"", "\"\xF8\x88\x80\x80\x80\"",
      "\"\x80\"",         "[\"ok\",\"\xC3\x28\"]",
  };
  for (const std::string& text : texts) {
    SCOPED_TRACE(text);
    ExpectFails(text, Status::ParseError);
  }
}

TEST(FlattenJson, InvalidUtf8InAKeyIsParseError) {
  ExpectFails("{\"\xC3\x28\":1}", Status::ParseError);
  ExpectFails("{\"a\xFF\":1}", Status::ParseError);
}

TEST(FlattenJson, InvalidUtf8OutsideStringsIsParseError) {
  ExpectFails("\xFF", Status::ParseError);
  ExpectFails("\xC3\x28", Status::ParseError);
  ExpectFails("[1,\xFF]", Status::ParseError);
}

TEST(FlattenJson, UnpairedSurrogateEscapesAreParseError) {
  const char* const texts[] = {
      R"("\ud800")",  R"("\udc00")",       R"("\ud83d")",
      R"("\ud800x")", R"("\ud800\u0041")", R"({"\ud800":1})",
  };
  for (const char* text : texts) {
    SCOPED_TRACE(text);
    ExpectFails(text, Status::ParseError);
  }
}

TEST(FlattenJson, FormFeedAndVerticalTabOutsideStringsAreParseError) {
  const std::string texts[] = {"\f[1]", "[1]\f", "[\v1]", "\v[1]", "[1,\f2]"};
  for (const std::string& text : texts) {
    SCOPED_TRACE(text);
    ExpectFails(text, Status::ParseError);
  }
}

TEST(FlattenJson, CommentsAreParseError) {
  ExpectFails("/* c */ [1]", Status::ParseError);
  ExpectFails("[1] // c", Status::ParseError);
  ExpectFails("[1, /* c */ 2]", Status::ParseError);
}

TEST(FlattenJson, NulBeforeOrInsideTheDocumentIsParseError) {
  ExpectFails(std::string("\0[1]", 4), Status::ParseError);
  ExpectFails(std::string("[1,\0 2]", 7), Status::ParseError);
  ExpectFails(std::string("\0", 1), Status::ParseError);
}

TEST(FlattenJson, UnescapedControlCharactersInStringsAreParseError) {
  const std::string texts[] = {
      "\"a\tb\"",     "\"a\nb\"",         "\"a\rb\"",
      "\"\x01\"",     "\"\x1f\"",         std::string("\"a\0b\"", 5),
      "{\"k\nk\":1}", "{\"k\":\"v\nv\"}",
  };
  for (const std::string& text : texts) {
    SCOPED_TRACE(text);
    ExpectFails(text, Status::ParseError);
  }
}

TEST(FlattenJson, NumberTooLargeForADoubleIsParseError) {
  ExpectFails("1e999", Status::ParseError);
  ExpectFails("-1e999", Status::ParseError);
  ExpectFails("[1,1e999]", Status::ParseError);
  ExpectFails(R"({"a":1e999})", Status::ParseError);
}

TEST(FlattenJson, ErrorLeavesDocumentAndEntriesUntouchedAfterPartialProgress) {
  FlattenResult result = MakeSentinel();
  EXPECT_EQ(FlattenJson(R"({"a":[1,2,3],"b":{"c":4},"d":)", result), Status::ParseError);
  ExpectSentinel(result);
}

// ---------------------------------------------------------------------------
// Limits: input size.
// ---------------------------------------------------------------------------

TEST(FlattenJson, InputOfExactlyTheMaximumSizeIsAccepted) {
  const std::string text = PaddedToSize("[0]", kMaxJsonInputBytes);
  ASSERT_EQ(text.size(), kMaxJsonInputBytes);
  FlattenResult result;
  ASSERT_EQ(FlattenJson(text, result), Status::Ok);
  ExpectEntries(result, {{"/0", "0", JsonEntryType::Number}});
}

TEST(FlattenJson, StringFillingTheMaximumSizeIsAccepted) {
  const std::string text = "\"" + std::string(kMaxJsonInputBytes - 2, 'a') + "\"";
  ASSERT_EQ(text.size(), kMaxJsonInputBytes);
  FlattenResult result;
  ASSERT_EQ(FlattenJson(text, result), Status::Ok);
  ASSERT_EQ(result.entries.size(), 1u);
  EXPECT_EQ(result.entries[0].key, "");
  EXPECT_EQ(result.entries[0].value.size(), kMaxJsonInputBytes - 2);
}

TEST(FlattenJson, InputOneByteOverTheMaximumIsDocumentTooLarge) {
  ExpectFails(PaddedToSize("[0]", kMaxJsonInputBytes + 1), Status::DocumentTooLarge);
}

TEST(FlattenJson, ValidStringOneByteOverTheMaximumIsDocumentTooLarge) {
  const std::string text = "\"" + std::string(kMaxJsonInputBytes - 1, 'a') + "\"";
  ASSERT_EQ(text.size(), kMaxJsonInputBytes + 1);
  ExpectFails(text, Status::DocumentTooLarge);
}

TEST(FlattenJson, OversizedInputIsDocumentTooLargeEvenWhenNotJson) {
  ExpectFails(std::string(kMaxJsonInputBytes + 1, 'x'), Status::DocumentTooLarge);
}

TEST(FlattenJson, NulAsTheLastByteOfAMaximumSizeInputIsParseErrorAndOneMoreByteIsTooLarge) {
  std::string text = PaddedToSize("[0]", kMaxJsonInputBytes - 1);
  text.push_back('\0');
  ASSERT_EQ(text.size(), kMaxJsonInputBytes);
  ExpectFails(text, Status::ParseError);

  text = PaddedToSize("[0]", kMaxJsonInputBytes);
  text.push_back('\0');
  ExpectFails(text, Status::DocumentTooLarge);
}

TEST(FlattenJson, ByteOrderMarkCountsTowardsTheMaximumSize) {
  const std::string bom("\xEF\xBB\xBF");
  const std::string atLimit = bom + PaddedToSize("[0]", kMaxJsonInputBytes - bom.size());
  ASSERT_EQ(atLimit.size(), kMaxJsonInputBytes);
  FlattenResult result;
  ASSERT_EQ(FlattenJson(atLimit, result), Status::Ok);
  ExpectEntries(result, {{"/0", "0", JsonEntryType::Number}});

  ExpectFails(bom + PaddedToSize("[0]", kMaxJsonInputBytes - bom.size() + 1),
              Status::DocumentTooLarge);
}

// ---------------------------------------------------------------------------
// Limits: nesting depth (root container = depth 1).
// ---------------------------------------------------------------------------

TEST(FlattenJson, NestedArraysAtTheMaximumDepthAreAccepted) {
  FlattenResult result;
  ASSERT_EQ(FlattenJson(NestedArrays(kMaxJsonDepth), result), Status::Ok);
  ASSERT_EQ(result.entries.size(), 1u);
  EXPECT_EQ(result.entries[0].key, Repeated("/0", kMaxJsonDepth - 1));
  EXPECT_EQ(result.entries[0].value, "[]");
  EXPECT_EQ(TypeNumber(result.entries[0].type), TypeNumber(JsonEntryType::EmptyArray));
}

TEST(FlattenJson, NestedArraysOneLevelOverTheMaximumAreNestingTooDeep) {
  ExpectFails(NestedArrays(kMaxJsonDepth + 1), Status::NestingTooDeep);
}

TEST(FlattenJson, NestedObjectsAtTheMaximumDepthAreAccepted) {
  FlattenResult result;
  ASSERT_EQ(FlattenJson(NestedObjects(kMaxJsonDepth), result), Status::Ok);
  ASSERT_EQ(result.entries.size(), 1u);
  EXPECT_EQ(result.entries[0].key, Repeated("/a", kMaxJsonDepth - 1));
  EXPECT_EQ(result.entries[0].value, "{}");
  EXPECT_EQ(TypeNumber(result.entries[0].type), TypeNumber(JsonEntryType::EmptyObject));
}

TEST(FlattenJson, NestedObjectsOneLevelOverTheMaximumAreNestingTooDeep) {
  ExpectFails(NestedObjects(kMaxJsonDepth + 1), Status::NestingTooDeep);
}

TEST(FlattenJson, LeafInsideTheDeepestAllowedContainerIsAccepted) {
  const std::string text = std::string(kMaxJsonDepth, '[') + "1" + std::string(kMaxJsonDepth, ']');
  FlattenResult result;
  ASSERT_EQ(FlattenJson(text, result), Status::Ok);
  ASSERT_EQ(result.entries.size(), 1u);
  EXPECT_EQ(result.entries[0].key, Repeated("/0", kMaxJsonDepth));
  EXPECT_EQ(result.entries[0].value, "1");
}

TEST(FlattenJson, LeafInsideAContainerOverTheMaximumDepthIsNestingTooDeep) {
  const std::string text =
      std::string(kMaxJsonDepth + 1, '[') + "1" + std::string(kMaxJsonDepth + 1, ']');
  ExpectFails(text, Status::NestingTooDeep);
}

TEST(FlattenJson, MixedObjectAndArrayNestingIsCountedTogether) {
  std::string atLimit;
  for (std::size_t i = 0; i < kMaxJsonDepth / 2; ++i) {
    atLimit += "[{\"a\":";
  }
  atLimit += "1";
  for (std::size_t i = 0; i < kMaxJsonDepth / 2; ++i) {
    atLimit += "}]";
  }
  FlattenResult result;
  ASSERT_EQ(FlattenJson(atLimit, result), Status::Ok);
  ASSERT_EQ(result.entries.size(), 1u);
  EXPECT_EQ(result.entries[0].key, Repeated("/0/a", kMaxJsonDepth / 2));

  ExpectFails("[" + atLimit + "]", Status::NestingTooDeep);
}

TEST(FlattenJson, SiblingContainersDoNotAccumulateDepth) {
  const std::string text = FlatArrayOf(NestedArrays(kMaxJsonDepth - 1), 3);
  FlattenResult result;
  ASSERT_EQ(FlattenJson(text, result), Status::Ok);
  EXPECT_EQ(result.entries.size(), 3u);
}

TEST(FlattenJson, SiblingContainersOverTheMaximumDepthAreNestingTooDeep) {
  ExpectFails(FlatArrayOf(NestedArrays(kMaxJsonDepth), 3), Status::NestingTooDeep);
}

TEST(FlattenJson, UnterminatedDeepNestingIsNestingTooDeepWithoutCrashing) {
  ExpectFails(std::string(kMaxJsonDepth + 1, '['), Status::NestingTooDeep);
  ExpectFails(std::string(100000, '['), Status::NestingTooDeep);
  ExpectFails(Repeated("{\"a\":", 100000), Status::NestingTooDeep);
}

TEST(FlattenJson, TooDeepAndMalformedFurtherOnReportsNestingTooDeep) {
  ExpectFails(NestedArrays(kMaxJsonDepth + 1) + " garbage", Status::NestingTooDeep);
}

TEST(FlattenJson, MoreThanTheMaximumLeavesBeforeTooDeepNestingIsTooManyEntries) {
  const std::string text =
      "[" + Repeated("0,", kMaxFlatEntries + 1) + NestedArrays(kMaxJsonDepth + 1) + "]";
  ExpectFails(text, Status::TooManyEntries);
}

TEST(FlattenJson, TooDeepNestingBeforeMoreThanTheMaximumLeavesIsNestingTooDeep) {
  const std::string text =
      "[" + NestedArrays(kMaxJsonDepth + 1) + "," + Repeated("0,", kMaxFlatEntries + 1) + "0]";
  ExpectFails(text, Status::NestingTooDeep);
}

TEST(FlattenJson, DepthLimitIsNotReportedForShallowMalformedInput) {
  ExpectFails(std::string(kMaxJsonDepth, '['), Status::ParseError);
}

// ---------------------------------------------------------------------------
// Limits: entry count. Leaves are counted as they appear in the text.
// ---------------------------------------------------------------------------

TEST(FlattenJson, FlatArrayOfTheMaximumEntryCountIsAccepted) {
  FlattenResult result;
  ASSERT_EQ(FlattenJson(FlatArrayOf("0", kMaxFlatEntries), result), Status::Ok);
  ASSERT_EQ(result.entries.size(), kMaxFlatEntries);
  EXPECT_EQ(result.entries.front().key, "/0");
  EXPECT_EQ(result.entries.back().key, "/" + std::to_string(kMaxFlatEntries - 1));
}

TEST(FlattenJson, FlatArrayOneEntryOverTheMaximumIsTooManyEntries) {
  ExpectFails(FlatArrayOf("0", kMaxFlatEntries + 1), Status::TooManyEntries);
}

TEST(FlattenJson, FlatObjectOfTheMaximumEntryCountIsAccepted) {
  FlattenResult result;
  ASSERT_EQ(FlattenJson(FlatObjectUniqueKeys(kMaxFlatEntries), result), Status::Ok);
  ASSERT_EQ(result.entries.size(), kMaxFlatEntries);
  EXPECT_EQ(result.entries.front().key, "/k0");
  EXPECT_EQ(result.entries.back().key, "/k" + std::to_string(kMaxFlatEntries - 1));
}

TEST(FlattenJson, FlatObjectOneEntryOverTheMaximumIsTooManyEntries) {
  ExpectFails(FlatObjectUniqueKeys(kMaxFlatEntries + 1), Status::TooManyEntries);
}

TEST(FlattenJson, EmptyContainersCountAsOneEntryEach) {
  FlattenResult result;
  ASSERT_EQ(FlattenJson(FlatArrayOf("{}", kMaxFlatEntries), result), Status::Ok);
  EXPECT_EQ(result.entries.size(), kMaxFlatEntries);
  ASSERT_EQ(FlattenJson(FlatArrayOf("[]", kMaxFlatEntries), result), Status::Ok);
  EXPECT_EQ(result.entries.size(), kMaxFlatEntries);

  ExpectFails(FlatArrayOf("{}", kMaxFlatEntries + 1), Status::TooManyEntries);
  ExpectFails(FlatArrayOf("[]", kMaxFlatEntries + 1), Status::TooManyEntries);
}

TEST(FlattenJson, EmptyContainersAndLeavesShareOneCount) {
  const std::string half =
      Repeated("0,", kMaxFlatEntries / 2) + Repeated("{},", kMaxFlatEntries / 2);
  ExpectFails("[" + half + "{}]", Status::TooManyEntries);

  FlattenResult result;
  ASSERT_EQ(FlattenJson("[" + half.substr(0, half.size() - 1) + "]", result), Status::Ok);
  EXPECT_EQ(result.entries.size(), kMaxFlatEntries);
}

TEST(FlattenJson, NonEmptyContainersDoNotCountAgainstTheEntryLimit) {
  const std::string text = "{\"a\":" + FlatArrayOf("0", kMaxFlatEntries) + "}";
  FlattenResult result;
  ASSERT_EQ(FlattenJson(text, result), Status::Ok);
  EXPECT_EQ(result.entries.size(), kMaxFlatEntries);
  EXPECT_EQ(result.entries.front().key, "/a/0");
}

TEST(FlattenJson, NestedLeavesCountTowardsTheEntryLimit) {
  const std::string over = "[" + FlatArrayOf("0", kMaxFlatEntries / 2) + "," +
                           FlatArrayOf("0", kMaxFlatEntries / 2) + ",0]";
  ExpectFails(over, Status::TooManyEntries);
  const std::string at = "[" + FlatArrayOf("0", kMaxFlatEntries / 2) + "," +
                         FlatArrayOf("0", kMaxFlatEntries / 2) + "]";
  FlattenResult result;
  ASSERT_EQ(FlattenJson(at, result), Status::Ok);
  EXPECT_EQ(result.entries.size(), kMaxFlatEntries);
}

TEST(FlattenJson, RepeatedKeyOccurrencesEachCountTowardsTheEntryLimit) {
  FlattenResult result;
  ASSERT_EQ(FlattenJson(ObjectRepeatingKey(kMaxFlatEntries), result), Status::Ok);
  ExpectEntries(result, {{"/a", "0", JsonEntryType::Number}});

  ExpectFails(ObjectRepeatingKey(kMaxFlatEntries + 1), Status::TooManyEntries);
}

TEST(FlattenJson, MoreThanTheMaximumLeavesIsRejectedEvenIfDuplicatesWouldReduceTheList) {
  std::string text = "{";
  for (std::size_t i = 0; i < kMaxFlatEntries; ++i) {
    text += "\"k" + std::to_string(i % 10) + "\":0,";
  }
  text += "\"k0\":0}";
  ExpectFails(text, Status::TooManyEntries);
}

TEST(FlattenJson, TooManyEntriesIsReportedBeforeLaterMalformedText) {
  const std::string text = "[" + Repeated("0,", kMaxFlatEntries + 1) + "garbage";
  ExpectFails(text, Status::TooManyEntries);
}

TEST(FlattenJson, EntryLimitIsNotReportedForAShortMalformedText) {
  ExpectFails("[0,0,garbage", Status::ParseError);
}

// ---------------------------------------------------------------------------
// Limits: total key text. Every leaf and empty container counts the byte length of its entry key,
// per occurrence in the text, after ~0/~1 escaping.
// ---------------------------------------------------------------------------

TEST(FlattenJson, KeyTextExactlyAtTheLimitIsAcceptedAndMatchesTheStoredKeyBytes) {
  EXPECT_EQ(DigitSumBelow(kBudgetLeafCount), kBudgetDigitSum);
  const std::string text = KeyBudgetDocument(std::string(kBudgetTailKeyLength, 'b'));
  ASSERT_LE(text.size(), kMaxJsonInputBytes);
  FlattenResult result;
  ASSERT_EQ(FlattenJson(text, result), Status::Ok);
  ASSERT_EQ(result.entries.size(), kBudgetLeafCount + 1);
  EXPECT_EQ(result.entries[0].key.size(), 1 + kBudgetLongKeyLength + 1 + 1);
  EXPECT_EQ(result.entries.back().key.size(), 1 + kBudgetTailKeyLength);
  EXPECT_EQ(TotalKeyBytes(result), kMaxFlatKeyBytes);
}

TEST(FlattenJson, KeyTextOneByteOverTheLimitIsKeyTextTooLarge) {
  ExpectFails(KeyBudgetDocument(std::string(kBudgetTailKeyLength + 1, 'b')),
              Status::KeyTextTooLarge);
}

TEST(FlattenJson, TildeInAKeyCountsTwoBytesTowardsTheKeyTextLimit) {
  // "/" + 41706 * "~0" + "x" is 83414 bytes, the same as "/" + 83413 * 'b'.
  ExpectKeyBudgetAtTheLimit(KeyBudgetDocument(std::string(41706, '~') + "x"), kBudgetLeafCount + 1);
  ExpectFails(KeyBudgetDocument(std::string(41707, '~') + "x"), Status::KeyTextTooLarge);
}

TEST(FlattenJson, SlashInAKeyCountsTwoBytesTowardsTheKeyTextLimit) {
  ExpectKeyBudgetAtTheLimit(KeyBudgetDocument(std::string(41706, '/') + "x"), kBudgetLeafCount + 1);
  ExpectFails(KeyBudgetDocument(std::string(41707, '/') + "x"), Status::KeyTextTooLarge);
}

TEST(FlattenJson, EmptyContainerKeysCountTowardsTheKeyTextLimit) {
  const std::string atLimit(kBudgetTailKeyLength, 'b');
  const std::string overLimit(kBudgetTailKeyLength + 1, 'b');
  for (const char* const empty : {"{}", "[]"}) {
    SCOPED_TRACE(empty);
    ExpectKeyBudgetAtTheLimit(KeyBudgetDocument(atLimit, empty), kBudgetLeafCount + 1);
    ExpectFails(KeyBudgetDocument(overLimit, empty), Status::KeyTextTooLarge);
  }
}

TEST(FlattenJson, RepeatedKeyOccurrencesEachCountTowardsTheKeyTextLimit) {
  // One occurrence is 2100 * 1024 + 7290 = 2157690 bytes; two are 4315380, past the limit even
  // though the duplicate collapse leaves only 2100 entries.
  const std::string member = ZeroArrayMember(std::string(kBudgetLongKeyLength, 'a'), 2100);
  EXPECT_EQ(DigitSumBelow(2100), 7290u);
  ASSERT_LT(2100 * (1 + kBudgetLongKeyLength + 1) + DigitSumBelow(2100), kMaxFlatKeyBytes);
  ASSERT_GT(2 * (2100 * (1 + kBudgetLongKeyLength + 1) + DigitSumBelow(2100)), kMaxFlatKeyBytes);

  FlattenResult result;
  ASSERT_EQ(FlattenJson("{" + member + "}", result), Status::Ok);
  EXPECT_EQ(result.entries.size(), 2100u);

  ExpectFails("{" + member + "," + member + "}", Status::KeyTextTooLarge);
}

TEST(FlattenJson, DeepLongKeyAmplificationIsKeyTextTooLargeNotInternalError) {
  constexpr std::size_t kLevels = 63;
  constexpr std::size_t kLevelKeyLength = 15000;
  constexpr std::size_t kLeaves = 9000;
  std::string text;
  const std::string key(kLevelKeyLength, 'k');
  for (std::size_t i = 0; i < kLevels; ++i) {
    text += "{\"" + key + "\":";
  }
  text += FlatArrayOf("0", kLeaves);
  text.append(kLevels, '}');

  ASSERT_LE(text.size(), kMaxJsonInputBytes);
  ASSERT_EQ(kLevels + 1, kMaxJsonDepth);
  ASSERT_LE(kLeaves, kMaxFlatEntries);
  // Each leaf key is over 945000 bytes, so the limit is crossed within the first five leaves.
  ASSERT_GT(kLevels * (kLevelKeyLength + 1), kMaxFlatKeyBytes / 5);

  ExpectFails(text, Status::KeyTextTooLarge);
}

TEST(FlattenJson, EntryLimitIsReportedBeforeKeyTextLimitOnTheSameLeaf) {
  // The first 10000 leaves "/<400 a's>/<i>" total 10000 * 402 + 38890 = 4058890 bytes, under the
  // limit; the 10001st leaf "/<200000 b's>" is both the entry past the cap and the key past the
  // limit.
  const std::string text =
      "{" + ZeroArrayMember(std::string(400, 'a'), kMaxFlatEntries) + ",\"" +
      std::string(200000, 'b') + "\":0}";
  const std::size_t keyBytesBeforeTheLastLeaf =
      kMaxFlatEntries * (1 + 400 + 1) + DigitSumBelow(kMaxFlatEntries);
  ASSERT_EQ(keyBytesBeforeTheLastLeaf, 4058890u);
  ASSERT_LT(keyBytesBeforeTheLastLeaf, kMaxFlatKeyBytes);
  ASSERT_GT(keyBytesBeforeTheLastLeaf + 1 + 200000, kMaxFlatKeyBytes);
  ASSERT_LE(text.size(), kMaxJsonInputBytes);

  ExpectFails(text, Status::TooManyEntries);
}

TEST(FlattenJson, KeyTextLimitIsReportedBeforeLaterExcessiveDepth) {
  const std::string text = KeyBudgetPrefix(std::string(kBudgetTailKeyLength + 1, 'b'), "0") +
                           ",\"C\":" + NestedArrays(kMaxJsonDepth + 6) + "}";
  ExpectFails(text, Status::KeyTextTooLarge);
}

TEST(FlattenJson, TooDeepContainerBeforeTheKeyTextLimitIsNestingTooDeep) {
  const std::string text = "{\"C\":" + NestedArrays(kMaxJsonDepth + 6) + "," +
                           KeyBudgetPrefix(std::string(kBudgetTailKeyLength + 1, 'b'), "0").substr(1) +
                           "}";
  ExpectFails(text, Status::NestingTooDeep);
}

TEST(FlattenJson, KeyTextTooLargeIsReportedBeforeLaterMalformedText) {
  const std::string text =
      KeyBudgetPrefix(std::string(kBudgetTailKeyLength + 1, 'b'), "0") + ",\"C\":garbage";
  ExpectFails(text, Status::KeyTextTooLarge);
}

TEST(FlattenJson, KeyTextLimitIsNotReportedForAShortMalformedText) {
  ExpectFails(KeyBudgetPrefix(std::string(kBudgetTailKeyLength, 'b'), "0") + ",\"C\":garbage",
              Status::ParseError);
}

// ---------------------------------------------------------------------------
// DescribeLeaf.
// ---------------------------------------------------------------------------

TEST(DescribeLeaf, NonEmptyObjectAndArrayAreTypeMismatchAndLeaveTheOutputsUntouched) {
  const JsonValue nodes[] = {JsonValue::parse(R"({"a":1})"), JsonValue::parse("[1]"),
                             JsonValue::parse(R"({"a":{}})"), JsonValue::parse("[[]]")};
  for (const JsonValue& node : nodes) {
    SCOPED_TRACE(node.dump());
    std::string text = "sentinel";
    JsonEntryType type = kNoType;
    EXPECT_EQ(DescribeLeaf(node, text, type), Status::TypeMismatch);
    EXPECT_EQ(text, "sentinel");
    EXPECT_EQ(TypeNumber(type), TypeNumber(kNoType));
  }
}

TEST(DescribeLeaf, EmptyObjectAndEmptyArrayAreLeaves) {
  std::string text = "sentinel";
  JsonEntryType type = kNoType;
  ASSERT_EQ(DescribeLeaf(JsonValue::object(), text, type), Status::Ok);
  EXPECT_EQ(text, "{}");
  EXPECT_EQ(TypeNumber(type), TypeNumber(JsonEntryType::EmptyObject));

  text = "sentinel";
  type = kNoType;
  ASSERT_EQ(DescribeLeaf(JsonValue::array(), text, type), Status::Ok);
  EXPECT_EQ(text, "[]");
  EXPECT_EQ(TypeNumber(type), TypeNumber(JsonEntryType::EmptyArray));
}

TEST(DescribeLeaf, ScalarsGetTheirTextAndType) {
  struct Case {
    JsonValue node;
    const char* text;
    JsonEntryType type;
  };
  const Case cases[] = {
      {JsonValue(42), "42", JsonEntryType::Number},
      {JsonValue(-7), "-7", JsonEntryType::Number},
      {JsonValue(2.5), "2.5", JsonEntryType::Number},
      {JsonValue("text"), "text", JsonEntryType::String},
      {JsonValue(""), "", JsonEntryType::String},
      {JsonValue(true), "true", JsonEntryType::Bool},
      {JsonValue(false), "false", JsonEntryType::Bool},
      {JsonValue(nullptr), "null", JsonEntryType::Null},
  };
  for (const Case& c : cases) {
    SCOPED_TRACE(c.text);
    std::string text = "sentinel";
    JsonEntryType type = kNoType;
    EXPECT_EQ(DescribeLeaf(c.node, text, type), Status::Ok);
    EXPECT_EQ(text, c.text);
    EXPECT_EQ(TypeNumber(type), TypeNumber(c.type));
  }
}
