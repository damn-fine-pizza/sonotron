// Unit tests for the MIDI-FX insert chain (Phase-5 Item #10, node 5100/5200):
// Insert::process's four stateless per-type functions (ScaleLock/
// VelocityProc/Echo/NoteRepeat) and InsertChain::apply's own bookkeeping.
// Pure, freestanding, no Engine -- mirrors test_pad_bank.cpp's own treatment
// of PadEngine.
//
// NOT covered here (see the dedicated regression file instead):
// InsertChain::apply's behavior when MULTIPLE notes reach a later stage and
// their COMBINED fan-out would exceed kMaxChainFan -- kept isolated in its
// own target/lock, see test_insert_chain_fan_overflow_regression.cpp.

#include "arrangrr/fx/insert_chain.hpp"

#include "test.hpp"

namespace {

using namespace arrangrr;

constexpr Key kCMajor{.root_pc = 0, .mode = Mode::kMajor};

FxNote note(int n, std::uint8_t vel = 100, std::uint16_t gate = 200, TickOffset offset = 0) {
  return FxNote{.note = n, .vel = vel, .gate = gate, .offset = offset};
}

// ---- kScaleLock -------------------------------------------------------

void test_scale_lock_out_of_key_note_snaps_to_nearest_in_key_pitch() {
  Insert ins;
  ins.type = InsertType::kScaleLock;
  ins.params.scale_lock = ScaleLockParams{.strength = 1};
  const FxContext ctx{.key = kCMajor};
  FxNote out[4];
  // C#4 (61, pc 1) is chromatic to C major -- nearest in-key note is C4 (60),
  // one semitone DOWN (ties/near-ties broken downward, D16 determinism).
  const int n = ins.process(note(61), out, 4, ctx);
  CHECK(n == 1);
  CHECK(out[0].note == 60);
}

void test_scale_lock_full_search_range_still_snaps() {
  Insert ins;
  ins.type = InsertType::kScaleLock;
  ins.params.scale_lock = ScaleLockParams{.strength = 1};
  const FxContext ctx{.key = kCMajor};
  FxNote out[4];
  // F#4 (66, pc 6) is exactly the tritone -- nearest in-key note is F4 (65).
  const int n = ins.process(note(66), out, 4, ctx);
  CHECK(n == 1);
  CHECK(out[0].note == 65);
}

void test_scale_lock_already_in_key_note_is_unchanged() {
  Insert ins;
  ins.type = InsertType::kScaleLock;
  ins.params.scale_lock = ScaleLockParams{.strength = 1};
  const FxContext ctx{.key = kCMajor};
  FxNote out[4];
  const int n = ins.process(note(67), out, 4, ctx);  // G4, in C major
  CHECK(n == 1);
  CHECK(out[0].note == 67);
}

void test_scale_lock_strength_zero_is_exact_passthrough() {
  Insert ins;
  ins.type = InsertType::kScaleLock;
  ins.params.scale_lock = ScaleLockParams{.strength = 0};
  const FxContext ctx{.key = kCMajor};
  FxNote out[4];
  // 61 is chromatic to C major, but strength == 0 means NO snap at all.
  const int n = ins.process(note(61, 90, 150, 5), out, 4, ctx);
  CHECK(n == 1);
  CHECK(out[0].note == 61);
  CHECK(out[0].vel == 90 && out[0].gate == 150 && out[0].offset == 5);
}

void test_scale_lock_respects_zero_max_out() {
  Insert ins;
  ins.type = InsertType::kScaleLock;
  ins.params.scale_lock = ScaleLockParams{.strength = 1};
  const FxContext ctx{.key = kCMajor};
  FxNote out[4];
  CHECK(ins.process(note(61), out, 0, ctx) == 0);
}

// ---- kVelocityProc -------------------------------------------------------

void test_velocity_proc_scale_mode() {
  Insert ins;
  ins.type = InsertType::kVelocityProc;
  ins.params.vel_proc =
      VelocityProcParams{.mode = static_cast<std::uint8_t>(VelocityProcMode::kScale), .amount = 50};
  FxNote out[4];
  const int n = ins.process(note(100), out, 4, FxContext{});
  CHECK(n == 1);
  CHECK(out[0].vel == 50);  // 100 * 50 / 100
}

void test_velocity_proc_fixed_mode_ignores_input_velocity() {
  Insert ins;
  ins.type = InsertType::kVelocityProc;
  ins.params.vel_proc =
      VelocityProcParams{.mode = static_cast<std::uint8_t>(VelocityProcMode::kFixed), .amount = 77};
  FxNote out[4];
  const int n = ins.process(note(1), out, 4, FxContext{});
  CHECK(n == 1);
  CHECK(out[0].vel == 77);
}

void test_velocity_proc_compress_static_pulls_toward_center() {
  Insert ins;
  ins.type = InsertType::kVelocityProc;
  ins.params.vel_proc = VelocityProcParams{
      .mode = static_cast<std::uint8_t>(VelocityProcMode::kCompressStatic), .amount = 50};
  FxNote out[4];
  // 64 + (100 - 64) * (100 - 50) / 100 = 64 + 18 = 82.
  const int n = ins.process(note(100), out, 4, FxContext{});
  CHECK(n == 1);
  CHECK(out[0].vel == 82);
}

void test_velocity_proc_compress_static_full_amount_collapses_to_center() {
  Insert ins;
  ins.type = InsertType::kVelocityProc;
  ins.params.vel_proc = VelocityProcParams{
      .mode = static_cast<std::uint8_t>(VelocityProcMode::kCompressStatic), .amount = 100};
  FxNote out[4];
  const int n = ins.process(note(20), out, 4, FxContext{});
  CHECK(n == 1);
  CHECK(out[0].vel == 64);  // fully compressed to the static center
}

void test_velocity_proc_unknown_mode_falls_back_to_compress_static() {
  Insert ins;
  ins.type = InsertType::kVelocityProc;
  ins.params.vel_proc = VelocityProcParams{.mode = 99, .amount = 50};  // out-of-range mode value
  FxNote out[4];
  const int n = ins.process(note(100), out, 4, FxContext{});
  CHECK(n == 1);
  CHECK(out[0].vel == 82);  // same formula as the explicit kCompressStatic test above
}

void test_velocity_proc_output_clamped_to_1_when_scaled_to_zero() {
  Insert ins;
  ins.type = InsertType::kVelocityProc;
  ins.params.vel_proc =
      VelocityProcParams{.mode = static_cast<std::uint8_t>(VelocityProcMode::kScale), .amount = 0};
  FxNote out[4];
  const int n = ins.process(note(100), out, 4, FxContext{});
  CHECK(n == 1);
  CHECK(out[0].vel == 1);  // never 0 -- a vel-0 NoteOn would be read as a NoteOff
}

void test_velocity_proc_output_clamped_to_127_when_scaled_high() {
  Insert ins;
  ins.type = InsertType::kVelocityProc;
  ins.params.vel_proc = VelocityProcParams{
      .mode = static_cast<std::uint8_t>(VelocityProcMode::kScale), .amount = 255};
  FxNote out[4];
  const int n = ins.process(note(100), out, 4, FxContext{});
  CHECK(n == 1);
  CHECK(out[0].vel == 127);  // 100*255/100 == 255, clamped down to 127
}

void test_velocity_proc_respects_zero_max_out() {
  Insert ins;
  ins.type = InsertType::kVelocityProc;
  FxNote out[4];
  CHECK(ins.process(note(100), out, 0, FxContext{}) == 0);
}

// ---- kEcho ----------------------------------------------------------------

void test_echo_seed_plus_decaying_repeats() {
  Insert ins;
  ins.type = InsertType::kEcho;
  ins.params.echo = EchoParams{.repeats = 3, .vel_decay = 128, .delay_ticks = 60};
  FxNote out[8];
  const int n = ins.process(note(100, 100, 200, 0), out, 8, FxContext{});
  CHECK(n == 4);  // seed + 3 repeats
  // Seed is byte-identical to the input (delta == 0).
  CHECK(out[0].note == 100 && out[0].vel == 100 && out[0].offset == 0);
  // vel decays geometrically: v1 = 100*128/255 = 50; v2 = 50*128/255 = 25;
  // v3 = 25*128/255 = 12 (integer division, compounding from the PREVIOUS
  // repeat, not re-derived from the original each time).
  CHECK(out[1].vel == 50 && out[1].offset == 60);
  CHECK(out[2].vel == 25 && out[2].offset == 120);
  CHECK(out[3].vel == 12 && out[3].offset == 180);
}

void test_echo_zero_repeats_emits_only_the_seed() {
  Insert ins;
  ins.type = InsertType::kEcho;
  ins.params.echo = EchoParams{.repeats = 0, .vel_decay = 200, .delay_ticks = 40};
  FxNote out[8];
  const int n = ins.process(note(60, /*vel=*/90), out, 8, FxContext{});
  CHECK(n == 1);
  CHECK(out[0].vel == 90 && out[0].offset == 0);
}

void test_echo_no_decay_keeps_velocity_constant() {
  Insert ins;
  ins.type = InsertType::kEcho;
  ins.params.echo =
      EchoParams{.repeats = 2, .vel_decay = 255, .delay_ticks = 10};  // 255 == no decay
  FxNote out[8];
  const int n = ins.process(note(60, /*vel=*/80), out, 8, FxContext{});
  CHECK(n == 3);
  CHECK(out[0].vel == 80 && out[1].vel == 80 && out[2].vel == 80);
  CHECK(out[1].offset == 10 && out[2].offset == 20);
}

void test_echo_decay_floor_clamps_to_one_never_zero() {
  Insert ins;
  ins.type = InsertType::kEcho;
  ins.params.echo = EchoParams{.repeats = 1, .vel_decay = 0, .delay_ticks = 5};  // full decay
  FxNote out[8];
  const int n = ins.process(note(1), out, 8, FxContext{});
  CHECK(n == 2);
  CHECK(out[1].vel == 1);  // (1*0)/255 == 0, clamped up to 1 (never a phantom NoteOff)
}

void test_echo_respects_buffer_cap() {
  Insert ins;
  ins.type = InsertType::kEcho;
  ins.params.echo = EchoParams{.repeats = 5, .vel_decay = 255, .delay_ticks = 10};
  FxNote out[8];
  const int n = ins.process(note(100), out, /*max_out=*/2, FxContext{});
  CHECK(n == 2);  // seed + only 1 of the 5 requested repeats: the cap wins
  CHECK(out[0].offset == 0 && out[1].offset == 10);
}

void test_echo_respects_zero_max_out() {
  Insert ins;
  ins.type = InsertType::kEcho;
  ins.params.echo = EchoParams{.repeats = 3};
  FxNote out[8];
  CHECK(ins.process(note(100), out, 0, FxContext{}) == 0);
}

// ---- kNoteRepeat ------------------------------------------------------

void test_note_repeat_emits_count_hits_at_rate_spacing() {
  Insert ins;
  ins.type = InsertType::kNoteRepeat;
  ins.params.note_repeat = NoteRepeatParams{.count = 3, .rate_ticks = 40};
  FxNote out[8];
  const int n = ins.process(note(60, 100, 30, 0), out, 8, FxContext{});  // gate 30 < rate 40
  CHECK(n == 3);
  CHECK(out[0].offset == 0 && out[1].offset == 40 && out[2].offset == 80);
  // gate is UNCHANGED here: the input gate (30) is already below rate_ticks
  // (40), so the clamp is a no-op.
  CHECK(out[0].gate == 30 && out[1].gate == 30 && out[2].gate == 30);
}

void test_note_repeat_clamps_gate_so_hits_never_overlap() {
  Insert ins;
  ins.type = InsertType::kNoteRepeat;
  ins.params.note_repeat = NoteRepeatParams{.count = 2, .rate_ticks = 40};
  FxNote out[8];
  const int n = ins.process(note(60, 100, 500, 0), out, 8, FxContext{});  // gate 500 >> rate 40
  CHECK(n == 2);
  CHECK(out[0].gate == 40 && out[1].gate == 40);  // clamped down to rate_ticks
}

void test_note_repeat_zero_count_defaults_to_one_hit() {
  Insert ins;
  ins.type = InsertType::kNoteRepeat;
  ins.params.note_repeat = NoteRepeatParams{.count = 0, .rate_ticks = 40};
  FxNote out[8];
  const int n = ins.process(note(60), out, 8, FxContext{});
  CHECK(n == 1);
  CHECK(out[0].offset == 0);
}

void test_note_repeat_respects_buffer_cap() {
  Insert ins;
  ins.type = InsertType::kNoteRepeat;
  ins.params.note_repeat = NoteRepeatParams{.count = 10, .rate_ticks = 5};
  FxNote out[8];
  const int n = ins.process(note(60), out, /*max_out=*/3, FxContext{});
  CHECK(n == 3);
}

// ---- InsertChain::apply ----------------------------------------------------

void test_chain_default_construction_is_exact_passthrough() {
  // The golden-preservation invariant this whole item's graft depends on: a
  // freshly-constructed (never configured) chain returns EXACTLY {input}
  // unchanged, for every field.
  const InsertChain chain;
  const FxNote in = note(64, 77, 333, 11);
  FxNote out[kMaxChainFan];
  const int n = chain.apply(in, out, kMaxChainFan, FxContext{});
  CHECK(n == 1);
  CHECK(out[0].note == in.note);
  CHECK(out[0].vel == in.vel);
  CHECK(out[0].gate == in.gate);
  CHECK(out[0].offset == in.offset);
}

void test_chain_disabled_insert_is_skipped_entirely() {
  InsertChain chain;
  CHECK(chain.set_type(0, InsertType::kVelocityProc));
  CHECK(chain.set_param(0, /*mode=*/0, static_cast<std::int32_t>(VelocityProcMode::kFixed)));
  CHECK(chain.set_param(0, /*amount=*/1, 5));  // would force vel to 5 if active
  CHECK(chain.set_enabled(0, false));
  FxNote out[kMaxChainFan];
  const int n = chain.apply(note(100), out, kMaxChainFan, FxContext{});
  CHECK(n == 1);
  CHECK(out[0].vel == 100);  // untouched: the disabled slot never ran
}

void test_chain_two_fan_out_inserts_multiply() {
  InsertChain chain;
  // slot 0: NoteRepeat x2 @ rate 40 -> 2 notes at offsets {0, 40}.
  CHECK(chain.set_type(0, InsertType::kNoteRepeat));
  CHECK(chain.set_param(0, 0, 2));   // count
  CHECK(chain.set_param(0, 1, 40));  // rate_ticks
  // slot 1: Echo x1 @ delay 10, no decay -> doubles each of those into 2.
  CHECK(chain.set_type(1, InsertType::kEcho));
  CHECK(chain.set_param(1, 0, 1));    // repeats
  CHECK(chain.set_param(1, 1, 255));  // vel_decay: no decay
  CHECK(chain.set_param(1, 2, 10));   // delay_ticks

  FxNote out[kMaxChainFan];
  const int n = chain.apply(note(90, 90, 20, 0), out, kMaxChainFan, FxContext{});
  CHECK(n == 4);  // 2 (NoteRepeat) x 2 (Echo) == 4
  // Deterministic emission order: NoteRepeat's offset-0 copy expands first
  // (into offsets 0, 10), then its offset-40 copy (into offsets 40, 50).
  CHECK(out[0].offset == 0);
  CHECK(out[1].offset == 10);
  CHECK(out[2].offset == 40);
  CHECK(out[3].offset == 50);
  for (int i = 0; i < n; ++i) {
    CHECK(out[i].vel == 90);  // Echo's vel_decay == 255: no decay anywhere
  }
}

// A SINGLE insert's own fan-out exceeding kMaxChainFan, fed by exactly ONE
// input note, IS handled gracefully -- bounded, no crash. The MULTI-note
// collision case (an earlier stage's own fan-out feeding a later stage) is
// locked separately in test_insert_chain_fan_overflow_regression.cpp.
void test_chain_single_stage_overflow_is_bounded_without_crashing() {
  InsertChain chain;
  CHECK(chain.set_type(0, InsertType::kNoteRepeat));
  CHECK(chain.set_param(0, 0, 20));  // count: would want 20 notes, only 1 stage, 1 input note
  CHECK(chain.set_param(0, 1, 5));   // rate_ticks
  FxNote out[kMaxChainFan];
  const int n = chain.apply(note(60), out, kMaxChainFan, FxContext{});
  CHECK(n == kMaxChainFan);  // capped at the fan budget, not 20
}

void test_chain_apply_max_out_truncates_independently_of_the_fan_cap() {
  InsertChain chain;
  CHECK(chain.set_type(0, InsertType::kNoteRepeat));
  CHECK(chain.set_param(0, 0, 5));  // 5 hits: well under kMaxChainFan
  CHECK(chain.set_param(0, 1, 10));
  FxNote out[kMaxChainFan];
  const int n = chain.apply(note(60), out, /*max_out=*/3, FxContext{});  // caller caps tighter
  CHECK(n == 3);
}

// ---- InsertChain bookkeeping (bounds, tagged-union refresh) ---------------

void test_chain_slot_bounds() {
  InsertChain chain;
  CHECK(!chain.set_type(kMaxInserts, InsertType::kEcho));
  CHECK(chain.set_type(kMaxInserts - 1, InsertType::kEcho));  // last valid slot
  CHECK(!chain.set_param(kMaxInserts, 0, 1));
  CHECK(!chain.set_enabled(kMaxInserts, false));
  CHECK(!chain.clear(kMaxInserts));
  CHECK(chain.get(kMaxInserts) == nullptr);
  CHECK(chain.get(kMaxInserts - 1) != nullptr);
}

void test_chain_set_param_rejects_unknown_param_id_for_current_type() {
  InsertChain chain;
  CHECK(chain.set_type(0, InsertType::kScaleLock));  // only param_id 0 (strength) exists
  CHECK(chain.set_param(0, 0, 1));
  CHECK(!chain.set_param(0, 1, 1));
}

void test_chain_set_type_refreshes_stale_union_bits() {
  InsertChain chain;
  CHECK(chain.set_type(0, InsertType::kEcho));
  CHECK(chain.set_param(0, 0, 7));  // Echo.repeats = 7
  CHECK(chain.get(0)->params.echo.repeats == 7);
  CHECK(chain.set_type(0, InsertType::kScaleLock));  // switch type: must drop the stale Echo bits
  CHECK(chain.get(0)->params.scale_lock.strength == 0);  // fresh default, not a misread of repeats
}

void test_chain_clear_and_clear_all() {
  InsertChain chain;
  CHECK(chain.set_type(0, InsertType::kEcho));
  CHECK(chain.set_param(0, 0, 4));
  CHECK(chain.clear(0));
  CHECK(chain.get(0)->type == InsertType::kScaleLock);  // reset to the inert default
  CHECK(chain.get(0)->enabled);

  CHECK(chain.set_type(2, InsertType::kNoteRepeat));
  CHECK(chain.set_enabled(3, false));
  chain.clear_all();
  CHECK(chain.get(2)->type == InsertType::kScaleLock);
  CHECK(chain.get(3)->enabled);  // back to the enabled-by-default inert state
}

void test_chain_set_param_clamps_negative_and_overflow_values() {
  InsertChain chain;
  CHECK(chain.set_type(0, InsertType::kScaleLock));
  CHECK(chain.set_param(0, 0, -5));  // negative clamps to 0
  CHECK(chain.get(0)->params.scale_lock.strength == 0);
  CHECK(chain.set_param(0, 0, 9999));  // overflow clamps to the u8 max
  CHECK(chain.get(0)->params.scale_lock.strength == 255);

  CHECK(chain.set_type(0, InsertType::kEcho));
  CHECK(chain.set_param(0, 2, 99999));  // delay_ticks is u16: clamps to 65535
  CHECK(chain.get(0)->params.echo.delay_ticks == 65535);
}

// ---- kGroove (Phase-6 Theme 4, 5210) --------------------------------------

void test_groove_process_is_identity_with_default_params() {
  Insert ins;
  ins.type = InsertType::kGroove;
  const FxContext ctx{.step = 3, .tick = 700, .role = 2, .groove = GrooveParams{}};
  FxNote out[4];
  const int n = ins.process(note(64, 90, 200, 11), out, 4, ctx);
  CHECK(n == 1);
  CHECK(out[0].note == 64 && out[0].vel == 90 && out[0].gate == 200 && out[0].offset == 11);
  CHECK(out[0].groove_offset == 0);  // identity GrooveParams: no push, no vel change
}

void test_groove_process_applies_accent_and_carries_the_push_via_groove_offset() {
  Insert ins;
  ins.type = InsertType::kGroove;
  // step 4: step%4==0 but step%8!=0 -- the DOWNWARD accent branch.
  const FxContext ctx{.step = 4, .tick = 0, .role = 0, .groove = GrooveParams{.accent = 100}};
  FxNote out[4];
  const int n = ins.process(note(60, 100), out, 4, ctx);
  CHECK(n == 1);
  CHECK(out[0].vel == 92);           // 100 - 8*100/100
  CHECK(out[0].offset == 0);         // untouched -- the chain's own clock
  CHECK(out[0].groove_offset == 0);  // no swing/humanize configured: no timing push
}

void test_groove_process_uses_the_notes_own_offset_not_ctx_step_alone() {
  Insert ins;
  ins.type = InsertType::kGroove;
  // ctx.step == 0 (an upward accent position would NOT apply there), but
  // in.offset advances it by exactly 2 steps to step 2 (still no accent
  // branch fires at step 2, so use swing instead to make the offset-aware
  // position observable): swing_grid 8 pushes off-8th steps (2, 6, 10, 14).
  const FxContext ctx{.step = 0, .tick = 0, .role = 0, .groove = GrooveParams{.swing = 100}};
  FxNote out[4];
  const int n =
      ins.process(note(60, 100, 200, static_cast<TickOffset>(2 * kTicksPerStep)), out, 4, ctx);
  CHECK(n == 1);
  CHECK(out[0].groove_offset > 0);  // swing pushed -- proves step_j = ctx.step + in.offset/step
}

void test_groove_process_respects_zero_max_out() {
  Insert ins;
  ins.type = InsertType::kGroove;
  FxNote out[4];
  CHECK(ins.process(note(60), out, 0, FxContext{}) == 0);
}

// ---- kArp (Phase-6 Theme 4, 5220) -- Insert::ingest/on_tick, switch-
// dispatched capability, no-op for the 5 other types -------------------------

constexpr InsertType kNonArpTypes[] = {InsertType::kScaleLock, InsertType::kVelocityProc,
                                       InsertType::kEcho, InsertType::kNoteRepeat,
                                       InsertType::kGroove};

void test_insert_ingest_is_a_noop_for_every_non_arp_type() {
  ArpeggiatorEngine arp;
  for (InsertType t : kNonArpTypes) {
    Insert ins;
    ins.type = t;
    ins.ingest(note(60, 100), arp);
  }
  CHECK(!arp.active());  // nothing was ever fed
}

void test_insert_ingest_feeds_the_arp_only_when_enabled() {
  ArpeggiatorEngine arp;
  Insert ins;
  ins.type = InsertType::kArp;
  ins.enabled = false;
  ins.ingest(note(60, 100), arp);
  CHECK(!arp.active());  // disabled slot: still a no-op

  ins.enabled = true;
  ins.ingest(note(60, 100), arp);
  CHECK(arp.active());
  CHECK(arp.held_count() == 1);
}

void test_insert_ingest_ignores_an_out_of_range_note() {
  ArpeggiatorEngine arp;
  Insert ins;
  ins.type = InsertType::kArp;
  ins.ingest(note(-1, 100), arp);
  ins.ingest(note(128, 100), arp);
  CHECK(!arp.active());
}

void test_insert_process_karp_always_swallows_the_note() {
  Insert ins;
  ins.type = InsertType::kArp;
  FxNote out[4];
  // Never scheduled directly through the ordinary per-note apply() pass --
  // held-chord ingest and emission are separate capabilities (ingest()/
  // on_tick() below).
  CHECK(ins.process(note(60, 100), out, 4, FxContext{}) == 0);
}

void test_insert_on_tick_is_a_noop_for_every_non_arp_type() {
  ArpeggiatorEngine arp;
  arp.note_on(60, 100);
  for (InsertType t : kNonArpTypes) {
    Insert ins;
    ins.type = t;
    FxNote out[4];
    CHECK(ins.on_tick(0, arp, out, 4) == 0);
  }
}

void test_insert_on_tick_noop_when_disabled_or_unsized() {
  ArpeggiatorEngine arp;
  arp.note_on(60, 100);
  Insert ins;
  ins.type = InsertType::kArp;
  ins.enabled = false;
  FxNote out[4];
  CHECK(ins.on_tick(0, arp, out, 4) == 0);
  ins.enabled = true;
  CHECK(ins.on_tick(0, arp, out, /*max_out=*/0) == 0);
}

void test_insert_on_tick_syncs_config_from_its_own_params_before_ticking() {
  // Two held notes, direction kDown: the FIRST emitted note is the HIGHER
  // pitch -- proves on_tick() actually pushed this slot's own ArpInsertParams
  // (rate/direction/octaves/gate) into the engine before calling it, rather
  // than ticking whatever the engine's own (default kUp) params already held.
  ArpeggiatorEngine arp;
  arp.note_on(60, 100);
  arp.note_on(64, 100);
  Insert ins;
  ins.type = InsertType::kArp;
  ins.params.arp = ArpInsertParams{.rate = static_cast<std::uint8_t>(ArpRate::kSixteenth),
                                   .direction = static_cast<std::uint8_t>(ArpDirection::kDown),
                                   .octaves = 1,
                                   .gate = 100};
  FxNote out[4];
  const int n = ins.on_tick(0, arp, out, 4);
  CHECK(n == 1);
  CHECK(out[0].note == 64);  // kDown starts from the highest held note
}

// ---- InsertChain: has_arp/ingest/apply_from/on_tick (Phase-6 Theme 4) -----

void test_chain_has_arp_detects_only_an_enabled_karp_slot() {
  InsertChain chain;
  CHECK(!chain.has_arp());  // fresh chain: slot 7 is kGroove, nothing is kArp
  CHECK(chain.set_type(0, InsertType::kArp));
  CHECK(chain.has_arp());
  CHECK(chain.set_enabled(0, false));
  CHECK(!chain.has_arp());  // disabled: no longer counts
}

void test_chain_ingest_forwards_to_whichever_slot_is_karp() {
  InsertChain chain;
  CHECK(chain.set_type(3, InsertType::kArp));
  ArpeggiatorEngine arp;
  chain.ingest(note(67, 90), arp);
  CHECK(arp.active());
  CHECK(arp.held_count() == 1);
}

void test_chain_on_tick_emits_through_the_slots_after_the_arp_slot() {
  InsertChain chain;
  CHECK(chain.set_type(0, InsertType::kArp));
  CHECK(chain.set_param(0, /*rate=*/0, static_cast<std::int32_t>(ArpRate::kSixteenth)));
  CHECK(chain.set_param(0, /*gate=*/3, 100));
  CHECK(chain.set_type(1, InsertType::kVelocityProc));
  CHECK(chain.set_param(1, 0, static_cast<std::int32_t>(VelocityProcMode::kFixed)));
  CHECK(chain.set_param(1, 1, 77));  // forces every note past slot 1 to vel 77

  ArpeggiatorEngine arp;
  chain.ingest(note(60, 100), arp);  // held chord: single note 60

  FxNote out[kMaxChainFan];
  const int n = chain.on_tick(0, arp, FxContext{}, out, kMaxChainFan);
  CHECK(n == 1);
  CHECK(out[0].note == 60);
  CHECK(out[0].vel == 77);  // proves apply_from(slot+1, ...) actually ran slot 1
}

// Torquato QA heavy-pass (Phase-6 Theme 4, target 3, "conversely" half): a
// chain with NO enabled kArp slot -- the state of every role before this item
// (or any role that simply never opts in) -- is a genuine no-op through the
// new P5 ungated pass: on_tick() never writes to `out` regardless of how
// many notes the (fresh, all-default) held arp engine claims to have, or how
// many ticks are probed. This is the structural half of the byte-identity
// guarantee the 22 existing goldens already confirm empirically.
void test_chain_on_tick_is_a_genuine_noop_with_no_enabled_arp_slot() {
  const InsertChain chain;  // fresh: default trailing kGroove, no kArp anywhere
  ArpeggiatorEngine arp;
  arp.note_on(60, 100);  // even a genuinely active engine changes nothing
  FxNote out[kMaxChainFan];
  for (Tick t = 0; t < 4 * kTicksPerStep; t += 60) {
    CHECK(chain.on_tick(t, arp, FxContext{}, out, kMaxChainFan) == 0);
  }
}

void test_chain_apply_from_skips_earlier_slots() {
  InsertChain chain;
  CHECK(chain.set_type(0, InsertType::kVelocityProc));  // would force vel 5 if reached
  CHECK(chain.set_param(0, 0, static_cast<std::int32_t>(VelocityProcMode::kFixed)));
  CHECK(chain.set_param(0, 1, 5));
  FxNote out[kMaxChainFan];
  const int n = chain.apply_from(1, note(60, 100), out, kMaxChainFan, FxContext{});
  CHECK(n == 1);
  CHECK(out[0].vel == 100);  // slot 0 never ran -- apply_from started at slot 1
}

// ---- InsertChain: groove pinned-last by default (Phase-6 Theme 4, 5210) ---

void test_chain_default_construction_pins_groove_as_the_last_slot() {
  InsertChain chain;
  const Insert* last = chain.get(kMaxInserts - 1);
  CHECK(last != nullptr);
  CHECK(last->type == InsertType::kGroove);
  CHECK(last->enabled);
  // Every other slot keeps the ORIGINAL kScaleLock/strength==0 default.
  for (std::size_t s = 0; s + 1 < kMaxInserts; ++s) {
    CHECK(chain.get(s)->type == InsertType::kScaleLock);
  }
}

void test_chain_clear_last_slot_restores_groove_not_scale_lock() {
  InsertChain chain;
  CHECK(chain.set_type(kMaxInserts - 1, InsertType::kEcho));  // drift it away
  CHECK(chain.clear(kMaxInserts - 1));
  CHECK(chain.get(kMaxInserts - 1)->type == InsertType::kGroove);
}

void test_chain_clear_all_restores_groove_last_and_scale_lock_elsewhere() {
  InsertChain chain;
  CHECK(chain.set_type(0, InsertType::kEcho));
  CHECK(chain.set_type(kMaxInserts - 1, InsertType::kArp));
  chain.clear_all();
  CHECK(chain.get(0)->type == InsertType::kScaleLock);
  CHECK(chain.get(kMaxInserts - 1)->type == InsertType::kGroove);
}

void test_chain_set_type_kgroove_has_no_settable_params() {
  InsertChain chain;
  CHECK(chain.set_type(0, InsertType::kGroove));
  CHECK(!chain.set_param(0, 0, 1));  // Fork 2/3: no per-slot params for kGroove
}

void test_chain_set_type_karp_clamps_every_field_to_its_own_range() {
  InsertChain chain;
  CHECK(chain.set_type(0, InsertType::kArp));
  // Fresh defaults match ArpeggiatorParams' own (rate=kSixteenth, direction=
  // kUp, octaves=1, gate=75).
  CHECK(chain.get(0)->params.arp.rate == static_cast<std::uint8_t>(ArpRate::kSixteenth));
  CHECK(chain.get(0)->params.arp.direction == static_cast<std::uint8_t>(ArpDirection::kUp));
  CHECK(chain.get(0)->params.arp.octaves == 1);
  CHECK(chain.get(0)->params.arp.gate == 75);

  CHECK(chain.set_param(0, /*rate=*/0, 999));  // clamps to the last valid ArpRate
  CHECK(chain.get(0)->params.arp.rate == kArpRateCount - 1);
  CHECK(chain.set_param(0, /*direction=*/1, -5));  // clamps to 0
  CHECK(chain.get(0)->params.arp.direction == 0);
  CHECK(chain.set_param(0, /*octaves=*/2, 0));  // clamps to the 1..4 range's low end
  CHECK(chain.get(0)->params.arp.octaves == 1);
  CHECK(chain.set_param(0, /*octaves=*/2, 9));  // clamps to the high end
  CHECK(chain.get(0)->params.arp.octaves == 4);
  CHECK(chain.set_param(0, /*gate=*/3, 500));  // clamps to 100
  CHECK(chain.get(0)->params.arp.gate == 100);
  CHECK(!chain.set_param(0, /*unknown=*/4, 1));
}

// ---- Fork 2's documented footgun / the position-aware counterpart --------

void test_chain_default_last_groove_regrooves_each_fanned_copy_at_its_own_position() {
  InsertChain chain;  // slot 7 auto-defaults to kGroove (pinned-last)
  CHECK(chain.set_type(0, InsertType::kEcho));
  CHECK(chain.set_param(0, 0, 1));    // repeats
  CHECK(chain.set_param(0, 1, 255));  // vel_decay: no decay
  CHECK(chain.set_param(0, 2, static_cast<std::int32_t>(4 * kTicksPerStep)));  // delay_ticks

  const FxContext ctx{.step = 4, .tick = 0, .role = 0, .groove = GrooveParams{.accent = 100}};
  FxNote out[kMaxChainFan];
  const int n = chain.apply(note(60, 100), out, kMaxChainFan, ctx);
  CHECK(n == 2);
  CHECK(out[0].vel == 92);   // seed re-groomed at step 4 (downward push)
  CHECK(out[1].vel == 115);  // the copy re-groomed at ITS OWN step 4+4==8 (upward push)
}

void test_chain_reordered_groove_before_echo_does_not_regroove_fanned_copies() {
  // Fork 2's documented footgun: kGroove is genuinely reorderable (not
  // enforced-last), but a fanning insert placed AFTER a REORDERED kGroove
  // only sees the groove computed ONCE, at the seed's own grid position --
  // the fanned copy inherits that single push instead of re-grooving at its
  // own, later position (contrast with the default-last test above, which
  // regrooves each copy at 115).
  InsertChain chain;
  CHECK(chain.set_type(0, InsertType::kGroove));  // reordered to the FRONT
  CHECK(chain.set_type(1, InsertType::kEcho));
  CHECK(chain.set_param(1, 0, 1));    // repeats
  CHECK(chain.set_param(1, 1, 255));  // vel_decay: no decay
  CHECK(chain.set_param(1, 2, static_cast<std::int32_t>(4 * kTicksPerStep)));  // delay_ticks
  // Neutralize the chain's OWN auto-pinned-last kGroove slot (index
  // kMaxInserts-1) so this test isolates the REORDERED slot-0 kGroove as the
  // chain's ONLY groove instance -- otherwise the default trailing kGroove
  // would ALSO run, re-grooving each copy at its own position exactly like
  // the default-last test above, masking the very footgun this test exists
  // to pin.
  CHECK(chain.set_type(kMaxInserts - 1, InsertType::kScaleLock));

  const FxContext ctx{.step = 4, .tick = 0, .role = 0, .groove = GrooveParams{.accent = 100}};
  FxNote out[kMaxChainFan];
  const int n = chain.apply(note(60, 100), out, kMaxChainFan, ctx);
  CHECK(n == 2);
  CHECK(out[0].vel == 92);  // the seed's own groomed vel
  CHECK(out[1].vel == 92);  // the copy INHERITS it unchanged -- NOT the 115 a
                            // fresh groove at its own step 8 would produce
}

}  // namespace

int main() {
  test_scale_lock_out_of_key_note_snaps_to_nearest_in_key_pitch();
  test_scale_lock_full_search_range_still_snaps();
  test_scale_lock_already_in_key_note_is_unchanged();
  test_scale_lock_strength_zero_is_exact_passthrough();
  test_scale_lock_respects_zero_max_out();

  test_velocity_proc_scale_mode();
  test_velocity_proc_fixed_mode_ignores_input_velocity();
  test_velocity_proc_compress_static_pulls_toward_center();
  test_velocity_proc_compress_static_full_amount_collapses_to_center();
  test_velocity_proc_unknown_mode_falls_back_to_compress_static();
  test_velocity_proc_output_clamped_to_1_when_scaled_to_zero();
  test_velocity_proc_output_clamped_to_127_when_scaled_high();
  test_velocity_proc_respects_zero_max_out();

  test_echo_seed_plus_decaying_repeats();
  test_echo_zero_repeats_emits_only_the_seed();
  test_echo_no_decay_keeps_velocity_constant();
  test_echo_decay_floor_clamps_to_one_never_zero();
  test_echo_respects_buffer_cap();
  test_echo_respects_zero_max_out();

  test_note_repeat_emits_count_hits_at_rate_spacing();
  test_note_repeat_clamps_gate_so_hits_never_overlap();
  test_note_repeat_zero_count_defaults_to_one_hit();
  test_note_repeat_respects_buffer_cap();

  test_chain_default_construction_is_exact_passthrough();
  test_chain_disabled_insert_is_skipped_entirely();
  test_chain_two_fan_out_inserts_multiply();
  test_chain_single_stage_overflow_is_bounded_without_crashing();
  test_chain_apply_max_out_truncates_independently_of_the_fan_cap();

  test_chain_slot_bounds();
  test_chain_set_param_rejects_unknown_param_id_for_current_type();
  test_chain_set_type_refreshes_stale_union_bits();
  test_chain_clear_and_clear_all();
  test_chain_set_param_clamps_negative_and_overflow_values();

  test_groove_process_is_identity_with_default_params();
  test_groove_process_applies_accent_and_carries_the_push_via_groove_offset();
  test_groove_process_uses_the_notes_own_offset_not_ctx_step_alone();
  test_groove_process_respects_zero_max_out();

  test_insert_ingest_is_a_noop_for_every_non_arp_type();
  test_insert_ingest_feeds_the_arp_only_when_enabled();
  test_insert_ingest_ignores_an_out_of_range_note();
  test_insert_process_karp_always_swallows_the_note();
  test_insert_on_tick_is_a_noop_for_every_non_arp_type();
  test_insert_on_tick_noop_when_disabled_or_unsized();
  test_insert_on_tick_syncs_config_from_its_own_params_before_ticking();

  test_chain_has_arp_detects_only_an_enabled_karp_slot();
  test_chain_ingest_forwards_to_whichever_slot_is_karp();
  test_chain_on_tick_emits_through_the_slots_after_the_arp_slot();
  test_chain_on_tick_is_a_genuine_noop_with_no_enabled_arp_slot();
  test_chain_apply_from_skips_earlier_slots();

  test_chain_default_construction_pins_groove_as_the_last_slot();
  test_chain_clear_last_slot_restores_groove_not_scale_lock();
  test_chain_clear_all_restores_groove_last_and_scale_lock_elsewhere();
  test_chain_set_type_kgroove_has_no_settable_params();
  test_chain_set_type_karp_clamps_every_field_to_its_own_range();

  test_chain_default_last_groove_regrooves_each_fanned_copy_at_its_own_position();
  test_chain_reordered_groove_before_echo_does_not_regroove_fanned_copies();

  return arrangrr::test::failures();
}
