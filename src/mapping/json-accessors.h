// json-accessors.h -- strict typed reads at a JSON Pointer; see docs/capl-json-surface.md.
#pragma once

#include <cstdint>
#include <string_view>

#include "core/json-path.h"
#include "core/json-value.h"
#include "core/status.h"

// Path errors pass through from ResolvePath; value errors from the matching To*. `out` is written only on Ok.
// The PathTokens overloads take a path already parsed by ParsePath and never parse.
Status ReadLongAt(const JsonValue& document, std::string_view path, std::int32_t& out);
Status ReadDoubleAt(const JsonValue& document, std::string_view path, double& out);
Status ReadBoolAt(const JsonValue& document, std::string_view path, bool& out);

// Arrays only: an object or scalar -> TypeMismatch, null -> NullValue.
Status CountElementsAt(const JsonValue& document, std::string_view path, std::uint32_t& out);

Status ReadLongAt(const JsonValue& document, const PathTokens& tokens, std::int32_t& out);
Status ReadDoubleAt(const JsonValue& document, const PathTokens& tokens, double& out);
Status ReadBoolAt(const JsonValue& document, const PathTokens& tokens, bool& out);
Status CountElementsAt(const JsonValue& document, const PathTokens& tokens, std::uint32_t& out);
