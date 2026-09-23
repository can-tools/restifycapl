// http-client.h -- HTTP transport seam and value types (src/http/, level
// 1). HttpTransport is the only dependency callers in this layer take;
// CurlTransport (the production implementation) is file-local to
// http-client.cpp, so curl/curl.h never appears here. See
// docs/http-layer.md for the full option policy, CURLcode mapping and
// handle-lifecycle rules this header only summarizes in short form.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/status.h"

enum class HttpMethod { Get, Post, Put, Patch, Delete, Head };

struct HttpHeader {
  std::string name;
  std::string value;
};

// 8 MiB, identical on both architectures -- see docs/http-layer.md for why
// this exists and why the value must never differ by build target.
constexpr std::uint32_t kDefaultMaxResponseBytes = 8u * 1024u * 1024u;

struct RequestOptions {
  // 0 = use the 5000 ms / 30000 ms defaults below. There is intentionally
  // no way to request "no timeout": an unbounded blocking call, on a
  // high-priority thread in CANoe's realtime branch, can stall a
  // measurement with no way for the CAPL script to recover. Do not add
  // one -- see docs/http-layer.md.
  std::uint32_t connectTimeoutMs = 0;
  std::uint32_t totalTimeoutMs = 0;
  std::uint32_t maxResponseBytes = 0;  // 0 = kDefaultMaxResponseBytes

  // Never default this to true and never derive it from anything else --
  // a skip-by-default would make every request silently unauthenticated
  // against its peer, with no visible symptom. sync-operations must
  // forward this field unmodified; see docs/http-layer.md for why the
  // capability exists at all and what bounds it.
  bool skipTlsVerification = false;
};

struct HttpRequest {
  HttpMethod method = HttpMethod::Get;
  std::string url;
  std::vector<HttpHeader> headers;
  std::string body;
  RequestOptions options;
};

struct HttpResponse {
  std::int32_t statusCode = 0;  // 0 if no response was received at all
  std::string body;
  std::vector<HttpHeader> headers;
};

// Pulled out of the curl-callback-only code paths in http-client.cpp so
// they are directly callable from tests with no libcurl type involved. See
// docs/http-layer.md's "What cannot be verified without a live server"
// section for the boundary this does and does not close.
bool WouldExceedResponseCap(std::size_t currentSize, std::size_t incoming,
                             std::uint32_t capBytes);
Status ResolveTransferResult(bool capExceeded, Status mappedStatus);

// Pure-virtual seam: production traffic goes through CurlTransport
// (http-client.cpp); tests inject a fake instead. See docs/http-layer.md
// for the alternatives this shape was chosen over.
class HttpTransport {
 public:
  virtual ~HttpTransport();
  virtual Status Perform(const HttpRequest& request, HttpResponse& response) = 0;
};

// The process-wide CurlTransport instance HttpClient defaults to.
// Definition lives in http-client.cpp, where curl.h is available.
HttpTransport& DefaultTransport();

class HttpClient {
 public:
  explicit HttpClient(HttpTransport& transport = DefaultTransport());

  // request.url.empty() -> InvalidArgument, no transport call made.
  // Otherwise delegates to the injected transport as-is.
  Status Perform(const HttpRequest& request, HttpResponse& response);

 private:
  HttpTransport& transport_;
};
