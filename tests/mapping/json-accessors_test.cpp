// Coverage for src/mapping/json-accessors.*, called directly on a parsed JsonValue.

#include "mapping/json-accessors.h"

#include <clocale>
#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "core/json-value.h"
#include "core/status.h"
#include "../test-support/status-print.h"
#include "mapping/json-flatten.h"

namespace {

constexpr std::int32_t kSentinelLong = -999;
constexpr double kSentinelDouble = -999.5;
constexpr std::uint32_t kSentinelCount = 0xC0FFEE11u;

JsonValue Parse(const std::string& text) {
  return JsonValue::parse(text);
}

Status Long(const std::string& json, const std::string& path, std::int32_t& out) {
  return ReadLongAt(Parse(json), path, out);
}

Status Double(const std::string& json, const std::string& path, double& out) {
  return ReadDoubleAt(Parse(json), path, out);
}

Status Bool(const std::string& json, const std::string& path, bool& out) {
  return ReadBoolAt(Parse(json), path, out);
}

Status Count(const std::string& json, const std::string& path, std::uint32_t& out) {
  return CountElementsAt(Parse(json), path, out);
}

void ExpectLongFails(const std::string& json, const std::string& path, Status expected) {
  SCOPED_TRACE(json + " @ " + path);
  std::int32_t out = kSentinelLong;
  EXPECT_EQ(Long(json, path, out), expected);
  EXPECT_EQ(out, kSentinelLong);
}

void ExpectLongOk(const std::string& json, std::int32_t expected) {
  SCOPED_TRACE(json);
  std::int32_t out = kSentinelLong;
  EXPECT_EQ(Long(json, "/v", out), Status::Ok);
  EXPECT_EQ(out, expected);
}

void ExpectDoubleFails(const std::string& json, const std::string& path, Status expected) {
  SCOPED_TRACE(json + " @ " + path);
  double out = kSentinelDouble;
  EXPECT_EQ(Double(json, path, out), expected);
  EXPECT_EQ(out, kSentinelDouble);
}

void ExpectBoolFails(const std::string& json, const std::string& path, Status expected) {
  SCOPED_TRACE(json + " @ " + path);
  bool out = true;
  EXPECT_EQ(Bool(json, path, out), expected);
  EXPECT_TRUE(out);
}

void ExpectCountFails(const std::string& json, const std::string& path, Status expected) {
  SCOPED_TRACE(json + " @ " + path);
  std::uint32_t out = kSentinelCount;
  EXPECT_EQ(Count(json, path, out), expected);
  EXPECT_EQ(out, kSentinelCount);
}

std::uint32_t CountOk(const std::string& json, const std::string& path) {
  SCOPED_TRACE(json + " @ " + path);
  std::uint32_t out = kSentinelCount;
  EXPECT_EQ(Count(json, path, out), Status::Ok);
  return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// ReadLongAt
// ---------------------------------------------------------------------------

TEST(ReadLongAt, IntegerIsRead) {
  ExpectLongOk(R"({"v":42})", 42);
  ExpectLongOk(R"({"v":-7})", -7);
  ExpectLongOk(R"({"v":0})", 0);
}

TEST(ReadLongAt, WholeValuedFloatIsRead) {
  ExpectLongOk(R"({"v":5.0})", 5);
  ExpectLongOk(R"({"v":-3.0})", -3);
  ExpectLongOk(R"({"v":1e2})", 100);
}

TEST(ReadLongAt, FractionalNumberIsNotIntegralAndNeverTruncated) {
  ExpectLongFails(R"({"v":3.7})", "/v", Status::NotIntegral);
  ExpectLongFails(R"({"v":-0.5})", "/v", Status::NotIntegral);
}

TEST(ReadLongAt, Int32BoundsAreAcceptedAndOneBeyondIsNumericOverflow) {
  ExpectLongOk(R"({"v":2147483647})", INT32_MAX);
  ExpectLongOk(R"({"v":-2147483648})", INT32_MIN);
  ExpectLongFails(R"({"v":2147483648})", "/v", Status::NumericOverflow);
  ExpectLongFails(R"({"v":-2147483649})", "/v", Status::NumericOverflow);
}

TEST(ReadLongAt, IntegerBeyond64BitsIsNumericOverflow) {
  ExpectLongFails(R"({"v":123456789012345678901234567890})", "/v", Status::NumericOverflow);
  ExpectLongFails(R"({"v":-123456789012345678901234567890})", "/v", Status::NumericOverflow);
}

TEST(ReadLongAt, NumericStringIsTypeMismatchNotCoerced) {
  ExpectLongFails(R"({"v":"42"})", "/v", Status::TypeMismatch);
}

TEST(ReadLongAt, BoolIsTypeMismatch) {
  ExpectLongFails(R"({"v":true})", "/v", Status::TypeMismatch);
  ExpectLongFails(R"({"v":false})", "/v", Status::TypeMismatch);
}

TEST(ReadLongAt, NullIsNullValue) { ExpectLongFails(R"({"v":null})", "/v", Status::NullValue); }

TEST(ReadLongAt, ContainerIsTypeMismatch) {
  ExpectLongFails(R"({"v":[1]})", "/v", Status::TypeMismatch);
  ExpectLongFails(R"({"v":{"a":1}})", "/v", Status::TypeMismatch);
  ExpectLongFails(R"({"v":[]})", "/v", Status::TypeMismatch);
}

TEST(ReadLongAt, RootScalarIsReadThroughTheEmptyPath) {
  std::int32_t out = kSentinelLong;
  EXPECT_EQ(Long("17", "", out), Status::Ok);
  EXPECT_EQ(out, 17);
}

TEST(ReadLongAt, ArrayElementIsReadByIndex) {
  std::int32_t out = kSentinelLong;
  EXPECT_EQ(Long(R"({"a":[10,20,30]})", "/a/2", out), Status::Ok);
  EXPECT_EQ(out, 30);
}

// ---------------------------------------------------------------------------
// ReadDoubleAt
// ---------------------------------------------------------------------------

TEST(ReadDoubleAt, IntegerAndFloatAreRead) {
  double out = kSentinelDouble;
  EXPECT_EQ(Double(R"({"v":7})", "/v", out), Status::Ok);
  EXPECT_DOUBLE_EQ(out, 7.0);
  EXPECT_EQ(Double(R"({"v":-2.25})", "/v", out), Status::Ok);
  EXPECT_DOUBLE_EQ(out, -2.25);
  EXPECT_EQ(Double(R"({"v":0})", "/v", out), Status::Ok);
  EXPECT_DOUBLE_EQ(out, 0.0);
}

TEST(ReadDoubleAt, ExponentFormIsRead) {
  double out = kSentinelDouble;
  EXPECT_EQ(Double(R"({"v":1.5e3})", "/v", out), Status::Ok);
  EXPECT_DOUBLE_EQ(out, 1500.0);
  EXPECT_EQ(Double(R"({"v":2E-2})", "/v", out), Status::Ok);
  EXPECT_DOUBLE_EQ(out, 0.02);
}

TEST(ReadDoubleAt, LargeIntegerBeyondInt32IsStillADouble) {
  double out = kSentinelDouble;
  EXPECT_EQ(Double(R"({"v":4294967296})", "/v", out), Status::Ok);
  EXPECT_DOUBLE_EQ(out, 4294967296.0);
}

TEST(ReadDoubleAt, NumericStringIsTypeMismatchNotCoerced) {
  ExpectDoubleFails(R"({"v":"1.5"})", "/v", Status::TypeMismatch);
}

TEST(ReadDoubleAt, BoolAndContainerAreTypeMismatch) {
  ExpectDoubleFails(R"({"v":true})", "/v", Status::TypeMismatch);
  ExpectDoubleFails(R"({"v":[1.5]})", "/v", Status::TypeMismatch);
  ExpectDoubleFails(R"({"v":{}})", "/v", Status::TypeMismatch);
}

TEST(ReadDoubleAt, NullIsNullValue) {
  ExpectDoubleFails(R"({"v":null})", "/v", Status::NullValue);
}

TEST(ReadDoubleAt, OverflowingExponentIsRejectedAtParseSoNoNonFiniteValueExists) {
  FlattenResult result;
  EXPECT_EQ(FlattenJson(R"({"v":1e400})", result), Status::ParseError);
  EXPECT_EQ(FlattenJson(R"({"v":-1e400})", result), Status::ParseError);
}

// ---------------------------------------------------------------------------
// ReadBoolAt
// ---------------------------------------------------------------------------

TEST(ReadBoolAt, TrueAndFalseAreRead) {
  bool out = false;
  EXPECT_EQ(Bool(R"({"v":true})", "/v", out), Status::Ok);
  EXPECT_TRUE(out);
  EXPECT_EQ(Bool(R"({"v":false})", "/v", out), Status::Ok);
  EXPECT_FALSE(out);
}

TEST(ReadBoolAt, NumericZeroAndOneAreTypeMismatch) {
  ExpectBoolFails(R"({"v":0})", "/v", Status::TypeMismatch);
  ExpectBoolFails(R"({"v":1})", "/v", Status::TypeMismatch);
  ExpectBoolFails(R"({"v":1.0})", "/v", Status::TypeMismatch);
}

TEST(ReadBoolAt, StringAndContainerAreTypeMismatch) {
  ExpectBoolFails(R"({"v":"true"})", "/v", Status::TypeMismatch);
  ExpectBoolFails(R"({"v":[true]})", "/v", Status::TypeMismatch);
}

TEST(ReadBoolAt, NullIsNullValue) { ExpectBoolFails(R"({"v":null})", "/v", Status::NullValue); }

// ---------------------------------------------------------------------------
// CountElementsAt
// ---------------------------------------------------------------------------

TEST(CountElementsAt, EmptyArrayIsZero) {
  EXPECT_EQ(CountOk(R"({"v":[]})", "/v"), 0u);
}

TEST(CountElementsAt, ArrayLengthIsCountedNotItsDescendants) {
  EXPECT_EQ(CountOk(R"({"v":[1,2,3]})", "/v"), 3u);
  EXPECT_EQ(CountOk(R"({"v":[[1,2],[3],[]]})", "/v"), 3u);
}

TEST(CountElementsAt, NestedArrayIsCountedThroughItsPath) {
  EXPECT_EQ(CountOk(R"({"v":[[1,2],[3,4,5]]})", "/v/1"), 3u);
  EXPECT_EQ(CountOk(R"({"a":{"b":[null,null]}})", "/a/b"), 2u);
}

TEST(CountElementsAt, RootArrayIsCountedThroughTheEmptyPath) {
  EXPECT_EQ(CountOk("[1,2,3,4]", ""), 4u);
  EXPECT_EQ(CountOk("[]", ""), 0u);
}

TEST(CountElementsAt, ObjectIsTypeMismatch) {
  ExpectCountFails(R"({"v":{"a":1,"b":2}})", "/v", Status::TypeMismatch);
  ExpectCountFails(R"({"v":{}})", "/v", Status::TypeMismatch);
  ExpectCountFails(R"({"a":1})", "", Status::TypeMismatch);
}

TEST(CountElementsAt, ScalarIsTypeMismatch) {
  ExpectCountFails(R"({"v":5})", "/v", Status::TypeMismatch);
  ExpectCountFails(R"({"v":"abc"})", "/v", Status::TypeMismatch);
  ExpectCountFails(R"({"v":true})", "/v", Status::TypeMismatch);
  ExpectCountFails("7", "", Status::TypeMismatch);
}

TEST(CountElementsAt, NullIsNullValue) {
  ExpectCountFails(R"({"v":null})", "/v", Status::NullValue);
  ExpectCountFails("null", "", Status::NullValue);
}

// ---------------------------------------------------------------------------
// Path errors, shared by all four reads.
// ---------------------------------------------------------------------------

TEST(AccessorPathErrors, MissingKeyIsPathNotFound) {
  ExpectLongFails(R"({"a":1})", "/b", Status::PathNotFound);
  ExpectDoubleFails(R"({"a":1})", "/b", Status::PathNotFound);
  ExpectBoolFails(R"({"a":true})", "/b", Status::PathNotFound);
  ExpectCountFails(R"({"a":[]})", "/b", Status::PathNotFound);
}

TEST(AccessorPathErrors, IndexPastTheEndIsIndexOutOfRange) {
  ExpectLongFails(R"({"a":[1]})", "/a/1", Status::IndexOutOfRange);
  ExpectDoubleFails(R"({"a":[1]})", "/a/1", Status::IndexOutOfRange);
  ExpectBoolFails(R"({"a":[true]})", "/a/1", Status::IndexOutOfRange);
  ExpectCountFails(R"({"a":[[1]]})", "/a/1", Status::IndexOutOfRange);
}

TEST(AccessorPathErrors, MalformedPathIsPathSyntaxError) {
  ExpectLongFails(R"({"a":1})", "a", Status::PathSyntaxError);
  ExpectDoubleFails(R"({"a":1})", "a", Status::PathSyntaxError);
  ExpectBoolFails(R"({"a":true})", "a", Status::PathSyntaxError);
  ExpectCountFails(R"({"a":[]})", "a", Status::PathSyntaxError);
  ExpectLongFails(R"({"a":1})", "/a~2", Status::PathSyntaxError);
}

TEST(AccessorPathErrors, ContainerKindMismatchMidPathIsTypeMismatch) {
  ExpectLongFails(R"({"a":5})", "/a/b", Status::TypeMismatch);
  ExpectLongFails(R"({"a":[1]})", "/a/x", Status::TypeMismatch);
  ExpectDoubleFails(R"({"a":"s"})", "/a/0", Status::TypeMismatch);
  ExpectBoolFails(R"({"a":true})", "/a/0", Status::TypeMismatch);
  ExpectCountFails(R"({"a":1})", "/a/0", Status::TypeMismatch);
}

TEST(AccessorPathErrors, NumericTokenOnObjectIsPathNotFound) {
  ExpectLongFails(R"({"a":{"b":1}})", "/a/0", Status::PathNotFound);
}

TEST(AccessorPathErrors, NullMidPathIsTypeMismatch) {
  ExpectLongFails(R"({"a":null})", "/a/b", Status::TypeMismatch);
}

// ---------------------------------------------------------------------------
// Locale independence: the reads return the stored double without a text round-trip.
// ---------------------------------------------------------------------------

TEST(AccessorLocale, ReadsDoNotChangeTheGlobalLocaleAndAreUnaffectedByIt) {
  const std::string before = std::setlocale(LC_ALL, nullptr);
  const bool localeSet = std::setlocale(LC_NUMERIC, "de-DE") != nullptr;
  if (!localeSet) {
    GTEST_SKIP() << "de-DE locale not available on this system";
  }
  const std::string during = std::setlocale(LC_ALL, nullptr);

  double out = kSentinelDouble;
  EXPECT_EQ(Double(R"({"v":3.5})", "/v", out), Status::Ok);
  EXPECT_DOUBLE_EQ(out, 3.5);
  EXPECT_EQ(std::string(std::setlocale(LC_ALL, nullptr)), during);

  std::setlocale(LC_ALL, before.c_str());
  EXPECT_EQ(std::string(std::setlocale(LC_ALL, nullptr)), before);
}

TEST(AccessorLocale, ReadsLeaveTheDefaultLocaleUntouched) {
  const std::string before = std::setlocale(LC_ALL, nullptr);
  double out = kSentinelDouble;
  EXPECT_EQ(Double(R"({"v":2.5})", "/v", out), Status::Ok);
  std::int32_t number = kSentinelLong;
  EXPECT_EQ(Long(R"({"v":2})", "/v", number), Status::Ok);
  EXPECT_EQ(std::string(std::setlocale(LC_ALL, nullptr)), before);
}

// ---------------------------------------------------------------------------
// PathTokens overloads agree with the string overloads.
// ---------------------------------------------------------------------------

namespace {

PathTokens Tokens(const std::string& path) {
  PathTokens tokens;
  EXPECT_EQ(ParsePath(path, tokens), Status::Ok);
  return tokens;
}

const char* const kTokenDoc =
    R"({"i":42,"f":2.5,"frac":3.7,"big":2147483648,"b":true,"s":"x","n":null,)"
    R"("arr":[1,2,3],"obj":{"k":1},"a/b":7,"nested":[[1,2],[3,4,5]]})";

void ExpectAllAgree(const std::string& path) {
  SCOPED_TRACE(path);
  const JsonValue document = Parse(kTokenDoc);
  const PathTokens tokens = Tokens(path);

  std::int32_t longString = kSentinelLong;
  std::int32_t longTokens = kSentinelLong;
  const Status longStatus = ReadLongAt(document, path, longString);
  EXPECT_EQ(ReadLongAt(document, tokens, longTokens), longStatus);
  EXPECT_EQ(longTokens, longString);

  double doubleString = kSentinelDouble;
  double doubleTokens = kSentinelDouble;
  const Status doubleStatus = ReadDoubleAt(document, path, doubleString);
  EXPECT_EQ(ReadDoubleAt(document, tokens, doubleTokens), doubleStatus);
  EXPECT_EQ(doubleTokens, doubleString);

  bool boolString = true;
  bool boolTokens = true;
  const Status boolStatus = ReadBoolAt(document, path, boolString);
  EXPECT_EQ(ReadBoolAt(document, tokens, boolTokens), boolStatus);
  EXPECT_EQ(boolTokens, boolString);

  std::uint32_t countString = kSentinelCount;
  std::uint32_t countTokens = kSentinelCount;
  const Status countStatus = CountElementsAt(document, path, countString);
  EXPECT_EQ(CountElementsAt(document, tokens, countTokens), countStatus);
  EXPECT_EQ(countTokens, countString);
}

struct PathFailure {
  const char* path;
  Status expected;
};

}  // namespace

TEST(AccessorTokenOverloads, AgreeWithTheStringOverloadsAcrossValuesAndPathErrors) {
  const char* const paths[] = {
      "",         "/i",       "/f",      "/frac",   "/big",    "/b",         "/s",
      "/n",       "/arr",     "/obj",    "/arr/1",  "/nested/1", "/a~1b",    "/missing",
      "/arr/3",   "/arr/-",   "/arr/x",  "/s/x",    "/n/x",    "/i/0",       "/obj/0",
      "/nested/0/1",
  };
  for (const char* path : paths) {
    ExpectAllAgree(path);
  }
}

TEST(AccessorTokenOverloads, ReadLongTokensSuccessAndFailureStatuses) {
  const JsonValue document = Parse(kTokenDoc);
  std::int32_t out = kSentinelLong;
  EXPECT_EQ(ReadLongAt(document, Tokens("/i"), out), Status::Ok);
  EXPECT_EQ(out, 42);

  const PathFailure failures[] = {
      {"/frac", Status::NotIntegral},     {"/big", Status::NumericOverflow},
      {"/s", Status::TypeMismatch},       {"/n", Status::NullValue},
      {"/missing", Status::PathNotFound}, {"/arr/3", Status::IndexOutOfRange},
      {"/s/x", Status::TypeMismatch},
  };
  for (const PathFailure& c : failures) {
    SCOPED_TRACE(c.path);
    out = kSentinelLong;
    EXPECT_EQ(ReadLongAt(document, Tokens(c.path), out), c.expected);
    EXPECT_EQ(out, kSentinelLong);
  }
}

TEST(AccessorTokenOverloads, ReadDoubleTokensSuccessAndFailureStatuses) {
  const JsonValue document = Parse(kTokenDoc);
  double out = kSentinelDouble;
  EXPECT_EQ(ReadDoubleAt(document, Tokens("/f"), out), Status::Ok);
  EXPECT_DOUBLE_EQ(out, 2.5);

  const PathFailure failures[] = {
      {"/s", Status::TypeMismatch},       {"/b", Status::TypeMismatch},
      {"/n", Status::NullValue},          {"/missing", Status::PathNotFound},
      {"/arr/3", Status::IndexOutOfRange},
  };
  for (const PathFailure& c : failures) {
    SCOPED_TRACE(c.path);
    out = kSentinelDouble;
    EXPECT_EQ(ReadDoubleAt(document, Tokens(c.path), out), c.expected);
    EXPECT_EQ(out, kSentinelDouble);
  }
}

TEST(AccessorTokenOverloads, ReadBoolTokensSuccessAndFailureStatuses) {
  const JsonValue document = Parse(kTokenDoc);
  bool out = false;
  EXPECT_EQ(ReadBoolAt(document, Tokens("/b"), out), Status::Ok);
  EXPECT_TRUE(out);

  const PathFailure failures[] = {
      {"/i", Status::TypeMismatch},       {"/s", Status::TypeMismatch},
      {"/n", Status::NullValue},          {"/missing", Status::PathNotFound},
      {"/arr/-", Status::IndexOutOfRange},
  };
  for (const PathFailure& c : failures) {
    SCOPED_TRACE(c.path);
    out = true;
    EXPECT_EQ(ReadBoolAt(document, Tokens(c.path), out), c.expected);
    EXPECT_TRUE(out);
  }
}

TEST(AccessorTokenOverloads, CountElementsTokensSuccessAndFailureStatuses) {
  const JsonValue document = Parse(kTokenDoc);
  std::uint32_t out = kSentinelCount;
  EXPECT_EQ(CountElementsAt(document, Tokens("/arr"), out), Status::Ok);
  EXPECT_EQ(out, 3u);
  EXPECT_EQ(CountElementsAt(document, Tokens("/nested/1"), out), Status::Ok);
  EXPECT_EQ(out, 3u);

  const PathFailure failures[] = {
      {"/obj", Status::TypeMismatch},     {"/i", Status::TypeMismatch},
      {"/n", Status::NullValue},          {"/missing", Status::PathNotFound},
      {"/arr/3", Status::IndexOutOfRange},
  };
  for (const PathFailure& c : failures) {
    SCOPED_TRACE(c.path);
    out = kSentinelCount;
    EXPECT_EQ(CountElementsAt(document, Tokens(c.path), out), c.expected);
    EXPECT_EQ(out, kSentinelCount);
  }
}

TEST(AccessorTokenOverloads, EmptyTokensReadTheRoot) {
  std::int32_t number = kSentinelLong;
  EXPECT_EQ(ReadLongAt(Parse("17"), PathTokens{}, number), Status::Ok);
  EXPECT_EQ(number, 17);

  std::uint32_t count = kSentinelCount;
  EXPECT_EQ(CountElementsAt(Parse("[1,2,3,4]"), PathTokens{}, count), Status::Ok);
  EXPECT_EQ(count, 4u);
}

TEST(AccessorTokenOverloads, TokensAreMatchedLiterallyAndNeverParsed) {
  const JsonValue document = Parse(R"({"a~2":5,"a/b":6})");
  std::int32_t out = kSentinelLong;
  EXPECT_EQ(ReadLongAt(document, std::string("/a~2"), out), Status::PathSyntaxError);
  EXPECT_EQ(out, kSentinelLong);
  EXPECT_EQ(ReadLongAt(document, PathTokens{"a~2"}, out), Status::Ok);
  EXPECT_EQ(out, 5);
  EXPECT_EQ(ReadLongAt(document, PathTokens{"a/b"}, out), Status::Ok);
  EXPECT_EQ(out, 6);
}
