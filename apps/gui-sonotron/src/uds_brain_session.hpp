#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "brain_event.hpp"
#include "brain_session.hpp"

// First concrete BrainSession (ux-workstation.md §9, decision §14.4): a
// non-blocking AF_UNIX/SOCK_STREAM client that speaks the GUI's OWN wire
// protocol -- it never links components/hostrt's UdsServer, it just connects
// as a plain client (mirrors
// components/hostrt/tests/test_host.cpp::test_uds_server_end_to_end). The
// render loop drives it once per frame: send() to push an L1 command line,
// poll() to drain and decode whatever the core streamed back. No background
// reader thread (matches the server's own best-effort broadcast -- see
// docs/design/gui-contract-map.md §1).

namespace sonotron {

class UdsBrainSession final : public BrainSession {
 public:
  UdsBrainSession() = default;
  ~UdsBrainSession() override;

  UdsBrainSession(const UdsBrainSession&) = delete;
  UdsBrainSession& operator=(const UdsBrainSession&) = delete;
  UdsBrainSession(UdsBrainSession&&) = delete;
  UdsBrainSession& operator=(UdsBrainSession&&) = delete;

  // Connects to the AF_UNIX socket at `path`. On success the fd is left
  // non-blocking, status() becomes Status::kConnected, and true is
  // returned. On any failure it returns false, status() becomes
  // Status::kError, last_error() holds a human-readable reason, and the
  // session stays disconnected. Never throws. Safe to call again to
  // reconnect (drops any existing connection first).
  bool connect_to(const std::string& path);

  // BrainSession
  void send(std::string_view command_line) override;
  void poll(std::vector<BrainEvent>& out) override;
  const BrainSnapshot& snapshot() const override;
  Status status() const override;

  // Human-readable reason for the last connect/send/recv failure, for the
  // GUI's connection-status readout.
  const std::string& last_error() const { return m_last_error; }

 private:
  // Closes the socket WITHOUT sending anything (never sends `quit` -- that
  // would kill the shared host for every client) and marks the session
  // Status::kDisconnected. Just hangs up.
  void close_fd();

  // Appends '\n' and writes `line` to the socket. Returns false (and drops
  // the connection, setting status() to Status::kError) if the peer is
  // gone or a hard error occurs. A short write is retried within the call.
  bool send_line(const std::string& line);

  // Drains everything currently readable (recv with MSG_DONTWAIT until
  // EAGAIN), reassembles complete lines through the LineBuffer, and appends
  // them to `out_lines`. On EOF the connection is dropped cleanly
  // (Status::kDisconnected); on a fatal recv error it is dropped as
  // Status::kError.
  void poll_lines(std::vector<std::string>& out_lines);

  int m_fd = -1;
  LineBuffer m_buffer;
  std::string m_last_error;
  Status m_status = Status::kDisconnected;
  BrainSnapshot m_snapshot;
};

}  // namespace sonotron
