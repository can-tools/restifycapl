// async-text-api.h -- translates pointer+size CAPL parameters into
// AsyncEngine calls. See docs/capl-async-surface.md for the full contract.
#pragma once

#include <cstdint>

#include "http/async-operations.h"
#include "http/http-client.h"
#include "http/sync-text-api.h"

Status DispatchGetAsync(HttpClient& client, AsyncEngine& engine, const char* urlText,
                         std::uint32_t urlSize, const char* headersText, std::uint32_t headersSize,
                         std::uint32_t& requestId);

Status DispatchDeleteAsync(HttpClient& client, AsyncEngine& engine, const char* urlText,
                            std::uint32_t urlSize, const char* headersText,
                            std::uint32_t headersSize, std::uint32_t& requestId);

Status DispatchPostAsync(HttpClient& client, AsyncEngine& engine, const char* urlText,
                          std::uint32_t urlSize, const char* headersText, std::uint32_t headersSize,
                          const char* bodyText, std::uint32_t bodySize, std::uint32_t& requestId);

Status DispatchPutAsync(HttpClient& client, AsyncEngine& engine, const char* urlText,
                         std::uint32_t urlSize, const char* headersText, std::uint32_t headersSize,
                         const char* bodyText, std::uint32_t bodySize, std::uint32_t& requestId);

Status DispatchPatchAsync(HttpClient& client, AsyncEngine& engine, const char* urlText,
                           std::uint32_t urlSize, const char* headersText,
                           std::uint32_t headersSize, const char* bodyText, std::uint32_t bodySize,
                           std::uint32_t& requestId);

Status DispatchRequestAsync(HttpClient& client, AsyncEngine& engine, const char* methodText,
                             std::uint32_t methodSize, const char* urlText, std::uint32_t urlSize,
                             const char* headersText, std::uint32_t headersSize,
                             const char* bodyText, std::uint32_t bodySize,
                             std::uint32_t connectTimeoutMs, std::uint32_t totalTimeoutMs,
                             std::uint32_t maxResponseBytes, std::uint32_t& requestId);

Status PollAsyncResponse(AsyncEngine& engine, std::uint32_t requestId, std::int32_t& state);

Status AwaitAsyncResponse(AsyncEngine& engine, std::uint32_t requestId,
                           std::uint32_t waitTimeoutMs);

Status ReadAsyncResponse(AsyncEngine& engine, std::uint32_t requestId, char* responseBody,
                          std::uint32_t responseBodySize, std::int32_t& requestStatus,
                          std::int32_t& httpStatusCode, std::uint32_t& responseBodyLength);

Status DiscardAsyncResponse(AsyncEngine& engine, std::uint32_t requestId);

Status DiscardAllAsyncResponses(AsyncEngine& engine, std::uint32_t& stillRunning);
