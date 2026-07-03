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
  if (esc_ == Esc::kEsc) {
    esc_ = byte == '[' ? Esc::kCsi : Esc::kNone;
    csi_.clear();
    return r;
  }
  if (esc_ == Esc::kCsi) {
    if (byte >= '0' && byte <= '9') {
      csi_.push_back(static_cast<char>(byte));
      return r;
    }
    esc_ = Esc::kNone;
    r.dirty = true;
    switch (byte) {
      case 'D':  // left
        if (cursor_ > 0) --cursor_;
        break;
      case 'C':  // right
        if (cursor_ < buffer_.size()) ++cursor_;
        break;
      case 'A':  // up: older history
        if (history_pos_ > 0) {
          if (history_pos_ == history_.size()) stash_ = buffer_;
          history_load(history_pos_ - 1);
        }
        break;
      case 'B':  // down: newer history / back to the fresh line
        if (history_pos_ < history_.size()) {
          if (history_pos_ + 1 == history_.size()) {
            history_pos_ = history_.size();
            buffer_ = stash_;
            cursor_ = buffer_.size();
          } else {
            history_load(history_pos_ + 1);
          }
        }
        break;
      case 'H':  // home
        cursor_ = 0;
        break;
      case 'F':  // end
        cursor_ = buffer_.size();
        break;
      case '~':  // vt sequences: 1~ home, 3~ delete, 4~ end
        if (csi_ == "3" && cursor_ < buffer_.size()) buffer_.erase(cursor_, 1);
        if (csi_ == "1") cursor_ = 0;
        if (csi_ == "4") cursor_ = buffer_.size();
        break;
      default:
        r.dirty = false;
        break;
    }
    return r;
  }

  switch (byte) {
    case 0x1B:
      esc_ = Esc::kEsc;
      return r;
    case '\r':
    case '\n':
      r.line = buffer_;
      if (!buffer_.empty()) history_.push_back(buffer_);
      buffer_.clear();
      cursor_ = 0;
      history_pos_ = history_.size();
      stash_.clear();
      r.dirty = true;
      return r;
    case 0x7F:  // backspace
    case 0x08:
      if (cursor_ > 0) {
        buffer_.erase(cursor_ - 1, 1);
        --cursor_;
        r.dirty = true;
      }
      return r;
    case 0x03:  // Ctrl-C: clear the line; on an empty line, quit
      if (buffer_.empty()) {
        r.quit = true;
      } else {
        buffer_.clear();
        cursor_ = 0;
        r.dirty = true;
      }
      return r;
    case 0x04:  // Ctrl-D: quit on an empty line
      if (buffer_.empty()) r.quit = true;
      return r;
    case 0x15:  // Ctrl-U: clear
      buffer_.clear();
      cursor_ = 0;
      r.dirty = true;
      return r;
    default:
      if (byte >= 0x20) {  // printable (UTF-8 continuation bytes included)
        buffer_.insert(cursor_, 1, static_cast<char>(byte));
        ++cursor_;
        r.dirty = true;
      }
      return r;
  }
}

void LineEditor::history_load(std::size_t index) {
  history_pos_ = index;
  buffer_ = history_[index];
  cursor_ = buffer_.size();
}

// --- Console -----------------------------------------------------------------

bool Console::init() {
  if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) return false;
  if (tcgetattr(STDIN_FILENO, &g_saved_termios) != 0) return false;
  g_termios_saved = true;
  termios raw = g_saved_termios;
  raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO | ISIG));
  raw.c_iflag &= static_cast<tcflag_t>(~(IXON | ICRNL));
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) return false;

  refresh_geometry();
  active_ = true;
  // Scroll region = output pane; park the cursor inside it.
  write_raw("\x1b[2J");  // clear screen once
  char buf[32];
  std::snprintf(buf, sizeof(buf), "\x1b[1;%dr", rows_ - 2);
  write_raw(buf);
  std::snprintf(buf, sizeof(buf), "\x1b[%d;1H", rows_ - 2);
  write_raw(buf);
  set_status("arrangrr");
  return true;
}

void Console::shutdown() {
  if (!active_) return;
  active_ = false;
  write_raw("\x1b[r");  // reset scroll region
  char buf[16];
  std::snprintf(buf, sizeof(buf), "\x1b[%d;1H\n", rows_);
  write_raw(buf);
  if (g_termios_saved) tcsetattr(STDIN_FILENO, TCSANOW, &g_saved_termios);
}

void Console::refresh_geometry() {
  winsize ws{};
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 4 && ws.ws_col > 10) {
    rows_ = ws.ws_row;
    cols_ = ws.ws_col;
  }
}

void Console::write_raw(const std::string& s) {
  (void)!write(STDOUT_FILENO, s.data(), s.size());
}

void Console::emit(const std::string& line) {
  if (!active_) {
    std::puts(line.c_str());
    return;
  }
  // Print at the bottom of the scroll region (scrolls the pane), then leave
  // the cursor there; the input line is repainted by render_input.
  std::string out;
  char buf[32];
  std::snprintf(buf, sizeof(buf), "\x1b[%d;1H", rows_ - 2);
  out += buf;
  out += "\n";
  out += line;
  write_raw(out);
}

void Console::set_status(const std::string& text) {
  status_ = text;
  if (!active_) return;
  refresh_geometry();
  std::string out;
  char buf[32];
  std::snprintf(buf, sizeof(buf), "\x1b[%d;1H", rows_ - 1);
  out += buf;
  out += "\x1b[7m";  // inverse video
  std::string padded = " " + status_;
  padded.resize(static_cast<std::size_t>(cols_), ' ');
  out += padded;
  out += "\x1b[0m";
  write_raw(out);
}

void Console::render_input(const LineEditor& ed) {
  if (!active_) return;
  std::string out;
  char buf[32];
  std::snprintf(buf, sizeof(buf), "\x1b[%d;1H\x1b[2K", rows_);
  out += buf;
  out += kPrompt;
  out += ed.buffer();
  const std::size_t col = std::string(kPrompt).size() + ed.cursor() + 1;
  std::snprintf(buf, sizeof(buf), "\x1b[%d;%zuH", rows_, col);
  out += buf;
  write_raw(out);
}

}  // namespace arrangrr::host
