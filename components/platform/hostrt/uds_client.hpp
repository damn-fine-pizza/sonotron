#pragma once

#include <string>
#include <vector>

#include "uds_server.hpp"  // reuses arrangrr::host::LineBuffer's line framing

// Pure-client counterpart of uds_server.hpp's listen/accept side (docs/
// design/orchestrator-pipeline-extraction.md §17.5 Phase 3b): a blocking-
// connect, then non-blocking-I/O AF_UNIX/SOCK_STREAM client. Carries the
// SAME wire discipline as every other client of sonotron-server's control
// socket (gui-sonotron's UdsBrainSession is the architectural precedent this
// mirrors) -- a plain text line out, a '\n'-framed line in, never a binary
// Command on the wire. This class owns only the socket + line framing; it
// never parses a line's meaning (that is client_event.hpp / param_state_wire
// .hpp's job) -- kept deliberately dumb so it is trivially reusable by any
// caller that wants a blocking-connect UDS client (today: cli-arrangrr's
// --connect mode).

namespace arrangrr::host {

class UdsClient {
 public:
  UdsClient() = default;
  ~UdsClient();

  UdsClient(const UdsClient&) = delete;
  UdsClient& operator=(const UdsClient&) = delete;
  UdsClient(UdsClient&&) = delete;
  UdsClient& operator=(UdsClient&&) = delete;

  // Connects to the AF_UNIX socket at `path`. On success the fd is left
  // non-blocking for the caller's own poll() loop and true is returned. On
  // any failure `error` holds a human-readable reason and false is
  // returned -- never throws.
  bool connect_to(const std::string& path, std::string& error);

  bool connected() const { return m_fd >= 0; }
  int fd() const { return m_fd; }

  // Appends '\n' and writes `line`. Returns false (and drops the
  // connection) on a hard send error or if the peer is gone; a short write
  // is retried within the call.
  bool send_line(const std::string& line);

  // Drains everything currently readable (non-blocking), reassembles
  // complete lines and appends them to `out_lines`, in arrival order.
  // Leaves the connection open on EAGAIN (nothing more to read this round);
  // drops it on EOF or a hard error (connected() becomes false either way).
  void poll_lines(std::vector<std::string>& out_lines);

  // Hangs up without sending anything (never sends "quit" -- that would
  // tear down a shared host for every other connected client).
  void close_connection();

 private:
  int m_fd = -1;
  LineBuffer m_buffer;
};

}  // namespace arrangrr::host
