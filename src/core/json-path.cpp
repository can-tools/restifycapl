#include "core/json-path.h"

#include <charconv>

namespace {

// A key segment runs until the next '.' or '[', or end of text -- these are
// the only delimiters this interim syntax recognizes (see json-path.h's
// note on keys containing '.' or '[').
std::string_view ScanKey(std::string_view path, std::size_t pos) {
  std::size_t end = pos;
  while (end < path.size() && path[end] != '.' && path[end] != '[') {
    ++end;
  }
  return path.substr(pos, end - pos);
}

// `pos` points at the opening '[' on entry; advanced past the closing ']'
// on success. std::from_chars does the range check directly into uint32_t,
// so no size_t-width narrowing cast is ever needed here.
Status ScanIndex(std::string_view path, std::size_t& pos, std::uint32_t& index) {
  const std::size_t open = pos;
  const std::size_t close = path.find(']', open);
  if (close == std::string_view::npos) {
    return Status::PathSyntaxError;  // unclosed bracket, e.g. "items[0"
  }

  const std::string_view digits = path.substr(open + 1, close - open - 1);
  if (digits.empty()) {
    return Status::PathSyntaxError;  // "[]"
  }

  const char* begin = digits.data();
  const char* end = digits.data() + digits.size();
  const auto result = std::from_chars(begin, end, index);
  if (result.ec == std::errc::result_out_of_range) {
    return Status::PathSyntaxError;  // valid digits, too large for uint32_t
  }
  if (result.ec != std::errc{} || result.ptr != end) {
    // from_chars into an unsigned type rejects a leading '-' outright, so
    // this single check also covers "[-1]" as well as "[x]".
    return Status::PathSyntaxError;
  }

  pos = close + 1;
  return Status::Ok;
}

}  // namespace

Status ParsePath(std::string_view path, std::vector<PathSegment>& out) {
  if (path.data() == nullptr) {
    return Status::InvalidArgument;
  }
  if (path.empty() || path.front() == '.') {
    return Status::PathSyntaxError;
  }

  std::vector<PathSegment> segments;
  std::size_t pos = 0;

  while (pos < path.size()) {
    if (path[pos] == '[') {
      std::uint32_t index = 0;
      const Status status = ScanIndex(path, pos, index);
      if (status != Status::Ok) {
        return status;
      }
      segments.push_back(PathSegment{PathSegment::Kind::Index, std::string{}, index});
      continue;
    }

    if (path[pos] == '.') {
      ++pos;
      if (pos >= path.size() || path[pos] == '.' || path[pos] == '[') {
        return Status::PathSyntaxError;  // trailing '.', "..", or ".["
      }
      const std::string_view key = ScanKey(path, pos);
      segments.push_back(PathSegment{PathSegment::Kind::Key, std::string{key}, 0});
      pos += key.size();
      continue;
    }

    if (pos != 0) {
      // A bare key char is only valid at the very start of the path or
      // right after a '.'; reaching here means a key glued directly onto a
      // prior segment with no separator, e.g. "items[0]name".
      return Status::PathSyntaxError;
    }
    const std::string_view key = ScanKey(path, pos);
    segments.push_back(PathSegment{PathSegment::Kind::Key, std::string{key}, 0});
    pos += key.size();
  }

  out = std::move(segments);
  return Status::Ok;
}

Status ResolvePath(const nlohmann::json& document, std::string_view path,
                    const nlohmann::json*& out) {
  std::vector<PathSegment> segments;
  const Status parseStatus = ParsePath(path, segments);
  if (parseStatus != Status::Ok) {
    return parseStatus;
  }

  const nlohmann::json* current = &document;
  for (const auto& segment : segments) {
    if (segment.kind == PathSegment::Kind::Index) {
      // Anything that is not an array here -- object, scalar, or null --
      // is the table's "[n] on object" / "container expected" TypeMismatch
      // row; bounds are only meaningful once we know it IS an array.
      if (!current->is_array()) {
        return Status::TypeMismatch;
      }
      if (segment.index >= current->size()) {
        return Status::IndexOutOfRange;
      }
      // Bounds already checked above, so this const operator[] is safe --
      // nlohmann's const array operator[] performs no bounds check itself.
      current = &(*current)[segment.index];
    } else {
      // Symmetric to the Index branch: anything that is not an object --
      // array, scalar, or null -- is the table's ".key on array" /
      // "container expected" TypeMismatch row.
      if (!current->is_object()) {
        return Status::TypeMismatch;
      }
      const auto it = current->find(segment.key);
      if (it == current->end()) {
        return Status::PathNotFound;
      }
      current = &(*it);
    }
  }

  out = current;
  return Status::Ok;
}
