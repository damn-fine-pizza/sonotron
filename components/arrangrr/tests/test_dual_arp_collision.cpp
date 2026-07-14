// Torquato QA heavy-pass (Phase-6 Theme 4, Fork 6): the dual-arp output
// collision. A role's own kArp insert (fired from WITHIN fire_arranger's new
// per-tick P5 pass, engine.hpp on_tick) and the Engine-global live-keyboard
// arp (m_arp, fired by fire_arp AFTER fire_arranger, engine.hpp:317-318) can
// be routed to the SAME output port+channel and emit the SAME pitch on the
// SAME transport tick. Both paths schedule through the ONE shared
// Engine::schedule_pattern retrigger-care choke point (engine.hpp), which was
// designed to protect a SINGLE producer's own same-note re-fire (the arp's
// own retrigger, test_arp_engine_retrigger_no_stuck_notes in
// test_arp_functional.cpp) from ever leaving a stuck note. This file asks:
// does that SAME mechanism silently misfire when the "retrigger" is actually
// TWO INDEPENDENT producers colliding on the same (port, channel, note) at
// the same tick, not one producer re-firing itself?

#include "arrangrr/arp/arpeggiator.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"
#include "test_harness.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 1024>;

struct Band {
  test::TestEngine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0,
           std::uint16_t idx = 0, Op op = Op::kSet) {
    Command command{.op = op, .param = p, .idx = idx, .a = a, .b = b, .c = c};
    e.push_command(command, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void advance(std::uint32_t n) {
    e.advance_ticks(n, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void feed_note_on(std::uint8_t port, std::uint8_t note, std::uint8_t vel) {
    const std::uint8_t bytes[3] = {0x90, note, vel};
    e.push_midi_in(port, Span<const std::uint8_t>(bytes, 3),
                   [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
};

// Every kMidi event on `port`/`channel` carrying MIDI note `note_num`, in
// scheduled-tick/emission order, as (tick, is_on) pairs.
StaticVector<std::pair<Tick, bool>, 32> events_for(const Events& ev, std::uint8_t port,
                                                   std::uint8_t channel, std::uint8_t note_num) {
  StaticVector<std::pair<Tick, bool>, 32> out;
  for (const OutEvent& o : ev) {
    if (o.kind != OutEvent::Kind::kMidi || o.port != port || o.msg.channel() != channel ||
        o.msg.d1 != note_num) {
      continue;
    }
    if (o.msg.type() == midi::kNoteOn && o.msg.d2 > 0) {
      CHECK(out.push_back({o.tick, true}));
    } else if (o.msg.type() == midi::kNoteOff || (o.msg.type() == midi::kNoteOn && o.msg.d2 == 0)) {
      CHECK(out.push_back({o.tick, false}));
    }
  }
  return out;
}

// Fork 6 (headline collision): a role's kArp insert and the live-keyboard arp
// share the SAME output port+channel and the SAME pitch, both firing on the
// SAME transport tick (240, the first tick where their two independent
// 16th-note grids align -- see the test body for why tick 0 itself is
// asymmetric). Configured with a FULL gate (100%) on both, so each arp's own
// note-off is scheduled to land exactly on the NEXT retrigger tick -- exactly
// the shape Engine::schedule_pattern's cancel_note_off retrigger-care logic
// (out_scheduler.hpp) was written to protect a SINGLE re-firing producer
// from. Here there are TWO independent producers.
void test_dual_arp_same_output_and_pitch_collision_at_shared_tick() {
  Band b;
  b.cmd(Param::kStyleLoad, 0, 0, 0, 0, Op::kDo);  // "basic": kVarABass step0 tone0 -> note 36
  b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 0 | (0 << 8), 0, 0,
        Op::kSet);

  // Bass's OWN chain: slot 0 = kArp, full gate, 16th-note rate (240 ticks/step)
  // -- same rate as the live arp below, so both grids share every multiple
  // of 240 ticks (see the kTransportStart comment below for why tick 0 itself
  // does not actually exercise both producers).
  const auto bass = static_cast<std::uint16_t>(TrackRole::kBass);
  b.cmd(Param::kFxSet, 0, static_cast<std::int32_t>(InsertType::kArp), 0, bass);
  b.cmd(Param::kFxParam, 0, /*rate=*/0, static_cast<std::int32_t>(ArpRate::kSixteenth), bass);
  b.cmd(Param::kFxParam, 0, /*gate=*/3, 100, bass);  // full gate: off lands at +240

  // Live-keyboard arp: SAME output port(0)/channel(0) as Bass's own route,
  // SAME rate, SAME full gate.
  b.cmd(Param::kArpOut, 0 | (0 << 8), 0);
  b.cmd(Param::kArp, static_cast<std::int32_t>(ArpField::kRate),
        static_cast<std::int32_t>(ArpRate::kSixteenth));
  b.cmd(Param::kArp, static_cast<std::int32_t>(ArpField::kGate), 100);
  b.cmd(Param::kArp, static_cast<std::int32_t>(ArpField::kEnabled), 1);

  // kTransportStart fires tick 0's fire_arranger SYNCHRONOUSLY, inline in
  // cmd_transport (engine.cpp) -- but NOT fire_arp (that only runs from the
  // Engine::on_tick StageLike pass advance_ticks below drives). Tick 0 is
  // therefore asymmetric: only the role-arp ingests+fires there (holding
  // note 36 from Bass's own resolved chord tone). Feed the live arp its note
  // right after start, then advance to tick 240 -- the FIRST tick where the
  // natural per-tick Engine::on_tick loop (engine.hpp:317-318) calls
  // fire_arranger THEN fire_arp in the SAME pass, so this is where the two
  // arps' independent 16th-note (240-tick) grids first align and BOTH fire
  // for real, sharing this file's collision scenario.
  b.cmd(Param::kTransportStart, 0, 0, 0, 0, Op::kDo);
  // The live arp's own input port defaults to 0 (never changed above); feed
  // it the SAME pitch (36) the resolved Bass chord-tone will produce over the
  // home-key default chord (root_pc 0, anchor 36 + 0 == 36), so both arps
  // hold/emit the identical (port, channel, note) triple.
  b.feed_note_on(0, 36, 100);

  b.advance(240);  // reach tick 240: both 16th-note grids fire together here

  const auto trace = events_for(b.ev, /*port=*/0, /*channel=*/0, /*note=*/36);

  // If the two arps' independent emissions were kept properly separate, tick
  // 240 (the first tick both 240-tick grids share) would carry exactly TWO
  // note-ons (one per arp) and NO note-off would land at tick 240 at all --
  // every legitimate note-off (both arps' own, and the role-arp's tick-0
  // note) belongs at tick 480 (240 + step_ticks(240)*gate(100)/100). Assert
  // that shape; a failure here means Engine::schedule_pattern's shared
  // retrigger-care logic (out_scheduler.hpp's cancel_note_off) is
  // tombstoning ONE arp's legitimate note-off and injecting a same-tick
  // compensating note-off meant for a single re-firing producer, not two
  // independent ones -- see this file's header comment.
  int ons_at_240 = 0;
  int offs_at_240 = 0;
  for (const auto& [tick, is_on] : trace) {
    if (tick == 240) {
      if (is_on) {
        ++ons_at_240;
      } else {
        ++offs_at_240;
      }
    }
  }
  CHECK(ons_at_240 == 2);   // both arps' own note-on for the shared pitch
  CHECK(offs_at_240 == 0);  // neither arp's note-off should be due yet
}

// GREEN characterization (target 1's second half): even mid-collision,
// Panic (Engine::cmd_routing's kPanic case) drops BOTH the live-keyboard
// arp's held set AND every role's own arp-insert held chord -- confirmed
// behaviorally by driving the SAME collision setup as the test above, then
// panicking and proving neither producer emits another note-on afterward.
void test_panic_clears_both_the_live_arp_and_every_role_arp_insert() {
  Band b;
  b.cmd(Param::kStyleLoad, 0, 0, 0, 0, Op::kDo);
  b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 0 | (0 << 8), 0, 0,
        Op::kSet);
  const auto bass = static_cast<std::uint16_t>(TrackRole::kBass);
  b.cmd(Param::kFxSet, 0, static_cast<std::int32_t>(InsertType::kArp), 0, bass);
  b.cmd(Param::kFxParam, 0, /*rate=*/0, static_cast<std::int32_t>(ArpRate::kSixteenth), bass);
  b.cmd(Param::kFxParam, 0, /*gate=*/3, 100, bass);
  b.cmd(Param::kArpOut, 0 | (0 << 8), 0);
  b.cmd(Param::kArp, static_cast<std::int32_t>(ArpField::kRate),
        static_cast<std::int32_t>(ArpRate::kSixteenth));
  b.cmd(Param::kArp, static_cast<std::int32_t>(ArpField::kGate), 100);
  b.cmd(Param::kArp, static_cast<std::int32_t>(ArpField::kEnabled), 1);
  b.cmd(Param::kTransportStart, 0, 0, 0, 0, Op::kDo);
  b.feed_note_on(0, 36, 100);
  b.advance(240);  // both arps have now fired at least once (this file's own collision point)

  CHECK(b.e.arp().active());  // the live arp is genuinely holding a note pre-panic

  b.ev.clear();
  b.cmd(Param::kPanic, 0, 0, 0, 0, Op::kDo);
  CHECK(!b.e.arp().active());  // Engine::cmd_routing's kPanic: m_arp.panic()

  b.ev.clear();
  b.advance(480);  // several more arp-rate grid boundaries, both role and live

  int ons_after_panic = 0;
  for (const OutEvent& o : b.ev) {
    if (o.kind == OutEvent::Kind::kMidi && o.msg.type() == midi::kNoteOn && o.msg.d2 > 0) {
      ++ons_after_panic;
    }
  }
  CHECK(ons_after_panic == 0);  // neither producer ever sounds again post-panic
}

}  // namespace

int main() {
  test_dual_arp_same_output_and_pitch_collision_at_shared_tick();
  test_panic_clears_both_the_live_arp_and_every_role_arp_insert();
  return arrangrr::test::failures();
}
