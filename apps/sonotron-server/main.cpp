// sonotron-server: the headless backend around the arrangrr host library.
//
// `components/hostrt`'s `Shell` (wrapping `runtime::Runtime<Engine,N>` + the
// arrangrr stage) is the "server library" from
// docs/design/sonotron-server-phase2-brief.md §Topology — the composition of
// Runtime + the arrangrr wiring, exposing the Command-in/OutEvent-out ports.
// This binary is the THIN main() the brief keeps separate from that library:
// it owns exactly the I/O loop the brief does NOT put in the lib — ALSA, the
// UDS control socket, the tick-timer clock drive, and the poll() fan-in —
// plus the --script golden-test driver.
//
// No TUI, no REPL: those stay in apps/tools/cli-arrangrr (untouched in this
// phase; Phase 3 turns it into a pure client of this same wire protocol —
// docs/design/orchestrator-pipeline-extraction.md §4/§15). Today the two
// binaries duplicate this wiring on purpose (Phase 2a is purely additive).
//
//   sonotron-server --script FILE [--events jsonl|human]
//       deterministic virtual clock, canonical JSONL on stdout. The golden
//       harness (tests/golden) drives this mode — same Shell/Runtime/arrangrr
//       code as cli-arrangrr's own --script, byte-identical output.
//
//   sonotron-server [--control PATH] [--events jsonl|human]
//       live: ALSA virtual ports + a real clock. Driven ONLY by MIDI input
//       and the (optional) --control UDS socket — there is no stdin/REPL.

#include <poll.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>

#include "alsa_midi.hpp"
#include "common/time.hpp"
#include "jsonl.hpp"
#include "shell.hpp"
#include "uds_server.hpp"

namespace {

using namespace arrangrr;
using namespace arrangrr::host;

int run_script(const char* path, bool human) {
  std::ifstream file;
  std::istream* in = &std::cin;
  if (std::strcmp(path, "-") != 0) {
    file.open(path);
    if (!file) {
      std::fprintf(stderr, "cannot open script: %s\n", path);
      return 2;
    }
    in = &file;
  }

  Shell* shell_ref = nullptr;
  Shell shell([&](const OutEvent& ev) {
    const bool flats = shell_ref != nullptr && shell_ref->prefer_flats();
    const std::string rendered = human ? to_human(ev, flats) : to_jsonl(ev, flats);
    // Phase 3a (docs/design/orchestrator-pipeline-extraction.md §17.3b):
    // kParamState has no text shape yet -- to_human/to_jsonl render it as an
    // empty string; skip printing so the golden text stream stays untouched.
    if (!rendered.empty()) {
      std::puts(rendered.c_str());
    }
  });
  shell_ref = &shell;

  std::string line, error;
  int line_no = 0;
  while (std::getline(*in, line)) {
    ++line_no;
    if (!shell.exec_line(line, error)) {
      std::fprintf(stderr, "%s:%d: %s\n", path, line_no, error.c_str());
      return 1;
    }
    if (shell.quit_requested()) {
      break;
    }
  }
  return 0;
}

std::uint64_t monotonic_us() {
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<std::uint64_t>(ts.tv_sec) * 1'000'000u +
         static_cast<std::uint64_t>(ts.tv_nsec) / 1000u;
}

// The live/headless loop: owns exactly what the Phase 2 brief keeps out of
// the server library — AlsaMidi, the UdsServer control socket + wiring, the
// tick-timer clock drive, and the poll() fan-in across ALSA + control fds.
// No stdin/REPL/TUI at all: this binary is driven ONLY by the control socket
// (and by whatever MIDI arrives on its ALSA ports).
int run_server(bool human, const char* control_path) {
  AlsaMidi alsa;
  std::string error;
  if (!alsa.open("sonotron-server", error)) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 2;
  }

  UdsServer control;
  if (control_path != nullptr) {
    if (control.start(control_path)) {
      std::printf("control socket: %s\n", control_path);
    } else {
      std::fprintf(stderr, "control socket disabled (see error above)\n");
    }
  }

  Shell* shell_ref = nullptr;
  Shell shell([&](const OutEvent& ev) {
    if (ev.kind == OutEvent::Kind::kMidi) {
      alsa.send(ev.port, ev.msg);
    }
    const bool flats = shell_ref != nullptr && shell_ref->prefer_flats();
    const std::string line = human ? to_human(ev, flats) : to_jsonl(ev, flats);
    // Phase 3a (docs/design/orchestrator-pipeline-extraction.md §17.3b):
    // kParamState has no text shape yet -- to_human/to_jsonl render it as an
    // empty string; skip stdout/broadcast for it entirely.
    if (!line.empty()) {
      std::puts(line.c_str());
    }
    // Control clients always get the canonical JSONL wire format, independent
    // of --events human/jsonl (a control-plane client parses JSON, never the
    // human one-liner) — same discipline as cli-arrangrr's GUI transport.
    if (control.enabled()) {
      const std::string jsonl_line = to_jsonl(ev, flats);
      if (!jsonl_line.empty()) {
        control.broadcast(jsonl_line);
      }
    }
  });
  shell_ref = &shell;

  // Inbound: a control-plane line is the SAME L1 grammar the REPL accepts. A
  // parse error is reported back to the originating client only.
  control.set_line_handler([&](int client_fd, const std::string& line) {
    std::string cmd_error;
    if (!shell.exec_line(line, cmd_error)) {
      control.send_error(client_fd, cmd_error, line);
    }
  });
  shell.set_port_hook([&](const PortDef& def) {
    std::string port_error;
    if (!alsa.create_port(def, port_error)) {
      std::fprintf(stderr, "%s\n", port_error.c_str());
    }
  });

  // Default setup: one in, one out, wired thru — mirrors cli-arrangrr's live
  // default so a bare launch is immediately useful.
  std::string ignored;
  shell.exec_line("port open in in0", ignored);
  shell.exec_line("port open out out0", ignored);
  shell.exec_line("thru in0 out0", ignored);
  std::printf("sonotron-server live: ALSA ports in0/out0 (thru).\n");

  // Tick wakeup timer; actual tick count derives from elapsed time through
  // the core's drift-free TickAccumulator.
  const int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
  itimerspec its{};
  its.it_interval.tv_nsec = 500'000;  // 0.5 ms wakeup
  its.it_value.tv_nsec = 500'000;
  timerfd_settime(tfd, 0, &its, nullptr);

  TickAccumulator acc;
  std::uint64_t last_us = monotonic_us();

  // Fixed slots (the tick timer) + ALSA's descriptors + the control adapter's
  // listen socket and however many clients are connected. A handful of
  // control clients is the entire expected load, so this is generous
  // headroom rather than a real limit (mirrors cli-arrangrr's own bound).
  constexpr int kMaxPollFds = 64;

  bool running = true;
  while (running && !shell.quit_requested()) {
    struct pollfd fds[kMaxPollFds];
    int n = 0;
    fds[n].fd = tfd;
    fds[n].events = POLLIN;
    ++n;
    n += alsa.fill_poll_fds(&fds[n], kMaxPollFds - n);

    int control_start = -1;
    int control_client_count = 0;
    if (control.enabled()) {
      control_start = n;
      fds[n].fd = control.listen_fd();
      fds[n].events = POLLIN;
      ++n;
      for (const int cfd : control.client_fds()) {
        if (n >= kMaxPollFds) {
          break;  // defensive: more clients than the array can hold
        }
        fds[n].fd = cfd;
        fds[n].events = POLLIN;
        ++n;
        ++control_client_count;
      }
    }

    if (poll(fds, static_cast<nfds_t>(n), 100) < 0) {
      running = false;
      break;
    }

    // Clock: harvest elapsed time into whole ticks.
    if (fds[0].revents & POLLIN) {
      std::uint64_t expirations = 0;
      (void)read(tfd, &expirations, sizeof(expirations));
      const std::uint64_t now_us = monotonic_us();
      acc.set_bpm(shell.engine().transport().bpm());
      const std::uint32_t ticks = acc.advance_us(now_us - last_us);
      last_us = now_us;
      if (ticks > 0) {
        std::string tick_error;
        shell.advance_by(ticks, tick_error);
      }
    }

    // MIDI input from ALSA.
    alsa.drain_input([&](std::uint8_t port, const std::uint8_t* bytes, std::size_t len) {
      shell.feed_midi(port, Span<const std::uint8_t>(bytes, len));
    });

    // Control adapter: new connections, then bytes from each existing
    // client. Each complete line lands in the line handler wired above,
    // which drives it through the exact same shell.exec_line() the socket
    // client uses.
    if (control_start >= 0) {
      if (fds[control_start].revents & POLLIN) {
        control.handle_listen_readable();
      }
      for (int i = 0; i < control_client_count; ++i) {
        const int idx = control_start + 1 + i;
        if (fds[idx].revents & (POLLIN | POLLHUP | POLLERR)) {
          control.handle_client_readable(fds[idx].fd);
        }
      }
    }
  }
  close(tfd);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  const char* script = nullptr;
  const char* control = nullptr;
  bool human = false;
  bool events_set = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--script") == 0 && i + 1 < argc) {
      script = argv[++i];
    } else if (std::strcmp(argv[i], "--control") == 0 && i + 1 < argc) {
      control = argv[++i];
    } else if (std::strcmp(argv[i], "--events") == 0 && i + 1 < argc) {
      human = std::strcmp(argv[++i], "human") == 0;
      events_set = true;
    } else if (std::strcmp(argv[i], "--help") == 0) {
      std::printf(
          "usage: sonotron-server [--script FILE|-] [--events jsonl|human] "
          "[--control PATH]\n"
          "  headless backend: no TUI, no REPL -- driven by MIDI input and\n"
          "  the --control UDS socket only. See\n"
          "  docs/design/sonotron-server-phase2-brief.md.\n");
      return 0;
    } else {
      std::fprintf(stderr, "unknown argument: %s\n", argv[i]);
      return 2;
    }
  }
  // Script mode defaults to canonical JSONL (golden format); live to human —
  // mirrors cli-arrangrr's own defaults.
  if (script) {
    return run_script(script, human);
  }
  return run_server(events_set ? human : true, control);
}
