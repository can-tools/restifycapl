// sync-operations.h -- synchronous REST verb helpers built on HttpClient
// (src/http/, level 3).
//
// These calls block and allocate, so per Vector's own CAPL DLL
// documentation they are safe only from Measurement Setup / test nodes,
// never from the Simulation Setup realtime branch. See
// docs/http-layer.md for the full citation.
#pragma once

#include <string>
#include <vector>

#include "core/status.h"
#include "http/http-client.h"

// Executes request as-is through client. A non-empty body on Get, Head or
// Delete is rejected as Status::InvalidArgument before client.Perform is
// called; every other field of request, including all of
// request.options, reaches client.Perform unmodified.
Status Request(HttpClient& client, const HttpRequest& request, HttpResponse& response);

Status Get(HttpClient& client, const std::string& url, const std::vector<HttpHeader>& headers,
           const RequestOptions& options, HttpResponse& response);

Status Post(HttpClient& client, const std::string& url, const std::vector<HttpHeader>& headers,
            const std::string& body, const RequestOptions& options, HttpResponse& response);

Status Put(HttpClient& client, const std::string& url, const std::vector<HttpHeader>& headers,
           const std::string& body, const RequestOptions& options, HttpResponse& response);

Status Patch(HttpClient& client, const std::string& url, const std::vector<HttpHeader>& headers,
             const std::string& body, const RequestOptions& options, HttpResponse& response);

Status Delete(HttpClient& client, const std::string& url, const std::vector<HttpHeader>& headers,
              const RequestOptions& options, HttpResponse& response);
