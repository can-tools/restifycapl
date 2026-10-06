// Coverage for src/mapping/json-quotes.*.

#include "mapping/json-quotes.h"

#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "core/status.h"
#include "../test-support/status-print.h"

namespace {

std::string Convert(std::string_view in) {
  std::string out = "stale";
  EXPECT_EQ(ConvertApostropheStrings(in, out), Status::Ok);
  return out;
}

void ExpectConverts(std::string_view in, std::string_view expected) {
  SCOPED_TRACE(std::string(in));
  EXPECT_EQ(Convert(in), std::string(expected));
}

void ExpectUnchanged(std::string_view in) { ExpectConverts(in, in); }

}  // namespace

TEST(ConvertApostropheStrings, ApostropheKeyAndValueBecomeDoubleQuoted) {
  ExpectConverts("{'a':'b'}", R"({"a":"b"})");
}

TEST(ConvertApostropheStrings, NestedObjectsAndArraysAreConverted) {
  ExpectConverts("{'a':{'b':['c','d',{'e':'f'}]},'g':[['h']]}",
                 R"({"a":{"b":["c","d",{"e":"f"}]},"g":[["h"]]})");
}

TEST(ConvertApostropheStrings, MixedNotationsInOneDocumentAreConverted) {
  ExpectConverts(R"({'a':"b","c":'d'})", R"({"a":"b","c":"d"})");
}

TEST(ConvertApostropheStrings, EmptyAndAdjacentApostropheStrings) {
  ExpectConverts("['','']", R"(["",""])");
  ExpectConverts("['a''b']", R"(["a""b"])");
}

TEST(ConvertApostropheStrings, NumbersLiteralsStructureAndWhitespaceOutsideStringsAreUntouched) {
  ExpectConverts(" \t{ 'a' :\r\n [ 1 , -2.5e+3 , true , false , null , 'x' ] } ",
                 " \t{ \"a\" :\r\n [ 1 , -2.5e+3 , true , false , null , \"x\" ] } ");
}

TEST(ConvertApostropheStrings, TextWithoutApostropheOutsideDoubleQuotedStringsIsByteIdentical) {
  ExpectUnchanged(R"({"a":[1,2.5,-3e2,true,false,null,"s"],"b":{"c":""}})");
  ExpectUnchanged(" \t[ 1 ,\r\n 2 ] ");
  ExpectUnchanged("42");
  ExpectUnchanged("true");
  ExpectUnchanged("");
  ExpectUnchanged("not json at all");
  ExpectUnchanged("\xEF\xBB\xBF[\"\xC3\xA9\"]");
}

TEST(ConvertApostropheStrings, ApostropheInsideADoubleQuotedStringIsAnOrdinaryCharacter) {
  ExpectUnchanged(R"("it's")");
  ExpectUnchanged(R"({"note":"it's ok","k's":"'quoted'"})");
  ExpectConverts(R"({'a':"it's"})", R"({"a":"it's"})");
}

TEST(ConvertApostropheStrings, EscapedQuoteInsideADoubleQuotedStringDoesNotEndIt) {
  ExpectUnchanged(R"({"a":"say \"it's\" now"})");
  ExpectConverts(R"({"a":"back\\","b":'x'})", R"({"a":"back\\","b":"x"})");
}

TEST(ConvertApostropheStrings, EscapedApostropheInsideADoubleQuotedStringIsCopiedUnchanged) {
  ExpectUnchanged(R"("a\'b")");
  ExpectConverts(R"({"a":"x\'y",'b':'c'})", R"({"a":"x\'y","b":"c"})");
}

TEST(ConvertApostropheStrings, RawDoubleQuoteInsideAnApostropheStringIsEscaped) {
  ExpectConverts(R"('say "hi"')", R"("say \"hi\"")");
  ExpectConverts(R"({'a':'"'})", R"({"a":"\""})");
}

TEST(ConvertApostropheStrings, EscapedApostropheBecomesALiteralApostrophe) {
  ExpectConverts(R"('it\'s')", R"("it's")");
  ExpectConverts(R"({'note':'it\'s ok'})", R"({"note":"it's ok"})");
  ExpectConverts(R"('\'')", R"("'")");
}

TEST(ConvertApostropheStrings, EscapedBackslashInsideAnApostropheStringIsKept) {
  ExpectConverts(R"('a\\b')", R"("a\\b")");
}

TEST(ConvertApostropheStrings, EscapedBackslashBeforeAnApostropheClosesTheString) {
  ExpectConverts(R"('a\\'b')", R"("a\\"b")");
}

TEST(ConvertApostropheStrings, OtherEscapePairsInsideAnApostropheStringAreKept) {
  ExpectConverts(R"('a\nb')", R"("a\nb")");
  ExpectConverts(R"('a\tb\/c\"d')", R"("a\tb\/c\"d")");
}

TEST(ConvertApostropheStrings, JsonUnicodeEscapeInsideAnApostropheStringIsNotInterpreted) {
  const std::string in = "'\\u0041'";
  ASSERT_EQ(in.size(), 8u);
  ASSERT_EQ(in.substr(1, 6), "\\u0041");
  EXPECT_EQ(Convert(in), "\"\\u0041\"");
}

TEST(ConvertApostropheStrings, UnterminatedApostropheStringIsLeftAsItIs) {
  ExpectConverts("'abc", "\"abc");
  ExpectConverts("{'a':'b", "{\"a\":\"b");
  ExpectConverts("'abc\\", "\"abc\\");
  ExpectConverts("'", "\"");
}

TEST(ConvertApostropheStrings, UnterminatedDoubleQuotedStringIsLeftAsItIs) {
  ExpectUnchanged("\"abc");
  ExpectUnchanged("\"abc\\");
  ExpectUnchanged("{\"a\":\"it's");
}

TEST(ConvertApostropheStrings, EmptyInputGivesEmptyOutput) {
  std::string out = "stale";
  EXPECT_EQ(ConvertApostropheStrings(std::string_view(), out), Status::Ok);
  EXPECT_TRUE(out.empty());
}

TEST(ConvertApostropheStrings, OutputReplacesWhateverWasInTheOutputString) {
  std::string out = "previous content that must not survive";
  EXPECT_EQ(ConvertApostropheStrings("['a']", out), Status::Ok);
  EXPECT_EQ(out, R"(["a"])");
}

TEST(ConvertApostropheStrings, NoNulIsAddedAndAnInputNulIsKeptInPlace) {
  const std::string plain = Convert("{'a':'b\"c'}");
  EXPECT_EQ(plain.find('\0'), std::string::npos);

  const std::string withNul = Convert(std::string("['a\0b']", 7));
  EXPECT_EQ(withNul, std::string("[\"a\0b\"]", 7));
}

TEST(ConvertApostropheStrings, OutputCanBeLongerThanTheInput) {
  const std::string in = "'\"\"\"'";
  const std::string out = Convert(in);
  EXPECT_EQ(out, "\"\\\"\\\"\\\"\"");
  EXPECT_GT(out.size(), in.size());
}

TEST(ConvertApostropheStrings, ConvertingTheOutputAgainChangesNothing) {
  const std::string once = Convert(R"({'a':'it\'s "x"','b':"it's"})");
  EXPECT_EQ(once, R"({"a":"it's \"x\"","b":"it's"})");
  EXPECT_EQ(Convert(once), once);
}
