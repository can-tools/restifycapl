#include "mapping/json-quotes.h"

namespace {
enum class Scan { Outside, DoubleQuoted, Apostrophe };
}

Status ConvertApostropheStrings(std::string_view in, std::string& out) {
  out.clear();
  out.reserve(in.size());
  Scan state = Scan::Outside;
  for (std::size_t i = 0; i < in.size(); ++i) {
    const char c = in[i];
    switch (state) {
      case Scan::Outside:
        if (c == '\'') {
          out.push_back('"');
          state = Scan::Apostrophe;
        } else {
          out.push_back(c);
          if (c == '"') {
            state = Scan::DoubleQuoted;
          }
        }
        break;
      case Scan::DoubleQuoted:
        out.push_back(c);
        if (c == '\\' && i + 1 < in.size()) {
          out.push_back(in[++i]);
        } else if (c == '"') {
          state = Scan::Outside;
        }
        break;
      case Scan::Apostrophe:
        if (c == '\\' && i + 1 < in.size()) {
          // Only \' is rewritten; any other pair is copied whole, so \\' closes the string.
          const char next = in[++i];
          if (next == '\'') {
            out.push_back('\'');
          } else {
            out.push_back(c);
            out.push_back(next);
          }
        } else if (c == '\'') {
          out.push_back('"');
          state = Scan::Outside;
        } else if (c == '"') {
          out.append("\\\"");
        } else {
          out.push_back(c);
        }
        break;
    }
  }
  return Status::Ok;
}
