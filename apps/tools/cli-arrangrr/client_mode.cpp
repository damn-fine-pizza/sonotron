#include "client_mode.hpp"

#include <poll.h>
#include <unistd.h>

#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include "tui_client.hpp"
#include "uds_client.hpp"

// See client_mode.hpp for the design summary. NOT included here, ever:
// arrangrr/abi.hpp or anything under components/arrangrr -- this file only
// #includes hostrt's own arrangrr-free client seam (tui_client.hpp,
// uds_client.hpp) plus plain POSIX/stdlib headers.

namespace arrangrr::client {

namespace {

// Grace-period window started once stdin hits EOF (or a local "quit"), so
// the last few in-flight server replies (e.g. the echo of the very last
// gesture sent) still get folded and printed before exiting.
constexpr std::uint64_t kDrainGraceMs = 300;

std::uint64_t monotonic_ms() {
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<std::uint64_t>(ts.tv_sec) * 1000u +
         static_cast<std::uint64_t>(ts.tv_nsec) / 1'000'000u;
}

// A locally-handled pseudo-command: "quit"/"exit" must never be forwarded
// as an L1 line to a shared server (it would tear down the host for every
// other connected client) -- the same precedent apps/gui-sonotron/src/
// uds_brain_session.cpp's own is_blocked_command() established.
bool is_local_quit(const std::string& line) {
  std::size_t begin = 0;
  while (begin < line.size() && (line[begin] == ' ' || line[begin] == '\t')) {
    ++begin;
  }
  std::size_t end = begin;
  while (end < line.size() && line[end] != ' ' && line[end] != '\t') {
    ++end;
  }
  const std::string first = line.substr(begin, end - begin);
  return first == "quit" || first == "exit";
}

// Session state the two per-fd handlers below share, grouped so
// run_connect()'s own poll loop stays a flat, low-complexity dispatcher.
// `socket` is declared before `client` (member init order) so `client`'s
// SendLine callback can safely bind `this->socket` -- the socket object
// exists (though not yet connected) by the time the callback is built;
// `connect_to()` itself runs later, from run_connect().
struct ClientSession {
  arrangrr::host::UdsClient socket;
  arrangrr::host::TuiClient client;
  bool reading_stdin = true;
  std::string stdin_acc;
  std::uint64_t drain_deadline_ms = 0;

  ClientSession() : client([this](const std::string& line) { (void)socket.send_line(line); }) {}

  void begin_drain() {
    reading_stdin = false;
    drain_deadline_ms = monotonic_ms() + kDrainGraceMs;
  }
};

// One stdin-readable event: reads whatever is available, forwards each
// complete non-empty line verbatim to the server (ALL client actions go
// over L1 text -- D38, extended to cli-arrangrr), except a local "quit"/
// "exit" which starts the drain-and-exit sequence instead. EOF (or a read
// error) also starts that sequence.
void handle_stdin_readable(ClientSession& session) {
  char buf[512];
  const ssize_t got = read(STDIN_FILENO, buf, sizeof(buf));
  if (got <= 0) {
    session.begin_drain();
    return;
  }
  session.stdin_acc.append(buf, static_cast<std::size_t>(got));
  std::size_t nl = 0;
  while ((nl = session.stdin_acc.find('\n')) != std::string::npos) {
    const std::string line = session.stdin_acc.substr(0, nl);
    session.stdin_acc.erase(0, nl + 1);
    if (is_local_quit(line)) {
      session.begin_drain();
      return;
    }
    if (!line.empty()) {
      (void)session.socket.send_line(line);
    }
  }
}

// One socket-readable event: drains every complete inbound line, prints it,
// folds it into the client-side mirror, and prints the mirror's own
// description of the change when the line was a recognized kParamState
// echo -- observable proof a gesture's round trip actually landed.
void handle_socket_readable(ClientSession& session) {
  std::vector<std::string> lines;
  session.socket.poll_lines(lines);
  for (const std::string& line : lines) {
    std::printf("[in] %s\n", line.c_str());
    session.client.on_wire_line(line);
    if (!session.client.last_param_change().empty()) {
      std::printf("[mirror] %s\n", session.client.last_param_change().c_str());
    }
  }
}

}  // namespace

int run_connect(const std::string& sock_path) {
  ClientSession session;
  std::string error;
  if (!session.socket.connect_to(sock_path, error)) {
    std::fprintf(stderr, "cli-arrangrr --connect: %s\n", error.c_str());
    return 2;
  }
  std::printf("cli-arrangrr: connected to %s (pure client -- type L1 commands, 'quit' to leave)\n",
              sock_path.c_str());

  while (session.socket.connected()) {
    struct pollfd fds[2];
    int n = 0;
    int stdin_idx = -1;
    if (session.reading_stdin) {
      stdin_idx = n;
      fds[n].fd = STDIN_FILENO;
      fds[n].events = POLLIN;
      ++n;
    }
    const int socket_idx = n;
    fds[n].fd = session.socket.fd();
    fds[n].events = POLLIN;
    ++n;

    if (poll(fds, static_cast<nfds_t>(n), 100) < 0) {
      break;
    }
    if (stdin_idx >= 0 && (fds[stdin_idx].revents & (POLLIN | POLLHUP))) {
      handle_stdin_readable(session);
    }
    if (fds[socket_idx].revents & (POLLIN | POLLHUP | POLLERR)) {
      handle_socket_readable(session);
    }
    if (!session.reading_stdin && monotonic_ms() >= session.drain_deadline_ms) {
      break;
    }
  }

  std::printf("cli-arrangrr: disconnected\n");
  return 0;
}

}  // namespace arrangrr::client
