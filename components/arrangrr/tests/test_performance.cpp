// Functional tests for the Performance capture/recall primitive (Phase-5
// Item #9, docs/phase5-design-reviews.md "Pad/Scene live -> Performance"):
// Engine's cmd_perf/capture_performance/validate_performance/apply_performance
// cross-producer wiring (Arranger/Transport/ChordEngine/ChordSequencer) --
// exactly like test_clip.cpp -> functional. serialize()/deserialize() wire
// fidelity is pure and lives separately in test_performance_wire.cpp.

#include "arrangrr/perf/performance.hpp"

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
           std::uint16_t idx = 0, Boundary boundary = Boundary::kImmediate,
           std::uint8_t n_bars = 1) {
    Command command;
    command.op = Op::kDo;
    command.boundary = boundary;
    command.param = p;
    command.idx = idx;
    command.n_bars = n_bars;
    command.a = a;
    command.b = b;
    command.c = c;
    e.push_command(command, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void advance(std::uint32_t n) {
    e.advance_ticks(n, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void setup_basic() {
    e.push_command(Command{.op = Op::kSet, .param = Param::kKeySet, .a = 0, .b = 0},
                   [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
    cmd(Param::kStyleLoad, 0);
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kDrums), 0 | (9 << 8));
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 0 | (1 << 8));
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kChord1), 0 | (2 << 8));
  }
  void add_clip(TrackRole role, std::uint8_t scene, ContentKind kind, std::uint16_t content_index) {
    cmd(Param::kClipAdd, static_cast<std::int32_t>(role), scene,
        static_cast<std::int32_t>(kind) | (static_cast<std::int32_t>(content_index) << 8));
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
};

bool same_groove(const GrooveParams& a, const GrooveParams& b) {
  return a.swing == b.swing && a.humanize_timing == b.humanize_timing &&
         a.humanize_velocity == b.humanize_velocity && a.accent == b.accent &&
         a.swing_grid == b.swing_grid && a.quantize == b.quantize && a.seed == b.seed;
}

// A baseline Performance with every field inside validate_performance's
// accepted range -- each atomicity test corrupts exactly ONE field away from
// this, so a rejection pins down the specific validation branch.
Performance valid_performance() {
  Performance p;
  p.style_id = 0;  // "basic": a real builtin index
  p.tempo_x100 = 12000;
  p.pad_bank_id = 0;
  p.chord_sequence_id = 0xFFFF;  // none
  p.controller_map_id = 0xFFFF;  // none
  p.variation = static_cast<std::uint8_t>(SectionType::kVarA);
  p.chord_mode = 0;
  p.chord_follow = 0;
  p.key_root = 0;
  p.key_mode = 0;
  return p;
}

// ---- item 7: recall atomicity (validate-all-then-apply-nothing) -----------

void test_perf_recall_unsaved_slot_applies_nothing() {
  Band b;
  b.setup_basic();
  const std::uint16_t style_before = b.e.arranger().style_id();
  const SectionType section_before = b.e.arranger().current();
  b.ev.clear();
  b.cmd(Param::kPerformanceRecall, 0, 0, 0, /*idx=*/5);  // never stored
  CHECK(b.warns() == 1);
  CHECK(b.e.arranger().style_id() == style_before);
  CHECK(b.e.arranger().current() == section_before);
}

// Injects `bad` directly into the store (bypassing perf_store/capture, which
// can never itself produce an out-of-range field) and asserts the recall is
// rejected with exactly one kBadArgument warn and the live rig is BYTE-FOR-
// FIELD unchanged -- Corelli's "validate every referenced id FIRST; apply
// NOTHING on any failure" property, tested hard, one branch at a time.
void assert_recall_rejected_and_rig_unchanged(Band& b, std::uint16_t slot, const Performance& bad) {
  CHECK(b.e.performances().store(slot, bad));
  const std::uint16_t style_before = b.e.arranger().style_id();
  const SectionType section_before = b.e.arranger().current();
  const auto bpm_before = b.e.transport().bpm();
  const bool bass_muted_before = b.e.arranger().muted(TrackRole::kBass);
  const bool drums_soloed_before = b.e.arranger().soloed(TrackRole::kDrums);
  const GrooveParams groove_before = b.e.arranger().groove_params();
  const Key key_before = b.e.chords().key();
  const std::uint16_t pad_bank_before = b.e.pad_bank();
  b.ev.clear();
  b.cmd(Param::kPerformanceRecall, 0, 0, 0, slot);
  CHECK(b.warns() == 1);
  CHECK(b.e.arranger().style_id() == style_before);
  CHECK(b.e.arranger().current() == section_before);
  CHECK(b.e.transport().bpm() == bpm_before);
  CHECK(b.e.arranger().muted(TrackRole::kBass) == bass_muted_before);
  CHECK(b.e.arranger().soloed(TrackRole::kDrums) == drums_soloed_before);
  CHECK(same_groove(b.e.arranger().groove_params(), groove_before));
  CHECK(b.e.chords().key().root_pc == key_before.root_pc);
  CHECK(b.e.chords().key().mode == key_before.mode);
  CHECK(b.e.pad_bank() == pad_bank_before);  // Phase-6 Theme 3 Item #4
}

void test_perf_recall_rejects_bad_style_id() {
  Band b;
  b.setup_basic();
  Performance bad = valid_performance();
  bad.style_id = 200;  // >= styles::kBuiltinCount
  assert_recall_rejected_and_rig_unchanged(b, 0, bad);
}

void test_perf_recall_rejects_bad_variation() {
  Band b;
  b.setup_basic();
  Performance bad = valid_performance();
  bad.variation = kSectionTypeCount;  // one past the last valid SectionType
  assert_recall_rejected_and_rig_unchanged(b, 0, bad);
}

void test_perf_recall_rejects_bad_chord_sequence_id() {
  Band b;
  b.setup_basic();  // no sequences registered: count() == 0
  Performance bad = valid_performance();
  bad.chord_sequence_id = 0;  // not the 0xFFFF sentinel, but no sequence 0 exists
  assert_recall_rejected_and_rig_unchanged(b, 0, bad);
}

void test_perf_recall_rejects_mute_mask_bits_beyond_role_range() {
  Band b;
  b.setup_basic();
  Performance bad = valid_performance();
  bad.track_mute_mask = 1u << 31;  // way beyond the 10-role mask
  assert_recall_rejected_and_rig_unchanged(b, 0, bad);
}

void test_perf_recall_rejects_solo_mask_bits_beyond_role_range() {
  Band b;
  b.setup_basic();
  Performance bad = valid_performance();
  bad.track_solo_mask = 1u << 31;
  assert_recall_rejected_and_rig_unchanged(b, 0, bad);
}

void test_perf_recall_rejects_bad_key_root() {
  Band b;
  b.setup_basic();
  Performance bad = valid_performance();
  bad.key_root = 12;  // valid range is 0..11
  assert_recall_rejected_and_rig_unchanged(b, 0, bad);
}

void test_perf_recall_rejects_bad_key_mode() {
  Band b;
  b.setup_basic();
  Performance bad = valid_performance();
  bad.key_mode = kModeCount;  // one past the last valid Mode
  assert_recall_rejected_and_rig_unchanged(b, 0, bad);
}

void test_perf_recall_rejects_bad_chord_mode() {
  Band b;
  b.setup_basic();
  Performance bad = valid_performance();
  bad.chord_mode = kChordModeCount;  // one past the last valid ChordMode
  assert_recall_rejected_and_rig_unchanged(b, 0, bad);
}

void test_perf_recall_rejects_bad_chord_follow() {
  Band b;
  b.setup_basic();
  Performance bad = valid_performance();
  bad.chord_follow = static_cast<std::uint8_t>(ChordFollow::kLivePriority) + 1;
  assert_recall_rejected_and_rig_unchanged(b, 0, bad);
}

void test_perf_recall_rejects_bad_route_port() {
  Band b;
  b.setup_basic();
  Performance bad = valid_performance();
  bad.routes[0].port = kMaxPorts;  // one past the last valid port
  assert_recall_rejected_and_rig_unchanged(b, 0, bad);
}

void test_perf_recall_rejects_bad_route_channel() {
  Band b;
  b.setup_basic();
  Performance bad = valid_performance();
  bad.routes[0].channel = 16;  // valid range is 0..15
  assert_recall_rejected_and_rig_unchanged(b, 0, bad);
}

// Phase-6 Theme 3 Item #4: a malformed record's pad_bank_id must not poison
// state -- rejected by validate(), never applied (same discipline as every
// other field above).
void test_perf_recall_rejects_bad_pad_bank_id() {
  Band b;
  b.setup_basic();
  Performance bad = valid_performance();
  bad.pad_bank_id = static_cast<std::uint16_t>(kMaxPadBanks);  // one past the last valid bank
  assert_recall_rejected_and_rig_unchanged(b, 0, bad);
}

// ---- item 6: capture -> recall round trip ----------------------------------

void test_perf_capture_recall_round_trip_restores_everything() {
  Band b;
  b.setup_basic();  // style 0 "basic"; drums/bass/chord1 routed+enabled; chord2 left UNROUTED
  CHECK(!b.e.arranger().part_info(TrackRole::kChord2).routed);  // disabled by construction

  // A distinctive live rig.
  b.cmd(Param::kStyleSection,
        static_cast<std::int32_t>(SectionType::kVarB));  // transport
                                                         // stopped -> immediate
  b.cmd(Param::kPartMute, static_cast<std::int32_t>(TrackRole::kBass), 1);
  b.cmd(Param::kPartSolo, static_cast<std::int32_t>(TrackRole::kDrums), 1);
  b.cmd(Param::kGroove, static_cast<std::int32_t>(GrooveField::kSwing), 40);
  b.cmd(Param::kGroove, static_cast<std::int32_t>(GrooveField::kHumanizeTiming), 10);
  b.cmd(Param::kGroove, static_cast<std::int32_t>(GrooveField::kHumanizeVelocity), 5);
  b.cmd(Param::kGroove, static_cast<std::int32_t>(GrooveField::kAccent), 20);
  b.cmd(Param::kGroove, static_cast<std::int32_t>(GrooveField::kSwingGrid), 16);
  b.cmd(Param::kGroove, static_cast<std::int32_t>(GrooveField::kQuantize), 30);
  b.cmd(Param::kGroove, static_cast<std::int32_t>(GrooveField::kSeed), 777);
  b.cmd(Param::kKeySet, 4, static_cast<std::int32_t>(Mode::kDorian));
  b.cmd(Param::kChordMode, static_cast<std::int32_t>(ChordMode::kShell));
  b.cmd(Param::kChordFollow, static_cast<std::int32_t>(ChordFollow::kManual));
  b.cmd(Param::kTransportTempo, 13350);
  // Phase-6 Theme 3 Item #1: a NEGATIVE transpose, to exercise the signed
  // round-trip (docs/reflections/phase6-theme3-master-transpose-scope.md
  // Decision 4a; now a real std::int16_t field, Item #3's P3) end to end, not
  // just a positive value.
  b.cmd(Param::kMasterTranspose, -7);
  CHECK(b.e.arranger().master_transpose() == -7);
  CHECK(b.e.chords().master_transpose() == -7);

  b.cmd(Param::kPerformanceStore, 0, 0, 0, /*idx=*/0);
  CHECK(b.warns() == 0);

  // Mutate the live rig away from the snapshot in every dimension, INCLUDING
  // enabling the previously-disabled route (Corelli fix #3's own concern:
  // set_route always enables, so the recall must EXPLICITLY restore the
  // disabled bit via set_route_enabled, not just leave it alone).
  b.cmd(Param::kStyleSection, static_cast<std::int32_t>(SectionType::kVarC));
  b.cmd(Param::kPartMute, static_cast<std::int32_t>(TrackRole::kBass), 0);
  b.cmd(Param::kPartSolo, static_cast<std::int32_t>(TrackRole::kDrums), 0);
  b.cmd(Param::kGroove, static_cast<std::int32_t>(GrooveField::kSwing), 0);
  b.cmd(Param::kGroove, static_cast<std::int32_t>(GrooveField::kQuantize), 0);
  b.cmd(Param::kGroove, static_cast<std::int32_t>(GrooveField::kSeed), 1);
  b.cmd(Param::kKeySet, 0, static_cast<std::int32_t>(Mode::kMajor));
  b.cmd(Param::kChordMode, static_cast<std::int32_t>(ChordMode::kDiatonic));
  b.cmd(Param::kChordFollow, static_cast<std::int32_t>(ChordFollow::kAuto));
  b.cmd(Param::kTransportTempo, 9000);
  b.cmd(Param::kMasterTranspose, 5);  // drifted to a different (positive) value
  b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kChord2), 0 | (5 << 8));
  CHECK(b.e.arranger().part_info(TrackRole::kChord2).routed);  // drifted: now enabled

  b.ev.clear();
  b.cmd(Param::kPerformanceRecall, 0, 0, 0, /*idx=*/0);
  CHECK(b.warns() == 0);

  // style_id() now round-trips too (Nazzareno's fix, see the header comment
  // of Engine::apply_performance and the dedicated regression lock in
  // test_performance_style_id_regression.cpp -- was RED, now green).
  CHECK(b.e.arranger().style_id() == 0);
  CHECK(b.e.arranger().current() == SectionType::kVarB);
  CHECK(b.e.arranger().muted(TrackRole::kBass));
  CHECK(b.e.arranger().soloed(TrackRole::kDrums));
  CHECK(!b.e.arranger().part_info(TrackRole::kChord2).routed);  // restored to DISABLED
  const GrooveParams& g = b.e.arranger().groove_params();
  CHECK(g.swing == 40 && g.humanize_timing == 10 && g.humanize_velocity == 5 && g.accent == 20 &&
        g.swing_grid == 16 && g.quantize == 30 && g.seed == 777);
  CHECK(b.e.transport().bpm() == 13350);
  CHECK(b.e.chords().key().root_pc == 4);
  CHECK(b.e.chords().key().mode == Mode::kDorian);
  CHECK(b.e.chords().mode() == ChordMode::kShell);
  CHECK(b.e.chords().follow() == ChordFollow::kManual);
  // Phase-6 Theme 3 Item #1: the negative transpose survives the round trip
  // (a real std::int16_t field as of Item #3's P3), and BOTH note-emitting
  // paths (Arranger + ChordEngine) are restored together.
  CHECK(b.e.arranger().master_transpose() == -7);
  CHECK(b.e.chords().master_transpose() == -7);
}

// A default (never-touched) live rig captures master_transpose == 0, and an
// existing on-disk record whose reserved field was always 0 (every record
// written before this item) still means "no transpose" once applied -- the
// exact backward-compatibility guarantee docs/reflections/phase6-theme3-
// master-transpose-scope.md Decision 4a locks.
void test_perf_zero_transpose_round_trips_as_no_transpose() {
  Band b;
  b.setup_basic();
  CHECK(b.e.arranger().master_transpose() == 0);
  CHECK(b.e.chords().master_transpose() == 0);

  b.cmd(Param::kPerformanceStore, 0, 0, 0, /*idx=*/0);
  CHECK(b.warns() == 0);
  CHECK(b.e.performances().get(0)->master_transpose == 0);

  // Drift away, then recall: both paths return to "no transpose".
  b.cmd(Param::kMasterTranspose, 9);
  CHECK(b.e.arranger().master_transpose() == 9);
  b.ev.clear();
  b.cmd(Param::kPerformanceRecall, 0, 0, 0, /*idx=*/0);
  CHECK(b.warns() == 0);
  CHECK(b.e.arranger().master_transpose() == 0);
  CHECK(b.e.chords().master_transpose() == 0);
}

// Engine-level round trip across the specific value set this item's own QA
// mandate calls out (-1, -12, +12, +7): a fresh capture -> store -> drift ->
// recall cycle per value, hitting BOTH note-emitting paths (Arranger AND
// ChordEngine) exactly like the -7 case above, complementing test_
// performance_wire.cpp's own byte-level coverage of the same value set.
void test_perf_master_transpose_round_trips_across_the_full_value_set() {
  const std::int32_t values[] = {-1, -12, 12, 7};
  for (std::int32_t v : values) {
    Band b;
    b.setup_basic();
    b.cmd(Param::kMasterTranspose, v);
    CHECK(b.warns() == 0);

    b.cmd(Param::kPerformanceStore, 0, 0, 0, /*idx=*/0);
    CHECK(b.warns() == 0);
    // Phase-6 Theme 3 Item #3 (P3): master_transpose is a real std::int16_t
    // now -- a plain numeric compare, no more low-byte reinterpret decoding.
    CHECK(b.e.performances().get(0)->master_transpose == v);

    b.cmd(Param::kMasterTranspose, 0);  // drift away to the no-op value
    CHECK(b.e.arranger().master_transpose() == 0);

    b.ev.clear();
    b.cmd(Param::kPerformanceRecall, 0, 0, 0, /*idx=*/0);
    CHECK(b.warns() == 0);
    CHECK(b.e.arranger().master_transpose() == v);
    CHECK(b.e.chords().master_transpose() == v);
  }
}

// Phase-6 Theme 3 Item #4: a NON-zero active pad bank survives the
// capture -> store -> drift -> recall round trip, mirroring master
// transpose's own -7 round-trip test above.
void test_perf_pad_bank_round_trips_a_nonzero_value() {
  Band b;
  b.setup_basic();
  b.cmd(Param::kPadBankSelect, 5);
  CHECK(b.e.pad_bank() == 5);

  b.cmd(Param::kPerformanceStore, 0, 0, 0, /*idx=*/0);
  CHECK(b.warns() == 0);
  CHECK(b.e.performances().get(0)->pad_bank_id == 5);

  b.cmd(Param::kPadBankSelect, 0);  // drift away
  CHECK(b.e.pad_bank() == 0);

  b.ev.clear();
  b.cmd(Param::kPerformanceRecall, 0, 0, 0, /*idx=*/0);
  CHECK(b.warns() == 0);
  CHECK(b.e.pad_bank() == 5);  // restored
}

// ---- item 3 (Phase-6 Theme 3): FX-chain capture/recall --------------------

// A captured Performance snapshots each role's live FX chain (Arranger::
// m_chain); recall restores it byte-for-byte, verified through the NEW
// Arranger::chain() read accessor (Arranger's own FX surface is otherwise
// write-only -- see arranger.hpp's own comment on it).
void test_perf_capture_recall_round_trip_restores_fx_chains() {
  Band b;
  b.setup_basic();
  const auto bass = static_cast<std::uint16_t>(TrackRole::kBass);
  const auto drums = static_cast<std::uint16_t>(TrackRole::kDrums);

  // Bass, slot 0: Echo, enabled, distinctive params.
  b.cmd(Param::kFxSet, 0, static_cast<std::int32_t>(InsertType::kEcho), 0, bass);
  b.cmd(Param::kFxParam, 0, 0, 3, bass);    // repeats = 3
  b.cmd(Param::kFxParam, 0, 1, 200, bass);  // vel_decay = 200
  b.cmd(Param::kFxParam, 0, 2, 50, bass);   // delay_ticks = 50

  // Drums, slot 1: VelocityProc, DISABLED, distinctive params.
  b.cmd(Param::kFxSet, 1, static_cast<std::int32_t>(InsertType::kVelocityProc), 0, drums);
  b.cmd(Param::kFxParam, 1, 0, static_cast<std::int32_t>(VelocityProcMode::kFixed), drums);
  b.cmd(Param::kFxParam, 1, 1, 90, drums);
  b.cmd(Param::kFxEnable, 1, 0, 0, drums);  // b = 0 -> disabled
  CHECK(b.warns() == 0);

  b.cmd(Param::kPerformanceStore, 0, 0, 0, /*idx=*/0);
  CHECK(b.warns() == 0);

  // Drift both chains away from the snapshot before recalling.
  b.cmd(Param::kFxClear, -1, 0, 0, bass);  // a < 0: clear the WHOLE chain
  b.cmd(Param::kFxSet, 2, static_cast<std::int32_t>(InsertType::kNoteRepeat), 0, drums);

  b.ev.clear();
  b.cmd(Param::kPerformanceRecall, 0, 0, 0, /*idx=*/0);
  CHECK(b.warns() == 0);

  const Insert* bass_slot0 = b.e.arranger().chain(TrackRole::kBass).get(0);
  CHECK(bass_slot0 != nullptr);
  CHECK(bass_slot0->type == InsertType::kEcho);
  CHECK(bass_slot0->enabled);
  CHECK(bass_slot0->params.echo.repeats == 3);
  CHECK(bass_slot0->params.echo.vel_decay == 200);
  CHECK(bass_slot0->params.echo.delay_ticks == 50);

  const Insert* drums_slot1 = b.e.arranger().chain(TrackRole::kDrums).get(1);
  CHECK(drums_slot1 != nullptr);
  CHECK(drums_slot1->type == InsertType::kVelocityProc);
  CHECK(!drums_slot1->enabled);
  CHECK(drums_slot1->params.vel_proc.mode == static_cast<std::uint8_t>(VelocityProcMode::kFixed));
  CHECK(drums_slot1->params.vel_proc.amount == 90);

  // Every OTHER slot on these two roles returns to the inert default
  // (Insert{}) -- the recall restored the WHOLE chain, not just the one slot
  // this test happened to configure.
  const Insert* bass_slot1 = b.e.arranger().chain(TrackRole::kBass).get(1);
  CHECK(bass_slot1 != nullptr);
  CHECK(bass_slot1->type == InsertType::kScaleLock);
  CHECK(bass_slot1->enabled);

  // Drums slot 2 (drifted to NoteRepeat right before recall) is back to the
  // captured default too -- proves the drift is fully undone, not merely
  // "the slots this test happened to assert on".
  const Insert* drums_slot2 = b.e.arranger().chain(TrackRole::kDrums).get(2);
  CHECK(drums_slot2 != nullptr);
  CHECK(drums_slot2->type == InsertType::kScaleLock);
}

// Torquato QA hardening (Phase-6 Theme 3 Item #3 independent verification,
// docs/reflections/phase6-theme3-performance-format-v2-review.md): the
// mandate's own "fully-populated (8 slots)" case, driven through the REAL
// product code path (Engine::capture_performance's Insert::Params memcpy ->
// Performance::insert_chains -> Engine::apply_performance's own memcpy back
// -> Arranger::restore_fx), not the wire-level byte walk test_performance_
// wire.cpp already covers. Every one of the 10 roles x 8 slots = 80 Insert
// slots gets a DISTINCT type (cycling through all 4 InsertType values),
// DISTINCT params, and a mixed enabled/disabled pattern; the chain is then
// fully CLEARED (drifted away) before recall, and every single slot is
// checked back DEEPLY (type + enabled + the exact per-type param fields) via
// Arranger::chain() -- a shallow (type-only) restore would still pass a
// weaker check, so this test reads every union member explicitly.
void test_perf_capture_recall_round_trip_fully_populates_all_80_fx_slots() {
  Band b;
  b.setup_basic();
  constexpr int kRoles = 10;
  constexpr int kSlots = 8;

  struct Expected {
    InsertType type;
    bool enabled;
    std::uint8_t a = 0;
    std::uint8_t b = 0;
    std::uint16_t c = 0;
  };
  Expected expected[kRoles][kSlots];

  for (int r = 0; r < kRoles; ++r) {
    const auto role_u16 = static_cast<std::uint16_t>(r);
    for (int s = 0; s < kSlots; ++s) {
      const int type_i = (r + s) % 4;  // cycles through all 4 InsertType values
      const auto type = static_cast<InsertType>(type_i);
      const bool enabled = (r + s) % 2 == 0;
      b.cmd(Param::kFxSet, s, type_i, 0, role_u16);
      Expected& exp = expected[r][s];
      exp.type = type;
      exp.enabled = enabled;
      switch (type) {
        case InsertType::kScaleLock:
          exp.a = static_cast<std::uint8_t>((r * 8 + s + 7) % 256);
          b.cmd(Param::kFxParam, s, 0, exp.a, role_u16);
          break;
        case InsertType::kVelocityProc:
          exp.a = static_cast<std::uint8_t>((r + s) % 3);
          exp.b = static_cast<std::uint8_t>((r * 3 + s * 2 + 11) % 256);
          b.cmd(Param::kFxParam, s, 0, exp.a, role_u16);
          b.cmd(Param::kFxParam, s, 1, exp.b, role_u16);
          break;
        case InsertType::kEcho:
          exp.a = static_cast<std::uint8_t>((r + s + 1) % 256);
          exp.b = static_cast<std::uint8_t>((r * 5 + s + 3) % 256);
          exp.c = static_cast<std::uint16_t>((r * 13 + s * 7 + 17) % 4000);
          b.cmd(Param::kFxParam, s, 0, exp.a, role_u16);
          b.cmd(Param::kFxParam, s, 1, exp.b, role_u16);
          b.cmd(Param::kFxParam, s, 2, exp.c, role_u16);
          break;
        case InsertType::kNoteRepeat:
        default:
          exp.a = static_cast<std::uint8_t>((r + s + 2) % 256);
          exp.c = static_cast<std::uint16_t>((r * 11 + s * 3 + 29) % 4000);
          b.cmd(Param::kFxParam, s, 0, exp.a, role_u16);
          b.cmd(Param::kFxParam, s, 1, exp.c, role_u16);
          break;
      }
      b.cmd(Param::kFxEnable, s, enabled ? 1 : 0, 0, role_u16);
    }
  }
  CHECK(b.warns() == 0);

  b.cmd(Param::kPerformanceStore, 0, 0, 0, /*idx=*/0);
  CHECK(b.warns() == 0);

  // Drift EVERY role's chain back to the inert default before recalling.
  for (int r = 0; r < kRoles; ++r) {
    b.cmd(Param::kFxClear, -1, 0, 0, static_cast<std::uint16_t>(r));
  }

  b.ev.clear();
  b.cmd(Param::kPerformanceRecall, 0, 0, 0, /*idx=*/0);
  CHECK(b.warns() == 0);

  for (int r = 0; r < kRoles; ++r) {
    const auto role = static_cast<TrackRole>(r);
    for (int s = 0; s < kSlots; ++s) {
      const Expected& exp = expected[r][s];
      const Insert* ins = b.e.arranger().chain(role).get(static_cast<std::size_t>(s));
      CHECK(ins != nullptr);
      CHECK(ins->type == exp.type);
      CHECK(ins->enabled == exp.enabled);
      switch (exp.type) {
        case InsertType::kScaleLock:
          CHECK(ins->params.scale_lock.strength == exp.a);
          break;
        case InsertType::kVelocityProc:
          CHECK(ins->params.vel_proc.mode == exp.a);
          CHECK(ins->params.vel_proc.amount == exp.b);
          break;
        case InsertType::kEcho:
          CHECK(ins->params.echo.repeats == exp.a);
          CHECK(ins->params.echo.vel_decay == exp.b);
          CHECK(ins->params.echo.delay_ticks == exp.c);
          break;
        case InsertType::kNoteRepeat:
          CHECK(ins->params.note_repeat.count == exp.a);
          CHECK(ins->params.note_repeat.rate_ticks == exp.c);
          break;
      }
    }
  }
}

// The mandate's own "all-empty-chain" case: a Performance captured with every
// role's chain left at its constructor default (nothing ever configured)
// recalls to the SAME inert default on every one of the 80 slots -- proves
// the empty case is not merely "untested" but explicitly byte/field-exact,
// even after the live rig is deliberately drifted to something non-default
// before the recall.
void test_perf_capture_recall_round_trip_all_empty_chains() {
  Band b;
  b.setup_basic();

  b.cmd(Param::kPerformanceStore, 0, 0, 0, /*idx=*/0);
  CHECK(b.warns() == 0);

  // Drift one role's one slot away from the captured (empty) snapshot.
  b.cmd(Param::kFxSet, 3, static_cast<std::int32_t>(InsertType::kEcho), 0,
        static_cast<std::uint16_t>(TrackRole::kLead));
  b.cmd(Param::kFxParam, 3, 0, 5, static_cast<std::uint16_t>(TrackRole::kLead));

  b.ev.clear();
  b.cmd(Param::kPerformanceRecall, 0, 0, 0, /*idx=*/0);
  CHECK(b.warns() == 0);

  for (int r = 0; r < 10; ++r) {
    const auto role = static_cast<TrackRole>(r);
    for (int s = 0; s < 8; ++s) {
      const Insert* ins = b.e.arranger().chain(role).get(static_cast<std::size_t>(s));
      CHECK(ins != nullptr);
      CHECK(ins->type == InsertType::kScaleLock);  // Insert{}'s own default
      CHECK(ins->enabled);
      CHECK(ins->params.scale_lock.strength == 0);
    }
  }
}

// ---- item 8: recall bar-gate ordering (Corelli fix #3) --------------------

void test_perf_recall_bar_gate_lands_after_clip_promotion() {
  Band b;
  b.setup_basic();  // style 0 "basic", section varA

  // Capture a DIFFERENT rig (style 1 "pop", section varC) into slot 0. Uses
  // kStyleLoad (not kStyleSwitch) so the captured Performance carries a
  // CONCRETE style_id == 1 -- kStyleSwitch routes through Arranger::
  // request_style, which never carries a concrete index (see the dedicated
  // red test test_perf_recall_loses_concrete_style_id_metadata above), and a
  // Performance captured with style_id == 0xFFFF ("keep current") would
  // never even ATTEMPT a style switch on recall, defeating this scenario.
  b.cmd(Param::kStyleLoad, 1);
  b.cmd(Param::kStyleSection,
        static_cast<std::int32_t>(SectionType::kVarC));  // transport
                                                         // stopped -> immediate
  b.cmd(Param::kPerformanceStore, 0, 0, 0, /*idx=*/0);

  // Back to the ORIGINAL live rig before arming anything (kStyleLoad also
  // resets the section to varA).
  b.cmd(Param::kStyleLoad, 0);
  CHECK(b.e.arranger().style_id() == 0);
  CHECK(b.e.arranger().current() == SectionType::kVarA);

  b.add_clip(TrackRole::kBass, 0, ContentKind::kStyleSection,
             static_cast<std::uint16_t>(SectionType::kVarB));  // clip id 0, targets varB (exists
                                                               // in "basic")
  b.cmd(Param::kTransportStart);

  // Arm BOTH a next-bar clip launch AND a next-bar performance recall for the
  // SAME bar boundary.
  b.cmd(Param::kClipLaunch, 0, 0, 0, /*idx=*/0, Boundary::kNextBar);
  b.cmd(Param::kPerformanceRecall, 0, 0, 0, /*idx=*/0, Boundary::kNextBar);

  b.ev.clear();
  b.advance(kTicksPerBar);  // cross into the shared bar boundary

  CHECK(b.warns() == 0);                                      // no glitch
  CHECK(b.e.clips().get(0)->state == LaunchState::kPlaying);  // not dropped, not stuck armed

  // Ordering proof: within this tick's events, the clip's own kClip echo
  // fired BEFORE the performance recall's kStyleLoad confirmation echo --
  // i.e. fire_clips ran (and resolved the clip against the OLD "basic"
  // style) strictly before apply_pending_performance_recall switched the rig
  // (Engine::on_tick, Corelli fix #3).
  int clip_index = -1;
  int style_echo_index = -1;
  for (std::size_t i = 0; i < b.ev.size(); ++i) {
    const OutEvent& o = b.ev[i];
    if (o.kind == OutEvent::Kind::kClip && clip_index < 0) {
      clip_index = static_cast<int>(i);
    }
    if (o.kind == OutEvent::Kind::kParamState &&
        o.code == static_cast<std::uint16_t>(Param::kStyleLoad) && style_echo_index < 0) {
      style_echo_index = static_cast<int>(i);
    }
  }
  CHECK(clip_index >= 0);
  CHECK(style_echo_index >= 0);
  CHECK(clip_index < style_echo_index);

  // The new rig (from the recall) is what's ACTUALLY in force for the
  // arranger from this bar on -- not left on "basic"/varB (what the clip
  // itself resolved against). style_id() is now safe to assert too
  // (Nazzareno's fix, see test_performance_style_id_regression.cpp).
  CHECK(b.e.arranger().style_id() == 1);
  CHECK(b.e.arranger().current() == SectionType::kVarC);
}

}  // namespace

int main() {
  test_perf_recall_unsaved_slot_applies_nothing();
  test_perf_recall_rejects_bad_style_id();
  test_perf_recall_rejects_bad_variation();
  test_perf_recall_rejects_bad_chord_sequence_id();
  test_perf_recall_rejects_mute_mask_bits_beyond_role_range();
  test_perf_recall_rejects_solo_mask_bits_beyond_role_range();
  test_perf_recall_rejects_bad_key_root();
  test_perf_recall_rejects_bad_key_mode();
  test_perf_recall_rejects_bad_chord_mode();
  test_perf_recall_rejects_bad_chord_follow();
  test_perf_recall_rejects_bad_route_port();
  test_perf_recall_rejects_bad_route_channel();
  test_perf_recall_rejects_bad_pad_bank_id();
  test_perf_capture_recall_round_trip_restores_everything();
  test_perf_zero_transpose_round_trips_as_no_transpose();
  test_perf_master_transpose_round_trips_across_the_full_value_set();
  test_perf_pad_bank_round_trips_a_nonzero_value();
  test_perf_capture_recall_round_trip_restores_fx_chains();
  test_perf_capture_recall_round_trip_fully_populates_all_80_fx_slots();
  test_perf_capture_recall_round_trip_all_empty_chains();
  test_perf_recall_bar_gate_lands_after_clip_promotion();
  return arrangrr::test::failures();
}
