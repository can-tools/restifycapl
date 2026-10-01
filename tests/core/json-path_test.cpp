// Coverage for src/core/json-path.*.

#include "core/json-path.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "core/json-value.h"
#include "core/status.h"

namespace {

struct TokenCase {
  const char* path;
  std::vector<std::string> tokens;
};

struct StatusCase {
  const char* path;
  Status status;
};

}  // namespace

// ---------------------------------------------------------------------------
// ParsePath -- RFC 6901 syntax (see json-path.h / docs/json-path.md).
// ---------------------------------------------------------------------------

TEST(ParsePath, EmptyPathIsOkWithNoTokens) {
  std::vector<std::string> tokens = {"stale"};
  EXPECT_EQ(ParsePath("", tokens), Status::Ok);
  EXPECT_TRUE(tokens.empty());
}

TEST(ParsePath, LeadingDotIsSyntaxError) {
  std::vector<std::string> tokens;
  EXPECT_EQ(ParsePath(".items[0]", tokens), Status::PathSyntaxError);
}

TEST(ParsePath, TextWithoutLeadingSlashIsSyntaxError) {
  const char* const paths[] = {"data.items[0].name", "a", "a/b", "[0]", " /a", "~", "~0"};
  for (const char* path : paths) {
    SCOPED_TRACE(path);
    std::vector<std::string> tokens;
    EXPECT_EQ(ParsePath(path, tokens), Status::PathSyntaxError);
  }
}

TEST(ParsePath, ValidMultiTokenPathParsesCorrectly) {
  std::vector<std::string> tokens;
  EXPECT_EQ(ParsePath("/data/items/0/name", tokens), Status::Ok);
  ASSERT_EQ(tokens.size(), 4u);
  EXPECT_EQ(tokens[0], "data");
  EXPECT_EQ(tokens[1], "items");
  EXPECT_EQ(tokens[2], "0");
  EXPECT_EQ(tokens[3], "name");
}

TEST(ParsePath, RootArrayStylePathYieldsTextTokens) {
  std::vector<std::string> tokens;
  EXPECT_EQ(ParsePath("/0/name", tokens), Status::Ok);
  ASSERT_EQ(tokens.size(), 2u);
  EXPECT_EQ(tokens[0], "0");
  EXPECT_EQ(tokens[1], "name");
}

TEST(ParsePath, DotsBracketsAndWhitespaceAreOrdinaryKeyCharacters) {
  const TokenCase cases[] = {
      {"/items[0]", {"items[0]"}},
      {"/items[0", {"items[0"}},
      {"/items[]", {"items[]"}},
      {"/a..b", {"a..b"}},
      {"/items.", {"items."}},
      {"/@odata.context", {"@odata.context"}},
      {"/ a b ", {" a b "}},
  };
  for (const TokenCase& c : cases) {
    SCOPED_TRACE(c.path);
    std::vector<std::string> tokens;
    EXPECT_EQ(ParsePath(c.path, tokens), Status::Ok);
    EXPECT_EQ(tokens, c.tokens);
  }
}

TEST(ParsePath, TildeEscapesDecodeToSlashAndTilde) {
  const TokenCase cases[] = {
      {"/a~1b", {"a/b"}},
      {"/m~0n", {"m~n"}},
      {"/a~1b/c~0d", {"a/b", "c~d"}},
      {"/~1", {"/"}},
      {"/~0", {"~"}},
  };
  for (const TokenCase& c : cases) {
    SCOPED_TRACE(c.path);
    std::vector<std::string> tokens;
    EXPECT_EQ(ParsePath(c.path, tokens), Status::Ok);
    EXPECT_EQ(tokens, c.tokens);
  }
}

// "~01" must decode to "~1": replacing "~0" first and then "~1" would give "/".
TEST(ParsePath, TildeZeroOneDecodesToTildeOneNotSlash) {
  const TokenCase cases[] = {
      {"/~01", {"~1"}},
      {"/~001", {"~01"}},
      {"/~10", {"/0"}},
      {"/~0~1", {"~/"}},
      {"/~1~0", {"/~"}},
  };
  for (const TokenCase& c : cases) {
    SCOPED_TRACE(c.path);
    std::vector<std::string> tokens;
    EXPECT_EQ(ParsePath(c.path, tokens), Status::Ok);
    EXPECT_EQ(tokens, c.tokens);
  }
}

TEST(ParsePath, BadTildeEscapeIsSyntaxError) {
  const char* const paths[] = {"/a~2", "/~", "/a~", "/a~/b", "/~x", "/~ ", "/a/b~", "/~~", "/~00~"};
  for (const char* path : paths) {
    SCOPED_TRACE(path);
    std::vector<std::string> tokens;
    EXPECT_EQ(ParsePath(path, tokens), Status::PathSyntaxError);
  }
}

TEST(ParsePath, EmptyTokensAreKept) {
  const TokenCase cases[] = {
      {"/", {""}},
      {"//", {"", ""}},
      {"//0", {"", "0"}},
      {"/x/", {"x", ""}},
      {"/x//y", {"x", "", "y"}},
  };
  for (const TokenCase& c : cases) {
    SCOPED_TRACE(c.path);
    std::vector<std::string> tokens;
    EXPECT_EQ(ParsePath(c.path, tokens), Status::Ok);
    EXPECT_EQ(tokens, c.tokens);
  }
}

// std::string_view{} default-constructs to {nullptr, 0} -- structurally
// invalid as an argument, distinct from the valid empty path "" (whole document).
TEST(ParsePath, NullDataStringViewIsInvalidArgument) {
  std::vector<std::string> tokens;
  const std::string_view invalid;
  ASSERT_EQ(invalid.data(), nullptr);
  EXPECT_EQ(ParsePath(invalid, tokens), Status::InvalidArgument);
}

TEST(ParsePath, OutIsLeftUnchangedOnFailure) {
  const std::vector<std::string> original = {"keep", "me"};

  std::vector<std::string> tokens = original;
  EXPECT_EQ(ParsePath("no-slash", tokens), Status::PathSyntaxError);
  EXPECT_EQ(tokens, original);

  tokens = original;
  EXPECT_EQ(ParsePath("/ok/then~2bad", tokens), Status::PathSyntaxError);
  EXPECT_EQ(tokens, original);

  tokens = original;
  EXPECT_EQ(ParsePath(std::string_view{}, tokens), Status::InvalidArgument);
  EXPECT_EQ(tokens, original);
}

TEST(ParsePath, OutIsReplacedNotAppendedOnSuccess) {
  std::vector<std::string> tokens = {"stale", "tokens", "here"};
  EXPECT_EQ(ParsePath("/a", tokens), Status::Ok);
  ASSERT_EQ(tokens.size(), 1u);
  EXPECT_EQ(tokens[0], "a");
}

// ---------------------------------------------------------------------------
// ResolvePath -- valid hits.
// ---------------------------------------------------------------------------

TEST(ResolvePath, ValidNestedHitResolvesToLeafString) {
  const JsonValue document = JsonValue::parse(R"({"data":{"items":[{"name":"Ann"}]}})");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/data/items/0/name", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->get<std::string>(), "Ann");
}

TEST(ResolvePath, DeeplyNestedPathResolvesCorrectly) {
  const JsonValue document = JsonValue::parse(R"({"a":{"b":{"c":{"d":42}}}})");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/a/b/c/d", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->get<std::int32_t>(), 42);
}

TEST(ResolvePath, RootArrayWithLeadingIndexResolvesCorrectly) {
  const JsonValue document = JsonValue::parse(R"([{"name":"first"},{"name":"second"}])");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/1/name", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->get<std::string>(), "second");
}

TEST(ResolvePath, FirstAndLastArrayIndexResolve) {
  const JsonValue document = JsonValue::array({10, 20, 30});
  const JsonValue* out = nullptr;

  EXPECT_EQ(ResolvePath(document, "/0", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->get<std::int32_t>(), 10);

  out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/2", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->get<std::int32_t>(), 30);
}

TEST(ResolvePath, EmptyPathReturnsWholeDocument) {
  const JsonValue document = JsonValue::parse(R"({"a":1})");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "", out), Status::Ok);
  EXPECT_EQ(out, &document);
}

TEST(ResolvePath, EmptyPathOnScalarDocumentReturnsIt) {
  const JsonValue document = 42;
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "", out), Status::Ok);
  EXPECT_EQ(out, &document);
}

TEST(ResolvePath, ContainerNodeCanBeTheResolvedTarget) {
  const JsonValue document = JsonValue::parse(R"({"data":{"items":[1,2]}})");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/data/items", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_TRUE(out->is_array());
  EXPECT_EQ(out->size(), 2u);
}

TEST(ResolvePath, NullLeafResolvesOk) {
  const JsonValue document = JsonValue::parse(R"({"user":null})");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/user", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_TRUE(out->is_null());
}

// ---------------------------------------------------------------------------
// ResolvePath -- object keys.
// ---------------------------------------------------------------------------

TEST(ResolvePath, KeyAbsentOnObjectIsPathNotFound) {
  const JsonValue document = JsonValue::parse(R"({"a":1})");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/missing", out), Status::PathNotFound);
}

TEST(ResolvePath, NestedKeyAbsentIsPathNotFound) {
  const JsonValue document = JsonValue::parse(R"({"a":{"b":1}})");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/a/c", out), Status::PathNotFound);
}

// An all-digit token on an object is a key, never an index.
TEST(ResolvePath, IndexOnObjectIsKeyLookup) {
  const JsonValue document = JsonValue::parse(R"({"a":1})");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/0", out), Status::PathNotFound);
}

TEST(ResolvePath, DigitKeyOnObjectResolvesAsKey) {
  const JsonValue document = JsonValue::parse(R"({"0":"zero","01":"zero-one","-":"dash"})");
  const JsonValue* out = nullptr;

  EXPECT_EQ(ResolvePath(document, "/0", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->get<std::string>(), "zero");

  out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/01", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->get<std::string>(), "zero-one");

  out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/-", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->get<std::string>(), "dash");
}

TEST(ResolvePath, KeyIsCaseSensitiveAndNotTrimmed) {
  const JsonValue document = JsonValue::parse(R"({"Name":1," pad ":2})");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/name", out), Status::PathNotFound);
  EXPECT_EQ(ResolvePath(document, "/ pad", out), Status::PathNotFound);
  EXPECT_EQ(ResolvePath(document, "/ pad ", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->get<std::int32_t>(), 2);
}

TEST(ResolvePath, DotsAndBracketsInKeysNeedNoEscaping) {
  const JsonValue document = JsonValue::parse(R"({"@odata.context":"ctx","items[0]":"literal"})");
  const JsonValue* out = nullptr;

  EXPECT_EQ(ResolvePath(document, "/@odata.context", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->get<std::string>(), "ctx");

  out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/items[0]", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->get<std::string>(), "literal");
}

TEST(ResolvePath, TildeEscapesAddressKeysWithSlashAndTilde) {
  const JsonValue document = JsonValue::parse(R"({"a/b":1,"m~n":2})");
  const JsonValue* out = nullptr;

  EXPECT_EQ(ResolvePath(document, "/a~1b", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->get<std::int32_t>(), 1);

  out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/m~0n", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->get<std::int32_t>(), 2);
}

// "/~01" addresses the key "~1", not the key "/".
TEST(ResolvePath, TildeZeroOneAddressesTildeOneKeyNotSlashKey) {
  const JsonValue document = JsonValue::parse(R"({"/":"slash","~1":"tilde-one"})");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/~01", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->get<std::string>(), "tilde-one");

  out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/~1", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->get<std::string>(), "slash");
}

TEST(ResolvePath, EscapedKeyIsNotFoundWhenUnescapedFormIsUsed) {
  const JsonValue document = JsonValue::parse(R"({"a/b":1})");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/a/b", out), Status::PathNotFound);
}

// ---------------------------------------------------------------------------
// ResolvePath -- empty keys.
// ---------------------------------------------------------------------------

TEST(ResolvePath, SlashAloneAddressesEmptyKeyAtRoot) {
  const JsonValue document = JsonValue::parse(R"({"":"empty","a":1})");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->get<std::string>(), "empty");
}

TEST(ResolvePath, SlashAloneIsPathNotFoundWithoutEmptyKey) {
  const JsonValue document = JsonValue::parse(R"({"a":1})");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/", out), Status::PathNotFound);
}

TEST(ResolvePath, SlashAloneDiffersFromEmptyPath) {
  const JsonValue document = JsonValue::parse(R"({"":"inner"})");
  const JsonValue* whole = nullptr;
  const JsonValue* inner = nullptr;
  ASSERT_EQ(ResolvePath(document, "", whole), Status::Ok);
  ASSERT_EQ(ResolvePath(document, "/", inner), Status::Ok);
  EXPECT_NE(whole, inner);
  EXPECT_TRUE(whole->is_object());
  EXPECT_TRUE(inner->is_string());
}

TEST(ResolvePath, TrailingSlashAddressesEmptyKeyInside) {
  const JsonValue document = JsonValue::parse(R"({"x":{"":2}})");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/x/", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->get<std::int32_t>(), 2);
}

TEST(ResolvePath, TrailingSlashWithoutEmptyKeyIsPathNotFound) {
  const JsonValue document = JsonValue::parse(R"({"x":{}})");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/x/", out), Status::PathNotFound);
}

TEST(ResolvePath, DoubleSlashThenIndexAddressesEmptyKeyThenElement) {
  const JsonValue document = JsonValue::parse(R"({"":[10,20]})");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "//1", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->get<std::int32_t>(), 20);
}

TEST(ResolvePath, DoubleSlashThenZeroOnObjectIsKeyLookup) {
  const JsonValue document = JsonValue::parse(R"({"":{"0":"key-zero"}})");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "//0", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->get<std::string>(), "key-zero");
}

TEST(ResolvePath, EmptyKeyOnArrayIsTypeMismatch) {
  const JsonValue document = JsonValue::array({1, 2, 3});
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/", out), Status::TypeMismatch);
}

TEST(ResolvePath, EmptyKeyOnScalarIsTypeMismatch) {
  const JsonValue document = 42;
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/", out), Status::TypeMismatch);
}

// ---------------------------------------------------------------------------
// ResolvePath -- array tokens.
// ---------------------------------------------------------------------------

TEST(ResolvePath, IndexOnArrayOutOfRangeIsIndexOutOfRange) {
  const JsonValue document = JsonValue::array({1, 2, 3});
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/5", out), Status::IndexOutOfRange);
}

TEST(ResolvePath, IndexEqualToSizeIsIndexOutOfRange) {
  const JsonValue document = JsonValue::array({1, 2, 3});
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/3", out), Status::IndexOutOfRange);
}

TEST(ResolvePath, EmptyArrayIndexZeroIsIndexOutOfRange) {
  const JsonValue document = JsonValue::parse(R"({"items":[]})");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/items/0", out), Status::IndexOutOfRange);
}

TEST(ResolvePath, DashOnArrayIsIndexOutOfRange) {
  const JsonValue document = JsonValue::array({1, 2, 3});
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/-", out), Status::IndexOutOfRange);
}

TEST(ResolvePath, DashOnEmptyArrayIsIndexOutOfRange) {
  const JsonValue document = JsonValue::array();
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/-", out), Status::IndexOutOfRange);
}

TEST(ResolvePath, NonCanonicalIndexOnArrayIsTypeMismatch) {
  const StatusCase cases[] = {
      {"/01", Status::TypeMismatch},
      {"/00", Status::TypeMismatch},
      {"/-1", Status::TypeMismatch},
      {"/-0", Status::TypeMismatch},
      {"/+1", Status::TypeMismatch},
      {"/x", Status::TypeMismatch},
      {"/1x", Status::TypeMismatch},
      {"/1.0", Status::TypeMismatch},
      {"/ 1", Status::TypeMismatch},
      {"/1 ", Status::TypeMismatch},
      {"/", Status::TypeMismatch},
  };
  const JsonValue document = JsonValue::array({1, 2, 3});
  for (const StatusCase& c : cases) {
    SCOPED_TRACE(c.path);
    const JsonValue* out = nullptr;
    EXPECT_EQ(ResolvePath(document, c.path, out), c.status);
  }
}

TEST(ResolvePath, KeyOnArrayIsTypeMismatch) {
  const JsonValue document = JsonValue::array({1, 2, 3});
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/key", out), Status::TypeMismatch);
}

TEST(ResolvePath, IndexAtUint32BoundaryIsIndexOutOfRange) {
  const JsonValue document = JsonValue::array({1, 2, 3});
  const StatusCase cases[] = {
      {"/4294967295", Status::IndexOutOfRange},
      {"/4294967296", Status::IndexOutOfRange},
      {"/99999999999", Status::IndexOutOfRange},
  };
  for (const StatusCase& c : cases) {
    SCOPED_TRACE(c.path);
    const JsonValue* out = nullptr;
    EXPECT_EQ(ResolvePath(document, c.path, out), c.status);
  }
}

TEST(ResolvePath, VeryLongDigitStringOnArrayIsIndexOutOfRange) {
  const JsonValue document = JsonValue::array({1, 2, 3});
  const std::string path = "/" + std::string(200, '9');
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, path, out), Status::IndexOutOfRange);
}

TEST(ResolvePath, OversizedDigitStringOnObjectIsKeyLookup) {
  const std::string key(200, '9');
  JsonValue document = JsonValue::object();
  document[key] = "big";
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/" + key, out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->get<std::string>(), "big");
}

// ---------------------------------------------------------------------------
// ResolvePath -- scalars and null in the middle of a path.
// ---------------------------------------------------------------------------

TEST(ResolvePath, KeyOnScalarIsTypeMismatch) {
  const JsonValue document = 42;
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/a", out), Status::TypeMismatch);
}

TEST(ResolvePath, MidPathScalarIsTypeMismatch) {
  const JsonValue document = JsonValue::parse(R"({"user":{"name":"Ann"}})");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/user/name/first", out), Status::TypeMismatch);
}

TEST(ResolvePath, MidPathNumberAndBoolAreTypeMismatch) {
  const JsonValue document = JsonValue::parse(R"({"n":1,"b":true})");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/n/x", out), Status::TypeMismatch);
  EXPECT_EQ(ResolvePath(document, "/b/0", out), Status::TypeMismatch);
}

// Mid-path null falls under the "container expected" rule, not NullValue --
// NullValue is reserved for terminal typed reads.
TEST(ResolvePath, MidPathNullIsTypeMismatch) {
  const JsonValue document = JsonValue::parse(R"({"user":null})");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/user/name", out), Status::TypeMismatch);
}

// ---------------------------------------------------------------------------
// ResolvePath -- malformed paths and argument validation.
// ---------------------------------------------------------------------------

TEST(ResolvePath, OldDotAndBracketSyntaxIsSyntaxError) {
  const JsonValue document = JsonValue::parse(R"({"data":{"items":[{"name":"Ann"}]},"a":1})");
  const char* const paths[] = {"data.items[0].name", "a", "[0]", "data/items"};
  for (const char* path : paths) {
    SCOPED_TRACE(path);
    const JsonValue* out = nullptr;
    EXPECT_EQ(ResolvePath(document, path, out), Status::PathSyntaxError);
  }
}

TEST(ResolvePath, BadTildeEscapeIsSyntaxError) {
  const JsonValue document = JsonValue::parse(R"({"a":1,"a~2":2})");
  const char* const paths[] = {"/a~2", "/a~", "/~"};
  for (const char* path : paths) {
    SCOPED_TRACE(path);
    const JsonValue* out = nullptr;
    EXPECT_EQ(ResolvePath(document, path, out), Status::PathSyntaxError);
  }
}

// A syntax error later in the path wins over a failure the walk would hit first.
TEST(ResolvePath, SyntaxErrorIsReportedBeforeDocumentIsWalked) {
  const JsonValue object = JsonValue::parse(R"({"a":1})");
  const JsonValue scalar = 42;
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(object, "/missing/a~2", out), Status::PathSyntaxError);
  EXPECT_EQ(ResolvePath(scalar, "/a/b~", out), Status::PathSyntaxError);
}

TEST(ResolvePath, NullDataStringViewIsInvalidArgument) {
  const JsonValue document = JsonValue::parse(R"({"a":1})");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, std::string_view{}, out), Status::InvalidArgument);
}

TEST(ResolvePath, OutIsLeftUnchangedOnEveryFailure) {
  const JsonValue document = JsonValue::parse(R"({"a":[1],"n":null})");
  const JsonValue sentinel = "sentinel";
  const char* const paths[] = {
      "a",         // PathSyntaxError
      "/a~2",      // PathSyntaxError
      "/missing",  // PathNotFound
      "/a/5",      // IndexOutOfRange
      "/a/-",      // IndexOutOfRange
      "/a/x",      // TypeMismatch
      "/n/x",      // TypeMismatch
  };
  for (const char* path : paths) {
    SCOPED_TRACE(path);
    const JsonValue* out = &sentinel;
    EXPECT_NE(ResolvePath(document, path, out), Status::Ok);
    EXPECT_EQ(out, &sentinel);
  }

  const JsonValue* out = &sentinel;
  EXPECT_EQ(ResolvePath(document, std::string_view{}, out), Status::InvalidArgument);
  EXPECT_EQ(out, &sentinel);
}

// ---------------------------------------------------------------------------
// ResolvePath -- aliasing and document order.
// ---------------------------------------------------------------------------

// ResolvePath must alias the caller's document, never copy -- assert address
// equality against the equivalent nlohmann access chain.
TEST(ResolvePath, ResolvedPointerAliasesDocumentRatherThanCopying) {
  const JsonValue document = JsonValue::parse(R"({"data":{"items":[{"name":"Ann"}]}})");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/data/items/0/name", out), Status::Ok);
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out, &document.at("data").at("items").at(0).at("name"));
}

TEST(ResolvePath, ResolvedPointerAliasesContainerNodes) {
  const JsonValue document = JsonValue::parse(R"({"data":{"items":[1,2]}})");
  const JsonValue* out = nullptr;
  EXPECT_EQ(ResolvePath(document, "/data/items", out), Status::Ok);
  EXPECT_EQ(out, &document.at("data").at("items"));
}

TEST(ResolvePath, ResolutionDoesNotDependOnKeyOrderInDocument) {
  const JsonValue forward = JsonValue::parse(R"({"a":1,"b":2,"c":{"x":3,"y":4}})");
  const JsonValue reversed = JsonValue::parse(R"({"c":{"y":4,"x":3},"b":2,"a":1})");
  const char* const paths[] = {"/a", "/b", "/c/x", "/c/y"};
  const std::int32_t expected[] = {1, 2, 3, 4};
  for (std::size_t i = 0; i < 4; ++i) {
    SCOPED_TRACE(paths[i]);
    const JsonValue* fromForward = nullptr;
    const JsonValue* fromReversed = nullptr;
    ASSERT_EQ(ResolvePath(forward, paths[i], fromForward), Status::Ok);
    ASSERT_EQ(ResolvePath(reversed, paths[i], fromReversed), Status::Ok);
    EXPECT_EQ(fromForward->get<std::int32_t>(), expected[i]);
    EXPECT_EQ(fromReversed->get<std::int32_t>(), expected[i]);
  }
}

TEST(ResolvePath, NonAlphabeticalDocumentOrderStillResolvesInPlace) {
  const JsonValue document = JsonValue::parse(R"({"z":1,"a":2})");
  const JsonValue* out = nullptr;

  EXPECT_EQ(ResolvePath(document, "/a", out), Status::Ok);
  EXPECT_EQ(out, &document.at("a"));

  EXPECT_EQ(ResolvePath(document, "/z", out), Status::Ok);
  EXPECT_EQ(out, &document.at("z"));
}

// ---------------------------------------------------------------------------
// ResolvePath -- RFC 6901 section 5 examples.
// ---------------------------------------------------------------------------

TEST(ResolvePath, Rfc6901SectionFiveExamples) {
  const JsonValue document = JsonValue::parse(
      R"({"foo":["bar","baz"],"":0,"a/b":1,"c%d":2,"e^f":3,"g|h":4,"i\\j":5,"k\"l":6," ":7,"m~n":8})");

  const JsonValue* out = nullptr;

  ASSERT_EQ(ResolvePath(document, "", out), Status::Ok);
  EXPECT_EQ(out, &document);

  ASSERT_EQ(ResolvePath(document, "/foo", out), Status::Ok);
  EXPECT_EQ(*out, JsonValue::parse(R"(["bar","baz"])"));

  ASSERT_EQ(ResolvePath(document, "/foo/0", out), Status::Ok);
  EXPECT_EQ(out->get<std::string>(), "bar");

  const struct {
    const char* path;
    std::int32_t value;
  } numeric[] = {
      {"/", 0},       {"/a~1b", 1}, {"/c%d", 2}, {"/e^f", 3}, {"/g|h", 4},
      {"/i\\j", 5},   {"/k\"l", 6}, {"/ ", 7},   {"/m~0n", 8},
  };
  for (const auto& c : numeric) {
    SCOPED_TRACE(c.path);
    out = nullptr;
    ASSERT_EQ(ResolvePath(document, c.path, out), Status::Ok);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->get<std::int32_t>(), c.value);
  }
}

TEST(ParsePath, Rfc6901SectionFiveExamplesDecodeToTheirKeys) {
  const TokenCase cases[] = {
      {"", {}},
      {"/foo", {"foo"}},
      {"/foo/0", {"foo", "0"}},
      {"/", {""}},
      {"/a~1b", {"a/b"}},
      {"/c%d", {"c%d"}},
      {"/e^f", {"e^f"}},
      {"/g|h", {"g|h"}},
      {"/i\\j", {"i\\j"}},
      {"/k\"l", {"k\"l"}},
      {"/ ", {" "}},
      {"/m~0n", {"m~n"}},
  };
  for (const TokenCase& c : cases) {
    SCOPED_TRACE(c.path);
    std::vector<std::string> tokens = {"stale"};
    EXPECT_EQ(ParsePath(c.path, tokens), Status::Ok);
    EXPECT_EQ(tokens, c.tokens);
  }
}
