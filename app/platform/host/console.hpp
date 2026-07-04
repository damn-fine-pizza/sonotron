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
//   [ events scroll region ] [ panels ] [ console pane ] [ status ] [ input ]
// The events stream (MIDI) scrolls in the DECSTBM region at the top; panels
// and the console pane are repainted bands (never terminal-scrolled); the
// status bar (inverse video) and the persistent input line anchor the bottom.
class Console {
 public:
  // Row budget below the events region, from the bottom up.
  static constexpr int kStatusInputRows = 2;                 // status bar + input line
  static constexpr int kConsoleRows = 6;                     // console ring capacity (log lines)
  static constexpr int kConsolePaneRows = kConsoleRows + 1;  // + one rule/label row
  static constexpr int kMinEventRows = 1;                    // events region never fully starved
  static constexpr int kPanelCapNum = 2;                     // panels/console pane <= 40% of H
  static constexpr int kPanelCapDen = 5;

  // Row heights of the four stacked bands above the input line. events + panel
  // + console + kStatusInputRows always sums to the terminal height, so the
  // bands tile the screen without overlap.
  struct Bands {
    int events = 0;   // top scroll region (rows 1..events)
    int panel = 0;    // panel band
    int console = 0;  // console pane band (rule + log lines)
  };
  // Pure geometry: split `rows` into bands given the desired panel height.
  // Static + side-effect-free so it is unit-testable without a terminal.
  static Bands compute_bands(int rows, int panel_lines);

  Console();
  ~Console();

  Console(const Console&) = delete;
  Console& operator=(const Console&) = delete;

  bool init();      // enters raw mode + scroll region; false if not a tty
  void shutdown();  // restores everything (idempotent)

  void emit(const std::string& line);          // into the EVENTS stream (top)
  void console_line(const std::string& line);  // into the CONSOLE pane (bottom)
  void set_status(const std::string& text);    // repaint the status bar (and
                                               // re-layout after a resize)
  void render_input(const LineEditor& ed);     // repaint prompt + buffer + cursor

  // Persistent panel between the events pane and the console pane (help lives
  // here). Empty vector hides it. The events pane always keeps the majority of
  // the screen: overlong panels are truncated to <= 40%.
  void set_panel(const std::vector<std::string>& lines);

  // Current terminal width; panel renderers regenerate content from state
  // through the resize hook, which fires after every geometry change.
  int columns() const { return m_cols; }
  void set_resize_hook(std::function<void()> hook) { m_resize_hook = std::move(hook); }

  // Console ring inspection (tests only).
  const std::vector<std::string>& console_ring() const { return m_console; }

 private:
  void refresh_geometry();
  void apply_layout();     // scroll region + panel + console + status repaint
  void render_console();   // repaint the console pane band in place
  int log_bottom() const;  // bottom row of the events scroll region
  int status_row() const { return m_rows - 1; }
  int input_row() const { return m_rows; }
  void write_raw(const std::string& s);

  bool m_active = false;
  int m_rows = 24;
  int m_cols = 80;
  std::string m_status;
  std::vector<std::string> m_panel;
  std::vector<std::string> m_console;  // bounded command-I/O ring (<= kConsoleRows)
  int m_events_bottom = 0;             // events-region bottom last drawn — clears ghosts on shrink
  std::function<void()> m_resize_hook;
};

}  // namespace arrangrr::host
