#include "core/buffer-copy.h"

#include <cstring>

Status CopyToBuffer(std::string_view text, char* buffer, std::uint32_t bufferSize) {
  if (buffer == nullptr || bufferSize == 0) {
    return Status::InvalidArgument;
  }
  buffer[0] = '\0';

  // text.size() + 1 > bufferSize can wrap at SIZE_MAX; this form cannot.
  if (text.size() >= static_cast<std::size_t>(bufferSize)) {
    return Status::BufferTooSmall;
  }

  std::memcpy(buffer, text.data(), text.size());
  buffer[text.size()] = '\0';
  return Status::Ok;
}
