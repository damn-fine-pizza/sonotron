// _GNU_SOURCE exposes the Linux MSG_DONTWAIT / MSG_NOSIGNAL recv()/send()
// flags, which -std=c++26 (strict ISO) would otherwise hide. Must precede
// any system header.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "uds_brain_session.hpp"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstring>

namespace sonotron {

namespace {

constexpr std::size_t kReadChunk = 4096;

bool set_nonblocking(int fd) {
  const int flags = ::fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }
  return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}

// The shell (components/hostrt/shell.cpp::dispatch_ui) treats a first token
// of exactly "quit" or "exit" as "tear down the whole shared host, for every
// connected client". A GUI window-close or a stray Enter must never forward
// that verb over a socket other clients share, so we refuse it here, before
// it ever reaches send_line() -- independent of trailing tokens, since the
// shell itself only inspects the first token to decide.
bool is_blocked_command(std::string_view line) {
  std::size_t begin = 0;
  while (begin < line.size() && (line[begin] == ' ' || line[begin] == '\t')) {
    ++begin;
  }
  std::size_t end = begin;
  while (end < line.size() && line[end] != ' ' && line[end] != '\t') {
    ++end;
  }
  const std::string_view first_token = line.substr(begin, end - begin);
  return first_token == "quit" || first_token == "exit";
}

}  // namespace

UdsBrainSession::~UdsBrainSession() { close_fd(); }

bool UdsBrainSession::connect_to(const std::string& path) {
  close_fd();
  m_last_error.clear();
  m_status = Status::kConnecting;

  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    m_last_error = std::string("socket: ") + std::strerror(errno);
    m_status = Status::kError;
    return false;
  }

  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  if (path.size() >= sizeof(addr.sun_path)) {
    m_last_error = "socket path too long";
    ::close(fd);
    m_status = Status::kError;
    return false;
  }
  std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

  // Connect while still blocking: for AF_UNIX/SOCK_STREAM connect() completes
  // as soon as the server has the socket in its listen backlog, so there is
  // no EINPROGRESS dance to manage. We switch to non-blocking only
  // afterwards, for the per-frame recv() loop.
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    m_last_error = std::string("connect: ") + std::strerror(errno);
    ::close(fd);
    m_status = Status::kError;
    return false;
  }
  if (!set_nonblocking(fd)) {
    m_last_error = std::string("fcntl: ") + std::strerror(errno);
    ::close(fd);
    m_status = Status::kError;
    return false;
  }

  m_fd = fd;
  m_buffer = LineBuffer{};
  m_status = Status::kConnected;
  return true;
}

bool UdsBrainSession::send_line(const std::string& line) {
  if (m_fd < 0) {
    return false;
  }
  std::string wire = line;
  wire.push_back('\n');
  std::size_t sent = 0;
  while (sent < wire.size()) {
    const ssize_t n = ::send(m_fd, wire.data() + sent, wire.size() - sent, MSG_NOSIGNAL);
    if (n > 0) {
      sent += static_cast<std::size_t>(n);
      continue;
    }
    if (n < 0 && (errno == EINTR)) {
      continue;
    }
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      // Kernel send buffer momentarily full. For the tiny L1 command lines
      // the GUI emits this effectively never happens; treat it as a soft
      // failure for this frame rather than blocking the render thread.
      m_last_error = "send would block";
      return false;
    }
    m_last_error = std::string("send: ") + std::strerror(errno);
    close_fd();
    m_status = Status::kError;
    return false;
  }
  return true;
}

void UdsBrainSession::poll_lines(std::vector<std::string>& out_lines) {
  if (m_fd < 0) {
    return;
  }
  char buf[kReadChunk];
  while (true) {
    const ssize_t n = ::recv(m_fd, buf, sizeof(buf), MSG_DONTWAIT);
    if (n > 0) {
      if (!m_buffer.feed(buf, static_cast<std::size_t>(n), out_lines)) {
        m_last_error = "line too long";
        close_fd();  // a peer line grew past the cap: drop the connection
        m_status = Status::kError;
        return;
      }
      continue;
    }
    if (n == 0) {
      close_fd();  // orderly EOF from the host
      return;
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;  // drained everything available this frame
    }
    m_last_error = std::string("recv: ") + std::strerror(errno);
    close_fd();
    m_status = Status::kError;
    return;
  }
}

void UdsBrainSession::close_fd() {
  if (m_fd >= 0) {
    ::close(m_fd);  // just hang up; never send `quit` (shared host)
    m_fd = -1;
  }
  m_status = Status::kDisconnected;
}

void UdsBrainSession::send(std::string_view command_line) {
  if (is_blocked_command(command_line)) {
    return;
  }
  send_line(std::string(command_line));
}

void UdsBrainSession::poll(std::vector<BrainEvent>& out) {
  std::vector<std::string> lines;
  poll_lines(lines);
  for (const std::string& line : lines) {
    out.push_back(parse_brain_event(line));
  }
}

const BrainSnapshot& UdsBrainSession::snapshot() const { return m_snapshot; }

BrainSession::Status UdsBrainSession::status() const { return m_status; }

}  // namespace sonotron
