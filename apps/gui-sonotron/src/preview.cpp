#include "preview.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

#include "arrangrr/arranger/arranger.hpp"
#include "arrangrr/arranger/motif.hpp"
#include "arrangrr/arranger/style.hpp"
#include "arrangrr/arranger/style_model.hpp"
#include "arrangrr/common/span.hpp"
#include "arrangrr/loop/loop_event.hpp"
#include "chorddet/theory.hpp"
#include "common/time.hpp"
#include "default_style_progressions.hpp"

// D38/Corelli §15 (see preview.hpp's own header comment): this is the ONLY
// translation unit in gui_sonotron_preview, and the only place in the whole
// library that names an arrangrr:: type. preview.hpp itself stays clean.

namespace sonotron::preview {

namespace {

// The core's own TrackRole vocabulary (arrangrr/timeline/timeline.hpp) has
// kDrums..kCc == 10 entries. A hand-copied literal (same discipline as
// track_roles.hpp's own TrackRole mirror) -- this .cpp COULD include
// timeline.hpp directly for the real enum count, but the bound only needs
// to reject a nonsensical `role_index`, so a plain literal keeps this file
// from having one more transitive include to track.
constexpr std::size_t kCoreRoleCount = 10;

// The SAME canonical placeholder harmony preview_for() used before this
// pass (a C-major key + tonic triad) -- kept only as a defensive fallback
// for chord_for_bar() below, for the two cases that should never happen for
// well-formed authored data: an empty progression table entry, or a
// progression root pc that is somehow chromatic to its own declared key.
constexpr arrangrr::Key kFallbackKey{.root_pc = 0, .mode = arrangrr::Mode::kMajor};
constexpr arrangrr::ChordState kFallbackChord{
    .root_pc = 0, .quality = arrangrr::ChordQuality::kMaj, .valid = true};

// Resolves the (key, chord) this style's own real default progression
// (sonotron::default_progression_for, default_style_progressions.hpp) holds
// on absolute bar `bar` (bar 0 = the section's first bar, bar 1 = its
// second, and so on -- the progression LOOPS once it runs out, mirroring
// the engine's own Param::kSeqLoop behavior). Falls back to the canonical
// C-major placeholder only in the two defensive cases noted above; never
// propagates a bad per-bar result into any other bar's own lookup.
std::pair<arrangrr::Key, arrangrr::ChordState> chord_for_bar(int style_index, int bar) {
  const sonotron::DefaultProgression& prog = sonotron::default_progression_for(style_index);

  int total_prog_bars = 0;
  for (std::size_t i = 0; i < prog.step_count; ++i) {
    total_prog_bars += prog.steps[i].bars;
  }
  if (total_prog_bars == 0) {
    return {kFallbackKey, kFallbackChord};  // defensive; entry 0 always has 4 steps
  }

  const int bar_in_cycle = bar % total_prog_bars;
  int cumulative = 0;
  for (std::size_t i = 0; i < prog.step_count; ++i) {
    const int step_bars = prog.steps[i].bars;
    if (bar_in_cycle >= cumulative && bar_in_cycle < cumulative + step_bars) {
      const arrangrr::Key key{.root_pc = prog.key_root_pc, .mode = prog.key_mode};
      const int degree = arrangrr::theory::degree_of(key, prog.steps[i].root_pc);
      if (degree < 0) {
        return {kFallbackKey, kFallbackChord};  // defensive; well-formed data never hits this
      }
      const arrangrr::ChordQuality quality =
          prog.steps[i].quality_ovr >= 0
              ? static_cast<arrangrr::ChordQuality>(prog.steps[i].quality_ovr)
              : arrangrr::theory::smart_quality(prog.key_mode, degree);
      const arrangrr::ChordState chord{
          .root_pc = prog.steps[i].root_pc, .quality = quality, .valid = true};
      return {key, chord};
    }
    cumulative += step_bars;
  }
  return {kFallbackKey, kFallbackChord};  // defensive; the loop above always covers bar_in_cycle
}

// Non-motif StylePattern: a StyleEvent's own `step` already spans absolute
// positions across every authored bar (style_model.hpp), so a single pass
// suffices -- no per-bar sub-loop needed. Only the window bound (widened
// from kSteps to out.bars*kSteps) and the per-event bar lookup for
// chord_for_bar() are new. Extracted out of preview_for() (readability-
// function-cognitive-complexity) -- pure refactor, no behavior change.
void resolve_literal_pattern(int style_index, const arrangrr::StylePattern& pattern,
                             PreviewPattern& out) {
  for (const arrangrr::StyleEvent& ev : pattern.events) {
    if (ev.step >= static_cast<std::uint16_t>(out.bars * kSteps)) {
      continue;  // outside this section's own multi-bar window (defensive)
    }
    const int bar = ev.step / kSteps;
    int note;
    if (pattern.policy == arrangrr::RolePolicy::kFixed) {
      note = ev.tone;
    } else {
      const auto [key, chord] = chord_for_bar(style_index, bar);
      note = arrangrr::Arranger::resolve(pattern, ev, key, chord, /*transpose=*/0);
    }
    if (note < 0 || note > 127) {
      continue;
    }
    // Owner bug #13 fix: pack this StyleEvent into the first FREE voice slot
    // at its own step, rather than overwriting `out.pitch[ev.step]` outright
    // -- a drum kit's kick/snare colliding with the hat bed on the same 16th
    // (e.g. basic.hpp's kVarADrums) used to lose every voice but the last
    // one resolved. A step with more simultaneous voices than
    // kMaxVoicesPerStep (no built-in style reaches this) defensively drops
    // the extra event rather than overflowing.
    for (int& slot : out.pitch[ev.step]) {
      if (slot < 0) {
        slot = note;
        break;
      }
    }
  }
}

// Motif-driven StylePattern: the seed is built ONCE (it does not change per
// bar -- only apply_repeat()'s transform does). See PreviewPattern::approx's
// own doc comment for why `repeat = bar` here is a deliberate, owner-approved
// APPROXIMATION (a real Arranger increments its repeat counter once per full
// section repeat, not once per bar within one pass) rather than a literal
// reproduction of one single real playback pass. Extracted out of
// preview_for() (readability-function-cognitive-complexity) -- pure
// refactor, no behavior change.
void resolve_motif_pattern(int style_index, const arrangrr::StyleSection& sec,
                           const arrangrr::StylePattern& pattern, PreviewPattern& out) {
  const arrangrr::Motif seed =
      pattern.events.empty()
          ? arrangrr::motif::generate(
                pattern.motif->seed, pattern.motif->length,
                arrangrr::motif::idiom_onset_mask(sec.patterns, pattern.motif->idiom_role),
                pattern.motif->center_degree, pattern.motif->vel, pattern.motif->gate)
          : arrangrr::motif::from_span(pattern.events);

  for (int bar = 0; bar < out.bars; ++bar) {
    const arrangrr::Motif generated =
        arrangrr::motif::apply_repeat(seed, *pattern.motif, static_cast<std::uint32_t>(bar));
    const auto [key, chord] = chord_for_bar(style_index, bar);
    for (std::uint8_t i = 0; i < generated.count; ++i) {
      const arrangrr::StyleEvent& ev = generated.events[i];
      if (ev.step >= static_cast<std::uint16_t>(kSteps)) {
        continue;  // defensive; motif events are always local by construction
      }
      const std::size_t absolute_step =
          static_cast<std::size_t>(bar) * static_cast<std::size_t>(kSteps) + ev.step;
      const int note = pattern.policy == arrangrr::RolePolicy::kFixed
                           ? ev.tone
                           : arrangrr::Arranger::resolve(pattern, ev, key, chord, /*transpose=*/0);
      if (note < 0 || note > 127) {
        continue;
      }
      // Same voice-slot packing as resolve_literal_pattern above (owner bug #13).
      for (int& slot : out.pitch[absolute_step]) {
        if (slot < 0) {
          slot = note;
          break;
        }
      }
    }
  }
}

}  // namespace

PreviewPattern preview_for(int style_index, Section section, std::size_t role_index) {
  PreviewPattern out{};
  for (auto& slots : out.pitch) {
    slots.fill(-1);
  }

  if (style_index < 0 || style_index >= static_cast<int>(arrangrr::styles::kBuiltinCount) ||
      role_index >= kCoreRoleCount) {
    return out;
  }

  const arrangrr::Style* style = arrangrr::styles::kBuiltins[static_cast<std::size_t>(style_index)];
  const auto core_section = static_cast<arrangrr::SectionType>(section);
  const arrangrr::StyleSection* sec = style->find(core_section);
  if (sec == nullptr) {
    return out;
  }

  // Set the multi-bar window width EVEN if this role turns out to have no
  // content below -- an honestly-empty preview still needs the correct
  // `bars` metadata (a caller iterating the full kMaxSteps width has to gate
  // on it either way).
  out.bars = std::clamp(static_cast<int>(sec->bars), 1, kMaxBars);

  const auto role = static_cast<arrangrr::TrackRole>(static_cast<std::uint8_t>(role_index));
  const arrangrr::StylePattern* pattern = nullptr;
  for (const arrangrr::StylePattern& p : sec->patterns) {
    if (p.role == role) {
      pattern = &p;
      break;
    }
  }
  if (pattern == nullptr) {
    return out;  // this role has no content in this section -- an honestly empty preview
  }

  // Every non-kFixed role resolves against this style's own real default
  // progression (chord_for_bar() above), never a live chord -- APPROXIMATE
  // by construction, whether or not a motif is also involved. kFixed roles
  // (drums/perc) are literal regardless of harmony -- never approximate.
  out.approx = pattern->policy != arrangrr::RolePolicy::kFixed;

  if (pattern->motif == nullptr) {
    resolve_literal_pattern(style_index, *pattern, out);
  } else {
    resolve_motif_pattern(style_index, *sec, *pattern, out);
  }
  return out;
}

int section_bars(int style_index, Section section) {
  if (style_index < 0 || style_index >= static_cast<int>(arrangrr::styles::kBuiltinCount)) {
    return 1;
  }
  const arrangrr::Style* style = arrangrr::styles::kBuiltins[static_cast<std::size_t>(style_index)];
  const auto core_section = static_cast<arrangrr::SectionType>(section);
  const arrangrr::StyleSection* sec = style->find(core_section);
  return sec == nullptr ? 1 : static_cast<int>(sec->bars);
}

PreviewPattern preview_for_track(const StepPatternModel& track) {
  PreviewPattern out{};
  for (auto& slots : out.pitch) {
    slots.fill(-1);
  }
  out.approx = false;  // authored directly, never resolved against a placeholder

  const int steps =
      std::clamp(static_cast<int>(track.length()), 1, static_cast<int>(kStepPatternMaxSteps));
  out.bars = std::clamp((steps + kSteps - 1) / kSteps, 1, kMaxBars);
  const int window = std::min(steps, kMaxSteps);
  for (int i = 0; i < window; ++i) {
    const StepPatternStep& s = track.step(static_cast<std::size_t>(i));
    if (s.vel == 0) {
      continue;  // empty slot -- no event
    }
    out.pitch[static_cast<std::size_t>(i)][0] = s.note;
  }
  return out;
}

PreviewPattern preview_for_loop(const LoopPreviewEvent* events, std::size_t count) {
  PreviewPattern out{};
  for (auto& slots : out.pitch) {
    slots.fill(-1);
  }
  // Always approximate: even a kInterval event is resolved against the
  // placeholder harmony below, never the loop's own live one (see this
  // function's header comment).
  out.approx = true;

  if (events == nullptr || count == 0) {
    return out;
  }

  // Canonical placeholder harmony -- a loop has no style/progression of its
  // own to walk the way preview_for() now does for a style section, so this
  // function keeps the fixed C-major/tonic-triad placeholder for the same
  // reason it always did (no live chord while browsing the grid).
  const arrangrr::Key key{.root_pc = 0, .mode = arrangrr::Mode::kMajor};
  const arrangrr::ChordState chord{
      .root_pc = 0, .quality = arrangrr::ChordQuality::kMaj, .valid = true};

  constexpr arrangrr::Tick kTicksPerStep =
      arrangrr::kTicksPerBar / static_cast<arrangrr::Tick>(kSteps);
  static_assert(kTicksPerStep * kSteps == arrangrr::kTicksPerBar,
                "kSteps must divide kTicksPerBar evenly");

  for (std::size_t i = 0; i < count; ++i) {
    const LoopPreviewEvent& pev = events[i];
    if (pev.start >= arrangrr::kTicksPerBar) {
      continue;  // outside the one-bar preview window (mirrors preview_for's own guard)
    }
    arrangrr::LoopEvent ev;
    ev.start = static_cast<arrangrr::Tick>(pev.start);
    ev.duration = static_cast<arrangrr::Tick>(pev.duration);
    ev.tone = static_cast<std::int8_t>(pev.tone);
    ev.octave = static_cast<std::int8_t>(pev.octave);
    ev.velocity = static_cast<std::uint8_t>(pev.velocity);
    ev.source = static_cast<arrangrr::LoopNoteSource>(pev.source);

    const int note = arrangrr::resolve_note(ev, chord, key);
    if (note < 0 || note > 127) {
      continue;
    }
    const std::size_t step = static_cast<std::size_t>(pev.start / kTicksPerStep);
    if (step >= static_cast<std::size_t>(kSteps)) {
      continue;
    }
    // Same voice-slot packing preview_for() uses above (owner bug #13):
    // multiple recorded loop events landing on the same step each keep
    // their own slot instead of the last one overwriting the others.
    for (int& slot : out.pitch[step]) {
      if (slot < 0) {
        slot = note;
        break;
      }
    }
  }
  return out;
}

}  // namespace sonotron::preview
