#include "console.hpp"

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <cstdio>

#include "ui_style.hpp"

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
  paint_status();
  return true;
}

void Console::set_panel(const std::vector<std::string>& lines) {
  if (!m_active) {
    return;
  }
  const int pr = panel_rows();
  std::string out;
  char buf[32];
  // Paint the grid over rows 1..H-2. Each row is cleared first; PanelManager
  // already fitted the lines to the column width (styles included), so we write
  // them verbatim and only guard the width as a safety net.
  for (int r = 0; r < pr; ++r) {
    std::snprintf(buf, sizeof(buf), "\x1b[%d;1H\x1b[2K", r + 1);
    out += buf;
    if (r < static_cast<int>(lines.size())) {
      out += ansi::visible_truncate(lines[static_cast<std::size_t>(r)],
                                    static_cast<std::size_t>(m_cols));
    }
  }
  write_raw(out);
}

void Console::shutdown() {
  if (!m_active) {
    return;
  }
  m_active = false;
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

void Console::paint_status() {
  if (!m_active) {
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

void Console::set_status(const std::string& text) {
  m_status = text;
  if (!m_active) {
    return;
  }
  const int prev_rows = m_rows;
  const int prev_cols = m_cols;
  refresh_geometry();
  if (m_rows != prev_rows || m_cols != prev_cols) {
    // Terminal resized: clear and let the hook re-render + re-push the grid from
    // state at the new geometry (resize contract), then repaint the status bar.
    write_raw("\x1b[2J");
    if (m_resize_hook) {
      m_resize_hook();
    }
    paint_status();
    return;
  }
  paint_status();
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
