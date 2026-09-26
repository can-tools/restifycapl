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
  ParseError = -10,          // reserved for JSON parsing (json-flatten); sync-operations does not claim this code
  PathSyntaxError = -11,     // ParsePath: malformed path text, no document needed
  PathNotFound = -12,        // object key absent
  IndexOutOfRange = -13,     // array index >= size
  TypeMismatch = -14,        // segment/container kind mismatch (see json-path.h's rule), or ValueToText on non-leaf node
  NullValue = -15,           // JSON null encountered where a typed value was requested (kept distinct from TypeMismatch -- a null gets a default silently, a wrong-typed value is logged or treated as an error)
  NumericOverflow = -16,     // valid JSON number, out of int32_t range for ToLong
  NotIntegral = -17,         // valid JSON number, has a fractional part, requested as an integer type -- must NOT silently truncate
  // -18..-24 reserved for the HTTP/transport layer (src/http/).
  NetworkError = -18,        // transfer failed (DNS, connect, send/recv, redirect limit); also the catch-all for any unmapped CURLcode
  Timeout = -19,             // connect or total timeout expired -- libcurl cannot distinguish the two, so one code covers both
  TlsError = -20,            // TLS/Schannel handshake or certificate verification failure, kept distinct from NetworkError
  TransportInitFailed = -21, // curl_global_init or curl_easy_init failed -- process-level, not request-level
  InvalidUrl = -22,          // malformed or unsupported-scheme URL, kept distinct from InvalidArgument (-1)
  ResponseTooLarge = -23,    // response body exceeded the configured cap
  // -24 reserved, currently empty, for the HTTP layer.
  // -25..-29 reserved for the async layer; do not mint from this range here.
};
