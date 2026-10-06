#include "mapping/json-flatten.h"

#include <algorithm>
#include <utility>

#include "core/type-conversion.h"
#include "mapping/json-quotes.h"

namespace {

// Limits are enforced in this SAX pass so oversized input is rejected before any DOM allocation.
class LimitCheckingSax final : public nlohmann::json_sax<JsonValue> {
 public:
  Status failure() const { return failure_; }

  bool null() override { return Leaf(); }
  bool boolean(bool) override { return Leaf(); }
  bool number_integer(number_integer_t) override { return Leaf(); }
  bool number_unsigned(number_unsigned_t) override { return Leaf(); }
  bool number_float(number_float_t, const string_t&) override { return Leaf(); }
  bool string(string_t&) override { return Leaf(); }
  bool binary(binary_t&) override { return Reject(Status::ParseError); }

  bool start_object(std::size_t) override { return Open(false); }
  bool end_object() override { return Close(); }
  bool start_array(std::size_t) override { return Open(true); }
  bool end_array() override { return Close(); }

  bool key(string_t& name) override {
    previousEventOpenedContainer_ = false;
    if (name.find('\0') != string_t::npos) {
      return Reject(Status::ParseError);
    }
    pendingTokenBytes_ = name.size();
    for (const char c : name) {
      if (c == '~' || c == '/') {
        ++pendingTokenBytes_;
      }
    }
    return true;
  }

  bool parse_error(std::size_t, const std::string&, const JsonValue::exception&) override {
    return false;
  }

 private:
  bool Reject(Status status) {
    failure_ = status;
    return false;
  }

  bool CountEntry() {
    if (entryCount_ >= kMaxFlatEntries) {
      return Reject(Status::TooManyEntries);
    }
    ++entryCount_;
    return true;
  }

  bool AddKeyBytes(std::size_t keyBytes) {
    if (keyBytes > kMaxFlatKeyBytes - keyTotal_) {
      return Reject(Status::KeyTextTooLarge);
    }
    keyTotal_ += keyBytes;
    return true;
  }

  static std::size_t DecimalDigits(std::size_t n) {
    std::size_t digits = 1;
    while (n >= 10) {
      n /= 10;
      ++digits;
    }
    return digits;
  }

  std::size_t NextKeyBytes() {
    if (depth_ == 0) {
      return 0;
    }
    KeyFrame& top = frames_[depth_ - 1];
    const std::size_t token = top.isArray ? DecimalDigits(top.nextIndex++) : pendingTokenBytes_;
    return top.prefixBytes + 1 + token;
  }

  bool Leaf() {
    previousEventOpenedContainer_ = false;
    const std::size_t keyBytes = NextKeyBytes();
    return CountEntry() && AddKeyBytes(keyBytes);
  }

  bool Open(bool isArray) {
    if (depth_ >= kMaxJsonDepth) {
      return Reject(Status::NestingTooDeep);
    }
    const std::size_t keyBytes = NextKeyBytes();
    frames_[depth_] = KeyFrame{isArray, 0, keyBytes};
    ++depth_;
    previousEventOpenedContainer_ = true;
    return true;
  }

  bool Close() {
    const bool wasEmpty = previousEventOpenedContainer_;
    previousEventOpenedContainer_ = false;
    --depth_;
    return !wasEmpty || (CountEntry() && AddKeyBytes(frames_[depth_].prefixBytes));
  }

  struct KeyFrame {
    bool isArray = false;
    std::size_t nextIndex = 0;
    std::size_t prefixBytes = 0;
  };

  Status failure_ = Status::ParseError;
  std::size_t depth_ = 0;
  std::size_t entryCount_ = 0;
  std::size_t keyTotal_ = 0;
  std::size_t pendingTokenBytes_ = 0;
  KeyFrame frames_[kMaxJsonDepth];
  bool previousEventOpenedContainer_ = false;
};

struct Frame {
  JsonValue::const_iterator next;
  JsonValue::const_iterator end;
  std::size_t index;
  std::size_t prefixSize;
  bool isObject;
};

void AppendEscapedToken(std::string& out, const std::string& token) {
  for (const char c : token) {
    if (c == '~') {
      out += "~0";
    } else if (c == '/') {
      out += "~1";
    } else {
      out.push_back(c);
    }
  }
}

bool IsNonEmptyContainer(const JsonValue& node) {
  return (node.is_object() || node.is_array()) && !node.empty();
}

JsonEntryType ClassifyScalar(const JsonValue& node) {
  if (node.is_string()) {
    return JsonEntryType::String;
  }
  if (node.is_number()) {
    return JsonEntryType::Number;
  }
  if (node.is_boolean()) {
    return JsonEntryType::Bool;
  }
  if (node.is_null()) {
    return JsonEntryType::Null;
  }
  return JsonEntryType::None;
}

Status AppendLeaf(const std::string& key, const JsonValue& node, std::vector<FlatEntry>& entries) {
  FlatEntry entry;
  entry.key = key;
  const Status status = DescribeLeaf(node, entry.value, entry.type);
  if (status != Status::Ok) {
    return status;
  }
  entries.push_back(std::move(entry));
  return Status::Ok;
}

Status FlattenDocument(const JsonValue& root, std::vector<FlatEntry>& entries) {
  std::string path;
  if (!IsNonEmptyContainer(root)) {
    return AppendLeaf(path, root, entries);
  }

  std::vector<Frame> stack;
  stack.push_back(Frame{root.begin(), root.end(), 0, 0, root.is_object()});
  while (!stack.empty()) {
    Frame& frame = stack.back();
    if (frame.next == frame.end) {
      stack.pop_back();
      continue;
    }

    path.resize(frame.prefixSize);
    path.push_back('/');
    if (frame.isObject) {
      AppendEscapedToken(path, frame.next.key());
    } else {
      path += std::to_string(frame.index);
    }
    const JsonValue& child = *frame.next;
    ++frame.next;
    ++frame.index;

    if (IsNonEmptyContainer(child)) {
      stack.push_back(Frame{child.begin(), child.end(), 0, path.size(), child.is_object()});
    } else {
      const Status status = AppendLeaf(path, child, entries);
      if (status != Status::Ok) {
        return status;
      }
    }
  }
  return Status::Ok;
}

}  // namespace

Status DescribeLeaf(const JsonValue& node, std::string& text, JsonEntryType& type) {
  if (node.is_object() || node.is_array()) {
    if (!node.empty()) {
      return Status::TypeMismatch;
    }
    text = node.is_object() ? "{}" : "[]";
    type = node.is_object() ? JsonEntryType::EmptyObject : JsonEntryType::EmptyArray;
    return Status::Ok;
  }

  const Status status = ValueToText(node, text);
  if (status != Status::Ok) {
    return status;
  }
  type = ClassifyScalar(node);
  return Status::Ok;
}

Status FlattenJson(std::string_view text, FlattenResult& out) {
  if (text.size() > kMaxJsonInputBytes) {
    return Status::DocumentTooLarge;
  }
  if (text.empty()) {
    return Status::ParseError;
  }
  if (std::find(text.begin(), text.end(), '\0') != text.end()) {
    return Status::ParseError;
  }

  std::string converted;
  if (text.find('\'') != std::string_view::npos) {
    const Status quoteStatus = ConvertApostropheStrings(text, converted);
    if (quoteStatus != Status::Ok) {
      return quoteStatus;
    }
    text = converted;
  }

  const char* const first = text.data();
  const char* const last = first + text.size();

  LimitCheckingSax sax;
  if (!JsonValue::sax_parse(first, last, &sax)) {
    return sax.failure();
  }

  // allow_exceptions=false: failure yields a discarded value instead of throwing.
  JsonValue document = JsonValue::parse(first, last, nullptr, false);
  if (document.is_discarded()) {
    return Status::ParseError;
  }

  std::vector<FlatEntry> entries;
  const Status status = FlattenDocument(document, entries);
  if (status != Status::Ok) {
    return status;
  }

  out.document = std::move(document);
  out.entries = std::move(entries);
  return Status::Ok;
}
