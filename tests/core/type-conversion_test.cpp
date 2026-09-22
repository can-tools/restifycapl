// TEST-2 (Stage 8, plan.md §6) -- coverage for src/core/type-conversion.*.

#include "core/type-conversion.h"

#include <clocale>
#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "core/status.h"
#include "json.hpp"

using nlohmann::json;

namespace {

constexpr std::int32_t kSentinelLong = -999;
constexpr double kSentinelDouble = -999.0;

}  // namespace

// ---------------------------------------------------------------------------
// ToLong
// ---------------------------------------------------------------------------

TEST(ToLong, ValidInteger) {
  json value = 42;
  std::int32_t out = kSentinelLong;
  EXPECT_EQ(ToLong(value, out), Status::Ok);
  EXPECT_EQ(out, 42);
}

TEST(ToLong, FractionalNumberIsNotIntegral) {
  json value = 3.7;
  std::int32_t out = kSentinelLong;
  EXPECT_EQ(ToLong(value, out), Status::NotIntegral);
  EXPECT_NE(out, 3);
  EXPECT_EQ(out, kSentinelLong);
}

TEST(ToLong, WholeValuedFloatIsOk) {
  json value = 3.0;
  std::int32_t out = kSentinelLong;
  EXPECT_EQ(ToLong(value, out), Status::Ok);
  EXPECT_EQ(out, 3);
}

TEST(ToLong, OneAboveInt32MaxIsOverflow) {
  json value = 2147483648LL;
  std::int32_t out = kSentinelLong;
  EXPECT_EQ(ToLong(value, out), Status::NumericOverflow);
}

TEST(ToLong, OneBelowInt32MinIsOverflow) {
  json value = -2147483649LL;
  std::int32_t out = kSentinelLong;
  EXPECT_EQ(ToLong(value, out), Status::NumericOverflow);
}

// Same source runs under both `make test ARCH=x86` and `make test ARCH=x64`
// (plan.md §6 TEST-2); this case is the architecture-parity check for
// constraint §3.6 -- int32_t range checks must not vary with pointer width.
TEST(ToLong, BeyondInt64RangeIsOverflow) {
  json value = 1e300;
  std::int32_t out = kSentinelLong;
  EXPECT_EQ(ToLong(value, out), Status::NumericOverflow);
}

TEST(ToLong, NullIsNullValue) {
  json value = nullptr;
  std::int32_t out = kSentinelLong;
  EXPECT_EQ(ToLong(value, out), Status::NullValue);
}

TEST(ToLong, StringIsTypeMismatch) {
  json value = "42";
  std::int32_t out = kSentinelLong;
  EXPECT_EQ(ToLong(value, out), Status::TypeMismatch);
}

TEST(ToLong, BoolIsTypeMismatch) {
  json value = true;
  std::int32_t out = kSentinelLong;
  EXPECT_EQ(ToLong(value, out), Status::TypeMismatch);
}

TEST(ToLong, ObjectIsTypeMismatch) {
  json value = json::object({{"a", 1}});
  std::int32_t out = kSentinelLong;
  EXPECT_EQ(ToLong(value, out), Status::TypeMismatch);
}

TEST(ToLong, ArrayIsTypeMismatch) {
  json value = json::array({1, 2, 3});
  std::int32_t out = kSentinelLong;
  EXPECT_EQ(ToLong(value, out), Status::TypeMismatch);
}

// ---------------------------------------------------------------------------
// ToDouble
// ---------------------------------------------------------------------------

TEST(ToDouble, FloatValue) {
  json value = 3.5;
  double out = kSentinelDouble;
  EXPECT_EQ(ToDouble(value, out), Status::Ok);
  EXPECT_DOUBLE_EQ(out, 3.5);
}

TEST(ToDouble, IntegerTypedNumber) {
  json value = 7;
  double out = kSentinelDouble;
  EXPECT_EQ(ToDouble(value, out), Status::Ok);
  EXPECT_DOUBLE_EQ(out, 7.0);
}

TEST(ToDouble, NullIsNullValue) {
  json value = nullptr;
  double out = kSentinelDouble;
  EXPECT_EQ(ToDouble(value, out), Status::NullValue);
}

TEST(ToDouble, StringIsTypeMismatch) {
  json value = "3.5";
  double out = kSentinelDouble;
  EXPECT_EQ(ToDouble(value, out), Status::TypeMismatch);
}

// ---------------------------------------------------------------------------
// ToBool
// ---------------------------------------------------------------------------

TEST(ToBool, TrueValue) {
  json value = true;
  bool out = false;
  EXPECT_EQ(ToBool(value, out), Status::Ok);
  EXPECT_TRUE(out);
}

TEST(ToBool, FalseValue) {
  json value = false;
  bool out = true;
  EXPECT_EQ(ToBool(value, out), Status::Ok);
  EXPECT_FALSE(out);
}

TEST(ToBool, NumericZeroIsTypeMismatch) {
  json value = 0;
  bool out = false;
  EXPECT_EQ(ToBool(value, out), Status::TypeMismatch);
}

TEST(ToBool, NumericOneIsTypeMismatch) {
  json value = 1;
  bool out = false;
  EXPECT_EQ(ToBool(value, out), Status::TypeMismatch);
}

TEST(ToBool, StringTrueIsTypeMismatch) {
  json value = "true";
  bool out = false;
  EXPECT_EQ(ToBool(value, out), Status::TypeMismatch);
}

TEST(ToBool, NullIsNullValue) {
  json value = nullptr;
  bool out = false;
  EXPECT_EQ(ToBool(value, out), Status::NullValue);
}

// ---------------------------------------------------------------------------
// ToText
// ---------------------------------------------------------------------------

TEST(ToText, StringReturnsUnquotedContents) {
  json value = "hello world";
  std::string out;
  EXPECT_EQ(ToText(value, out), Status::Ok);
  EXPECT_EQ(out, "hello world");
}

TEST(ToText, NumberIsTypeMismatch) {
  json value = 42;
  std::string out;
  EXPECT_EQ(ToText(value, out), Status::TypeMismatch);
}

TEST(ToText, NullIsNullValue) {
  json value = nullptr;
  std::string out;
  EXPECT_EQ(ToText(value, out), Status::NullValue);
}

TEST(ToText, ObjectIsTypeMismatch) {
  json value = json::object({{"a", 1}});
  std::string out;
  EXPECT_EQ(ToText(value, out), Status::TypeMismatch);
}

TEST(ToText, ArrayIsTypeMismatch) {
  json value = json::array({1, 2, 3});
  std::string out;
  EXPECT_EQ(ToText(value, out), Status::TypeMismatch);
}

// ---------------------------------------------------------------------------
// ValueToText -- §5.3, one test per row.
// ---------------------------------------------------------------------------

TEST(ValueToText, NullBecomesLiteralNullText) {
  json value = nullptr;
  std::string out;
  EXPECT_EQ(ValueToText(value, out), Status::Ok);
  EXPECT_EQ(out, "null");
}

TEST(ValueToText, NumberIsStringified) {
  json value = 42;
  std::string out;
  EXPECT_EQ(ValueToText(value, out), Status::Ok);
  EXPECT_EQ(out, "42");
}

TEST(ValueToText, BoolIsStringified) {
  json value = true;
  std::string out;
  EXPECT_EQ(ValueToText(value, out), Status::Ok);
  EXPECT_EQ(out, "true");
}

TEST(ValueToText, StringIsItsOwnContents) {
  json value = "hello";
  std::string out;
  EXPECT_EQ(ValueToText(value, out), Status::Ok);
  EXPECT_EQ(out, "hello");
}

TEST(ValueToText, ObjectIsTypeMismatchAndOutUntouched) {
  json value = json::object({{"a", 1}});
  std::string out = "sentinel";
  EXPECT_EQ(ValueToText(value, out), Status::TypeMismatch);
  EXPECT_EQ(out, "sentinel");
}

TEST(ValueToText, ArrayIsTypeMismatchAndOutUntouched) {
  json value = json::array({1, 2, 3});
  std::string out = "sentinel";
  EXPECT_EQ(ValueToText(value, out), Status::TypeMismatch);
  EXPECT_EQ(out, "sentinel");
}

// ---------------------------------------------------------------------------
// ParseLong
// ---------------------------------------------------------------------------

TEST(ParseLong, ValidInteger) {
  std::int32_t out = kSentinelLong;
  EXPECT_EQ(ParseLong("42", out), Status::Ok);
  EXPECT_EQ(out, 42);
}

TEST(ParseLong, EmptyIsInvalidArgument) {
  std::int32_t out = kSentinelLong;
  EXPECT_EQ(ParseLong("", out), Status::InvalidArgument);
}

TEST(ParseLong, NonNumericIsParseError) {
  std::int32_t out = kSentinelLong;
  EXPECT_EQ(ParseLong("abc", out), Status::ParseError);
}

TEST(ParseLong, TrailingGarbageIsParseError) {
  std::int32_t out = kSentinelLong;
  EXPECT_EQ(ParseLong("12abc", out), Status::ParseError);
}

TEST(ParseLong, TooLargeIsNumericOverflow) {
  std::int32_t out = kSentinelLong;
  EXPECT_EQ(ParseLong("99999999999", out), Status::NumericOverflow);
}

// Pinned by a plan amendment (plan.md §6 TEST-2): a fractional text value
// must report NotIntegral, never be conflated with ParseError.
TEST(ParseLong, FractionalTextIsNotIntegral) {
  std::int32_t out = kSentinelLong;
  EXPECT_EQ(ParseLong("3.5", out), Status::NotIntegral);
}

// ---------------------------------------------------------------------------
// ParseDouble
// ---------------------------------------------------------------------------

TEST(ParseDouble, ValidFloat) {
  double out = kSentinelDouble;
  EXPECT_EQ(ParseDouble("3.5", out), Status::Ok);
  EXPECT_DOUBLE_EQ(out, 3.5);
}

TEST(ParseDouble, EmptyIsInvalidArgument) {
  double out = kSentinelDouble;
  EXPECT_EQ(ParseDouble("", out), Status::InvalidArgument);
}

TEST(ParseDouble, NonNumericIsParseError) {
  double out = kSentinelDouble;
  EXPECT_EQ(ParseDouble("abc", out), Status::ParseError);
}

TEST(ParseDouble, TrailingGarbageIsParseError) {
  double out = kSentinelDouble;
  EXPECT_EQ(ParseDouble("12abc", out), Status::ParseError);
}

TEST(ParseDouble, TooLargeIsNumericOverflow) {
  double out = kSentinelDouble;
  // 1e400 has no finite double representation; from_chars reports it as
  // result_out_of_range the same way an oversized integer literal does.
  EXPECT_EQ(ParseDouble("1e400", out), Status::NumericOverflow);
}

// ---------------------------------------------------------------------------
// Locale independence (constraint §3.7).
//
// ValueToText goes through nlohmann's own serializer and ParseDouble through
// std::from_chars, neither of which consult the global C/C++ locale -- this
// is a static guarantee of the implementation choice, not something that can
// regress at runtime, but the swap below exercises it anyway: if either
// function ever changed to a locale-sensitive path (e.g. sprintf/strtod),
// this test would fail under a decimal-comma locale.
// ---------------------------------------------------------------------------

TEST(LocaleIndependence, ValueToTextAndParseDoubleUseDotDecimal) {
  const char* previous = std::setlocale(LC_NUMERIC, nullptr);
  const bool locale_set = std::setlocale(LC_NUMERIC, "de-DE") != nullptr;
  if (!locale_set) {
    GTEST_SKIP() << "de-DE locale not available on this system";
  }

  json value = 3.5;
  std::string text;
  EXPECT_EQ(ValueToText(value, text), Status::Ok);
  EXPECT_EQ(text.find(','), std::string::npos);
  EXPECT_NE(text.find('.'), std::string::npos);

  double parsed = kSentinelDouble;
  EXPECT_EQ(ParseDouble("3.5", parsed), Status::Ok);
  EXPECT_DOUBLE_EQ(parsed, 3.5);

  std::setlocale(LC_NUMERIC, previous);
}
