// json-path.h -- dot+bracket JSON path parsing and resolution (src/core,
// level 0). `data.items[0].name` syntax is explicitly interim, not the
// target architecture -- struct mapping (currently deferred, see CLAUDE.md's
// Scope section) is the intended eventual successor for structured access.
// See docs/json-path.md for the full edge-case and resolution-rule tables.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/status.h"
#include "json.hpp"

// One parsed path element: either an object-key hop or an array-index hop.
// `key` is populated only when kind == Key; `index` only when kind == Index.
struct PathSegment {
  enum class Kind { Key, Index };
  Kind kind;
  std::string key;
  std::uint32_t index;
};

// ParsePath -- document-free: validates path text only, never touches a
// document. Returns only Ok, InvalidArgument, or PathSyntaxError.
//
// InvalidArgument fires only for a structurally invalid std::string_view
// itself (path.data() == nullptr, e.g. a default-constructed string_view) --
// the same boundary-guard shape as CopyToBuffer's nullptr-buffer check
// elsewhere in src/core/. A genuinely empty text ("") is well-formed as an
// argument but semantically invalid as a path, so it is PathSyntaxError
// instead. See docs/json-path.md for the full edge-case table.
Status ParsePath(std::string_view path, std::vector<PathSegment>& out);

// ResolvePath -- parses `path` (see ParsePath) then walks `document`,
// applying the segment-kind resolution rule at every step. See
// docs/json-path.md for the full rule table.
//
// A JSON null encountered mid-path where a container was expected is a
// TypeMismatch -- NullValue is reserved for a terminal node read by a typed
// accessor (see type-conversion.h's To* family), not for a traversal
// failure here.
//
// On success, `out` points INTO `document` -- a non-owning pointer whose
// lifetime is tied to `document`'s. The caller must keep `document` alive
// for as long as `out` is used; ResolvePath allocates no storage of its own
// for the resolved value.
Status ResolvePath(const nlohmann::json& document, std::string_view path,
                    const nlohmann::json*& out);
