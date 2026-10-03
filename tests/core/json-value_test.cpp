// Coverage for src/core/json-value.h.

#include "core/json-value.h"

#include <string>

#include <gtest/gtest.h>

TEST(JsonValueOrder, ObjectKeepsDocumentOrderNotAlphabetical) {
  const JsonValue value = JsonValue::parse(R"({"c":1,"b":2,"a":3})");
  EXPECT_EQ(value.dump(), R"({"c":1,"b":2,"a":3})");

  std::string keys;
  for (const auto& item : value.items()) {
    keys += item.key();
  }
  EXPECT_EQ(keys, "cba");
}
