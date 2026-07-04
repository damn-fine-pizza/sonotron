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

// Terminal renderer. Layout, top to bottom over H rows:
//   [ panel grid: rows 1..H-2 ] [ status bar: H-1 ] [ input line: H ]
// The whole area above the status bar is one uniform panel grid, composed by
// PanelManager and painted verbatim here (no scroll region, no bands): the
// events/console SCROLLING now lives in PanelManager's per-panel backlogs. The
// status bar (inverse video) and the persistent input line anchor the bottom.
class Console {
 public:
  static constexpr int kStatusInputRows = 2;  // status bar + input line

  Console();
  ~Console();

  Console(const Console&) = delete;
  Console& operator=(const Console&) = delete;

  bool init();      // enters raw mode; false if not a tty
  void shutdown();  // restores everything (idempotent)

  void set_status(const std::string& text);  // repaint the status bar (and
                                             // re-layout after a resize)
  void render_input(const LineEditor& ed);   // repaint prompt + buffer + cursor

  // Paints the composed panel grid over rows 1..H-2. `lines` is expected to be
  // exactly panel_rows() long (PanelManager fits it to width + height); shorter
  // input clears the remaining rows, longer input is clamped.
  void set_panel(const std::vector<std::string>& lines);

  // Rows available to the panel grid (everything above the status/input pair).
  int panel_rows() const { return m_rows > kStatusInputRows ? m_rows - kStatusInputRows : 0; }

  // Current terminal width; panel renderers regenerate content from state
  // through the resize hook, which fires after every geometry change.
  int columns() const { return m_cols; }
  void set_resize_hook(std::function<void()> hook) { m_resize_hook = std::move(hook); }

 private:
  void refresh_geometry();
  int status_row() const { return m_rows - 1; }
  int input_row() const { return m_rows; }
  void write_raw(const std::string& s);
  void paint_status();

  bool m_active = false;
  int m_rows = 24;
  int m_cols = 80;
  std::string m_status;
  std::function<void()> m_resize_hook;
};

}  // namespace arrangrr::host
