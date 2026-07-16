#include "arrangrr/arranger/arranger.hpp"

#include <string_view>

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"
#include "test_harness.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 512>;

// A minimal style exercising a NEW role (kPad) with a default GM voice, to test
// the per-role register anchor and gm_program emission independently of the
// (not-yet-enriched) builtins.
constexpr StyleEvent kVoiceBass[] = {{.step = 0, .tone = 0, .octave = 0, .vel = 100, .gate = 200}};
constexpr StyleEvent kVoicePad[] = {{.step = 0, .tone = 0, .octave = 0, .vel = 70, .gate = 3600}};
constexpr StylePattern kVoicePatterns[] = {
    {.role = TrackRole::kBass,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kVoiceBass),
     .gm_program = 33},  // Fingered Bass
    {.role = TrackRole::kPad,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kVoicePad),
     .gm_program = 89},  // Pad 2 (warm)
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

// Torquato QA heavy-pass (Phase-6 Theme 4, target 4 -- owner decision 5, the
// arp-insert held-chord model): a 2-tone "chord" at step 0 (tones 0 and 2,
// i.e. root+fifth over Cmaj -> 36 and 43), NOTHING for 7 straight steps
// (silence), then a single DIFFERENT tone (tone 1, the third -> 40) at
// step 8. Exercises both halves of decision 5 in one fixture: the held chord
// must survive the silent steps unchanged (hold-through-silence), and the
// step-8 group must REPLACE it outright, not accumulate on top of it.
constexpr StyleEvent kArpHoldBass[] = {
    {.step = 0, .tone = 0, .octave = 0, .vel = 100, .gate = 200},
    {.step = 0, .tone = 2, .octave = 0, .vel = 100, .gate = 200},
    {.step = 8, .tone = 1, .octave = 0, .vel = 100, .gate = 200},
};
constexpr StylePattern kArpHoldPatterns[] = {
    {.role = TrackRole::kBass,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kArpHoldBass)},
};
constexpr StyleSection kArpHoldSections[] = {
    {.type = SectionType::kVarA,
     .bars = 1,
     .patterns = Span<const StylePattern>(kArpHoldPatterns)}};
constexpr Style kArpHoldStyle{.name = "arpholdtest",
                              .sections = Span<const StyleSection>(kArpHoldSections)};

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
  arr.on_tick(0, Key{}, chord, [&](std::uint8_t, TickOffset, const MidiMessage& m, std::uint8_t) {
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
  test::TestEngine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0, Op op = Op::kDo,
           Boundary boundary = Boundary::kImmediate) {
    Command command;
    command.op = op;
    command.param = p;
    command.boundary = boundary;
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
  // Queue a combined switch to rock/varA while playing (boundary kNextBar -> deferred).
  b.cmd(Param::kStyleSwitch, 2, static_cast<std::int32_t>(SectionType::kVarA), 0, Op::kSet,
        Boundary::kNextBar);
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
  CHECK(beat3 > 80 && beat3 < beat1);                   // beat 3 lifted, but less than beat 1
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

// ---- Phase-6 Theme 4 (5210/5220): groove-as-insert / arp-as-insert, driven
// directly through Arranger::on_tick (no Engine needed) ---------------------

// Bit-identity proof for the auto-present, pinned-last kGroove slot (Fork 2):
// the chain-driven schedule must reproduce groove::apply()'s OWN independent
// computation for the exact same (params, role, step, tick, vel) inputs --
// the same guarantee feel_swing/feel_blues/feel_shuffle's goldens pin at a
// higher level, checked here directly against the pure function.
void test_groove_insert_reproduces_groove_apply_math_at_default_last_slot() {
  Arranger arr;
  CHECK(arr.load_style(&kFeelTestStyle));  // kFeelGroove{swing=40, accent=12, swing_grid=16}
  CHECK(arr.set_route(TrackRole::kBass, 0, 0));
  const ChordState chord{.root_pc = 0, .quality = ChordQuality::kMaj, .valid = true};
  arr.on_transport_start();

  TickOffset on_delay = -1;
  std::uint8_t on_vel = 0;
  arr.on_tick(0, Key{}, chord,
              [&](std::uint8_t, TickOffset delay, const MidiMessage& m, std::uint8_t) {
                if (m.channel() == 0 && m.type() == midi::kNoteOn) {
                  on_delay = delay;
                  on_vel = m.d2;
                }
              });
  const auto bass_role_idx = static_cast<std::uint8_t>(TrackRole::kBass);
  const GrooveOut expected = groove::apply(kFeelGroove, bass_role_idx, /*step=*/0, /*tick=*/0,
                                           /*base_vel=*/100);  // kVoiceBass's own vel
  CHECK(on_delay == expected.timing_offset);
  CHECK(on_vel == expected.velocity);
}

// Fork 1: a role's arp-insert swallows its own resolved block note (never
// scheduled directly) and instead plays it from the SAME on_tick call's P5
// pass, which fires immediately (transport_tick 0 is always a rate-grid
// boundary).
void test_arp_insert_swallows_direct_note_and_plays_from_the_p5_pass() {
  Arranger arr;
  CHECK(arr.load_style(&kVoiceTestStyle));  // kBass: tone 0, step 0, vel 100, gate 200
  CHECK(arr.set_route(TrackRole::kBass, 0, 0));
  CHECK(arr.set_fx(TrackRole::kBass, 0, InsertType::kArp));
  CHECK(arr.set_fx_param(TrackRole::kBass, 0, /*rate=*/0,
                         static_cast<std::int32_t>(ArpRate::kSixteenth)));
  CHECK(arr.set_fx_param(TrackRole::kBass, 0, /*gate=*/3, 75));

  const ChordState chord{.root_pc = 0, .quality = ChordQuality::kMaj, .valid = true};
  arr.on_transport_start();

  int note_on_count = 0;
  int note_off_count = 0;
  std::uint8_t seen_note = 0;
  TickOffset off_delay = -1;
  arr.on_tick(0, Key{}, chord,
              [&](std::uint8_t, TickOffset delay, const MidiMessage& m, std::uint8_t) {
                if (m.channel() != 0) {
                  return;
                }
                if (m.type() == midi::kNoteOn) {
                  ++note_on_count;
                  seen_note = m.d1;
                  CHECK(delay == 0);
                } else if (m.type() == midi::kNoteOff) {
                  ++note_off_count;
                  off_delay = delay;
                }
              });
  CHECK(note_on_count == 1);  // the raw block note never sounds -- only the arp's own emission
  CHECK(seen_note == 36);     // bass anchor 36 + chord root 0 == C2, the SAME note the arp holds
  CHECK(note_off_count == 1);
  CHECK(off_delay == 180);  // step_ticks(240) * gate(75) / 100
}

// P5's own structural claim: the arp-insert pass is UNGATED, not a drop-in
// addition to the grid-gated per-step loop -- it must fire even on a tick
// that is NOT a multiple of kTicksPerStep(240), when the arp's own rate is
// finer than the style's own grid.
void test_arp_insert_fires_on_own_rate_grid_even_off_the_style_step_grid() {
  Arranger arr;
  CHECK(arr.load_style(&kVoiceTestStyle));
  CHECK(arr.set_route(TrackRole::kBass, 0, 0));
  CHECK(arr.set_fx(TrackRole::kBass, 0, InsertType::kArp));
  CHECK(arr.set_fx_param(TrackRole::kBass, 0, /*rate=*/0,
                         static_cast<std::int32_t>(ArpRate::kThirtySecond)));  // 120 ticks

  const ChordState chord{.root_pc = 0, .quality = ChordQuality::kMaj, .valid = true};
  arr.on_transport_start();

  int note_ons_at_0 = 0;
  arr.on_tick(0, Key{}, chord, [&](std::uint8_t, TickOffset, const MidiMessage& m, std::uint8_t) {
    if (m.channel() == 0 && m.type() == midi::kNoteOn) {
      ++note_ons_at_0;
    }
  });
  CHECK(note_ons_at_0 == 1);

  // Tick 120 is NOT a multiple of kTicksPerStep(240) -- the style's own grid
  // produces nothing here (the grid-gated loop never even runs its body),
  // yet the arp-insert's own 32nd-note rate (120 ticks) fires anyway.
  int note_ons_at_120 = 0;
  arr.on_tick(120, Key{}, chord,
              [&](std::uint8_t, TickOffset delay, const MidiMessage& m, std::uint8_t) {
                if (m.channel() == 0 && m.type() == midi::kNoteOn) {
                  ++note_ons_at_120;
                  CHECK(delay == 0);
                }
              });
  CHECK(note_ons_at_120 == 1);
}

// Target 4 (owner decision 5): the arp-insert's held-chord model is REPLACE,
// not accumulate, on each fresh non-empty resolved group, and the held chord
// survives every silent step in between unchanged.
void test_arp_insert_holds_chord_through_silence_and_replaces_not_accumulates() {
  Arranger arr;
  CHECK(arr.load_style(&kArpHoldStyle));
  CHECK(arr.set_route(TrackRole::kBass, 0, 0));
  CHECK(arr.set_fx(TrackRole::kBass, 0, InsertType::kArp));
  CHECK(arr.set_fx_param(TrackRole::kBass, 0, /*rate=*/0,
                         static_cast<std::int32_t>(ArpRate::kSixteenth)));  // 240 ticks: lockstep
                                                                            // with the style grid
  CHECK(arr.set_fx_param(TrackRole::kBass, 0, /*direction=*/1,
                         static_cast<std::int32_t>(ArpDirection::kUp)));
  CHECK(arr.set_fx_param(TrackRole::kBass, 0, /*gate=*/3, 100));

  const ChordState chord{.root_pc = 0, .quality = ChordQuality::kMaj, .valid = true};
  arr.on_transport_start();

  auto fire = [&](Tick t) {
    std::uint8_t seen = 255;
    arr.on_tick(t, Key{}, chord, [&](std::uint8_t, TickOffset, const MidiMessage& m, std::uint8_t) {
      if (m.channel() == 0 && m.type() == midi::kNoteOn) {
        seen = m.d1;
      }
    });
    return seen;
  };

  // Step 0 (tick 0): the 2-note chord {36, 43} (root+fifth) is freshly
  // ingested (a clear-then-ingest on a non-empty group) -- kUp cycles
  // ascending, so THIS SAME tick's own P5 emission is the lowest note.
  CHECK(fire(0) == 36);
  // Steps 1..7 (ticks 240..1680): the style has NO bass event here at all --
  // hold-through-silence means the arp-insert must keep cycling the SAME
  // 2-note chord the entire way, never falling silent and never resetting.
  CHECK(fire(240) == 43);
  CHECK(fire(480) == 36);
  CHECK(fire(720) == 43);
  CHECK(fire(960) == 36);
  CHECK(fire(1200) == 43);
  CHECK(fire(1440) == 36);
  CHECK(fire(1680) == 43);
  // Step 8 (tick 1920): a FRESH single-tone group (the third, 40) replaces
  // the held chord outright -- decision 5's "replace, not accumulate": the
  // emission is the new tone ALONE, never a 3-note mix with the old 36/43.
  CHECK(fire(1920) == 40);
  // Step 9 (tick 2160): silence again -- a length-1 held sequence always
  // re-emits the same single note, proving the old 2-note chord is genuinely
  // gone (a leftover 2-note cycle would have alternated back to 36 or 43
  // here, not repeated 40).
  CHECK(fire(2160) == 40);
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
  b.cmd(Param::kStyleSection, 99);                                              // bogus section id
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
    a.on_tick(t, Key{}, no_chord,
              [&](std::uint8_t, TickOffset delay, const MidiMessage& msg, std::uint8_t) {
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

namespace threebeat {
// Phase 7 (node T0): a 1-bar section authored in 3/4 (F4's Style/beats_per_bar
// field, mirroring Style::tempo). The style field only states the AUTHORED
// intent -- Arranger holds no Transport&, so it never reads beats_per_bar
// itself; the actual bar-length math on_tick uses is whatever
// `ticks_per_bar` the caller threads in (Engine::fire_arranger, in
// production). This fixture drives on_tick directly with an explicit 3-beat
// value, exercising the SAME threading mechanism at the unit level.
constexpr StyleEvent kDrums[] = {
    {.step = 0, .tone = 38, .octave = 0, .vel = 90, .gate = 120},  // step 0 of every bar
};
constexpr StylePattern kPatterns[] = {
    {.role = TrackRole::kDrums,
     .policy = RolePolicy::kFixed,
     .events = Span<const StyleEvent>(kDrums)},
};
constexpr StyleSection kSections[] = {
    {.type = SectionType::kVarA, .bars = 1, .patterns = Span<const StylePattern>(kPatterns)},
};
constexpr Style kStyle{
    .name = "threebeat", .sections = Span<const StyleSection>(kSections), .beats_per_bar = 3};
}  // namespace threebeat

// Phase 7 (node T0): Style::beats_per_bar carries the style's own authored
// time signature, and Arranger::on_tick's threaded `ticks_per_bar` parameter
// is the mechanism that actually varies the bar length the section-wrap math
// uses (a non-4/4 set genuinely changes bar length, the T0 acceptance gate).
void test_style_beats_per_bar_field_and_explicit_ticks_per_bar_change_wrap() {
  Arranger a;
  CHECK(a.load_style(&threebeat::kStyle));
  CHECK(a.current_style()->beats_per_bar == 3);
  CHECK(a.set_route(TrackRole::kDrums, 0, 9));
  a.on_transport_start();
  const ChordState no_chord{};
  const Tick three_beat_bar = 3 * kTicksPerBeat;
  CHECK(three_beat_bar < kTicksPerBar);  // sanity: genuinely shorter than 4/4
  StaticVector<Tick, 8> hits;
  for (Tick t = 0; t < 2 * three_beat_bar; ++t) {
    a.on_tick(
        t, Key{}, no_chord,
        [&](std::uint8_t, TickOffset delay, const MidiMessage& msg, std::uint8_t) {
          if (msg.type() == midi::kNoteOn && delay == 0 && msg.d1 == 38) {
            CHECK(hits.push_back(t));
          }
        },
        three_beat_bar);
  }
  // Two 1-bar cycles of a 3-beat bar: the section wraps at tick 2880 (3*960),
  // NOT at 3840 (the default 4/4 kTicksPerBar) -- proving the threaded
  // ticks_per_bar, not the compile-time constant, drives the wrap.
  CHECK(hits.size() == 2 && hits[0] == 0 && hits[1] == three_beat_bar);
}

namespace fourbeat {
// A sibling of threebeat::kStyle sharing the SAME section shape/patterns but
// the default (4-beat) time signature -- lets the two request_style tests
// below assert a genuine FIELD SWITCH (4 -> 3 and back) rather than reading
// an unrelated style's default.
constexpr Style kStyle{.name = "fourbeat",
                       .sections = Span<const StyleSection>(threebeat::kSections)};
static_assert(kStyle.beats_per_bar == kBeatsPerBar);
}  // namespace fourbeat

// Phase 7 (node T0), item 4 (Style -> Arranger data plumbing): an IMMEDIATE
// live style switch (request_style(..., /*immediate=*/true)) swaps
// current_style() to the new pointer AT ONCE -- Style::beats_per_bar is part
// of that same pointer swap, so the new style's authored time signature is
// visible the instant the switch lands, mirroring Style::tempo's own
// immediate-switch precedent (test_style_switch_seeds_groove above).
// Arranger itself never reads this field (Engine::apply_style_time_sig does,
// production-side) -- this pins the DATA the field-switch mechanism itself
// exposes to that caller.
void test_request_style_immediate_swaps_beats_per_bar_field() {
  Arranger a;
  CHECK(a.load_style(&fourbeat::kStyle));
  CHECK(a.current_style()->beats_per_bar == 4);
  CHECK(a.request_style(&threebeat::kStyle, SectionType::kVarA, /*immediate=*/true));
  CHECK(a.current_style()->beats_per_bar == 3);
  // And back, proving this is a genuine two-way field swap, not a one-shot.
  CHECK(a.request_style(&fourbeat::kStyle, SectionType::kVarA, /*immediate=*/true));
  CHECK(a.current_style()->beats_per_bar == 4);
}

// Phase 7 (node T0), item 4: a DEFERRED live style switch (queued while
// playing) does NOT swap current_style() until the bar boundary it was
// queued for -- Style::beats_per_bar rides the SAME m_pending_style pointer
// every other deferred field does (Style::tempo's own deferred-switch
// precedent, test_deferred_style_switch_seeds_tempo, engine.hpp). Driven at
// the Arranger unit level with the OLD style's own bar length threaded
// through on_tick (matching production: Engine::fire_arranger reads
// m_transport.ticks_per_bar() BEFORE apply_style_time_sig ever runs for this
// bar -- the switch's OWN new time signature only takes musical effect from
// the NEXT bar on, exactly like a deferred tempo change).
void test_request_style_deferred_swaps_beats_per_bar_field_at_bar_boundary() {
  Arranger a;
  CHECK(a.load_style(&fourbeat::kStyle));
  CHECK(a.set_route(TrackRole::kDrums, 0, 9));
  a.on_transport_start();
  const ChordState no_chord{};
  auto tick_fourbeat = [&](Tick t) {
    return a.on_tick(
        t, Key{}, no_chord, [](std::uint8_t, TickOffset, const MidiMessage&, std::uint8_t) {},
        kTicksPerBar);
  };
  // Mid-bar: queue the switch (transport playing -> immediate == false).
  tick_fourbeat(0);
  CHECK(a.request_style(&threebeat::kStyle, SectionType::kVarA, /*immediate=*/false));
  CHECK(a.current_style()->beats_per_bar == 4);  // NOT yet -- still the OLD style
  for (Tick t = 1; t < kTicksPerBar; ++t) {
    tick_fourbeat(t);
    CHECK(a.current_style()->beats_per_bar == 4);  // unchanged until the boundary
  }
  // The bar boundary itself: still threaded with the OLD (4/4) bar length --
  // production mirrors this exactly (Engine::fire_arranger passes the
  // pre-switch m_transport.ticks_per_bar(); apply_style_time_sig only runs
  // AFTER on_tick reports style_changed, seeding the transport for bars
  // after this one).
  const Arranger::TickResult r = a.on_tick(
      kTicksPerBar, Key{}, no_chord,
      [](std::uint8_t, TickOffset, const MidiMessage&, std::uint8_t) {}, kTicksPerBar);
  CHECK(r.style_changed);
  CHECK(a.current_style()->beats_per_bar == 3);  // landed: the new style's field is now live
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
  auto sink = [](std::uint8_t, TickOffset, const MidiMessage&, std::uint8_t) {};

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
  // Combined switch to (style 0, VarB): stopped -> immediate regardless of boundary.
  b.cmd(Param::kStyleSwitch, 0, static_cast<std::int32_t>(SectionType::kVarB), 0, Op::kDo,
        Boundary::kNextBar);
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
  // boundary kNextBar while playing -> quantized to the next bar, no immediate
  // section event.
  b.cmd(Param::kStyleSwitch, 0, static_cast<std::int32_t>(SectionType::kVarB), 0, Op::kDo,
        Boundary::kNextBar);
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
  // boundary kImmediate while playing -> hard cut now.
  b.cmd(Param::kStyleSwitch, 0, static_cast<std::int32_t>(SectionType::kVarB), 1, Op::kDo,
        Boundary::kImmediate);
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
  const char* expected[] = {"basic", "pop",     "rock",  "ballad", "funk",   "disco",
                            "house", "swing",   "bossa", "samba",  "reggae", "country",
                            "blues", "shuffle", "latin", "motown"};
  // Every builtin must resolve the full twelve-section vocabulary so the chooser
  // and the section stepper always have a consistent set to work with.
  const SectionType full_set[] = {
      SectionType::kIntro1, SectionType::kIntro2, SectionType::kVarA,    SectionType::kVarB,
      SectionType::kVarC,   SectionType::kVarD,   SectionType::kFillA,   SectionType::kFillB,
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
// key-aware, without leaning on any builtin's pattern data. `transpose`
// defaults to 0 (Phase-6 Theme 3 Item #1) so every pre-existing call site
// stays byte-identical.
int resolve_one(TrackRole role, RolePolicy policy, NoteSource src, std::int8_t tone,
                std::int8_t octave, const Key& key, const ChordState& chord,
                std::int8_t transpose = 0) {
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
  a.set_master_transpose(transpose);
  a.on_transport_start();
  int note = -1;
  a.on_tick(0, key, chord, [&](std::uint8_t, TickOffset, const MidiMessage& m, std::uint8_t) {
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

// Phase-6 Theme 3 Item #1 (global transpose, docs/reflections/phase6-theme3-
// master-transpose-scope.md, Decisions 1/2/3/5): resolve()'s own
// `transpose` parameter, threaded through Arranger::set_master_transpose ->
// on_tick -> resolve(), added to the ABSOLUTE note in the kInterval/
// kScaleDegree/kChordTone branches, kFixed exempt, drop-not-fold at the
// [0,127] boundary -- the same convention as every other clamp on these
// lines.
void test_master_transpose_resolve() {
  const Key c_major{.root_pc = 0, .mode = Mode::kMajor};
  const Key a_minor{.root_pc = 9, .mode = Mode::kMinor};
  const ChordState c_maj{.root_pc = 0, .quality = ChordQuality::kMaj, .valid = true};
  const ChordState c_min{.root_pc = 0, .quality = ChordQuality::kMin, .valid = true};
  constexpr int kLeadAnchor = 72;  // kRoleAnchor[kLead]
  const auto lead = [&](NoteSource src, std::int8_t tone, std::int8_t octave, const Key& key,
                        const ChordState& chord, std::int8_t transpose) {
    return resolve_one(TrackRole::kLead, RolePolicy::kChordTone, src, tone, octave, key, chord,
                       transpose);
  };

  // kChordTone: transpose lands on top of the resolved chord tone.
  CHECK(lead(NoteSource::kChordTone, 0, 0, c_major, c_maj, /*transpose=*/5) ==
        kLeadAnchor + 0 + 5);  // 77
  CHECK(lead(NoteSource::kChordTone, 1, 0, c_major, c_maj, /*transpose=*/-4) ==
        kLeadAnchor + 4 - 4);  // 72

  // kInterval: transpose lands on top of the signed semitone offset.
  CHECK(lead(NoteSource::kInterval, 14, 0, c_major, c_maj, /*transpose=*/-3) ==
        kLeadAnchor + 14 - 3);  // 83

  // kScaleDegree: transpose lands on top of the diatonic pitch.
  CHECK(lead(NoteSource::kScaleDegree, 1, 0, c_major, c_maj, /*transpose=*/2) ==
        kLeadAnchor + 2 + 2);  // 76

  // kFixed (drums/perc) is exempt REGARDLESS of transpose: resolve() returns
  // before `transpose` is ever consulted.
  CHECK(resolve_one(TrackRole::kDrums, RolePolicy::kFixed, NoteSource::kInterval, 38, 0, a_minor,
                    c_min, /*transpose=*/12) == 38);
  CHECK(resolve_one(TrackRole::kDrums, RolePolicy::kFixed, NoteSource::kInterval, 38, 0, a_minor,
                    c_min, /*transpose=*/-12) == 38);

  // Drop, don't fold, at the boundary: a note that lands exactly in range
  // WITHOUT transpose still resolves; adding a transpose that pushes it past
  // 0 or 127 drops it (-1), never wraps/clamps to the edge.
  CHECK(lead(NoteSource::kInterval, 0, -6, c_major, c_maj, /*transpose=*/0) ==
        0);  // 72 - 72 + 0 = 0, exactly in range
  CHECK(lead(NoteSource::kInterval, 0, -6, c_major, c_maj, /*transpose=*/-1) ==
        -1);  // one semitone of transpose pushes it below 0: dropped
  CHECK(lead(NoteSource::kInterval, 0, 4, c_major, c_maj, /*transpose=*/0) ==
        120);  // 72 + 48 + 0 = 120, exactly in range
  CHECK(lead(NoteSource::kInterval, 0, 4, c_major, c_maj, /*transpose=*/8) ==
        -1);  // 120 + 8 = 128: dropped, not folded back into range
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
  test_groove_insert_reproduces_groove_apply_math_at_default_last_slot();
  test_arp_insert_swallows_direct_note_and_plays_from_the_p5_pass();
  test_arp_insert_fires_on_own_rate_grid_even_off_the_style_step_grid();
  test_arp_insert_holds_chord_through_silence_and_replaces_not_accumulates();
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
  test_style_beats_per_bar_field_and_explicit_ticks_per_bar_change_wrap();
  test_request_style_immediate_swaps_beats_per_bar_field();
  test_request_style_deferred_swaps_beats_per_bar_field_at_bar_boundary();
  test_immediate_switch_when_stopped();
  test_seamless_style_switch();
  test_style_switch_immediate_when_stopped();
  test_style_switch_next_bar_when_playing();
  test_style_switch_immediate_while_playing();
  test_style_switch_bad_index_warns();
  test_builtin_styles_registered();
  test_builtin_styles_play_roles();
  test_note_source_vocabulary();
  test_master_transpose_resolve();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_arranger: all OK\n");
  }
  return arrangrr::test::failures();
}
