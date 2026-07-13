#include "brain_event.hpp"

#include <cctype>
#include <cstdlib>

namespace sonotron {

bool LineBuffer::feed(const char* data, std::size_t len, std::vector<std::string>& out_lines) {
  for (std::size_t i = 0; i < len; ++i) {
    const char c = data[i];
    if (c == '\n') {
      if (!m_pending.empty() && m_pending.back() == '\r') {
        m_pending.pop_back();
      }
      out_lines.push_back(std::move(m_pending));
      m_pending.clear();
    } else {
      m_pending.push_back(c);
    }
  }
  return m_pending.size() <= kMaxLineLength;
}

std::string JsonObject::get_string(const std::string& key, const std::string& fallback) const {
  const auto it = strings.find(key);
  return it == strings.end() ? fallback : it->second;
}

long JsonObject::get_int(const std::string& key, long fallback) const {
  const auto it = ints.find(key);
  return it == ints.end() ? fallback : it->second;
}

namespace {

void skip_ws(const std::string& s, std::size_t& i) {
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) {
    ++i;
  }
}

// Decodes a single hex digit, or -1 if it is not one.
int hex_digit(char h) {
  if (h >= '0' && h <= '9') {
    return h - '0';
  }
  if (h >= 'a' && h <= 'f') {
    return h - 'a' + 10;
  }
  if (h >= 'A' && h <= 'F') {
    return h - 'A' + 10;
  }
  return -1;
}

// Decodes a \uXXXX escape (s[i] sits just past the 'u'). Appends the byte
// when the code point is < 0x80, otherwise '?' (enough for the ASCII labels
// we display). Advances `i` past the 4 hex digits. Returns false on a bad
// escape.
bool decode_unicode_escape(const std::string& s, std::size_t& i, std::string& out) {
  if (i + 4 > s.size()) {
    return false;
  }
  int value = 0;
  for (int k = 0; k < 4; ++k) {
    const int digit = hex_digit(s[i++]);
    if (digit < 0) {
      return false;
    }
    value = (value << 4) | digit;
  }
  out.push_back(value < 0x80 ? static_cast<char>(value) : '?');
  return true;
}

// Decodes one backslash escape (s[i] sits just past the backslash) into
// `out`, advancing `i`. Returns false only on a malformed \uXXXX. An unknown
// escape is tolerated as its literal char.
bool decode_escape(const std::string& s, std::size_t& i, std::string& out) {
  const char esc = s[i++];
  switch (esc) {
    case 'b':
      out.push_back('\b');
      return true;
    case 'f':
      out.push_back('\f');
      return true;
    case 'n':
      out.push_back('\n');
      return true;
    case 'r':
      out.push_back('\r');
      return true;
    case 't':
      out.push_back('\t');
      return true;
    case 'u':
      return decode_unicode_escape(s, i, out);
    default:
      out.push_back(esc);  // ", \, /, or an unknown escape: take it literally
      return true;
  }
}

// Parses a JSON string starting at s[i] == '"'. On success `i` lands just
// past the closing quote and `out` holds the unescaped bytes. Returns false
// on an unterminated or malformed string.
bool parse_string(const std::string& s, std::size_t& i, std::string& out) {
  out.clear();
  ++i;  // consume opening quote
  while (i < s.size()) {
    const char c = s[i++];
    if (c == '"') {
      return true;
    }
    if (c != '\\') {
      out.push_back(c);
    } else if (i >= s.size() || !decode_escape(s, i, out)) {
      return false;
    }
  }
  return false;  // ran off the end without a closing quote
}

// Skips a JSON number, storing it (truncated to integer) in `value`. Accepts
// a leading '-', digits, and an optional fractional/exponent part which we
// scan past but ignore. Returns false only when no digit is present at all.
bool parse_number(const std::string& s, std::size_t& i, long& value) {
  const std::size_t start = i;
  if (i < s.size() && s[i] == '-') {
    ++i;
  }
  const std::size_t digits_begin = i;
  while (i < s.size() && (std::isdigit(static_cast<unsigned char>(s[i])) != 0)) {
    ++i;
  }
  if (i == digits_begin) {
    return false;  // no integer digits
  }
  // Consume a fractional/exponent tail without honouring it (our values are
  // integers; this only keeps the scanner in sync if a float ever appears).
  while (i < s.size() && (s[i] == '.' || s[i] == 'e' || s[i] == 'E' || s[i] == '+' || s[i] == '-' ||
                          (std::isdigit(static_cast<unsigned char>(s[i])) != 0))) {
    ++i;
  }
  value = std::strtol(s.substr(start, i - start).c_str(), nullptr, 10);
  return true;
}

// Skips a balanced nested object/array beginning at an opening brace/bracket,
// so a flat-object scan stays in sync even if a value is itself structured.
bool skip_balanced(const std::string& s, std::size_t& i) {
  int depth = 0;
  bool in_string = false;
  while (i < s.size()) {
    const char c = s[i++];
    if (in_string) {
      if (c == '\\') {
        ++i;  // skip the escaped char
      } else if (c == '"') {
        in_string = false;
      }
      continue;
    }
    if (c == '"') {
      in_string = true;
    } else if (c == '{' || c == '[') {
      ++depth;
    } else if (c == '}' || c == ']') {
      if (--depth == 0) {
        return true;
      }
    }
  }
  return false;
}

// Skips a bare literal (true/false/null) token. Returns false if it is not
// one.
bool skip_literal(const std::string& s, std::size_t& i) {
  const auto matches = [&](const char* lit) {
    std::size_t k = 0;
    while (lit[k] != '\0') {
      if (i + k >= s.size() || s[i + k] != lit[k]) {
        return false;
      }
      ++k;
    }
    i += k;
    return true;
  };
  return matches("true") || matches("false") || matches("null");
}

// Parses the value at s[i] and, if it is a scalar, stores it under `key` in
// `out`; nested objects/arrays and literals are consumed but not stored.
// Advances `i` past the value. Returns false on a malformed value.
bool parse_member_value(const std::string& s, std::size_t& i, const std::string& key,
                        JsonObject& out) {
  const char v = s[i];
  if (v == '"') {
    std::string value;
    if (!parse_string(s, i, value)) {
      return false;
    }
    out.strings[key] = std::move(value);
    return true;
  }
  if (v == '-' || (std::isdigit(static_cast<unsigned char>(v)) != 0)) {
    long value = 0;
    if (!parse_number(s, i, value)) {
      return false;
    }
    out.ints[key] = value;
    return true;
  }
  if (v == '{' || v == '[') {
    return skip_balanced(s, i);
  }
  return skip_literal(s, i);
}

// Parses one `"key": value` member at s[i] (which must be the opening
// quote). Advances `i` past the value. Returns false on any structural
// error.
bool parse_member(const std::string& s, std::size_t& i, JsonObject& out) {
  if (i >= s.size() || s[i] != '"') {
    return false;
  }
  std::string key;
  if (!parse_string(s, i, key)) {
    return false;
  }
  skip_ws(s, i);
  if (i >= s.size() || s[i] != ':') {
    return false;
  }
  ++i;
  skip_ws(s, i);
  if (i >= s.size()) {
    return false;
  }
  return parse_member_value(s, i, key, out);
}

}  // namespace

bool parse_json_object(const std::string& line, JsonObject& out) {
  out.strings.clear();
  out.ints.clear();

  std::size_t i = 0;
  skip_ws(line, i);
  if (i >= line.size() || line[i] != '{') {
    return false;
  }
  ++i;
  skip_ws(line, i);
  if (i < line.size() && line[i] == '}') {
    return true;  // empty object
  }

  while (i < line.size()) {
    skip_ws(line, i);
    if (!parse_member(line, i, out)) {
      return false;
    }
    skip_ws(line, i);
    if (i >= line.size()) {
      return false;
    }
    if (line[i] == ',') {
      ++i;
      continue;
    }
    if (line[i] == '}') {
      return true;
    }
    return false;
  }
  return false;
}

BrainEvent parse_brain_event(const std::string& line) {
  BrainEvent ev;
  JsonObject obj;
  if (!parse_json_object(line, obj)) {
    return ev;  // invalid, kind == kUnknown
  }
  ev.tick = obj.get_int("@", 0);

  // A per-client error line has no "ev" field: {"error":...,"cmd":...}.
  if (obj.has_string("error")) {
    ev.kind = BrainEvent::Kind::kError;
    ev.valid = true;
    ev.error = obj.get_string("error");
    ev.cmd = obj.get_string("cmd");
    return ev;
  }

  const std::string kind = obj.get_string("ev");
  if (kind == "midi-out") {
    ev.kind = BrainEvent::Kind::kMidiOut;
    ev.valid = true;
    ev.port = static_cast<int>(obj.get_int("port", 0));
    ev.msg = obj.get_string("msg");
  } else if (kind == "chord") {
    ev.kind = BrainEvent::Kind::kChord;
    ev.valid = true;
    ev.chord_in = obj.get_string("in");
    ev.chord_out = obj.get_string("out");
    ev.chord_deg = obj.get_string("deg", "-");
  } else if (kind == "section") {
    ev.kind = BrainEvent::Kind::kSection;
    ev.valid = true;
    ev.section_name = obj.get_string("name", "-");
  } else if (kind == "transport") {
    ev.kind = BrainEvent::Kind::kTransport;
    ev.valid = true;
    ev.transport_state = obj.get_string("state", "stopped");
  } else if (kind == "warn") {
    ev.kind = BrainEvent::Kind::kWarn;
    ev.valid = true;
    ev.warn_code = obj.get_string("code");
  } else if (kind == "chord-followed") {
    ev.kind = BrainEvent::Kind::kChordFollowed;
    ev.valid = true;
    ev.followed_current = obj.get_string("cur", "-");
    ev.followed_current_pcs = static_cast<int>(obj.get_int("cur_pcs", 0));
    ev.followed_next = obj.get_string("next", "-");
    ev.followed_next_pcs = static_cast<int>(obj.get_int("next_pcs", 0));
    ev.followed_source = obj.get_string("src", "manual");
  } else if (kind == "beat") {
    ev.kind = BrainEvent::Kind::kBeat;
    ev.valid = true;
    ev.beat_bar = static_cast<int>(obj.get_int("bar", 0));
    ev.beat_index = static_cast<int>(obj.get_int("beat", 0));
    ev.beat_pulse = static_cast<int>(obj.get_int("pulse", 0));
  } else if (kind == "clip") {
    ev.kind = BrainEvent::Kind::kClip;
    ev.valid = true;
    ev.clip_id = static_cast<int>(obj.get_int("id", 0));
    ev.clip_state = obj.get_string("state", "stopped");
  }
  // Any other unrecognized "ev" value falls through to kind == kUnknown,
  // valid == false.
  return ev;
}

}  // namespace sonotron
