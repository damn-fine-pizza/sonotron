#include "arrangrr/arp/arpeggiator.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"
#include "test_harness.hpp"

namespace {

using namespace arrangrr;

struct Cap {
  std::uint8_t note;
  std::uint8_t vel;
  TickOffset gate;
};
using Caps = StaticVector<Cap, 64>;

void collect(ArpeggiatorEngine& a, Tick from, Tick to, Caps& out) {
  for (Tick t = from; t < to; ++t) {
    a.on_tick(t, [&](std::uint8_t n, std::uint8_t v, TickOffset g) {
      CHECK(out.push_back(Cap{.note = n, .vel = v, .gate = g}));
    });
  }
}

ArpeggiatorParams make(ArpDirection dir, std::uint8_t octaves = 1, std::uint8_t gate = 50) {
  ArpeggiatorParams p;
  p.rate = ArpRate::kSixteenth;  // 240 ticks/step
  p.direction = dir;
  p.octaves = octaves;
  p.gate = gate;
  return p;
}

void test_arp_up_and_gate() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kUp, 1, 50));
  a.note_on(60, 100);
  a.note_on(64, 90);
  a.note_on(67, 80);
  Caps c;
  collect(a, 0, 240 * 4, c);  // four 16th steps
  CHECK(c.size() == 4);
  CHECK(c[0].note == 60 && c[1].note == 64 && c[2].note == 67 && c[3].note == 60);  // wraps
  CHECK(c[0].vel == 100 && c[1].vel == 90);
  CHECK(c[0].gate == 120);  // 240 * 50%
}

void test_arp_down() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kDown));
  a.note_on(60, 100);
  a.note_on(64, 100);
  a.note_on(67, 100);
  Caps c;
  collect(a, 0, 240 * 3, c);
  CHECK(c[0].note == 67 && c[1].note == 64 && c[2].note == 60);
}

void test_arp_octaves() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kUp, 2));
  a.note_on(60, 100);
  a.note_on(64, 100);
  Caps c;
  collect(a, 0, 240 * 5, c);
  // up over 2 octaves: 60 64 72 76 then wrap to 60
  CHECK(c[0].note == 60 && c[1].note == 64 && c[2].note == 72 && c[3].note == 76 && c[4].note == 60);
}

void test_arp_updown() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kUpDown));
  a.note_on(60, 100);
  a.note_on(64, 100);
  a.note_on(67, 100);
  Caps c;
  collect(a, 0, 240 * 5, c);
  // 60 64 67 64 then back to 60 (period 4)
  CHECK(c[0].note == 60 && c[1].note == 64 && c[2].note == 67 && c[3].note == 64 && c[4].note == 60);
}

void test_arp_as_played() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kAsPlayed));
  a.note_on(67, 100);  // played high first
  a.note_on(60, 100);
  a.note_on(64, 100);
  Caps c;
  collect(a, 0, 240 * 3, c);
  CHECK(c[0].note == 67 && c[1].note == 60 && c[2].note == 64);  // insertion order preserved
}

void test_arp_only_on_grid() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kUp));
  a.note_on(60, 100);
  Caps c;
  collect(a, 1, 240, c);  // between grid boundaries: nothing fires
  CHECK(c.size() == 0);
  collect(a, 240, 241, c);  // the next boundary fires once
  CHECK(c.size() == 1 && c[0].note == 60);
}

void test_arp_latch() {
  ArpeggiatorEngine a;
  ArpeggiatorParams p = make(ArpDirection::kUp);
  p.latch = true;
  a.set_params(p);
  a.note_on(60, 100);
  a.note_on(64, 100);
  a.note_off(60);
  a.note_off(64);
  CHECK(a.active());  // latched: still playing after release
  CHECK(a.held_count() == 2);
  a.note_on(62, 100);  // a fresh key starts a new chord
  CHECK(a.held_count() == 1);
  Caps c;
  collect(a, 0, 240, c);
  CHECK(c[0].note == 62);
}

void test_arp_no_latch_release_stops() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kUp));
  a.note_on(60, 100);
  a.note_off(60);
  CHECK(!a.active());
  Caps c;
  collect(a, 0, 240 * 2, c);
  CHECK(c.size() == 0);
}

void test_arp_random_deterministic() {
  ArpeggiatorParams p = make(ArpDirection::kRandom);
  p.seed = 42;
  ArpeggiatorEngine a;
  a.set_params(p);
  a.note_on(60, 100);
  a.note_on(64, 100);
  a.note_on(67, 100);
  Caps c1;
  collect(a, 0, 240 * 8, c1);

  ArpeggiatorEngine b;
  b.set_params(p);
  b.note_on(60, 100);
  b.note_on(64, 100);
  b.note_on(67, 100);
  Caps c2;
  collect(b, 0, 240 * 8, c2);

  CHECK(c1.size() == c2.size() && c1.size() == 8);
  bool same = true;
  bool any_off_root = false;
  for (std::size_t i = 0; i < c1.size(); ++i) {
    same = same && (c1[i].note == c2[i].note);
    any_off_root = any_off_root || (c1[i].note != 60);
  }
  CHECK(same);         // same seed -> identical sequence
  CHECK(any_off_root);  // and it actually varies the note
}

// Live keyboard arp through the full engine: held input notes are captured and
// replayed rhythmically on the arp's output route while the transport runs.
void test_arp_engine_live() {
  test::TestEngine e;
  StaticVector<OutEvent, 256> ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };
  auto cmd = [&](Param p, std::int32_t a, std::int32_t b) {
    Command c{.op = Op::kSet, .param = p, .idx = 0, .a = a, .b = b, .c = 0};
    e.push_command(c, sink);
  };
  cmd(Param::kArpOut, 0 | (1 << 8), 0);  // out port 0, channel 2 (0-based 1)
  cmd(Param::kArp, static_cast<std::int32_t>(ArpField::kRate),
      static_cast<std::int32_t>(ArpRate::kEighth));
  cmd(Param::kArp, static_cast<std::int32_t>(ArpField::kEnabled), 1);

  auto feed = [&](std::uint8_t status, std::uint8_t d1, std::uint8_t d2) {
    const std::uint8_t b[3] = {status, d1, d2};
    e.push_midi_in(0, Span<const std::uint8_t>(b, 3), sink);
  };
  cmd(Param::kTransportStart, 0, 0);  // the arp only captures while playing
  feed(0x90, 60, 100);                // hold C E G on input port 0
  feed(0x90, 64, 100);
  feed(0x90, 67, 100);
  e.advance_ticks(480 * 4, sink);

  int arp_ons = 0;
  std::uint8_t first = 0;
  for (const OutEvent& o : ev) {
    if (o.kind == OutEvent::Kind::kMidi && o.msg.type() == midi::kNoteOn && o.msg.channel() == 1) {
      if (arp_ons == 0) {
        first = o.msg.d1;
      }
      ++arp_ons;
    }
  }
  CHECK(arp_ons >= 3);  // rhythmic sequence, one note per 1/8
  CHECK(first == 60);   // up direction starts on the lowest held note
}

// With the arp enabled but the transport STOPPED, keyboard notes on the arp
// input port must still ROUTE through (the arp only arpeggiates while playing,
// so it must not swallow the keyboard when it is idle — otherwise held keys
// light up but never sound). See the "keys light up but don't sound" bug.
void test_arp_stopped_passes_through() {
  test::TestEngine e;
  StaticVector<OutEvent, 64> ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };
  auto cmd = [&](Op op, Param p, std::int32_t a, std::int32_t b, std::int32_t c) {
    Command k{.op = op, .param = p, .idx = 0, .a = a, .b = b, .c = c};
    e.push_command(k, sink);
  };

  // Route input port 0 -> output port 1 (pass everything) so a passthrough note
  // is observable at the sink.
  cmd(Op::kDo, Param::kRouteAdd, 0 | ((-1 & 0xFF) << 8), 1 | ((-1 & 0xFF) << 8),
      static_cast<std::int32_t>(route_pass::kAll));
  cmd(Op::kSet, Param::kArp, static_cast<std::int32_t>(ArpField::kEnabled), 1, 0);
  CHECK(e.arp_enabled());
  CHECK(!e.transport().playing());  // transport is stopped

  const std::uint8_t on[3] = {0x90, 60, 100};
  e.push_midi_in(0, Span<const std::uint8_t>(on, 3), sink);

  // The note must reach the routed output, not be captured by the idle arp.
  bool routed = false;
  for (const OutEvent& o : ev) {
    if (o.kind == OutEvent::Kind::kMidi && o.port == 1 && o.msg.type() == midi::kNoteOn &&
        o.msg.d1 == 60) {
      routed = true;
    }
  }
  CHECK(routed);
}

// Terminal key auto-repeat re-fires note-on for a still-held key. With latch
// OFF, N repeated note-ons followed by a SINGLE note-off must return the arp to
// idle (a distinct-note set, not a raw counter, so repeats cannot strand it).
void test_arp_autorepeat_single_release() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kUp));  // latch defaults off
  for (int i = 0; i < 20; ++i) {
    a.note_on(60, 100);  // auto-repeat stream for one physically-held key
  }
  CHECK(a.held_count() == 1);  // deduplicated
  CHECK(a.active());
  a.note_off(60);  // the single real release
  CHECK(!a.active());  // idle again — not stranded by the repeats
  CHECK(a.held_count() == 0);
  Caps c;
  collect(a, 0, 240 * 4, c);
  CHECK(c.size() == 0);  // nothing fires
}

// Auto-repeat under latch: a burst of repeats for the same key, then a full
// release, must NOT leave physical bookkeeping stuck. A subsequent fresh press
// starts a NEW chord (physical_empty() honest despite the repeats).
void test_arp_autorepeat_latch_reset() {
  ArpeggiatorEngine a;
  ArpeggiatorParams p = make(ArpDirection::kUp);
  p.latch = true;
  a.set_params(p);
  for (int i = 0; i < 10; ++i) {
    a.note_on(60, 100);
  }
  a.note_off(60);       // all keys physically up
  CHECK(a.active());    // latched: still sounding
  a.note_on(64, 100);   // a genuinely fresh press starts a new chord
  CHECK(a.held_count() == 1);
  Caps c;
  collect(a, 0, 240, c);
  CHECK(c[0].note == 64);
}

// `arp off` (set_arp_enabled false -> panic) must immediately stop output even
// while the transport keeps running.
void test_arp_disable_stops_output() {
  test::TestEngine e;
  StaticVector<OutEvent, 256> ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };
  auto cmd = [&](Param p, std::int32_t a, std::int32_t b) {
    Command c{.op = Op::kSet, .param = p, .idx = 0, .a = a, .b = b, .c = 0};
    e.push_command(c, sink);
  };
  cmd(Param::kArpOut, 0, 0);
  cmd(Param::kArp, static_cast<std::int32_t>(ArpField::kRate),
      static_cast<std::int32_t>(ArpRate::kEighth));
  cmd(Param::kArp, static_cast<std::int32_t>(ArpField::kEnabled), 1);
  Command start{.op = Op::kDo, .param = Param::kTransportStart, .idx = 0, .a = 0, .b = 0, .c = 0};
  e.push_command(start, sink);

  const std::uint8_t on[3] = {0x90, 60, 100};
  e.push_midi_in(0, Span<const std::uint8_t>(on, 3), sink);  // captured while playing
  e.advance_ticks(480 * 2, sink);

  // Disable the arp; from here on it must emit nothing more.
  cmd(Param::kArp, static_cast<std::int32_t>(ArpField::kEnabled), 0);
  const std::size_t after_disable = ev.size();
  e.advance_ticks(480 * 4, sink);  // keep the transport running

  int new_note_ons = 0;
  for (std::size_t i = after_disable; i < ev.size(); ++i) {
    if (ev[i].kind == OutEvent::Kind::kMidi && ev[i].msg.type() == midi::kNoteOn &&
        ev[i].msg.d2 > 0) {
      ++new_note_ons;
    }
  }
  CHECK(new_note_ons == 0);  // silenced immediately, no machine-gun after off
}

// arp_rate_ticks maps every rate to its PPQN grid; also covers set_field(kRate).
void test_arp_rate_ticks_all() {
  CHECK(arp_rate_ticks(ArpRate::kQuarter) == kPpqn);
  CHECK(arp_rate_ticks(ArpRate::kEighth) == kPpqn / 2);
  CHECK(arp_rate_ticks(ArpRate::kSixteenth) == kPpqn / 4);
  CHECK(arp_rate_ticks(ArpRate::kThirtySecond) == kPpqn / 8);

  // A quarter-note arp emits once per kPpqn ticks.
  ArpeggiatorEngine a;
  ArpeggiatorParams p = make(ArpDirection::kUp);
  p.rate = ArpRate::kQuarter;
  a.set_params(p);
  a.note_on(60, 100);
  Caps c;
  collect(a, 0, kPpqn * 2, c);
  CHECK(c.size() == 2);  // two quarter-note boundaries in two beats
}

// set_field drives every ArpField arm and clamps out-of-range values.
void test_arp_set_field_all_and_clamp() {
  ArpeggiatorEngine a;
  a.set_field(ArpField::kEnabled, 1);  // no-op in the engine (routing concern)

  a.set_field(ArpField::kRate, static_cast<std::int32_t>(ArpRate::kEighth));
  CHECK(a.params().rate == ArpRate::kEighth);
  a.set_field(ArpField::kRate, 99);  // clamp high -> last rate
  CHECK(a.params().rate == static_cast<ArpRate>(kArpRateCount - 1));
  a.set_field(ArpField::kRate, -5);  // clamp low -> first rate
  CHECK(a.params().rate == static_cast<ArpRate>(0));

  a.set_field(ArpField::kDirection, static_cast<std::int32_t>(ArpDirection::kDownUp));
  CHECK(a.params().direction == ArpDirection::kDownUp);
  a.set_field(ArpField::kDirection, 999);  // clamp high
  CHECK(a.params().direction == static_cast<ArpDirection>(kArpDirectionCount - 1));

  a.set_field(ArpField::kOctaves, 3);
  CHECK(a.params().octaves == 3);
  a.set_field(ArpField::kOctaves, 0);  // clamp to 1
  CHECK(a.params().octaves == 1);
  a.set_field(ArpField::kOctaves, 10);  // clamp to 4
  CHECK(a.params().octaves == 4);

  a.set_field(ArpField::kGate, 60);
  CHECK(a.params().gate == 60);
  a.set_field(ArpField::kGate, -5);  // clamp to 0
  CHECK(a.params().gate == 0);
  a.set_field(ArpField::kGate, 250);  // clamp to 100
  CHECK(a.params().gate == 100);

  a.set_field(ArpField::kLatch, 1);
  CHECK(a.params().latch);
  a.set_field(ArpField::kLatch, 0);
  CHECK(!a.params().latch);

  a.set_field(ArpField::kSeed, 12345);
  CHECK(a.params().seed == 12345u);
  a.set_field(ArpField::kSeed, -1);  // negative clamps to 0
  CHECK(a.params().seed == 0u);
}

// note_on/note_off ignore out-of-range notes (>127) without touching state.
void test_arp_out_of_range_notes() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kUp));
  a.note_on(200, 100);  // > 127: ignored
  CHECK(a.held_count() == 0);
  CHECK(!a.active());
  a.note_on(60, 100);
  a.note_off(200);  // > 127: no physical_clear, does not remove the real note
  CHECK(a.held_count() == 1);
}

// Velocity 0 on note_on is bumped to 1 (a played note is never silent).
void test_arp_velocity_zero_bumped() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kUp));
  a.note_on(60, 0);
  Caps c;
  collect(a, 0, 240, c);
  CHECK(c.size() == 1 && c[0].vel == 1);
}

// The playable set is capped at kMaxArpNotes; extra distinct notes are dropped.
void test_arp_max_notes_cap() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kUp));
  for (std::uint8_t i = 0; i < kMaxArpNotes + 3; ++i) {
    a.note_on(static_cast<std::uint8_t>(48 + i), 100);
  }
  CHECK(a.held_count() == kMaxArpNotes);  // saturated, no overflow
}

// Removing a MIDDLE held note (no latch) shifts the tail down: exercises the
// remove() shift loop.
void test_arp_remove_middle() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kAsPlayed));
  a.note_on(60, 100);
  a.note_on(64, 100);
  a.note_on(67, 100);
  a.note_off(64);  // drop the middle one
  CHECK(a.held_count() == 2);
  Caps c;
  collect(a, 0, 240 * 2, c);
  CHECK(c.size() == 2 && c[0].note == 60 && c[1].note == 67);
}

// Notes pressed out of order are sorted ascending for every non-as-played
// direction: exercises the insertion-sort swap in build_sequence.
void test_arp_up_sorts_unordered_input() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kUp));
  a.note_on(67, 70);  // highest played first
  a.note_on(60, 90);
  a.note_on(64, 80);
  Caps c;
  collect(a, 0, 240 * 3, c);
  // Ascending regardless of press order, velocities travel with their notes.
  CHECK(c[0].note == 60 && c[0].vel == 90);
  CHECK(c[1].note == 64 && c[1].vel == 80);
  CHECK(c[2].note == 67 && c[2].vel == 70);
}

// kDownUp: descend the full range, then climb back through the interior notes.
void test_arp_downup() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kDownUp));
  a.note_on(60, 100);
  a.note_on(64, 100);
  a.note_on(67, 100);
  Caps c;
  collect(a, 0, 240 * 5, c);
  // 67 64 60 (down), then 64 (interior climb), then wrap to 67.
  CHECK(c[0].note == 67 && c[1].note == 64 && c[2].note == 60 && c[3].note == 64 && c[4].note == 67);
}

// octaves>1 with a high base note: transposed copies above 127 are skipped.
void test_arp_octave_overflow_skipped() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kUp, 3));  // 3 octaves
  a.note_on(120, 100);                       // 120, 132(skip), 144(skip)
  Caps c;
  collect(a, 0, 240 * 2, c);
  // Only the base copy is in range; the sequence has length 1 and repeats.
  CHECK(c.size() == 2 && c[0].note == 120 && c[1].note == 120);
}

// Turning latch OFF while nothing is physically held drops the latched chord
// immediately (set_latch clear branch).
void test_arp_latch_off_drops_chord() {
  ArpeggiatorEngine a;
  ArpeggiatorParams p = make(ArpDirection::kUp);
  p.latch = true;
  a.set_params(p);
  a.note_on(60, 100);
  a.note_off(60);      // physically released, but latched
  CHECK(a.active());
  a.set_field(ArpField::kLatch, 0);  // latch off with nothing held -> silence
  CHECK(!a.active());
  Caps c;
  collect(a, 0, 240 * 2, c);
  CHECK(c.size() == 0);
}

// Turning latch OFF while a key is STILL physically held keeps the chord.
void test_arp_latch_off_while_held_keeps() {
  ArpeggiatorEngine a;
  ArpeggiatorParams p = make(ArpDirection::kUp);
  p.latch = true;
  a.set_params(p);
  a.note_on(60, 100);  // still physically down
  a.set_field(ArpField::kLatch, 0);  // latch off but key held -> keeps sounding
  CHECK(a.active());
  CHECK(a.held_count() == 1);
}

// panic() clears both the playable set and physical bookkeeping, so a fresh
// press afterwards starts cleanly.
void test_arp_panic_clears_all() {
  ArpeggiatorEngine a;
  ArpeggiatorParams p = make(ArpDirection::kUp);
  p.latch = true;
  a.set_params(p);
  a.note_on(60, 100);
  a.note_on(64, 100);
  a.panic();
  CHECK(!a.active() && a.held_count() == 0);
  a.note_on(67, 100);  // physical_empty() honest -> fresh chord
  CHECK(a.held_count() == 1);
  Caps c;
  collect(a, 0, 240, c);
  CHECK(c[0].note == 67);
}

// on_tick with no held notes resets the step counter and emits nothing.
void test_arp_on_tick_empty_resets_step() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kUp));
  Caps c;
  collect(a, 0, 240 * 3, c);  // nothing held
  CHECK(c.size() == 0);
  // After holding a note, the first grid boundary still starts at step 0.
  a.note_on(64, 100);
  collect(a, 0, 1, c);
  CHECK(c.size() == 1 && c[0].note == 64);
}

}  // namespace

int main() {
  test_arp_rate_ticks_all();
  test_arp_set_field_all_and_clamp();
  test_arp_out_of_range_notes();
  test_arp_velocity_zero_bumped();
  test_arp_max_notes_cap();
  test_arp_remove_middle();
  test_arp_up_sorts_unordered_input();
  test_arp_downup();
  test_arp_octave_overflow_skipped();
  test_arp_latch_off_drops_chord();
  test_arp_latch_off_while_held_keeps();
  test_arp_panic_clears_all();
  test_arp_on_tick_empty_resets_step();
  test_arp_up_and_gate();
  test_arp_down();
  test_arp_octaves();
  test_arp_updown();
  test_arp_as_played();
  test_arp_only_on_grid();
  test_arp_latch();
  test_arp_no_latch_release_stops();
  test_arp_random_deterministic();
  test_arp_engine_live();
  test_arp_stopped_passes_through();
  test_arp_autorepeat_single_release();
  test_arp_autorepeat_latch_reset();
  test_arp_disable_stops_output();
  return arrangrr::test::failures();
}
