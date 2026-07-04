#include "console.hpp"

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <cstdio>

namespace arrangrr::host {

namespace {
constexpr const char* kPrompt = "arrangrr> ";
termios g_saved_termios;
bool g_termios_saved = false;
}  // namespace

// --- LineEditor --------------------------------------------------------------

LineEditor::Result LineEditor::feed(std::uint8_t byte) {
  Result r;

  // Escape-sequence state machine (arrows, home/end, delete).
  if (m_esc == Esc::kEsc) {
    m_esc = byte == '[' ? Esc::kCsi : Esc::kNone;
    m_csi.clear();
    return r;
  }
  if (m_esc == Esc::kCsi) {
    if (byte >= '0' && byte <= '9') {
      m_csi.push_back(static_cast<char>(byte));
      return r;
    }
    m_esc = Esc::kNone;
    r.dirty = true;
    switch (byte) {
      case 'D':  // left
        if (m_cursor > 0) {
          --m_cursor;
        }
        break;
      case 'C':  // right
        if (m_cursor < m_buffer.size()) {
          ++m_cursor;
        }
        break;
      case 'A':  // up: older history
        if (m_history_pos > 0) {
          if (m_history_pos == m_history.size()) {
            m_stash = m_buffer;
          }
          history_load(m_history_pos - 1);
        }
        break;
      case 'B':  // down: newer history / back to the fresh line
        if (m_history_pos < m_history.size()) {
          if (m_history_pos + 1 == m_history.size()) {
            m_history_pos = m_history.size();
            m_buffer = m_stash;
            m_cursor = m_buffer.size();
          } else {
            history_load(m_history_pos + 1);
          }
        }
        break;
      case 'H':  // home
        m_cursor = 0;
        break;
      case 'F':  // end
        m_cursor = m_buffer.size();
        break;
      case '~':  // vt sequences: 1~ home, 3~ delete, 4~ end
        if (m_csi == "3" && m_cursor < m_buffer.size()) {
          m_buffer.erase(m_cursor, 1);
        }
        if (m_csi == "1") {
          m_cursor = 0;
        }
        if (m_csi == "4") {
          m_cursor = m_buffer.size();
        }
        break;
      default:
        r.dirty = false;
        break;
    }
    return r;
  }

  switch (byte) {
    case 0x1B:
      m_esc = Esc::kEsc;
      return r;
    case '\r':
    case '\n':
      r.line = m_buffer;
      if (!m_buffer.empty()) {
        m_history.push_back(m_buffer);
      }
      m_buffer.clear();
      m_cursor = 0;
      m_history_pos = m_history.size();
      m_stash.clear();
      r.dirty = true;
      return r;
    case 0x7F:  // backspace
    case 0x08:
      if (m_cursor > 0) {
        m_buffer.erase(m_cursor - 1, 1);
        --m_cursor;
        r.dirty = true;
      }
      return r;
    case 0x03:  // Ctrl-C: clear the line; on an empty line, quit
      if (m_buffer.empty()) {
        r.quit = true;
      } else {
        m_buffer.clear();
        m_cursor = 0;
        r.dirty = true;
      }
      return r;
    case 0x04:  // Ctrl-D: quit on an empty line
      if (m_buffer.empty()) {
        r.quit = true;
      }
      return r;
    case 0x15:  // Ctrl-U: clear
      m_buffer.clear();
      m_cursor = 0;
      r.dirty = true;
      return r;
    default:
      if (byte >= 0x20) {  // printable (UTF-8 continuation bytes included)
        m_buffer.insert(m_cursor, 1, static_cast<char>(byte));
        ++m_cursor;
        r.dirty = true;
      }
      return r;
  }
}

void LineEditor::history_load(std::size_t index) {
  m_history_pos = index;
  m_buffer = m_history[index];
  m_cursor = m_buffer.size();
}

// --- Console -----------------------------------------------------------------

Console::Console() = default;

Console::~Console() { shutdown(); }

bool Console::init() {
  if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
    return false;
  }
  if (tcgetattr(STDIN_FILENO, &g_saved_termios) != 0) {
    return false;
  }
  g_termios_saved = true;
  termios raw = g_saved_termios;
  raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO | ISIG));
  raw.c_iflag &= static_cast<tcflag_t>(~(IXON | ICRNL));
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
    return false;
  }

  refresh_geometry();
  m_active = true;
  write_raw("\x1b[2J");  // clear screen once
  m_status = "arrangrr";
  apply_layout();
  return true;
}

Console::Bands Console::compute_bands(int rows, int panel_lines) {
  Bands b;
  const int avail = rows - kStatusInputRows;             // rows above status + input
  const int cap = (rows * kPanelCapNum) / kPanelCapDen;  // 40% ceiling

  // Panels keep the events region the majority owner: <= 40% of the screen.
  int panel = panel_lines < 0 ? 0 : panel_lines;
  if (panel > cap) {
    panel = cap;
  }

  // The console pane wants kConsolePaneRows but obeys the same 40% ceiling and,
  // on a very short terminal, shrinks further so the events region survives —
  // status/input are never sacrificed (they live in kStatusInputRows).
  int console = kConsolePaneRows;
  if (console > cap) {
    console = cap;
  }
  if (console > avail - panel - kMinEventRows) {
    console = avail - panel - kMinEventRows;
  }
  if (console < 0) {
    console = 0;
  }

  b.panel = panel;
  b.console = console;
  b.events = avail - panel - console;  // remainder; >= kMinEventRows by construction
  return b;
}

int Console::log_bottom() const {
  return compute_bands(m_rows, static_cast<int>(m_panel.size())).events;
}

void Console::render_console() {
  if (!m_active) {
    return;
  }
  const Bands b = compute_bands(m_rows, static_cast<int>(m_panel.size()));
  if (b.console <= 0) {
    return;
  }
  char buf[32];
  const int top = b.events + b.panel + 1;  // first console-pane row (the rule)

  // Rule/label row: a dim ASCII rule that sets the pane apart from both the
  // live events stream above and the panels — "-- console " then dashes.
  std::string out;
  std::snprintf(buf, sizeof(buf), "\x1b[%d;1H\x1b[2K", top);
  out += buf;
  std::string rule = "-- console ";
  rule.resize(static_cast<std::size_t>(m_cols), '-');
  out += "\x1b[2m";
  out += rule;
  out += "\x1b[0m";

  // Log rows: oldest at the top, newest just above the status bar. When the
  // ring holds fewer lines than there are rows, blank rows pad the top.
  const int log_rows = b.console - 1;  // the rule takes one row
  const int filled = static_cast<int>(m_console.size());
  const int blank = log_rows - filled;  // >0 pads the top; <0 drops the oldest
  for (int i = 0; i < log_rows; ++i) {
    std::snprintf(buf, sizeof(buf), "\x1b[%d;1H\x1b[2K", top + 1 + i);
    out += buf;
    const int idx = i - blank;
    if (idx >= 0 && idx < filled) {
      std::string line = m_console[static_cast<std::size_t>(idx)];
      if (static_cast<int>(line.size()) > m_cols) {
        line.resize(static_cast<std::size_t>(m_cols));
      }
      out += line;
    }
  }
  write_raw(out);
}

void Console::apply_layout() {
  if (!m_active) {
    return;
  }
  char buf[32];
  const Bands b = compute_bands(m_rows, static_cast<int>(m_panel.size()));
  const int bottom = b.events;  // scroll region 1..bottom

  // Rows that just moved out of the panel/console bands back into the (grown)
  // events region still hold stale bytes: clear them or they ghost. Only rows
  // below the OLD events boundary are touched, so emitted log lines — which
  // live at rows <= the old boundary — are never wiped.
  std::string vacate;
  for (int row = m_events_bottom + 1; row <= bottom; ++row) {
    std::snprintf(buf, sizeof(buf), "\x1b[%d;1H\x1b[2K", row);
    vacate += buf;
  }
  if (!vacate.empty()) {
    write_raw(vacate);
  }
  m_events_bottom = bottom;

  // Scroll region = events pane only.
  std::snprintf(buf, sizeof(buf), "\x1b[1;%dr", bottom);
  write_raw(buf);
  // Panel rows (each cleared, truncated to the pane width).
  std::string out;
  for (int i = 0; i < b.panel; ++i) {
    std::snprintf(buf, sizeof(buf), "\x1b[%d;1H\x1b[2K", bottom + 1 + i);
    out += buf;
    std::string line =
        i < static_cast<int>(m_panel.size()) ? m_panel[static_cast<std::size_t>(i)] : "";
    if (static_cast<int>(line.size()) > m_cols) {
      line.resize(static_cast<std::size_t>(m_cols));
    }
    out += "\x1b[2m";  // dim: visually separate from the live events stream
    out += line;
    out += "\x1b[0m";
  }
  write_raw(out);

  render_console();
  set_status(m_status);
}

void Console::set_panel(const std::vector<std::string>& lines) {
  m_panel = lines;
  apply_layout();
}

void Console::shutdown() {
  if (!m_active) {
    return;
  }
  m_active = false;
  write_raw("\x1b[r");  // reset scroll region
  char buf[16];
  std::snprintf(buf, sizeof(buf), "\x1b[%d;1H\n", m_rows);
  write_raw(buf);
  if (g_termios_saved) {
    tcsetattr(STDIN_FILENO, TCSANOW, &g_saved_termios);
  }
}

void Console::refresh_geometry() {
  winsize ws{};
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 4 && ws.ws_col > 10) {
    m_rows = ws.ws_row;
    m_cols = ws.ws_col;
  }
}

void Console::write_raw(const std::string& s) { (void)!write(STDOUT_FILENO, s.data(), s.size()); }

void Console::emit(const std::string& line) {
  if (!m_active) {
    std::puts(line.c_str());
    return;
  }
  // Print at the bottom of the scroll region (scrolls the pane), then leave
  // the cursor there; the input line is repainted by render_input.
  std::string out;
  char buf[32];
  std::snprintf(buf, sizeof(buf), "\x1b[%d;1H", log_bottom());
  out += buf;
  out += "\n";
  out += line;
  write_raw(out);
}

void Console::console_line(const std::string& line) {
  // Newest line nearest the input: append, then drop the oldest past capacity.
  m_console.push_back(line);
  while (m_console.size() > static_cast<std::size_t>(kConsoleRows)) {
    m_console.erase(m_console.begin());
  }
  render_console();  // repaint the pane in place (no terminal scroll)
}

void Console::set_status(const std::string& text) {
  m_status = text;
  if (!m_active) {
    return;
  }
  const int prev_rows = m_rows;
  const int prev_cols = m_cols;
  refresh_geometry();
  if (m_rows != prev_rows || m_cols != prev_cols) {
    apply_layout();  // terminal resized: rebuild regions and repaint

    // Panel content is regenerated from state at the new geometry (H1
    // resize contract): the hook re-renders and re-pushes the panels.
    if (m_resize_hook) {
      m_resize_hook();
    }
    return;
  }
  std::string out;
  char buf[32];
  std::snprintf(buf, sizeof(buf), "\x1b[%d;1H", status_row());
  out += buf;
  out += "\x1b[7m";  // inverse video
  std::string padded = " " + m_status;
  padded.resize(static_cast<std::size_t>(m_cols), ' ');
  out += padded;
  out += "\x1b[0m";
  write_raw(out);
}

void Console::render_input(const LineEditor& ed) {
  if (!m_active) {
    return;
  }
  std::string out;
  char buf[32];
  std::snprintf(buf, sizeof(buf), "\x1b[%d;1H\x1b[2K", input_row());
  out += buf;
  out += kPrompt;
  out += ed.buffer();
  const std::size_t col = std::string(kPrompt).size() + ed.cursor() + 1;
  std::snprintf(buf, sizeof(buf), "\x1b[%d;%zuH", input_row(), col);
  out += buf;
  write_raw(out);
}

}  // namespace arrangrr::host
