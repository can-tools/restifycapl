// type-conversion.h -- JSON <-> C++ leaf-value conversion (src/core, level 0,
// see docs/work/stage-08-core-pure-logic/plans/plan.md §6 CPP-2).
//
// Two families, deliberately kept apart (plan.md D2):
//   To*    -- const nlohmann::json& in, typed C++ value out. Strict: a JSON
//             string arriving where a number was requested is TypeMismatch,
//             never a silent fall-through to ParseLong/ParseDouble.
//   Parse* -- std::string_view in (raw text, e.g. a CAPL-supplied string),
//             typed C++ value out. A deliberately named lenient escape
//             hatch -- never called internally by the To* family.
// Keeping these separate means a caller that reaches for ToLong on a JSON
// string gets a loud TypeMismatch instead of silently accepting "42" as if
// it were a number.
//
// ToText vs ValueToText: ToText is the strict string accessor (JSON string
// only; every other JSON type fails). ValueToText is the lenient leaf
// stringifier used when the caller wants "whatever is there" as text
// regardless of its JSON type. Both exist because callers need both shapes
// -- a typed field accessor that fails loudly on the wrong type, and a
// generic dumper that always produces something for any leaf.
//
// §5.3 ValueToText leaf-stringify behavior (verbatim, plan.md §5.3):
//
// | Input node                        | Output                     | Status       |
// |------------------------------------|----------------------------|--------------|
// | JSON null                          | the text "null"            | Ok           |
// | JSON number 42                     | "42"                        | Ok           |
// | JSON true                          | "true"                      | Ok           |
// | JSON string                        | the string's own contents  | Ok           |
// | JSON object or array (non-leaf)    | (out untouched)             | TypeMismatch |
//
// null -> "null"/Ok guarantees ValueToText always succeeds on a genuine
// leaf, the same guarantee ToText gives for strings -- no special-cased
// failure for null. Container -> TypeMismatch is deliberate and loud:
// src/mapping/json-flatten.* (a later stage) recurses through containers
// itself and only ever calls ValueToText on leaves it has already
// discovered, so a container arriving here means a bug in the caller's
// recursion, not a normal runtime case -- it must not be silently
// serialized into a nested JSON string that would then look like a
// flattened "value".
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "core/status.h"
#include "json.hpp"

// JSON integer/float number in int32_t range -> Ok. Out of range ->
// NumericOverflow. A number with a fractional part (3.7) -> NotIntegral,
// never truncated. A whole-valued float (3.0) -> Ok/3. null -> NullValue.
// Any non-number -> TypeMismatch.
Status ToLong(const nlohmann::json& value, std::int32_t& out);

// Any JSON number (integer or float) -> Ok. null -> NullValue. Non-number
// -> TypeMismatch.
Status ToDouble(const nlohmann::json& value, double& out);

// JSON true/false only -> Ok. null -> NullValue. Everything else,
// including numeric 0/1, -> TypeMismatch (strict; no truthiness).
Status ToBool(const nlohmann::json& value, bool& out);

// JSON string only -> Ok, unquoted contents. null -> NullValue. Number,
// bool, object, array -> TypeMismatch.
Status ToText(const nlohmann::json& value, std::string& out);

// Lenient leaf stringifier -- see the §5.3 table above.
Status ValueToText(const nlohmann::json& value, std::string& out);

// Lenient text -> int32_t. Empty text -> InvalidArgument. Unparseable ->
// ParseError. Trailing characters after a valid numeric prefix -> ParseError
// (strict-tail; "lenient" refers to accepting raw text at all, not to
// tolerating garbage). Out of int32_t range -> NumericOverflow. Text with a
// fractional part (e.g. "3.5") -> NotIntegral, for the same reason ToLong
// returns NotIntegral on a non-integral JSON number -- never ParseError for
// this specific case, and never silent truncation. Never called from the
// To* family.
Status ParseLong(std::string_view text, std::int32_t& out);

// Lenient text -> double. Empty text -> InvalidArgument. Unparseable ->
// ParseError. Trailing characters after a valid numeric prefix ->
// ParseError. Out of double range -> NumericOverflow. Never called from the
// To* family.
Status ParseDouble(std::string_view text, double& out);
