// Coverage double for src/http/http-client.h's HttpTransport seam. Never
// opens a socket, never includes curl/curl.h, never linked into the
// product DLL -- test-only.
#pragma once

#include <mutex>
#include <utility>

#include "http/http-client.h"

class FakeTransport : public HttpTransport {
 public:
  // Every subsequent Perform call returns status and a copy of response,
  // until this is called again.
  void SetResult(Status status, HttpResponse response) {
    std::lock_guard<std::mutex> lock(mutex_);
    scriptedStatus_ = status;
    scriptedResponse_ = std::move(response);
  }

  Status Perform(const HttpRequest& request, HttpResponse& response) override {
    std::lock_guard<std::mutex> lock(mutex_);
    ++callCount_;
    lastRequest_ = request;
    hasLastRequest_ = true;
    response = scriptedResponse_;
    return scriptedStatus_;
  }

  // Full request last seen by Perform, including the resolved
  // RequestOptions -- lets a test assert what sync-operations actually
  // built, not merely what came back.
  HttpRequest LastRequest() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastRequest_;
  }

  bool HasLastRequest() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return hasLastRequest_;
  }

  int CallCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return callCount_;
  }

 private:
  mutable std::mutex mutex_;
  Status scriptedStatus_ = Status::Ok;
  HttpResponse scriptedResponse_;
  HttpRequest lastRequest_;
  bool hasLastRequest_ = false;
  int callCount_ = 0;
};
