// Unit tests for arrangrr::perf::validate (Phase-5 Item #9 coverage
// follow-up): the PURE bounds-check half of what used to be Engine::
// validate_performance's entire body, extracted (performance.hpp/
// performance.cpp) so every branch is directly unit-testable WITHOUT an
// Engine instance -- before this split it was reachable only through
// functional Engine tests (test_performance.cpp's own atomicity coverage)
// and never counted toward the enforced metric-1 unit-coverage gate.
//
// Pattern: one canonical VALID Performance (validate() == true), then for
// EACH field perturb ONLY that field across its exact boundary -- the
// value AT the limit (still valid) and the value ONE PAST the limit
// (invalid) -- asserting the precise branch perf::validate takes. This is
// pure, freestanding, no Engine/TestEngine.

#include "arrangrr/perf/performance.hpp"

#include "arrangrr/arranger/style.hpp"      // styles::kBuiltinCount
#include "arrangrr/chord/chord_engine.hpp"  // ChordMode/kChordModeCount, ChordFollow, Mode/kModeCount
#include "test.hpp"

namespace {

using namespace arrangrr;

// A canonical, fully in-range Performance -- every field at a boundary-safe
// interior value. Each test below copies this and perturbs ONE field.
Performance valid_performance() {
  Performance p;
  p.style_id = 0;  // a real builtin index
  p.variation = static_cast<std::uint8_t>(SectionType::kVarA);
  p.chord_sequence_id = 0xFFFF;  // none
  p.track_mute_mask = 0;
  p.track_solo_mask = 0;
  p.key_root = 0;
  p.key_mode = 0;
  p.chord_mode = 0;
  p.chord_follow = 0;
  // routes[] default-construct to {port=0, channel=0, enabled=0}, in range.
  return p;
}

constexpr std::size_t kNoSequences = 0;

void test_valid_performance_passes() { CHECK(perf::validate(valid_performance(), kNoSequences)); }

// ---- style_id ---------------------------------------------------------

void test_style_id_sentinel_0xffff_is_valid() {
  Performance p = valid_performance();
  p.style_id = 0xFFFF;  // "keep current"
  CHECK(perf::validate(p, kNoSequences));
}

void test_style_id_last_builtin_is_valid() {
  Performance p = valid_performance();
  p.style_id = static_cast<std::uint16_t>(styles::kBuiltinCount - 1);
  CHECK(perf::validate(p, kNoSequences));
}

void test_style_id_one_past_builtin_count_is_invalid() {
  Performance p = valid_performance();
  p.style_id = static_cast<std::uint16_t>(styles::kBuiltinCount);
  CHECK(!perf::validate(p, kNoSequences));
}

// ---- variation ----------------------------------------------------------

void test_variation_last_section_type_is_valid() {
  Performance p = valid_performance();
  p.variation = static_cast<std::uint8_t>(kSectionTypeCount - 1);
  CHECK(perf::validate(p, kNoSequences));
}

void test_variation_one_past_section_type_count_is_invalid() {
  Performance p = valid_performance();
  p.variation = kSectionTypeCount;
  CHECK(!perf::validate(p, kNoSequences));
}

// ---- chord_sequence_id vs chord_sequence_count ---------------------------

void test_chord_sequence_id_sentinel_0xffff_is_valid_even_with_zero_sequences() {
  Performance p = valid_performance();
  p.chord_sequence_id = 0xFFFF;  // none
  CHECK(perf::validate(p, kNoSequences));
}

void test_chord_sequence_id_last_valid_index_is_valid() {
  Performance p = valid_performance();
  constexpr std::size_t kCount = 4;
  p.chord_sequence_id = static_cast<std::uint16_t>(kCount - 1);
  CHECK(perf::validate(p, kCount));
}

void test_chord_sequence_id_equal_to_count_is_invalid() {
  Performance p = valid_performance();
  constexpr std::size_t kCount = 4;
  p.chord_sequence_id = static_cast<std::uint16_t>(kCount);
  CHECK(!perf::validate(p, kCount));
}

void test_chord_sequence_id_concrete_with_zero_sequences_is_invalid() {
  Performance p = valid_performance();
  p.chord_sequence_id = 0;  // a concrete (non-sentinel) id, but no sequences exist
  CHECK(!perf::validate(p, kNoSequences));
}

// ---- track_mute_mask / track_solo_mask (10-role space, bits 0..9) -------

void test_mute_mask_bit_9_is_valid() {
  Performance p = valid_performance();
  p.track_mute_mask = 1u << 9;  // the 10th (last) role
  CHECK(perf::validate(p, kNoSequences));
}

void test_mute_mask_bit_10_is_invalid() {
  Performance p = valid_performance();
  p.track_mute_mask = 1u << 10;  // one past the 10-role space
  CHECK(!perf::validate(p, kNoSequences));
}

void test_solo_mask_bit_9_is_valid() {
  Performance p = valid_performance();
  p.track_solo_mask = 1u << 9;
  CHECK(perf::validate(p, kNoSequences));
}

void test_solo_mask_bit_10_is_invalid() {
  Performance p = valid_performance();
  p.track_solo_mask = 1u << 10;
  CHECK(!perf::validate(p, kNoSequences));
}

// ---- key_root / key_mode -------------------------------------------------

void test_key_root_11_is_valid() {
  Performance p = valid_performance();
  p.key_root = 11;  // last pitch class
  CHECK(perf::validate(p, kNoSequences));
}

void test_key_root_12_is_invalid() {
  Performance p = valid_performance();
  p.key_root = 12;
  CHECK(!perf::validate(p, kNoSequences));
}

void test_key_mode_last_mode_is_valid() {
  Performance p = valid_performance();
  p.key_mode = static_cast<std::uint8_t>(kModeCount - 1);
  CHECK(perf::validate(p, kNoSequences));
}

void test_key_mode_one_past_mode_count_is_invalid() {
  Performance p = valid_performance();
  p.key_mode = kModeCount;
  CHECK(!perf::validate(p, kNoSequences));
}

// ---- chord_mode -----------------------------------------------------------

void test_chord_mode_last_mode_is_valid() {
  Performance p = valid_performance();
  p.chord_mode = static_cast<std::uint8_t>(kChordModeCount - 1);
  CHECK(perf::validate(p, kNoSequences));
}

void test_chord_mode_one_past_chord_mode_count_is_invalid() {
  Performance p = valid_performance();
  p.chord_mode = kChordModeCount;
  CHECK(!perf::validate(p, kNoSequences));
}

// ---- chord_follow -----------------------------------------------------------

void test_chord_follow_live_priority_is_valid() {
  Performance p = valid_performance();
  p.chord_follow = static_cast<std::uint8_t>(ChordFollow::kLivePriority);
  CHECK(perf::validate(p, kNoSequences));
}

void test_chord_follow_one_past_live_priority_is_invalid() {
  Performance p = valid_performance();
  p.chord_follow = static_cast<std::uint8_t>(ChordFollow::kLivePriority) + 1;
  CHECK(!perf::validate(p, kNoSequences));
}

// ---- routes[] ---------------------------------------------------------

void test_route_port_and_channel_at_max_is_valid() {
  Performance p = valid_performance();
  p.routes[3] =
      PerfRoute{.port = static_cast<std::uint8_t>(kMaxPorts - 1), .channel = 15, .enabled = 1};
  CHECK(perf::validate(p, kNoSequences));
}

void test_route_port_one_past_kmaxports_is_invalid() {
  Performance p = valid_performance();
  p.routes[3] = PerfRoute{.port = static_cast<std::uint8_t>(kMaxPorts), .channel = 0, .enabled = 1};
  CHECK(!perf::validate(p, kNoSequences));
}

void test_route_channel_16_is_invalid() {
  Performance p = valid_performance();
  p.routes[3] = PerfRoute{.port = 0, .channel = 16, .enabled = 1};
  CHECK(!perf::validate(p, kNoSequences));
}

}  // namespace

int main() {
  test_valid_performance_passes();
  test_style_id_sentinel_0xffff_is_valid();
  test_style_id_last_builtin_is_valid();
  test_style_id_one_past_builtin_count_is_invalid();
  test_variation_last_section_type_is_valid();
  test_variation_one_past_section_type_count_is_invalid();
  test_chord_sequence_id_sentinel_0xffff_is_valid_even_with_zero_sequences();
  test_chord_sequence_id_last_valid_index_is_valid();
  test_chord_sequence_id_equal_to_count_is_invalid();
  test_chord_sequence_id_concrete_with_zero_sequences_is_invalid();
  test_mute_mask_bit_9_is_valid();
  test_mute_mask_bit_10_is_invalid();
  test_solo_mask_bit_9_is_valid();
  test_solo_mask_bit_10_is_invalid();
  test_key_root_11_is_valid();
  test_key_root_12_is_invalid();
  test_key_mode_last_mode_is_valid();
  test_key_mode_one_past_mode_count_is_invalid();
  test_chord_mode_last_mode_is_valid();
  test_chord_mode_one_past_chord_mode_count_is_invalid();
  test_chord_follow_live_priority_is_valid();
  test_chord_follow_one_past_live_priority_is_invalid();
  test_route_port_and_channel_at_max_is_valid();
  test_route_port_one_past_kmaxports_is_invalid();
  test_route_channel_16_is_invalid();
  return arrangrr::test::failures();
}
