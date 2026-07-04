#include "shell.hpp"
#include "shell_internal.hpp"

#include <cstdio>

// Shell command handlers for help/panels and MIDI/transport I/O (port, route,
// thru, clock, raw send, panic, transport, bpm, advance). Bodies moved verbatim
// from shell.cpp; see shell_music_commands.cpp for the musical commands.

namespace arrangrr::host {

using namespace shell_detail;

std::vector<std::string> Shell::build_help(const std::string& topic) const {
  if (topic == "chord") {
    return {
        "help: chord",
        "  key <root> <mode>            C..B(+#/b); major minor dorian phrygian lydian",
        "                               mixolydian locrian",
        "  chord mode diatonic|single|shell",
        "  play <note.. up to 4> [quality] [vel]   e.g. play D | play C E Bb | play G 7",
        "  chord play ... | chord stop | chord hold on|off | chord out <port>[:ch]",
        "  chord detect on|off         live: held piano keys re-harmonize the band",
        "  qualities: maj min dim aug maj7 min7 m7 7 dom7 m7b5 halfdim dim7 sus2 sus4",
    };
  }
  if (topic == "seq") {
    return {
        "help: seq",
        "  seq new <name> | seq use <name>",
        "  seq add <note> [quality] [Nbars|Nbeats]   step entry (default 1bar)",
        "  seq rec | seq stop            record live plays; quantized to bars on stop",
        "  seq loop on|off | seq play",
        "  seq transpose to <root> [mode] | seq transpose +N|-N   re-derives degrees",
        "  seq del <i> | seq clear",
    };
  }
  if (topic == "style") {
    return {
        "help: style",
        "  style load basic",
        "  style route <role> <port>[:ch]   roles: drums perc bass chord1 chord2 pad",
        "                                   arp phrase lead cc",
        "  style section <name>             intro1|2 varA..D fillA..D break ending1|2",
        "                                   (lands on the next bar while playing)",
    };
  }
  if (topic == "track") {
    return {
        "help: track",
        "  track new <name> <port>[:ch] [role]",
        "  track step <name> <1-based #> <note|clear> [vel] [gate]",
        "  track length <name> <steps>      per-track length = polymeter",
        "  track mute|solo <name> on|off",
    };
  }
  if (topic == "midi") {
    return {
        "help: midi",
        "  port open in|out <name> [as <alias>]",
        "  route <in>[:ch] -> <out>[:ch] | thru <in> <out>",
        "  clock out <port>|none            MIDI clock master on that port",
        "  midi send <port> <hex bytes..>   raw injection",
        "  panic                            all notes off everywhere",
    };
  }
  if (topic == "panel") {
    return {
        "help: panel",
        "  panel list                       panels and their state",
        "  panel open|close|toggle <p>      p: menu piano filter (help = menu alias)",
        "  panel close all",
        "  panel focus <p>|repl|next        focus bookkeeping (key dispatch: H2)",
        "  panel status | panel help",
        "  help <topic> fills and opens the menu panel",
    };
  }
  if (topic == "piano") {
    return {
        "help: piano",
        "  panel open piano, then TAB to enter/leave play mode",
        "  white: A S D F G H J K L ; '     black: W E T Y U O P (P = D#5)",
        "  SPACE key mode: momentary (hold; needs kitty terminal) <-> toggle (any)",
        "  toggle: press = note-on, same key again = note-off",
        "  play keys: TAB exit | N names | V view | C clear | Z layout",
        "             . octave- | / octave+ | [ ] transpose",
        "  CTRL+P play/stop (global) | ` style/section chooser | CTRL+C quit",
        "  piano octave <N>|up|down | channel <1..16> | velocity <1..127>",
        "  piano view keyboard|active-notes|event-log | piano panic",
        "  chord detect on|off: held keys re-harmonize the band (chords panel)",
    };
  }
  if (topic == "styles") {
    return {
        "help: styles  (` focuses this panel; TAB cycles here too)",
        "  up/down: style       left/right: section (variation)",
        "  - / = : prev / next variation        _ / + : prev / next style",
        "  digits: filter styles by number      piano keys: set the tonality",
        "  ENTER: apply next bar                CTRL+\\ : apply now",
        "  steps are debounced ~0.5s (skip fast); the selection stays put",
    };
  }
  if (topic == "notes") {
    return {
        "help: notes",
        "  notes names cde|doremi|toggle",
        "  CDE (C D E ...) or DoReMi (Do Re Mi ...), applies to piano views",
        "  scientific octaves, C4 = 60; sharps default, flats follow the key",
    };
  }
  if (topic == "filter") {
    return {
        "help: filter",
        "  filter channel <1..16>           only that channel in the event log",
        "  filter port <N>                  only that port",
        "  filter event note-on|note-off    only that event kind",
        "  filter clear                     pass everything again",
    };
  }
  if (topic == "view") {
    return {
        "help: view",
        "  view show note-names|note-numbers|velocity|channel|port on|off",
        "  view show-octaves boundary|all|none    keyboard octave markers",
        "  view clear                             clear monitor buffers",
    };
  }
  return {
      "help  (help <topic> fills and opens the help panel)",
      "  transport start|stop|continue|tempo <bpm>",
      "  chord  - key, modes, play          (help chord)",
      "  seq    - chord progressions        (help seq)",
      "  style  - the arranger band         (help style)",
      "  track  - step sequencer            (help track)",
      "  midi   - ports, routing, panic     (help midi)",
      "  panel  - help/piano/filter panels  (help panel)",
      "  piano  - simulated keyboard        (help piano)",
      "  advance <N>[bars] | @<tick> <cmd> | quit",
      "  notes: C4=60, octave optional (D = D4), sharps/flats (F#3, Bb)",
  };
}

void Shell::open_help_topic(const std::string& topic) {
  const std::vector<std::string> lines = build_help(topic);
  m_panels.set_content(PanelId::kHelp, lines);
  m_panels.open(PanelId::kHelp);
  m_help_pinned = true;  // an explicit topic overrides the contextual content

  if (!push_panels()) {
    print_lines(lines);
  }
}

bool Shell::panel_layout(const std::vector<std::string>& t, std::string& error) {
  static const char* kUsage = "panel layout 1|2|toggle";
  if (t.size() < 3) {
    error = kUsage;
    return false;
  }

  if (t[2] == "1") {
    m_panels.set_per_row(panel_layout::kOnePerRow);
  } else if (t[2] == "2") {
    m_panels.set_per_row(panel_layout::kTwoPerRow);
  } else if (t[2] == "toggle") {
    m_panels.toggle_layout();
  } else {
    error = kUsage;
    return false;
  }

  (void)push_panels();
  return true;
}

bool Shell::panel_target(const std::string& sub, const std::vector<std::string>& t,
                         std::string& error) {
  if (t.size() < 3) {
    error = "panel " + sub + ": missing panel name";
    return false;
  }
  const std::string& target = t[2];

  if (sub == "close" && target == "all") {
    m_panels.close_all();
    (void)push_panels();
    return true;
  }
  if (sub == "focus" && (target == "repl" || target == "next")) {
    if (target == "repl") {
      m_panels.focus_repl();
    } else {
      m_panels.focus_next();
    }
    (void)push_panels();
    return true;
  }

  PanelId id{};
  if (!parse_panel_name(target, id)) {
    error = "unknown panel '" + target + "' (events console styles chords piano menu filter empty)";
    return false;
  }

  if (sub == "open") {
    m_panels.open(id);
  } else if (sub == "close") {
    m_panels.close(id);
  } else if (sub == "toggle") {
    m_panels.toggle(id);
  } else {
    m_panels.focus(id);
  }

  // A help panel opened before any `help <topic>` shows the overview.
  if (m_panels.visible(PanelId::kHelp) && m_panels.content(PanelId::kHelp).empty()) {
    m_panels.set_content(PanelId::kHelp, build_help(""));
  }

  const bool consumed = push_panels();
  if (!consumed && m_panels.visible(id)) {
    print_lines(m_panels.content(id));  // flat/script mode: show what opened
  }
  return true;
}

bool Shell::cmd_panel(const std::vector<std::string>& t, std::string& error) {
  static const char* kUsage =
      "panel list | open|close|toggle <p> | close all | focus <p>|repl|next | status | help";

  if (t.size() < 2) {
    error = kUsage;
    return false;
  }
  const std::string& sub = t[1];

  if (sub == "layout") {
    return panel_layout(t, error);
  }
  if (sub == "list") {
    print_lines(m_panels.list_lines());
    return true;
  }
  if (sub == "status") {
    print_lines(m_panels.status_lines());
    return true;
  }
  if (sub == "help") {
    open_help_topic("panel");
    return true;
  }
  if (sub == "open" || sub == "close" || sub == "toggle" || sub == "focus") {
    return panel_target(sub, t, error);
  }

  error = kUsage;
  return false;
}

bool Shell::cmd_help(const std::vector<std::string>& t, std::string& error) {
  const std::string topic = t.size() >= 2 ? t[1] : "";

  // Lifecycle moved under `panel ...` (H1): keep a migration hint alive.
  if (topic == "open" || topic == "close") {
    error = "help " + topic + " was removed: use panel open help / panel close help";
    return false;
  }

  open_help_topic(topic);
  return true;
}

bool Shell::cmd_port(const std::vector<std::string>& t, std::string& error) {
  // port open in|out <name> [as <alias>]
  const bool input = t[2] == "in";
  if (!input && t[2] != "out") {
    error = "port open in|out <name> [as <alias>]";
    return false;
  }
  if (t.size() < 4) {
    error = "port open: missing name";
    return false;
  }
  std::string name = t[3];
  if (t.size() >= 6 && t[4] == "as") {
    name = t[5];
  }
  std::uint8_t& next = input ? m_next_in : m_next_out;
  if (next >= kMaxPorts) {
    error = "no free port slots";
    return false;
  }
  const PortDef def{.name = name, .is_input = input, .index = next++};
  m_ports.push_back(def);
  if (m_port_hook) {
    m_port_hook(def);
  }
  return true;
}

bool Shell::cmd_transport(const std::vector<std::string>& t, std::string& error) {
  Command c;
  if (t[1] == "start") {
    c.param = Param::kTransportStart;
  } else if (t[1] == "stop") {
    c.param = Param::kTransportStop;
  } else if (t[1] == "continue") {
    c.param = Param::kTransportContinue;
  } else if (t[1] == "tempo" && t.size() >= 3) {
    std::uint32_t bpm = 0;
    if (!parse_bpm_x100(t[2], bpm)) {
      error = "bad tempo: " + t[2];
      return false;
    }
    c.op = Op::kSet;
    c.param = Param::kTransportTempo;
    c.a = static_cast<std::int32_t>(bpm);
  } else {
    error = "transport start|stop|continue|tempo <bpm>";
    return false;
  }
  m_engine.push_command(c, m_sink);
  return true;
}

bool Shell::cmd_bpm(const std::vector<std::string>& t, std::string& error) {
  // Convenience alias for `transport tempo`: with no argument it reports the
  // current tempo, with one it sets it through the same kTransportTempo path.
  constexpr std::uint32_t kBpmScale = 100;  // bpm() is bpm x100 (BpmX100)
  constexpr int kBpmLineSize = 32;
  char buf[kBpmLineSize];

  if (t.size() < 2) {
    const std::uint32_t bpm = m_engine.transport().bpm();
    std::snprintf(buf, sizeof(buf), "bpm %u.%02u", bpm / kBpmScale, bpm % kBpmScale);
    print_line(buf);
    return true;
  }

  std::uint32_t bpm = 0;
  if (!parse_bpm_x100(t[1], bpm)) {
    error = "bad bpm: " + t[1];
    return false;
  }
  Command c;
  c.op = Op::kSet;
  c.param = Param::kTransportTempo;
  c.a = static_cast<std::int32_t>(bpm);
  m_engine.push_command(c, m_sink);

  // Confirm with the tempo the engine actually holds (it clamps out-of-range
  // values), so the console never claims a change the core refused.
  const std::uint32_t now = m_engine.transport().bpm();
  std::snprintf(buf, sizeof(buf), "tempo set to %u.%02u bpm", now / kBpmScale, now % kBpmScale);
  print_line(buf);
  return true;
}

bool Shell::cmd_route(const std::vector<std::string>& t, std::string& error) {
  // route <in>[:ch] -> <out>[:ch]
  std::string in_name, out_name;
  int in_ch = -1, out_ch = -1;
  if (!split_port_channel(t[1], in_name, in_ch) || !split_port_channel(t[3], out_name, out_ch)) {
    error = "bad route channels";
    return false;
  }
  const int in = find_port(in_name, true);
  const int out = find_port(out_name, false);
  if (in < 0 || out < 0) {
    error = "unknown port in route";
    return false;
  }
  Command c;
  c.param = Param::kRouteAdd;
  c.a = in | ((in_ch & 0xFF) << 8);
  c.b = out | ((out_ch & 0xFF) << 8);
  c.c = route_pass::kAll;
  m_engine.push_command(c, m_sink);
  return true;
}

bool Shell::cmd_thru(const std::vector<std::string>& t, std::string& error) {
  // thru <in> <out> — sugar for an all-pass route.
  const int in = find_port(t[1], true);
  const int out = find_port(t[2], false);
  if (in < 0 || out < 0) {
    error = "unknown port in thru";
    return false;
  }
  Command c;
  c.param = Param::kRouteAdd;
  c.a = in | (0xFF << 8);   // any channel
  c.b = out | (0xFF << 8);  // keep channel
  c.c = route_pass::kAll;
  m_engine.push_command(c, m_sink);
  return true;
}

bool Shell::cmd_clock(const std::vector<std::string>& t, std::string& error) {
  Command c;
  c.op = Op::kSet;
  c.param = Param::kClockOutMask;
  if (t[2] == "none") {
    c.a = 0;
  } else {
    const int out = find_port(t[2], false);
    if (out < 0) {
      error = "unknown clock port";
      return false;
    }
    c.a = 1 << out;
  }
  m_engine.push_command(c, m_sink);
  return true;
}

bool Shell::cmd_midi_send(const std::vector<std::string>& t, std::string& error) {
  const int port = find_port(t[2], true);
  if (port < 0) {
    error = "unknown input port: " + t[2];
    return false;
  }
  std::vector<std::uint8_t> bytes;
  for (std::size_t i = 3; i < t.size(); ++i) {
    std::uint8_t b = 0;
    if (!parse_hex_byte(t[i], b)) {
      error = "bad hex byte: " + t[i];
      return false;
    }
    bytes.push_back(b);
  }
  m_engine.push_midi_in(static_cast<std::uint8_t>(port),
                        Span<const std::uint8_t>(bytes.data(), bytes.size()), m_sink);
  return true;
}

bool Shell::cmd_panic(const std::vector<std::string>& /*t*/, std::string& /*error*/) {
  Command c;
  c.param = Param::kPanic;
  m_engine.push_command(c, m_sink);
  return true;
}

bool Shell::cmd_advance(const std::vector<std::string>& t, std::string& error) {
  // advance <ticks> | advance <N>bars
  std::string arg = t[1];
  std::uint64_t n = 0;
  std::uint64_t mult = 1;
  if (arg.size() > 4 && arg.substr(arg.size() - 4) == "bars") {
    mult = kTicksPerBar;
    arg = arg.substr(0, arg.size() - 4);
  }
  if (!parse_u64(arg, n)) {
    error = "bad advance amount";
    return false;
  }
  return advance_to(m_engine.now() + n * mult, error);
}

}  // namespace arrangrr::host
