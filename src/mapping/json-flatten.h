// json-flatten.h -- JSON text to a flat list of JSON Pointer leaf entries; see docs/json-flatten.md.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/json-value.h"
#include "core/status.h"

inline constexpr std::size_t kMaxJsonInputBytes = 1048576;
inline constexpr std::size_t kMaxJsonDepth = 64;
inline constexpr std::size_t kMaxFlatEntries = 10000;

enum class JsonEntryType : std::int32_t {
  None = 0,
  String = 1,
  Number = 2,
  Bool = 3,
  Null = 4,
  EmptyObject = 5,
  EmptyArray = 6,
};

struct FlatEntry {
  std::string key;
  std::string value;
  JsonEntryType type = JsonEntryType::None;
};

struct FlattenResult {
  JsonValue document;
  std::vector<FlatEntry> entries;
};

// FlattenJson: parses `text` and lists its leaves as JSON Pointer keys in document order; `out` is replaced only on Ok; see docs/json-flatten.md.
Status FlattenJson(std::string_view text, FlattenResult& out);

// DescribeLeaf: text and entry type of a scalar, null, or empty container; TypeMismatch for a non-empty container.
Status DescribeLeaf(const JsonValue& node, std::string& text, JsonEntryType& type);
