#include "core/type-conversion.h"

#include <charconv>
#include <cmath>
#include <limits>

namespace {

// "3.5" and "3.0" both parse as an integer prefix ("3") plus this leftover
// -- reparsing the whole text as a double is what lets ParseLong tell a
// fractional value (NotIntegral) apart from real trailing garbage
// (ParseError, e.g. "12abc").
bool LooksLikeFloatContinuation(char c) {
  return c == '.' || c == 'e' || c == 'E';
}

Status FractionalTextToLong(std::string_view text, std::int32_t& out) {
  double value = 0.0;
  const char* begin = text.data();
  const char* end = text.data() + text.size();
  const auto result = std::from_chars(begin, end, value);

  if (result.ec == std::errc::result_out_of_range) {
    return Status::NumericOverflow;
  }
  if (result.ec != std::errc{} || result.ptr != end) {
    return Status::ParseError;
  }

  const double truncated = std::trunc(value);
  if (value != truncated) {
    return Status::NotIntegral;
  }
  if (truncated < static_cast<double>(std::numeric_limits<std::int32_t>::min()) ||
      truncated > static_cast<double>(std::numeric_limits<std::int32_t>::max())) {
    return Status::NumericOverflow;
  }
  out = static_cast<std::int32_t>(truncated);
  return Status::Ok;
}

}  // namespace

Status ToLong(const nlohmann::json& value, std::int32_t& out) {
  if (value.is_null()) {
    return Status::NullValue;
  }
  if (!value.is_number()) {
    return Status::TypeMismatch;
  }

  // Branch on nlohmann's own storage kind rather than converting through
  // get<int64_t>()/get<uint64_t>() indiscriminately -- narrowing across
  // those two storage kinds is not range-checked by nlohmann itself, so
  // doing it here would reintroduce the same overflow hazard this function
  // already guards against, just one level down.
  if (value.is_number_unsigned()) {
    const std::uint64_t raw = value.get<std::uint64_t>();
    if (raw > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) {
      return Status::NumericOverflow;
    }
    out = static_cast<std::int32_t>(raw);
    return Status::Ok;
  }
  if (value.is_number_integer()) {
    const std::int64_t raw = value.get<std::int64_t>();
    if (raw < static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()) ||
        raw > static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())) {
      return Status::NumericOverflow;
    }
    out = static_cast<std::int32_t>(raw);
    return Status::Ok;
  }

  // Remaining case: is_number_float(). A JSON literal too large for
  // int64_t/uint64_t is parsed by nlohmann as a float, so this branch also
  // covers "beyond int64_t range" inputs.
  const double raw = value.get<double>();
  const double truncated = std::trunc(raw);
  if (raw != truncated) {
    return Status::NotIntegral;
  }
  if (truncated < static_cast<double>(std::numeric_limits<std::int32_t>::min()) ||
      truncated > static_cast<double>(std::numeric_limits<std::int32_t>::max())) {
    return Status::NumericOverflow;
  }
  out = static_cast<std::int32_t>(truncated);
  return Status::Ok;
}

Status ToDouble(const nlohmann::json& value, double& out) {
  if (value.is_null()) {
    return Status::NullValue;
  }
  if (!value.is_number()) {
    return Status::TypeMismatch;
  }
  out = value.get<double>();
  return Status::Ok;
}

Status ToBool(const nlohmann::json& value, bool& out) {
  if (value.is_null()) {
    return Status::NullValue;
  }
  if (!value.is_boolean()) {
    return Status::TypeMismatch;
  }
  out = value.get<bool>();
  return Status::Ok;
}

Status ToText(const nlohmann::json& value, std::string& out) {
  if (value.is_null()) {
    return Status::NullValue;
  }
  if (!value.is_string()) {
    return Status::TypeMismatch;
  }
  out = value.get<std::string>();
  return Status::Ok;
}

Status ValueToText(const nlohmann::json& value, std::string& out) {
  if (value.is_null()) {
    out = "null";
    return Status::Ok;
  }
  if (value.is_string()) {
    out = value.get<std::string>();
    return Status::Ok;
  }
  if (value.is_boolean() || value.is_number()) {
    // nlohmann's own serializer -- locale-independent.
    out = value.dump();
    return Status::Ok;
  }
  return Status::TypeMismatch;
}

Status ParseLong(std::string_view text, std::int32_t& out) {
  if (text.empty()) {
    return Status::InvalidArgument;
  }

  std::int32_t value = 0;
  const char* begin = text.data();
  const char* end = text.data() + text.size();
  const auto result = std::from_chars(begin, end, value);

  if (result.ec == std::errc::result_out_of_range) {
    return Status::NumericOverflow;
  }
  if (result.ec == std::errc{} && result.ptr == end) {
    out = value;
    return Status::Ok;
  }
  if (result.ec == std::errc{} && result.ptr != end &&
      LooksLikeFloatContinuation(*result.ptr)) {
    return FractionalTextToLong(text, out);
  }
  return Status::ParseError;
}

Status ParseDouble(std::string_view text, double& out) {
  if (text.empty()) {
    return Status::InvalidArgument;
  }

  double value = 0.0;
  const char* begin = text.data();
  const char* end = text.data() + text.size();
  const auto result = std::from_chars(begin, end, value);

  if (result.ec == std::errc::result_out_of_range) {
    return Status::NumericOverflow;
  }
  if (result.ec != std::errc{} || result.ptr != end) {
    return Status::ParseError;
  }
  out = value;
  return Status::Ok;
}
