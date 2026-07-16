// _GNU_SOURCE exposes the Linux MSG_DONTWAIT / MSG_NOSIGNAL recv()/send()
// flags, which -std=c++26 (strict ISO) would otherwise hide. Must precede
// any system header.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "uds_client.hpp"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstring>

namespace arrangrr::host {

namespace {

constexpr std::size_t kReadChunk = 4096;

bool set_nonblocking(int fd) {
  const int flags = ::fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }
  return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}

}  // namespace

UdsClient::~UdsClient() { close_connection(); }

bool UdsClient::connect_to(const std::string& path, std::string& error) {
  close_connection();

  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    error = std::string("socket: ") + std::strerror(errno);
    return false;
  }

  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  if (path.size() >= sizeof(addr.sun_path)) {
    error = "socket path too long";
    ::close(fd);
    return false;
  }
  std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

  // Connect while still blocking: for AF_UNIX/SOCK_STREAM connect()
  // completes as soon as the server has the socket in its listen backlog,
  // so there is no EINPROGRESS dance to manage. Non-blocking is switched on
  // only afterwards, for the caller's own poll() loop.
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    error = std::string("connect: ") + std::strerror(errno);
    ::close(fd);
    return false;
  }
  if (!set_nonblocking(fd)) {
    error = std::string("fcntl: ") + std::strerror(errno);
    ::close(fd);
    return false;
  }

  m_fd = fd;
  m_buffer = LineBuffer{};
  return true;
}

bool UdsClient::send_line(const std::string& line) {
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
    if (n < 0 && errno == EINTR) {
      continue;
    }
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return false;  // kernel send buffer momentarily full; try again later
    }
    close_connection();
    return false;
  }
  return true;
}

void UdsClient::poll_lines(std::vector<std::string>& out_lines) {
  if (m_fd < 0) {
    return;
  }
  char buf[kReadChunk];
  while (true) {
    const ssize_t n = ::recv(m_fd, buf, sizeof(buf), MSG_DONTWAIT);
    if (n > 0) {
      if (!m_buffer.feed(buf, static_cast<std::size_t>(n), out_lines)) {
        close_connection();  // a peer line grew past the cap: drop it
        return;
      }
      continue;
    }
    if (n == 0) {
      close_connection();  // orderly EOF from the host
      return;
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;  // drained everything available this round
    }
    close_connection();
    return;
  }
}

void UdsClient::close_connection() {
  if (m_fd >= 0) {
    ::close(m_fd);  // just hang up; never send "quit" (shared host)
    m_fd = -1;
  }
}

}  // namespace arrangrr::host
