// buffer-copy.h -- bounds-checked copy of C++ text into a caller-owned raw
// buffer (src/core, level 0, see
// docs/work/stage-08-core-pure-logic/plans/plan.md §6 CPP-16).
//
// Filed separately from type-conversion.h (plan.md D2a): type-conversion.h
// holds only functions whose direction is JSON/text in, C++ value out;
// this header holds the one function whose direction is the opposite --
// C++ value out to a raw caller-owned buffer. It has exactly one consumer,
// src/module/exports.cpp's restifyGetVersion.
#pragma once

#include <cstdint>
#include <string_view>

#include "core/status.h"

// buffer == nullptr || bufferSize == 0 -> InvalidArgument, writes nothing.
// Otherwise buffer[0] is cleared first. If text (plus its NUL terminator)
// does not fit -> BufferTooSmall, buffer left empty, never truncated.
// Otherwise text is copied and NUL-terminated -> Ok.
Status CopyToBuffer(std::string_view text, char* buffer, std::uint32_t bufferSize);
