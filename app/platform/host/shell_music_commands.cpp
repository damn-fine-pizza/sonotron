#include "shell.hpp"
#include "shell_internal.hpp"

#include <cstdlib>

#include "gm_program.hpp"

// Shell command handlers for the musical surface: key, chord/play, style,
// sequences and step tracks. Bodies moved verbatim from shell.cpp; see
// shell_io_commands.cpp for the MIDI/transport I/O commands.

namespace arrangrr::host {

using namespace shell_detail;

namespace {

// Structural label of a section type (the inverse of parse_section). The
// spellings match parse_section, so a listing round-trips through
// `style section <name>`.
const char* section_type_name(SectionType type) {
  switch (type) {
    case SectionType::kIntro1:
      return "intro1";
    case SectionType::kIntro2:
      return "intro2";
    case SectionType::kVarA:
      return "varA";
    case SectionType::kVarB:
      return "varB";
    case SectionType::kVarC:
      return "varC";
    case SectionType::kVarD:
      return "varD";
    case SectionType::kFillA:
      return "fillA";
    case SectionType::kFillB:
      return "fillB";
    case SectionType::kFillC:
      return "fillC";
    case SectionType::kFillD:
      return "fillD";
    case SectionType::kBreak:
      return "break";
    case SectionType::kEnding1:
      return "ending1";
    case SectionType::kEnding2:
      return "ending2";
  }
  return "?";
}

}  // namespace

bool Shell::cmd_key(const std::vector<std::string>& t, std::string& error) {
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

bool Shell::cmd_play(const std::vector<std::string>& t, std::string& error) {
  const std::size_t base = t[0] == "play" ? 1 : 2;
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

bool Shell::cmd_chord(const std::vector<std::string>& t, std::string& error) {
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
  if (t[1] == "detect" && t.size() >= 3 && (t[2] == "on" || t[2] == "off")) {
    // Live piano->chord: held keys on the piano input port re-harmonize the
    // arranger (whole-keyboard mode, chord-memory hold-last).
    const bool on = t[2] == "on";
    Command c;
    c.op = Op::kSet;
    c.param = Param::kChordDetect;
    c.a = on ? 1 : 0;
    c.b = kPianoInputPort;
    m_engine.push_command(c, m_sink);
    console_output(on ? "chord detect: on (play a chord to steer the band)"
                      : "chord detect: off");
    (void)push_panels();  // reflect the new detect state in the chords panel now
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

bool Shell::cmd_program(const std::vector<std::string>& t, std::string& error) {
  // program <port>[:ch] <GM voice>   e.g. program synth:2 trumpet | program synth 40
  std::string port_name;
  int channel = -1;
  if (!split_port_channel(t[1], port_name, channel)) {
    error = "bad program destination: " + t[1];
    return false;
  }
  const int port = find_port(port_name, false);
  if (port < 0) {
    error = "unknown output port: " + port_name;
    return false;
  }
  // Join the trailing tokens so multi-word GM names survive the tokenizer
  // ("program synth electric piano 1"); parse_gm_program also takes a number.
  std::string voice = t[2];
  for (std::size_t i = 3; i < t.size(); ++i) {
    voice += ' ';
    voice += t[i];
  }
  const int program = parse_gm_program(voice);
  if (program < 0) {
    error = "unknown GM voice: " + voice;
    return false;
  }
  const int ch = channel < 0 ? 0 : channel;  // default channel 1 (0-based 0)
  Command c;
  c.op = Op::kSet;
  c.param = Param::kProgram;
  c.a = program;
  c.b = port | (ch << 8);
  m_engine.push_command(c, m_sink);
  console_output("program " + port_name + ":" + std::to_string(ch + 1) + " -> " +
                 std::to_string(program) + " " + gm_program_name(static_cast<std::uint8_t>(program)));
  return true;
}

bool Shell::cmd_part(const std::vector<std::string>& t, std::string& error) {
  // part <role> mute|solo on|off   — the arranger-band mixer from the CLI.
  if (t.size() < 4) {
    error = "part <role> mute|solo on|off";
    return false;
  }
  TrackRole role = TrackRole::kLead;
  if (!parse_role(t[1], role)) {
    error = "unknown role: " + t[1];
    return false;
  }
  Param param = Param::kPartMute;
  if (t[2] == "solo") {
    param = Param::kPartSolo;
  } else if (t[2] != "mute") {
    error = "part <role> mute|solo on|off";
    return false;
  }
  if (t[3] != "on" && t[3] != "off") {
    error = "part <role> mute|solo on|off";
    return false;
  }
  Command c;
  c.op = Op::kSet;
  c.param = param;
  c.a = static_cast<std::int32_t>(role);
  c.b = t[3] == "on" ? 1 : 0;
  m_engine.push_command(c, m_sink);
  (void)push_panels();
  return true;
}

bool Shell::cmd_style(const std::vector<std::string>& t, std::string& error) {
  const std::string& verb = t[1];

  // Prints one line per section of `style`. `mark_current` annotates the
  // arranger's active section (only meaningful for the loaded style).
  auto print_sections = [this](const Style& style, bool mark_current) {
    const SectionType current = m_engine.arranger().current();
    const bool loaded = m_engine.arranger().loaded();
    for (const StyleSection& s : style.sections) {
      std::string line = section_type_name(s.type);
      if (mark_current && loaded && current == s.type) {
        line += "  (current)";
      }
      print_line(line);
    }
  };

  // `style list` — every builtin as `index  name`.
  if (verb == "list") {
    for (std::uint8_t i = 0; i < styles::kBuiltinCount; ++i) {
      print_line(std::to_string(static_cast<int>(i)) + "  " + styles::kBuiltins[i]->name);
    }
    return true;
  }

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
    // `style section list` — the CURRENTLY loaded style's sections. The shell
    // tracks no current-style index and the arranger exposes no style pointer,
    // so with a single builtin the loaded style is kBuiltins[0].
    if (t[2] == "list") {
      print_sections(*styles::kBuiltins[0], /*mark_current=*/true);
      return true;
    }
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
  // `style <name> section list` — a named builtin style's sections.
  if (t.size() >= 4 && t[2] == "section" && t[3] == "list") {
    const int idx = find_builtin_style(verb);
    if (idx < 0) {
      error = "unknown style: " + verb;
      return false;
    }
    print_sections(*styles::kBuiltins[idx], /*mark_current=*/false);
    return true;
  }
  error = "style list | load|route|section ... | [<name>] section list";
  return false;
}

bool Shell::seq_add(const std::vector<std::string>& t, std::string& error) {
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
  Command c;
  c.param = Param::kSeqAdd;
  c.a = note;
  c.b = (quality + 1) | (100 << 8);
  c.c = static_cast<std::int32_t>(dur);
  m_engine.push_command(c, m_sink);
  return true;
}

bool Shell::seq_del(const std::vector<std::string>& t, std::string& error) {
  std::uint64_t idx = 0;
  if (!parse_u64(t[2], idx) || idx < 1) {
    error = "bad step index: " + t[2];
    return false;
  }
  Command c;
  c.param = Param::kSeqDel;
  c.a = static_cast<std::int32_t>(idx - 1);  // CLI is 1-based
  m_engine.push_command(c, m_sink);
  return true;
}

bool Shell::seq_transpose(const std::vector<std::string>& t, std::string& error) {
  Command c;
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

bool Shell::cmd_seq(const std::vector<std::string>& t, std::string& error) {
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
    return seq_add(t, error);
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
    return seq_transpose(t, error);
  }
  if (verb == "del" && t.size() >= 3) {
    return seq_del(t, error);
  }
  if (verb == "clear") {
    c.param = Param::kSeqClear;
    m_engine.push_command(c, m_sink);
    return true;
  }
  error = "seq new|use|rec|stop|add|loop|play|transpose|del|clear ...";
  return false;
}

bool Shell::track_new(const std::vector<std::string>& t, std::string& error) {
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

bool Shell::track_step(const std::vector<std::string>& t, int track, std::string& error) {
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

bool Shell::cmd_track(const std::vector<std::string>& t, std::string& error) {
  const std::string& verb = t[1];

  if (verb == "new" && t.size() >= 4) {
    return track_new(t, error);
  }

  const int track = find_track(t[2]);
  if (track < 0) {
    error = "unknown track: " + t[2];
    return false;
  }

  if (verb == "step" && t.size() >= 5) {
    return track_step(t, track, error);
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

}  // namespace arrangrr::host
