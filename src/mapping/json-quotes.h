// json-quotes.h -- apostrophe-quoted JSON strings to standard JSON; see docs/json-flatten.md.
#pragma once

#include <string>
#include <string_view>

#include "core/status.h"

// Rewrites 'text' strings as "text" strings; text without an apostrophe outside
// double-quoted strings is copied byte for byte.
Status ConvertApostropheStrings(std::string_view in, std::string& out);
