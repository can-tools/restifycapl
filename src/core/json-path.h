// json-path.h -- dot+bracket JSON path parsing and resolution (src/core,
// level 0, see docs/work/stage-08-core-pure-logic/plans/plan.md §6 CPP-3).
//
// D1 -- interim syntax caveat: `data.items[0].name` (dot for object keys,
// brackets for array indices) is explicitly interim, not the target
// architecture. Struct mapping (currently deferred, see CLAUDE.md's Scope
// section) is the intended eventual successor for structured access. This
// is a named, stated caveat -- not a silent permanent contract -- so a
// future struct-mapping stage is free to supersede this syntax without
// archaeology.
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
// argument but semantically invalid as a path, so it is PathSyntaxError, per
// the edge-case table below, not InvalidArgument.
//
// Edge cases (defaults fixed by plan.md §6 CPP-3, not implementer discretion):
//
// | Case                                             | Resolution        |
// |---------------------------------------------------|-------------------|
// | Empty path ""                                      | PathSyntaxError   |
// | Leading dot, ".items[0]"                            | PathSyntaxError   |
// | Top-level index on a root array, "[0].name"          | Valid -- Ok       |
// | Negative or non-numeric index, "[-1]", "[x]"          | PathSyntaxError   |
// | Index too large for uint32_t                          | PathSyntaxError   |
// | Unclosed bracket "items[0", empty brackets "[]",        | PathSyntaxError   |
// | double dot "..", trailing "."                            |                   |
// | Key containing "." or "["                                  | Unreachable via this syntax -- a documented limitation of the interim syntax (D1), not an error path |
// | Whitespace                                                    | Not trimmed; part of the key |
Status ParsePath(std::string_view path, std::vector<PathSegment>& out);

// ResolvePath -- parses `path` (see ParsePath) then walks `document`,
// applying the segment-kind resolution rule below at every step.
//
// §5.2 segment-kind resolution rule (verbatim, plan.md §5.2):
//
// | Segment kind | Container kind at that point                         | Result          |
// |--------------|--------------------------------------------------------|-----------------|
// | [n]          | array, n >= size                                        | IndexOutOfRange |
// | .key         | object, key absent                                      | PathNotFound    |
// | [n]          | object (not an array)                                    | TypeMismatch    |
// | .key         | array (not an object)                                     | TypeMismatch    |
// | either       | scalar (string/number/bool) where a container was expected | TypeMismatch    |
//
// A JSON null encountered mid-path where a container was expected falls
// under the last row (TypeMismatch) -- NullValue is reserved for a terminal
// node read by a typed accessor (see type-conversion.h's To* family), not
// for a traversal failure here.
//
// On success, `out` points INTO `document` -- a non-owning pointer whose
// lifetime is tied to `document`'s. The caller must keep `document` alive
// for as long as `out` is used; ResolvePath allocates no storage of its own
// for the resolved value.
Status ResolvePath(const nlohmann::json& document, std::string_view path,
                    const nlohmann::json*& out);
