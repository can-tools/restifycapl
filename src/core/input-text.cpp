#include "core/input-text.h"

#include <cstring>

Status BoundedText(const char* p, std::uint32_t size, std::string_view& out) {
  if (p == nullptr || size == 0) {
    return Status::InvalidArgument;
  }

  const void* nul = std::memchr(p, '\0', size);
  if (nul == nullptr) {
    return Status::UnterminatedInputText;
  }

  const auto length = static_cast<std::size_t>(static_cast<const char*>(nul) - p);
  out = std::string_view(p, length);
  return Status::Ok;
}
