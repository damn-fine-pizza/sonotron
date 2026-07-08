#include "json.hpp"

#include <cmath>
#include <cstdio>

namespace arrstyle {

Json Json::boolean(bool v) {
  Json j;
  j.m_type = Type::kBool;
  j.m_bool = v;
  return j;
}

Json Json::integer(std::int64_t v) {
  Json j;
  j.m_type = Type::kInt;
  j.m_int = v;
  return j;
}

Json Json::string(std::string v) {
  Json j;
  j.m_type = Type::kString;
  j.m_string = std::move(v);
  return j;
}

Json Json::array() {
  Json j;
  j.m_type = Type::kArray;
  return j;
}

Json Json::object() {
  Json j;
  j.m_type = Type::kObject;
  return j;
}

void Json::set(std::string key, Json value) {
  m_type = Type::kObject;
  for (Member& m : m_object) {
    if (m.first == key) {
      m.second = std::move(value);
      return;
    }
  }
  m_object.emplace_back(std::move(key), std::move(value));
}

const Json* Json::find(const std::string& key) const noexcept {
  if (m_type != Type::kObject) {
    return nullptr;
  }
  for (const Member& m : m_object) {
    if (m.first == key) {
      return &m.second;
    }
  }
  return nullptr;
}

void Json::push_back(Json value) {
  m_type = Type::kArray;
  m_array.push_back(std::move(value));
}

namespace {

void append_escaped(std::string& out, const std::string& s) {
  out.push_back('"');
  for (const char c : s) {
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

void append_indent(std::string& out, int indent, int depth) {
  out.append(static_cast<std::size_t>(indent) * static_cast<std::size_t>(depth), ' ');
}

}  // namespace

void Json::dump_to(std::string& out, int indent, int depth) const {
  switch (m_type) {
    case Type::kNull:
      out += "null";
      break;
    case Type::kBool:
      out += m_bool ? "true" : "false";
      break;
    case Type::kInt:
      out += std::to_string(m_int);
      break;
    case Type::kString:
      append_escaped(out, m_string);
      break;
    case Type::kArray: {
      if (m_array.empty()) {
        out += "[]";
        break;
      }
      out += "[\n";
      for (std::size_t i = 0; i < m_array.size(); ++i) {
        append_indent(out, indent, depth + 1);
        m_array[i].dump_to(out, indent, depth + 1);
        if (i + 1 < m_array.size()) {
          out.push_back(',');
        }
        out.push_back('\n');
      }
      append_indent(out, indent, depth);
      out.push_back(']');
      break;
    }
    case Type::kObject: {
      if (m_object.empty()) {
        out += "{}";
        break;
      }
      out += "{\n";
      for (std::size_t i = 0; i < m_object.size(); ++i) {
        append_indent(out, indent, depth + 1);
        append_escaped(out, m_object[i].first);
        out += ": ";
        m_object[i].second.dump_to(out, indent, depth + 1);
        if (i + 1 < m_object.size()) {
          out.push_back(',');
        }
        out.push_back('\n');
      }
      append_indent(out, indent, depth);
      out.push_back('}');
      break;
    }
  }
}

std::string Json::dump(int indent) const {
  std::string out;
  dump_to(out, indent, 0);
  return out;
}

// -------------------------------------------------------------------------
// Reader (recursive descent, bounded by input length).
// -------------------------------------------------------------------------

namespace {

class Parser {
 public:
  explicit Parser(const std::string& text) : m_text(text) {}

  bool parse(Json& out) {
    skip_ws();
    if (!parse_value(out)) {
      return false;
    }
    skip_ws();
    if (m_pos != m_text.size()) {
      return fail("trailing content after top-level value");
    }
    return true;
  }

  const std::string& error() const noexcept { return m_error; }

 private:
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

  bool fail(const std::string& msg) {
    if (m_error.empty()) {
      m_error = "line " + std::to_string(m_line) + ", column " + std::to_string(m_col) + ": " + msg;
    }
    return false;
  }

  bool parse_value(Json& out) {
    if (at_end()) {
      return fail("unexpected end of input");
    }
    const char c = peek();
    switch (c) {
      case '{':
        return parse_object(out);
      case '[':
        return parse_array(out);
      case '"': {
        std::string s;
        if (!parse_string(s)) {
          return false;
        }
        out = Json::string(std::move(s));
        return true;
      }
      case 't':
      case 'f':
        return parse_bool(out);
      case 'n':
        return parse_null(out);
      default:
        if (c == '-' || (c >= '0' && c <= '9')) {
          return parse_number(out);
        }
        return fail("unexpected character");
    }
  }

  bool parse_object(Json& out) {
    out = Json::object();
    advance();  // '{'
    skip_ws();
    if (!at_end() && peek() == '}') {
      advance();
      return true;
    }
    while (true) {
      skip_ws();
      if (at_end() || peek() != '"') {
        return fail("expected string key");
      }
      std::string key;
      if (!parse_string(key)) {
        return false;
      }
      skip_ws();
      if (at_end() || peek() != ':') {
        return fail("expected ':'");
      }
      advance();
      skip_ws();
      Json value;
      if (!parse_value(value)) {
        return false;
      }
      out.set(std::move(key), std::move(value));
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

  bool parse_array(Json& out) {
    out = Json::array();
    advance();  // '['
    skip_ws();
    if (!at_end() && peek() == ']') {
      advance();
      return true;
    }
    while (true) {
      skip_ws();
      Json value;
      if (!parse_value(value)) {
        return false;
      }
      out.push_back(std::move(value));
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

  bool parse_string(std::string& out) {
    advance();  // opening quote
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
        const char e = peek();
        switch (e) {
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
          case 'b':
            out.push_back('\b');
            break;
          case 'f':
            out.push_back('\f');
            break;
          case 'u':
            if (!parse_unicode(out)) {
              return false;
            }
            continue;  // parse_unicode already consumed the digits
          default:
            return fail("invalid escape");
        }
        advance();
        continue;
      }
      out.push_back(c);
      advance();
    }
    return fail("unterminated string");
  }

  bool parse_unicode(std::string& out) {
    advance();  // 'u'
    unsigned code = 0;
    for (int i = 0; i < 4; ++i) {
      if (at_end()) {
        return fail("truncated \\u escape");
      }
      const char h = peek();
      unsigned nibble = 0;
      if (h >= '0' && h <= '9') {
        nibble = static_cast<unsigned>(h - '0');
      } else if (h >= 'a' && h <= 'f') {
        nibble = static_cast<unsigned>(h - 'a' + 10);
      } else if (h >= 'A' && h <= 'F') {
        nibble = static_cast<unsigned>(h - 'A' + 10);
      } else {
        return fail("invalid \\u digit");
      }
      code = (code << 4) | nibble;
      advance();
    }
    // Minimal UTF-8 encoding for the basic multilingual plane (enough for the
    // ASCII text our writer produces; surrogate pairs are not needed here).
    if (code < 0x80) {
      out.push_back(static_cast<char>(code));
    } else if (code < 0x800) {
      out.push_back(static_cast<char>(0xC0 | (code >> 6)));
      out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xE0 | (code >> 12)));
      out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    }
    return true;
  }

  bool parse_bool(Json& out) {
    if (m_text.compare(m_pos, 4, "true") == 0) {
      for (int i = 0; i < 4; ++i) {
        advance();
      }
      out = Json::boolean(true);
      return true;
    }
    if (m_text.compare(m_pos, 5, "false") == 0) {
      for (int i = 0; i < 5; ++i) {
        advance();
      }
      out = Json::boolean(false);
      return true;
    }
    return fail("invalid literal");
  }

  bool parse_null(Json& out) {
    if (m_text.compare(m_pos, 4, "null") == 0) {
      for (int i = 0; i < 4; ++i) {
        advance();
      }
      out = Json::null();
      return true;
    }
    return fail("invalid literal");
  }

  bool parse_number(Json& out) {
    const std::size_t start = m_pos;
    bool is_float = false;
    if (!at_end() && peek() == '-') {
      advance();
    }
    while (!at_end()) {
      const char c = peek();
      if (c >= '0' && c <= '9') {
        advance();
      } else if (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
        is_float = true;
        advance();
      } else {
        break;
      }
    }
    const std::string token = m_text.substr(start, m_pos - start);
    if (token.empty() || token == "-") {
      return fail("invalid number");
    }
    if (is_float) {
      out = Json::integer(static_cast<std::int64_t>(std::llround(std::stod(token))));
    } else {
      out = Json::integer(static_cast<std::int64_t>(std::stoll(token)));
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

bool parse_json(const std::string& text, Json& out, std::string& error) {
  Parser parser(text);
  if (!parser.parse(out)) {
    error = parser.error();
    return false;
  }
  return true;
}

}  // namespace arrstyle
