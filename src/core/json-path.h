#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/status.h"
#include "core/json-value.h"

// ParsePath: splits an RFC 6901 JSON Pointer into decoded tokens, without a document; see docs/json-path.md.
Status ParsePath(std::string_view path, std::vector<std::string>& out);

// ResolvePath: walks `document` along `path`; on Ok, `out` points into `document` and is valid only while it lives; see docs/json-path.md.
Status ResolvePath(const JsonValue& document, std::string_view path,
                   const JsonValue*& out);
