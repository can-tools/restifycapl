#include "core/json-path.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <system_error>
#include <utility>

namespace {

bool DecodeToken(std::string_view raw, std::string& out) {
  out.clear();
  out.reserve(raw.size());
  for (std::size_t i = 0; i < raw.size(); ++i) {
    const char c = raw[i];
    if (c != '~') {
      out.push_back(c);
      continue;
    }
    if (i + 1 >= raw.size()) {
      return false;
    }
    // Each escape is decoded once, left to right. Replacing "~0" before "~1"
    // would turn "~01" into "/" instead of the correct "~1".
    const char next = raw[++i];
    if (next == '1') {
      out.push_back('/');
    } else if (next == '0') {
      out.push_back('~');
    } else {
      return false;
    }
  }
  return true;
}

bool IsDigit(char c) { return c >= '0' && c <= '9'; }

Status ParseIndex(const std::string& token, std::uint32_t& index) {
  if (token == "-") {
    return Status::IndexOutOfRange;
  }
  if (token.empty() || (token.size() > 1 && token.front() == '0')) {
    return Status::TypeMismatch;
  }
  for (const char c : token) {
    if (!IsDigit(c)) {
      return Status::TypeMismatch;
    }
  }

  const char* begin = token.data();
  const char* end = token.data() + token.size();
  std::uint32_t value = 0;
  const auto result = std::from_chars(begin, end, value);
  if (result.ec == std::errc::result_out_of_range) {
    return Status::IndexOutOfRange;
  }
  if (result.ec != std::errc{} || result.ptr != end) {
    return Status::TypeMismatch;
  }
  index = value;
  return Status::Ok;
}

}  // namespace

Status ParsePath(std::string_view path, std::vector<std::string>& out) {
  if (path.data() == nullptr) {
    return Status::InvalidArgument;
  }
  if (path.empty()) {
    out.clear();
    return Status::Ok;
  }
  if (path.front() != '/') {
    return Status::PathSyntaxError;
  }

  std::vector<std::string> tokens;
  std::size_t start = 1;
  for (;;) {
    const std::size_t end = path.find('/', start);
    const std::size_t length =
        (end == std::string_view::npos) ? std::string_view::npos : end - start;
    std::string token;
    if (!DecodeToken(path.substr(start, length), token)) {
      return Status::PathSyntaxError;
    }
    tokens.push_back(std::move(token));
    if (end == std::string_view::npos) {
      break;
    }
    start = end + 1;
  }

  out = std::move(tokens);
  return Status::Ok;
}

Status ResolvePath(const JsonValue& document, std::string_view path,
                   const JsonValue*& out) {
  std::vector<std::string> tokens;
  const Status parseStatus = ParsePath(path, tokens);
  if (parseStatus != Status::Ok) {
    return parseStatus;
  }

  const JsonValue* current = &document;
  for (const std::string& token : tokens) {
    if (current->is_object()) {
      const auto it = current->find(token);
      if (it == current->end()) {
        return Status::PathNotFound;
      }
      current = &(*it);
    } else if (current->is_array()) {
      std::uint32_t index = 0;
      const Status indexStatus = ParseIndex(token, index);
      if (indexStatus != Status::Ok) {
        return indexStatus;
      }
      if (static_cast<std::size_t>(index) >= current->size()) {
        return Status::IndexOutOfRange;
      }
      current = &(*current)[static_cast<std::size_t>(index)];
    } else {
      return Status::TypeMismatch;
    }
  }

  out = current;
  return Status::Ok;
}
