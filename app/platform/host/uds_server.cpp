#include "uds_server.hpp"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>

// D38 GUI transport: see uds_server.hpp for the design summary. This
// translation unit owns every POSIX socket call the adapter makes; main.cpp
// never touches a socket fd directly beyond the numbers this class hands it.

namespace arrangrr::host {

namespace {

// One read()/recv() chunk size. L1 command lines and JSONL event lines are
// both short, so this is generous headroom rather than a real limit.
constexpr std::size_t kReadBufferSize = 4096;

// Pending TCP-style backlog for the listening socket. A handful of GUI
// clients is the entire expected load; this is not a high-throughput server.
constexpr int kListenBacklog = 16;

bool set_nonblocking(int fd) {
  const int flags = ::fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }
  return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}

// Minimal JSON string escaping for values we did not generate ourselves
// (an error message, or the raw command line that failed) — jsonl.cpp never
// needed this because every string it formats is host-controlled, but error
// text here can contain quotes/backslashes/control bytes verbatim.
std::string json_escape(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (const char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
          out += buf;
        } else {
          out += c;
        }
        break;
    }
  }
  return out;
}

}  // namespace

bool LineBuffer::feed(const char* data, std::size_t len, std::vector<std::string>& out_lines) {
  for (std::size_t i = 0; i < len; ++i) {
    const char c = data[i];
    if (c == '\n') {
      if (!m_pending.empty() && m_pending.back() == '\r') {
        m_pending.pop_back();
      }
      out_lines.push_back(std::move(m_pending));
      m_pending.clear();
    } else {
      m_pending.push_back(c);
    }
  }
  return m_pending.size() <= kMaxLineLength;
}

UdsServer::~UdsServer() {
  for (const int fd : m_client_order) {
    ::close(fd);
  }
  if (m_listen_fd >= 0) {
    ::close(m_listen_fd);
  }
  if (!m_path.empty()) {
    ::unlink(m_path.c_str());
  }
}

bool UdsServer::start(const std::string& path) {
  m_path = path;
  ::unlink(path.c_str());  // drop a stale socket file from a previous run

  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    std::fprintf(stderr, "control socket: cannot create (%s)\n", std::strerror(errno));
    return false;
  }

  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  if (path.size() >= sizeof(addr.sun_path)) {
    std::fprintf(stderr, "control socket: path too long: %s\n", path.c_str());
    ::close(fd);
    return false;
  }
  std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

  if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    std::fprintf(stderr, "control socket: cannot bind %s (%s)\n", path.c_str(),
                 std::strerror(errno));
    ::close(fd);
    return false;
  }
  if (::listen(fd, kListenBacklog) < 0) {
    std::fprintf(stderr, "control socket: cannot listen on %s (%s)\n", path.c_str(),
                 std::strerror(errno));
    ::close(fd);
    ::unlink(path.c_str());
    return false;
  }
  if (!set_nonblocking(fd)) {
    std::fprintf(stderr, "control socket: cannot set non-blocking (%s)\n", std::strerror(errno));
    ::close(fd);
    ::unlink(path.c_str());
    return false;
  }

  m_listen_fd = fd;
  return true;
}

void UdsServer::handle_listen_readable() {
  if (m_listen_fd < 0) {
    return;
  }
  for (;;) {
    const int fd = ::accept(m_listen_fd, nullptr, nullptr);
    if (fd < 0) {
      break;  // EAGAIN/EWOULDBLOCK: no more pending connections this round
    }
    if (!set_nonblocking(fd)) {
      ::close(fd);
      continue;
    }
    m_client_order.push_back(fd);
    m_buffers.emplace(fd, LineBuffer{});
  }
}

void UdsServer::handle_client_readable(int fd) {
  char buf[kReadBufferSize];
  for (;;) {
    const ssize_t n = ::read(fd, buf, sizeof(buf));
    if (n > 0) {
      auto it = m_buffers.find(fd);
      if (it == m_buffers.end()) {
        return;  // not a tracked client (should not happen)
      }
      std::vector<std::string> lines;
      const bool ok = it->second.feed(buf, static_cast<std::size_t>(n), lines);
      for (const std::string& line : lines) {
        if (m_on_line) {
          m_on_line(fd, line);
        }
      }
      if (!ok) {
        close_client(fd);  // unterminated line grew past the cap: drop it
        return;
      }
      continue;  // there may be more buffered this round
    }
    if (n == 0) {
      close_client(fd);  // EOF
      return;
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;  // drained for now
    }
    close_client(fd);  // a real read error
    return;
  }
}

void UdsServer::close_client(int fd) {
  ::close(fd);
  m_buffers.erase(fd);
  m_client_order.erase(std::remove(m_client_order.begin(), m_client_order.end(), fd),
                       m_client_order.end());
}

void UdsServer::write_line(int fd, const std::string& line) {
  std::string out = line;
  out.push_back('\n');
  std::size_t sent = 0;
  while (sent < out.size()) {
    const ssize_t n = ::send(fd, out.data() + sent, out.size() - sent, MSG_NOSIGNAL);
    if (n > 0) {
      sent += static_cast<std::size_t>(n);
      continue;
    }
    if (n < 0 && errno == EINTR) {
      continue;
    }
    // EAGAIN (the client's kernel receive buffer is full — a slow reader),
    // EPIPE/ECONNRESET (the peer is gone), or anything else: this class
    // never blocks and never lets a misbehaving client take the host down,
    // so the connection is simply dropped. Broadcast is therefore
    // best-effort — a GUI client slower than the event stream loses events
    // rather than stalling MIDI output for everyone else.
    close_client(fd);
    return;
  }
}

void UdsServer::broadcast(const std::string& line) {
  // Snapshot first: write_line() can call close_client(), which mutates
  // m_client_order while we would otherwise be iterating it.
  const std::vector<int> targets = m_client_order;
  for (const int fd : targets) {
    write_line(fd, line);
  }
}

void UdsServer::send_error(int client_fd, const std::string& message, const std::string& cmd) {
  const std::string json =
      R"({"error":")" + json_escape(message) + R"(","cmd":")" + json_escape(cmd) + R"("})";
  write_line(client_fd, json);
}

}  // namespace arrangrr::host
