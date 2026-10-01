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
  MalformedHeaderBlock = -4,  // header block text violates the Name: Value grammar
  UnknownHttpMethod = -5,     // method text did not match any known HTTP verb
  UnterminatedInputText = -6, // no NUL terminator found within the caller-stated size bound
  // -7..-9 reserved, currently empty, for future module-local glue codes
  ParseError = -10,          // returned by json-flatten for invalid JSON, empty text and invalid UTF-8
  PathSyntaxError = -11,     // ParsePath: malformed path text, no document needed
  PathNotFound = -12,        // object key absent
  IndexOutOfRange = -13,     // array index >= size
  TypeMismatch = -14,        // segment/container kind mismatch (see docs/json-path.md), or ValueToText on non-leaf node
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
  RequestCancelled = -24,     // CURLE_ABORTED_BY_CALLBACK; internal only, CAPL never sees this
  NoFreeRequestSlot = -25,    // dispatch: all 8 slots occupied
  RequestNotComplete = -26,   // read on a live id still Pending/Running
  UnknownRequestId = -27,     // id is 0, never issued, Consumed, Abandoned, or Free
  WaitTimeout = -28,          // await's deadline reached
  AsyncStartFailed = -29,     // worker thread creation failed
  // -30..-39 reserved for the mapping layer (src/mapping/); -35..-39 are free.
  NoFreeDocumentSlot = -30,   // parse: all document slots occupied
  UnknownDocumentId = -31,    // document id unknown or already discarded
  DocumentTooLarge = -32,     // JSON input larger than 1 MiB
  NestingTooDeep = -33,       // JSON nesting deeper than 64
  TooManyEntries = -34,       // more than 10,000 flattened entries
};
