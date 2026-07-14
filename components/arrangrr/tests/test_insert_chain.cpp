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

  return arrangrr::test::failures();
}
