// sync-text-api.h -- translates pointer+size CAPL text parameters into
// sync-operations calls. Grammar, status table and write-ordering rule:
// docs/capl-sync-surface.md.
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "core/status.h"
#include "http/http-client.h"

// Case-insensitive match against the six HttpMethod names. No match ->
// UnknownHttpMethod, out untouched.
Status ParseMethodText(std::string_view text, HttpMethod& out);

// block.empty() -> Ok, out left empty (no headers). Otherwise split on
// '\n' (trailing '\r' per line tolerated), each line "Name: Value" split
// on the first ':'; missing colon, empty name or empty value after
// trimming -> MalformedHeaderBlock. Duplicates preserved in order.
Status ParseHeaderBlock(std::string_view block, std::vector<HttpHeader>& out);

// The seven functions below share one contract: every (text, size) pair is
// resolved through BoundedText first, InvalidArgument/UnterminatedInputText
// propagate immediately with no call into HttpClient. httpStatusCode and
// responseBodyLength are written iff a response was actually received (see
// docs/capl-sync-surface.md); the body is then copied via CopyToBuffer.

Status ExecuteRequestSync(HttpClient& client, const char* methodText, std::uint32_t methodSize,
                           const char* urlText, std::uint32_t urlSize, const char* headersText,
                           std::uint32_t headersSize, const char* bodyText, std::uint32_t bodySize,
                           std::uint32_t connectTimeoutMs, std::uint32_t totalTimeoutMs,
                           std::uint32_t maxResponseBytes, char* responseBody,
                           std::uint32_t responseBodySize, std::int32_t& httpStatusCode,
                           std::uint32_t& responseBodyLength);

Status ExecuteGetSync(HttpClient& client, const char* urlText, std::uint32_t urlSize,
                       const char* headersText, std::uint32_t headersSize, char* responseBody,
                       std::uint32_t responseBodySize, std::int32_t& httpStatusCode,
                       std::uint32_t& responseBodyLength);

Status ExecuteDeleteSync(HttpClient& client, const char* urlText, std::uint32_t urlSize,
                          const char* headersText, std::uint32_t headersSize, char* responseBody,
                          std::uint32_t responseBodySize, std::int32_t& httpStatusCode,
                          std::uint32_t& responseBodyLength);

Status ExecutePostSync(HttpClient& client, const char* urlText, std::uint32_t urlSize,
                        const char* headersText, std::uint32_t headersSize, const char* bodyText,
                        std::uint32_t bodySize, char* responseBody, std::uint32_t responseBodySize,
                        std::int32_t& httpStatusCode, std::uint32_t& responseBodyLength);

Status ExecutePutSync(HttpClient& client, const char* urlText, std::uint32_t urlSize,
                       const char* headersText, std::uint32_t headersSize, const char* bodyText,
                       std::uint32_t bodySize, char* responseBody, std::uint32_t responseBodySize,
                       std::int32_t& httpStatusCode, std::uint32_t& responseBodyLength);

Status ExecutePatchSync(HttpClient& client, const char* urlText, std::uint32_t urlSize,
                         const char* headersText, std::uint32_t headersSize, const char* bodyText,
                         std::uint32_t bodySize, char* responseBody, std::uint32_t responseBodySize,
                         std::int32_t& httpStatusCode, std::uint32_t& responseBodyLength);
