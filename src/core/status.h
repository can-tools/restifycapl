// status.h -- shared Status enum for src/core/ (json-path.h, type-conversion.h,
// buffer-copy.h). Lives in its own header rather than any one of them,
// because all three need it and any choice of host would create an
// arbitrary include dependency (see plan.md §5.1,
// docs/work/stage-08-core-pure-logic/plans/plan.md).
//
// 0 / -1 / -2 / -3 are pre-existing, already-shipped codes from
// restifyGetVersion (src/module/exports.cpp) -- absorbed into this enum
// rather than renumbered. Their values must never change
// (capl-export-contract: CAPL scripts already depend on them at runtime).
//
// -4..-9 are reserved for future module-local glue codes and are currently
// empty.
//
// VersionResourceUnavailable (-3) is declared here so the numbering space
// has a single owner, but it is unreachable from anything in src/core/ --
// only src/module/exports.cpp can produce it (the Win32 version-resource
// read that yields it has no place in CANoe-unaware code).
#pragma once

#include <cstdint>

enum class Status : int32_t {
  Ok = 0,
  InvalidArgument = -1,
  BufferTooSmall = -2,
  VersionResourceUnavailable = -3,  // exports.cpp only; src/core/ can never produce this
  // -4..-9 reserved, currently empty, for future module-local glue codes
  ParseError = -10,          // no Stage 8 caller yet -- forward reservation; likely Stage 9 (sync-operations) or Stage 12 (json-flatten)
  PathSyntaxError = -11,     // ParsePath: malformed path text, no document needed
  PathNotFound = -12,        // object key absent
  IndexOutOfRange = -13,     // array index >= size
  TypeMismatch = -14,        // segment/container kind mismatch (see json-path.h's rule), or ValueToText on non-leaf node
  NullValue = -15,           // JSON null encountered where a typed value was requested (kept distinct from TypeMismatch -- a null gets a default silently, a wrong-typed value is logged or treated as an error)
  NumericOverflow = -16,     // valid JSON number, out of int32_t range for ToLong
  NotIntegral = -17,         // valid JSON number, has a fractional part, requested as an integer type -- must NOT silently truncate
};
