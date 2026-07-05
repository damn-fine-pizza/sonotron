// Functional / interaction tests for the arpeggiator that pin behaviour the
// existing test_arp.cpp leaves open: seed VARIATION (not just same-seed
// determinism), gate rounding bounds, the max-octaves span, adding a note
// mid-pattern (m_step continuity + live resize), and the ENGINE-level
// no-stuck-note invariants — every arp note-on is balanced by its note-off,
// including across a transport STOP mid-pattern and across same-note retrigger.
// Deterministic, exact assertions, custom test.hpp harness, host/core only.

#include "arrangrr/arp/arpeggiator.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;

// ---- pure-engine capture helper (mirrors test_arp.cpp) ---------------------

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

// ---- pure-engine gap tests -------------------------------------------------

// Same seed -> identical (covered elsewhere). Here: a DIFFERENT seed yields a
// DIFFERENT but itself-reproducible stream. Pins "random" as a seeded hash, not
// entropy: re-running seed 7 twice is identical, and it diverges from seed 42.
void test_arp_random_different_seed_reproducible() {
  auto run = [](std::uint32_t seed, Caps& out) {
    ArpeggiatorParams p = make(ArpDirection::kRandom);
    p.seed = seed;
    ArpeggiatorEngine a;
    a.set_params(p);
    a.note_on(60, 100);
    a.note_on(64, 100);
    a.note_on(67, 100);
    collect(a, 0, 240 * 12, out);
  };
  Caps s7a;
  Caps s7b;
  Caps s42;
  run(7, s7a);
  run(7, s7b);
  run(42, s42);
  CHECK(s7a.size() == 12 && s7b.size() == 12 && s42.size() == 12);

  bool seven_reproducible = true;
  for (std::size_t i = 0; i < s7a.size(); ++i) {
    seven_reproducible = seven_reproducible && (s7a[i].note == s7b[i].note);
  }
  CHECK(seven_reproducible);  // seed 7 is a pure function of (seed, step)

  bool diverges = false;
  for (std::size_t i = 0; i < s7a.size(); ++i) {
    diverges = diverges || (s7a[i].note != s42[i].note);
  }
  CHECK(diverges);  // a different seed is a genuinely different pattern
}

// Gate rounding bounds: gate 0% must never emit a zero-length note (clamped to
// 1 tick); gate 100% emits the full step length. Pins the `gate_ticks < 1 ? 1`
// floor and the (step*gate)/100 arithmetic at both extremes.
void test_arp_gate_bounds() {
  {
    ArpeggiatorEngine a;
    a.set_params(make(ArpDirection::kUp, 1, /*gate=*/0));
    a.note_on(60, 100);
    Caps c;
    collect(a, 0, 240, c);
    CHECK(c.size() == 1 && c[0].gate == 1);  // never zero-length
  }
  {
    ArpeggiatorEngine a;
    a.set_params(make(ArpDirection::kUp, 1, /*gate=*/100));
    a.note_on(60, 100);
    Caps c;
    collect(a, 0, 240, c);
    CHECK(c.size() == 1 && c[0].gate == 240);  // full 16th step
  }
}

// Max octaves (4): the ascending stack spans base + 3 transposed copies, so two
// held notes give an 8-long sequence, then it wraps. Pins the octaves bound.
void test_arp_octaves_max_four() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kUp, /*octaves=*/4));
  a.note_on(60, 100);
  a.note_on(64, 100);
  Caps c;
  collect(a, 0, 240 * 9, c);
  const std::uint8_t expect[8] = {60, 64, 72, 76, 84, 88, 96, 100};
  CHECK(c.size() == 9);
  for (std::uint8_t i = 0; i < 8; ++i) {
    CHECK(c[i].note == expect[i]);
  }
  CHECK(c[8].note == 60);  // wraps back to the start of the octave stack
}

// Adding a note MID-pattern: the sequence resizes live and m_step keeps
// advancing (it is NOT reset while notes remain held), so the newly added note
// enters the cycle at the current step position rather than restarting.
void test_arp_add_note_mid_sequence() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kUp));  // 240 ticks/step
  a.note_on(60, 100);
  a.note_on(64, 100);
  Caps c;
  collect(a, 0, 240 * 2, c);  // steps 0,1 -> 60,64 ; m_step now == 2
  CHECK(c.size() == 2 && c[0].note == 60 && c[1].note == 64);

  a.note_on(67, 100);               // sequence becomes {60,64,67}
  collect(a, 240 * 2, 240 * 5, c);  // steps 2,3,4 over the resized sequence
  CHECK(c.size() == 5);
  CHECK(c[2].note == 67);  // step 2 % 3 -> index 2 -> the just-added note
  CHECK(c[3].note == 60);  // step 3 % 3 -> 0
  CHECK(c[4].note == 64);  // step 4 % 3 -> 1
}

// A single held note repeats forever, keeping its velocity; the sequence has
// length 1 so every step re-emits it. Baseline for the retrigger stuck-note
// invariant exercised at engine level below.
void test_arp_single_note_repeats() {
  ArpeggiatorEngine a;
  a.set_params(make(ArpDirection::kUp, 1, /*gate=*/100));
  a.note_on(60, 77);
  Caps c;
  collect(a, 0, 240 * 4, c);
  CHECK(c.size() == 4);
  for (std::uint8_t i = 0; i < 4; ++i) {
    CHECK(c[i].note == 60 && c[i].vel == 77);
  }
}

// ---- engine-level (live wiring) tests --------------------------------------

using Events = StaticVector<OutEvent, 512>;

bool is_arp_on(const OutEvent& o, std::uint8_t ch) {
  return o.kind == OutEvent::Kind::kMidi && o.msg.channel() == ch &&
         o.msg.type() == midi::kNoteOn && o.msg.d2 > 0;
}
bool is_arp_off(const OutEvent& o, std::uint8_t ch) {
  return o.kind == OutEvent::Kind::kMidi && o.msg.channel() == ch &&
         (o.msg.type() == midi::kNoteOff || (o.msg.type() == midi::kNoteOn && o.msg.d2 == 0));
}

// Balance check: replay the event stream, tracking outstanding on-count per
// pitch on the arp channel. Returns true iff no note-off ever underflows and
// every note is silent at the end (outstanding == 0 for all pitches).
bool notes_balanced(const Events& ev, std::uint8_t ch) {
  int outstanding[128] = {};
  for (const OutEvent& o : ev) {
    if (is_arp_off(o, ch)) {
      if (o.msg.d1 < 128) {
        --outstanding[o.msg.d1];
        if (outstanding[o.msg.d1] < 0) {
          return false;  // an off with no matching on: truncation / double-off
        }
      }
    } else if (is_arp_on(o, ch)) {
      if (o.msg.d1 < 128) {
        ++outstanding[o.msg.d1];
      }
    }
  }
  for (int v : outstanding) {
    if (v != 0) {
      return false;  // a sounding note never released: stuck note
    }
  }
  return true;
}

int count_ons(const Events& ev, std::uint8_t ch) {
  int n = 0;
  for (const OutEvent& o : ev) {
    if (is_arp_on(o, ch)) {
      ++n;
    }
  }
  return n;
}

// Transport STOP mid-pattern must leave NO stuck notes: the note-offs already
// scheduled (against stream time m_now, not the transport tick) still flush as
// stream time advances past their gate, so every arp note-on is balanced.
void test_arp_engine_stop_no_stuck_notes() {
  Engine e;
  Events ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };
  auto cmd = [&](Param p, std::int32_t a, std::int32_t b) {
    Command c{.op = Op::kSet, .param = p, .idx = 0, .a = a, .b = b, .c = 0};
    e.push_command(c, sink);
  };
  cmd(Param::kArpOut, 0 | (1 << 8), 0);  // out port 0, channel 1 (0-based)
  cmd(Param::kArp, static_cast<std::int32_t>(ArpField::kRate),
      static_cast<std::int32_t>(ArpRate::kEighth));                  // 480 ticks/step
  cmd(Param::kArp, static_cast<std::int32_t>(ArpField::kGate), 50);  // gate 240 ticks
  cmd(Param::kArp, static_cast<std::int32_t>(ArpField::kEnabled), 1);

  cmd(Param::kTransportStart, 0, 0);
  auto feed = [&](std::uint8_t d1) {
    const std::uint8_t b[3] = {0x90, d1, 100};
    e.push_midi_in(0, Span<const std::uint8_t>(b, 3), sink);
  };
  feed(60);
  feed(64);
  feed(67);
  e.advance_ticks(480 * 4 + 100, sink);  // fire several 1/8 steps, then land mid-step

  cmd(Param::kTransportStop, 0, 0);  // stop with a note likely still sounding
  e.advance_ticks(480, sink);        // let stream time flush the pending offs

  CHECK(count_ons(ev, 1) >= 3);  // it actually played a sequence
  CHECK(notes_balanced(ev, 1));  // and left nothing stuck after the stop
}

// Same-note retrigger at gate 100% (off lands exactly on the next on): the
// scheduler's tick-aware cancel_note_off must re-emit the compensating off
// BEFORE the next on so the note is not doubled nor stranded. A single held
// note replays every step; after stop everything must be balanced.
void test_arp_engine_retrigger_no_stuck_notes() {
  Engine e;
  Events ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };
  auto cmd = [&](Param p, std::int32_t a, std::int32_t b) {
    Command c{.op = Op::kSet, .param = p, .idx = 0, .a = a, .b = b, .c = 0};
    e.push_command(c, sink);
  };
  cmd(Param::kArpOut, 0 | (1 << 8), 0);
  cmd(Param::kArp, static_cast<std::int32_t>(ArpField::kRate),
      static_cast<std::int32_t>(ArpRate::kEighth));
  cmd(Param::kArp, static_cast<std::int32_t>(ArpField::kGate), 100);  // off == next on
  cmd(Param::kArp, static_cast<std::int32_t>(ArpField::kEnabled), 1);

  cmd(Param::kTransportStart, 0, 0);
  const std::uint8_t on[3] = {0x90, 60, 100};
  e.push_midi_in(0, Span<const std::uint8_t>(on, 3), sink);  // one held key
  e.advance_ticks(480 * 5, sink);

  cmd(Param::kTransportStop, 0, 0);
  e.advance_ticks(480, sink);

  CHECK(count_ons(ev, 1) >= 4);  // several retriggers of the same pitch
  CHECK(notes_balanced(ev, 1));  // no doubled on, no stuck note across retrigger
}

// Note-off placement: for a given arp note the off lands exactly `gate` ticks
// after its on. Pins fire_arp's (on now, off now+gate) scheduling at the wire.
void test_arp_engine_gate_offset_on_wire() {
  Engine e;
  Events ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };
  auto cmd = [&](Param p, std::int32_t a, std::int32_t b) {
    Command c{.op = Op::kSet, .param = p, .idx = 0, .a = a, .b = b, .c = 0};
    e.push_command(c, sink);
  };
  cmd(Param::kArpOut, 0 | (1 << 8), 0);
  cmd(Param::kArp, static_cast<std::int32_t>(ArpField::kRate),
      static_cast<std::int32_t>(ArpRate::kEighth));                  // step 480
  cmd(Param::kArp, static_cast<std::int32_t>(ArpField::kGate), 50);  // gate 240
  cmd(Param::kArp, static_cast<std::int32_t>(ArpField::kEnabled), 1);

  cmd(Param::kTransportStart, 0, 0);
  const std::uint8_t on[3] = {0x90, 60, 100};
  e.push_midi_in(0, Span<const std::uint8_t>(on, 3), sink);  // single pitch, no wrap ambiguity
  e.advance_ticks(480 * 2, sink);

  bool found_on = false;
  Tick on_tick = 0;
  bool checked = false;
  for (const OutEvent& o : ev) {
    if (!found_on && is_arp_on(o, 1) && o.msg.d1 == 60) {
      found_on = true;
      on_tick = o.tick;
    } else if (found_on && is_arp_off(o, 1) && o.msg.d1 == 60) {
      CHECK(o.tick == on_tick + 240);  // off is exactly one gate after the on
      checked = true;
      break;
    }
  }
  CHECK(found_on && checked);
}

// While PLAYING, a note on the arp input port is SWALLOWED (captured for the
// arp), not passed through the router: even with an all-pass route present, the
// raw input note must not reach the routed output, and the arp must hold it.
// This is the playing-side counterpart to test_arp_stopped_passes_through.
void test_arp_engine_swallows_while_playing() {
  Engine e;
  Events ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };
  auto cmd = [&](Op op, Param p, std::int32_t a, std::int32_t b, std::int32_t c) {
    Command k{.op = op, .param = p, .idx = 0, .a = a, .b = b, .c = c};
    e.push_command(k, sink);
  };
  // All-pass route input 0 -> output 1, so a passthrough WOULD be visible.
  cmd(Op::kDo, Param::kRouteAdd, 0 | ((-1 & 0xFF) << 8), 1 | ((-1 & 0xFF) << 8),
      static_cast<std::int32_t>(route_pass::kAll));
  cmd(Op::kSet, Param::kArpOut, 0 | (1 << 8), 0, 0);  // arp plays on port 0
  cmd(Op::kSet, Param::kArp, static_cast<std::int32_t>(ArpField::kEnabled), 1, 0);
  cmd(Op::kDo, Param::kTransportStart, 0, 0, 0);

  const std::uint8_t on[3] = {0x90, 60, 100};
  e.push_midi_in(0, Span<const std::uint8_t>(on, 3), sink);  // captured, not routed

  bool routed_raw = false;
  for (const OutEvent& o : ev) {
    if (o.kind == OutEvent::Kind::kMidi && o.port == 1 && o.msg.type() == midi::kNoteOn &&
        o.msg.d1 == 60) {
      routed_raw = true;
    }
  }
  CHECK(!routed_raw);       // the router did NOT see the swallowed note
  CHECK(e.arp().active());  // the arp captured it instead
  CHECK(e.arp().held_count() == 1);
}

}  // namespace

int main() {
  test_arp_random_different_seed_reproducible();
  test_arp_gate_bounds();
  test_arp_octaves_max_four();
  test_arp_add_note_mid_sequence();
  test_arp_single_note_repeats();
  test_arp_engine_stop_no_stuck_notes();
  test_arp_engine_retrigger_no_stuck_notes();
  test_arp_engine_gate_offset_on_wire();
  test_arp_engine_swallows_while_playing();
  return arrangrr::test::failures();
}
