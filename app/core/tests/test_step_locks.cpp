#include "arrangrr/timeline/timeline.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"

// Step-sequencer parameter-locks (roadmap item 5): probability, ratchet, micro,
// tie. The hard invariant is that a fully-neutral step reproduces the original
// on_tick output byte-for-byte; every non-neutral behaviour is deterministic
// (D16 seeded position hash, D27 integer ticks, D29 off-before-on preserved).

namespace {

using namespace arrangrr;

// One scheduled emission captured straight out of Timeline::on_tick.
struct Hit {
  std::uint8_t port;
  TickOffset delay;
  std::uint8_t type;
  std::uint8_t note;
  std::uint8_t vel;
};
using Hits = StaticVector<Hit, 128>;

// Drives one Timeline::on_tick and captures every emission.
Hits fire(Timeline& tl, Tick transport_tick) {
  Hits hits;
  tl.on_tick(transport_tick, [&](std::uint8_t port, TickOffset delay, const MidiMessage& msg) {
    CHECK(hits.push_back(Hit{port, delay, msg.type(), msg.d1, msg.d2}));
  });
  return hits;
}

int count_type(const Hits& hits, std::uint8_t type) {
  int n = 0;
  for (const Hit& h : hits) {
    if (h.type == type) {
      ++n;
    }
  }
  return n;
}

// A neutral step (probability=100, ratchet=1, micro=0, tie=false) emits exactly
// one note-on at delay 0 and one note-off at delay=gate — the original path.
void test_neutral_is_byte_identical() {
  Timeline plain;
  CHECK(plain.add_track(TrackRole::kLead, 1, 3) == 0);
  CHECK(plain.set_step(0, 0, 60, 100, 120));  // original 5-arg call
  const Hits a = fire(plain, 0);

  Timeline explicit_neutral;
  CHECK(explicit_neutral.add_track(TrackRole::kLead, 1, 3) == 0);
  // Every param-lock stated at its neutral value.
  CHECK(explicit_neutral.set_step(0, 0, 60, 100, 120, /*prob=*/100, /*ratchet=*/1,
                                  /*micro=*/0, /*tie=*/false));
  const Hits b = fire(explicit_neutral, 0);

  CHECK(a.size() == 2);
  CHECK(b.size() == 2);
  for (std::size_t i = 0; i < a.size() && i < b.size(); ++i) {
    CHECK(a[i].port == b[i].port && a[i].delay == b[i].delay && a[i].type == b[i].type &&
          a[i].note == b[i].note && a[i].vel == b[i].vel);
  }
  // Exact original shape: on at 0, off at gate.
  CHECK(a[0].type == midi::kNoteOn && a[0].delay == 0 && a[0].note == 60 && a[0].vel == 100);
  CHECK(a[1].type == midi::kNoteOff && a[1].delay == 120 && a[1].note == 60);
}

// Probability is a deterministic position hash: same seed (track index) and
// position (global step) => same verdict. 0 never fires, 100 always fires.
void test_probability_is_deterministic() {
  constexpr std::uint32_t kPositions = 64;

  auto run = [](std::uint8_t probability, StaticVector<bool, kPositions>& fired) {
    Timeline tl;
    CHECK(tl.add_track(TrackRole::kLead, 0, 0) == 0);
    CHECK(tl.set_length(0, 1));  // slot 0 fires on every global step
    CHECK(tl.set_step(0, 0, 60, 100, 120, probability));
    for (std::uint32_t p = 0; p < kPositions; ++p) {
      const Hits h = fire(tl, p * kTicksPerStep);
      CHECK(fired.push_back(count_type(h, midi::kNoteOn) == 1));
    }
  };

  StaticVector<bool, kPositions> half_a;
  StaticVector<bool, kPositions> half_b;
  run(50, half_a);
  run(50, half_b);
  // Byte-for-byte reproducible across two independent runs (golden-lockable).
  CHECK(half_a.size() == kPositions && half_b.size() == kPositions);
  int fires = 0;
  for (std::uint32_t p = 0; p < kPositions; ++p) {
    CHECK(half_a[p] == half_b[p]);
    fires += half_a[p] ? 1 : 0;
  }
  // A real mixture: not all, not none (guards against a degenerate hash).
  CHECK(fires > 0 && fires < static_cast<int>(kPositions));

  StaticVector<bool, kPositions> never;
  StaticVector<bool, kPositions> always;
  run(0, never);
  run(100, always);
  for (std::uint32_t p = 0; p < kPositions; ++p) {
    CHECK(!never[p]);  // 0 % never fires
    CHECK(always[p]);  // 100 % always fires (neutral)
  }
}

// Ratchet emits exactly N evenly-spaced hits inside the step.
void test_ratchet_evenly_spaced() {
  Timeline tl;
  CHECK(tl.add_track(TrackRole::kLead, 0, 0) == 0);
  CHECK(tl.set_step(0, 0, 60, 100, 120, /*prob=*/100, /*ratchet=*/4));
  const Hits h = fire(tl, 0);

  CHECK(count_type(h, midi::kNoteOn) == 4);
  CHECK(count_type(h, midi::kNoteOff) == 4);
  // slice = 240 / 4 = 60; sub-hits land on the grid at 0, 60, 120, 180.
  const TickOffset expect[4] = {0, 60, 120, 180};
  int seen = 0;
  for (const Hit& hit : h) {
    if (hit.type != midi::kNoteOn) {
      continue;
    }
    CHECK(hit.delay == expect[seen]);
    ++seen;
  }
  CHECK(seen == 4);

  // The bound holds: a ratchet above the cap is clamped to kMaxRatchet.
  CHECK(tl.set_step(0, 0, 60, 100, 120, 100, 99));
  const Hits capped = fire(tl, 0);
  CHECK(count_type(capped, midi::kNoteOn) == kMaxRatchet);
}

// Micro is a FORWARD-only lay-back: it pushes note-on AND note-off together by
// the same amount, so the gate length is preserved. Anticipation (a negative
// push) is deferred — it needs step look-ahead — so micro is 0..127 only.
void test_micro_shifts_gate_preserved() {
  Timeline tl;
  CHECK(tl.add_track(TrackRole::kLead, 0, 0) == 0);
  CHECK(tl.set_step(0, 0, 60, 100, 120, /*prob=*/100, /*ratchet=*/1, /*micro=*/10));
  const Hits pos = fire(tl, 0);
  CHECK(pos.size() == 2);
  CHECK(pos[0].type == midi::kNoteOn && pos[0].delay == 10);
  CHECK(pos[1].type == midi::kNoteOff && pos[1].delay == 130);  // 10 + 120, gate kept

  // A larger forward push still preserves the gate (on and off move together).
  CHECK(tl.set_step(0, 0, 60, 100, 120, 100, 1, /*micro=*/60));
  const Hits far = fire(tl, 0);
  CHECK(far.size() == 2);
  CHECK(far[0].type == midi::kNoteOn && far[0].delay == 60);
  CHECK(far[1].type == midi::kNoteOff && far[1].delay == 180);  // 60 + 120
}

// Tie bridges the note-off one full step past the gate, holding into the next
// step instead of retriggering.
void test_tie_bridges_into_next_step() {
  Timeline tl;
  CHECK(tl.add_track(TrackRole::kLead, 0, 0) == 0);
  CHECK(tl.set_step(0, 0, 60, 100, 120, /*prob=*/100, /*ratchet=*/1, /*micro=*/0, /*tie=*/true));
  const Hits h = fire(tl, 0);
  CHECK(h.size() == 2);
  CHECK(h[0].type == midi::kNoteOn && h[0].delay == 0);
  // 120 gate + one full step (240) = 360, past the next step boundary at 240.
  CHECK(h[1].type == midi::kNoteOff);
  CHECK(h[1].delay == static_cast<TickOffset>(120 + kTicksPerStep));
  CHECK(static_cast<std::uint32_t>(h[1].delay) > kTicksPerStep);
}

// Real tie: when the tied step is followed by the SAME note, the next step's
// emission is suppressed so the bridged note holds (no retrigger). When it is a
// different note (or empty), the next step fires normally.
void test_tie_suppresses_same_note_next_step() {
  // Same note next step: step 0 ties into step 1 on note 60 -> step 1 is held.
  Timeline same;
  CHECK(same.add_track(TrackRole::kLead, 0, 0) == 0);
  CHECK(same.set_length(0, 2));
  CHECK(same.set_step(0, 0, 60, 100, 120, /*prob=*/100, /*ratchet=*/1, /*micro=*/0, /*tie=*/true));
  CHECK(same.set_step(0, 1, 60, 100, 120));
  const Hits s0 = fire(same, 0);                 // tied step fires the bridge
  const Hits s1 = fire(same, kTicksPerStep);     // held step must be silent
  CHECK(count_type(s0, midi::kNoteOn) == 1);
  CHECK(s0[0].type == midi::kNoteOn && s0[0].note == 60);
  CHECK(s1.size() == 0);  // suppressed: no second note-on, no retrigger

  // Different note next step: no collision, the next step fires as usual.
  Timeline diff;
  CHECK(diff.add_track(TrackRole::kLead, 0, 0) == 0);
  CHECK(diff.set_length(0, 2));
  CHECK(diff.set_step(0, 0, 60, 100, 120, /*prob=*/100, /*ratchet=*/1, /*micro=*/0, /*tie=*/true));
  CHECK(diff.set_step(0, 1, 64, 100, 120));
  const Hits d0 = fire(diff, 0);
  const Hits d1 = fire(diff, kTicksPerStep);
  CHECK(count_type(d0, midi::kNoteOn) == 1 && d0[0].note == 60);
  CHECK(count_type(d1, midi::kNoteOn) == 1 && d1[0].note == 64);

  // Empty next step: bridge holds and nothing else fires (baseline behaviour).
  Timeline empty;
  CHECK(empty.add_track(TrackRole::kLead, 0, 0) == 0);
  CHECK(empty.set_length(0, 2));
  CHECK(empty.set_step(0, 0, 60, 100, 120, /*prob=*/100, /*ratchet=*/1, /*micro=*/0, /*tie=*/true));
  const Hits e1 = fire(empty, kTicksPerStep);
  CHECK(e1.size() == 0);
}

// ---- ABI path: the extended kTrackStep encoding decodes to the same locks ----

using Events = StaticVector<OutEvent, 256>;

struct Player {
  Engine e;
  Events ev;
  void cmd(const Command& c) {
    e.push_command(c, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void advance(std::uint32_t n) {
    e.advance_ticks(n, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  int count(std::uint8_t type, std::uint8_t note) const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kMidi && o.msg.type() == type && o.msg.d1 == note) {
        ++n;
      }
    }
    return n;
  }
};

Command track_new(std::uint8_t port, std::uint8_t channel) {
  Command c;
  c.param = Param::kTrackNew;
  c.a = static_cast<std::int32_t>(TrackRole::kLead);
  c.b = port | (channel << 8);
  return c;
}

// Encodes a kTrackStep exactly as the host shell does.
Command track_step(std::uint16_t track, std::int32_t step, std::uint8_t note, std::uint8_t vel,
                   std::uint16_t gate, bool locks, std::uint8_t prob, std::uint8_t ratchet,
                   std::uint8_t micro, bool tie) {
  Command c;
  c.param = Param::kTrackStep;
  c.idx = track;
  c.a = step;
  c.b = note | (vel << 8);
  c.c = gate;
  if (locks) {
    c.b |= static_cast<std::int32_t>(static_cast<std::uint32_t>(prob) << 16) |
           static_cast<std::int32_t>(static_cast<std::uint32_t>(ratchet) << 24) |
           static_cast<std::int32_t>(tie ? (1u << 28) : 0u);
    c.c |= static_cast<std::int32_t>((static_cast<std::uint32_t>(micro) & 0xFFu) << 16) |
           static_cast<std::int32_t>(0x80000000u);
  }
  return c;
}

void test_abi_extended_encoding() {
  // Short form and neutral-valued extended form must be identical (one hit).
  Player shortf;
  shortf.cmd(track_new(0, 0));
  shortf.cmd(track_step(0, 0, 60, 100, 120, /*locks=*/false, 100, 1, 0, false));
  Command start;
  start.param = Param::kTransportStart;
  shortf.cmd(start);
  CHECK(shortf.count(midi::kNoteOn, 60) == 1);

  Player neutral_locks;
  neutral_locks.cmd(track_new(0, 0));
  neutral_locks.cmd(track_step(0, 0, 60, 100, 120, /*locks=*/true, 100, 1, 0, false));
  neutral_locks.cmd(start);
  CHECK(neutral_locks.count(midi::kNoteOn, 60) == 1);

  // Ratchet decoded off the wire: 3 hits in the first step.
  Player ratchet;
  ratchet.cmd(track_new(0, 0));
  ratchet.cmd(track_step(0, 0, 62, 100, 60, /*locks=*/true, 100, 3, 0, false));
  ratchet.cmd(start);
  ratchet.advance(kTicksPerStep - 1);  // stay inside step 0
  CHECK(ratchet.count(midi::kNoteOn, 62) == 3);
}

// End-to-end note-stack invariant through Engine::advance_ticks (NOT raw
// Timeline::on_tick): a mono ratchet must never have two note-ons of the same
// (channel, note) outstanding on the wire, and every note-on must be preceded
// by the note-off of the previous hit. This is the test that catches the
// duplicate-NoteOn / bare-reattack bug in schedule_pattern: before the fix the
// simulated "currently-on" count climbs to 2+ and a note-on arrives with a
// still-open note (on-count already 1).
void test_ratchet_note_stack_engine() {
  Player p;
  p.cmd(track_new(0, 0));  // port 0, channel 0
  // Mono ratchet: note 60, gate 60, ratchet 4 -> slice 60, sub_gate 60. Every
  // hit's off lands exactly on the next hit's on: a clean off-then-on chain.
  p.cmd(track_step(0, 0, 60, 100, 60, /*locks=*/true, 100, 4, 0, false));
  Command start;
  start.param = Param::kTransportStart;
  p.cmd(start);
  p.advance(2 * kTicksPerStep);  // cover the whole step and flush the final off

  int on = 0;      // currently-sounding count for (channel 0, note 60)
  int max_on = 0;  // peak concurrency
  int ons = 0;
  int offs = 0;
  for (const OutEvent& o : p.ev) {
    if (o.kind != OutEvent::Kind::kMidi || o.msg.d1 != 60) {
      continue;
    }
    if (o.msg.type() == midi::kNoteOn) {
      CHECK(on == 0);  // every on must be preceded by the previous off
      ++on;
      ++ons;
      if (on > max_on) {
        max_on = on;
      }
    } else if (o.msg.type() == midi::kNoteOff) {
      --on;
      ++offs;
      CHECK(on >= 0);  // never an unmatched off
    }
  }
  CHECK(max_on == 1);  // mono: never two note-ons outstanding
  CHECK(on == 0);      // balanced: every on released
  CHECK(ons == 4 && offs == 4);
}

}  // namespace

int main() {
  test_neutral_is_byte_identical();
  test_probability_is_deterministic();
  test_ratchet_evenly_spaced();
  test_micro_shifts_gate_preserved();
  test_tie_bridges_into_next_step();
  test_tie_suppresses_same_note_next_step();
  test_abi_extended_encoding();
  test_ratchet_note_stack_engine();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_step_locks: all OK\n");
  }
  return arrangrr::test::failures();
}
