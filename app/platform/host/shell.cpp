#include "shell.hpp"

#include <algorithm>
#include <cstdlib>

namespace arrangrr::host {

namespace {

std::vector<std::string> tokenize(const std::string& line) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : line) {
    // '#' opens a comment only at the start of a token — mid-token it is a
    // sharp (F#3). A comment therefore needs whitespace before it.
    if (c == '#' && cur.empty()) break;
    if (c == ' ' || c == '\t') {
      if (!cur.empty()) out.push_back(std::move(cur)), cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  if (!cur.empty()) out.push_back(std::move(cur));
  return out;
}

bool parse_u64(const std::string& s, std::uint64_t& out) {
  if (s.empty()) return false;
  char* end = nullptr;
  out = std::strtoull(s.c_str(), &end, 10);
  return end && *end == '\0';
}

// "120" or "120.5" or "120.50" -> bpm_x100.
bool parse_bpm_x100(const std::string& s, std::uint32_t& out) {
  const auto dot = s.find('.');
  std::uint64_t whole = 0, frac = 0;
  if (dot == std::string::npos) {
    if (!parse_u64(s, whole)) return false;
  } else {
    std::string f = s.substr(dot + 1);
    if (f.empty() || f.size() > 2) return false;
    if (!parse_u64(s.substr(0, dot), whole) || !parse_u64(f, frac)) return false;
    if (f.size() == 1) frac *= 10;
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
  if (!parse_u64(s.substr(colon + 1), ch) || ch < 1 || ch > 16) return false;
  channel = static_cast<int>(ch - 1);
  return true;
}

// Note name in scientific pitch notation (C4 = 60): letter, optional #/b,
// octave -1..9. Plain MIDI numbers are accepted too.
bool parse_note(const std::string& s, std::uint8_t& out) {
  std::uint64_t raw = 0;
  if (parse_u64(s, raw)) {
    if (raw > 127) return false;
    out = static_cast<std::uint8_t>(raw);
    return true;
  }
  if (s.empty()) return false;
  static constexpr int kSemis[7] = {9, 11, 0, 2, 4, 5, 7};  // A B C D E F G
  const char letter = s[0];
  if (letter < 'A' || letter > 'G') return false;
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
    if (dflt < 0 || dflt > 127) return false;
    out = static_cast<std::uint8_t>(dflt);
    return true;
  }
  bool negative = false;
  if (s[pos] == '-') {
    negative = true;
    ++pos;
  }
  std::uint64_t octave = 0;
  if (!parse_u64(s.substr(pos), octave) || octave > 9) return false;
  const int oct = negative ? -static_cast<int>(octave) : static_cast<int>(octave);
  if (oct < -1) return false;
  const int note = (oct + 1) * 12 + semi;
  if (note < 0 || note > 127) return false;
  out = static_cast<std::uint8_t>(note);
  return true;
}

// Pitch class only (key roots): letter + optional #/b.
bool parse_pc(const std::string& s, std::uint8_t& out) {
  if (s.empty()) return false;
  static constexpr int kSemis[7] = {9, 11, 0, 2, 4, 5, 7};
  if (s[0] < 'A' || s[0] > 'G') return false;
  int semi = kSemis[s[0] - 'A'];
  if (s.size() == 2) {
    if (s[1] == '#') ++semi;
    else if (s[1] == 'b') --semi;
    else return false;
  } else if (s.size() > 2) {
    return false;
  }
  out = static_cast<std::uint8_t>((semi + 12) % 12);
  return true;
}

bool parse_mode(const std::string& s, Mode& out) {
  struct Entry { const char* name; Mode mode; };
  static constexpr Entry kModes[] = {
      {"major", Mode::kMajor},     {"minor", Mode::kMinor},
      {"dorian", Mode::kDorian},   {"phrygian", Mode::kPhrygian},
      {"lydian", Mode::kLydian},   {"mixolydian", Mode::kMixolydian},
      {"locrian", Mode::kLocrian},
  };
  for (const Entry& e : kModes)
    if (s == e.name) { out = e.mode; return true; }
  return false;
}

bool parse_quality(const std::string& s, std::int8_t& out) {
  struct Entry { const char* name; ChordQuality q; };
  static constexpr Entry kQ[] = {
      {"maj", ChordQuality::kMaj},       {"min", ChordQuality::kMin},
      {"dim", ChordQuality::kDim},       {"aug", ChordQuality::kAug},
      {"maj7", ChordQuality::kMaj7},     {"min7", ChordQuality::kMin7},
      {"m7", ChordQuality::kMin7},       {"7", ChordQuality::kDom7},
      {"dom7", ChordQuality::kDom7},     {"m7b5", ChordQuality::kHalfDim7},
      {"halfdim", ChordQuality::kHalfDim7}, {"dim7", ChordQuality::kDim7},
      {"sus2", ChordQuality::kSus2},     {"sus4", ChordQuality::kSus4},
  };
  for (const Entry& e : kQ)
    if (s == e.name) { out = static_cast<std::int8_t>(e.q); return true; }
  return false;
}

// Flat-side keys spell with flats (Bb, Eb, ...): true when the parent major
// signature has flats. Parent major root = key root minus the mode's offset.
bool key_prefers_flats(std::uint8_t root_pc, Mode mode) {
  static constexpr std::uint8_t kOffset[7] = {0, 9, 2, 4, 5, 7, 11};
  const std::uint8_t parent =
      static_cast<std::uint8_t>((root_pc + 12 - kOffset[static_cast<int>(mode)]) % 12);
  return parent == 5 || parent == 10 || parent == 3 || parent == 8 || parent == 1 ||
         parent == 6;
}

// "2bars" / "1bar" / "4beats" / "1beat" -> ticks.
bool parse_duration(const std::string& s, std::uint64_t& out_ticks) {
  auto strip = [&](const char* suffix, std::uint64_t mult) {
    const std::size_t n = std::string(suffix).size();
    if (s.size() <= n || s.substr(s.size() - n) != suffix) return false;
    std::uint64_t v = 0;
    if (!parse_u64(s.substr(0, s.size() - n), v) || v == 0) return false;
    out_ticks = v * mult;
    return true;
  };
  return strip("bars", kTicksPerBar) || strip("bar", kTicksPerBar) ||
         strip("beats", kTicksPerBeat) || strip("beat", kTicksPerBeat);
}

bool parse_role(const std::string& s, TrackRole& out) {
  struct Entry {
    const char* name;
    TrackRole role;
  };
  static constexpr Entry kRoles[] = {
      {"drums", TrackRole::kDrums},   {"perc", TrackRole::kPerc},
      {"bass", TrackRole::kBass},     {"chord1", TrackRole::kChord1},
      {"chord2", TrackRole::kChord2}, {"pad", TrackRole::kPad},
      {"arp", TrackRole::kArp},       {"phrase", TrackRole::kPhrase},
      {"lead", TrackRole::kLead},     {"cc", TrackRole::kCc},
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
  if (s.empty() || s.size() > 2) return false;
  char* end = nullptr;
  const unsigned long v = std::strtoul(s.c_str(), &end, 16);
  if (!end || *end != '\0' || v > 0xFF) return false;
  out = static_cast<std::uint8_t>(v);
  return true;
}

}  // namespace

int Shell::find_seq(const std::string& name) const {
  for (std::size_t i = 0; i < seqs_.size(); ++i) {
    if (seqs_[i] == name) return static_cast<int>(i);
  }
  std::uint64_t idx = 0;
  if (parse_u64(name, idx) && idx < seqs_.size()) return static_cast<int>(idx);
  return -1;
}

int Shell::find_track(const std::string& name) const {
  for (std::size_t i = 0; i < tracks_.size(); ++i) {
    if (tracks_[i] == name) return static_cast<int>(i);
  }
  std::uint64_t idx = 0;
  if (parse_u64(name, idx) && idx < tracks_.size()) return static_cast<int>(idx);
  return -1;
}

int Shell::find_port(const std::string& name, bool input) const {
  for (const PortDef& p : ports_) {
    if (p.name == name && p.is_input == input) return p.index;
  }
  // Bare numeric index is accepted too.
  std::uint64_t idx = 0;
  if (parse_u64(name, idx) && idx < kMaxPorts) return static_cast<int>(idx);
  return -1;
}

bool Shell::exec_line(const std::string& line, std::string& error) {
  std::vector<std::string> tokens = tokenize(line);
  if (tokens.empty()) return true;

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
    if (tick < engine_.now()) {
      error = "@tick in the past";
      return false;
    }
    pending_.push_back(Pending{tick, pending_order_++, std::move(tokens)});
    return true;
  }

  return exec_now(tokens, error);
}

bool Shell::advance_to(std::uint64_t target, std::string& error) {
  while (true) {
    // Earliest pending line at or before target (stable by insertion order).
    auto next = pending_.end();
    for (auto it = pending_.begin(); it != pending_.end(); ++it) {
      if (it->tick > target) continue;
      if (next == pending_.end() || it->tick < next->tick ||
          (it->tick == next->tick && it->order < next->order)) {
        next = it;
      }
    }
    if (next == pending_.end()) break;
    const std::uint64_t at = next->tick;
    if (at > engine_.now()) {
      engine_.advance_ticks(static_cast<std::uint32_t>(at - engine_.now()), sink_);
    }
    std::vector<std::string> tokens = std::move(next->tokens);
    pending_.erase(next);
    if (!exec_now(tokens, error)) return false;
  }
  if (target > engine_.now()) {
    engine_.advance_ticks(static_cast<std::uint32_t>(target - engine_.now()), sink_);
  }
  return true;
}

bool Shell::exec_now(const std::vector<std::string>& t, std::string& error) {
  const std::string& cmd = t[0];

  if (cmd == "quit" || cmd == "exit") {
    quit_ = true;
    return true;
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
    if (t.size() >= 6 && t[4] == "as") name = t[5];
    std::uint8_t& next = input ? next_in_ : next_out_;
    if (next >= kMaxPorts) {
      error = "no free port slots";
      return false;
    }
    const PortDef def{name, input, next++};
    ports_.push_back(def);
    if (port_hook_) port_hook_(def);
    return true;
  }

  if (cmd == "transport" && t.size() >= 2) {
    Command c;
    if (t[1] == "start")
      c.param = Param::kTransportStart;
    else if (t[1] == "stop")
      c.param = Param::kTransportStop;
    else if (t[1] == "continue")
      c.param = Param::kTransportContinue;
    else if (t[1] == "tempo" && t.size() >= 3) {
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
    engine_.push_command(c, sink_);
    return true;
  }

  if (cmd == "route" && t.size() == 4 && t[2] == "->") {
    // route <in>[:ch] -> <out>[:ch]
    std::string in_name, out_name;
    int in_ch = -1, out_ch = -1;
    if (!split_port_channel(t[1], in_name, in_ch) ||
        !split_port_channel(t[3], out_name, out_ch)) {
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
    engine_.push_command(c, sink_);
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
    engine_.push_command(c, sink_);
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
    engine_.push_command(c, sink_);
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
    engine_.push_midi_in(static_cast<std::uint8_t>(port),
                         Span<const std::uint8_t>(bytes.data(), bytes.size()), sink_);
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
    prefer_flats_ = key_prefers_flats(root, mode);
    Command c;
    c.op = Op::kSet;
    c.param = Param::kKeySet;
    c.a = root;
    c.b = static_cast<std::int32_t>(mode);
    engine_.push_command(c, sink_);
    return true;
  }

  if ((cmd == "play" && t.size() >= 2) ||
      (cmd == "chord" && t.size() >= 3 && t[1] == "play")) {
    const std::size_t base = cmd == "play" ? 1 : 2;
    std::uint8_t note = 0;
    if (!parse_note(t[base], note)) {
      error = "bad note: " + t[base];
      return false;
    }
    std::int8_t quality = -1;
    std::uint64_t vel = 100;
    std::size_t next = base + 1;
    if (next < t.size() && parse_quality(t[next], quality)) ++next;
    if (next < t.size() && (!parse_u64(t[next], vel) || vel < 1 || vel > 127)) {
      error = "bad velocity: " + t[next];
      return false;
    }
    Command c;
    c.param = Param::kChordPlay;
    c.a = note;
    c.b = quality;
    c.c = static_cast<std::int32_t>(vel);
    engine_.push_command(c, sink_);
    return true;
  }

  if (cmd == "chord" && t.size() >= 2) {
    if (t[1] == "stop") {
      Command c;
      c.param = Param::kChordStop;
      engine_.push_command(c, sink_);
      return true;
    }
    if (t[1] == "hold" && t.size() >= 3 && (t[2] == "on" || t[2] == "off")) {
      Command c;
      c.op = Op::kSet;
      c.param = Param::kChordHold;
      c.a = t[2] == "on" ? 1 : 0;
      engine_.push_command(c, sink_);
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
      engine_.push_command(c, sink_);
      return true;
    }
    error = "chord play|stop|hold|out ...";
    return false;
  }

  if (cmd == "seq" && t.size() >= 2) {
    const std::string& verb = t[1];
    Command c;

    if (verb == "new" && t.size() >= 3) {
      c.param = Param::kSeqNew;
      engine_.push_command(c, sink_);
      seqs_.push_back(t[2]);
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
      engine_.push_command(c, sink_);
      return true;
    }
    if (verb == "rec") {
      c.param = Param::kSeqRec;
      engine_.push_command(c, sink_);
      return true;
    }
    if (verb == "stop") {
      c.param = Param::kSeqStop;
      engine_.push_command(c, sink_);
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
      if (next < t.size() && parse_quality(t[next], quality)) ++next;
      if (next < t.size() && !parse_duration(t[next], dur)) {
        error = "bad duration (Nbars/Nbeats): " + t[next];
        return false;
      }
      c.param = Param::kSeqAdd;
      c.a = note;
      c.b = (quality + 1) | (100 << 8);
      c.c = static_cast<std::int32_t>(dur);
      engine_.push_command(c, sink_);
      return true;
    }
    if (verb == "loop" && t.size() >= 3 && (t[2] == "on" || t[2] == "off")) {
      c.op = Op::kSet;
      c.param = Param::kSeqLoop;
      c.a = t[2] == "on" ? 1 : 0;
      engine_.push_command(c, sink_);
      return true;
    }
    if (verb == "play") {
      c.param = Param::kSeqPlay;
      engine_.push_command(c, sink_);
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
        c.b = (t.size() >= 5 && parse_mode(t[4], mode)) ? static_cast<std::int32_t>(mode)
                                                        : -1;
        // Spelling follows the new key when the mode is known.
        if (c.b >= 0) prefer_flats_ = key_prefers_flats(root, mode);
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
      engine_.push_command(c, sink_);
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
      engine_.push_command(c, sink_);
      return true;
    }
    if (verb == "clear") {
      c.param = Param::kSeqClear;
      engine_.push_command(c, sink_);
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
      Command c;
      c.param = Param::kTrackNew;
      c.a = static_cast<std::int32_t>(role);
      c.b = port | ((channel < 0 ? 0 : channel) << 8);
      engine_.push_command(c, sink_);
      tracks_.push_back(t[2]);
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
      engine_.push_command(c, sink_);
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
      engine_.push_command(c, sink_);
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
      engine_.push_command(c, sink_);
      return true;
    }

    error = "track new|step|length|mute|solo ...";
    return false;
  }

  if (cmd == "panic") {
    Command c;
    c.param = Param::kPanic;
    engine_.push_command(c, sink_);
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
    return advance_to(engine_.now() + n * mult, error);
  }

  error = "unknown command: " + cmd;
  return false;
}

}  // namespace arrangrr::host
