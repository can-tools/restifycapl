// input-text.h -- raw buffer to C++ text view, the inverse of
// buffer-copy.h. Grammar and rationale: docs/capl-sync-surface.md.
#pragma once

#include <cstdint>
#include <string_view>

#include "core/status.h"

// p == nullptr or size == 0 -> InvalidArgument, out untouched. No NUL found
// within p[0..size) -> UnterminatedInputText, out untouched. Otherwise out
// becomes a view of p up to (not including) the first NUL -> Ok.
Status BoundedText(const char* p, std::uint32_t size, std::string_view& out);
