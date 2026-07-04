#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

// Claude-CLI-style console for live mode: an output pane that scrolls above,
// a status bar, and a persistent editable input line at the bottom. Built on
// plain ANSI escapes + termios — no dependencies (D4). Only engaged when
// stdin AND stdout are a terminal; scripts and pipes keep flat output.

namespace arrangrr::host {

// Pure line editor: bytes in, edited line + history out. No tty knowledge —
// unit-testable. Understands arrows (left/right = cursor, up/down = history),
// backspace, delete, home/end, Ctrl-U (clear), Ctrl-C (clear line; on an
// empty line -> quit), Ctrl-D (quit on empty line).
class LineEditor {
 public:
  struct Result {
    std::optional<std::string> line;  // set when Enter completed a line
    bool quit = false;                // Ctrl-D / Ctrl-C on an empty line
    bool dirty = false;               // input display needs a redraw
  };

  Result feed(std::uint8_t byte);

  const std::string& buffer() const { return m_buffer; }
  std::size_t cursor() const { return m_cursor; }

 private:
  void history_load(std::size_t index);

  std::string m_buffer;
  std::size_t m_cursor = 0;
  std::vector<std::string> m_history;
  std::size_t m_history_pos = 0;  // == history_.size() when editing a fresh line
  std::string m_stash;            // fresh line saved while browsing history
  enum class Esc { kNone, kEsc, kCsi } m_esc = Esc::kNone;
  std::string m_csi;
};

// Terminal renderer. Layout (H rows): rows 1..H-2 scroll region (output
// pane), row H-1 status bar (inverse video), row H input line.
class Console {
 public:
  Console();
  ~Console();

  Console(const Console&) = delete;
  Console& operator=(const Console&) = delete;

  bool init();      // enters raw mode + scroll region; false if not a tty
  void shutdown();  // restores everything (idempotent)

  void emit(const std::string& line);        // print into the output pane
  void set_status(const std::string& text);  // repaint the status bar (and
                                             // re-layout after a resize)
  void render_input(const LineEditor& ed);   // repaint prompt + buffer + cursor

  // Persistent panel between the log pane and the status bar (help lives
  // here). Empty vector hides it. The log pane always keeps >= ~60% of the
  // screen: overlong panels are truncated.
  void set_panel(const std::vector<std::string>& lines);

  // Current terminal width; panel renderers regenerate content from state
  // through the resize hook, which fires after every geometry change.
  int columns() const { return m_cols; }
  void set_resize_hook(std::function<void()> hook) { m_resize_hook = std::move(hook); }

 private:
  void refresh_geometry();
  void apply_layout();  // scroll region + panel + status repaint
  int log_bottom() const;
  int status_row() const { return m_rows - 1; }
  int input_row() const { return m_rows; }
  void write_raw(const std::string& s);

  bool m_active = false;
  int m_rows = 24;
  int m_cols = 80;
  std::string m_status;
  std::vector<std::string> m_panel;
  int m_panel_rows = 0;  // panel height last drawn — clears ghosts when it shrinks
  std::function<void()> m_resize_hook;
};

}  // namespace arrangrr::host
