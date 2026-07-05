#include "arrangrr/arranger/arranger.hpp"

#include <string_view>

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 512>;

// A minimal style exercising a NEW role (kPad) with a default GM voice, to test
// the per-role register anchor and gm_program emission independently of the
// (not-yet-enriched) builtins.
constexpr StyleEvent kVoiceBass[] = {{.step = 0, .tone = 0, .octave = 0, .vel = 100, .gate = 200}};
constexpr StyleEvent kVoicePad[] = {{.step = 0, .tone = 0, .octave = 0, .vel = 70, .gate = 3600}};
constexpr StylePattern kVoicePatterns[] = {
    {.role = TrackRole::kBass, .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kVoiceBass), .gm_program = 33},  // Fingered Bass
    {.role = TrackRole::kPad, .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kVoicePad), .gm_program = 89},  // Pad 2 (warm)
};
constexpr StyleSection kVoiceSections[] = {
    {.type = SectionType::kVarA, .bars = 1, .patterns = Span<const StylePattern>(kVoicePatterns)}};
constexpr Style kVoiceTestStyle{.name = "voicetest",
                                .sections = Span<const StyleSection>(kVoiceSections)};

void test_role_anchor_and_gm_voices() {
  Arranger arr;
  CHECK(arr.load_style(&kVoiceTestStyle));
  CHECK(arr.set_route(TrackRole::kBass, 0, 0));  // port 0, channel 1 (0-based 0)
  CHECK(arr.set_route(TrackRole::kPad, 0, 4));   // port 0, channel 5 (0-based 4)

  // emit_voices: one Program Change per routed role that declares a voice.
  int pc_bass = -1;
  int pc_pad = -1;
  arr.emit_voices([&](std::uint8_t, TickOffset, const MidiMessage& m) {
    if ((m.status & 0xF0) == 0xC0) {
      if ((m.status & 0x0F) == 0) {
        pc_bass = m.d1;
      }
      if ((m.status & 0x0F) == 4) {
        pc_pad = m.d1;
      }
    }
  });
  CHECK(pc_bass == 33);
  CHECK(pc_pad == 89);

  // Per-role anchor: over C major, bass root sits at 36 (C2), pad root at the
  // new pad register 48 (C3) — not piled onto the mid comp octave.
  const ChordState chord{.root_pc = 0, .quality = ChordQuality::kMaj, .valid = true};
  arr.on_transport_start();
  bool bass_c2 = false;
  bool pad_c3 = false;
  arr.on_tick(0, chord, [&](std::uint8_t, TickOffset, const MidiMessage& m) {
    if (m.type() == midi::kNoteOn) {
      if (m.channel() == 0 && m.d1 == 36) {
        bass_c2 = true;
      }
      if (m.channel() == 4 && m.d1 == 48) {
        pad_c3 = true;
      }
    }
  });
  CHECK(bass_c2);
  CHECK(pad_c3);
}

struct Band {
  Engine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0, Op op = Op::kDo) {
    Command command;
    command.op = op;
    command.param = p;
    command.a = a;
    command.b = b;
    command.c = c;
    e.push_command(command, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void advance(std::uint32_t n) {
    e.advance_ticks(n, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void setup_basic() {
    cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
    cmd(Param::kStyleLoad, 0);
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kDrums), 0 | (9 << 8), 0,
        Op::kSet);
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 0 | (1 << 8), 0, Op::kSet);
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kChord1), 0 | (2 << 8), 0,
        Op::kSet);
  }
  int ons(std::uint8_t channel, std::uint8_t note = 255) const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind != OutEvent::Kind::kMidi || o.msg.type() != midi::kNoteOn) {
        continue;
      }
      if (o.msg.channel() != channel) {
        continue;
      }
      if (note != 255 && o.msg.d1 != note) {
        continue;
      }
      ++n;
    }
    return n;
  }
  StaticVector<std::uint16_t, 16> sections() const {
    StaticVector<std::uint16_t, 16> out;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kSection) {
        CHECK(out.push_back(o.code));
      }
    }
    return out;
  }
};

void test_drums_play_without_chord_but_tonal_roles_wait() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kTransportStart);
  b.advance(kTicksPerBar - 1);
  CHECK(b.ons(9) > 0);   // drums are fixed: they groove chord-less
  CHECK(b.ons(1) == 0);  // bass silent until a chord exists (NTT needs one)
  CHECK(b.ons(2) == 0);
}

void test_ntt_resolution_follows_chord() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kChordPlay, 60, -1, 100);  // Cmaj7 in C major (I)
  b.ev.clear();
  b.cmd(Param::kTransportStart);
  b.advance(kTicksPerBar / 2 - 1);  // stop short of step 8 (root repeats there)
  // VarA bass: root (36 + 0 = C2 = 36) at step 0, fifth (36+7=43) at step 4.
  CHECK(b.ons(1, 36) == 1);
  CHECK(b.ons(1, 43) == 1);
  // Chord comp at step 0: Cmaj7 stack anchored at 60: 60 64 67 71.
  CHECK(b.ons(2, 60) == 1 && b.ons(2, 64) == 1 && b.ons(2, 67) == 1 && b.ons(2, 71) == 1);
  // Change chord to Dm7 (ii) and check the next bar's downbeat.
  b.cmd(Param::kChordPlay, 62, -1, 100);
  b.ev.clear();
  b.advance(kTicksPerBar);
  CHECK(b.ons(1, 38) >= 1);                       // bass root D2 = 38
  CHECK(b.ons(2, 62) >= 1 && b.ons(2, 65) >= 1);  // Dm7 comp: 62 65 69 72
}

void test_groove_apply() {
  GrooveParams p;
  // No groove: passthrough.
  GrooveOut g = groove::apply(p, 0, 2, 480, 100);
  CHECK(g.timing_offset == 0);
  CHECK(g.velocity == 100);

  // Swing pushes the off-8th (step 2) late; the downbeat (0) and on-8ths stay.
  p = GrooveParams{};
  p.swing = 100;
  CHECK(groove::apply(p, 0, 0, 0, 100).timing_offset == 0);
  CHECK(groove::apply(p, 0, 2, 0, 100).timing_offset > 0);
  CHECK(groove::apply(p, 0, 4, 0, 100).timing_offset == 0);
  // Swing grid 16 delays the odd 16ths instead.
  p.swing_grid = 16;
  CHECK(groove::apply(p, 0, 1, 0, 100).timing_offset > 0);

  // Accent lifts beat 1, softens beat 2.
  p = GrooveParams{};
  p.accent = 100;
  CHECK(groove::apply(p, 0, 0, 0, 80).velocity > 80);
  CHECK(groove::apply(p, 0, 4, 0, 80).velocity < 80);

  // Humanize is deterministic: same seed+position => identical result.
  p = GrooveParams{};
  p.humanize_velocity = 100;
  p.humanize_timing = 100;
  const GrooveOut a1 = groove::apply(p, 3, 5, 720, 90);
  const GrooveOut a2 = groove::apply(p, 3, 5, 720, 90);
  CHECK(a1.velocity == a2.velocity && a1.timing_offset == a2.timing_offset);
  GrooveParams q = p;
  q.seed = 999;
  const GrooveOut b = groove::apply(q, 3, 5, 720, 90);
  CHECK(b.velocity != a1.velocity || b.timing_offset != a1.timing_offset);

  // Velocity always clamped into 1..127.
  CHECK(groove::apply(p, 0, 3, 111, 1).velocity >= 1);
  CHECK(groove::apply(p, 0, 0, 222, 127).velocity <= 127);
}

void test_groove_command() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kGroove, static_cast<std::int32_t>(GrooveField::kSwing), 60, 0, Op::kSet);
  CHECK(b.e.arranger().groove_params().swing == 60);
  b.cmd(Param::kGroove, static_cast<std::int32_t>(GrooveField::kAccent), 200, 0, Op::kSet);
  CHECK(b.e.arranger().groove_params().accent == 100);  // clamped
  b.cmd(Param::kGroove, 99, 10, 0, Op::kSet);           // bad field -> warn, no change
}

void test_part_mute_solo() {
  {  // Mute silences one part; the others keep playing.
    Band b;
    b.setup_basic();
    b.cmd(Param::kChordPlay, 60, -1, 100);  // C: tonal roles have a chord to sound
    b.cmd(Param::kPartMute, static_cast<std::int32_t>(TrackRole::kBass), 1, 0, Op::kSet);
    b.cmd(Param::kTransportStart);
    b.advance(kTicksPerBar - 1);
    CHECK(b.ons(1) == 0);  // bass muted
    CHECK(b.ons(9) > 0);   // drums still groove
    CHECK(b.ons(2) > 0);   // chord1 still comps
  }
  {  // Solo isolates: only the soloed part plays.
    Band b;
    b.setup_basic();
    b.cmd(Param::kChordPlay, 60, -1, 100);
    b.cmd(Param::kPartSolo, static_cast<std::int32_t>(TrackRole::kDrums), 1, 0, Op::kSet);
    b.cmd(Param::kTransportStart);
    b.advance(kTicksPerBar - 1);
    CHECK(b.ons(9) > 0);   // drums soloed -> audible
    CHECK(b.ons(1) == 0);  // bass silenced by the solo
    CHECK(b.ons(2) == 0);  // chord1 silenced by the solo
  }
}

void test_quantized_variation_switch() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kChordPlay, 60, -1, 100);
  b.cmd(Param::kTransportStart);
  b.advance(10);
  b.cmd(Param::kStyleSection, static_cast<std::int32_t>(SectionType::kVarB));
  b.ev.clear();
  b.advance(kTicksPerBar);  // switch lands on the next bar boundary
  const auto sec = b.sections();
  CHECK(sec.size() == 1 && sec[0] == static_cast<std::uint16_t>(SectionType::kVarB));
  CHECK(b.e.arranger().current() == SectionType::kVarB);
}

void test_fill_one_shot_returns_to_variation() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kChordPlay, 60, -1, 100);
  b.cmd(Param::kTransportStart);
  b.cmd(Param::kStyleSection, static_cast<std::int32_t>(SectionType::kFillA));
  b.ev.clear();
  b.advance(2 * kTicksPerBar + 10);
  const auto sec = b.sections();
  CHECK(sec.size() == 2);
  CHECK(sec[0] == static_cast<std::uint16_t>(SectionType::kFillA));
  CHECK(sec[1] == static_cast<std::uint16_t>(SectionType::kVarA));  // returned
}

void test_intro_leads_to_variation() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kStyleSection, static_cast<std::int32_t>(SectionType::kIntro1));
  b.cmd(Param::kChordPlay, 60, -1, 100);
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.advance(kTicksPerBar + 10);
  const auto sec = b.sections();
  CHECK(sec.size() == 1 && sec[0] == static_cast<std::uint16_t>(SectionType::kVarA));
}

void test_ending_stops_transport() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kChordPlay, 60, -1, 100);
  b.cmd(Param::kTransportStart);
  b.cmd(Param::kStyleSection, static_cast<std::int32_t>(SectionType::kEnding1));
  b.advance(3 * kTicksPerBar);
  CHECK(!b.e.transport().playing());
  bool saw_stop = false;
  for (const OutEvent& o : b.ev) {
    if (o.kind == OutEvent::Kind::kTransport &&
        o.code == static_cast<std::uint16_t>(TransportState::kStopped)) {
      saw_stop = true;
    }
  }
  CHECK(saw_stop);
}

void test_triad_wrap_and_route_gating() {
  Band b;
  b.cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
  b.cmd(Param::kStyleLoad, 0);
  // Only bass routed: chord/drums silent.
  b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 0 | (1 << 8), 0, Op::kSet);
  // Sus4 triad (3 tones): VarB bass uses tone 3 -> wraps to root +1 octave.
  b.cmd(Param::kChordPlay, 60, static_cast<std::int8_t>(ChordQuality::kSus4), 100);
  b.cmd(Param::kStyleSection, static_cast<std::int32_t>(SectionType::kVarB));
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.advance(kTicksPerBar - 1);
  CHECK(b.ons(9) == 0 && b.ons(2) == 0);  // unrouted roles gated off
  // 48 fires twice: step 10 is the root an octave up ({10,0,1}) and step 12
  // is tone 3 wrapping past the sus4 triad {0,5,7} -> 36 + 0 + 12 = 48.
  CHECK(b.ons(1, 48) == 2);
}

void test_style_warns() {
  Band b;
  b.cmd(Param::kStyleLoad, 99);    // no such builtin (past the 16 registered)
  b.cmd(Param::kStyleSection, 2);  // no style loaded
  b.cmd(Param::kStyleLoad, 0);
  b.cmd(Param::kStyleSection, 99);                                             // bogus section id
  b.cmd(Param::kStyleSection, static_cast<std::int32_t>(SectionType::kBreak));  // absent
  b.cmd(Param::kStyleRoute, 99, 0, 0, Op::kSet);
  b.cmd(Param::kStyleRoute, 0, 9, 0, Op::kSet);  // bad port
  int warns = 0;
  for (const OutEvent& o : b.ev) {
    if (o.kind == OutEvent::Kind::kWarn) {
      ++warns;
    }
  }
  CHECK(warns == 6);
}

namespace twobar {
// A 2-bar section: the review found bars 2..N were unreachable because the
// section clock restarted at every bar boundary.
constexpr StyleEvent kDrums[] = {
    {0, 38, 0, 90, 120},    // bar 1, step 0
    {16, 36, 0, 100, 120},  // bar 2, step 16 — must actually play
};
constexpr StylePattern kPatterns[] = {
    {TrackRole::kDrums, RolePolicy::kFixed, Span<const StyleEvent>(kDrums)},
};
constexpr StyleSection kSections[] = {
    {SectionType::kVarA, 2, Span<const StylePattern>(kPatterns)},
};
constexpr Style kStyle{"twobar", Span<const StyleSection>(kSections)};
}  // namespace twobar

void test_multibar_section_plays_bar_two() {
  Arranger a;
  CHECK(a.load_style(&twobar::kStyle));
  CHECK(a.set_route(TrackRole::kDrums, 0, 9));
  a.on_transport_start();
  const ChordState no_chord{};
  StaticVector<std::uint32_t, 8> hits36, hits38;
  for (Tick t = 0; t < 4 * kTicksPerBar; ++t) {
    a.on_tick(t, no_chord, [&](std::uint8_t, TickOffset delay, const MidiMessage& msg) {
      if (msg.type() != midi::kNoteOn || delay != 0) {
        return;
      }
      if (msg.d1 == 36) {
        CHECK(hits36.push_back(t));
      }
      if (msg.d1 == 38) {
        CHECK(hits38.push_back(t));
      }
    });
  }
  // Two full cycles of a 2-bar section over 4 bars.
  CHECK(hits38.size() == 2 && hits38[0] == 0 && hits38[1] == 2 * kTicksPerBar);
  CHECK(hits36.size() == 2);
  CHECK(hits36[0] == kTicksPerBar);      // bar 2 fires
  CHECK(hits36[1] == 3 * kTicksPerBar);  // and again on the wrap
}

void test_immediate_switch_when_stopped() {
  Band b;
  b.setup_basic();
  b.ev.clear();
  b.cmd(Param::kStyleSection, static_cast<std::int32_t>(SectionType::kVarB));
  CHECK(b.e.arranger().current() == SectionType::kVarB);
  const auto sec = b.sections();
  CHECK(sec.size() == 1 && sec[0] == static_cast<std::uint16_t>(SectionType::kVarB));
}

void test_seamless_style_switch() {
  Arranger a;
  CHECK(a.load_style(&styles::basic::kStyle));
  a.on_transport_start();
  const ChordState no_chord{};
  auto sink = [](std::uint8_t, TickOffset, const MidiMessage&) {};

  // Queue a live switch to a different style (both define varA). It must land
  // TOGETHER on the next bar downbeat, never mid-bar.
  CHECK(a.request_style(&twobar::kStyle, SectionType::kVarA, false));

  bool changed_before_bar = false;
  bool changed_at_bar = false;
  for (Tick t = 0; t <= kTicksPerBar; ++t) {
    const Arranger::TickResult r = a.on_tick(t, no_chord, sink);
    if (r.style_changed) {
      if (t < kTicksPerBar) {
        changed_before_bar = true;
      } else {
        changed_at_bar = true;
      }
    }
  }
  CHECK(!changed_before_bar);  // seamless: not a mid-bar cut
  CHECK(changed_at_bar);       // applied on the downbeat

  // Immediate switch + section fallback: basic has no break section, so it lands varA.
  Arranger f;
  CHECK(f.load_style(&twobar::kStyle));
  CHECK(f.request_style(&styles::basic::kStyle, SectionType::kBreak, true));
  CHECK(f.current() == SectionType::kVarA);

  // A null style is refused.
  CHECK(!f.request_style(nullptr, SectionType::kVarA, true));
}

void test_style_switch_immediate_when_stopped() {
  Band b;
  b.setup_basic();  // basic is builtin 0, loaded on varA
  b.ev.clear();
  // Combined switch to (style 0, VarB): stopped -> immediate regardless of c.
  b.cmd(Param::kStyleSwitch, 0, static_cast<std::int32_t>(SectionType::kVarB), 0);
  CHECK(b.e.arranger().current() == SectionType::kVarB);
  const auto sec = b.sections();
  CHECK(sec.size() == 1 && sec[0] == static_cast<std::uint16_t>(SectionType::kVarB));
}

void test_style_switch_next_bar_when_playing() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kChordPlay, 60, -1, 100);
  b.cmd(Param::kTransportStart);
  b.advance(10);
  b.ev.clear();
  // c = 0 while playing -> quantized to the next bar, no immediate section event.
  b.cmd(Param::kStyleSwitch, 0, static_cast<std::int32_t>(SectionType::kVarB), 0);
  CHECK(b.sections().empty());  // nothing landed yet
  CHECK(b.e.arranger().current() == SectionType::kVarA);
  b.ev.clear();
  b.advance(kTicksPerBar);  // crosses the bar boundary
  const auto sec = b.sections();
  CHECK(sec.size() == 1 && sec[0] == static_cast<std::uint16_t>(SectionType::kVarB));
  CHECK(b.e.arranger().current() == SectionType::kVarB);
}

void test_style_switch_immediate_while_playing() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kChordPlay, 60, -1, 100);
  b.cmd(Param::kTransportStart);
  b.advance(10);
  b.ev.clear();
  // c != 0 while playing -> hard cut now.
  b.cmd(Param::kStyleSwitch, 0, static_cast<std::int32_t>(SectionType::kVarB), 1);
  CHECK(b.e.arranger().current() == SectionType::kVarB);
  const auto sec = b.sections();
  CHECK(sec.size() == 1 && sec[0] == static_cast<std::uint16_t>(SectionType::kVarB));
}

void test_style_switch_bad_index_warns() {
  Band b;
  b.setup_basic();
  b.ev.clear();
  b.cmd(Param::kStyleSwitch, 99, static_cast<std::int32_t>(SectionType::kVarA), 1);  // no style 99
  b.cmd(Param::kStyleSwitch, -1, static_cast<std::int32_t>(SectionType::kVarA), 1);  // negative
  b.cmd(Param::kStyleSwitch, 0, 99, 1);                                              // bad section
  int warns = 0;
  for (const OutEvent& o : b.ev) {
    if (o.kind == OutEvent::Kind::kWarn &&
        o.code == static_cast<std::uint16_t>(WarnCode::kBadArgument)) {
      ++warns;
    }
  }
  CHECK(warns == 3);
  // No crash, and the arranger stayed on its loaded section.
  CHECK(b.e.arranger().current() == SectionType::kVarA);
}

void test_builtin_styles_registered() {
  // Sixteen builtins exist in order: the original four first, then the twelve
  // genre styles.
  CHECK(styles::kBuiltinCount == 16);
  const char* expected[] = {"basic",  "pop",   "rock",   "ballad", "funk",  "disco",
                            "house",  "swing", "bossa",  "samba",  "reggae", "country",
                            "blues",  "shuffle", "latin", "motown"};
  // Every builtin must resolve the full twelve-section vocabulary so the chooser
  // and the section stepper always have a consistent set to work with.
  const SectionType full_set[] = {
      SectionType::kIntro1, SectionType::kIntro2, SectionType::kVarA,   SectionType::kVarB,
      SectionType::kVarC,   SectionType::kVarD,   SectionType::kFillA,  SectionType::kFillB,
      SectionType::kFillC,  SectionType::kFillD,  SectionType::kEnding1, SectionType::kEnding2};
  for (std::uint8_t i = 0; i < styles::kBuiltinCount; ++i) {
    const Style* s = styles::kBuiltins[i];
    CHECK(s != nullptr);
    CHECK(std::string_view(s->name) == std::string_view(expected[i]));
    for (SectionType t : full_set) {
      CHECK(s->find(t) != nullptr);
    }
  }
}

void test_builtin_styles_play_roles() {
  // Each builtin, loaded and ticked over one bar, emits drum, bass and chord
  // events through the normal path.
  for (std::uint8_t i = 0; i < styles::kBuiltinCount; ++i) {
    Band b;
    b.cmd(Param::kKeySet, 0, 0, 0, Op::kSet);
    b.cmd(Param::kStyleLoad, i);
    b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kDrums), 0 | (9 << 8), 0,
          Op::kSet);
    b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 0 | (1 << 8), 0,
          Op::kSet);
    b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kChord1), 0 | (2 << 8), 0,
          Op::kSet);
    b.cmd(Param::kChordPlay, 60, static_cast<std::int8_t>(ChordQuality::kMaj7), 100);
    b.cmd(Param::kTransportStart);
    b.ev.clear();
    b.advance(kTicksPerBar);
    CHECK(b.ons(9) > 0);  // drums groove
    CHECK(b.ons(1) > 0);  // bass follows the chord
    CHECK(b.ons(2) > 0);  // chord comps
  }
}

}  // namespace

int main() {
  test_drums_play_without_chord_but_tonal_roles_wait();
  test_ntt_resolution_follows_chord();
  test_role_anchor_and_gm_voices();
  test_groove_apply();
  test_groove_command();
  test_part_mute_solo();
  test_quantized_variation_switch();
  test_fill_one_shot_returns_to_variation();
  test_intro_leads_to_variation();
  test_ending_stops_transport();
  test_triad_wrap_and_route_gating();
  test_style_warns();
  test_multibar_section_plays_bar_two();
  test_immediate_switch_when_stopped();
  test_seamless_style_switch();
  test_style_switch_immediate_when_stopped();
  test_style_switch_next_bar_when_playing();
  test_style_switch_immediate_while_playing();
  test_style_switch_bad_index_warns();
  test_builtin_styles_registered();
  test_builtin_styles_play_roles();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_arranger: all OK\n");
  }
  return arrangrr::test::failures();
}
