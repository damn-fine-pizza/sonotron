#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// Unix-domain-socket control adapter (D38 GUI transport): a live-mode-only,
// opt-in server that lets an external client (the future Dear ImGui GUI)
// drive arrangrr and observe its event stream. It is a PURE transport: it
// carries the SAME L1 command grammar the REPL already accepts (one text
// line in, `Shell::exec_line`) and streams back the SAME OutEvent JSONL the
// host already renders (`to_jsonl`). Zero business logic lives here.
//
// The listen socket and every client socket are kept non-blocking; all
// actual I/O is driven by main.cpp's poll() loop, which calls
// handle_listen_readable() / handle_client_readable(fd) when poll() reports
// those fds readable. This class only owns the sockets and the per-client
// line buffering — never a thread, never a blocking call.

namespace arrangrr::host {

// Pure line-framing buffer: feed() it arbitrary byte chunks exactly as they
// arrive off a non-blocking read(), and it emits every complete
// '\n'-terminated line (in arrival order), retaining any trailing partial
// line for the next feed(). Deliberately socket-free so the framing logic is
// unit-testable without any real file descriptor.
class LineBuffer {
 public:
  // Caps how large an UNTERMINATED (partial) line may grow before feed()
  // reports overflow. Protects the host from unbounded memory growth if a
  // client never sends '\n' (a broken or hostile peer) — L1 command lines
  // are short, so this is generous headroom, not a real limit in practice.
  static constexpr std::size_t kMaxLineLength = 4096;

  // Appends `len` bytes from `data`, appending every complete line (the
  // trailing '\n' stripped; a trailing '\r' also stripped, so CRLF-framed
  // clients work too) to `out_lines`. An empty line between two newlines is
  // reported as an empty string; the caller decides whether to ignore it.
  // Returns false when the still-pending partial line has grown past
  // kMaxLineLength — the caller should treat the connection as broken and
  // drop it. Lines already appended to `out_lines` before the overflow was
  // detected remain valid and should still be dispatched.
  bool feed(const char* data, std::size_t len, std::vector<std::string>& out_lines);

  // The bytes accumulated so far for a not-yet-terminated line.
  const std::string& pending() const { return m_pending; }

 private:
  std::string m_pending;
};

class UdsServer {
 public:
  // Invoked once per complete inbound line, across any connected client.
  // `client_fd` identifies the originating connection, so the caller (main)
  // can route a resulting error back to exactly that client via
  // send_error(). The handler itself decides what the line means — this
  // class never inspects it.
  using LineHandler = std::function<void(int client_fd, const std::string& line)>;
  // Invoked once, right after a new client is accepted and registered
  // (before it has sent anything). Lets the caller reply with a "state
  // dump" -- e.g. every current kParamState value -- so a freshly-connected
  // client syncs its panels without waiting for the next mutation (docs/
  // design/orchestrator-pipeline-extraction.md §17, "state-dump on
  // connect").
  using ConnectHandler = std::function<void(int client_fd)>;

  UdsServer() = default;
  ~UdsServer();

  UdsServer(const UdsServer&) = delete;
  UdsServer& operator=(const UdsServer&) = delete;
  UdsServer(UdsServer&&) = delete;
  UdsServer& operator=(UdsServer&&) = delete;

  // Creates and binds the listening socket at `path` (a stale file at that
  // path is unlinked first). On ANY failure (path too long, socket/bind/
  // listen error, cannot set non-blocking) this logs one line to stderr,
  // leaves the adapter disabled (enabled() == false) and returns false —
  // the host must keep running with the adapter simply off. Never throws,
  // never crashes the process.
  bool start(const std::string& path);

  bool enabled() const { return m_listen_fd >= 0; }
  int listen_fd() const { return m_listen_fd; }

  // Fds of every currently connected client, in accept order. main.cpp adds
  // these (plus listen_fd()) to its poll() set each iteration.
  const std::vector<int>& client_fds() const { return m_client_order; }

  void set_line_handler(LineHandler handler) { m_on_line = std::move(handler); }
  void set_connect_handler(ConnectHandler handler) { m_on_connect = std::move(handler); }

  // Accepts every pending connection on the listen socket (non-blocking —
  // returns as soon as accept() reports none left). Call when poll() reports
  // listen_fd() readable.
  void handle_listen_readable();

  // Reads whatever is available on `fd` (non-blocking), feeds it through
  // that client's LineBuffer, and dispatches each complete line to the line
  // handler. Closes and forgets the client on EOF, a read error, or a
  // line-length overflow (see LineBuffer::kMaxLineLength). Call when poll()
  // reports one of client_fds() readable.
  void handle_client_readable(int fd);

  // Sends one JSON error line (`{"error":"...","cmd":"..."}`) to exactly one
  // client — the one whose command failed to parse.
  void send_error(int client_fd, const std::string& message, const std::string& cmd);

  // Writes `line + '\n'` to every connected client. A client that cannot
  // keep up (EAGAIN on a full kernel send buffer) or is gone (EPIPE/ECONNRESET)
  // is dropped rather than allowed to block or crash the host — broadcast is
  // best-effort delivery, never a guarantee. See uds_server.cpp for the full
  // rationale.
  void broadcast(const std::string& line);

  // Writes `line + '\n'` to exactly one client (the state-dump-on-connect
  // use case: replaying current values to ONLY the client that just joined,
  // never re-broadcasting them to everyone else). Same best-effort-drop
  // behavior as broadcast()/send_error().
  void send_line(int client_fd, const std::string& line);

 private:
  void close_client(int fd);
  void write_line(int fd, const std::string& line);

  int m_listen_fd = -1;
  std::string m_path;
  LineHandler m_on_line;
  ConnectHandler m_on_connect;
  std::unordered_map<int, LineBuffer> m_buffers;
  std::vector<int> m_client_order;
};

}  // namespace arrangrr::host
