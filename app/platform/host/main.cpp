// arrangrr host binary.
//
//   arrangrr --script FILE [--events jsonl|human]   deterministic virtual clock,
//                                                   canonical JSONL on stdout
//   arrangrr [--events jsonl|human]                 live: ALSA virtual ports +
//                                                   real clock + REPL on stdin
//
// The script mode is the golden-test driver (DESIGN.md §29.5): same shell,
// same engine, no wall clock anywhere near the core.

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
#include "arrangrr/common/time.hpp"
#include "console.hpp"
#include "jsonl.hpp"
#include "shell.hpp"

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
    std::puts((human ? to_human(ev, flats) : to_jsonl(ev, flats)).c_str());
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

std::string status_line(const Shell& shell) {
  const Engine& e = shell.engine();
  const Position pos = e.transport().position();
  char buf[128];
  std::snprintf(buf, sizeof(buf), "%s  bar %u.%u  tempo %u.%02u  tick %u",
                e.transport().playing() ? "PLAYING" : "stopped", pos.bar, pos.beat,
                e.transport().bpm() / 100, e.transport().bpm() % 100, e.now());
  return buf;
}

int run_live(bool human, const char* init_path, const char* motd_path) {
  AlsaMidi alsa;
  std::string error;
  if (!alsa.open("arrangrr", error)) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 2;
  }

  // Pane UI when we own a terminal; flat output for pipes and tests.
  Console console;
  const bool tui = console.init();
  LineEditor editor;

  Shell* shell_ref = nullptr;
  Shell shell([&](const OutEvent& ev) {
    if (ev.kind == OutEvent::Kind::kMidi) {
      alsa.send(ev.port, ev.msg);
    }
    const bool flats = shell_ref != nullptr && shell_ref->prefer_flats();
    const std::string line = human ? to_human(ev, flats) : to_jsonl(ev, flats);
    if (tui) {
      console.emit(line);
    } else {
      std::puts(line.c_str());
    }
  });
  shell_ref = &shell;
  shell.set_panel_hook([&](const std::vector<std::string>& lines) {
    if (!tui) {
      return false;  // plain mode prints help inline as before
    }
    console.set_panel(lines);
    console.render_input(editor);
    return true;
  });
  if (tui) {
    shell.set_print_hook([&](const std::string& line) { console.emit(line); });
    shell.set_width_provider([&]() { return console.columns(); });
    // Resize contract (H1): geometry changed -> panels re-render from state.
    console.set_resize_hook([&]() { shell.refresh_panels(); });

    // Colors/unicode auto-resolve from the real terminal (H3): a TTY enables
    // colors; a UTF-8 locale enables unicode. --no-colors etc. would override.
    const char* locale = std::getenv("LC_ALL");
    if (locale == nullptr || *locale == '\0') {
      locale = std::getenv("LANG");
    }
    const bool utf8 = locale != nullptr && std::strstr(locale, "UTF-8") != nullptr;
    shell.configure_terminal(true, utf8);
  }

  // --motd FILE: guidance seeded into the help panel from the start. A
  // printed banner would be wiped by the pane UI's initial clear-screen.
  if (tui && motd_path != nullptr) {
    std::ifstream motd(motd_path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(motd, line)) {
      lines.push_back(line);
    }
    if (!lines.empty()) {
      shell.show_motd(lines);
    }
  }
  shell.set_port_hook([&](const PortDef& def) {
    std::string port_error;
    if (!alsa.create_port(def, port_error)) {
      std::fprintf(stderr, "%s\n", port_error.c_str());
    }
  });

  // Default setup: one in, one out, wired thru — instantly useful.
  std::string ignored;
  shell.exec_line("port open in in0", ignored);
  shell.exec_line("port open out out0", ignored);
  shell.exec_line("thru in0 out0", ignored);
  std::printf("arrangrr live: ALSA ports in0/out0 (thru). Type commands; 'quit' to exit.\n");

  // Piano and filter panels are visible from the start (help opens on demand
  // via `help <topic>` / --motd). Panels only exist in the live TUI.
  if (tui) {
    shell.exec_line("panel open filter", ignored);
    shell.exec_line("panel open piano", ignored);
  }

  // --init FILE: run a setup script, then stay interactive.
  if (init_path != nullptr) {
    std::ifstream init(init_path);
    if (!init) {
      std::fprintf(stderr, "cannot open init script: %s\n", init_path);
      return 2;
    }
    std::string line;
    int line_no = 0;
    while (std::getline(init, line)) {
      ++line_no;
      if (!shell.exec_line(line, error)) {
        std::fprintf(stderr, "%s:%d: %s\n", init_path, line_no, error.c_str());
        return 1;
      }
    }
    std::printf("init: %s loaded\n", init_path);
  }

  // Tick wakeup timer; actual tick count derives from elapsed time through
  // the core's drift-free TickAccumulator.
  const int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
  itimerspec its{};
  its.it_interval.tv_nsec = 500'000;  // 0.5 ms wakeup
  its.it_value.tv_nsec = 500'000;
  timerfd_settime(tfd, 0, &its, nullptr);

  TickAccumulator acc;
  std::uint64_t last_us = monotonic_us();

  if (!tui) {
    std::printf("arrangrr> ");
    std::fflush(stdout);
  } else {
    console.render_input(editor);
  }

  std::string stdin_acc;  // partial-line accumulator (flat mode)
  std::uint64_t last_status_us = 0;
  bool running = true;
  while (running && !shell.quit_requested()) {
    struct pollfd fds[16];
    int n = 0;
    fds[n].fd = STDIN_FILENO;
    fds[n].events = POLLIN;
    ++n;
    fds[n].fd = tfd;
    fds[n].events = POLLIN;
    ++n;
    n += alsa.fill_poll_fds(&fds[n], 16 - n);

    if (poll(fds, static_cast<nfds_t>(n), 100) < 0) {
      break;
    }

    // Clock: harvest elapsed time into whole ticks.
    if (fds[1].revents & POLLIN) {
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

    // REPL input. Raw read(2), never iostream: cin's stdio-synced buffer
    // hides pending lines from both poll() and in_avail(), stranding every
    // line after the first of a multi-line write until the NEXT write
    // arrives (a permanent one-behind pipeline).
    if (fds[0].revents & (POLLIN | POLLHUP)) {
      char buf[512];
      const ssize_t got = read(STDIN_FILENO, buf, sizeof(buf));
      if (got <= 0) {
        running = false;
      } else if (tui) {
        for (ssize_t i = 0; i < got; ++i) {
          // UI key dispatch runs before the line editor (H2): with panel
          // focus the byte drives panels/piano; with REPL focus it falls
          // through and typing behaves exactly as before.
          if (shell.handle_ui_key(static_cast<std::uint8_t>(buf[i]))) {
            continue;
          }
          const LineEditor::Result r = editor.feed(static_cast<std::uint8_t>(buf[i]));
          if (r.quit) {
            running = false;
            break;
          }
          if (r.line) {
            console.emit("> " + *r.line);
            if (!shell.exec_line(*r.line, error)) {
              console.emit("error: " + error);
            }
            if (shell.quit_requested()) {
              running = false;
              break;
            }
          }
        }
        console.render_input(editor);
      } else {
        stdin_acc.append(buf, static_cast<std::size_t>(got));
        std::size_t nl;
        while ((nl = stdin_acc.find('\n')) != std::string::npos) {
          const std::string line = stdin_acc.substr(0, nl);
          stdin_acc.erase(0, nl + 1);
          if (!shell.exec_line(line, error)) {
            std::printf("error: %s\n", error.c_str());
          }
          if (shell.quit_requested()) {
            break;
          }
        }
        std::printf("arrangrr> ");
        std::fflush(stdout);
      }
    }

    // Status bar + input cursor upkeep (throttled to ~10 Hz).
    if (tui) {
      const std::uint64_t now_us = monotonic_us();
      if (now_us - last_status_us > 100'000) {
        last_status_us = now_us;
        console.set_status(status_line(shell));
        console.render_input(editor);
      }
    }
  }
  close(tfd);
  console.shutdown();
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  const char* script = nullptr;
  const char* init = nullptr;
  const char* motd = nullptr;
  bool human = false;
  bool events_set = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--script") == 0 && i + 1 < argc) {
      script = argv[++i];
    } else if (std::strcmp(argv[i], "--init") == 0 && i + 1 < argc) {
      init = argv[++i];
    } else if (std::strcmp(argv[i], "--motd") == 0 && i + 1 < argc) {
      motd = argv[++i];
    } else if (std::strcmp(argv[i], "--events") == 0 && i + 1 < argc) {
      human = std::strcmp(argv[++i], "human") == 0;
      events_set = true;
    } else if (std::strcmp(argv[i], "--help") == 0) {
      std::printf(
          "usage: arrangrr [--script FILE|-] [--init FILE] [--motd FILE] "
          "[--events jsonl|human]\n");
      return 0;
    } else {
      std::fprintf(stderr, "unknown argument: %s\n", argv[i]);
      return 2;
    }
  }
  // Script mode defaults to canonical JSONL (golden format); live to human.
  if (script) {
    return run_script(script, human);
  }
  return run_live(events_set ? human : true, init, motd);
}
