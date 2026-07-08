#include "layout_json.hpp"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace sonotron {

namespace {

// -------------------------------------------------------------------------
// Writer.
// -------------------------------------------------------------------------

void append_escaped_string(std::string& out, const std::string& value) {
  out.push_back('"');
  for (const char c : value) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        // Everything else (including multi-byte UTF-8, e.g. the middle dot
        // in "Transport · Seed") passes through unescaped; JSON strings may
        // contain raw UTF-8 outside the ASCII control range.
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
          out += buf;
        } else {
          out.push_back(c);
        }
        break;
    }
  }
  out.push_back('"');
}

// Deterministic float formatting: enough significant digits to round-trip
// exactly, trimmed of a trailing ".0" only when the value is a whole number
// with no fractional weight semantics lost (e.g. `1` not `1.0`... but we
// always keep at least one fractional digit for our 0<w<=~4 weight range to
// stay readable and unambiguous, e.g. "0.15", "0.5", "1").
void append_number(std::string& out, float value) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.6g", static_cast<double>(value));
  out += buf;
}

void append_zone(std::string& out, const Zone& zone, int indent, int depth) {
  const std::string pad(static_cast<std::size_t>(indent) * static_cast<std::size_t>(depth), ' ');
  const std::string field_pad(
      static_cast<std::size_t>(indent) * static_cast<std::size_t>(depth + 1), ' ');
  out += pad + "{\n";
  out += field_pad + "\"id\": ";
  append_escaped_string(out, zone.id);
  out += ",\n";
  out += field_pad + "\"title\": ";
  append_escaped_string(out, zone.title);
  out += ",\n";
  out += field_pad + "\"row\": " + std::to_string(zone.row);
  if (zone.full_span) {
    out += ",\n" + field_pad + "\"span\": \"full\"";
  } else {
    out += ",\n" + field_pad + "\"col\": " + std::to_string(zone.col);
  }
  if (zone.width_weight.has_value()) {
    out += ",\n" + field_pad + "\"w\": ";
    append_number(out, *zone.width_weight);
  }
  if (zone.height_weight.has_value()) {
    out += ",\n" + field_pad + "\"h\": ";
    append_number(out, *zone.height_weight);
  }
  out += "\n" + pad + "}";
}

// -------------------------------------------------------------------------
// Reader: a hand-rolled recursive-descent parser bounded to the exact
// subset of JSON the layout schema uses (a top-level object with "window"
// and a flat "zones" array of scalar-valued objects). Not a general JSON
// value type — see the header comment for why that is deliberate. Escapes
// support the common set (\" \\ \/ \n \r \t); \uXXXX is not implemented
// since our own writer never emits it and the file is trusted, not
// untrusted input.
class Parser {
 public:
  explicit Parser(const std::string& text) : m_text(text) {}

  bool parse(Layout& out) {
    out = Layout{};
    out.zones.clear();
    skip_ws();
    if (!expect('{')) {
      return false;
    }
    skip_ws();
    if (!at_end() && peek() == '}') {
      advance();
      return finish();
    }
    while (true) {
      skip_ws();
      std::string key;
      if (!parse_string(key)) {
        return false;
      }
      skip_ws();
      if (!expect(':')) {
        return false;
      }
      skip_ws();
      if (!parse_top_level_field(key, out)) {
        return false;
      }
      skip_ws();
      if (at_end()) {
        return fail("unterminated object");
      }
      if (peek() == ',') {
        advance();
        continue;
      }
      if (peek() == '}') {
        advance();
        return finish();
      }
      return fail("expected ',' or '}'");
    }
  }

  const std::string& error() const noexcept { return m_error; }

 private:
  // Dispatches one top-level "key": value pair (parser positioned right
  // after the ':') into the matching Layout field, or discards it via
  // skip_value() for an unrecognized key (forward-compatible with a layout
  // file written by a newer version of this app). Split out of parse()'s
  // loop body purely to keep that loop's own cognitive complexity low —
  // no behavior change from having this inline.
  bool parse_top_level_field(const std::string& key, Layout& out) {
    if (key == "window") {
      return parse_string(out.window_title);
    }
    if (key == "font_size") {
      return parse_font_size(out);
    }
    if (key == "zones") {
      return parse_zones(out.zones);
    }
    return skip_value();
  }

  // Parses the "font_size" value into `out.font_size_px`. A syntactically
  // invalid number (e.g. `"font_size": "x"`) is a parse failure like any
  // other malformed field. A syntactically valid but out-of-range value
  // (e.g. `0`) is NOT a parse failure: it is silently ignored, leaving
  // `out.font_size_px` at the compiled-in default already set by
  // `out = Layout{}` in parse() — a stray hand-edited value degrades
  // gracefully instead of making the whole layout file unusable.
  bool parse_font_size(Layout& out) {
    double value = 0.0;
    if (!parse_number(value)) {
      return false;
    }
    const float candidate = static_cast<float>(value);
    if (is_valid_font_size_px(candidate)) {
      out.font_size_px = candidate;
    }
    return true;
  }

  bool finish() {
    skip_ws();
    if (!at_end()) {
      return fail("trailing content after top-level object");
    }
    return true;
  }

  bool parse_zones(std::vector<Zone>& zones) {
    if (!expect('[')) {
      return false;
    }
    skip_ws();
    if (!at_end() && peek() == ']') {
      advance();
      return true;
    }
    while (true) {
      skip_ws();
      Zone zone;
      if (!parse_zone(zone)) {
        return false;
      }
      zones.push_back(std::move(zone));
      skip_ws();
      if (at_end()) {
        return fail("unterminated zones array");
      }
      if (peek() == ',') {
        advance();
        continue;
      }
      if (peek() == ']') {
        advance();
        return true;
      }
      return fail("expected ',' or ']'");
    }
  }

  bool parse_zone(Zone& zone) {
    zone = Zone{};
    std::string span;
    bool has_span = false;
    if (!expect('{')) {
      return false;
    }
    skip_ws();
    if (!at_end() && peek() == '}') {
      advance();
      return true;
    }
    while (true) {
      skip_ws();
      std::string key;
      if (!parse_string(key)) {
        return false;
      }
      skip_ws();
      if (!expect(':')) {
        return false;
      }
      skip_ws();
      if (key == "id") {
        if (!parse_string(zone.id)) {
          return false;
        }
      } else if (key == "title") {
        if (!parse_string(zone.title)) {
          return false;
        }
      } else if (key == "row") {
        double value = 0.0;
        if (!parse_number(value)) {
          return false;
        }
        zone.row = static_cast<int>(std::llround(value));
      } else if (key == "col") {
        double value = 0.0;
        if (!parse_number(value)) {
          return false;
        }
        zone.col = static_cast<int>(std::llround(value));
      } else if (key == "span") {
        if (!parse_string(span)) {
          return false;
        }
        has_span = true;
      } else if (key == "w") {
        double value = 0.0;
        if (!parse_number(value)) {
          return false;
        }
        zone.width_weight = static_cast<float>(value);
      } else if (key == "h") {
        double value = 0.0;
        if (!parse_number(value)) {
          return false;
        }
        zone.height_weight = static_cast<float>(value);
      } else if (!skip_value()) {
        return false;
      }
      skip_ws();
      if (at_end()) {
        return fail("unterminated zone object");
      }
      if (peek() == ',') {
        advance();
        continue;
      }
      if (peek() == '}') {
        advance();
        break;
      }
      return fail("expected ',' or '}'");
    }
    zone.full_span = has_span && span == "full";
    return true;
  }

  bool skip_value() {
    skip_ws();
    if (at_end()) {
      return fail("unexpected end of input");
    }
    const char c = peek();
    if (c == '"') {
      std::string discard;
      return parse_string(discard);
    }
    if (c == '{') {
      return skip_object();
    }
    if (c == '[') {
      return skip_array();
    }
    if (c == 't') {
      return expect_literal("true");
    }
    if (c == 'f') {
      return expect_literal("false");
    }
    if (c == 'n') {
      return expect_literal("null");
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
      double discard = 0.0;
      return parse_number(discard);
    }
    return fail("unexpected character");
  }

  bool skip_object() {
    advance();  // '{'
    skip_ws();
    if (!at_end() && peek() == '}') {
      advance();
      return true;
    }
    while (true) {
      skip_ws();
      std::string key;
      if (!parse_string(key)) {
        return false;
      }
      skip_ws();
      if (!expect(':')) {
        return false;
      }
      skip_ws();
      if (!skip_value()) {
        return false;
      }
      skip_ws();
      if (at_end()) {
        return fail("unterminated object");
      }
      if (peek() == ',') {
        advance();
        continue;
      }
      if (peek() == '}') {
        advance();
        return true;
      }
      return fail("expected ',' or '}'");
    }
  }

  bool skip_array() {
    advance();  // '['
    skip_ws();
    if (!at_end() && peek() == ']') {
      advance();
      return true;
    }
    while (true) {
      skip_ws();
      if (!skip_value()) {
        return false;
      }
      skip_ws();
      if (at_end()) {
        return fail("unterminated array");
      }
      if (peek() == ',') {
        advance();
        continue;
      }
      if (peek() == ']') {
        advance();
        return true;
      }
      return fail("expected ',' or ']'");
    }
  }

  bool expect_literal(const char* literal) {
    const std::size_t len = std::string_view(literal).size();
    if (m_text.compare(m_pos, len, literal) != 0) {
      return fail("invalid literal");
    }
    for (std::size_t i = 0; i < len; ++i) {
      advance();
    }
    return true;
  }

  bool at_end() const noexcept { return m_pos >= m_text.size(); }
  char peek() const noexcept { return m_text[m_pos]; }

  void advance() {
    if (m_text[m_pos] == '\n') {
      ++m_line;
      m_col = 1;
    } else {
      ++m_col;
    }
    ++m_pos;
  }

  void skip_ws() {
    while (!at_end()) {
      const char c = peek();
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        advance();
      } else {
        break;
      }
    }
  }

  bool expect(char c) {
    if (at_end() || peek() != c) {
      return fail(std::string("expected '") + c + "'");
    }
    advance();
    return true;
  }

  bool fail(const std::string& message) {
    if (m_error.empty()) {
      m_error =
          "line " + std::to_string(m_line) + ", column " + std::to_string(m_col) + ": " + message;
    }
    return false;
  }

  bool parse_string(std::string& out) {
    if (!expect('"')) {
      return false;
    }
    out.clear();
    while (!at_end()) {
      const char c = peek();
      if (c == '"') {
        advance();
        return true;
      }
      if (c == '\\') {
        advance();
        if (at_end()) {
          return fail("unterminated escape");
        }
        const char escaped = peek();
        switch (escaped) {
          case '"':
            out.push_back('"');
            break;
          case '\\':
            out.push_back('\\');
            break;
          case '/':
            out.push_back('/');
            break;
          case 'n':
            out.push_back('\n');
            break;
          case 'r':
            out.push_back('\r');
            break;
          case 't':
            out.push_back('\t');
            break;
          default:
            return fail("unsupported escape (only \\\" \\\\ \\/ \\n \\r \\t are supported)");
        }
        advance();
        continue;
      }
      out.push_back(c);
      advance();
    }
    return fail("unterminated string");
  }

  bool parse_number(double& out) {
    const std::size_t start = m_pos;
    if (!at_end() && peek() == '-') {
      advance();
    }
    while (!at_end()) {
      const char c = peek();
      if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
        advance();
      } else {
        break;
      }
    }
    const std::string token = m_text.substr(start, m_pos - start);
    if (token.empty() || token == "-") {
      return fail("invalid number");
    }
    try {
      out = std::stod(token);
    } catch (const std::exception&) {
      return fail("invalid number");
    }
    return true;
  }

  const std::string& m_text;
  std::size_t m_pos = 0;
  int m_line = 1;
  int m_col = 1;
  std::string m_error;
};

}  // namespace

bool parse_layout(const std::string& text, Layout& out, std::string& error) {
  Parser parser(text);
  if (!parser.parse(out)) {
    error = parser.error();
    return false;
  }
  return true;
}

std::string write_layout(const Layout& layout) {
  std::string out;
  out += "{\n";
  out += "  \"window\": ";
  append_escaped_string(out, layout.window_title);
  out += ",\n";
  out += "  \"font_size\": ";
  append_number(out, layout.font_size_px);
  out += ",\n";
  out += "  \"zones\": [";
  if (layout.zones.empty()) {
    out += "]\n";
  } else {
    out += "\n";
    for (std::size_t i = 0; i < layout.zones.size(); ++i) {
      append_zone(out, layout.zones[i], 2, 2);
      if (i + 1 < layout.zones.size()) {
        out += ",";
      }
      out += "\n";
    }
    out += "  ]\n";
  }
  out += "}";
  return out;
}

bool load_or_create_default(const std::string& path, Layout& out, std::string& error) {
  std::error_code exists_error;
  const bool file_exists = std::filesystem::exists(path, exists_error);
  if (!file_exists) {
    out = default_layout();
    return save_layout(path, out, error);
  }

  std::ifstream in(path, std::ios::binary);
  if (!in) {
    error = "could not open layout file for reading: " + path;
    return false;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return parse_layout(buffer.str(), out, error);
}

bool save_layout(const std::string& path, const Layout& layout, std::string& error) {
  std::error_code dir_error;
  const std::filesystem::path file_path(path);
  if (file_path.has_parent_path()) {
    std::filesystem::create_directories(file_path.parent_path(), dir_error);
  }

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    error = "could not open layout file for writing: " + path;
    return false;
  }
  out << write_layout(layout) << "\n";
  if (!out) {
    error = "write failed for layout file: " + path;
    return false;
  }
  return true;
}

}  // namespace sonotron
