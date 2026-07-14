// Functional tests for the MIDI-FX insert chain's Engine-ABI wiring
// (Phase-5 Item #10, node 5100/5200): cmd_fx (kFxSet/kFxParam/kFxEnable/
// kFxClear) dispatch and validation, and the Corelli must-fix end-to-end --
// a chain-produced fan-out note's groove/schedule is computed at ITS OWN
// grid position, so an Echo replica lands on its own beat, not the seed's.
// Only observable through Engine's cross-producer wiring (Arranger's D40
// pipeline + the scheduler) -> functional, same precedent as test_pad.cpp.
//
// Arranger exposes NO read accessor into its per-role InsertChain state (by
// design -- set_fx/set_fx_param/set_fx_enable/clear_fx are write-only, same
// as set_route/set_groove_field). Every assertion here is therefore
// BEHAVIORAL: a kFxSet+kFxParam configuration is proven to have taken effect
// by observing its effect on the scheduled MIDI output, never by reading
// internal state back out.

#include "arrangrr/fx/insert_chain.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"
#include "test_harness.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 512>;

struct Band {
  test::TestEngine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0,
           std::uint16_t idx = 0) {
    Command command;
    command.op = Op::kDo;
    command.param = p;
    command.idx = idx;
    command.a = a;
    command.b = b;
    command.c = c;
    e.push_command(command, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void advance(std::uint32_t n) {
    e.advance_ticks(n, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  int warns() const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kWarn) {
        ++n;
      }
    }
    return n;
  }
  // Every kMidi NoteOn on `port`/`channel` carrying MIDI note `note_num`,
  // in the order fired, as (tick, velocity) pairs.
  void note_ons(std::uint8_t port, std::uint8_t channel, std::uint8_t note_num,
                StaticVector<std::pair<Tick, std::uint8_t>, 16>& out) const {
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kMidi && o.port == port && o.msg.type() == midi::kNoteOn &&
          o.msg.channel() == channel && o.msg.d1 == note_num) {
        CHECK(out.push_back({o.tick, o.msg.d2}));
      }
    }
  }
  void configure_drum_echo(std::uint8_t slot, std::uint8_t repeats, std::uint8_t vel_decay,
                           std::uint16_t delay_ticks) {
    cmd(Param::kFxSet, slot, static_cast<std::int32_t>(InsertType::kEcho), 0,
        static_cast<std::uint16_t>(TrackRole::kDrums));
    cmd(Param::kFxParam, slot, /*repeats=*/0, repeats,
        static_cast<std::uint16_t>(TrackRole::kDrums));
    cmd(Param::kFxParam, slot, /*vel_decay=*/1, vel_decay,
        static_cast<std::uint16_t>(TrackRole::kDrums));
    cmd(Param::kFxParam, slot, /*delay_ticks=*/2, delay_ticks,
        static_cast<std::uint16_t>(TrackRole::kDrums));
  }
};

// ---- cmd_fx bad-argument validation ----------------------------------------

void test_fx_set_rejects_out_of_range_role() {
  Band b;
  b.cmd(Param::kFxSet, 0, static_cast<std::int32_t>(InsertType::kEcho), 0,
        /*idx=*/static_cast<std::uint16_t>(TrackRole::kCc) + 1);
  CHECK(b.warns() == 1);
}

void test_fx_set_rejects_out_of_range_slot() {
  Band b;
  b.cmd(Param::kFxSet, /*a=slot*/ kMaxInserts, static_cast<std::int32_t>(InsertType::kEcho), 0,
        static_cast<std::uint16_t>(TrackRole::kBass));
  CHECK(b.warns() == 1);
}

void test_fx_set_rejects_out_of_range_type() {
  Band b;
  b.cmd(Param::kFxSet, 0, /*b=type*/ static_cast<std::int32_t>(InsertType::kNoteRepeat) + 1, 0,
        static_cast<std::uint16_t>(TrackRole::kBass));
  CHECK(b.warns() == 1);
}

void test_fx_param_rejects_unknown_param_id_for_current_type() {
  Band b;
  b.cmd(Param::kFxSet, 0, static_cast<std::int32_t>(InsertType::kScaleLock), 0,
        static_cast<std::uint16_t>(TrackRole::kBass));  // only param_id 0 (strength) exists
  b.ev.clear();
  b.cmd(Param::kFxParam, 0, /*param_id*/ 5, 1, static_cast<std::uint16_t>(TrackRole::kBass));
  CHECK(b.warns() == 1);
}

void test_fx_param_rejects_out_of_range_role() {
  Band b;
  b.cmd(Param::kFxParam, 0, 0, 1, static_cast<std::uint16_t>(TrackRole::kCc) + 1);
  CHECK(b.warns() == 1);
}

void test_fx_enable_rejects_out_of_range_slot() {
  Band b;
  b.cmd(Param::kFxEnable, kMaxInserts, 0, 0, static_cast<std::uint16_t>(TrackRole::kBass));
  CHECK(b.warns() == 1);
}

void test_fx_enable_rejects_out_of_range_role() {
  Band b;
  b.cmd(Param::kFxEnable, 0, 0, 0, static_cast<std::uint16_t>(TrackRole::kCc) + 1);
  CHECK(b.warns() == 1);
}

void test_fx_clear_rejects_out_of_range_role() {
  Band b;
  b.cmd(Param::kFxClear, 0, 0, 0, static_cast<std::uint16_t>(TrackRole::kCc) + 1);
  CHECK(b.warns() == 1);
}

// ---- Behavioral proof that valid kFxSet/kFxParam/kFxEnable/kFxClear calls
//      actually reconfigure the role's chain -----------------------------

// "basic" style's kDrums part is RolePolicy::kFixed (no chord/key dependency,
// unlike kBass/kChord1's kChordTone patterns) and fires a kick (note 36) at
// step 0 -- the cleanest single, deterministic event to trace a chain
// reconfiguration against.
void test_fx_set_and_param_valid_configuration_takes_effect() {
  Band b;
  b.cmd(Param::kStyleLoad, 0);  // "basic"
  b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kDrums), 0 | (9 << 8));
  b.configure_drum_echo(0, /*repeats=*/2, /*vel_decay=*/128, /*delay_ticks=*/100);

  b.cmd(Param::kTransportStart);
  b.advance(250);

  StaticVector<std::pair<Tick, std::uint8_t>, 16> kicks;
  b.note_ons(0, 9, 36, kicks);
  CHECK(kicks.size() == 3);  // seed + 2 repeats: kFxSet+kFxParam took effect
}

void test_fx_enable_false_disables_the_configured_insert() {
  Band b;
  b.cmd(Param::kStyleLoad, 0);
  b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kDrums), 0 | (9 << 8));
  b.configure_drum_echo(0, 2, 128, 100);
  b.cmd(Param::kFxEnable, /*slot=*/0, /*enabled=*/0, 0,
        static_cast<std::uint16_t>(TrackRole::kDrums));

  b.cmd(Param::kTransportStart);
  b.advance(250);

  StaticVector<std::pair<Tick, std::uint8_t>, 16> kicks;
  b.note_ons(0, 9, 36, kicks);
  CHECK(kicks.size() == 1);  // only the seed: the disabled slot never ran
  CHECK(kicks[0].first == 0 && kicks[0].second == 110);
}

void test_fx_clear_one_slot_resets_it_to_inert() {
  Band b;
  b.cmd(Param::kStyleLoad, 0);
  b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kDrums), 0 | (9 << 8));
  b.configure_drum_echo(0, 2, 128, 100);
  b.cmd(Param::kFxClear, /*a=slot*/ 0, 0, 0, static_cast<std::uint16_t>(TrackRole::kDrums));

  b.cmd(Param::kTransportStart);
  b.advance(250);

  StaticVector<std::pair<Tick, std::uint8_t>, 16> kicks;
  b.note_ons(0, 9, 36, kicks);
  CHECK(kicks.size() == 1);  // slot 0 back to the inert ScaleLock(strength=0) default
}

void test_fx_clear_whole_role_with_a_negative_one() {
  Band b;
  b.cmd(Param::kStyleLoad, 0);
  b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kDrums), 0 | (9 << 8));
  b.configure_drum_echo(0, 2, 128, 100);
  b.cmd(Param::kFxClear, /*a=*/-1, 0, 0, static_cast<std::uint16_t>(TrackRole::kDrums));

  b.cmd(Param::kTransportStart);
  b.advance(250);

  StaticVector<std::pair<Tick, std::uint8_t>, 16> kicks;
  b.note_ons(0, 9, 36, kicks);
  CHECK(kicks.size() == 1);  // the whole chain reset, not just slot 0
}

// ---- Corelli property: fan-out replicas schedule at their OWN position ----

void test_fx_echo_replicas_schedule_at_own_beat_not_the_seeds() {
  Band b;
  b.cmd(Param::kStyleLoad, 0);
  b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kDrums), 0 | (9 << 8));
  b.configure_drum_echo(0, /*repeats=*/2, /*vel_decay=*/128, /*delay_ticks=*/100);

  b.cmd(Param::kTransportStart);  // fires step 0 immediately (transport_tick == 0)
  b.advance(250);                 // far enough to cross both echo replicas' ticks

  StaticVector<std::pair<Tick, std::uint8_t>, 16> kicks;
  b.note_ons(/*port=*/0, /*channel=*/9, /*note=*/36, kicks);
  CHECK(kicks.size() == 3);  // seed + 2 repeats
  // Seed lands at tick 0, byte-identical velocity (110, kVarADrums' own kick
  // velocity). Repeat 1 lands at tick 100 (== seed_tick + 1*delay_ticks), NOT
  // tick 0 -- the Corelli property. vel1 = 110*128/255 = 55 (integer div).
  // Repeat 2 lands at tick 200 (== seed_tick + 2*delay_ticks); vel2 =
  // 55*128/255 = 27.
  CHECK(kicks[0].first == 0 && kicks[0].second == 110);
  CHECK(kicks[1].first == 100 && kicks[1].second == 55);
  CHECK(kicks[2].first == 200 && kicks[2].second == 27);
}

// The empty/default chain is a provable no-op: byte-identical schedule to
// the pre-Item-#10 path (a second, Engine-level confirmation of the same
// invariant test_insert_chain.cpp's own
// test_chain_default_construction_is_exact_passthrough pins at the pure
// InsertChain level).
void test_fx_default_chain_does_not_alter_the_existing_schedule() {
  Band b;
  b.cmd(Param::kStyleLoad, 0);
  b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kDrums), 0 | (9 << 8));
  b.cmd(Param::kTransportStart);
  b.advance(20);

  StaticVector<std::pair<Tick, std::uint8_t>, 16> kicks;
  b.note_ons(0, 9, 36, kicks);
  CHECK(kicks.size() == 1);
  CHECK(kicks[0].first == 0 && kicks[0].second == 110);
}

}  // namespace

int main() {
  test_fx_set_rejects_out_of_range_role();
  test_fx_set_rejects_out_of_range_slot();
  test_fx_set_rejects_out_of_range_type();
  test_fx_param_rejects_unknown_param_id_for_current_type();
  test_fx_param_rejects_out_of_range_role();
  test_fx_enable_rejects_out_of_range_slot();
  test_fx_enable_rejects_out_of_range_role();
  test_fx_clear_rejects_out_of_range_role();

  test_fx_set_and_param_valid_configuration_takes_effect();
  test_fx_enable_false_disables_the_configured_insert();
  test_fx_clear_one_slot_resets_it_to_inert();
  test_fx_clear_whole_role_with_a_negative_one();

  test_fx_echo_replicas_schedule_at_own_beat_not_the_seeds();
  test_fx_default_chain_does_not_alter_the_existing_schedule();
  return arrangrr::test::failures();
}
