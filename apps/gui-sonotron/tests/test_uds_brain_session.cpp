// Round-trip test for UdsBrainSession (ux-workstation.md §9) against a real
// AF_UNIX listen socket driven synchronously in-process (same style as
// components/platform/hostrt/tests/test_host.cpp::test_uds_server_end_to_end): no
// threads, no sleeps. Exercises connect -> send -> the server reads the
// exact bytes, and the server writes JSONL lines -> poll() decodes them.
// Also pins the quit/exit blacklist (gui-contract-map.md §0): a GUI
// window-close or a stray Enter must never tear down the shared host. No
// GPU, no display, no core -- this is the GUI's own client of a plain
// listening AF_UNIX socket.

#include "src/uds_brain_session.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>
#include <string>
#include <vector>

#include "src/brain_event.hpp"
#include "test.hpp"

using sonotron::BrainEvent;
using sonotron::BrainSession;
using sonotron::UdsBrainSession;

namespace {

// RAII around a plain listening AF_UNIX socket, standing in for the core's
// UdsServer without linking it (the pure-client boundary rule).
struct ListenSocket {
  int fd = -1;
  std::string path;

  ~ListenSocket() {
    if (fd >= 0) {
      ::close(fd);
    }
    if (!path.empty()) {
      ::unlink(path.c_str());
    }
  }
};

ListenSocket make_listen_socket() {
  ListenSocket sock;
  sock.path = "/tmp/sonotron_gui_test_" + std::to_string(static_cast<long>(::getpid())) + ".sock";
  ::unlink(sock.path.c_str());

  sock.fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  CHECK(sock.fd >= 0);
  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::strncpy(addr.sun_path, sock.path.c_str(), sizeof(addr.sun_path) - 1);
  CHECK(::bind(sock.fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
  CHECK(::listen(sock.fd, 4) == 0);
  return sock;
}

void test_round_trip() {
  ListenSocket listener = make_listen_socket();

  UdsBrainSession session;
  CHECK(session.connect_to(listener.path));
  CHECK(session.status() == BrainSession::Status::kConnected);

  const int server_side = ::accept(listener.fd, nullptr, nullptr);
  CHECK(server_side >= 0);

  // Client -> server: the exact command bytes plus a newline.
  session.send("transport start");
  char buf[256];
  const ssize_t n = ::read(server_side, buf, sizeof(buf));
  CHECK(n > 0);
  CHECK(std::string(buf, static_cast<std::size_t>(n)) == "transport start\n");

  // Server -> client: two JSONL lines in one write, split and decoded by
  // poll().
  const std::string reply = R"({"ev":"transport","state":"playing","@":0})"
                            "\n"
                            R"({"ev":"section","name":"varA","@":1})"
                            "\n";
  CHECK(::write(server_side, reply.data(), reply.size()) == static_cast<ssize_t>(reply.size()));

  std::vector<BrainEvent> events;
  session.poll(events);
  CHECK(events.size() == 2);
  CHECK(events[0].valid);
  CHECK(events[0].kind == BrainEvent::Kind::kTransport);
  CHECK(events[0].transport_state == "playing");
  CHECK(events[1].valid);
  CHECK(events[1].kind == BrainEvent::Kind::kSection);
  CHECK(events[1].section_name == "varA");

  // EOF from the server drops the client connection cleanly.
  ::close(server_side);
  std::vector<BrainEvent> more;
  session.poll(more);
  CHECK(session.status() == BrainSession::Status::kDisconnected);
}

void test_connect_failure() {
  UdsBrainSession session;
  CHECK(!session.connect_to("/tmp/sonotron_gui_nonexistent_xyz.sock"));
  CHECK(session.status() == BrainSession::Status::kError);
  CHECK(!session.last_error().empty());
}

void test_quit_exit_blacklist_never_reaches_the_wire() {
  ListenSocket listener = make_listen_socket();

  UdsBrainSession session;
  CHECK(session.connect_to(listener.path));
  const int server_side = ::accept(listener.fd, nullptr, nullptr);
  CHECK(server_side >= 0);

  // None of these must produce a single byte on the wire.
  session.send("quit");
  session.send("exit");
  session.send("  exit  now");  // first token is still "exit": still blocked

  // The control: an ordinary command MUST go through, proving the socket
  // stayed usable and the blacklist did not silently break send() outright.
  session.send("transport stop");

  char buf[256];
  const ssize_t n = ::read(server_side, buf, sizeof(buf));
  CHECK(n > 0);
  CHECK(std::string(buf, static_cast<std::size_t>(n)) == "transport stop\n");

  ::close(server_side);
}

}  // namespace

int main() {
  test_round_trip();
  test_connect_failure();
  test_quit_exit_blacklist_never_reaches_the_wire();
  return sonotron::test::failures();
}
