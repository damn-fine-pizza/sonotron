#include "ui_style.hpp"

#include <cstdio>

namespace arrangrr::host {

namespace ansi {

namespace {

// True once an ESC begins an SGR run; the run ends at the terminating 'm'.
bool sgr_terminator(char c) { return c == 'm'; }

}  // namespace

std::size_t visible_length(std::string_view s) {
  std::size_t visible = 0;
  bool in_sgr = false;

  for (char c : s) {
    if (in_sgr) {
      if (sgr_terminator(c)) {
        in_sgr = false;
      }
      continue;
    }
    if (c == kEscape) {
      in_sgr = true;
      continue;
    }
    ++visible;
  }

  return visible;
}

std::string visible_truncate(std::string_view s, std::size_t max_visible) {
  std::string out;
  std::size_t visible = 0;
  bool in_sgr = false;
  bool saw_escape = false;

  for (char c : s) {
    if (in_sgr) {
      out.push_back(c);
      if (sgr_terminator(c)) {
        in_sgr = false;
      }
      continue;
    }
    if (c == kEscape) {
      in_sgr = true;
      saw_escape = true;
      out.push_back(c);
      continue;
    }
    if (visible >= max_visible) {
      // Cut here: keep any remaining SGR closed so styling never leaks.
      if (saw_escape) {
        out += kReset;
      }
      return out;
    }
    out.push_back(c);
    ++visible;
  }

  return out;
}

std::string visible_pad(std::string_view s, std::size_t width) {
  std::string out(s);
  const std::size_t visible = visible_length(s);

  if (visible < width) {
    out.append(width - visible, ' ');
  }

  return out;
}

}  // namespace ansi

namespace {

// Basic SGR foreground codes (30..37); background would be 40..47.
constexpr std::int8_t kRed = 31;
constexpr std::int8_t kGreen = 32;
constexpr std::int8_t kYellow = 33;
constexpr std::int8_t kBlue = 34;
constexpr std::int8_t kMagenta = 35;
constexpr std::int8_t kCyan = 36;

// Designated initializers keep each role self-describing; the array order still
// tracks the UiRole enum (standard C++ has no enum-indexed designators), which
// the kUiRoleCount sentinel guards against drift.
constexpr UiTheme kDefaultTheme = {
    .name = "default",
    .roles = {{
        {},                                                    // kNormal
        {.dim = true},                                         // kMuted
        {.bold = true, .foreground = kCyan},                   // kPanelTitle
        {.bold = true, .reverse = true, .foreground = kCyan},  // kPanelTitleFocused
        {.reverse = true},                                     // kStatusBar
        {.foreground = kCyan},                                 // kInputPrompt
        {.bold = true, .foreground = kCyan},                   // kHelpTopic
        {.bold = true, .reverse = true},                       // kPianoActiveKey
        {.bold = true, .foreground = kGreen},                  // kMidiNoteOn
        {.dim = true},                                         // kMidiNoteOff
        {.foreground = kYellow},                               // kMidiDrum
        {.bold = true, .foreground = kYellow},                 // kMidiNotePending
        {.bold = true, .foreground = kYellow},                 // kWarning
        {.bold = true, .foreground = kRed},                    // kError
        {.bold = true, .foreground = kGreen},                  // kSuccess
    }},
};

// mono: attributes only, never a color code.
constexpr UiTheme kMonoTheme = {
    .name = "mono",
    .roles = {{
        {},                               // kNormal
        {.dim = true},                    // kMuted
        {.bold = true},                   // kPanelTitle
        {.reverse = true},                // kPanelTitleFocused
        {.reverse = true},                // kStatusBar
        {.bold = true},                   // kInputPrompt
        {.bold = true},                   // kHelpTopic
        {.reverse = true},                // kPianoActiveKey
        {.bold = true},                   // kMidiNoteOn
        {.dim = true},                    // kMidiNoteOff
        {.bold = true},                   // kMidiDrum
        {.bold = true, .reverse = true},  // kMidiNotePending
        {.bold = true},                   // kWarning
        {.bold = true, .reverse = true},  // kError
        {.bold = true},                   // kSuccess
    }},
};

// high-contrast: strong bold + reverse and saturated primaries.
constexpr UiTheme kHighContrastTheme = {
    .name = "high-contrast",
    .roles = {{
        {.bold = true},                          // kNormal
        {},                                      // kMuted (legible: no dimming)
        {.bold = true, .reverse = true},         // kPanelTitle
        {.bold = true, .reverse = true},         // kPanelTitleFocused
        {.reverse = true},                       // kStatusBar
        {.bold = true, .foreground = kCyan},     // kInputPrompt
        {.bold = true, .foreground = kCyan},     // kHelpTopic
        {.bold = true, .reverse = true},         // kPianoActiveKey
        {.bold = true, .foreground = kGreen},    // kMidiNoteOn
        {.bold = true, .foreground = kBlue},     // kMidiNoteOff
        {.bold = true, .foreground = kMagenta},  // kMidiDrum
        {.bold = true, .foreground = kYellow},   // kMidiNotePending
        {.bold = true, .foreground = kYellow},   // kWarning
        {.bold = true, .foreground = kRed},      // kError
        {.bold = true, .foreground = kGreen},    // kSuccess
    }},
};

// dark: tuned for a dark background — accents lifted with bold.
constexpr UiTheme kDarkTheme = {
    .name = "dark",
    .roles = {{
        {},                                                    // kNormal
        {.dim = true},                                         // kMuted
        {.bold = true, .foreground = kCyan},                   // kPanelTitle
        {.bold = true, .reverse = true, .foreground = kCyan},  // kPanelTitleFocused
        {.reverse = true},                                     // kStatusBar
        {.bold = true, .foreground = kCyan},                   // kInputPrompt
        {.bold = true, .foreground = kCyan},                   // kHelpTopic
        {.bold = true, .reverse = true},                       // kPianoActiveKey
        {.bold = true, .foreground = kGreen},                  // kMidiNoteOn
        {.dim = true},                                         // kMidiNoteOff
        {.bold = true, .foreground = kYellow},                 // kMidiDrum
        {.bold = true, .foreground = kYellow},                 // kMidiNotePending
        {.bold = true, .foreground = kYellow},                 // kWarning
        {.bold = true, .foreground = kRed},                    // kError
        {.bold = true, .foreground = kGreen},                  // kSuccess
    }},
};

// light: tuned for a light background — darker inks (blue/magenta), never
// yellow or bold-white which vanish on white.
constexpr UiTheme kLightTheme = {
    .name = "light",
    .roles = {{
        {},                                                    // kNormal
        {.dim = true},                                         // kMuted
        {.bold = true, .foreground = kBlue},                   // kPanelTitle
        {.bold = true, .reverse = true, .foreground = kBlue},  // kPanelTitleFocused
        {.reverse = true},                                     // kStatusBar
        {.bold = true, .foreground = kBlue},                   // kInputPrompt
        {.bold = true, .foreground = kMagenta},                // kHelpTopic
        {.bold = true, .reverse = true},                       // kPianoActiveKey
        {.bold = true, .foreground = kBlue},                   // kMidiNoteOn
        {.dim = true},                                         // kMidiNoteOff
        {.foreground = kMagenta},                              // kMidiDrum
        {.bold = true, .foreground = kMagenta},                // kMidiNotePending
        {.bold = true, .foreground = kMagenta},                // kWarning
        {.bold = true, .foreground = kRed},                    // kError
        {.bold = true, .foreground = kGreen},                  // kSuccess
    }},
};

// matrix: green phosphor — everything green, weight/reverse for emphasis.
constexpr UiTheme kMatrixTheme = {
    .name = "matrix",
    .roles = {{
        {.foreground = kGreen},                                 // kNormal
        {.dim = true, .foreground = kGreen},                    // kMuted
        {.bold = true, .foreground = kGreen},                   // kPanelTitle
        {.bold = true, .reverse = true, .foreground = kGreen},  // kPanelTitleFocused
        {.reverse = true, .foreground = kGreen},                // kStatusBar
        {.bold = true, .foreground = kGreen},                   // kInputPrompt
        {.bold = true, .foreground = kGreen},                   // kHelpTopic
        {.bold = true, .reverse = true, .foreground = kGreen},  // kPianoActiveKey
        {.bold = true, .foreground = kGreen},                   // kMidiNoteOn
        {.dim = true, .foreground = kGreen},                    // kMidiNoteOff
        {.foreground = kGreen},                                 // kMidiDrum
        {.reverse = true, .foreground = kGreen},                // kMidiNotePending
        {.bold = true, .foreground = kGreen},                   // kWarning
        {.bold = true, .reverse = true, .foreground = kGreen},  // kError
        {.bold = true, .foreground = kGreen},                   // kSuccess
    }},
};

constexpr std::array<const UiTheme*, 6> kThemes = {
    &kDefaultTheme, &kMonoTheme, &kHighContrastTheme, &kDarkTheme, &kLightTheme, &kMatrixTheme,
};

bool style_is_empty(const TerminalStyle& s) {
  return !s.bold && !s.dim && !s.reverse && s.foreground < 0 && s.background < 0;
}

std::string sgr_open(const TerminalStyle& s) {
  std::string out;
  out.push_back(ansi::kEscape);
  out.push_back('[');

  bool first = true;
  auto add = [&](int code) {
    if (!first) {
      out.push_back(';');
    }
    out += std::to_string(code);
    first = false;
  };

  if (s.bold) {
    add(1);
  }
  if (s.dim) {
    add(2);
  }
  if (s.reverse) {
    add(7);
  }
  if (s.foreground >= 0) {
    add(s.foreground);
  }
  if (s.background >= 0) {
    add(s.background);
  }

  out.push_back('m');
  return out;
}

}  // namespace

UiStyle::UiStyle() : m_theme(&kDefaultTheme) {}

void UiStyle::set_terminal_is_tty(bool is_tty) { m_is_tty = is_tty; }

void UiStyle::set_terminal_utf8(bool utf8) { m_utf8 = utf8; }

void UiStyle::set_color_mode(ColorMode mode) { m_color_mode = mode; }

void UiStyle::set_unicode_mode(UnicodeMode mode) { m_unicode_mode = mode; }

ColorMode UiStyle::color_mode() const { return m_color_mode; }

UnicodeMode UiStyle::unicode_mode() const { return m_unicode_mode; }

bool UiStyle::set_theme(std::string_view name) {
  for (const UiTheme* theme : kThemes) {
    if (theme->name == name) {
      m_theme = theme;
      return true;
    }
  }
  return false;
}

std::string_view UiStyle::theme_name() const { return m_theme->name; }

std::vector<std::string> UiStyle::theme_names() {
  std::vector<std::string> names;
  for (const UiTheme* theme : kThemes) {
    names.emplace_back(theme->name);
  }
  return names;
}

bool UiStyle::colors_enabled() const {
  switch (m_color_mode) {
    case ColorMode::kOn:
      return true;
    case ColorMode::kOff:
      return false;
    case ColorMode::kAuto:
      return m_is_tty;
  }
  return false;
}

bool UiStyle::unicode_enabled() const {
  switch (m_unicode_mode) {
    case UnicodeMode::kOn:
      return true;
    case UnicodeMode::kOff:
      return false;
    case UnicodeMode::kAuto:
      return m_utf8;
  }
  return false;
}

std::string UiStyle::apply(UiRole role, std::string_view text) const {
  // Colors off (mode or non-TTY): the text passes through untouched — this is
  // what keeps `--no-colors` and piped output free of escape sequences.
  if (!colors_enabled()) {
    return std::string(text);
  }

  const TerminalStyle& style = m_theme->roles[static_cast<std::size_t>(role)];
  if (style_is_empty(style)) {
    return std::string(text);
  }

  std::string out = sgr_open(style);
  out.append(text);
  out += ansi::kReset;
  return out;
}

}  // namespace arrangrr::host
