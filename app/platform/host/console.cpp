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
  // Scroll region = output pane; park the cursor inside it.
  write_raw("\x1b[2J");  // clear screen once
  char buf[32];
  std::snprintf(buf, sizeof(buf), "\x1b[1;%dr", m_rows - 2);
  write_raw(buf);
  std::snprintf(buf, sizeof(buf), "\x1b[%d;1H", m_rows - 2);
  write_raw(buf);
  set_status("arrangrr");
  return true;
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
  std::snprintf(buf, sizeof(buf), "\x1b[%d;1H", m_rows - 2);
  out += buf;
  out += "\n";
  out += line;
  write_raw(out);
}

void Console::set_status(const std::string& text) {
  m_status = text;
  if (!m_active) {
    return;
  }
  refresh_geometry();
  std::string out;
  char buf[32];
  std::snprintf(buf, sizeof(buf), "\x1b[%d;1H", m_rows - 1);
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
  std::snprintf(buf, sizeof(buf), "\x1b[%d;1H\x1b[2K", m_rows);
  out += buf;
  out += kPrompt;
  out += ed.buffer();
  const std::size_t col = std::string(kPrompt).size() + ed.cursor() + 1;
  std::snprintf(buf, sizeof(buf), "\x1b[%d;%zuH", m_rows, col);
  out += buf;
  write_raw(out);
}

}  // namespace arrangrr::host
