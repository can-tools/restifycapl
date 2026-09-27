// sync-text-api.h -- translates pointer+size CAPL text parameters into
// sync-operations calls. See docs/capl-sync-surface.md for the full contract.
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "core/status.h"
#include "http/http-client.h"

// No match against the six HttpMethod names -> UnknownHttpMethod.
Status ParseMethodText(std::string_view text, HttpMethod& out);

// Empty block -> Ok, no headers; malformed "Name: Value" line -> MalformedHeaderBlock.
Status ParseHeaderBlock(std::string_view block, std::vector<HttpHeader>& out);

// All seven below: truncated text -> UnterminatedInputText before any HttpClient call.
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
