#include "mapping/json-accessors.h"

#include "core/json-path.h"
#include "core/type-conversion.h"

namespace {

template <typename Convert>
Status ResolveAndConvert(const JsonValue& document, const PathTokens& tokens, Convert convert) {
  const JsonValue* node = nullptr;
  const Status status = ResolvePath(document, tokens, node);
  if (status != Status::Ok) {
    return status;
  }
  return convert(*node);
}

template <typename Out, typename Read>
Status ReadAtPath(const JsonValue& document, std::string_view path, Out& out, Read read) {
  PathTokens tokens;
  const Status status = ParsePath(path, tokens);
  if (status != Status::Ok) {
    return status;
  }
  return read(document, tokens, out);
}

}  // namespace

Status ReadLongAt(const JsonValue& document, std::string_view path, std::int32_t& out) {
  return ReadAtPath(document, path, out,
                    static_cast<Status (*)(const JsonValue&, const PathTokens&, std::int32_t&)>(ReadLongAt));
}

Status ReadLongAt(const JsonValue& document, const PathTokens& tokens, std::int32_t& out) {
  return ResolveAndConvert(document, tokens, [&out](const JsonValue& node) {
    std::int32_t value = 0;
    const Status status = ToLong(node, value);
    if (status == Status::Ok) {
      out = value;
    }
    return status;
  });
}

Status ReadDoubleAt(const JsonValue& document, std::string_view path, double& out) {
  return ReadAtPath(document, path, out,
                    static_cast<Status (*)(const JsonValue&, const PathTokens&, double&)>(ReadDoubleAt));
}

Status ReadDoubleAt(const JsonValue& document, const PathTokens& tokens, double& out) {
  return ResolveAndConvert(document, tokens, [&out](const JsonValue& node) {
    double value = 0;
    const Status status = ToDouble(node, value);
    if (status == Status::Ok) {
      out = value;
    }
    return status;
  });
}

Status ReadBoolAt(const JsonValue& document, std::string_view path, bool& out) {
  return ReadAtPath(document, path, out,
                    static_cast<Status (*)(const JsonValue&, const PathTokens&, bool&)>(ReadBoolAt));
}

Status ReadBoolAt(const JsonValue& document, const PathTokens& tokens, bool& out) {
  return ResolveAndConvert(document, tokens, [&out](const JsonValue& node) {
    bool value = false;
    const Status status = ToBool(node, value);
    if (status == Status::Ok) {
      out = value;
    }
    return status;
  });
}

Status CountElementsAt(const JsonValue& document, std::string_view path, std::uint32_t& out) {
  return ReadAtPath(document, path, out,
                    static_cast<Status (*)(const JsonValue&, const PathTokens&, std::uint32_t&)>(CountElementsAt));
}

Status CountElementsAt(const JsonValue& document, const PathTokens& tokens, std::uint32_t& out) {
  return ResolveAndConvert(document, tokens, [&out](const JsonValue& node) {
    if (node.is_null()) {
      return Status::NullValue;
    }
    if (!node.is_array()) {
      return Status::TypeMismatch;
    }
    out = static_cast<std::uint32_t>(node.size());
    return Status::Ok;
  });
}
