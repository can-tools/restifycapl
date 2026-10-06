#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/json-value.h"
#include "core/status.h"

// ParsePath: splits an RFC 6901 JSON Pointer into decoded tokens, without a
// document; see docs/json-path.md.
using PathTokens = std::vector<std::string>;
Status ParsePath(std::string_view path, PathTokens& out);

// ResolvePath: walks `document` along `path`; on Ok, `out` points into
// `document` and is valid only while it lives; see docs/json-path.md.
Status ResolvePath(const JsonValue& document, std::string_view path,
                   const JsonValue*& out);

// Same walk over tokens from ParsePath; never parses, so it cannot fail on syntax.
// A braced `{}` argument is ambiguous with the string overload; write `PathTokens{}` for an empty list.
Status ResolvePath(const JsonValue& document, const PathTokens& tokens,
                   const JsonValue*& out);
