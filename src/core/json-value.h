#pragma once

#include "json.hpp"

// Object field order is part of the CAPL-visible contract. Switching this back
// to nlohmann::json silently reorders the entries returned to CAPL.
using JsonValue = nlohmann::ordered_json;
