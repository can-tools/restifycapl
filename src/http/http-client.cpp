#include "http/http-client.h"

#include <curl/curl.h>

#include <climits>
#include <mutex>
#include <string_view>

namespace {

constexpr std::uint32_t kDefaultConnectTimeoutMs = 5000;
constexpr std::uint32_t kDefaultTotalTimeoutMs = 30000;

static_assert(sizeof(long) == sizeof(std::int32_t),
              "CurlTransport::Perform assumes long is 32 bits");

std::once_flag g_curlInitFlag;
bool g_curlInitOk = false;

// Trap: never move this call into DllMain, and never add a matching
// curl_global_cleanup. Both run under the Windows loader lock if
// triggered from DLL_PROCESS_ATTACH/DETACH -- CANoe may load and unload
// this DLL across measurements while other threads are still in flight,
// and touching libcurl's global state under that lock is a documented way
// to deadlock the host process. The resulting leak from never calling
// curl_global_cleanup is deliberate, one-time and process-scoped.
void EnsureCurlInitialized() {
  std::call_once(g_curlInitFlag,
                  [] { g_curlInitOk = (curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK); });
}

// Trap: do not add a case for CURLE_WRITE_ERROR here. The write callback
// below returns a short byte count when the response cap is breached, and
// libcurl reports that as this same generic code -- indistinguishable
// from any other write failure. ResolveTransferResult (below) gives a cap
// breach precedence over whatever this mapping returns, so routing
// CURLE_WRITE_ERROR through the catch-all here would misreport "body too
// large" as a network failure the moment that precedence is bypassed.
Status MapCurlCode(CURLcode code) {
  switch (code) {
    case CURLE_OK:
      return Status::Ok;
    case CURLE_OPERATION_TIMEDOUT:
      return Status::Timeout;
    case CURLE_URL_MALFORMAT:
    case CURLE_UNSUPPORTED_PROTOCOL:
      return Status::InvalidUrl;
    case CURLE_SSL_CONNECT_ERROR:
    case CURLE_PEER_FAILED_VERIFICATION:
    case CURLE_SSL_ENGINE_NOTFOUND:
    case CURLE_SSL_ENGINE_SETFAILED:
    case CURLE_SSL_CERTPROBLEM:
    case CURLE_SSL_CIPHER:
    case CURLE_USE_SSL_FAILED:
    case CURLE_SSL_ENGINE_INITFAILED:
    case CURLE_SSL_CACERT_BADFILE:
    case CURLE_SSL_SHUTDOWN_FAILED:
    case CURLE_SSL_CRL_BADFILE:
    case CURLE_SSL_ISSUER_ERROR:
    case CURLE_SSL_PINNEDPUBKEYNOTMATCH:
    case CURLE_SSL_INVALIDCERTSTATUS:
    case CURLE_SSL_CLIENTCERT:
      return Status::TlsError;
    case CURLE_COULDNT_RESOLVE_HOST:
    case CURLE_COULDNT_RESOLVE_PROXY:
    case CURLE_COULDNT_CONNECT:
    case CURLE_SEND_ERROR:
    case CURLE_RECV_ERROR:
    case CURLE_GOT_NOTHING:
    case CURLE_PARTIAL_FILE:
    case CURLE_TOO_MANY_REDIRECTS:
      return Status::NetworkError;
    default:
      return Status::NetworkError;
  }
}

struct CurlHandleGuard {
  CURL* handle;
  explicit CurlHandleGuard(CURL* h) : handle(h) {}
  ~CurlHandleGuard() {
    if (handle != nullptr) {
      curl_easy_cleanup(handle);
    }
  }
  CurlHandleGuard(const CurlHandleGuard&) = delete;
  CurlHandleGuard& operator=(const CurlHandleGuard&) = delete;
};

struct SlistGuard {
  curl_slist* list;
  explicit SlistGuard(curl_slist* l) : list(l) {}
  ~SlistGuard() {
    if (list != nullptr) {
      curl_slist_free_all(list);
    }
  }
  SlistGuard(const SlistGuard&) = delete;
  SlistGuard& operator=(const SlistGuard&) = delete;
};

struct WriteContext {
  std::string* body;
  std::uint32_t capBytes;
  bool capExceeded = false;
};

std::size_t WriteCallback(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
  auto* ctx = static_cast<WriteContext*>(userdata);
  const std::size_t incoming = size * nmemb;

  if (WouldExceedResponseCap(ctx->body->size(), incoming, ctx->capBytes)) {
    ctx->capExceeded = true;
    return 0;
  }

  try {
    ctx->body->append(ptr, incoming);
  } catch (...) {
    return 0;
  }
  return incoming;
}

std::size_t HeaderCallback(char* buffer, std::size_t size, std::size_t nitems, void* userdata) {
  auto* headers = static_cast<std::vector<HttpHeader>*>(userdata);
  const std::size_t total = size * nitems;

  std::string_view line(buffer, total);
  while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
    line.remove_suffix(1);
  }

  const std::size_t colon = line.find(':');
  if (colon == std::string_view::npos) {
    return total;  // status line, or the trailing blank line -- not a header
  }

  std::string_view value = line.substr(colon + 1);
  while (!value.empty() && value.front() == ' ') {
    value.remove_prefix(1);
  }

  try {
    headers->push_back(HttpHeader{std::string(line.substr(0, colon)), std::string(value)});
  } catch (...) {
    // Dropping one header on allocation failure is preferable to failing
    // a transfer that could otherwise still complete -- the body cap
    // above is what protects the process, not this vector.
  }
  return total;
}

long ClampToLong(std::uint32_t milliseconds) {
  constexpr std::uint32_t kLongMax = static_cast<std::uint32_t>(LONG_MAX);
  return static_cast<long>(milliseconds > kLongMax ? kLongMax : milliseconds);
}

class CurlTransport : public HttpTransport {
 public:
  Status Perform(const HttpRequest& request, HttpResponse& response) override;
};

Status CurlTransport::Perform(const HttpRequest& request, HttpResponse& response) {
  EnsureCurlInitialized();
  if (!g_curlInitOk) {
    return Status::TransportInitFailed;
  }

  try {
    response.statusCode = 0;
    response.body.clear();
    response.headers.clear();

    CURL* const rawHandle = curl_easy_init();  // fresh handle per request -- required for thread-safe concurrent use
    if (rawHandle == nullptr) {
      return Status::TransportInitFailed;
    }
    const CurlHandleGuard handleGuard(rawHandle);
    CURL* const handle = rawHandle;

    curl_slist* rawHeaderList = nullptr;
    for (const HttpHeader& header : request.headers) {
      const std::string line = header.name + ": " + header.value;
      rawHeaderList = curl_slist_append(rawHeaderList, line.c_str());
    }
    const SlistGuard headerListGuard(rawHeaderList);

    curl_easy_setopt(handle, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, rawHeaderList);

    switch (request.method) {
      case HttpMethod::Get:
        curl_easy_setopt(handle, CURLOPT_HTTPGET, 1L);
        break;
      case HttpMethod::Head:
        curl_easy_setopt(handle, CURLOPT_NOBODY, 1L);
        break;
      case HttpMethod::Delete:
        curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "DELETE");
        break;
      case HttpMethod::Post:
        curl_easy_setopt(handle, CURLOPT_POST, 1L);
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, request.body.data());
        curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE_LARGE,
                          static_cast<curl_off_t>(request.body.size()));
        break;
      case HttpMethod::Put:
        curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, request.body.data());
        curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE_LARGE,
                          static_cast<curl_off_t>(request.body.size()));
        break;
      case HttpMethod::Patch:
        curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "PATCH");
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, request.body.data());
        curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE_LARGE,
                          static_cast<curl_off_t>(request.body.size()));
        break;
    }

    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);  // required for use from a non-main thread

    // Trap: set both VERIFYPEER and VERIFYHOST, or set neither to
    // non-default -- never one without the other. Each handle is created
    // fresh above, so leaving either at libcurl's implicit default is a
    // silent dependency on library behavior, and VERIFYPEER=1 with
    // VERIFYHOST left unset is the classic half-disabled-TLS bug: the
    // certificate gets validated but the hostname inside it never does.
    if (request.options.skipTlsVerification) {
      curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0L);
    } else {
      curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L);
      curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L);
    }

    const std::uint32_t connectMs = request.options.connectTimeoutMs == 0
                                         ? kDefaultConnectTimeoutMs
                                         : request.options.connectTimeoutMs;
    const std::uint32_t totalMs = request.options.totalTimeoutMs == 0
                                       ? kDefaultTotalTimeoutMs
                                       : request.options.totalTimeoutMs;
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, ClampToLong(connectMs));
    curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, ClampToLong(totalMs));

    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 5L);  // bounded on purpose
    curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "");  // the reason zs.lib (zlib) is linked at all
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "restifycapl");

    const std::uint32_t capBytes = request.options.maxResponseBytes == 0
                                        ? kDefaultMaxResponseBytes
                                        : request.options.maxResponseBytes;
    WriteContext writeCtx{&response.body, capBytes, false};
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, &WriteCallback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &writeCtx);
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, &HeaderCallback);
    curl_easy_setopt(handle, CURLOPT_HEADERDATA, &response.headers);

    const CURLcode result = curl_easy_perform(handle);
    const Status transferResult = ResolveTransferResult(writeCtx.capExceeded, MapCurlCode(result));
    if (transferResult != Status::Ok) {
      return transferResult;
    }

    long httpCode = 0;
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &httpCode);
    response.statusCode = static_cast<std::int32_t>(httpCode);

    return Status::Ok;
  } catch (...) {
    return Status::NetworkError;
  }
}

}  // namespace

bool WouldExceedResponseCap(std::size_t currentSize, std::size_t incoming,
                             std::uint32_t capBytes) {
  // Widen capBytes up to size_t rather than narrowing currentSize down to
  // it -- narrowing here would make the cap trip at a different input
  // size on x86 than on x64. incoming > cap is checked first so that
  // cap - incoming below can never wrap around zero.
  const std::size_t cap = static_cast<std::size_t>(capBytes);
  return incoming > cap || currentSize > cap - incoming;
}

Status ResolveTransferResult(bool capExceeded, Status mappedStatus) {
  return capExceeded ? Status::ResponseTooLarge : mappedStatus;
}

HttpTransport::~HttpTransport() = default;

HttpTransport& DefaultTransport() {
  static CurlTransport instance;
  return instance;
}

HttpClient::HttpClient(HttpTransport& transport) : transport_(transport) {}

Status HttpClient::Perform(const HttpRequest& request, HttpResponse& response) {
  if (request.url.empty()) {
    return Status::InvalidArgument;
  }
  return transport_.Perform(request, response);
}
