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

// A style carrying a NON-default per-style feel (9110) and tempo (9120), to
// prove a style load seeds the arranger's live groove and the transport bpm.
// The builtins all keep the no-op default this pass, so these seams need an
// explicitly non-default fixture to exercise them.
constexpr GrooveParams kFeelGroove{.swing = 40, .accent = 12, .swing_grid = 16};
constexpr Style kFeelTestStyle{.name = "feeltest",
                               .sections = Span<const StyleSection>(kVoiceSections),
                               .groove = kFeelGroove,
                               .tempo = 15000};

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
  arr.on_tick(0, Key{}, chord, [&](std::uint8_t, TickOffset, const MidiMessage& m) {
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

// 9110: a style load seeds the arranger's live GrooveParams from Style::groove;
// a user groove edit overrides it until the next load, which re-seeds. A
// default-groove style leaves the feel a no-op (byte-identical output).
void test_style_seeds_groove() {
  Arranger arr;
  // (a) a non-default-groove style seeds m_groove on load.
  CHECK(arr.load_style(&kFeelTestStyle));
  CHECK(arr.groove_params().swing == 40);
  CHECK(arr.groove_params().accent == 12);
  CHECK(arr.groove_params().swing_grid == 16);

  // A user groove-panel edit overrides the style default...
  arr.set_groove_field(GrooveField::kSwing, 70);
  CHECK(arr.groove_params().swing == 70);

  // (b)+(c) loading a DEFAULT-groove style re-seeds from B (no-op feel), so the
  // prior style's feel AND the user edit are both discarded.
  CHECK(arr.load_style(&kVoiceTestStyle));
  CHECK(arr.groove_params().swing == 0);
  CHECK(arr.groove_params().accent == 0);
  CHECK(arr.groove_params().swing_grid == 8);

  // (c) loading the non-default style again re-seeds its feel.
  CHECK(arr.load_style(&kFeelTestStyle));
  CHECK(arr.groove_params().swing == 40);
}

// 9110: an immediate live style switch (transport-stopped path) also adopts the
// new style's feel, matching the load semantics.
void test_style_switch_seeds_groove() {
  Arranger arr;
  CHECK(arr.load_style(&kVoiceTestStyle));
  CHECK(arr.groove_params().swing == 0);
  CHECK(arr.request_style(&kFeelTestStyle, SectionType::kVarA, /*immediate=*/true));
  CHECK(arr.groove_params().swing == 40);
  CHECK(arr.groove_params().accent == 12);
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

// Transport start seeds the home-key tonic (lifecycle default), so the band
// starts IN the home key: drums groove and the tonal roles sound their NTT parts
// against the seeded tonic from bar 1 -- no chord press needed.
void test_band_starts_in_home_key() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kTransportStart);
  b.advance(kTicksPerBar - 1);
  CHECK(b.ons(9) > 0);  // drums groove
  CHECK(b.ons(1) > 0);  // bass plays the seeded home-key tonic
  CHECK(b.ons(2) > 0);  // tonal roles sound from bar 1
}

// 9120: a DEFERRED live style switch (queued while playing) adopts the new
// style's tempo when it lands at the bar boundary, exercising the engine's
// fire_arranger style_changed seam.
void test_deferred_style_switch_seeds_tempo() {
  Band b;
  b.setup_basic();  // loads basic (12000)
  b.cmd(Param::kTransportStart);
  CHECK(b.e.transport().bpm() == 12000);
  // Queue a combined switch to rock/varA while playing (cmd.c == 0 -> deferred).
  b.cmd(Param::kStyleSwitch, 2, static_cast<std::int32_t>(SectionType::kVarA), 0, Op::kSet);
  CHECK(b.e.transport().bpm() == 12000);  // not yet: waits for the bar boundary
  b.advance(kTicksPerBar);                // cross into the next bar
  CHECK(b.e.transport().bpm() == 13000);  // rock tempo landed with the switch
}

// 9120: loading a builtin style seeds the transport tempo from Style::tempo,
// through the engine's internal wiring (no new ABI command). Ottorino's
// per-style values; loading style B after A re-seeds; the default styles keep
// 120.00; latin stays at the default pending owner confirm.
void test_style_seeds_transport_tempo() {
  Band b;
  // (b) a default-tempo style keeps 120.00 (basic index 0, tempo 12000).
  b.cmd(Param::kStyleLoad, 0);
  CHECK(b.e.transport().bpm() == 12000);

  // (a) a non-default-tempo style seeds the transport (rock index 2 -> 13000).
  b.cmd(Param::kStyleLoad, 2);
  CHECK(b.e.transport().bpm() == 13000);

  // (c) loading style B after A re-seeds from B (ballad index 3 -> 7200,
  // blues index 12 -> 6600).
  b.cmd(Param::kStyleLoad, 3);
  CHECK(b.e.transport().bpm() == 7200);
  b.cmd(Param::kStyleLoad, 12);
  CHECK(b.e.transport().bpm() == 6600);

  // A user tempo override is re-seeded by the next style load (documented
  // semantic: a style load adopts the style's default tempo).
  b.cmd(Param::kTransportTempo, 20000, 0, 0, Op::kSet);
  CHECK(b.e.transport().bpm() == 20000);
  b.cmd(Param::kStyleLoad, 15);  // motown -> 12400
  CHECK(b.e.transport().bpm() == 12400);

  // Owner exception: latin (index 14) stays at the default until confirmed.
  b.cmd(Param::kStyleLoad, 14);
  CHECK(b.e.transport().bpm() == 12000);
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

  // Accent lifts beat 1 most, beat 3 (step 8) a little, and softens beats 2/4.
  p = GrooveParams{};
  p.accent = 100;
  const std::uint8_t beat1 = groove::apply(p, 0, 0, 0, 80).velocity;
  const std::uint8_t beat3 = groove::apply(p, 0, 8, 0, 80).velocity;
  CHECK(beat1 > 80);
  CHECK(beat3 > 80 && beat3 < beat1);  // beat 3 lifted, but less than beat 1
  CHECK(groove::apply(p, 0, 4, 0, 80).velocity < 80);   // beat 2 softened
  CHECK(groove::apply(p, 0, 12, 0, 80).velocity < 80);  // beat 4 softened

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

  // Quantize scales the swing+humanize timing offset back toward the grid,
  // touching only the timing (velocity is left alone).
  p = GrooveParams{};
  p.swing = 100;
  const TickOffset base_off = groove::apply(p, 0, 2, 0, 100).timing_offset;
  CHECK(base_off > 0);
  // 0% (default) leaves the offset intact.
  p.quantize = 0;
  CHECK(groove::apply(p, 0, 2, 0, 100).timing_offset == base_off);
  // 100% snaps the event exactly onto the grid (offset zeroed).
  p.quantize = 100;
  const GrooveOut q100 = groove::apply(p, 0, 2, 0, 100);
  CHECK(q100.timing_offset == 0);
  CHECK(q100.velocity == 100);  // velocity untouched by quantize
  // An intermediate value scales the offset proportionally toward zero.
  p.quantize = 50;
  CHECK(groove::apply(p, 0, 2, 0, 100).timing_offset == base_off * 50 / 100);
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

// groove::set_field drives every GrooveField arm and clamps each value.
void test_groove_set_field_all() {
  GrooveParams p;
  groove::set_field(p, GrooveField::kSwing, 40);
  CHECK(p.swing == 40);
  groove::set_field(p, GrooveField::kSwing, -5);  // clamp low
  CHECK(p.swing == 0);
  groove::set_field(p, GrooveField::kHumanizeTiming, 200);  // clamp high
  CHECK(p.humanize_timing == 100);
  groove::set_field(p, GrooveField::kHumanizeVelocity, 55);
  CHECK(p.humanize_velocity == 55);
  groove::set_field(p, GrooveField::kAccent, 30);
  CHECK(p.accent == 30);
  // Swing grid only accepts 16, anything else snaps to 8.
  groove::set_field(p, GrooveField::kSwingGrid, 16);
  CHECK(p.swing_grid == 16);
  groove::set_field(p, GrooveField::kSwingGrid, 4);  // not 16 -> 8
  CHECK(p.swing_grid == 8);
  groove::set_field(p, GrooveField::kSeed, 777);
  CHECK(p.seed == 777u);
  groove::set_field(p, GrooveField::kSeed, -3);  // negative -> 0
  CHECK(p.seed == 0u);
  groove::set_field(p, GrooveField::kQuantize, 75);
  CHECK(p.quantize == 75);
  groove::set_field(p, GrooveField::kQuantize, 200);  // clamp high
  CHECK(p.quantize == 100);
  groove::set_field(p, GrooveField::kQuantize, -5);  // clamp low
  CHECK(p.quantize == 0);
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
    a.on_tick(t, Key{}, no_chord, [&](std::uint8_t, TickOffset delay, const MidiMessage& msg) {
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
    const Arranger::TickResult r = a.on_tick(t, Key{}, no_chord, sink);
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

// Resolve a single tonal event through the normal on_tick path and return the
// MIDI note it produced (or -1 if it was skipped/clamped). Builds a throwaway
// one-event style so we can probe NoteSource resolution directly, chord- and
// key-aware, without leaning on any builtin's pattern data.
int resolve_one(TrackRole role, RolePolicy policy, NoteSource src, std::int8_t tone,
                std::int8_t octave, const Key& key, const ChordState& chord) {
  const StyleEvent ev[] = {
      {.step = 0, .tone = tone, .octave = octave, .vel = 100, .gate = 100, .src = src}};
  const StylePattern pat[] = {
      {.role = role, .policy = policy, .events = Span<const StyleEvent>(ev)}};
  const StyleSection sec[] = {
      {.type = SectionType::kVarA, .bars = 1, .patterns = Span<const StylePattern>(pat)}};
  const Style style{.name = "probe", .sections = Span<const StyleSection>(sec)};
  Arranger a;
  CHECK(a.load_style(&style));
  CHECK(a.set_route(role, 0, 0));
  a.on_transport_start();
  int note = -1;
  a.on_tick(0, key, chord, [&](std::uint8_t, TickOffset, const MidiMessage& m) {
    if (m.type() == midi::kNoteOn) {
      note = m.d1;
    }
  });
  return note;
}

void test_note_source_vocabulary() {
  const Key c_major{.root_pc = 0, .mode = Mode::kMajor};
  const Key a_minor{.root_pc = 9, .mode = Mode::kMinor};
  const ChordState c_maj{.root_pc = 0, .quality = ChordQuality::kMaj, .valid = true};
  const ChordState c_min{.root_pc = 0, .quality = ChordQuality::kMin, .valid = true};
  constexpr int kLeadAnchor = 72;  // kRoleAnchor[kLead]
  const auto lead = [&](NoteSource src, std::int8_t tone, std::int8_t octave, const Key& key,
                        const ChordState& chord) {
    return resolve_one(TrackRole::kLead, RolePolicy::kChordTone, src, tone, octave, key, chord);
  };

  // Backward compat: kChordTone (the default src) resolves EXACTLY as before —
  // over C major, chord-tone 0/1/2 = root/third/fifth at the lead anchor.
  CHECK(lead(NoteSource::kChordTone, 0, 0, c_major, c_maj) == kLeadAnchor + 0);  // 72 C
  CHECK(lead(NoteSource::kChordTone, 1, 0, c_major, c_maj) == kLeadAnchor + 4);  // 76 E
  CHECK(lead(NoteSource::kChordTone, 2, 0, c_major, c_maj) == kLeadAnchor + 7);  // 79 G

  // kInterval: signed semitones from the chord root, no shape/quality lookup.
  CHECK(lead(NoteSource::kInterval, 14, 0, c_major, c_maj) == kLeadAnchor + 14);  // 86
  CHECK(lead(NoteSource::kInterval, -1, 0, c_major, c_maj) == kLeadAnchor - 1);   // 71 approach
  CHECK(lead(NoteSource::kInterval, 3, 0, c_major, c_min) == kLeadAnchor + 3);    // quality-blind
  CHECK(lead(NoteSource::kInterval, 0, -7, c_major, c_maj) == -1);  // 72-84 clamps low to skipped
  CHECK(lead(NoteSource::kInterval, 0, 7, c_major, c_maj) == -1);   // 72+84 clamps high to skipped
  CHECK(lead(NoteSource::kInterval, 0, 0, c_major, ChordState{}) == -1);  // needs a valid chord

  // kScaleDegree in C major: degrees 0/1/4 = tonic/2nd/5th diatonic pitches.
  CHECK(lead(NoteSource::kScaleDegree, 0, 0, c_major, c_maj) == kLeadAnchor + 0);  // 72 C
  CHECK(lead(NoteSource::kScaleDegree, 1, 0, c_major, c_maj) == kLeadAnchor + 2);  // 74 D
  CHECK(lead(NoteSource::kScaleDegree, 4, 0, c_major, c_maj) == kLeadAnchor + 7);  // 79 G
  // Chord-quality independent: same diatonic notes over a minor chord.
  CHECK(lead(NoteSource::kScaleDegree, 1, 0, c_major, c_min) == kLeadAnchor + 2);
  // Chord-validity independent: the key always exists, so it still sounds.
  CHECK(lead(NoteSource::kScaleDegree, 0, 0, c_major, ChordState{}) == kLeadAnchor + 0);
  // Out-of-range scale-degree lines clamp to skipped on both ends.
  CHECK(lead(NoteSource::kScaleDegree, 0, 7, c_major, c_maj) == -1);   // 72+84 clamps high
  CHECK(lead(NoteSource::kScaleDegree, 0, -7, c_major, c_maj) == -1);  // 72-84 clamps low

  // A minor key follows the aeolian scale (root_pc = 9): degrees 0/1/4.
  CHECK(lead(NoteSource::kScaleDegree, 0, 0, a_minor, c_maj) == kLeadAnchor + 9 + 0);  // 81 A
  CHECK(lead(NoteSource::kScaleDegree, 1, 0, a_minor, c_maj) == kLeadAnchor + 9 + 2);  // 83 B
  CHECK(lead(NoteSource::kScaleDegree, 4, 0, a_minor, c_maj) == kLeadAnchor + 9 + 7);  // 88 E
  // The SCALE (not the chord) decides colour: degree 2 is a major third in C
  // major but a minor third in A minor.
  CHECK(lead(NoteSource::kScaleDegree, 2, 0, c_major, c_maj) == kLeadAnchor + 4);      // E
  CHECK(lead(NoteSource::kScaleDegree, 2, 0, a_minor, c_maj) == kLeadAnchor + 9 + 3);  // C natural

  // kFixed short-circuits to the literal note REGARDLESS of src (drums never
  // transpose), whatever key/chord/note-source is attached.
  CHECK(resolve_one(TrackRole::kDrums, RolePolicy::kFixed, NoteSource::kInterval, 38, 0, a_minor,
                    c_min) == 38);
  CHECK(resolve_one(TrackRole::kDrums, RolePolicy::kFixed, NoteSource::kScaleDegree, 42, 0, a_minor,
                    c_min) == 42);
}

}  // namespace

int main() {
  test_band_starts_in_home_key();
  test_style_seeds_groove();
  test_style_switch_seeds_groove();
  test_style_seeds_transport_tempo();
  test_deferred_style_switch_seeds_tempo();
  test_ntt_resolution_follows_chord();
  test_role_anchor_and_gm_voices();
  test_groove_apply();
  test_groove_command();
  test_groove_set_field_all();
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
  test_note_source_vocabulary();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_arranger: all OK\n");
  }
  return arrangrr::test::failures();
}
