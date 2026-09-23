// type-conversion.h -- JSON <-> C++ leaf-value conversion (src/core, level 0).
//
// To* (const nlohmann::json& in) and Parse* (std::string_view in) are two
// deliberately separate families -- a caller that reaches for ToLong on a
// JSON string gets a loud TypeMismatch instead of Parse*'s lenient text
// parsing silently accepting it as a number. Parse* is never called from
// the To* family for the same reason.
//
// ToText is the strict string accessor; ValueToText is the lenient leaf
// stringifier for "whatever is there" as text, regardless of JSON type.
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

// Lenient leaf stringifier -- see docs/type-conversion.md for the full
// behavior table.
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
