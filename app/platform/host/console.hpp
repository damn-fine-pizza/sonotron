#pragma once

#include <cstdint>
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
  bool init();      // enters raw mode + scroll region; false if not a tty
  void shutdown();  // restores everything (idempotent)
  ~Console() { shutdown(); }

  void emit(const std::string& line);        // print into the output pane
  void set_status(const std::string& text);  // repaint the status bar
  void render_input(const LineEditor& ed);   // repaint prompt + buffer + cursor

 private:
  void refresh_geometry();
  void write_raw(const std::string& s);

  bool m_active = false;
  int m_rows = 24;
  int m_cols = 80;
  std::string m_status;
};

}  // namespace arrangrr::host
