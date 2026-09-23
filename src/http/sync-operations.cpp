#include "http/sync-operations.h"

namespace {

bool ForbidsBody(HttpMethod method) {
  return method == HttpMethod::Get || method == HttpMethod::Head || method == HttpMethod::Delete;
}

Status BuildAndSend(HttpClient& client, HttpMethod method, const std::string& url,
                     const std::vector<HttpHeader>& headers, const std::string& body,
                     const RequestOptions& options, HttpResponse& response) {
  try {
    HttpRequest request;
    request.method = method;
    request.url = url;
    request.headers = headers;
    request.body = body;
    request.options = options;
    return Request(client, request, response);
  } catch (...) {
    return Status::NetworkError;
  }
}

}  // namespace

Status Request(HttpClient& client, const HttpRequest& request, HttpResponse& response) {
  if (ForbidsBody(request.method) && !request.body.empty()) {
    return Status::InvalidArgument;
  }
  return client.Perform(request, response);
}

Status Get(HttpClient& client, const std::string& url, const std::vector<HttpHeader>& headers,
           const RequestOptions& options, HttpResponse& response) {
  return BuildAndSend(client, HttpMethod::Get, url, headers, std::string(), options, response);
}

Status Post(HttpClient& client, const std::string& url, const std::vector<HttpHeader>& headers,
            const std::string& body, const RequestOptions& options, HttpResponse& response) {
  return BuildAndSend(client, HttpMethod::Post, url, headers, body, options, response);
}

Status Put(HttpClient& client, const std::string& url, const std::vector<HttpHeader>& headers,
           const std::string& body, const RequestOptions& options, HttpResponse& response) {
  return BuildAndSend(client, HttpMethod::Put, url, headers, body, options, response);
}

Status Patch(HttpClient& client, const std::string& url, const std::vector<HttpHeader>& headers,
             const std::string& body, const RequestOptions& options, HttpResponse& response) {
  return BuildAndSend(client, HttpMethod::Patch, url, headers, body, options, response);
}

Status Delete(HttpClient& client, const std::string& url, const std::vector<HttpHeader>& headers,
              const RequestOptions& options, HttpResponse& response) {
  return BuildAndSend(client, HttpMethod::Delete, url, headers, std::string(), options, response);
}
