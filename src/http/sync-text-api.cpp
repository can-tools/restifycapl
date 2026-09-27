#include "http/sync-text-api.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

#include "core/buffer-copy.h"
#include "core/input-text.h"
#include "http/sync-operations.h"

namespace {

bool EqualsIgnoreCase(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) {
    return false;
  }
  return std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
    return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
  });
}

std::string_view Trim(std::string_view text) {
  const auto isSpace = [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; };
  while (!text.empty() && isSpace(text.front())) {
    text.remove_prefix(1);
  }
  while (!text.empty() && isSpace(text.back())) {
    text.remove_suffix(1);
  }
  return text;
}

std::string_view StripTrailingCr(std::string_view line) {
  if (!line.empty() && line.back() == '\r') {
    line.remove_suffix(1);
  }
  return line;
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

// D5's ordering: httpStatusCode/responseBodyLength are set before the copy
// outcome is known, so a BufferTooSmall copy never erases them.
Status FinishResponse(Status result, const HttpResponse& response, char* responseBody,
                       std::uint32_t responseBodySize, std::int32_t& httpStatusCode,
                       std::uint32_t& responseBodyLength) {
  if (result == Status::InvalidArgument) {
    return result;
  }

  httpStatusCode = response.statusCode;
  responseBodyLength = static_cast<std::uint32_t>(response.body.size());
  Status copyStatus = CopyToBuffer(response.body, responseBody, responseBodySize);
  return result == Status::Ok ? copyStatus : result;
}

Status ExecuteNoBodyVerb(HttpClient& client, HttpMethod method, const char* urlText,
                          std::uint32_t urlSize, const char* headersText, std::uint32_t headersSize,
                          char* responseBody, std::uint32_t responseBodySize,
                          std::int32_t& httpStatusCode, std::uint32_t& responseBodyLength) {
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

  RequestOptions options = MakeOptions(0, 0, 0);
  HttpResponse response;
  Status result = method == HttpMethod::Get
                       ? Get(client, std::string(urlView), headers, options, response)
                       : Delete(client, std::string(urlView), headers, options, response);
  return FinishResponse(result, response, responseBody, responseBodySize, httpStatusCode,
                         responseBodyLength);
}

Status ExecuteBodyVerb(HttpClient& client, HttpMethod method, const char* urlText,
                        std::uint32_t urlSize, const char* headersText, std::uint32_t headersSize,
                        const char* bodyText, std::uint32_t bodySize, char* responseBody,
                        std::uint32_t responseBodySize, std::int32_t& httpStatusCode,
                        std::uint32_t& responseBodyLength) {
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

  RequestOptions options = MakeOptions(0, 0, 0);
  HttpResponse response;
  std::string url(urlView);
  std::string body(bodyView);
  Status result;
  switch (method) {
    case HttpMethod::Post:
      result = Post(client, url, headers, body, options, response);
      break;
    case HttpMethod::Put:
      result = Put(client, url, headers, body, options, response);
      break;
    default:
      result = Patch(client, url, headers, body, options, response);
      break;
  }
  return FinishResponse(result, response, responseBody, responseBodySize, httpStatusCode,
                         responseBodyLength);
}

}  // namespace

Status ParseMethodText(std::string_view text, HttpMethod& out) {
  static constexpr struct {
    std::string_view name;
    HttpMethod method;
  } kMethods[] = {
      {"GET", HttpMethod::Get},     {"POST", HttpMethod::Post}, {"PUT", HttpMethod::Put},
      {"PATCH", HttpMethod::Patch}, {"DELETE", HttpMethod::Delete}, {"HEAD", HttpMethod::Head},
  };

  for (const auto& entry : kMethods) {
    if (EqualsIgnoreCase(text, entry.name)) {
      out = entry.method;
      return Status::Ok;
    }
  }
  return Status::UnknownHttpMethod;
}

Status ParseHeaderBlock(std::string_view block, std::vector<HttpHeader>& out) {
  out.clear();
  if (block.empty()) {
    return Status::Ok;
  }

  std::size_t start = 0;
  while (true) {
    std::size_t newlinePos = block.find('\n', start);
    std::string_view line = newlinePos == std::string_view::npos ? block.substr(start)
                                                                   : block.substr(start, newlinePos - start);
    line = StripTrailingCr(line);

    std::size_t colonPos = line.find(':');
    if (colonPos == std::string_view::npos) {
      return Status::MalformedHeaderBlock;
    }
    std::string_view name = Trim(line.substr(0, colonPos));
    std::string_view value = Trim(line.substr(colonPos + 1));
    if (name.empty() || value.empty()) {
      return Status::MalformedHeaderBlock;
    }
    out.push_back(HttpHeader{std::string(name), std::string(value)});

    if (newlinePos == std::string_view::npos) {
      break;
    }
    start = newlinePos + 1;
  }
  return Status::Ok;
}

Status ExecuteRequestSync(HttpClient& client, const char* methodText, std::uint32_t methodSize,
                           const char* urlText, std::uint32_t urlSize, const char* headersText,
                           std::uint32_t headersSize, const char* bodyText, std::uint32_t bodySize,
                           std::uint32_t connectTimeoutMs, std::uint32_t totalTimeoutMs,
                           std::uint32_t maxResponseBytes, char* responseBody,
                           std::uint32_t responseBodySize, std::int32_t& httpStatusCode,
                           std::uint32_t& responseBodyLength) {
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

  HttpRequest request;
  request.method = method;
  request.url = std::string(urlView);
  request.headers = headers;
  request.body = std::string(bodyView);
  request.options = MakeOptions(connectTimeoutMs, totalTimeoutMs, maxResponseBytes);

  HttpResponse response;
  Status result = Request(client, request, response);
  return FinishResponse(result, response, responseBody, responseBodySize, httpStatusCode,
                         responseBodyLength);
}

Status ExecuteGetSync(HttpClient& client, const char* urlText, std::uint32_t urlSize,
                       const char* headersText, std::uint32_t headersSize, char* responseBody,
                       std::uint32_t responseBodySize, std::int32_t& httpStatusCode,
                       std::uint32_t& responseBodyLength) {
  return ExecuteNoBodyVerb(client, HttpMethod::Get, urlText, urlSize, headersText, headersSize,
                            responseBody, responseBodySize, httpStatusCode, responseBodyLength);
}

Status ExecuteDeleteSync(HttpClient& client, const char* urlText, std::uint32_t urlSize,
                          const char* headersText, std::uint32_t headersSize, char* responseBody,
                          std::uint32_t responseBodySize, std::int32_t& httpStatusCode,
                          std::uint32_t& responseBodyLength) {
  return ExecuteNoBodyVerb(client, HttpMethod::Delete, urlText, urlSize, headersText, headersSize,
                            responseBody, responseBodySize, httpStatusCode, responseBodyLength);
}

Status ExecutePostSync(HttpClient& client, const char* urlText, std::uint32_t urlSize,
                        const char* headersText, std::uint32_t headersSize, const char* bodyText,
                        std::uint32_t bodySize, char* responseBody, std::uint32_t responseBodySize,
                        std::int32_t& httpStatusCode, std::uint32_t& responseBodyLength) {
  return ExecuteBodyVerb(client, HttpMethod::Post, urlText, urlSize, headersText, headersSize,
                          bodyText, bodySize, responseBody, responseBodySize, httpStatusCode,
                          responseBodyLength);
}

Status ExecutePutSync(HttpClient& client, const char* urlText, std::uint32_t urlSize,
                       const char* headersText, std::uint32_t headersSize, const char* bodyText,
                       std::uint32_t bodySize, char* responseBody, std::uint32_t responseBodySize,
                       std::int32_t& httpStatusCode, std::uint32_t& responseBodyLength) {
  return ExecuteBodyVerb(client, HttpMethod::Put, urlText, urlSize, headersText, headersSize,
                          bodyText, bodySize, responseBody, responseBodySize, httpStatusCode,
                          responseBodyLength);
}

Status ExecutePatchSync(HttpClient& client, const char* urlText, std::uint32_t urlSize,
                         const char* headersText, std::uint32_t headersSize, const char* bodyText,
                         std::uint32_t bodySize, char* responseBody, std::uint32_t responseBodySize,
                         std::int32_t& httpStatusCode, std::uint32_t& responseBodyLength) {
  return ExecuteBodyVerb(client, HttpMethod::Patch, urlText, urlSize, headersText, headersSize,
                          bodyText, bodySize, responseBody, responseBodySize, httpStatusCode,
                          responseBodyLength);
}
