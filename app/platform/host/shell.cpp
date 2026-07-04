#include "shell.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace arrangrr::host {

namespace {

std::vector<std::string> tokenize(const std::string& line) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : line) {
    // '#' opens a comment only at the start of a token — mid-token it is a
    // sharp (F#3). A comment therefore needs whitespace before it.
    if (c == '#' && cur.empty()) {
      break;
    }
    if (c == ' ' || c == '\t') {
      if (!cur.empty()) {
        out.push_back(std::move(cur)), cur.clear();
      }
    } else {
      cur.push_back(c);
    }
  }
  if (!cur.empty()) {
    out.push_back(std::move(cur));
  }
  return out;
}

bool parse_u64(const std::string& s, std::uint64_t& out) {
  if (s.empty()) {
    return false;
  }
  char* end = nullptr;
  out = std::strtoull(s.c_str(), &end, 10);
  return end && *end == '\0';
}

// "120" or "120.5" or "120.50" -> bpm_x100.
bool parse_bpm_x100(const std::string& s, std::uint32_t& out) {
  const auto dot = s.find('.');
  std::uint64_t whole = 0, frac = 0;
  if (dot == std::string::npos) {
    if (!parse_u64(s, whole)) {
      return false;
    }
  } else {
    std::string f = s.substr(dot + 1);
    if (f.empty() || f.size() > 2) {
      return false;
    }
    if (!parse_u64(s.substr(0, dot), whole) || !parse_u64(f, frac)) {
      return false;
    }
    if (f.size() == 1) {
      frac *= 10;
    }
  }
  out = static_cast<std::uint32_t>(whole * 100 + frac);
  return true;
}

// "name" or "name:ch" (1-based channel) -> name + channel (-1 = unspecified).
bool split_port_channel(const std::string& s, std::string& name, int& channel) {
  const auto colon = s.find(':');
  channel = -1;
  if (colon == std::string::npos) {
    name = s;
    return true;
  }
  name = s.substr(0, colon);
  std::uint64_t ch = 0;
  if (!parse_u64(s.substr(colon + 1), ch) || ch < 1 || ch > 16) {
    return false;
  }
  channel = static_cast<int>(ch - 1);
  return true;
}

// Note name in scientific pitch notation (C4 = 60): letter, optional #/b,
// octave -1..9. Plain MIDI numbers are accepted too.
bool parse_note(const std::string& s, std::uint8_t& out) {
  std::uint64_t raw = 0;
  if (parse_u64(s, raw)) {
    if (raw > 127) {
      return false;
    }
    out = static_cast<std::uint8_t>(raw);
    return true;
  }
  if (s.empty()) {
    return false;
  }
  static constexpr int kSemis[7] = {9, 11, 0, 2, 4, 5, 7};  // A B C D E F G
  const char letter = s[0];
  if (letter < 'A' || letter > 'G') {
    return false;
  }
  int semi = kSemis[letter - 'A'];
  std::size_t pos = 1;
  if (pos < s.size() && s[pos] == '#') {
    ++semi;
    ++pos;
  } else if (pos < s.size() && s[pos] == 'b') {
    --semi;
    ++pos;
  }
  if (pos >= s.size()) {
    const int dflt = (4 + 1) * 12 + semi;  // no octave -> octave 4 (C4 = 60)
    if (dflt < 0 || dflt > 127) {
      return false;
    }
    out = static_cast<std::uint8_t>(dflt);
    return true;
  }
  bool negative = false;
  if (s[pos] == '-') {
    negative = true;
    ++pos;
  }
  std::uint64_t octave = 0;
  if (!parse_u64(s.substr(pos), octave) || octave > 9) {
    return false;
  }
  const int oct = negative ? -static_cast<int>(octave) : static_cast<int>(octave);
  if (oct < -1) {
    return false;
  }
  const int note = (oct + 1) * 12 + semi;
  if (note < 0 || note > 127) {
    return false;
  }
  out = static_cast<std::uint8_t>(note);
  return true;
}

// Pitch class only (key roots): letter + optional #/b.
bool parse_pc(const std::string& s, std::uint8_t& out) {
  if (s.empty()) {
    return false;
  }
  static constexpr int kSemis[7] = {9, 11, 0, 2, 4, 5, 7};
  if (s[0] < 'A' || s[0] > 'G') {
    return false;
  }
  int semi = kSemis[s[0] - 'A'];
  if (s.size() == 2) {
    if (s[1] == '#') {
      ++semi;
    } else if (s[1] == 'b') {
      --semi;
    } else {
      return false;
    }
  } else if (s.size() > 2) {
    return false;
  }
  out = static_cast<std::uint8_t>((semi + 12) % 12);
  return true;
}

bool parse_mode(const std::string& s, Mode& out) {
  struct Entry {
    const char* name;
    Mode mode;
  };
  static constexpr Entry kModes[] = {
      {"major", Mode::kMajor},       {"minor", Mode::kMinor},   {"dorian", Mode::kDorian},
      {"phrygian", Mode::kPhrygian}, {"lydian", Mode::kLydian}, {"mixolydian", Mode::kMixolydian},
      {"locrian", Mode::kLocrian},
  };
  for (const Entry& e : kModes) {
    if (s == e.name) {
      out = e.mode;
      return true;
    }
  }
  return false;
}

bool parse_quality(const std::string& s, std::int8_t& out) {
  struct Entry {
    const char* name;
    ChordQuality q;
  };
  static constexpr Entry kQ[] = {
      {"maj", ChordQuality::kMaj},          {"min", ChordQuality::kMin},
      {"dim", ChordQuality::kDim},          {"aug", ChordQuality::kAug},
      {"maj7", ChordQuality::kMaj7},        {"min7", ChordQuality::kMin7},
      {"m7", ChordQuality::kMin7},          {"7", ChordQuality::kDom7},
      {"dom7", ChordQuality::kDom7},        {"m7b5", ChordQuality::kHalfDim7},
      {"halfdim", ChordQuality::kHalfDim7}, {"dim7", ChordQuality::kDim7},
      {"sus2", ChordQuality::kSus2},        {"sus4", ChordQuality::kSus4},
  };
  for (const Entry& e : kQ) {
    if (s == e.name) {
      out = static_cast<std::int8_t>(e.q);
      return true;
    }
  }
  return false;
}

// Flat-side keys spell with flats (Bb, Eb, ...): true when the parent major
// signature has flats. Parent major root = key root minus the mode's offset.
bool key_prefers_flats(std::uint8_t root_pc, Mode mode) {
  static constexpr std::uint8_t kOffset[7] = {0, 9, 2, 4, 5, 7, 11};
  const std::uint8_t parent =
      static_cast<std::uint8_t>((root_pc + 12 - kOffset[static_cast<int>(mode)]) % 12);
  return parent == 5 || parent == 10 || parent == 3 || parent == 8 || parent == 1 || parent == 6;
}

// "2bars" / "1bar" / "4beats" / "1beat" -> ticks.
bool parse_duration(const std::string& s, std::uint64_t& out_ticks) {
  auto strip = [&](const char* suffix, std::uint64_t mult) {
    const std::size_t n = std::string(suffix).size();
    if (s.size() <= n || s.substr(s.size() - n) != suffix) {
      return false;
    }
    std::uint64_t v = 0;
    if (!parse_u64(s.substr(0, s.size() - n), v) || v == 0) {
      return false;
    }
    out_ticks = v * mult;
    return true;
  };
  return strip("bars", kTicksPerBar) || strip("bar", kTicksPerBar) ||
         strip("beats", kTicksPerBeat) || strip("beat", kTicksPerBeat);
}

bool parse_section(const std::string& s, SectionType& out) {
  struct Entry {
    const char* name;
    SectionType type;
  };
  static constexpr Entry kSections[] = {
      {"intro1", SectionType::kIntro1},   {"intro2", SectionType::kIntro2},
      {"varA", SectionType::kVarA},       {"varB", SectionType::kVarB},
      {"varC", SectionType::kVarC},       {"varD", SectionType::kVarD},
      {"fillA", SectionType::kFillA},     {"fillB", SectionType::kFillB},
      {"fillC", SectionType::kFillC},     {"fillD", SectionType::kFillD},
      {"break", SectionType::kBreak},     {"ending1", SectionType::kEnding1},
      {"ending2", SectionType::kEnding2},
  };
  for (const Entry& e : kSections) {
    if (s == e.name) {
      out = e.type;
      return true;
    }
  }
  return false;
}

bool parse_role(const std::string& s, TrackRole& out) {
  struct Entry {
    const char* name;
    TrackRole role;
  };
  static constexpr Entry kRoles[] = {
      {"drums", TrackRole::kDrums},   {"perc", TrackRole::kPerc},     {"bass", TrackRole::kBass},
      {"chord1", TrackRole::kChord1}, {"chord2", TrackRole::kChord2}, {"pad", TrackRole::kPad},
      {"arp", TrackRole::kArp},       {"phrase", TrackRole::kPhrase}, {"lead", TrackRole::kLead},
      {"cc", TrackRole::kCc},
  };
  for (const Entry& e : kRoles) {
    if (s == e.name) {
      out = e.role;
      return true;
    }
  }
  return false;
}

bool parse_hex_byte(const std::string& s, std::uint8_t& out) {
  if (s.empty() || s.size() > 2) {
    return false;
  }
  char* end = nullptr;
  const unsigned long v = std::strtoul(s.c_str(), &end, 16);
  if (!end || *end != '\0' || v > 0xFF) {
    return false;
  }
  out = static_cast<std::uint8_t>(v);
  return true;
}

// Panel width used when no live terminal is attached (script/flat mode).
constexpr int kDefaultPanelColumns = 80;

// The simulated piano feeds the same input port real hardware uses (in0).
constexpr std::uint8_t kPianoInputPort = 0;
constexpr std::uint8_t kPianoReleaseVelocity = 64;

// Piano policy limits (docs/TUI_SPEC.md §4) and MIDI wire ranges.
constexpr int kPianoMinOctave = -1;
constexpr int kPianoMaxOctave = 9;
constexpr int kSemitonesPerOctave = 12;
constexpr int kMidiNoteMax = 127;
constexpr int kMidiChannels = 16;
constexpr int kMidiVelocityMin = 1;
constexpr int kMidiVelocityMax = 127;

bool parse_int(const std::string& s, int& out) {
  if (s.empty()) {
    return false;
  }

  char* end = nullptr;
  const long v = std::strtol(s.c_str(), &end, 10);
  if (end == nullptr || *end != '\0') {
    return false;
  }

  out = static_cast<int>(v);
  return true;
}

}  // namespace

Shell::Shell(EventSink sink)
    // Every host-visible OutEvent flows through the monitor before the user
    // sink: the MIDI monitor observes exactly what the host emits (H2).
    : m_sink([this, user = std::move(sink)](const OutEvent& ev) {
        m_monitor.observe(ev, m_pending_source_key);
        user(ev);
      }) {
  m_panels.set_content(PanelId::kFilter, {"filter: channel|port|event|clear (help filter)"});
}

std::vector<std::string> Shell::build_help(const std::string& topic) const {
  if (topic == "chord") {
    return {
        "help: chord",
        "  key <root> <mode>            C..B(+#/b); major minor dorian phrygian lydian",
        "                               mixolydian locrian",
        "  chord mode diatonic|single|shell",
        "  play <note.. up to 4> [quality] [vel]   e.g. play D | play C E Bb | play G 7",
        "  chord play ... | chord stop | chord hold on|off | chord out <port>[:ch]",
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
        "  panel open|close|toggle <p>      p: help piano filter",
        "  panel close all",
        "  panel focus <p>|repl|next        focus bookkeeping (key dispatch: H2)",
        "  panel status | panel help",
        "  help <topic> fills and opens the help panel",
    };
  }
  if (topic == "piano") {
    return {
        "help: piano",
        "  panel open piano, then TAB to enter/leave play mode",
        "  white: A S D F G H J K L ; '           black: W E T Y U O P",
        "  SPACE toggles key mode: momentary (default) <-> toggle",
        "  momentary: hold to sound, release to stop (needs a kitty-protocol",
        "             terminal: kitty/foot/ghostty/wezterm/recent xterm)",
        "  toggle:    press = note-on, same key again = note-off (any terminal)",
        "  piano octave <N>|up|down | channel <1..16> | velocity <1..127>",
        "  piano view keyboard|active-notes|event-log | piano panic",
        "  play keys: TAB exit | N names | V view | C clear | . octave- | / octave+",
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

void Shell::print_lines(const std::vector<std::string>& lines) {
  for (const std::string& line : lines) {
    if (m_print_hook) {
      m_print_hook(line);
    } else {
      std::printf("%s\n", line.c_str());
    }
  }
}

int Shell::panel_columns() const {
  if (m_width_provider) {
    return m_width_provider();
  }
  return kDefaultPanelColumns;
}

void Shell::refresh_piano_content() {
  m_panels.set_content(PanelId::kPiano, render_piano_panel(m_piano, panel_columns(), m_monitor,
                                                           m_filter, m_view_options));
}

bool Shell::push_panels() {
  // Width-dependent content is regenerated from state on every push, so a
  // resize can never leave a stale layout behind (H1 resize contract).
  if (m_panels.visible(PanelId::kPiano)) {
    refresh_piano_content();
  }

  return m_panel_hook && m_panel_hook(m_panels.combined_lines(panel_columns()));
}

void Shell::refresh_panels() { (void)push_panels(); }

void Shell::show_motd(const std::vector<std::string>& lines) {
  m_panels.set_content(PanelId::kHelp, lines);
  m_panels.open(PanelId::kHelp);

  if (!push_panels()) {
    print_lines(lines);
  }
}

void Shell::open_help_topic(const std::string& topic) {
  const std::vector<std::string> lines = build_help(topic);
  m_panels.set_content(PanelId::kHelp, lines);
  m_panels.open(PanelId::kHelp);

  if (!push_panels()) {
    print_lines(lines);
  }
}

bool Shell::cmd_panel(const std::vector<std::string>& t, std::string& error) {
  static const char* kUsage =
      "panel list | open|close|toggle <p> | close all | focus <p>|repl|next | status | help";

  if (t.size() < 2) {
    error = kUsage;
    return false;
  }
  const std::string& sub = t[1];

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
      error = "unknown panel '" + target + "' (help, piano, filter)";
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

  error = kUsage;
  return false;
}

void Shell::print_line(const std::string& line) { print_lines({line}); }

bool Shell::piano_midi_note(int semitone_from_base, std::uint8_t& out) const {
  const int note = (m_piano.base_octave + 1) * kSemitonesPerOctave + semitone_from_base;
  if (note < 0 || note > kMidiNoteMax) {
    return false;
  }
  out = static_cast<std::uint8_t>(note);
  return true;
}

bool Shell::piano_note_held(std::uint8_t midi_note) const {
  for (std::size_t i = 0; i < m_piano_held.size(); ++i) {
    const ActiveNote& n = m_piano_held.notes()[i];
    if (n.note == midi_note && n.channel == m_piano.channel) {
      return true;
    }
  }
  return false;
}

void Shell::piano_send_note(char key, std::uint8_t midi_note, bool note_on) {
  // Every piano note goes through the same feed_midi -> push_midi_in path real
  // hardware uses (docs/TUI_SPEC.md §1.3): the piano is an input device, never
  // a shortcut into the engine.
  std::uint8_t bytes[3];
  bytes[0] = static_cast<std::uint8_t>((note_on ? midi::kNoteOn : midi::kNoteOff) | m_piano.channel);
  bytes[1] = midi_note;
  bytes[2] = note_on ? m_piano.velocity : kPianoReleaseVelocity;

  m_pending_source_key = key;
  feed_midi(kPianoInputPort, Span<const std::uint8_t>(bytes, sizeof(bytes)));
  m_pending_source_key = 0;

  (void)push_panels();
}

void Shell::toggle_piano_key(char key, int semitone_from_base) {
  // Toggle note-off policy (H2): a plain TTY delivers no key-release events,
  // so pressing the same key again releases the note. This is also the
  // fallback the plain-byte path always uses, even in momentary mode.
  std::uint8_t midi_note = 0;
  if (!piano_midi_note(semitone_from_base, midi_note)) {
    print_line("piano: note out of MIDI range (octave " + std::to_string(m_piano.base_octave) +
               ")");
    return;
  }

  if (piano_note_held(midi_note)) {
    m_piano_held.note_off(kPianoInputPort, m_piano.channel, midi_note);
    piano_send_note(key, midi_note, false);
    return;
  }

  if (!m_piano_held.note_on({kPianoInputPort, m_piano.channel, midi_note, m_piano.velocity, key, 0})) {
    print_line("piano: too many held notes");
    return;
  }
  piano_send_note(key, midi_note, true);
}

void Shell::piano_momentary_on(char key, int semitone_from_base) {
  // Momentary note-on (kitty key-down): sound the note unless it is already
  // sounding. Autorepeat re-presses land here too, so the held check keeps a
  // physically-held key from double-firing note-on.
  std::uint8_t midi_note = 0;
  if (!piano_midi_note(semitone_from_base, midi_note)) {
    print_line("piano: note out of MIDI range (octave " + std::to_string(m_piano.base_octave) +
               ")");
    return;
  }

  if (piano_note_held(midi_note)) {
    return;
  }

  if (!m_piano_held.note_on({kPianoInputPort, m_piano.channel, midi_note, m_piano.velocity, key, 0})) {
    print_line("piano: too many held notes");
    return;
  }
  piano_send_note(key, midi_note, true);
}

void Shell::piano_momentary_off(char key, int semitone_from_base) {
  // Momentary note-off (kitty key-up): release the note only if we were
  // sounding it.
  std::uint8_t midi_note = 0;
  if (!piano_midi_note(semitone_from_base, midi_note)) {
    return;
  }

  if (!piano_note_held(midi_note)) {
    return;
  }
  m_piano_held.note_off(kPianoInputPort, m_piano.channel, midi_note);
  piano_send_note(key, midi_note, false);
}

void Shell::piano_all_notes_off() {
  // Release everything the piano is holding through the normal input path.
  while (m_piano_held.size() > 0) {
    const ActiveNote n = m_piano_held.notes()[0];

    std::uint8_t bytes[3] = {static_cast<std::uint8_t>(midi::kNoteOff | n.channel), n.note,
                             kPianoReleaseVelocity};
    m_piano_held.note_off(n.port, n.channel, n.note);
    feed_midi(kPianoInputPort, Span<const std::uint8_t>(bytes, sizeof(bytes)));
  }

  (void)push_panels();
}

bool Shell::cmd_piano(const std::vector<std::string>& t, std::string& error) {
  static const char* kUsage =
      "piano octave <N>|up|down | channel <1..16> | velocity <1..127> | "
      "keymap default | view keyboard|active-notes|event-log | panic";

  if (t.size() < 2) {
    error = kUsage;
    return false;
  }
  const std::string& sub = t[1];

  if (sub == "octave") {
    if (t.size() < 3) {
      error = "piano octave <N>|up|down";
      return false;
    }

    int octave = m_piano.base_octave;
    if (t[2] == "up") {
      octave += 1;
    } else if (t[2] == "down") {
      octave -= 1;
    } else {
      char* end = nullptr;
      octave = static_cast<int>(std::strtol(t[2].c_str(), &end, 10));
      if (end == nullptr || *end != '\0') {
        error = "piano octave: not a number: " + t[2];
        return false;
      }
    }

    if (octave < kPianoMinOctave || octave > kPianoMaxOctave) {
      error = "piano octave: out of range " + std::to_string(kPianoMinOctave) + ".." +
              std::to_string(kPianoMaxOctave);
      return false;
    }

    m_piano.base_octave = octave;
    (void)push_panels();
    return true;
  }

  if (sub == "channel") {
    int ch = 0;
    if (t.size() < 3 || !parse_int(t[2], ch) || ch < 1 || ch > kMidiChannels) {
      error = "piano channel <1..16>";
      return false;
    }
    m_piano.channel = static_cast<std::uint8_t>(ch - 1);  // user 1-based, wire 0-based
    (void)push_panels();
    return true;
  }

  if (sub == "velocity") {
    int vel = 0;
    if (t.size() < 3 || !parse_int(t[2], vel) || vel < kMidiVelocityMin || vel > kMidiVelocityMax) {
      error = "piano velocity <1..127>";
      return false;
    }
    m_piano.velocity = static_cast<std::uint8_t>(vel);
    (void)push_panels();
    return true;
  }

  if (sub == "keymap") {
    if (t.size() < 3 || t[2] != "default") {
      error = "piano keymap default (the only keymap in H2)";
      return false;
    }
    return true;
  }

  if (sub == "view") {
    if (t.size() < 3) {
      error = "piano view keyboard|active-notes|event-log";
      return false;
    }
    if (t[2] == "keyboard") {
      m_piano.view = PianoView::kKeyboard;
    } else if (t[2] == "active-notes") {
      m_piano.view = PianoView::kActiveNotes;
    } else if (t[2] == "event-log") {
      m_piano.view = PianoView::kEventLog;
    } else {
      error = "piano view: unknown view: " + t[2];
      return false;
    }
    (void)push_panels();
    return true;
  }

  if (sub == "panic") {
    piano_all_notes_off();
    return true;
  }

  error = kUsage;
  return false;
}

bool Shell::cmd_notes(const std::vector<std::string>& t, std::string& error) {
  if (t.size() < 3 || t[1] != "names") {
    error = "notes names cde|doremi|toggle";
    return false;
  }

  if (t[2] == "cde") {
    m_piano.note_naming = NoteNaming::kCde;
  } else if (t[2] == "doremi") {
    m_piano.note_naming = NoteNaming::kDoReMi;
  } else if (t[2] == "toggle") {
    m_piano.note_naming =
        m_piano.note_naming == NoteNaming::kCde ? NoteNaming::kDoReMi : NoteNaming::kCde;
  } else {
    error = "notes names cde|doremi|toggle";
    return false;
  }

  (void)push_panels();
  return true;
}

bool Shell::cmd_filter(const std::vector<std::string>& t, std::string& error) {
  static const char* kUsage = "filter channel <1..16> | port <N> | event note-on|note-off | clear";

  if (t.size() < 2) {
    error = kUsage;
    return false;
  }

  if (t[1] == "clear") {
    m_filter = MidiEventFilter{};
    (void)push_panels();
    return true;
  }

  if (t[1] == "channel") {
    int ch = 0;
    if (t.size() < 3 || !parse_int(t[2], ch) || ch < 1 || ch > kMidiChannels) {
      error = "filter channel <1..16>";
      return false;
    }
    m_filter.channel = static_cast<std::uint8_t>(ch - 1);
    (void)push_panels();
    return true;
  }

  if (t[1] == "port") {
    int port = 0;
    if (t.size() < 3 || !parse_int(t[2], port) || port < 0 || port >= static_cast<int>(kMaxPorts)) {
      error = "filter port <0.." + std::to_string(kMaxPorts - 1) + ">";
      return false;
    }
    m_filter.port = static_cast<std::uint8_t>(port);
    (void)push_panels();
    return true;
  }

  if (t[1] == "event") {
    if (t.size() < 3) {
      error = "filter event note-on|note-off";
      return false;
    }
    if (t[2] == "note-on") {
      m_filter.event_kind = MidiEventKindFilter::kNoteOn;
    } else if (t[2] == "note-off") {
      m_filter.event_kind = MidiEventKindFilter::kNoteOff;
    } else {
      error = "filter event note-on|note-off";
      return false;
    }
    (void)push_panels();
    return true;
  }

  error = kUsage;
  return false;
}

bool Shell::cmd_view(const std::vector<std::string>& t, std::string& error) {
  static const char* kUsage =
      "view show note-names|note-numbers|velocity|channel|port on|off | "
      "view show-octaves boundary|all|none | view clear";

  if (t.size() < 2) {
    error = kUsage;
    return false;
  }

  if (t[1] == "clear") {
    // Clears the visible monitor buffers (the C shortcut does the same).
    m_monitor.clear();
    (void)push_panels();
    return true;
  }

  if (t[1] == "show-octaves") {
    if (t.size() < 3) {
      error = "view show-octaves boundary|all|none";
      return false;
    }
    if (t[2] == "boundary") {
      m_piano.octave_display = OctaveDisplayMode::kBoundary;
    } else if (t[2] == "all") {
      m_piano.octave_display = OctaveDisplayMode::kAll;
    } else if (t[2] == "none") {
      m_piano.octave_display = OctaveDisplayMode::kNone;
    } else {
      error = "view show-octaves boundary|all|none";
      return false;
    }
    (void)push_panels();
    return true;
  }

  if (t[1] == "show" && t.size() >= 4) {
    const bool on = t[3] == "on";
    if (!on && t[3] != "off") {
      error = "view show " + t[2] + " on|off";
      return false;
    }

    if (t[2] == "note-names") {
      m_view_options.show_note_names = on;
    } else if (t[2] == "note-numbers") {
      m_view_options.show_note_numbers = on;
    } else if (t[2] == "velocity") {
      m_view_options.show_velocity = on;
    } else if (t[2] == "channel") {
      m_view_options.show_channel = on;
    } else if (t[2] == "port") {
      m_view_options.show_port = on;
    } else {
      error = "view show: unknown option: " + t[2];
      return false;
    }
    (void)push_panels();
    return true;
  }

  error = kUsage;
  return false;
}

bool Shell::handle_ui_key(std::uint8_t byte) {
  // TAB cycles focus repl <-> visible panels, even FROM the repl: opening the
  // piano and pressing TAB drops you straight into play mode. With no panel to
  // focus it falls through so a lone TAB still reaches the editor.
  if (byte == '\t') {
    if (m_panels.focus_kind() != PanelFocus::kPanel && !m_panels.any_visible()) {
      return false;
    }
    m_panels.focus_next();
    (void)push_panels();
    return true;
  }

  // Every other shortcut/musical key needs a focused panel; with repl focus the
  // byte falls through to the line editor and typing stays exactly as before.
  if (m_panels.focus_kind() != PanelFocus::kPanel) {
    return false;
  }

  if (m_panels.focused_panel() != PanelId::kPiano) {
    return false;  // help/filter focus: keys fall through to the editor
  }

  // SPACE flips the key mode (momentary <-> toggle). It is a mode switch only
  // in piano focus and is never a musical note.
  if (byte == ' ') {
    m_piano_key_mode =
        m_piano_key_mode == PianoKeyMode::kMomentary ? PianoKeyMode::kToggle : PianoKeyMode::kMomentary;
    print_line(m_piano_key_mode == PianoKeyMode::kMomentary
                   ? "piano: momentary key mode (hold to sound; needs a kitty-protocol terminal)"
                   : "piano: toggle key mode (press = on, same key again = off)");
    return true;
  }

  const char upper = static_cast<char>(std::toupper(static_cast<int>(byte)));

  // Piano-focus shortcuts take priority over musical keys (none collide).
  // TAB is the way out of play mode; 'P' is deliberately NOT a shortcut — it
  // sits right next to 'O' (C#5) and a stray press must never close the panel.
  switch (upper) {
    case 'N': {
      std::string ignored;
      (void)cmd_notes({"notes", "names", "toggle"}, ignored);
      return true;
    }
    case 'V': {
      const PianoView next = m_piano.view == PianoView::kKeyboard      ? PianoView::kActiveNotes
                             : m_piano.view == PianoView::kActiveNotes ? PianoView::kEventLog
                                                                       : PianoView::kKeyboard;
      m_piano.view = next;
      (void)push_panels();
      return true;
    }
    case 'C':
      m_monitor.clear();
      (void)push_panels();
      return true;
    case '.': {
      std::string ignored;
      (void)cmd_piano({"piano", "octave", "down"}, ignored);
      return true;
    }
    case '/': {
      std::string ignored;
      (void)cmd_piano({"piano", "octave", "up"}, ignored);
      return true;
    }
    default:
      break;
  }

  // Musical keys: the plain-byte path is ALWAYS toggle (a plain TTY cannot see
  // key-release), so the piano is playable on every terminal even in momentary
  // mode. True momentary press/release arrives via piano_key_event instead.
  if (const PianoKeyBinding* binding = piano_binding_for(byte); binding != nullptr) {
    toggle_piano_key(binding->key, binding->semitone_from_base);
    return true;
  }

  // Piano focus swallows everything else so stray keys never leak into a
  // half-typed REPL command.
  return true;
}

const PianoKeyBinding* Shell::piano_binding_for(std::uint8_t byte) const {
  // Case-insensitive; ';' and '\'' have no upper form so they match by byte.
  const char upper = static_cast<char>(std::toupper(static_cast<int>(byte)));
  for (const PianoKeyBinding& binding : default_keymap_white()) {
    if (binding.key == upper || binding.key == static_cast<char>(byte)) {
      return &binding;
    }
  }
  for (const PianoKeyBinding& binding : default_keymap_black()) {
    if (binding.key == upper) {
      return &binding;
    }
  }
  return nullptr;
}

bool Shell::piano_key_event(char key, bool pressed) {
  // Only piano focus turns keys into notes (matches handle_ui_key). Non-piano
  // focus lets the caller fall back to the normal byte path.
  if (m_panels.focus_kind() != PanelFocus::kPanel ||
      m_panels.focused_panel() != PanelId::kPiano) {
    return false;
  }

  const PianoKeyBinding* binding = piano_binding_for(static_cast<std::uint8_t>(key));
  if (binding == nullptr) {
    return false;  // TAB / SPACE / shortcuts: caller drives the byte path
  }

  if (m_piano_key_mode == PianoKeyMode::kToggle) {
    // In toggle mode a key-down toggles; the key-up carries no meaning.
    if (pressed) {
      toggle_piano_key(binding->key, binding->semitone_from_base);
    }
    return true;
  }

  if (pressed) {
    piano_momentary_on(binding->key, binding->semitone_from_base);
  } else {
    piano_momentary_off(binding->key, binding->semitone_from_base);
  }
  return true;
}

int Shell::find_seq(const std::string& name) const {
  for (std::size_t i = 0; i < m_seqs.size(); ++i) {
    if (m_seqs[i] == name) {
      return static_cast<int>(i);
    }
  }
  std::uint64_t idx = 0;
  if (parse_u64(name, idx) && idx < m_seqs.size()) {
    return static_cast<int>(idx);
  }
  return -1;
}

int Shell::find_track(const std::string& name) const {
  for (std::size_t i = 0; i < m_tracks.size(); ++i) {
    if (m_tracks[i] == name) {
      return static_cast<int>(i);
    }
  }
  std::uint64_t idx = 0;
  if (parse_u64(name, idx) && idx < m_tracks.size()) {
    return static_cast<int>(idx);
  }
  return -1;
}

int Shell::find_port(const std::string& name, bool input) const {
  for (const PortDef& p : m_ports) {
    if (p.name == name && p.is_input == input) {
      return p.index;
    }
  }
  // Bare numeric index is accepted too.
  std::uint64_t idx = 0;
  if (parse_u64(name, idx) && idx < kMaxPorts) {
    return static_cast<int>(idx);
  }
  return -1;
}

bool Shell::exec_line(const std::string& line, std::string& error) {
  std::vector<std::string> tokens = tokenize(line);
  if (tokens.empty()) {
    return true;
  }

  // @tick prefix: queue for execution when time reaches the tick (D29).
  if (tokens[0].size() > 1 && tokens[0][0] == '@') {
    std::uint64_t tick = 0;
    if (!parse_u64(tokens[0].substr(1), tick)) {
      error = "bad @tick: " + tokens[0];
      return false;
    }
    tokens.erase(tokens.begin());
    if (tokens.empty()) {
      error = "@tick with no command";
      return false;
    }
    if (tick < m_engine.now()) {
      error = "@tick in the past";
      return false;
    }
    m_pending.push_back(Pending{tick, m_pending_order++, std::move(tokens)});
    return true;
  }

  return exec_now(tokens, error);
}

bool Shell::advance_to(std::uint64_t target, std::string& error) {
  while (true) {
    // Earliest pending line at or before target (stable by insertion order).
    auto next = m_pending.end();
    for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
      if (it->tick > target) {
        continue;
      }
      if (next == m_pending.end() || it->tick < next->tick ||
          (it->tick == next->tick && it->order < next->order)) {
        next = it;
      }
    }
    if (next == m_pending.end()) {
      break;
    }
    const std::uint64_t at = next->tick;
    if (at > m_engine.now()) {
      m_engine.advance_ticks(static_cast<std::uint32_t>(at - m_engine.now()), m_sink);
    }
    std::vector<std::string> tokens = std::move(next->tokens);
    m_pending.erase(next);
    if (!exec_now(tokens, error)) {
      return false;
    }
  }
  if (target > m_engine.now()) {
    m_engine.advance_ticks(static_cast<std::uint32_t>(target - m_engine.now()), m_sink);
  }
  return true;
}

bool Shell::exec_now(const std::vector<std::string>& t, std::string& error) {
  const std::string& cmd = t[0];

  if (cmd == "quit" || cmd == "exit") {
    m_quit = true;
    return true;
  }

  if (cmd == "help") {
    const std::string topic = t.size() >= 2 ? t[1] : "";

    // Lifecycle moved under `panel ...` (H1): keep a migration hint alive.
    if (topic == "open" || topic == "close") {
      error = "help " + topic + " was removed: use panel open help / panel close help";
      return false;
    }

    open_help_topic(topic);
    return true;
  }

  if (cmd == "panel") {
    return cmd_panel(t, error);
  }

  if (cmd == "piano") {
    return cmd_piano(t, error);
  }

  if (cmd == "notes") {
    return cmd_notes(t, error);
  }

  if (cmd == "filter") {
    return cmd_filter(t, error);
  }

  if (cmd == "view") {
    return cmd_view(t, error);
  }

  if (cmd == "port" && t.size() >= 3 && t[1] == "open") {
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
    const PortDef def{name, input, next++};
    m_ports.push_back(def);
    if (m_port_hook) {
      m_port_hook(def);
    }
    return true;
  }

  if (cmd == "transport" && t.size() >= 2) {
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

  if (cmd == "route" && t.size() == 4 && t[2] == "->") {
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

  if (cmd == "thru" && t.size() >= 3) {
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

  if (cmd == "clock" && t.size() >= 3 && t[1] == "out") {
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

  if (cmd == "midi" && t.size() >= 4 && t[1] == "send") {
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

  if (cmd == "key" && t.size() >= 3) {
    std::uint8_t root = 0;
    Mode mode = Mode::kMajor;
    if (!parse_pc(t[1], root)) {
      error = "bad key root: " + t[1];
      return false;
    }
    if (!parse_mode(t[2], mode)) {
      error = "bad mode: " + t[2];
      return false;
    }
    m_prefer_flats = key_prefers_flats(root, mode);
    Command c;
    c.op = Op::kSet;
    c.param = Param::kKeySet;
    c.a = root;
    c.b = static_cast<std::int32_t>(mode);
    m_engine.push_command(c, m_sink);
    return true;
  }

  if ((cmd == "play" && t.size() >= 2) || (cmd == "chord" && t.size() >= 3 && t[1] == "play")) {
    const std::size_t base = cmd == "play" ? 1 : 2;
    // Up to 4 note tokens (shell mode voicings), then [quality] [velocity].
    std::int32_t packed = 0;
    int note_count = 0;
    std::size_t next = base;
    std::uint8_t note = 0;
    while (next < t.size() && note_count < 4 && parse_note(t[next], note)) {
      if (note == 0) {
        error = "note 0 (C-1) cannot be packed; use 1..127";
        return false;
      }
      packed |= static_cast<std::int32_t>(note) << (8 * note_count);
      ++note_count;
      ++next;
    }
    if (note_count == 0) {
      error = "bad note: " + t[base];
      return false;
    }
    std::int8_t quality = -1;
    std::uint64_t vel = 100;
    if (next < t.size() && parse_quality(t[next], quality)) {
      ++next;
    }
    if (next < t.size() && (!parse_u64(t[next], vel) || vel < 1 || vel > 127)) {
      error = "bad velocity: " + t[next];
      return false;
    }
    Command c;
    c.param = Param::kChordPlay;
    c.a = packed;
    c.b = quality;
    c.c = static_cast<std::int32_t>(vel);
    m_engine.push_command(c, m_sink);
    return true;
  }

  if (cmd == "chord" && t.size() >= 2) {
    if (t[1] == "mode" && t.size() >= 3) {
      std::int32_t mode = -1;
      if (t[2] == "diatonic") {
        mode = 0;
      } else if (t[2] == "single") {
        mode = 1;
      } else if (t[2] == "shell") {
        mode = 2;
      }
      if (mode < 0) {
        error = "chord mode diatonic|single|shell";
        return false;
      }
      Command c;
      c.op = Op::kSet;
      c.param = Param::kChordMode;
      c.a = mode;
      m_engine.push_command(c, m_sink);
      return true;
    }
    if (t[1] == "stop") {
      Command c;
      c.param = Param::kChordStop;
      m_engine.push_command(c, m_sink);
      return true;
    }
    if (t[1] == "hold" && t.size() >= 3 && (t[2] == "on" || t[2] == "off")) {
      Command c;
      c.op = Op::kSet;
      c.param = Param::kChordHold;
      c.a = t[2] == "on" ? 1 : 0;
      m_engine.push_command(c, m_sink);
      return true;
    }
    if (t[1] == "out" && t.size() >= 3) {
      std::string port_name;
      int channel = -1;
      if (!split_port_channel(t[2], port_name, channel)) {
        error = "bad chord destination: " + t[2];
        return false;
      }
      const int port = find_port(port_name, false);
      if (port < 0) {
        error = "unknown output port: " + port_name;
        return false;
      }
      Command c;
      c.op = Op::kSet;
      c.param = Param::kChordOut;
      c.a = port | ((channel < 0 ? 0 : channel) << 8);
      m_engine.push_command(c, m_sink);
      return true;
    }
    error = "chord play|stop|hold|out ...";
    return false;
  }

  if (cmd == "style" && t.size() >= 2) {
    const std::string& verb = t[1];
    Command c;
    if (verb == "load" && t.size() >= 3) {
      // Built-in styles resolve by name host-side (D26).
      std::int32_t index = -1;
      if (t[2] == "basic") {
        index = 0;
      }
      if (index < 0) {
        error = "unknown style: " + t[2];
        return false;
      }
      c.param = Param::kStyleLoad;
      c.a = index;
      m_engine.push_command(c, m_sink);
      return true;
    }
    if (verb == "route" && t.size() >= 4) {
      TrackRole role = TrackRole::kLead;
      if (!parse_role(t[2], role)) {
        error = "unknown role: " + t[2];
        return false;
      }
      std::string port_name;
      int channel = -1;
      if (!split_port_channel(t[3], port_name, channel)) {
        error = "bad destination: " + t[3];
        return false;
      }
      const int port = find_port(port_name, false);
      if (port < 0) {
        error = "unknown output port: " + port_name;
        return false;
      }
      c.op = Op::kSet;
      c.param = Param::kStyleRoute;
      c.a = static_cast<std::int32_t>(role);
      c.b = port | ((channel < 0 ? 0 : channel) << 8);
      m_engine.push_command(c, m_sink);
      return true;
    }
    if (verb == "section" && t.size() >= 3) {
      SectionType type = SectionType::kVarA;
      if (!parse_section(t[2], type)) {
        error = "unknown section: " + t[2];
        return false;
      }
      c.param = Param::kStyleSection;
      c.a = static_cast<std::int32_t>(type);
      m_engine.push_command(c, m_sink);
      return true;
    }
    error = "style load|route|section ...";
    return false;
  }

  if (cmd == "seq" && t.size() >= 2) {
    const std::string& verb = t[1];
    Command c;

    if (verb == "new" && t.size() >= 3) {
      // The name table must never diverge from the core pool: check the
      // bound BEFORE registering (the core would warn and drop it).
      if (m_seqs.size() >= kMaxChordSequences) {
        error = "sequence pool is full";
        return false;
      }
      c.param = Param::kSeqNew;
      m_engine.push_command(c, m_sink);
      m_seqs.push_back(t[2]);
      return true;
    }
    if (verb == "use" && t.size() >= 3) {
      const int idx = find_seq(t[2]);
      if (idx < 0) {
        error = "unknown sequence: " + t[2];
        return false;
      }
      c.param = Param::kSeqUse;
      c.idx = static_cast<std::uint16_t>(idx);
      m_engine.push_command(c, m_sink);
      return true;
    }
    if (verb == "rec") {
      c.param = Param::kSeqRec;
      m_engine.push_command(c, m_sink);
      return true;
    }
    if (verb == "stop") {
      c.param = Param::kSeqStop;
      m_engine.push_command(c, m_sink);
      return true;
    }
    if (verb == "add" && t.size() >= 3) {
      // seq add <note> [quality] [Nbars|Nbeats]
      std::uint8_t note = 0;
      if (!parse_note(t[2], note)) {
        error = "bad note: " + t[2];
        return false;
      }
      std::int8_t quality = -1;
      std::uint64_t dur = kTicksPerBar;
      std::size_t next = 3;
      if (next < t.size() && parse_quality(t[next], quality)) {
        ++next;
      }
      if (next < t.size() && !parse_duration(t[next], dur)) {
        error = "bad duration (Nbars/Nbeats): " + t[next];
        return false;
      }
      c.param = Param::kSeqAdd;
      c.a = note;
      c.b = (quality + 1) | (100 << 8);
      c.c = static_cast<std::int32_t>(dur);
      m_engine.push_command(c, m_sink);
      return true;
    }
    if (verb == "loop" && t.size() >= 3 && (t[2] == "on" || t[2] == "off")) {
      c.op = Op::kSet;
      c.param = Param::kSeqLoop;
      c.a = t[2] == "on" ? 1 : 0;
      m_engine.push_command(c, m_sink);
      return true;
    }
    if (verb == "play") {
      c.param = Param::kSeqPlay;
      m_engine.push_command(c, m_sink);
      return true;
    }
    if (verb == "transpose" && t.size() >= 3) {
      c.op = Op::kSet;
      c.param = Param::kSeqTranspose;
      if (t[2] == "to" && t.size() >= 4) {
        std::uint8_t root = 0;
        if (!parse_pc(t[3], root)) {
          error = "bad key root: " + t[3];
          return false;
        }
        Mode mode = Mode::kMajor;
        c.a = root;
        c.b = (t.size() >= 5 && parse_mode(t[4], mode)) ? static_cast<std::int32_t>(mode) : -1;
        // Spelling follows the new key when the mode is known.
        if (c.b >= 0) {
          m_prefer_flats = key_prefers_flats(root, mode);
        }
      } else {
        char* end = nullptr;
        const long delta = std::strtol(t[2].c_str(), &end, 10);
        if (end == nullptr || *end != '\0' || delta == 0 || delta < -11 || delta > 11) {
          error = "bad transpose (use to <root> or +/-N): " + t[2];
          return false;
        }
        c.a = -1;
        c.c = static_cast<std::int32_t>(delta);
      }
      m_engine.push_command(c, m_sink);
      return true;
    }
    if (verb == "del" && t.size() >= 3) {
      std::uint64_t idx = 0;
      if (!parse_u64(t[2], idx) || idx < 1) {
        error = "bad step index: " + t[2];
        return false;
      }
      c.param = Param::kSeqDel;
      c.a = static_cast<std::int32_t>(idx - 1);  // CLI is 1-based
      m_engine.push_command(c, m_sink);
      return true;
    }
    if (verb == "clear") {
      c.param = Param::kSeqClear;
      m_engine.push_command(c, m_sink);
      return true;
    }
    error = "seq new|use|rec|stop|add|loop|play|transpose|del|clear ...";
    return false;
  }

  if (cmd == "track" && t.size() >= 3) {
    const std::string& verb = t[1];

    if (verb == "new" && t.size() >= 4) {
      // track new <name> <out-port>[:ch] [role]
      std::string port_name;
      int channel = -1;
      if (!split_port_channel(t[3], port_name, channel)) {
        error = "bad track destination: " + t[3];
        return false;
      }
      const int port = find_port(port_name, false);
      if (port < 0) {
        error = "unknown output port: " + port_name;
        return false;
      }
      TrackRole role = TrackRole::kLead;
      if (t.size() >= 5 && !parse_role(t[4], role)) {
        error = "unknown role: " + t[4];
        return false;
      }
      // The name table must never diverge from the core pool: check the
      // bound BEFORE registering (the core would warn and drop it).
      if (m_tracks.size() >= kMaxTracks) {
        error = "track pool is full";
        return false;
      }
      Command c;
      c.param = Param::kTrackNew;
      c.a = static_cast<std::int32_t>(role);
      c.b = port | ((channel < 0 ? 0 : channel) << 8);
      m_engine.push_command(c, m_sink);
      m_tracks.push_back(t[2]);
      return true;
    }

    const int track = find_track(t[2]);
    if (track < 0) {
      error = "unknown track: " + t[2];
      return false;
    }

    if (verb == "step" && t.size() >= 5) {
      // track step <name> <step#> <note|clear> [vel] [gate]
      std::uint64_t step = 0;
      if (!parse_u64(t[3], step) || step < 1 || step > kMaxStepsPerTrack) {
        error = "bad step number: " + t[3];
        return false;
      }
      Command c;
      c.param = Param::kTrackStep;
      c.idx = static_cast<std::uint16_t>(track);
      c.a = static_cast<std::int32_t>(step - 1);  // CLI is 1-based
      if (t[4] == "clear") {
        c.b = 0;
        c.c = 0;
      } else {
        std::uint8_t note = 0;
        std::uint64_t vel = 100, gate = kTicksPerStep / 2;
        if (!parse_note(t[4], note)) {
          error = "bad note: " + t[4];
          return false;
        }
        if (t.size() >= 6 && (!parse_u64(t[5], vel) || vel < 1 || vel > 127)) {
          error = "bad velocity: " + t[5];
          return false;
        }
        if (t.size() >= 7 && (!parse_u64(t[6], gate) || gate == 0 || gate > 0xFFFF)) {
          error = "bad gate: " + t[6];
          return false;
        }
        c.b = note | (static_cast<std::int32_t>(vel) << 8);
        c.c = static_cast<std::int32_t>(gate);
      }
      m_engine.push_command(c, m_sink);
      return true;
    }

    if (verb == "length" && t.size() >= 4) {
      std::uint64_t steps = 0;
      if (!parse_u64(t[3], steps)) {
        error = "bad length: " + t[3];
        return false;
      }
      Command c;
      c.op = Op::kSet;
      c.param = Param::kTrackLength;
      c.idx = static_cast<std::uint16_t>(track);
      c.a = static_cast<std::int32_t>(steps);
      m_engine.push_command(c, m_sink);
      return true;
    }

    if ((verb == "mute" || verb == "solo") && t.size() >= 4) {
      if (t[3] != "on" && t[3] != "off") {
        error = "track mute|solo <name> on|off";
        return false;
      }
      Command c;
      c.op = Op::kSet;
      c.param = verb == "mute" ? Param::kTrackMute : Param::kTrackSolo;
      c.idx = static_cast<std::uint16_t>(track);
      c.a = t[3] == "on" ? 1 : 0;
      m_engine.push_command(c, m_sink);
      return true;
    }

    error = "track new|step|length|mute|solo ...";
    return false;
  }

  if (cmd == "panic") {
    Command c;
    c.param = Param::kPanic;
    m_engine.push_command(c, m_sink);
    return true;
  }

  if (cmd == "advance" && t.size() >= 2) {
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

  error = "unknown command: " + cmd;
  return false;
}

}  // namespace arrangrr::host
