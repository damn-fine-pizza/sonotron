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
#include "kitty_keys.hpp"
#include "rc_config.hpp"
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
      shell_ref->log_event(line);  // append to the scrolling events panel
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
    // Seed the status bar up front so it reflects the (stopped) transport even
    // if a fast-piped session quits before the loop's first throttled refresh.
    console.set_status(status_line(shell));
    console.render_input(editor);
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
    // Press or autorepeat: try the piano first, then the normal byte path.
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
    } else if (final_byte == 'Z' && esc.size() == kBareCsiSize) {
      shell.focus_prev();  // SHIFT+TAB (CSI Z): reverse focus cycle
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
        // UI key dispatch runs before the line editor: with panel focus the
        // byte drives panels/piano; with REPL focus it falls through and typing
        // behaves exactly as before.
        for (ssize_t i = 0; i < got && running; ++i) {
          feed_tui_byte(static_cast<std::uint8_t>(buf[i]));
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

    // Kitty keyboard protocol, scoped to piano focus only. Enabling it globally
    // would reroute EVERY REPL keystroke (including arrows/history/editing)
    // through CSI-u escapes, so we push the flags only when the terminal is
    // known to support it AND the piano panel is focused AND we are in momentary
    // mode — the only situation where true key-release matters — and pop them
    // the instant any of those stops holding. Gated behind `tui` (isatty), so
    // scripts/pipes never see a byte of it.
    if (tui) {
      const bool piano_focused = shell.panels().focus_kind() == PanelFocus::kPanel &&
                                 shell.panels().focused_panel() == PanelId::kPiano;
      // The chooser needs plain-CSI arrows, so kitty is popped while it is up
      // (else arrows would arrive as CSI-u escapes the chooser never sees).
      const bool want_kitty = kitty_supported && piano_focused && !shell.styles_focused() &&
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
        console.render_input(editor);
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
