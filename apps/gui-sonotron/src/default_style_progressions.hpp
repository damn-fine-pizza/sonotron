#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "chorddet/theory.hpp"

// Host-side default harmonic progressions, ONE per built-in style
// (docs/proposals/per-style-default-progressions.md). Pure DATA: this feeds
// the core's EXISTING arrangrr::ChordSequence object through the existing
// kKeySet/kSeqNew/kSeqAdd/kSeqLoop/kSeqPlay Command verbs (built by
// in_process_brain_session.cpp) -- no new StyleDef field, no ABI change, no
// core touch. Indexed EXACTLY like arrangrr::styles::kBuiltins
// (arrangrr/arranger/style.hpp): 0 basic, 1 pop, 2 rock, 3 ballad, 4 funk,
// 5 disco, 6 house, 7 swing, 8 bossa, 9 samba, 10 reggae, 11 country,
// 12 blues, 13 shuffle, 14 latin, 15 motown.

namespace sonotron {

// The longest built-in progression (blues, a full 12-bar loop) sizes the
// fixed per-style step array; every shorter progression leaves the trailing
// slots default-initialized and unused (gated by `step_count`).
inline constexpr std::size_t kMaxProgressionSteps = 12;

struct ProgressionStep {
  // Pitch class only (0 = C .. 11 = B), NOT an absolute MIDI note -- built
  // into a note at octave 4 (60 + root_pc) when the Command is constructed,
  // matching what a bare `seq add <letter>` (no octave suffix) resolves to
  // (components/platform/hostrt/shell_parse.cpp's parse_note).
  std::uint8_t root_pc = 0;
  // -1 = smart quality (theory::smart_quality derives it from the scale
  // degree, D19); otherwise an explicit arrangrr::ChordQuality value.
  std::int8_t quality_ovr = -1;
  // Step duration, in bars (the live bar length at build time, not a
  // compile-time tick count).
  std::uint8_t bars = 1;
};

struct DefaultProgression {
  std::uint8_t key_root_pc = 0;
  arrangrr::Mode key_mode = arrangrr::Mode::kMajor;
  std::uint8_t step_count = 0;
  std::array<ProgressionStep, kMaxProgressionSteps> steps{};
};

// clang-format off
inline constexpr std::array<DefaultProgression, 16> kDefaultProgressions = {{
    // 0 basic -- I-IV-V-I, plain triads.
    {.key_root_pc = 0, .key_mode = arrangrr::Mode::kMajor, .step_count = 4,
     .steps = {{{.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1},
                {.root_pc = 5, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1},
                {.root_pc = 7, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1},
                {.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1}}}},
    // 1 pop -- I-V-vi-IV, plain triads.
    {.key_root_pc = 0, .key_mode = arrangrr::Mode::kMajor, .step_count = 4,
     .steps = {{{.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1},
                {.root_pc = 7, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1},
                {.root_pc = 9, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMin), .bars = 1},
                {.root_pc = 5, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1}}}},
    // 2 rock -- I-bVII-IV-I in G mixolydian, plain triads.
    {.key_root_pc = 7, .key_mode = arrangrr::Mode::kMixolydian, .step_count = 4,
     .steps = {{{.root_pc = 7, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1},
                {.root_pc = 5, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1},
                {.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1},
                {.root_pc = 7, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1}}}},
    // 3 ballad -- I-vi-ii-V, smart sevenths, 2 bars per chord.
    {.key_root_pc = 0, .key_mode = arrangrr::Mode::kMajor, .step_count = 4,
     .steps = {{{.root_pc = 0, .quality_ovr = -1, .bars = 2},
                {.root_pc = 9, .quality_ovr = -1, .bars = 2},
                {.root_pc = 2, .quality_ovr = -1, .bars = 2},
                {.root_pc = 7, .quality_ovr = -1, .bars = 2}}}},
    // 4 funk -- static I7 vamp, one chord for the whole loop.
    {.key_root_pc = 0, .key_mode = arrangrr::Mode::kMajor, .step_count = 1,
     .steps = {{{.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 2}}}},
    // 5 disco -- I-vi-ii-V, smart sevenths.
    {.key_root_pc = 0, .key_mode = arrangrr::Mode::kMajor, .step_count = 4,
     .steps = {{{.root_pc = 0, .quality_ovr = -1, .bars = 1},
                {.root_pc = 9, .quality_ovr = -1, .bars = 1},
                {.root_pc = 2, .quality_ovr = -1, .bars = 1},
                {.root_pc = 7, .quality_ovr = -1, .bars = 1}}}},
    // 6 house -- i-VII in A minor, 2 bars per chord.
    {.key_root_pc = 9, .key_mode = arrangrr::Mode::kMinor, .step_count = 2,
     .steps = {{{.root_pc = 9, .quality_ovr = -1, .bars = 2},
                {.root_pc = 7, .quality_ovr = -1, .bars = 2}}}},
    // 7 swing -- I-vi-ii-V, smart sevenths (same shape as disco, per spec).
    {.key_root_pc = 0, .key_mode = arrangrr::Mode::kMajor, .step_count = 4,
     .steps = {{{.root_pc = 0, .quality_ovr = -1, .bars = 1},
                {.root_pc = 9, .quality_ovr = -1, .bars = 1},
                {.root_pc = 2, .quality_ovr = -1, .bars = 1},
                {.root_pc = 7, .quality_ovr = -1, .bars = 1}}}},
    // 8 bossa -- ii-V-I-vi, smart sevenths.
    {.key_root_pc = 0, .key_mode = arrangrr::Mode::kMajor, .step_count = 4,
     .steps = {{{.root_pc = 2, .quality_ovr = -1, .bars = 1},
                {.root_pc = 7, .quality_ovr = -1, .bars = 1},
                {.root_pc = 0, .quality_ovr = -1, .bars = 1},
                {.root_pc = 9, .quality_ovr = -1, .bars = 1}}}},
    // 9 samba -- I-VI7-ii-V7 (VI7 is an explicit secondary-dominant override).
    {.key_root_pc = 0, .key_mode = arrangrr::Mode::kMajor, .step_count = 4,
     .steps = {{{.root_pc = 0, .quality_ovr = -1, .bars = 1},
                {.root_pc = 9, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 1},
                {.root_pc = 2, .quality_ovr = -1, .bars = 1},
                {.root_pc = 7, .quality_ovr = -1, .bars = 1}}}},
    // 10 reggae -- I-IV, plain triads.
    {.key_root_pc = 0, .key_mode = arrangrr::Mode::kMajor, .step_count = 2,
     .steps = {{{.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1},
                {.root_pc = 5, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1}}}},
    // 11 country -- I-IV-I-V, plain triads.
    {.key_root_pc = 0, .key_mode = arrangrr::Mode::kMajor, .step_count = 4,
     .steps = {{{.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1},
                {.root_pc = 5, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1},
                {.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1},
                {.root_pc = 7, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1}}}},
    // 12 blues -- full 12-bar blues, ALL dominant 7ths (I7 even on the tonic).
    {.key_root_pc = 0, .key_mode = arrangrr::Mode::kMajor, .step_count = 12,
     .steps = {{{.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 1},
                {.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 1},
                {.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 1},
                {.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 1},
                {.root_pc = 5, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 1},
                {.root_pc = 5, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 1},
                {.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 1},
                {.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 1},
                {.root_pc = 7, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 1},
                {.root_pc = 5, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 1},
                {.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 1},
                {.root_pc = 7, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 1}}}},
    // 13 shuffle -- V7-IV7-I7-I7, all explicit dominant 7ths.
    {.key_root_pc = 0, .key_mode = arrangrr::Mode::kMajor, .step_count = 4,
     .steps = {{{.root_pc = 7, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 1},
                {.root_pc = 5, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 1},
                {.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 1},
                {.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 1}}}},
    // 14 latin -- I-IV-V-IV, plain triads.
    {.key_root_pc = 0, .key_mode = arrangrr::Mode::kMajor, .step_count = 4,
     .steps = {{{.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1},
                {.root_pc = 5, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1},
                {.root_pc = 7, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1},
                {.root_pc = 5, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1}}}},
    // 15 motown -- I-vi-IV-V, smart sevenths.
    {.key_root_pc = 0, .key_mode = arrangrr::Mode::kMajor, .step_count = 4,
     .steps = {{{.root_pc = 0, .quality_ovr = -1, .bars = 1},
                {.root_pc = 9, .quality_ovr = -1, .bars = 1},
                {.root_pc = 5, .quality_ovr = -1, .bars = 1},
                {.root_pc = 7, .quality_ovr = -1, .bars = 1}}}},
}};
// clang-format on

// Bounds-checked lookup: an out-of-range index (an unknown/future-imported
// style, docs/proposals/per-style-default-progressions.md SS3.1/SS4.2) falls
// back to entry 0 (basic's own I-IV-V-I plain-triad loop) -- the most
// genre-neutral shape available, no second shape to invent or maintain.
inline constexpr const DefaultProgression& default_progression_for(std::int32_t style_index) {
  if (style_index >= 0 && static_cast<std::size_t>(style_index) < kDefaultProgressions.size()) {
    return kDefaultProgressions[static_cast<std::size_t>(style_index)];
  }
  return kDefaultProgressions[0];
}

}  // namespace sonotron
