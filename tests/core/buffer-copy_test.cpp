// Coverage for src/core/buffer-copy.*.

#include "core/buffer-copy.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

#include <gtest/gtest.h>

#include "core/status.h"

// ---------------------------------------------------------------------------
// CopyToBuffer
// ---------------------------------------------------------------------------

TEST(CopyToBuffer, ExactFitLeavesRoomForNul) {
  std::array<char, 5> buffer{};
  EXPECT_EQ(CopyToBuffer("abcd", buffer.data(), static_cast<std::uint32_t>(buffer.size())), Status::Ok);
  EXPECT_STREQ(buffer.data(), "abcd");
}

// text.size() == bufferSize leaves no room for the NUL terminator; the
// buffer must be left empty, not truncated.
TEST(CopyToBuffer, OneByteShortIsBufferTooSmallAndLeavesBufferEmpty) {
  std::array<char, 4> buffer{};
  buffer.fill('Z');
  EXPECT_EQ(CopyToBuffer("abcd", buffer.data(), static_cast<std::uint32_t>(buffer.size())), Status::BufferTooSmall);
  EXPECT_EQ(buffer[0], '\0');
}

TEST(CopyToBuffer, ZeroBufferSizeIsInvalidArgument) {
  char buffer[1] = {'Z'};
  EXPECT_EQ(CopyToBuffer("a", buffer, 0), Status::InvalidArgument);
}

TEST(CopyToBuffer, NullBufferIsInvalidArgument) {
  EXPECT_EQ(CopyToBuffer("a", nullptr, 4), Status::InvalidArgument);
}

TEST(CopyToBuffer, EmptyTextIntoOneByteBufferIsOk) {
  char buffer[1] = {'Z'};
  EXPECT_EQ(CopyToBuffer("", buffer, 1), Status::Ok);
  EXPECT_EQ(buffer[0], '\0');
}

// Sentinel byte one past the declared bufferSize must survive untouched --
// proves the copy never writes past bufferSize.
TEST(CopyToBuffer, DoesNotWritePastDeclaredBufferSize) {
  constexpr std::uint32_t kSize = 4;
  std::array<char, kSize + 1> buffer;
  buffer.fill(static_cast<char>(0xCC));

  EXPECT_EQ(CopyToBuffer("abcd", buffer.data(), kSize), Status::BufferTooSmall);
  EXPECT_EQ(buffer[kSize], static_cast<char>(0xCC));
}
