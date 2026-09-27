// Coverage for src/core/input-text.*.

#include "core/input-text.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>

#include <gtest/gtest.h>

#include "core/status.h"

namespace {

constexpr std::string_view kSentinel = "sentinel-untouched";

}  // namespace

// ---------------------------------------------------------------------------
// InvalidArgument
// ---------------------------------------------------------------------------

TEST(BoundedText, ZeroSizeIsInvalidArgumentAndLeavesOutUntouched) {
  std::string_view out = kSentinel;
  const char text[] = "abc";
  EXPECT_EQ(BoundedText(text, 0, out), Status::InvalidArgument);
  EXPECT_EQ(out, kSentinel);
}

TEST(BoundedText, NullPointerIsInvalidArgumentAndLeavesOutUntouched) {
  std::string_view out = kSentinel;
  EXPECT_EQ(BoundedText(nullptr, 4, out), Status::InvalidArgument);
  EXPECT_EQ(out, kSentinel);
}

// ---------------------------------------------------------------------------
// NUL found within bounds
// ---------------------------------------------------------------------------

TEST(BoundedText, NulAtStartYieldsEmptyText) {
  std::string_view out = kSentinel;
  const char text[] = "\0rest";
  EXPECT_EQ(BoundedText(text, static_cast<std::uint32_t>(sizeof(text)), out), Status::Ok);
  EXPECT_EQ(out, std::string_view());
}

TEST(BoundedText, NulInMiddleYieldsTextBeforeIt) {
  std::string_view out = kSentinel;
  const char text[] = "ab\0cd";
  EXPECT_EQ(BoundedText(text, static_cast<std::uint32_t>(sizeof(text)), out), Status::Ok);
  EXPECT_EQ(out, "ab");
}

TEST(BoundedText, NulAsLastByteYieldsFullTextMinusTerminator) {
  const char text[] = "abcd";  // sizeof(text) == 5: 'a','b','c','d','\0'
  std::string_view out = kSentinel;
  EXPECT_EQ(BoundedText(text, static_cast<std::uint32_t>(sizeof(text)), out), Status::Ok);
  EXPECT_EQ(out, "abcd");
}

// ---------------------------------------------------------------------------
// No NUL within the stated size
// ---------------------------------------------------------------------------

TEST(BoundedText, NoNulWithinStatedSizeIsUnterminatedInputText) {
  std::array<char, 4> buffer;
  buffer.fill('x');
  std::string_view out = kSentinel;
  EXPECT_EQ(BoundedText(buffer.data(), static_cast<std::uint32_t>(buffer.size()), out),
            Status::UnterminatedInputText);
}

// A real NUL sits immediately after the stated size, in memory the function
// must never read -- the stated size is the only bound BoundedText may use,
// even when the adjacent byte would have given a "convenient" answer.
TEST(BoundedText, NulJustPastStatedSizeIsStillUnterminatedInputText) {
  std::array<char, 5> buffer;
  buffer.fill('x');
  buffer[4] = '\0';
  std::string_view out = kSentinel;
  EXPECT_EQ(BoundedText(buffer.data(), 4, out), Status::UnterminatedInputText);
}

// ---------------------------------------------------------------------------
// Empty-string convention: size == 1, p[0] == '\0'
// ---------------------------------------------------------------------------

TEST(BoundedText, SizeOneWithNulByteYieldsEmptyText) {
  const char text[] = {'\0'};
  std::string_view out = kSentinel;
  EXPECT_EQ(BoundedText(text, 1, out), Status::Ok);
  EXPECT_EQ(out, std::string_view());
  EXPECT_TRUE(out.empty());
}
