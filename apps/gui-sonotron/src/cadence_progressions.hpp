#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "default_style_progressions.hpp"

// Option B cadence tags (docs/proposals/gui-live-harmony-musical-design.md
// S3.4): ONE short, style-appropriate V-I/IV-I (or minor-key v7-i) cadential
// tag per built-in style, swapped into a SHARED "cadence" ChordSequence pool
// slot the instant the arrangement enters Ending1/Ending2, so the ending
// resolves onto the tonic via a real cadential approach instead of whatever
// chord the free-running main progression happened to be on. Every entry is
// exactly 2 steps, 1 bar each -- matching Ending1's and Ending2's own
// IDENTICAL 2-bar length across all 16 built-in styles (measured in the
// design doc's own S3.1). Reuses sonotron::ProgressionStep (root_pc is a
// PITCH CLASS, not a scale degree -- Engine::seq_add re-derives the degree
// against whichever key the cadence pool slot is re-keyed to, see
// in_process_brain_session.cpp's append_cadence_commands()) and each
// style's OWN key_root_pc/key_mode from kDefaultProgressions -- no key data
// is duplicated here, avoiding any chance of the two tables drifting apart.
// Indexed EXACTLY like kDefaultProgressions / arrangrr::styles::kBuiltins.

namespace sonotron {

inline constexpr std::size_t kCadenceStepCount = 2;

struct CadenceProgression {
  std::array<ProgressionStep, kCadenceStepCount> steps{};
};

// clang-format off
inline constexpr std::array<CadenceProgression, 16> kCadenceProgressions = {{
    // 0 basic -- V-I, plain triads (matches basic's own I-IV-V-I plain-triad idiom).
    {.steps = {{{.root_pc = 7, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1},
                {.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1}}}},
    // 1 pop -- IV-I, plain triads (S3.4's own "pop: IV->I").
    {.steps = {{{.root_pc = 5, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1},
                {.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1}}}},
    // 2 rock -- IV-I in G mixolydian, plain triads. Mixolydian has no
    // functional V (its own v is a MINOR triad, no leading tone), so this
    // uses the mode's own plagal (backdoor) IV-I motion instead -- the same
    // shape rock's own progression already leans on (I-bVII-IV-I).
    {.steps = {{{.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1},
                {.root_pc = 7, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1}}}},
    // 3 ballad -- V-I, smart sevenths (S3.4's own "ballad: V->I"; matches
    // ballad's own smart-seventh harmonic language).
    {.steps = {{{.root_pc = 7, .quality_ovr = -1, .bars = 1},
                {.root_pc = 0, .quality_ovr = -1, .bars = 1}}}},
    // 4 funk -- V7-I7 (funk has no natural I-approach at all -- its own vamp
    // is a static I7 -- so this borrows the dominant-7th vocabulary the vamp
    // itself already uses rather than inventing an un-idiomatic plain triad).
    {.steps = {{{.root_pc = 7, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 1},
                {.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 1}}}},
    // 5 disco -- V-I, smart sevenths (S3.4's own "disco/swing: V->I").
    {.steps = {{{.root_pc = 7, .quality_ovr = -1, .bars = 1},
                {.root_pc = 0, .quality_ovr = -1, .bars = 1}}}},
    // 6 house -- v7-i in A minor: natural minor has no leading tone either,
    // so this borrows the harmonic-minor V7 (an explicit quality override on
    // the SAME diatonic root, not a chromatic root change -- Engine::seq_add
    // only rejects non-diatonic ROOTS, never an explicit quality override,
    // exactly like samba's own forced VI7 secondary dominant) for a genuine
    // minor-key authentic cadence.
    {.steps = {{{.root_pc = 4, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 1},
                {.root_pc = 9, .quality_ovr = -1, .bars = 1}}}},
    // 7 swing -- V-I, smart sevenths (S3.4's own "disco/swing: V->I").
    {.steps = {{{.root_pc = 7, .quality_ovr = -1, .bars = 1},
                {.root_pc = 0, .quality_ovr = -1, .bars = 1}}}},
    // 8 bossa -- V-I, smart sevenths. S3.4 calls bossa out explicitly: its
    // own loop point is vi->ii, NOT cadential, so this authors a dedicated
    // ii-V-I substitute -- collapsed to V-I (2 chords) because Ending's own
    // bar length here is exactly 2 bars, per S3.4's own "(or V-I if length
    // only allows 2 bars)" fallback.
    {.steps = {{{.root_pc = 7, .quality_ovr = -1, .bars = 1},
                {.root_pc = 0, .quality_ovr = -1, .bars = 1}}}},
    // 9 samba -- V-I, smart sevenths (S3.4's own "samba: V->I").
    {.steps = {{{.root_pc = 7, .quality_ovr = -1, .bars = 1},
                {.root_pc = 0, .quality_ovr = -1, .bars = 1}}}},
    // 10 reggae -- IV-I, plain triads (S3.4's own "reggae: IV->I").
    {.steps = {{{.root_pc = 5, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1},
                {.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1}}}},
    // 11 country -- V-I, plain triads (S3.4's own "country: V->I").
    {.steps = {{{.root_pc = 7, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1},
                {.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1}}}},
    // 12 blues -- V7-I7, all dominant 7ths (S3.4's own "blues/shuffle: V->I";
    // I7 on the tonic matches blues's own all-dominant idiom).
    {.steps = {{{.root_pc = 7, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 1},
                {.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 1}}}},
    // 13 shuffle -- V7-I7, all dominant 7ths (S3.4's own "blues/shuffle: V->I").
    {.steps = {{{.root_pc = 7, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 1},
                {.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kDom7), .bars = 1}}}},
    // 14 latin -- IV-I, plain triads (S3.4's own "latin: IV->I").
    {.steps = {{{.root_pc = 5, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1},
                {.root_pc = 0, .quality_ovr = static_cast<std::int8_t>(arrangrr::ChordQuality::kMaj), .bars = 1}}}},
    // 15 motown -- V-I, smart sevenths (S3.4's own "motown: V->I").
    {.steps = {{{.root_pc = 7, .quality_ovr = -1, .bars = 1},
                {.root_pc = 0, .quality_ovr = -1, .bars = 1}}}},
}};
// clang-format on

// Bounds-checked lookup, same fallback discipline as default_progression_for
// (falls back to entry 0, basic's own V-I).
inline constexpr const CadenceProgression& cadence_progression_for(std::int32_t style_index) {
  if (style_index >= 0 && static_cast<std::size_t>(style_index) < kCadenceProgressions.size()) {
    return kCadenceProgressions[static_cast<std::size_t>(style_index)];
  }
  return kCadenceProgressions[0];
}

}  // namespace sonotron
