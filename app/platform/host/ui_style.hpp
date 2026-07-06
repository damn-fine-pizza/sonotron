#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Host-only terminal styling (H3, docs/TUI_SPEC.md §6): the ONE place ANSI/SGR
// protocol lives. Renderers ask for a semantic UiRole; the active theme maps it
// to concrete attributes. With colors disabled every role resolves to the plain
// text verbatim (zero escapes), so `--no-colors` and non-TTY output stay clean.

namespace arrangrr::host {

namespace ansi {

inline constexpr char kEscape = '\x1b';
inline constexpr std::string_view kReset = "\x1b[0m";

// Visible width of a string that may carry SGR sequences (ESC '[' ... 'm'):
// the escape runs count as zero columns.
std::size_t visible_length(std::string_view s);

// Truncate to at most `max_visible` visible columns. SGR runs are preserved and
// never split; a trailing reset is appended when the text was cut mid-style.
std::string visible_truncate(std::string_view s, std::size_t max_visible);

// Right-pad with spaces to `width` visible columns (no-op when already wider).
std::string visible_pad(std::string_view s, std::size_t width);

}  // namespace ansi

// Semantic styling roles: renderers reference these, never raw colors.
enum class UiRole {
  kNormal,
  kMuted,
  kPanelTitle,
  kPanelTitleFocused,
  kStatusBar,
  kInputPrompt,
  kHelpTopic,
  kPianoActiveKey,
  kMidiNoteOn,
  kMidiNoteOff,
  kMidiDrum,
  kMidiNotePending,
  kWarning,
  kError,
  kSuccess,
  kCount,  // sentinel: keeps the theme arrays in lock-step with the enum
};

inline constexpr std::size_t kUiRoleCount = static_cast<std::size_t>(UiRole::kCount);

enum class ColorMode {
  kAuto,
  kOn,
  kOff,
};

enum class UnicodeMode {
  kAuto,
  kOn,
  kOff,
};

// Basic 8-color SGR only (foreground 30..37, background 40..47); -1 = terminal
// default. No 256-color to stay portable across minimal terminals.
struct TerminalStyle {
  bool bold = false;
  bool dim = false;
  bool reverse = false;
  std::int8_t foreground = -1;
  std::int8_t background = -1;
};

struct UiTheme {
  std::string_view name;
  std::array<TerminalStyle, kUiRoleCount> roles;
};

class UiStyle {
 public:
  UiStyle();

  // Environment probes are injected (keeps the class pure/testable): a
  // default-constructed UiStyle assumes no TTY and no UTF-8, so it emits
  // nothing until told otherwise.
  void set_terminal_is_tty(bool is_tty);
  void set_terminal_utf8(bool utf8);

  void set_color_mode(ColorMode mode);
  void set_unicode_mode(UnicodeMode mode);
  ColorMode color_mode() const;
  UnicodeMode unicode_mode() const;

  // Unknown name: returns false and leaves the current theme unchanged.
  bool set_theme(std::string_view name);
  std::string_view theme_name() const;
  static std::vector<std::string> theme_names();

  bool colors_enabled() const;
  bool unicode_enabled() const;

  // Wraps `text` in the role's SGR when colors are enabled and the role
  // resolves to any attribute; otherwise returns `text` unchanged.
  std::string apply(UiRole role, std::string_view text) const;

 private:
  const UiTheme* m_theme;
  ColorMode m_color_mode = ColorMode::kAuto;
  UnicodeMode m_unicode_mode = UnicodeMode::kAuto;
  bool m_is_tty = false;
  bool m_utf8 = false;
};

}  // namespace arrangrr::host
