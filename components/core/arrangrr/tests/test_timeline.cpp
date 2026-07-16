#include "arrangrr/timeline/timeline.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"
#include "test_harness.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 256>;

Command track_new(std::uint8_t port, std::uint8_t channel, TrackRole role = TrackRole::kLead) {
  Command c;
  c.param = Param::kTrackNew;
  c.a = static_cast<std::int32_t>(role);
  c.b = port | (channel << 8);
  return c;
}

Command track_step(std::uint16_t track, std::int32_t step, std::uint8_t note, std::uint8_t vel,
                   std::uint16_t gate) {
  Command c;
  c.param = Param::kTrackStep;
  c.idx = track;
  c.a = step;
  c.b = note | (vel << 8);
  c.c = gate;
  return c;
}

Command track_set(Param param, std::uint16_t track, std::int32_t value) {
  Command c;
  c.op = Op::kSet;
  c.param = param;
  c.idx = track;
  c.a = value;
  return c;
}

void test_timeline_model() {
  Timeline tl;
  CHECK(tl.add_track(TrackRole::kBass, 0, 2) == 0);
  CHECK(tl.add_track(TrackRole::kDrums, 1, 9) == 1);
  CHECK(tl.track_count() == 2);
  CHECK(tl.track(0)->channel == 2);
  CHECK(tl.track(5) == nullptr);
  CHECK(tl.set_step(0, 0, 36, 100, 120));
  CHECK(!tl.set_step(9, 0, 36, 100, 120));   // no such track
  CHECK(!tl.set_step(0, 99, 36, 100, 120));  // step out of range
  CHECK(!tl.set_step(0, 0, 200, 100, 120));  // note out of range
  CHECK(tl.set_length(0, 12));
  CHECK(!tl.set_length(0, 0));
  CHECK(!tl.set_length(0, 65));
  CHECK(!tl.any_solo());
  tl.track(1)->solo = true;
  CHECK(tl.any_solo());
}

void test_timeline_pool_bounded() {
  Timeline tl;
  for (std::size_t i = 0; i < kMaxTracks; ++i) {
    CHECK(tl.add_track(TrackRole::kLead, 0, 0) >= 0);
  }
  CHECK(tl.add_track(TrackRole::kLead, 0, 0) == -1);
}

struct Player {
  test::TestEngine e;
  Events ev;
  void sink(const OutEvent& o) { CHECK(ev.push_back(o)); }
  Engine::EventSink s = [](const OutEvent&) {};  // placeholder; use lambda below

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

void test_step_playback_with_gate() {
  Player p;
  p.cmd(track_new(0, 2));
  p.cmd(track_step(0, 0, 60, 100, 120));

  Command start;
  start.param = Param::kTransportStart;
  p.cmd(start);
  // Step 0 fires on start: NoteOn immediately, NoteOff gate ticks later.
  CHECK(p.count(midi::kNoteOn, 60) == 1);
  CHECK(p.count(midi::kNoteOff, 60) == 0);
  p.advance(120);
  CHECK(p.count(midi::kNoteOff, 60) == 1);
  // One full pattern (16 steps * 240 ticks): step 0 hits again at tick 3840.
  p.advance(3840 - 120);
  CHECK(p.count(midi::kNoteOn, 60) == 2);
}

void test_polymeter_wrap() {
  Player p;
  p.cmd(track_new(0, 0));  // track 0: length 4
  p.cmd(track_new(0, 1));  // track 1: length 3
  p.cmd(track_set(Param::kTrackLength, 0, 4));
  p.cmd(track_set(Param::kTrackLength, 1, 3));
  p.cmd(track_step(0, 0, 40, 100, 60));
  p.cmd(track_step(1, 0, 50, 100, 60));

  Command start;
  start.param = Param::kTransportStart;
  p.cmd(start);
  p.advance(12 * kTicksPerStep - 1);  // 12 global steps: lcm(4,3) = one cycle
  // Track 0 (len 4) fires at global steps 0,4,8 -> 3 times.
  CHECK(p.count(midi::kNoteOn, 40) == 3);
  // Track 1 (len 3) fires at 0,3,6,9 -> 4 times.
  CHECK(p.count(midi::kNoteOn, 50) == 4);
}

void test_mute_and_solo() {
  Player p;
  p.cmd(track_new(0, 0));
  p.cmd(track_new(0, 1));
  p.cmd(track_step(0, 0, 40, 100, 60));
  p.cmd(track_step(1, 0, 50, 100, 60));
  p.cmd(track_set(Param::kTrackMute, 0, 1));

  Command start;
  start.param = Param::kTransportStart;
  p.cmd(start);
  CHECK(p.count(midi::kNoteOn, 40) == 0);  // muted
  CHECK(p.count(midi::kNoteOn, 50) == 1);

  // Solo track 0 (still muted -> silent), track 1 not soloed -> silent too.
  p.cmd(track_set(Param::kTrackSolo, 0, 1));
  p.advance(16 * kTicksPerStep);
  CHECK(p.count(midi::kNoteOn, 40) == 0);
  CHECK(p.count(midi::kNoteOn, 50) == 1);
  // Unmute the soloed track: it plays, the other stays silenced by solo.
  p.cmd(track_set(Param::kTrackMute, 0, 0));
  p.advance(16 * kTicksPerStep);
  CHECK(p.count(midi::kNoteOn, 40) >= 1);
  CHECK(p.count(midi::kNoteOn, 50) == 1);
}

void test_track_command_warns() {
  Player p;
  p.cmd(track_new(9, 0));   // bad port
  p.cmd(track_new(0, 16));  // bad channel
  Command bad_role = track_new(0, 0);
  bad_role.a = 99;
  p.cmd(bad_role);
  p.cmd(track_step(7, 0, 60, 100, 120));        // no such track
  p.cmd(track_set(Param::kTrackLength, 7, 8));  // no such track
  p.cmd(track_set(Param::kTrackMute, 7, 1));    // no such track
  int warns = 0;
  for (const OutEvent& o : p.ev) {
    if (o.kind == OutEvent::Kind::kWarn) {
      ++warns;
    }
  }
  CHECK(warns == 6);
  // Fill the pool -> kTrackTableFull.
  for (std::size_t i = 0; i < kMaxTracks; ++i) {
    p.cmd(track_new(0, 0));
  }
  p.ev.clear();
  p.cmd(track_new(0, 0));
  CHECK(p.ev.size() == 1 && p.ev[0].code == static_cast<std::uint16_t>(WarnCode::kTrackTableFull));
}

void test_gate_zero_rejected_at_the_abi() {
  // gate 0 + the D29 off-before-on order = guaranteed stuck note; the core
  // must refuse it (the ABI is the product boundary, D26 — not the shell).
  Timeline tl;
  CHECK(tl.add_track(TrackRole::kLead, 0, 0) == 0);
  CHECK(!tl.set_step(0, 0, 60, 100, 0));
  CHECK(tl.set_step(0, 0, 60, 0, 0));  // clearing a slot stays legal
  Player p;
  p.cmd(track_new(0, 0));
  p.cmd(track_step(0, 0, 60, 100, 0));
  CHECK(p.ev.size() == 1 && p.ev[0].kind == OutEvent::Kind::kWarn);
}

void test_retrigger_not_truncated_by_stale_off() {
  // §9.B: step 1 = C4 with a gate LONGER than the step, step 2 = C4 again.
  // The re-fired note must get an off-before-on on its own tick, and the
  // stale off (which would truncate it) must be swallowed.
  Player p;
  p.cmd(track_new(0, 0));
  p.cmd(track_step(0, 0, 60, 100, 300));  // off would land at 300
  p.cmd(track_step(0, 1, 60, 100, 100));  // next hit at 240, off at 340
  Command start;
  start.param = Param::kTransportStart;
  p.cmd(start);
  p.advance(2 * kTicksPerStep);  // through tick 480
  StaticVector<std::uint32_t, 8> ons, offs;
  for (const OutEvent& o : p.ev) {
    if (o.kind != OutEvent::Kind::kMidi || o.msg.d1 != 60) {
      continue;
    }
    if (o.msg.type() == midi::kNoteOn) {
      CHECK(ons.push_back(o.tick));
    }
    if (o.msg.type() == midi::kNoteOff) {
      CHECK(offs.push_back(o.tick));
    }
  }
  CHECK(ons.size() == 2 && ons[0] == 0 && ons[1] == 240);
  CHECK(offs.size() == 2);
  CHECK(offs[0] == 240);  // forced release, sorted before the new on (D29)
  CHECK(offs[1] == 340);  // the new note's own gate — nothing at 300
}

void test_clear_step_silences() {
  Player p;
  p.cmd(track_new(0, 0));
  p.cmd(track_step(0, 0, 60, 100, 120));
  p.cmd(track_step(0, 0, 60, 0, 0));  // vel 0 clears the slot
  Command start;
  start.param = Param::kTransportStart;
  p.cmd(start);
  p.advance(16 * kTicksPerStep);
  CHECK(p.count(midi::kNoteOn, 60) == 0);
}

}  // namespace

int main() {
  test_timeline_model();
  test_timeline_pool_bounded();
  test_step_playback_with_gate();
  test_polymeter_wrap();
  test_mute_and_solo();
  test_track_command_warns();
  test_gate_zero_rejected_at_the_abi();
  test_retrigger_not_truncated_by_stale_off();
  test_clear_step_silences();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_timeline: all OK\n");
  }
  return arrangrr::test::failures();
}
