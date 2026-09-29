#include "http/async-text-api.h"

#include <string>
#include <string_view>
#include <vector>

#include "core/input-text.h"

namespace {

bool ForbidsBody(HttpMethod method) {
  return method == HttpMethod::Get || method == HttpMethod::Head || method == HttpMethod::Delete;
}

RequestOptions MakeOptions(std::uint32_t connectTimeoutMs, std::uint32_t totalTimeoutMs,
                            std::uint32_t maxResponseBytes) {
  RequestOptions options;
  options.connectTimeoutMs = connectTimeoutMs;
  options.totalTimeoutMs = totalTimeoutMs;
  options.maxResponseBytes = maxResponseBytes;
  options.skipTlsVerification = false;
  return options;
}

Status DispatchNoBodyVerb(HttpClient& client, AsyncEngine& engine, HttpMethod method,
                           const char* urlText, std::uint32_t urlSize, const char* headersText,
                           std::uint32_t headersSize, std::uint32_t& requestId) {
  requestId = 0;

  std::string_view urlView;
  Status status = BoundedText(urlText, urlSize, urlView);
  if (status != Status::Ok) {
    return status;
  }
  std::string_view headersView;
  status = BoundedText(headersText, headersSize, headersView);
  if (status != Status::Ok) {
    return status;
  }

  std::vector<HttpHeader> headers;
  status = ParseHeaderBlock(headersView, headers);
  if (status != Status::Ok) {
    return status;
  }
  if (urlView.empty()) {
    return Status::InvalidArgument;
  }

  HttpRequest request;
  request.method = method;
  request.url = std::string(urlView);
  request.headers = headers;
  request.options = MakeOptions(0, 0, 0);

  return engine.Dispatch(client, request, requestId);
}

Status DispatchBodyVerb(HttpClient& client, AsyncEngine& engine, HttpMethod method,
                         const char* urlText, std::uint32_t urlSize, const char* headersText,
                         std::uint32_t headersSize, const char* bodyText, std::uint32_t bodySize,
                         std::uint32_t& requestId) {
  requestId = 0;

  std::string_view urlView;
  Status status = BoundedText(urlText, urlSize, urlView);
  if (status != Status::Ok) {
    return status;
  }
  std::string_view headersView;
  status = BoundedText(headersText, headersSize, headersView);
  if (status != Status::Ok) {
    return status;
  }
  std::string_view bodyView;
  status = BoundedText(bodyText, bodySize, bodyView);
  if (status != Status::Ok) {
    return status;
  }

  std::vector<HttpHeader> headers;
  status = ParseHeaderBlock(headersView, headers);
  if (status != Status::Ok) {
    return status;
  }
  if (urlView.empty()) {
    return Status::InvalidArgument;
  }

  HttpRequest request;
  request.method = method;
  request.url = std::string(urlView);
  request.headers = headers;
  request.body = std::string(bodyView);
  request.options = MakeOptions(0, 0, 0);

  return engine.Dispatch(client, request, requestId);
}

}  // namespace

Status DispatchGetAsync(HttpClient& client, AsyncEngine& engine, const char* urlText,
                         std::uint32_t urlSize, const char* headersText, std::uint32_t headersSize,
                         std::uint32_t& requestId) {
  return DispatchNoBodyVerb(client, engine, HttpMethod::Get, urlText, urlSize, headersText,
                             headersSize, requestId);
}

Status DispatchDeleteAsync(HttpClient& client, AsyncEngine& engine, const char* urlText,
                            std::uint32_t urlSize, const char* headersText,
                            std::uint32_t headersSize, std::uint32_t& requestId) {
  return DispatchNoBodyVerb(client, engine, HttpMethod::Delete, urlText, urlSize, headersText,
                             headersSize, requestId);
}

Status DispatchPostAsync(HttpClient& client, AsyncEngine& engine, const char* urlText,
                          std::uint32_t urlSize, const char* headersText, std::uint32_t headersSize,
                          const char* bodyText, std::uint32_t bodySize, std::uint32_t& requestId) {
  return DispatchBodyVerb(client, engine, HttpMethod::Post, urlText, urlSize, headersText,
                           headersSize, bodyText, bodySize, requestId);
}

Status DispatchPutAsync(HttpClient& client, AsyncEngine& engine, const char* urlText,
                         std::uint32_t urlSize, const char* headersText, std::uint32_t headersSize,
                         const char* bodyText, std::uint32_t bodySize, std::uint32_t& requestId) {
  return DispatchBodyVerb(client, engine, HttpMethod::Put, urlText, urlSize, headersText,
                           headersSize, bodyText, bodySize, requestId);
}

Status DispatchPatchAsync(HttpClient& client, AsyncEngine& engine, const char* urlText,
                           std::uint32_t urlSize, const char* headersText,
                           std::uint32_t headersSize, const char* bodyText, std::uint32_t bodySize,
                           std::uint32_t& requestId) {
  return DispatchBodyVerb(client, engine, HttpMethod::Patch, urlText, urlSize, headersText,
                           headersSize, bodyText, bodySize, requestId);
}

Status DispatchRequestAsync(HttpClient& client, AsyncEngine& engine, const char* methodText,
                             std::uint32_t methodSize, const char* urlText, std::uint32_t urlSize,
                             const char* headersText, std::uint32_t headersSize,
                             const char* bodyText, std::uint32_t bodySize,
                             std::uint32_t connectTimeoutMs, std::uint32_t totalTimeoutMs,
                             std::uint32_t maxResponseBytes, std::uint32_t& requestId) {
  requestId = 0;

  std::string_view methodView;
  Status status = BoundedText(methodText, methodSize, methodView);
  if (status != Status::Ok) {
    return status;
  }
  std::string_view urlView;
  status = BoundedText(urlText, urlSize, urlView);
  if (status != Status::Ok) {
    return status;
  }
  std::string_view headersView;
  status = BoundedText(headersText, headersSize, headersView);
  if (status != Status::Ok) {
    return status;
  }
  std::string_view bodyView;
  status = BoundedText(bodyText, bodySize, bodyView);
  if (status != Status::Ok) {
    return status;
  }

  HttpMethod method;
  status = ParseMethodText(methodView, method);
  if (status != Status::Ok) {
    return status;
  }

  std::vector<HttpHeader> headers;
  status = ParseHeaderBlock(headersView, headers);
  if (status != Status::Ok) {
    return status;
  }
  if (urlView.empty()) {
    return Status::InvalidArgument;
  }
  if (ForbidsBody(method) && !bodyView.empty()) {
    return Status::InvalidArgument;
  }

  HttpRequest request;
  request.method = method;
  request.url = std::string(urlView);
  request.headers = headers;
  request.body = std::string(bodyView);
  request.options = MakeOptions(connectTimeoutMs, totalTimeoutMs, maxResponseBytes);

  return engine.Dispatch(client, request, requestId);
}

Status PollAsyncResponse(AsyncEngine& engine, std::uint32_t requestId, std::int32_t& state) {
  return engine.Poll(requestId, state);
}

Status AwaitAsyncResponse(AsyncEngine& engine, std::uint32_t requestId,
                           std::uint32_t waitTimeoutMs) {
  return engine.Await(requestId, waitTimeoutMs);
}

Status ReadAsyncResponse(AsyncEngine& engine, std::uint32_t requestId, char* responseBody,
                          std::uint32_t responseBodySize, std::int32_t& requestStatus,
                          std::int32_t& httpStatusCode, std::uint32_t& responseBodyLength) {
  return engine.Read(requestId, responseBody, responseBodySize, requestStatus, httpStatusCode,
                      responseBodyLength);
}

Status DiscardAsyncResponse(AsyncEngine& engine, std::uint32_t requestId) {
  return engine.Discard(requestId);
}

Status DiscardAllAsyncResponses(AsyncEngine& engine, std::uint32_t& stillRunning) {
  return engine.DiscardAll(stillRunning);
}
