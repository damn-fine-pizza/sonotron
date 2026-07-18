#include "scenes_json.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>

namespace sonotron {

namespace {

// -------------------------------------------------------------------------
// Writer.
// -------------------------------------------------------------------------

// Escaping identical to layout_json.cpp's append_escaped_string -- kept as
// its own small local copy rather than shared, exactly as layout_json.hpp's
// own header comment explains for why this file does not reach across app
// boundaries for a JSON helper: this whole idiom is meant to be a fresh,
// gui-sonotron-local implementation per file, not a shared library.
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

// -------------------------------------------------------------------------
// Reader: a hand-rolled recursive-descent parser bounded to the exact
// subset of JSON this schema uses (a top-level object with a flat "scenes"
// array of strings). Not a general JSON value type -- see the header
// comment for why that is deliberate. Escapes support the common set
// (\" \\ \/ \n \r \t); \uXXXX is not implemented since our own writer never
// emits it and the file is trusted, not untrusted input -- same discipline
// as layout_json.cpp's own Parser.
class Parser {
 public:
  explicit Parser(const std::string& text) : m_text(text) {}

  bool parse(GridModel& model) {
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
      if (key == "scenes") {
        if (!parse_scenes_array(model)) {
          return false;
        }
      } else if (key == "sections") {
        if (!parse_sections_array(model)) {
          return false;
        }
      } else if (key == "bars") {
        if (!parse_bars_array(model)) {
          return false;
        }
      } else if (key == "repeats") {
        if (!parse_repeats_array(model)) {
          return false;
        }
      } else if (!skip_value()) {
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
  bool finish() {
    skip_ws();
    if (!at_end()) {
      return fail("trailing content after top-level object");
    }
    return true;
  }

  // Parses the "scenes" array value, writing each string into `model` by
  // position. Entries at/past GridModel::kMaxSceneCount are parsed (so the
  // array's own JSON syntax is still fully validated) but discarded --
  // forward-compatible with a file written by a future binary with a
  // larger grid, same discard discipline as an unrecognized top-level key.
  bool parse_scenes_array(GridModel& model) {
    if (!expect('[')) {
      return false;
    }
    skip_ws();
    if (!at_end() && peek() == ']') {
      advance();
      return true;
    }
    std::size_t index = 0;
    while (true) {
      skip_ws();
      std::string value;
      if (!parse_string(value)) {
        return false;
      }
      if (index < GridModel::kMaxSceneCount) {
        model.set_scene_name(index, value);
      }
      ++index;
      skip_ws();
      if (at_end()) {
        return fail("unterminated scenes array");
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

  // Parses the "sections" array value (SLICE 4a additive format extension):
  // small non-negative integers, the raw SectionType byte GridModel::
  // scene_section stores. Entries at/past GridModel::kMaxSceneCount are
  // parsed (so the array's own JSON syntax is still fully validated) but
  // discarded -- same forward-compatibility discipline as
  // parse_scenes_array above. An out-of-range value (>255, or simply not a
  // real SectionType -- this parser does not know SectionType's own bound,
  // that validation lives at the wire-translation layer that consumes it)
  // is still stored as-is; GridModel::scene_section is a raw byte with no
  // value-level validation of its own, same as GridCellKind is never
  // range-checked either.
  bool parse_sections_array(GridModel& model) {
    if (!expect('[')) {
      return false;
    }
    skip_ws();
    if (!at_end() && peek() == ']') {
      advance();
      return true;
    }
    std::size_t index = 0;
    while (true) {
      skip_ws();
      std::uint64_t value = 0;
      if (!parse_uint(value)) {
        return false;
      }
      if (index < GridModel::kMaxSceneCount && value <= 0xFF) {
        model.set_scene_section(index, static_cast<std::uint8_t>(value));
      }
      ++index;
      skip_ws();
      if (at_end()) {
        return fail("unterminated sections array");
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

  // Parses the "bars" array value (auto-song fix, additive format
  // extension): small positive integers, GridModel::scene_bars' own
  // per-scene length in bars. Entries at/past GridModel::kMaxSceneCount are
  // parsed (so the array's own JSON syntax is still fully validated) but
  // discarded -- same forward-compatibility discipline as
  // parse_sections_array above. GridModel::set_scene_bars clamps to >= 1
  // itself, so a stray 0 in an on-disk file is not a parse failure -- it is
  // simply clamped on the way in.
  bool parse_bars_array(GridModel& model) {
    if (!expect('[')) {
      return false;
    }
    skip_ws();
    if (!at_end() && peek() == ']') {
      advance();
      return true;
    }
    std::size_t index = 0;
    while (true) {
      skip_ws();
      std::uint64_t value = 0;
      if (!parse_uint(value)) {
        return false;
      }
      if (index < GridModel::kMaxSceneCount) {
        model.set_scene_bars(index, static_cast<int>(value));
      }
      ++index;
      skip_ws();
      if (at_end()) {
        return fail("unterminated bars array");
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

  // Parses the "repeats" array value (task #5 additive format extension):
  // small positive integers, GridModel::scene_repeat's own per-scene repeat
  // count (or GridModel::kSceneRepeatInfinite, the "hold forever" sentinel).
  // Entries at/past GridModel::kMaxSceneCount are parsed (so the array's own
  // JSON syntax is still fully validated) but discarded -- same forward-
  // compatibility discipline as parse_bars_array above. GridModel::
  // set_scene_repeat clamps to [1, kSceneRepeatInfinite] itself, so a stray
  // out-of-range value in an on-disk file is not a parse failure -- it is
  // simply clamped on the way in.
  bool parse_repeats_array(GridModel& model) {
    if (!expect('[')) {
      return false;
    }
    skip_ws();
    if (!at_end() && peek() == ']') {
      advance();
      return true;
    }
    std::size_t index = 0;
    while (true) {
      skip_ws();
      std::uint64_t value = 0;
      if (!parse_uint(value)) {
        return false;
      }
      if (index < GridModel::kMaxSceneCount) {
        model.set_scene_repeat(index, static_cast<int>(value));
      }
      ++index;
      skip_ws();
      if (at_end()) {
        return fail("unterminated repeats array");
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

  // Parses one non-negative JSON integer literal (no sign, no fraction, no
  // exponent -- write_scenes never emits any of those for a section byte)
  // into `out`. A malformed/negative/fractional token is a parse failure,
  // same discipline as parse_string's own strictness.
  bool parse_uint(std::uint64_t& out) {
    if (at_end() || peek() < '0' || peek() > '9') {
      return fail("expected a non-negative integer");
    }
    out = 0;
    while (!at_end() && peek() >= '0' && peek() <= '9') {
      out = out * 10 + static_cast<std::uint64_t>(peek() - '0');
      advance();
    }
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
      return skip_number();
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

  bool skip_number() {
    if (!at_end() && peek() == '-') {
      advance();
    }
    bool any_digit = false;
    while (!at_end()) {
      const char c = peek();
      if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
        any_digit = any_digit || (c >= '0' && c <= '9');
        advance();
      } else {
        break;
      }
    }
    if (!any_digit) {
      return fail("invalid number");
    }
    return true;
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

  const std::string& m_text;
  std::size_t m_pos = 0;
  int m_line = 1;
  int m_col = 1;
  std::string m_error;
};

}  // namespace

bool parse_scenes(const std::string& text, GridModel& model, std::string& error) {
  Parser parser(text);
  if (!parser.parse(model)) {
    error = parser.error();
    return false;
  }
  return true;
}

std::string write_scenes(const GridModel& model) {
  std::string out;
  out += "{\n";
  out += "  \"scenes\": [";
  if (GridModel::kMaxSceneCount == 0) {
    out += "],\n";
  } else {
    out += "\n";
    for (std::size_t i = 0; i < GridModel::kMaxSceneCount; ++i) {
      out += "    ";
      append_escaped_string(out, std::string(model.scene_name(i)));
      if (i + 1 < GridModel::kMaxSceneCount) {
        out += ",";
      }
      out += "\n";
    }
    out += "  ],\n";
  }
  // SLICE 4a additive format extension: the per-scene SectionType byte,
  // sibling to "scenes" above. A pre-SLICE-4a reader ignores an unknown key
  // (parse_scenes_array is keyed on "scenes" only, its own `else if
  // (!skip_value())` branch), so this stays forward-compatible in both
  // directions.
  out += "  \"sections\": [";
  if (GridModel::kMaxSceneCount == 0) {
    out += "],\n";
  } else {
    out += "\n";
    for (std::size_t i = 0; i < GridModel::kMaxSceneCount; ++i) {
      out += "    " + std::to_string(model.scene_section(i));
      if (i + 1 < GridModel::kMaxSceneCount) {
        out += ",";
      }
      out += "\n";
    }
    out += "  ],\n";
  }
  // Auto-song fix, additive format extension: the per-scene bar length,
  // sibling to "scenes"/"sections" above. A pre-fix reader ignores an
  // unknown key (parse_scenes_array/parse_sections_array are keyed on their
  // own names only, the `else if (!skip_value())` branch), so this stays
  // forward-compatible in both directions.
  out += "  \"bars\": [";
  if (GridModel::kMaxSceneCount == 0) {
    out += "],\n";
  } else {
    out += "\n";
    for (std::size_t i = 0; i < GridModel::kMaxSceneCount; ++i) {
      out += "    " + std::to_string(model.scene_bars(i));
      if (i + 1 < GridModel::kMaxSceneCount) {
        out += ",";
      }
      out += "\n";
    }
    out += "  ],\n";
  }
  // Task #5 additive format extension: the per-scene REPEAT COUNT, sibling
  // to "scenes"/"sections"/"bars" above. A pre-task-#5 reader ignores an
  // unknown key (the same `else if (!skip_value())` branch every other
  // sibling array already relies on), so this stays forward-compatible in
  // both directions.
  out += "  \"repeats\": [";
  if (GridModel::kMaxSceneCount == 0) {
    out += "]\n";
  } else {
    out += "\n";
    for (std::size_t i = 0; i < GridModel::kMaxSceneCount; ++i) {
      out += "    " + std::to_string(model.scene_repeat(i));
      if (i + 1 < GridModel::kMaxSceneCount) {
        out += ",";
      }
      out += "\n";
    }
    out += "  ]\n";
  }
  out += "}";
  return out;
}

bool load_scenes_or_create_default(const std::string& path, GridModel& model, std::string& error) {
  std::error_code exists_error;
  const bool file_exists = std::filesystem::exists(path, exists_error);
  if (!file_exists) {
    return save_scenes(path, model, error);
  }

  std::ifstream in(path, std::ios::binary);
  if (!in) {
    error = "could not open scenes file for reading: " + path;
    return false;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();

  return parse_scenes(buffer.str(), model, error);
}

bool save_scenes(const std::string& path, const GridModel& model, std::string& error) {
  std::error_code dir_error;
  const std::filesystem::path file_path(path);
  if (file_path.has_parent_path()) {
    std::filesystem::create_directories(file_path.parent_path(), dir_error);
  }

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    error = "could not open scenes file for writing: " + path;
    return false;
  }
  out << write_scenes(model) << "\n";
  if (!out) {
    error = "write failed for scenes file: " + path;
    return false;
  }
  return true;
}

}  // namespace sonotron
