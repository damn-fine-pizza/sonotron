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

// Parses one `key=value` step param-lock token into its destination slot.
// Returns false and fills `error` on an unknown key or an out-of-range value;
// the caller owns the '=' split and the positional vel/gate tokens. Keeping the
// per-key validation here drops the cognitive complexity of track_step below the
// clang-tidy threshold. micro is FORWARD-only (0..127): a lay-back behind the
// beat — anticipation is deferred (it needs step look-ahead), so a negative
// value is not accepted.
bool parse_step_lock(const std::string& key, const std::string& val, std::uint64_t& probability,
                     std::uint64_t& ratchet, std::uint64_t& micro, bool& tie, std::string& error) {
  if (key == "prob") {
    if (!parse_u64(val, probability) || probability > 100) {
      error = "bad prob (0..100): " + val;
      return false;
    }
  } else if (key == "ratchet") {
    if (!parse_u64(val, ratchet) || ratchet < 1 || ratchet > kMaxRatchet) {
      error = "bad ratchet (1..8): " + val;
      return false;
    }
  } else if (key == "micro") {
    if (!parse_u64(val, micro) || micro > 127) {
      error = "bad micro (0..127, forward-only): " + val;
      return false;
    }
  } else if (key == "tie") {
    if (val == "on") {
      tie = true;
    } else if (val == "off") {
      tie = false;
    } else {
      error = "bad tie (on|off): " + val;
      return false;
    }
  } else {
    error = "track step: unknown param-lock: " + key;
    return false;
  }
  return true;
}

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

// See shell.hpp's declaration comment: thin wrappers over the shell_detail
// lookups both cmd_style()'s "load" verb and cmd_part() already call, exposed
// so a caller without a Shell instance can build the identical Command POD.
int Shell::resolve_style_index(const std::string& name) { return find_builtin_style(name); }

bool Shell::resolve_track_role(const std::string& name, TrackRole& out) {
  return parse_role(name, out);
}

// `restyle <style> [role]` (roadmap 9320): selects the target style the
// RestyleStage transforms the imported melody's own notes into -- a
// load-time L1 verb mirroring `style load <name>`'s own builtin-name
// resolution (docs/design/restyle-placement.md §3: "no ABI Command").
// RestyleStage is a Pipeline-level peer, not part of arrangrr's terminal
// Engine, so this reaches it directly through the Pipeline's own stage<>
// accessor -- the same way load_midi_source() reaches the MIDI-source stage.
// Also flips the raw melody-thru OFF (the double-note guard, restyle-
// placement.md §2): once Restyle owns the transformed output, the raw thru
// replay must not sound the same note a second time.
//
// `[role]` (second slice): the optional 2nd argument overrides the target
// role RestyleStage's register anchor / VoicingPolicy read (kLead when
// omitted -- the ORIGINAL fixed behavior, so a script that never names one
// keeps its exact prior meaning). Reuses `resolve_track_role`, the SAME
// name table `parts`/track-role verbs already share.
//
// Channel filter (second slice, a real-bug fix -- restyle-musical-scope.md/
// -placement.md's first slice was channel-blind, transforming a drum hit
// that merely shared a pitch class with a chord tone right alongside actual
// melody notes): this verb is the one wiring point that resets the mask to
// its sensible default (excludes the GM drum channel, restyle_stage.hpp's
// own `restyle::kDefaultChannelMask`) on every invocation, so no stale mask
// from a prior `restyle` call can linger.
bool Shell::cmd_restyle(const std::vector<std::string>& t, std::string& error) {
  const int index = find_builtin_style(t[1]);
  if (index < 0) {
    error = "unknown style: " + t[1];
    return false;
  }
  TrackRole role = TrackRole::kLead;
  if (t.size() >= 3) {
    if (!resolve_track_role(t[2], role)) {
      error = "unknown role: " + t[2];
      return false;
    }
  }
  auto& restyle = m_runtime.stage().template stage<orchestrator::kRestyleStageIndex>();
  if (!restyle.load_style(styles::kBuiltins[index], role)) {
    error = "restyle load failed: " + t[1];
    return false;
  }
  restyle.set_channel_mask(arrangrr::restyle::kDefaultChannelMask);
  m_runtime.stage().template stage<orchestrator::kMidiSourceStageIndex>().set_thru_enabled(false);
  return true;
}

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

// Translates a kNoteRaw Command (docs/design/orchestrator-pipeline-
// extraction.md §17.3a) into the exact 3-byte MIDI note-on/off message
// surface_send_note() already builds in-process: `idx`'s low byte is the
// input port, its high byte the 0-based MIDI channel; `a` is the note,
// `b` the velocity, `c` the on/off flag. Never touches Engine::push_command
// -- this is a pure wire-shape-to-bytes translation, exactly like
// surface_send_note()'s own feed_midi() call. PUBLIC and STATIC (see
// shell.hpp's own declaration comment): hoisted out of this file's former
// anonymous namespace so in_process_brain_session.cpp's run_engine() can
// reach it too (docs/proposals/looper-in-gui-contract.md §7 item 2), without
// duplicating this logic.
void Shell::note_raw_to_bytes(const Command& c, std::uint8_t out[3]) {
  const auto channel = static_cast<std::uint8_t>((c.idx >> 8) & 0xFF);
  out[0] = static_cast<std::uint8_t>((c.c != 0 ? midi::kNoteOn : midi::kNoteOff) | channel);
  out[1] = static_cast<std::uint8_t>(c.a);
  out[2] = static_cast<std::uint8_t>(c.b);
}

bool Shell::cmd_note(const std::vector<std::string>& t, std::string& error) {
  // note <port>[:ch] on|off <midinote> [velocity]   -- the wire-safe
  // equivalent of a piano/chords key gesture (§17.3a): a pure client sends
  // this L1 line instead of building a raw Command/MIDI bytes itself.
  std::string port_name;
  int channel = -1;
  if (!split_port_channel(t[1], port_name, channel)) {
    error = "bad note port: " + t[1];
    return false;
  }
  const int port = find_port(port_name, true);  // an INPUT port (feed_midi target)
  if (port < 0) {
    error = "unknown input port: " + port_name;
    return false;
  }
  if (t[2] != "on" && t[2] != "off") {
    error = "note <port>[:ch] on|off <midinote> [velocity]";
    return false;
  }
  const bool on = t[2] == "on";
  std::uint8_t note = 0;
  if (!parse_note(t[3], note)) {
    error = "bad midi note: " + t[3];
    return false;
  }
  std::uint64_t vel = on ? 100 : kPianoReleaseVelocity;
  if (t.size() >= 5) {
    if (!parse_u64(t[4], vel) || vel < 1 || vel > 127) {
      error = "bad velocity: " + t[4];
      return false;
    }
  }
  Command c;
  c.op = Op::kDo;
  c.param = Param::kNoteRaw;
  c.idx = static_cast<std::uint16_t>(static_cast<std::uint16_t>(port) |
                                     (static_cast<std::uint16_t>(channel < 0 ? 0 : channel) << 8));
  c.a = note;
  c.b = static_cast<std::int32_t>(vel);
  c.c = on ? 1 : 0;

  std::uint8_t bytes[3];
  note_raw_to_bytes(c, bytes);
  feed_midi(static_cast<std::uint8_t>(c.idx & 0xFF),
            Span<const std::uint8_t>(bytes, sizeof(bytes)));
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
  // Optional velocity (skipped when the next token is the quantize keyword).
  if (next < t.size() && t[next] != "next" && t[next] != "shift") {
    if (!parse_u64(t[next], vel) || vel < 1 || vel > 127) {
      error = "bad velocity: " + t[next];
      return false;
    }
    ++next;
  }
  // A trailing `next` (or `shift`) STAGES the chord for the next bar, consistent
  // with a SHIFTed note-letter on the harmony surface; otherwise it is immediate.
  const bool quantize = next < t.size() && (t[next] == "next" || t[next] == "shift");
  Command c;
  c.param = Param::kChordPlay;
  c.boundary = quantize ? Boundary::kNextBar : Boundary::kImmediate;
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
    console_output(mode == 1 ? "chord mode: single (single-finger — one held key steers the band)"
                             : (mode == 2 ? "chord mode: shell" : "chord mode: diatonic"));
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
    // Live keys -> chord: held keys on the HARMONY surface (the chords panel, its
    // dedicated kHarmonyInputPort) re-harmonize the arranger. Under the two-zone
    // topology the harmony port is silent (kHarmony) and IS the detect source;
    // the piano port stays kMelody (sounds, never steers). Chord-memory hold-last.
    const bool on = t[2] == "on";
    Command c;
    c.op = Op::kSet;
    c.param = Param::kChordDetect;
    c.a = on ? 1 : 0;
    c.b = kHarmonyInputPort;
    m_engine.push_command(c, m_sink);
    console_output(on ? "chord detect: on (focus the chords panel and play to steer the band)"
                      : "chord detect: off");
    (void)push_panels();  // reflect the new detect state in the chords panel now
    return true;
  }
  if (t[1] == "follow") {
    // D47: pick which producer steers the band's harmony. auto = legacy
    // last-writer-wins; the others narrow it to a single named source.
    ChordFollow follow{};
    if (t.size() < 3 || !parse_chord_follow(t[2], follow)) {
      error = "chord follow auto|detect|sequencer|manual|live";
      return false;
    }
    Command c;
    c.op = Op::kSet;
    c.param = Param::kChordFollow;
    c.a = static_cast<std::int32_t>(follow);
    m_engine.push_command(c, m_sink);
    console_output(std::string("chord follow: ") + chord_follow_label(m_engine.chord_follow()) +
                   " (" + chord_follow_hint(m_engine.chord_follow()) + ")");
    (void)push_panels();  // reflect the new follow source in the chords panel now
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
  // Channel 10 (0-based 9) is the GM percussion channel: program numbers
  // there pick a DRUM KIT, not a melodic voice, so echo the kit name when
  // it is one of the canonical GM2 kit anchors -- a plain gm_program_name()
  // lookup would print a misleading melodic name (e.g. program 0 on channel
  // 10 is "Standard Kit", not "Acoustic Grand Piano").
  constexpr int kGmPercussionChannelZeroBased = 9;
  const char* kit_name = ch == kGmPercussionChannelZeroBased
                             ? gm_drum_kit_name(static_cast<std::uint8_t>(program))
                             : nullptr;
  const std::string voice_display =
      kit_name != nullptr ? std::string(kit_name)
                          : std::string(gm_program_name(static_cast<std::uint8_t>(program)));
  console_output("program " + port_name + ":" + std::to_string(ch + 1) + " -> " +
                 std::to_string(program) + " " + voice_display);
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

bool Shell::cmd_groove(const std::vector<std::string>& t, std::string& error) {
  // groove <field> <value>   — the arranger feel from the CLI.
  if (t.size() < 3) {
    error = "groove <swing|humanize-t|humanize-v|accent|grid|quantize|seed> <value>";
    return false;
  }
  GrooveField field = GrooveField::kSwing;
  const std::string& f = t[1];
  if (f == "swing") {
    field = GrooveField::kSwing;
  } else if (f == "humanize-t" || f == "humanize-timing") {
    field = GrooveField::kHumanizeTiming;
  } else if (f == "humanize-v" || f == "humanize-velocity") {
    field = GrooveField::kHumanizeVelocity;
  } else if (f == "accent") {
    field = GrooveField::kAccent;
  } else if (f == "grid") {
    field = GrooveField::kSwingGrid;
  } else if (f == "quantize" || f == "q") {
    field = GrooveField::kQuantize;
  } else if (f == "seed") {
    field = GrooveField::kSeed;
  } else {
    error = "unknown groove field: " + f;
    return false;
  }
  std::uint64_t value = 0;
  if (!parse_u64(t[2], value)) {
    error = "bad value: " + t[2];
    return false;
  }
  Command c;
  c.op = Op::kSet;
  c.param = Param::kGroove;
  c.a = static_cast<std::int32_t>(field);
  c.b = static_cast<std::int32_t>(value);
  m_engine.push_command(c, m_sink);
  (void)push_panels();
  return true;
}

// `transpose <-12..12>` — Phase-6 Theme 3 Item #1 (docs/reflections/phase6-
// theme3-master-transpose-scope.md): the live global transpose, a signed
// semitone offset applied late (the absolute note number) at both the
// arranger and the chord engine. The engine itself is the source of truth
// for the [-12, +12] bound (Engine::cmd_master_transpose); this parse only
// rejects an unparsable token, matching seq_transpose's own +/-N shape.
bool Shell::cmd_transpose(const std::vector<std::string>& t, std::string& error) {
  char* end = nullptr;
  const long semitones = std::strtol(t[1].c_str(), &end, 10);
  if (end == nullptr || *end != '\0' || semitones < -12 || semitones > 12) {
    error = "bad transpose (-12..12): " + t[1];
    return false;
  }
  Command c;
  c.op = Op::kSet;
  c.param = Param::kMasterTranspose;
  c.a = static_cast<std::int32_t>(semitones);
  m_engine.push_command(c, m_sink);
  return true;
}

bool Shell::cmd_arp(const std::vector<std::string>& t, std::string& error) {
  const std::string& sub = t[1];
  auto send = [&](ArpField field, std::int32_t value) {
    Command c;
    c.op = Op::kSet;
    c.param = Param::kArp;
    c.a = static_cast<std::int32_t>(field);
    c.b = value;
    m_engine.push_command(c, m_sink);
  };
  if (sub == "on" || sub == "off") {
    send(ArpField::kEnabled, sub == "on" ? 1 : 0);
    console_output(sub == "on" ? "arp: on (hold keys with transport running)" : "arp: off");
    (void)push_panels();
    return true;
  }
  if (sub == "out" && t.size() >= 3) {
    std::string port_name;
    int channel = -1;
    if (!split_port_channel(t[2], port_name, channel)) {
      error = "bad arp destination: " + t[2];
      return false;
    }
    const int port = find_port(port_name, false);
    if (port < 0) {
      error = "unknown output port: " + port_name;
      return false;
    }
    Command c;
    c.op = Op::kSet;
    c.param = Param::kArpOut;
    c.a = port | ((channel < 0 ? 0 : channel) << 8);
    m_engine.push_command(c, m_sink);
    (void)push_panels();
    return true;
  }
  if (t.size() < 3) {
    error = "arp on|off | rate|dir|octaves|gate|latch|seed <v> | out <port>[:ch]";
    return false;
  }
  const std::string& v = t[2];
  if (sub == "rate") {
    int r = (v == "1/4") ? 0 : (v == "1/8") ? 1 : (v == "1/16") ? 2 : (v == "1/32") ? 3 : -1;
    if (r < 0) {
      error = "arp rate 1/4|1/8|1/16|1/32";
      return false;
    }
    send(ArpField::kRate, r);
  } else if (sub == "dir" || sub == "direction") {
    int d = (v == "up")          ? 0
            : (v == "down")      ? 1
            : (v == "updown")    ? 2
            : (v == "downup")    ? 3
            : (v == "as-played") ? 4
            : (v == "random")    ? 5
                                 : -1;
    if (d < 0) {
      error = "arp dir up|down|updown|downup|as-played|random";
      return false;
    }
    send(ArpField::kDirection, d);
  } else if (sub == "octaves" || sub == "gate" || sub == "seed") {
    std::uint64_t n = 0;
    if (!parse_u64(v, n)) {
      error = "bad value: " + v;
      return false;
    }
    send(sub == "octaves" ? ArpField::kOctaves
         : sub == "gate"  ? ArpField::kGate
                          : ArpField::kSeed,
         static_cast<std::int32_t>(n));
  } else if (sub == "latch") {
    if (v != "on" && v != "off") {
      error = "arp latch on|off";
      return false;
    }
    send(ArpField::kLatch, v == "on" ? 1 : 0);
  } else {
    error = "unknown arp field: " + sub;
    return false;
  }
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
    // Built-in styles resolve by name host-side (D26): any of the 16 builtins,
    // case-insensitively, not just "basic".
    const int index = find_builtin_style(t[2]);
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
  // Phase 7 (node T0): the live bar length, not the compile-time kTicksPerBar.
  std::uint64_t dur = m_engine.transport().ticks_per_bar();
  std::size_t next = 3;
  if (next < t.size() && parse_quality(t[next], quality)) {
    ++next;
  }
  if (next < t.size() && !parse_duration(t[next], m_engine.transport().ticks_per_bar(), dur)) {
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
  //           [prob=0..100] [ratchet=1..8] [micro=0..127] [tie=on|off]
  // The four key=value param-locks are optional and order-free; omitting them
  // all keeps the original short form (and byte-identical playback).
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
    m_engine.push_command(c, m_sink);
    return true;
  }

  std::uint8_t note = 0;
  if (!parse_note(t[4], note)) {
    error = "bad note: " + t[4];
    return false;
  }
  std::uint64_t vel = 100;
  std::uint64_t gate = kTicksPerStep / 2;
  std::uint64_t probability = 100;
  std::uint64_t ratchet = 1;
  std::uint64_t micro = 0;
  bool tie = false;
  bool has_locks = false;

  // Only the '=' split and positional vel/gate dispatch live here; each
  // param-lock's own validation is delegated to parse_step_lock.
  int positional = 0;  // 0 -> vel, 1 -> gate
  for (std::size_t i = 5; i < t.size(); ++i) {
    const std::string& tok = t[i];
    const std::size_t eq = tok.find('=');
    if (eq == std::string::npos) {
      if (positional == 0) {
        if (!parse_u64(tok, vel) || vel < 1 || vel > 127) {
          error = "bad velocity: " + tok;
          return false;
        }
        positional = 1;
      } else if (positional == 1) {
        if (!parse_u64(tok, gate) || gate == 0 || gate > 0xFFFF) {
          error = "bad gate: " + tok;
          return false;
        }
        positional = 2;
      } else {
        error = "track step: unexpected token: " + tok;
        return false;
      }
      continue;
    }
    has_locks = true;
    if (!parse_step_lock(tok.substr(0, eq), tok.substr(eq + 1), probability, ratchet, micro, tie,
                         error)) {
      return false;
    }
  }

  c.b = note | (static_cast<std::int32_t>(vel) << 8);
  c.c = static_cast<std::int32_t>(gate);
  if (has_locks) {
    // Opt-in extended encoding (ABI kTrackStep): bit 31 of c flags the locks.
    c.b |= static_cast<std::int32_t>(probability << 16) | static_cast<std::int32_t>(ratchet << 24) |
           static_cast<std::int32_t>(tie ? (1u << 28) : 0u);
    c.c |= static_cast<std::int32_t>((static_cast<std::uint32_t>(micro) & 0xFFu) << 16) |
           static_cast<std::int32_t>(0x80000000u);
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
