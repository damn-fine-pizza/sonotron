// arrangrr host binary.
//
//   arrangrr --script FILE [--events jsonl|human]   deterministic virtual clock,
//                                                   canonical JSONL on stdout
//   arrangrr [--events jsonl|human]                 live: ALSA virtual ports +
//                                                   real clock + REPL on stdin
//   arrangrr --connect PATH                         pure UDS client of an
//                                                   already-running
//                                                   sonotron-server (docs/
//                                                   design/orchestrator-
//                                                   pipeline-extraction.md
//                                                   §17.5 Phase 3b) -- see
//                                                   client_mode.cpp.
//
// The script mode is the golden-test driver (DESIGN.md §24.5): same shell,
// same engine, no wall clock anywhere near the core.

#include <poll.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "client_mode.hpp"
#include "common/time.hpp"
#include "console.hpp"
#include "jsonl.hpp"
#include "kitty_keys.hpp"
#include "midi_hal.hpp"
#include "rc_config.hpp"
#include "shell.hpp"
#include "uds_server.hpp"
#include "version/version.hpp"

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

// Scans `s` for a CSI query reply `ESC [ ? ... <terminator>` where terminator
// is the sequence's final byte (0x40..0x7E). Kitty's keyboard-flags reply ends
// in 'u'; a Primary Device Attributes reply ends in 'c'.
bool has_csi_query_reply(const std::string& s, char terminator) {
  for (std::size_t i = 0; i + 2 < s.size(); ++i) {
    if (s[i] != 0x1B || s[i + 1] != '[' || s[i + 2] != '?') {
      continue;
    }
    for (std::size_t j = i + 3; j < s.size(); ++j) {
      const char c = s[j];
      if (c >= 0x40 && c <= 0x7E) {  // CSI final byte terminates the sequence
        if (c == terminator) {
          return true;
        }
        break;
      }
    }
  }
  return false;
}

// Returns `s` with every terminal-originated CSI-private reply
// (ESC [ ? ... <final>) removed. Whatever remains is real user input that a
// fast pipe delivered inside the probe window and must not be lost.
std::string strip_csi_query_replies(const std::string& s) {
  std::string out;
  for (std::size_t i = 0; i < s.size();) {
    if (i + 2 < s.size() && s[i] == 0x1B && s[i + 1] == '[' && s[i + 2] == '?') {
      std::size_t j = i + 3;
      while (j < s.size() && !(s[j] >= 0x40 && s[j] <= 0x7E)) {
        ++j;
      }
      if (j < s.size()) {
        i = j + 1;  // drop the whole sequence including its final byte
        continue;
      }
      out.append(s, i, s.size() - i);  // incomplete tail: keep verbatim
      break;
    }
    out.push_back(s[i]);
    ++i;
  }
  return out;
}

// Probes whether the terminal implements the kitty keyboard protocol. Writes a
// keyboard-flags query immediately followed by a Device Attributes query, then
// reads stdin (raw mode, VMIN=0) until the DA reply lands or a short timeout
// elapses. DA is answered by every terminal, so its reply is the "done talking"
// sentinel; a kitty-flags reply seen alongside it means the protocol is live.
// The replies are consumed here so they never leak into the input stream; any
// non-reply bytes read alongside them are returned in `leftover` for replay.
// Must run after raw mode is set and before the REPL starts reading stdin.
bool detect_kitty_support(std::string& leftover) {
  (void)!write(STDOUT_FILENO, kitty::kQueryFlags.data(), kitty::kQueryFlags.size());
  (void)!write(STDOUT_FILENO, kitty::kQueryDeviceAttributes.data(),
               kitty::kQueryDeviceAttributes.size());

  constexpr int kProbeTimeoutMs = 150;
  std::string acc;
  for (;;) {
    pollfd fd{};
    fd.fd = STDIN_FILENO;
    fd.events = POLLIN;
    if (poll(&fd, 1, kProbeTimeoutMs) <= 0) {
      break;  // timeout or error: assume no support
    }
    char buf[256];
    const ssize_t got = read(STDIN_FILENO, buf, sizeof(buf));
    if (got <= 0) {
      break;
    }
    acc.append(buf, static_cast<std::size_t>(got));
    if (has_csi_query_reply(acc, 'c')) {
      break;  // the terminal has answered Device Attributes; it is done
    }
  }
  leftover = strip_csi_query_replies(acc);
  return has_csi_query_reply(acc, 'u');
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

int run_live(bool human, const char* init_path, const char* motd_path, const char* control_path) {
  const std::unique_ptr<IMidiHal> midi = make_midi_hal();
  std::string error;
  if (!midi->open(kAlsaClientName, error)) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 2;
  }

  // Pane UI when we own a terminal; flat output for pipes and tests.
  Console console;
  const bool tui = console.init();
  LineEditor editor;

  // D38 GUI transport: opt-in only (--control PATH). run_script() never
  // constructs this, so the adapter is impossible to reach in script/golden
  // mode regardless of what flags are passed.
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
      midi->send(ev.port, ev.msg);
    }
    const bool flats = shell_ref != nullptr && shell_ref->prefer_flats();
    const std::string line = human ? to_human(ev, flats) : to_jsonl(ev, flats);
    // Phase 3a (docs/design/orchestrator-pipeline-extraction.md §17.3b):
    // kParamState has no text shape yet -- to_human/to_jsonl render it as an
    // empty string; skip the events panel/stdout/broadcast for it entirely.
    if (!line.empty()) {
      if (tui) {
        shell_ref->log_event(line);  // append to the scrolling events panel
      } else {
        std::puts(line.c_str());
      }
    }
    // GUI clients always get the canonical JSONL wire format (the same one
    // the golden harness diffs), independent of --events human/jsonl, since
    // a control-plane client parses JSON, never the human one-liner.
    if (control.enabled()) {
      const std::string jsonl_line = to_jsonl(ev, flats);
      if (!jsonl_line.empty()) {
        control.broadcast(jsonl_line);
      }
    }
  });
  shell_ref = &shell;
  // Inbound: a control-plane line is the SAME L1 grammar the REPL accepts.
  // A parse error is reported back to the originating client only — it never
  // touches stdout/the panels, mirroring how a REPL syntax error stays local
  // to that terminal.
  control.set_line_handler([&](int client_fd, const std::string& line) {
    std::string cmd_error;
    if (!shell.exec_line(line, cmd_error)) {
      control.send_error(client_fd, cmd_error, line);
    }
  });
  shell.set_panel_hook([&](const std::vector<std::string>& lines) {
    if (!tui) {
      return false;  // plain mode prints help inline as before
    }
    console.set_panel(lines);
    console.render_input(editor, shell.repl_focused());
    return true;
  });
  if (tui) {
    shell.set_print_hook([&](const std::string& line) { shell.console_output(line); });
    shell.set_width_provider([&]() { return console.columns(); });
    shell.set_height_provider([&]() { return console.panel_rows(); });
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
    if (!midi->create_port(def, port_error)) {
      std::fprintf(stderr, "%s\n", port_error.c_str());
    }
  });

  // Default setup: one in, one out, wired thru — instantly useful.
  std::string ignored;
  shell.exec_line("port open in in0", ignored);
  shell.exec_line("port open out out0", ignored);
  shell.exec_line("thru in0 out0", ignored);
  std::printf("arrangrr live: ALSA ports in0/out0 (thru). Type commands; 'quit' to exit.\n");

  // Default-open panel set (bottom-to-top: piano, console, styles, chords,
  // events); the menu/filter/empty panels stay available via `panel open ...`.
  // Panels only exist in the live TUI. ~/.arrangrr.rc, if present, overrides the
  // order/heights/layout.
  if (tui) {
    shell.exec_line("panel open piano", ignored);
    shell.exec_line("panel open console", ignored);
    shell.exec_line("panel open styles", ignored);
    shell.exec_line("panel open chords", ignored);
    shell.exec_line("panel open parts", ignored);
    shell.exec_line("panel open events", ignored);
    shell.exec_line("panel focus piano", ignored);  // start in play mode

    // Default two-surface topology (before any ~/.arrangrr.init override): piano
    // panel = melody (sounds, no steer), chords panel = harmony (silent) + the
    // chord-detect source with detection ON. Fresh launch: play piano = sound;
    // focus the chords panel + play = silent re-harmonize.
    shell.configure_default_surfaces();

    const char* home = std::getenv("HOME");
    if (home != nullptr && *home != '\0') {
      shell.apply_rc(load_rc(std::string(home) + "/.arrangrr.rc"));
    }
  }

  // Terminal-aware piano input (H3). Probe once for the kitty keyboard protocol;
  // without it there is no key-release event, so momentary mode is impossible
  // and we say so plainly rather than pretending to hold notes.
  bool kitty_supported = false;
  std::string kitty_leftover;  // real input the probe read past the replies
  if (tui) {
    kitty_supported = detect_kitty_support(kitty_leftover);
    if (!kitty_supported) {
      shell.set_momentary_available(false);
      shell.console_output(
          "terminal has no key-release support — piano uses toggle mode "
          "(press = on, press again = off)");
    }
  }

  // --init FILE: run a setup script, then stay interactive. Explicit and STRICT
  // — a failing line aborts, so a scripted run fails loud.
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
  } else if (tui) {
    // No explicit --init: auto-run ~/.arrangrr.init if present, so a saved
    // session (chord mode/follow, style, ...) comes up on EVERY launch with no
    // flag — symmetric with ~/.arrangrr.rc. LENIENT: a bad line warns to the
    // console and startup continues (a stale default must never brick the TUI).
    const char* home = std::getenv("HOME");
    if (home != nullptr && *home != '\0') {
      const std::string path = std::string(home) + "/.arrangrr.init";
      std::ifstream init(path);
      if (init) {
        std::string line;
        int line_no = 0;
        while (std::getline(init, line)) {
          ++line_no;
          std::string line_error;
          if (!shell.exec_line(line, line_error)) {
            shell.console_output("~/.arrangrr.init:" + std::to_string(line_no) + ": " + line_error);
          }
        }
        shell.console_output("init: ~/.arrangrr.init loaded");
      }
    }
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
    // Seed the status bar up front so it reflects the (stopped) transport even
    // if a fast-piped session quits before the loop's first throttled refresh.
    console.set_status(status_line(shell));
    console.render_input(editor, shell.repl_focused());
  }

  std::string stdin_acc;  // partial-line accumulator (flat mode)
  std::uint64_t last_status_us = 0;
  bool running = true;

  // Debounced variation/style stepping (host-live): the pending selection is
  // applied ~500 ms after the LAST step key, so pressing fast skips the
  // variations/styles in between. The shell owns the pending state + a
  // generation counter; here we own only the clock.
  constexpr std::uint64_t kStyleStepDebounceUs = 500'000;
  std::uint32_t last_style_step_gen = shell.style_step_gen();
  std::uint64_t last_style_step_us = 0;

  // One byte through the existing panel-key + line-editor path (H2). Returns
  // false when the loop should stop (quit). Both the plain-byte stream and the
  // translated kitty press bytes funnel through here so behaviour is identical.
  auto feed_editor_byte = [&](std::uint8_t byte) -> bool {
    if (shell.handle_ui_key(byte)) {
      return true;
    }
    const LineEditor::Result r = editor.feed(byte);
    if (r.quit) {
      running = false;
      return false;
    }
    if (r.line) {
      shell.console_output("> " + *r.line);
      if (!shell.exec_line(*r.line, error)) {
        shell.console_output("error: " + error);
      }
      if (shell.quit_requested()) {
        running = false;
        return false;
      }
    }
    return true;
  };

  // A complete kitty key escape (body = the bytes between "\x1b[" and 'u').
  // Musical keys drive true momentary press/release; everything else (TAB,
  // SPACE, N/V/C/./ shortcuts, Enter...) falls back to the normal byte path on
  // key-down so the piano's own shortcuts keep working while the protocol is on.
  auto dispatch_kitty = [&](std::string_view body) {
    const std::optional<KittyKeyEvent> ev = parse_kitty_key(body);
    if (!ev) {
      return;  // malformed
    }
    // Ctrl chords (CTRL+P, CTRL+SPACE, CTRL+\) arrive as kitty escapes with a
    // modifier field that the piano must never see as a plain letter. Translate
    // them back to the control byte a raw TTY would have delivered and route
    // through the normal key path — on key-down only, so a single physical
    // press is a single action (repeat/release would double-toggle).
    if (const std::optional<std::uint8_t> ctrl = control_byte_for(ev->code, ev->modifiers)) {
      if (ev->type == KittyKeyEvent::Type::kPress) {
        (void)feed_editor_byte(*ctrl);
      }
      return;
    }
    if (ev->code > 0x7F) {
      return;  // a non-ASCII functional key we do not translate
    }
    const char key = static_cast<char>(ev->code);
    if (ev->type == KittyKeyEvent::Type::kRelease) {
      shell.piano_key_event(key, false);
      return;
    }
    // A kitty autorepeat (kRepeat) for a held MUSICAL key must never create a
    // second note-on: the key is already sounding from its kPress, and the
    // momentary held-check would absorb it anyway — dropping it here keeps a
    // held key to exactly one note-on / note-off pair. Non-musical repeats still
    // fall through so held shortcuts keep their normal byte behaviour.
    if (ev->type == KittyKeyEvent::Type::kRepeat &&
        shell.piano_is_musical_key(static_cast<std::uint8_t>(key))) {
      return;
    }
    // Press (or a non-musical autorepeat): try the piano first, then the normal
    // byte path.
    if (!shell.piano_key_event(key, true)) {
      (void)feed_editor_byte(static_cast<std::uint8_t>(key));
    }
  };

  // Escape-sequence framing state, persistent across read() boundaries so a
  // sequence split over two reads still frames correctly. `kitty_on` mirrors
  // whether the protocol is currently pushed (piano focus only, TTY only).
  std::string esc;
  bool in_esc = false;
  bool kitty_on = false;

  // Replays a raw byte run through the line-editor path verbatim (used for
  // escapes we don't translate: arrows, Alt-chords, a lone ESC).
  auto replay_bytes = [&](std::string_view bytes) {
    for (const char c : bytes) {
      if (!feed_editor_byte(static_cast<std::uint8_t>(c))) {
        break;
      }
    }
  };

  // Bare CSI arrow: body is exactly "\x1b[" (esc is ESC '[' <final>, size 3).
  constexpr std::size_t kBareCsiSize = 3;
  auto is_bare_arrow = [&](std::uint8_t final_byte) {
    return esc.size() == kBareCsiSize &&
           (final_byte == 'A' || final_byte == 'B' || final_byte == 'C' || final_byte == 'D');
  };

  // Finishes the in-progress CSI in `esc`: a 'u' terminator is a kitty key
  // event; a bare arrow drives the chooser while it is up; anything else is a
  // line-editor escape (arrows/home/end).
  auto finish_csi = [&](std::uint8_t final_byte) {
    if (final_byte == 'u') {
      dispatch_kitty(std::string_view(esc).substr(2, esc.size() - 3));
    } else if (final_byte == 'Z') {
      // SHIFT+TAB: reverse focus cycle. A CSI ending in 'Z' is always backtab,
      // whether bare (ESC [ Z) or modified (ESC [ 1 ; 2 Z) — accept both, or
      // terminals that send the modified form drop out of the reverse cycle.
      shell.focus_prev();
    } else if (shell.styles_focused() && is_bare_arrow(final_byte)) {
      // up/down move the style highlight; left/right the section highlight.
      switch (final_byte) {
        case 'A':
          shell.chooser_nav_style(-1);
          break;
        case 'B':
          shell.chooser_nav_style(+1);
          break;
        case 'C':
          shell.chooser_nav_section(+1);
          break;
        case 'D':
          shell.chooser_nav_section(-1);
          break;
        default:
          break;
      }
    } else if (shell.parts_focused() && is_bare_arrow(final_byte)) {
      // up/down move the selected part in the mixer; left/right are unused.
      if (final_byte == 'A') {
        shell.parts_select(-1);
      } else if (final_byte == 'B') {
        shell.parts_select(+1);
      }
    } else if (shell.groove_focused() && is_bare_arrow(final_byte)) {
      // up/down select a groove parameter; left/right adjust it.
      switch (final_byte) {
        case 'A':
          shell.groove_select(-1);
          break;
        case 'B':
          shell.groove_select(+1);
          break;
        case 'C':
          shell.groove_adjust(+1);
          break;
        case 'D':
          shell.groove_adjust(-1);
          break;
        default:
          break;
      }
    } else if (shell.arp_panel_focused() && is_bare_arrow(final_byte)) {
      // up/down select an arp parameter; left/right adjust it.
      switch (final_byte) {
        case 'A':
          shell.arp_select(-1);
          break;
        case 'B':
          shell.arp_select(+1);
          break;
        case 'C':
          shell.arp_adjust(+1);
          break;
        case 'D':
          shell.arp_adjust(-1);
          break;
        default:
          break;
      }
    } else {
      replay_bytes(esc);
    }
    esc.clear();
    in_esc = false;
  };

  // One TUI input byte through the escape-framing state machine: kitty key
  // escapes (\x1b[ ... u) route to dispatch_kitty; everything else (plain keys,
  // arrows, Alt-chords, lone ESC) replays through the H2 panel-key + line-editor
  // path. Shared by the main read loop and the startup-probe leftover replay.
  auto feed_tui_byte = [&](std::uint8_t b) {
    if (!in_esc) {
      if (b == 0x1B) {
        in_esc = true;
        esc.clear();
        esc.push_back(static_cast<char>(b));
      } else {
        (void)feed_editor_byte(b);
      }
      return;
    }

    esc.push_back(static_cast<char>(b));
    if (esc.size() == 2) {
      // Two-byte escape that is not a CSI (Alt-key, lone ESC). With the chooser
      // up an ESC cancels it and the run is consumed; otherwise replay it whole.
      if (b != '[') {
        if (shell.styles_focused()) {
          shell.chooser_cancel();
        } else {
          replay_bytes(esc);
        }
        esc.clear();
        in_esc = false;
      }
      return;
    }
    // Inside a CSI: a final byte in 0x40..0x7E terminates it.
    if (b >= 0x40 && b <= 0x7E) {
      finish_csi(b);
    }
  };

  // Any real keystrokes the kitty probe read past the terminal's replies (a
  // fast pipe can deliver typed input inside the probe window) are replayed
  // now, in order, so nothing typed at startup is lost.
  for (std::size_t i = 0; i < kitty_leftover.size() && running; ++i) {
    feed_tui_byte(static_cast<std::uint8_t>(kitty_leftover[i]));
  }

  // Fixed slots (stdin, the tick timer) + ALSA's descriptors + the control
  // adapter's listen socket and however many GUI clients are connected. A
  // handful of control clients is the entire expected load, so this is
  // generous headroom rather than a real limit.
  constexpr int kMaxPollFds = 64;

  while (running && !shell.quit_requested()) {
    struct pollfd fds[kMaxPollFds];
    int n = 0;
    fds[n].fd = STDIN_FILENO;
    fds[n].events = POLLIN;
    ++n;
    fds[n].fd = tfd;
    fds[n].events = POLLIN;
    ++n;
    // IMidiHal reports its own descriptors through a portable MidiPollFd
    // buffer (see midi_hal.hpp), never assuming `struct pollfd` layout --
    // copy the entries it fills in into this function's own native array.
    MidiPollFd midi_fds[kMaxPollFds];
    const int midi_n = midi->fill_poll_fds(midi_fds, kMaxPollFds - n);
    for (int i = 0; i < midi_n; ++i) {
      fds[n + i].fd = midi_fds[i].fd;
      fds[n + i].events = midi_fds[i].events;
      fds[n + i].revents = 0;
    }
    n += midi_n;

    // Control adapter (D38): the listen socket first, then every currently
    // connected client. control_start stays -1 when the adapter is off, so
    // the handling block below is a no-op — script mode never reaches any of
    // this because it never constructs a UdsServer at all.
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
      break;
    }

    // Clock: harvest elapsed time into whole ticks.
    if (fds[1].revents & POLLIN) {
      std::uint64_t expirations = 0;
      const ssize_t n_read = read(tfd, &expirations, sizeof(expirations));
      // timerfd is level-triggered: on a short read, -1 (EINTR/EAGAIN), or a
      // spurious wakeup, just skip this iteration -- POLLIN will be set
      // again on the next poll() as long as the timer has really expired.
      if (n_read == static_cast<ssize_t>(sizeof(expirations))) {
        const std::uint64_t now_us = monotonic_us();
        acc.set_bpm(shell.engine().transport().bpm());
        const std::uint32_t ticks = acc.advance_us(now_us - last_us);
        last_us = now_us;
        if (ticks > 0) {
          std::string tick_error;
          shell.advance_by(ticks, tick_error);
        }
      }
    }

    // MIDI input from the platform backend.
    midi->drain_input([&](std::uint8_t port, const std::uint8_t* bytes, std::size_t len) {
      shell.feed_midi(port, Span<const std::uint8_t>(bytes, len));
    });

    // Control adapter (D38): new connections, then bytes from each existing
    // client. Each complete line lands in the line handler wired above,
    // which drives it through the exact same shell.exec_line() the REPL
    // uses.
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
        // Stamp the input clock so the piano's toggle-mode auto-repeat debounce
        // can measure key cadence without the Shell touching a real clock.
        shell.set_input_time_us(monotonic_us());
        // UI key dispatch runs before the line editor: with panel focus the
        // byte drives panels/piano; with REPL focus it falls through and typing
        // behaves exactly as before.
        for (ssize_t i = 0; i < got && running; ++i) {
          feed_tui_byte(static_cast<std::uint8_t>(buf[i]));
        }
        console.render_input(editor, shell.repl_focused());
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

    // Kitty keyboard protocol, scoped to piano focus only. Enabling it globally
    // would reroute EVERY REPL keystroke (including arrows/history/editing)
    // through CSI-u escapes, so we push the flags only when the terminal is
    // known to support it AND the piano panel is focused AND we are in momentary
    // mode — the only situation where true key-release matters — and pop them
    // the instant any of those stops holding. Gated behind `tui` (isatty), so
    // scripts/pipes never see a byte of it.
    if (tui) {
      // Both playable surfaces want true key-release for momentary: the piano
      // panel (melody) and the chords panel (harmony). Kitty is pushed only while
      // one of them is focused in momentary mode.
      const PanelId focused = shell.panels().focused_panel();
      const bool play_surface = shell.panels().focus_kind() == PanelFocus::kPanel &&
                                (focused == PanelId::kPiano || focused == PanelId::kChords);
      // The chooser needs plain-CSI arrows, so kitty is popped while it is up
      // (else arrows would arrive as CSI-u escapes the chooser never sees).
      const bool want_kitty = kitty_supported && play_surface && !shell.styles_focused() &&
                              shell.piano_key_mode() == PianoKeyMode::kMomentary;
      if (want_kitty != kitty_on) {
        kitty_on = want_kitty;
        kitty::set_progressive_enhancement(STDOUT_FILENO, kitty_on);
      }
    }

    // Debounced variation/style step: reset the timer whenever the pending
    // selection advanced (generation bumped), then apply once it has been
    // quiet for kStyleStepDebounceUs.
    {
      const std::uint32_t gen = shell.style_step_gen();
      const std::uint64_t now_us = monotonic_us();
      if (gen != last_style_step_gen) {
        last_style_step_gen = gen;
        last_style_step_us = now_us;
      }
      if (shell.style_step_pending() && now_us - last_style_step_us > kStyleStepDebounceUs) {
        shell.apply_style_step();
      }
    }

    // Panel grid + status bar + input cursor upkeep (throttled to ~10 Hz). The
    // grid re-push here flushes the events panel's appended MIDI without a
    // repaint per event.
    if (tui) {
      const std::uint64_t now_us = monotonic_us();
      if (now_us - last_status_us > 100'000) {
        last_status_us = now_us;
        shell.refresh_panels();
        console.set_status(status_line(shell));
        console.render_input(editor, shell.repl_focused());
      }
    }
  }
  close(tfd);
  if (tui && kitty_on) {
    kitty::set_progressive_enhancement(STDOUT_FILENO, false);  // pop before restoring the tty
  }
  console.shutdown();
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  const char* script = nullptr;
  const char* init = nullptr;
  const char* motd = nullptr;
  const char* control = nullptr;
  const char* connect = nullptr;
  bool human = false;
  bool events_set = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--script") == 0 && i + 1 < argc) {
      script = argv[++i];
    } else if (std::strcmp(argv[i], "--init") == 0 && i + 1 < argc) {
      init = argv[++i];
    } else if (std::strcmp(argv[i], "--motd") == 0 && i + 1 < argc) {
      motd = argv[++i];
    } else if (std::strcmp(argv[i], "--control") == 0 && i + 1 < argc) {
      control = argv[++i];
    } else if (std::strcmp(argv[i], "--connect") == 0 && i + 1 < argc) {
      connect = argv[++i];
    } else if (std::strcmp(argv[i], "--events") == 0 && i + 1 < argc) {
      human = std::strcmp(argv[++i], "human") == 0;
      events_set = true;
    } else if (std::strcmp(argv[i], "--help") == 0) {
      std::printf(
          "usage: arrangrr [--script FILE|-] [--init FILE] [--motd FILE] "
          "[--events jsonl|human] [--control PATH] [--connect PATH]\n"
          "  the live TUI also auto-runs ~/.arrangrr.init (a command script) if "
          "present; --init FILE overrides it. See docs/arrangrr.init.example.\n"
          "  --connect PATH: pure client of an already-running sonotron-server\n"
          "  (or another --control-serving arrangrr/sonotron-server); mutually\n"
          "  exclusive with --script/--control/--init/--motd.\n");
      return 0;
    } else if (std::strcmp(argv[i], "--version") == 0) {
      std::printf("arrangrr %s (%s)\n", sonotron::version::kVersionString,
                  sonotron::version::kVersionFull);
      return 0;
    } else {
      std::fprintf(stderr, "unknown argument: %s\n", argv[i]);
      return 2;
    }
  }
  // --connect: a pure socket client -- never constructs a Shell/Engine of
  // its own (docs/design/orchestrator-pipeline-extraction.md §17.5 Phase
  // 3b). Checked first since it is a wholly different mode from the
  // embedded script/live paths below.
  if (connect != nullptr) {
    return arrangrr::client::run_connect(connect);
  }
  // Script mode defaults to canonical JSONL (golden format); live to human.
  if (script) {
    // --control is a live-only feature (D38): the golden-test driver never
    // sees it, by construction — run_script() has no such parameter at all.
    return run_script(script, human);
  }
  return run_live(events_set ? human : true, init, motd, control);
}
