#include "rc_config.hpp"

#include <fstream>
#include <istream>

namespace arrangrr::host {

namespace {

// Trims leading/trailing ASCII whitespace.
std::string trim(const std::string& s) {
  std::size_t begin = 0;
  std::size_t end = s.size();
  while (begin < end && (s[begin] == ' ' || s[begin] == '\t' || s[begin] == '\r')) {
    ++begin;
  }
  while (end > begin && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r')) {
    --end;
  }
  return s.substr(begin, end - begin);
}

// Strips a trailing '#' comment (comments are whole-line or trailing here).
std::string strip_comment(const std::string& s) {
  const std::size_t hash = s.find('#');
  return hash == std::string::npos ? s : s.substr(0, hash);
}

// Parses a non-negative integer; returns false on anything else.
bool parse_uint(const std::string& s, int& out) {
  if (s.empty()) {
    return false;
  }
  int value = 0;
  for (const char c : s) {
    if (c < '0' || c > '9') {
      return false;
    }
    value = value * 10 + (c - '0');
  }
  out = value;
  return true;
}

void parse_panel_line(const std::string& body, RcConfig& config) {
  std::string text = body;
  bool full_row = false;
  if (!text.empty() && text[0] == '*') {
    full_row = true;
    text = trim(text.substr(1));
  }

  int height = 0;
  const std::size_t eq = text.find('=');
  if (eq != std::string::npos) {
    const std::string rows = trim(text.substr(eq + 1));
    text = trim(text.substr(0, eq));
    if (!parse_uint(rows, height) || height <= 0) {
      config.warnings.push_back("bad height for '" + text + "': " + rows);
      height = 0;
    }
  }

  PanelId id{};
  if (!parse_panel_name(text, id)) {
    config.warnings.push_back("unknown panel '" + text + "'");
    return;
  }
  config.order.push_back(RcPanel{.id = id, .full_row = full_row, .height = height});
}

}  // namespace

RcConfig parse_rc(std::istream& in) {
  RcConfig config;
  bool in_panels = false;
  std::string raw;
  while (std::getline(in, raw)) {
    const std::string line = trim(strip_comment(raw));
    if (line.empty()) {
      continue;
    }
    if (line == "[panels]") {
      in_panels = true;
      continue;
    }

    const std::size_t eq = line.find('=');
    if (!in_panels && eq != std::string::npos) {
      const std::string key = trim(line.substr(0, eq));
      const std::string value = trim(line.substr(eq + 1));
      if (key == "layout") {
        int per_row = 0;
        if (parse_uint(value, per_row) &&
            (per_row == panel_layout::kOnePerRow || per_row == panel_layout::kTwoPerRow)) {
          config.has_layout = true;
          config.per_row = per_row;
        } else {
          config.warnings.push_back("bad layout: " + value);
        }
      } else {
        config.warnings.push_back("unknown key '" + key + "'");
      }
      continue;
    }

    if (in_panels) {
      parse_panel_line(line, config);
    } else {
      config.warnings.push_back("unexpected line: " + line);
    }
  }
  return config;
}

RcConfig load_rc(const std::string& path) {
  std::ifstream file(path);
  if (!file) {
    return RcConfig{};  // missing file -> built-in defaults
  }
  return parse_rc(file);
}

}  // namespace arrangrr::host
