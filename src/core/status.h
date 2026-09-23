// status.h -- shared Status enum for src/core/. Values 0/-1/-2/-3 are
// pre-existing, already-shipped codes and must never be renumbered
// (capl-export-contract). See docs/status-codes.md for the full
// numbering-range rationale.
#pragma once

#include <cstdint>

enum class Status : int32_t {
  Ok = 0,
  InvalidArgument = -1,
  BufferTooSmall = -2,
  VersionResourceUnavailable = -3,  // exports.cpp only; src/core/ can never produce this
  // -4..-9 reserved, currently empty, for future module-local glue codes
  ParseError = -10,          // reserved for JSON parse failures; no caller yet
  PathSyntaxError = -11,     // ParsePath: malformed path text, no document needed
  PathNotFound = -12,        // object key absent
  IndexOutOfRange = -13,     // array index >= size
  TypeMismatch = -14,        // segment/container kind mismatch (see json-path.h's rule), or ValueToText on non-leaf node
  NullValue = -15,           // JSON null encountered where a typed value was requested (kept distinct from TypeMismatch -- a null gets a default silently, a wrong-typed value is logged or treated as an error)
  NumericOverflow = -16,     // valid JSON number, out of int32_t range for ToLong
  NotIntegral = -17,         // valid JSON number, has a fractional part, requested as an integer type -- must NOT silently truncate
};
