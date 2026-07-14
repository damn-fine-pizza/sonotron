#include "arrangrr/engine.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "test.hpp"
#include "test_harness.hpp"

// Phase-1 runtime extraction: the Transport/clock/scheduler-drain-driven
// subset of components/arrangrr/tests/test_engine.cpp, moved here because the
// subject is now genuinely runtime concern (Corelli/Palladio's move plan).
// Harness is `arrangrr::test::TestEngine` (a Runtime<Engine> wrapper) rather
// than a bare `Engine`, since Transport/OutScheduler no longer live on Engine
// itself.

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 128>;

void test_scheduled_events_fire_on_advance() {
  test::TestEngine e;
  Events ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };

  e.schedule_at(0, 10, MidiMessage::note_on(0, 64, 90), sink);
  CHECK(ev.empty());  // not due yet
  e.advance_ticks(9, sink);
  CHECK(ev.empty());
  e.advance_ticks(1, sink);  // now == 10
  CHECK(ev.size() == 1);
  CHECK(ev[0].tick == 10 && ev[0].msg.d1 == 64);
}

void test_events_fire_with_stopped_transport() {
  // D29 / golden G1: stream time advances regardless of transport state.
  test::TestEngine e;
  Events ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };
  CHECK(!e.transport().playing());
  e.schedule_at(0, 5, MidiMessage::note_on(0, 62, 80), sink);
  e.advance_ticks(5, sink);
  CHECK(ev.size() == 1 && ev[0].tick == 5);
}

void test_transport_clock_emission() {
  test::TestEngine e;
  Events ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };

  Command mask;
  mask.op = Op::kSet;
  mask.param = Param::kClockOutMask;
  mask.a = 0b0001;  // port 0
  e.push_command(mask, sink);

  Command start;
  start.param = Param::kTransportStart;
  e.push_command(start, sink);
  // Expect: FA (Start), the immediate first F8, then the transport event.
  CHECK(ev.size() == 3);
  CHECK(ev[0].kind == OutEvent::Kind::kMidi && ev[0].msg.status == midi::kStart);
  CHECK(ev[1].kind == OutEvent::Kind::kMidi && ev[1].msg.status == midi::kClock);
  CHECK(ev[2].kind == OutEvent::Kind::kTransport);

  ev.clear();
  e.advance_ticks(80, sink);  // two clock periods (40 ticks each)
  int clocks = 0;
  for (const OutEvent& o : ev) {
    if (o.kind == OutEvent::Kind::kMidi && o.msg.status == midi::kClock) {
      ++clocks;
    }
  }
  CHECK(clocks == 2);

  ev.clear();
  Command stop;
  stop.param = Param::kTransportStop;
  e.push_command(stop, sink);
  CHECK(ev.size() == 2 && ev[0].msg.status == midi::kStop);
  ev.clear();
  e.advance_ticks(80, sink);
  CHECK(ev.empty());  // no clock while stopped
}

void test_transport_position_and_bounds() {
  Transport t;
  CHECK(t.set_bpm(12000));
  CHECK(!t.set_bpm(100));    // below 20.00 BPM
  CHECK(!t.set_bpm(99999));  // above 400.00 BPM
  t.start();
  for (int i = 0; i < static_cast<int>(kTicksPerBar + kTicksPerBeat + 5); ++i) {
    t.advance_one();
  }
  const Position p = t.position();  // bar 2, beat 2, tick 5
  CHECK(p.bar == 2 && p.beat == 2 && p.tick == 5);
  t.locate(0);
  CHECK(t.position().bar == 1 && t.position().beat == 1 && t.position().tick == 0);
  t.stop();
  CHECK(t.state() == TransportState::kStopped);
  t.resume();
  CHECK(t.playing());
}

// Phase 7 (node T0): the variable time-signature engine's byte-identity gate
// -- TimeSig{}'s default member initializers are the LITERAL two constants
// whose product already defined kTicksPerBar, so a default-constructed
// Transport computes the identical numeric value, bit-for-bit, until
// set_time_sig ever moves it.
void test_time_sig_default_is_byte_identical_to_legacy_constants() {
  constexpr TimeSig sig;
  CHECK(sig.beats_per_bar == kBeatsPerBar);
  CHECK(sig.ticks_per_beat == kTicksPerBeat);
  CHECK(sig.ticks_per_bar() == kTicksPerBar);

  Transport t;
  CHECK(t.time_sig().beats_per_bar == kBeatsPerBar);
  CHECK(t.time_sig().ticks_per_beat == kTicksPerBeat);
  CHECK(t.ticks_per_bar() == kTicksPerBar);
}

// Phase 7 (node T0): set_time_sig mirrors set_bpm's own validated-setter
// discipline exactly -- rejects out of [kMinBeatsPerBar, kMaxBeatsPerBar],
// state unchanged on reject; a genuine non-4/4 value changes ticks_per_bar()
// (and therefore position()'s own bar-length math) accordingly.
void test_set_time_sig_bounds_and_bar_length() {
  Transport t;
  CHECK(!t.set_time_sig(0));                    // below kMinBeatsPerBar
  CHECK(!t.set_time_sig(kMaxBeatsPerBar + 1));  // above kMaxBeatsPerBar
  CHECK(t.ticks_per_bar() == kTicksPerBar);     // both rejects: state unchanged
  CHECK(t.set_time_sig(kMinBeatsPerBar));       // boundary: valid
  CHECK(t.ticks_per_bar() == kMinBeatsPerBar * kTicksPerBeat);
  CHECK(t.set_time_sig(kMaxBeatsPerBar));  // boundary: valid
  CHECK(t.ticks_per_bar() == kMaxBeatsPerBar * kTicksPerBeat);

  // A genuine 3/4 bar: ticks_per_bar shrinks to 3 beats, and position()'s own
  // bar/beat derivation (Transport::position()) follows it, not the old 4/4
  // compile-time constant.
  CHECK(t.set_time_sig(3));
  CHECK(t.ticks_per_bar() == 3 * kTicksPerBeat);
  t.start();
  for (Tick i = 0; i < 3 * kTicksPerBeat + kTicksPerBeat + 5; ++i) {
    t.advance_one();
  }
  const Position p = t.position();  // bar 2 (3-beat bars), beat 2, tick 5
  CHECK(p.bar == 2 && p.beat == 2 && p.tick == 5);
}

// P0-2 determinism proof: kBeat fires exactly on the 24-PPQN pulses with a
// deterministic bar/beat/pulse sequence, ONLY while playing, and -- unlike
// the F8 clock byte -- regardless of the clock-out mask (default 0/disabled
// here, proving it is not gated on clock-out routing).
void test_beat_heartbeat_cadence() {
  test::TestEngine e;
  Events ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };

  Command start;
  start.param = Param::kTransportStart;
  e.push_command(start, sink);
  ev.clear();

  // Cover one full bar plus one extra beat: 96 pulses/bar (24 * 4 beats) + 24.
  e.advance_ticks(kTicksPerBar + kTicksPerBeat, sink);

  StaticVector<OutEvent, 128> beats;
  for (const OutEvent& o : ev) {
    CHECK(o.kind == OutEvent::Kind::kBeat);  // nothing else fires in a bare engine
    CHECK(beats.push_back(o));
  }
  CHECK(beats.size() == 96 + 24);

  // n counts the 40-tick pulses elapsed since transport start (1-based): the
  // VERY first pulse (bar 1 beat 1 pulse 0, tick 0) is consumed by Start's
  // own immediate F8 emission (cmd_transport, mirroring the existing MIDI
  // clock convention) and never re-fires through advance_ticks, so the
  // periodic sequence observed here begins at n=1 (bar 1 beat 1 pulse 1);
  // pulse 0 recurs at n=24, 48, ... marking the START of each later beat.
  for (std::size_t i = 0; i < beats.size(); ++i) {
    const std::size_t n = i + 1;
    const auto expected_pulse = static_cast<std::uint8_t>(n % 24);
    const auto expected_beat = static_cast<std::uint8_t>((n / 24) % kBeatsPerBar + 1);
    const auto expected_bar = static_cast<std::uint32_t>(n / (24 * kBeatsPerBar)) + 1;
    CHECK(beats[i].code == expected_bar);
    CHECK(beats[i].msg.status == expected_beat);
    CHECK(beats[i].msg.d1 == expected_pulse);
  }

  // Stopped: no kBeat at all, even across the same span of ticks.
  ev.clear();
  Command stop;
  stop.param = Param::kTransportStop;
  e.push_command(stop, sink);
  ev.clear();
  e.advance_ticks(kTicksPerBar + kTicksPerBeat, sink);
  for (const OutEvent& o : ev) {
    CHECK(o.kind != OutEvent::Kind::kBeat);
  }
}

void test_clock_on_two_ports_and_continue() {
  test::TestEngine e;
  Events ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };
  Command mask;
  mask.op = Op::kSet;
  mask.param = Param::kClockOutMask;
  mask.a = 0b0011;  // ports 0 and 1
  e.push_command(mask, sink);
  Command cont;
  cont.param = Param::kTransportContinue;
  e.push_command(cont, sink);
  // Continue: FB on both ports (no immediate F8 — that is Start-only).
  int fb = 0, f8 = 0;
  for (const OutEvent& o : ev) {
    if (o.kind != OutEvent::Kind::kMidi) {
      continue;
    }
    if (o.msg.status == midi::kContinue) {
      ++fb;
    }
    if (o.msg.status == midi::kClock) {
      ++f8;
    }
  }
  CHECK(fb == 2 && f8 == 0);
  ev.clear();
  e.advance_ticks(40, sink);
  f8 = 0;
  for (const OutEvent& o : ev) {
    if (o.kind == OutEvent::Kind::kMidi && o.msg.status == midi::kClock) {
      ++f8;
    }
  }
  CHECK(f8 == 2);  // tick 40 on both ports
}

void test_schedule_at_already_due() {
  test::TestEngine e;
  Events ev;
  auto sink = [&](const OutEvent& o) { CHECK(ev.push_back(o)); };
  e.advance_ticks(5, sink);
  e.schedule_at(0, 5, MidiMessage::note_on(0, 60, 1), sink);  // due now
  CHECK(ev.size() == 1 && ev[0].tick == 5);
}

}  // namespace

int main() {
  test_scheduled_events_fire_on_advance();
  test_events_fire_with_stopped_transport();
  test_transport_clock_emission();
  test_beat_heartbeat_cadence();
  test_transport_position_and_bounds();
  test_time_sig_default_is_byte_identical_to_legacy_constants();
  test_set_time_sig_bounds_and_bar_length();
  test_clock_on_two_ports_and_continue();
  test_schedule_at_already_due();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_engine (runtime): all OK\n");
  }
  return arrangrr::test::failures();
}
