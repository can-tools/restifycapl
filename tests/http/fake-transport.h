// Coverage double for src/http/http-client.h's HttpTransport seam. Never
// opens a socket, never includes curl/curl.h, never linked into the
// product DLL -- test-only.
#pragma once

#include <chrono>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "http/http-client.h"

// Every wait below is bounded: a gate that is never released fails the
// calling Perform() loudly after kGateWaitBound instead of hanging the
// worker thread that reached it, and the same bound applies to a test
// waiting for a gate to be entered.
class FakeTransport : public HttpTransport {
 public:
  // Every subsequent ungated Perform call returns status and a copy of
  // response, until this is called again. Unaffected by gates below.
  void SetResult(Status status, HttpResponse response) {
    std::lock_guard<std::mutex> lock(mutex_);
    scriptedStatus_ = status;
    scriptedResponse_ = std::move(response);
  }

  // Registers a gate keyed by request.url: the next Perform() call whose
  // request carries this url blocks until Release() is called for the same
  // url (or the bounded wait below expires). Requests with no matching gate
  // fall through to the scripted default immediately, unaffected.
  void ExpectGate(const std::string& url) {
    std::lock_guard<std::mutex> lock(mutex_);
    gates_[url] = std::make_shared<Gate>();
  }

  // Releases a gate previously registered with ExpectGate, handing the
  // blocked (or not-yet-arrived) Perform() call the given outcome.
  void Release(const std::string& url, Status status, HttpResponse response) {
    std::shared_ptr<Gate> gate = FindGate(url);
    if (!gate) {
      return;
    }
    std::lock_guard<std::mutex> gateLock(gate->mutex);
    gate->status = status;
    gate->response = std::move(response);
    gate->released = true;
    gate->cv.notify_all();
  }

  // Blocks the calling (test) thread until the worker servicing url has
  // entered Perform() and is waiting on its gate, or the bound expires.
  // Lets a test confirm "N requests in flight" with no sleeps.
  bool WaitForGateEntered(const std::string& url,
                          std::chrono::milliseconds bound = kGateWaitBound) {
    std::shared_ptr<Gate> gate = FindGate(url);
    if (!gate) {
      return false;
    }
    std::unique_lock<std::mutex> gateLock(gate->mutex);
    return gate->cv.wait_for(gateLock, bound, [&] { return gate->entered; });
  }

  Status Perform(const HttpRequest& request, HttpResponse& response) override {
    std::shared_ptr<Gate> gate;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      ++callCount_;
      lastRequest_ = request;
      hasLastRequest_ = true;
      auto it = gates_.find(request.url);
      if (it != gates_.end()) {
        gate = it->second;
      }
    }

    if (!gate) {
      std::lock_guard<std::mutex> lock(mutex_);
      response = scriptedResponse_;
      return scriptedStatus_;
    }

    return WaitOnGate(*gate, request, response);
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
  struct Gate {
    std::mutex mutex;
    std::condition_variable cv;
    bool entered = false;
    bool released = false;
    Status status = Status::Ok;
    HttpResponse response;
  };

  // Generous but finite: long enough that no real test iteration should
  // ever hit it, short enough that a gate left unreleased by a test bug
  // fails that test instead of hanging the whole binary.
  static constexpr std::chrono::milliseconds kGateWaitBound{5000};
  static constexpr std::chrono::milliseconds kGatePollInterval{20};

  std::shared_ptr<Gate> FindGate(const std::string& url) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = gates_.find(url);
    return it != gates_.end() ? it->second : nullptr;
  }

  static Status WaitOnGate(Gate& gate, const HttpRequest& request, HttpResponse& response) {
    std::unique_lock<std::mutex> gateLock(gate.mutex);
    gate.entered = true;
    gate.cv.notify_all();

    const auto deadline = std::chrono::steady_clock::now() + kGateWaitBound;
    while (!gate.released) {
      if (request.cancelFlag != nullptr && request.cancelFlag->load()) {
        return Status::RequestCancelled;
      }
      if (std::chrono::steady_clock::now() >= deadline) {
        ADD_FAILURE() << "FakeTransport gate for '" << request.url
                       << "' was never released within the bounded wait -- "
                          "a test or fixture forgot to release or cancel it";
        return Status::NetworkError;
      }
      gate.cv.wait_for(gateLock, kGatePollInterval);
    }
    response = gate.response;
    return gate.status;
  }

  mutable std::mutex mutex_;
  Status scriptedStatus_ = Status::Ok;
  HttpResponse scriptedResponse_;
  HttpRequest lastRequest_;
  bool hasLastRequest_ = false;
  int callCount_ = 0;
  std::map<std::string, std::shared_ptr<Gate>> gates_;
};
