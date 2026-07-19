#pragma once

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>

// Ground-truth ORACLE utility for gui-sonotron's preview-vs-engine fidelity
// bugs (owner task #4: build debug/oracle tooling to red-then-green the
// class of bug where the Sequence-Edit/cell preview -- preview::preview_for,
// preview.cpp -- disagrees with what the engine actually plays).
//
// TWO HALVES, deliberately different in shape:
//
//  (a) A PURE half (real_bar_onset_steps / real_bar_pitch /
//      real_motif_repeat_onset_steps) that calls the EXACT SAME real
//      arrangrr:: kernel functions preview.cpp itself calls, and that
//      Arranger::on_tick's own fire loop calls at real playback time
//      (Style::find, Arranger::resolve, motif::from_span/generate/
//      apply_repeat) -- never a re-implementation, never a guess.
//      Deterministic, no wall clock, no engine thread: this is the honest
//      "what SHOULD a fully-faithful preview show" ground truth for the
//      multi-bar-truncation bug (#1) and the dropped-motif-repeat-transform
//      bug (#2).
//
//  (b) A REAL half (observe_default_harmony) that drives an ACTUAL
//      InProcessBrainSession -- the identical production path main.cpp/
//      browser_panel.cpp use -- through `style load`/`transport start`, and
//      polls the REAL OutEvent stream (via kChordFollowed) for the
//      pitch-class context the engine is genuinely playing against. This is
//      the ground truth for the placeholder-harmony bug (#3): preview_for()
//      always resolves against a hardcoded C-major tonic triad, never this
//      real, per-style, moving progression.
//
// WHY (a) IS A CAPTURE, NOT A SHORTCUT (bugs #1/#2): the owner asked for a
// wire-level diff against the real OutEvent stream for every bug. That is
// exactly what test_default_style_progression_functional.cpp already does
// for chord/harmony data (bug #3, part (b) here) because the wire genuinely
// carries it (BrainEvent::kChordFollowed's pitch-class bitmask). It is NOT
// reachable today for per-role onset/pitch data: brain_event_from_outevent.
// cpp's OutEvent::Kind::kMidi decode keeps only a "noteon"/"noteoff" text
// label plus `port` (which is 0 for EVERY role gui-sonotron routes --
// `style route` always assigns "out0", varying only the MIDI CHANNEL
// embedded in the dropped `msg.status` byte) -- both the note number
// (msg.d1) and the channel/role identity are discarded before a BrainEvent
// is ever built. A real per-role, per-pitch wire capture is therefore not
// buildable from test code today without a product-side change to that
// decode -- FLAGGED in this workstream's report as a wanted test seam for
// Giotto (additive fields only, no behavior change), not something this
// QA pass may add itself (it lives in apps/gui-sonotron/src/, Giotto's
// disjoint lane on this workstream). Part (a) instead calls the identical
// production kernel functions the real engine invokes for the SAME (style,
// section, role, repeat) -- which is a strictly MORE precise and fully
// deterministic ground truth than a lossy, wall-clock-timed wire replay
// would have been anyway, for exactly the two bugs the wire cannot yet see.

namespace sonotron::preview_oracle {

// The set of onset STEPS (each in [0,16), i.e. position WITHIN one bar) a
// role's RAW authored StyleEvent span genuinely contains at absolute bar
// `bar_index` (0-based) of `section_type_value` (an arrangrr::SectionType
// numeric value -- mirrors preview::Section's own convention) in
// arrangrr::styles::kBuiltins[style_index] -- independent of preview_for()'s
// fixed one-bar (kSteps=16) window. Returns an empty set for an
// out-of-range style/section/role, a role with no pattern in this section,
// or a bar index at or past the section's own StyleSection::bars.
std::set<int> real_bar_onset_steps(int style_index, int section_type_value, std::size_t role_index,
                                    int bar_index);

// The resolved absolute MIDI pitch (0..127) the REAL engine would sound for
// the first authored StyleEvent landing at (`bar_index`, `step_in_bar`), or
// -1 if no event lands there (or the arguments are out of range). Resolved
// through the real arrangrr::Arranger::resolve() kernel -- the exact
// function preview_for() itself calls -- against the SAME canonical
// placeholder harmony preview_for() uses (a C-major key + a C-major tonic
// triad chord), so a divergence this function finds cannot be blamed on a
// different harmony assumption; it is purely about REACHING bar 2+ at all.
int real_bar_pitch(int style_index, int section_type_value, std::size_t role_index, int bar_index,
                    int step_in_bar);

// The onset STEP SET (each in [0,16)) a motif-driven role's seed motif
// carries at call-and-response `repeat` (0 = the verbatim statement; ODD
// repeats apply the style's own MotifSpec::transform with a repeat-keyed
// amount -- arrangrr::motif::apply_repeat's own "EVEN verbatim / ODD
// transformed" contract), via the REAL arrangrr::motif::from_span/generate +
// apply_repeat kernel -- the exact functions Arranger::on_tick calls every
// time a plain variation section loops back to itself (m_motif_repeat, the
// live per-section repeat counter). preview_for() only EVER computes
// repeat=0 (see preview.cpp's own header comment); this function is how a
// caller reaches repeat=1+, the ground truth for what a later loop of the
// SAME section really plays. Returns an empty set if the role has no
// pattern in this section, the pattern carries no MotifSpec, or the
// style/section/role is out of range.
std::set<int> real_motif_repeat_onset_steps(int style_index, int section_type_value,
                                             std::size_t role_index, std::uint32_t repeat);

// The resolved absolute MIDI pitch (0..127, or -1) for the FIRST authored
// StyleEvent landing at `step` of `section_type_value`/`role_index`,
// resolved through the REAL arrangrr::Arranger::resolve() kernel against the
// style's own REAL first default-progression chord (sonotron::
// default_progression_for(style_index).steps[0], default_style_progressions.
// hpp -- the SAME host-side data in_process_brain_session.cpp feeds the
// engine via kKeySet/kSeqNew/kSeqAdd/kSeqPlay right after `style load`) --
// NOT the placeholder. Comparing this against preview_for()'s own resolved
// pitch for the identical (style, section, role, step) is a direct,
// deterministic, wall-clock-free pitch-level diff for the placeholder-
// harmony bug (#3): any divergence is caused SOLELY by preview_for()'s
// hardcoded C-major-tonic-triad assumption, since both pitches are produced
// by the identical resolve() call on the identical StyleEvent.
//
// LIMITATION (kept simple on purpose): only handles an EXPLICIT
// ProgressionStep::quality_ovr (>= 0); returns -1 if the style's first step
// uses "smart" quality (quality_ovr == -1, theory::smart_quality) or the
// style/section/role/step lookup fails. Callers should pick one of the many
// built-in styles whose first progression step is an explicit quality (e.g.
// rock, index 2: {root_pc=7, quality_ovr=kMaj}) -- smart-quality support can
// be added later if a RED test genuinely needs a style that lacks one.
int real_first_progression_chord_pitch(int style_index, int section_type_value,
                                        std::size_t role_index, int step);

// ---------------------------------------------------------------------------
// Real-engine capture (bug #3: the placeholder-harmony divergence).

struct HarmonyObservation {
  int chord_event_count = 0;
  // OR of every "sequencer"-sourced BrainEvent::kChordFollowed pitch-class
  // bitmask seen (bit0=C .. bit11=B) -- the REAL harmonic context the style's
  // own default progression (default_style_progressions.hpp) actually plays
  // against, over the observation window.
  std::uint16_t pitch_class_union = 0;
  // The FIRST "sequencer"-sourced kChordFollowed pitch-class bitmask seen
  // (0 if none observed in the window) -- the REAL active chord's pcs at the
  // very start of playback, e.g. rock's I chord (G major, root_pc=7) is
  // {7,11,2} -> mask 0x884, never the placeholder's {0,4,7} -> 0x91. Sharper
  // than the union above for a divergence pin: no progression movement is
  // needed, the FIRST chord alone already diverges for any style whose key
  // root is not C major (e.g. rock, house, bossa).
  std::uint16_t first_pcs = 0;
};

// Drives `style load <style_name>` then `transport start` through a REAL
// InProcessBrainSession (the identical production path browser_panel.cpp
// itself calls -- NEVER a hand-written `seq`/`key` line), then polls for
// `wait_milliseconds` of REAL wall-clock time collecting every
// "sequencer"-sourced kChordFollowed pitch-class bitmask -- the same
// idiom test_default_style_progression_functional.cpp's own
// observe_default_progression() already uses and already proved reliable
// (issue #29), promoted here into a reusable utility so a NEW
// preview-divergence pin does not hand-roll a second copy of this harness.
HarmonyObservation observe_default_harmony(const std::string& style_name, long wait_milliseconds);

}  // namespace sonotron::preview_oracle
