// TEST-3 (Stage 8, plan.md §6) -- coverage for src/core/json-path.*.

#include "core/json-path.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "core/status.h"
#include "json.hpp"

using nlohmann::json;

// ---------------------------------------------------------------------------
// ParsePath -- edge-case table (json-path.h, mirrors plan.md §6 CPP-3).
// ---------------------------------------------------------------------------

TEST(ParsePath, EmptyPathIsSyntaxError) {
  std::vector<PathSegment> segments;
  EXPECT_EQ(ParsePath("", segments), Status::PathSyntaxError);
}

TEST(ParsePath, LeadingDotIsSyntaxError) {
  std::vector<PathSegment> segments;
  EXPECT_EQ(ParsePath(".items[0]", segments), Status::PathSyntaxError);
}

TEST(ParsePath, TopLevelIndexOnRootArrayIsOk) {
  std::vector<PathSegment> segments;
  EXPECT_EQ(ParsePath("[0].name", segments), Status::Ok);
  ASSERT_EQ(segments.size(), 2u);
  EXPECT_EQ(segments[0].kind, PathSegment::Kind::Index);
  EXPECT_EQ(segments[0].index, 0u);
  EXPECT_EQ(segments[1].kind, PathSegment::Kind::Key);
  EXPECT_EQ(segments[1].key, "name");
}

TEST(ParsePath, NegativeIndexIsSyntaxError) {
  std::vector<PathSegment> segments;
  EXPECT_EQ(ParsePath("[-1]", segments), Status::PathSyntaxError);
}

TEST(ParsePath, NonNumericIndexIsSyntaxError) {
  std::vector<PathSegment> segments;
  EXPECT_EQ(ParsePath("[x]", segments), Status::PathSyntaxError);
}

TEST(ParsePath, IndexTooLargeForUint32IsSyntaxError) {
  std::vector<PathSegment> segments;
  EXPECT_EQ(ParsePath("[4294967296]", segments), Status::PathSyntaxError);
}

TEST(ParsePath, UnclosedBracketIsSyntaxError) {
  std::vector<PathSegment> segments;
  EXPECT_EQ(ParsePath("items[0", segments), Status::PathSyntaxError);
}

TEST(ParsePath, EmptyBracketsIsSyntaxError) {
  std::vector<PathSegment> segments;
  EXPECT_EQ(ParsePath("items[]", segments), Status::PathSyntaxError);
}

TEST(ParsePath, DoubleDotIsSyntaxError) {
  std::vector<PathSegment> segments;
  EXPECT_EQ(ParsePath("items..name", segments), Status::PathSyntaxError);
}

TEST(ParsePath, TrailingDotIsSyntaxError) {
  std::vector<PathSegment> segments;
  EXPECT_EQ(ParsePath("items.", segments), Status::PathSyntaxError);
}

// std::string_view{} default-constructs to {nullptr, 0} -- structurally
// invalid as an argument, distinct from the syntactically-empty-but-valid ""
// case above (PathSyntaxError).
TEST(ParsePath, NullDataStringViewIsInvalidArgument) {
  std::vector<PathSegment> segments;
  const std::string_view invalid;
  ASSERT_EQ(invalid.data(), nullptr);
  EXPECT_EQ(ParsePath(invalid, segments), Status::InvalidArgument);
}

TEST(ParsePath, ValidMultiSegmentPathParsesCorrectly) {
  std::vector<PathSegment> segments;
  EXPECT_EQ(ParsePath("data.items[0].name", segments), Status::Ok);
  ASSERT_EQ(segments.size(), 4u);

  EXPECT_EQ(segments[0].kind, PathSegment::Kind::Key);
  EXPECT_EQ(segments[0].key, "data");

  EXPECT_EQ(segments[1].kind, PathSegment::Kind::Key);
  EXPECT_EQ(segments[1].key, "items");

  EXPECT_EQ(segments[2].kind, PathSegment::Kind::Index);
  EXPECT_EQ(segments[2].index, 0u);

  EXPECT_EQ(segments[3].kind, PathSegment::Kind::Key);
  EXPECT_EQ(segments[3].key, "name");
}

// ---------------------------------------------------------------------------
// ResolvePath -- valid nested hit.
// ---------------------------------------------------------------------------

TEST(ResolvePath, ValidNestedHitResolvesToLeafString) {
  const json document = json::parse(R"({"data":{"items":[{"name":"Ann"}]}})");
  const json* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "data.items[0].name", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->get<std::string>(), "Ann");
}

// ---------------------------------------------------------------------------
// ResolvePath -- §5.2 segment-kind resolution rule, one test per row.
// ---------------------------------------------------------------------------

// Row 1: [n] on array, n >= size -> IndexOutOfRange.
TEST(ResolvePath, IndexOnArrayOutOfRangeIsIndexOutOfRange) {
  const json document = json::array({1, 2, 3});
  const json* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "[5]", out), Status::IndexOutOfRange);
}

// Row 2: .key on object, key absent -> PathNotFound.
TEST(ResolvePath, KeyAbsentOnObjectIsPathNotFound) {
  const json document = json::parse(R"({"a":1})");
  const json* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "missing", out), Status::PathNotFound);
}

// Row 3: [n] on object (not an array) -> TypeMismatch.
TEST(ResolvePath, IndexOnObjectIsTypeMismatch) {
  const json document = json::parse(R"({"a":1})");
  const json* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "[0]", out), Status::TypeMismatch);
}

// Row 4: .key on array (not an object) -> TypeMismatch.
TEST(ResolvePath, KeyOnArrayIsTypeMismatch) {
  const json document = json::array({1, 2, 3});
  const json* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "key", out), Status::TypeMismatch);
}

// Row 5: either segment kind on a scalar where a container was expected ->
// TypeMismatch.
TEST(ResolvePath, KeyOnScalarIsTypeMismatch) {
  const json document = 42;
  const json* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "a", out), Status::TypeMismatch);
}

// ---------------------------------------------------------------------------
// ResolvePath -- additional scenarios from plan.md §6 TEST-3.
// ---------------------------------------------------------------------------

// D3.3: mid-path scalar node (name resolves to a string, not a container).
TEST(ResolvePath, MidPathScalarIsTypeMismatch) {
  const json document = json::parse(R"({"user":{"name":"Ann"}})");
  const json* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "user.name.first", out), Status::TypeMismatch);
}

// Mid-path null falls under §5.2's last row (container expected), not
// NullValue -- NullValue is reserved for terminal typed reads.
TEST(ResolvePath, MidPathNullIsTypeMismatch) {
  const json document = json::parse(R"({"user":null})");
  const json* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "user.name", out), Status::TypeMismatch);
}

TEST(ResolvePath, EmptyArrayIndexZeroIsIndexOutOfRange) {
  const json document = json::parse(R"({"items":[]})");
  const json* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "items[0]", out), Status::IndexOutOfRange);
}

TEST(ResolvePath, DeeplyNestedPathResolvesCorrectly) {
  const json document = json::parse(R"({"a":{"b":{"c":{"d":42}}}})");
  const json* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "a.b.c.d", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->get<std::int32_t>(), 42);
}

TEST(ResolvePath, RootArrayWithLeadingIndexResolvesCorrectly) {
  const json document = json::parse(R"([{"name":"first"},{"name":"second"}])");
  const json* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "[1].name", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->get<std::string>(), "second");
}

// ResolvePath must alias the caller's document, never copy -- assert address
// equality against the equivalent nlohmann::json access chain.
TEST(ResolvePath, ResolvedPointerAliasesDocumentRatherThanCopying) {
  const json document = json::parse(R"({"data":{"items":[{"name":"Ann"}]}})");
  const json* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "data.items[0].name", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out, &document["data"]["items"][0]["name"]);
}
