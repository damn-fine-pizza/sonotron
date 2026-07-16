#include "preview.hpp"

#include <cstdint>

#include "arrangrr/arranger/arranger.hpp"
#include "arrangrr/arranger/motif.hpp"
#include "arrangrr/arranger/style.hpp"
#include "arrangrr/arranger/style_model.hpp"
#include "arrangrr/common/span.hpp"
#include "arrangrr/loop/loop_event.hpp"
#include "chorddet/theory.hpp"
#include "common/time.hpp"

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

}  // namespace

PreviewPattern preview_for(int style_index, Section section, std::size_t role_index) {
  PreviewPattern out{};
  out.pitch.fill(-1);

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

  // Canonical placeholder harmony (STEP 2 of the workstream contract): a
  // fixed C-major key + tonic triad, constructed HERE -- the transport is
  // stopped while browsing the grid, so there is no live Key/ChordState to
  // borrow; the GUI never has to fabricate one of its own.
  const arrangrr::Key key{.root_pc = 0, .mode = arrangrr::Mode::kMajor};
  const arrangrr::ChordState chord{
      .root_pc = 0, .quality = arrangrr::ChordQuality::kMaj, .valid = true};

  // Every non-kFixed role resolves against the placeholder harmony above,
  // never the exact live chord -- APPROXIMATE by construction, whether or
  // not a motif is also involved. kFixed roles (drums/perc) are literal
  // regardless of harmony -- never approximate.
  out.approx = pattern->policy != arrangrr::RolePolicy::kFixed;

  arrangrr::Motif generated;
  arrangrr::Span<const arrangrr::StyleEvent> source = pattern->events;
  if (pattern->motif != nullptr) {
    const arrangrr::Motif seed =
        pattern->events.empty()
            ? arrangrr::motif::generate(
                  pattern->motif->seed, pattern->motif->length,
                  arrangrr::motif::idiom_onset_mask(sec->patterns, pattern->motif->idiom_role),
                  pattern->motif->center_degree, pattern->motif->vel, pattern->motif->gate)
            : arrangrr::motif::from_span(pattern->events);
    // repeat=0: the STATEMENT bar only. apply_repeat(seed, spec, 0) is the
    // seed verbatim (even repeats never transform) -- exactly what the
    // FIRST playback of this section sounds like. A later runtime repeat's
    // call-and-response transform is live `Arranger::m_motif_repeat` state
    // this preview has no access to (STEP 4: flagged via `out.approx`,
    // already true above since every motif-bearing role here is non-kFixed).
    generated = arrangrr::motif::apply_repeat(seed, *pattern->motif, /*repeat=*/0);
    source = arrangrr::Span<const arrangrr::StyleEvent>(generated.events, generated.count);
  }

  for (const arrangrr::StyleEvent& ev : source) {
    if (ev.step >= static_cast<std::uint16_t>(kSteps)) {
      continue;  // outside the one-bar preview window (defensive; no built-in style reaches it)
    }
    const int note = pattern->policy == arrangrr::RolePolicy::kFixed
                         ? ev.tone
                         : arrangrr::Arranger::resolve(*pattern, ev, key, chord, /*transpose=*/0);
    if (note < 0 || note > 127) {
      continue;
    }
    out.pitch[ev.step] = note;
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

PreviewPattern preview_for_loop(const LoopPreviewEvent* events, std::size_t count) {
  PreviewPattern out{};
  out.pitch.fill(-1);
  // Always approximate: even a kInterval event is resolved against the
  // placeholder harmony below, never the loop's own live one (see this
  // function's header comment).
  out.approx = true;

  if (events == nullptr || count == 0) {
    return out;
  }

  // Canonical placeholder harmony -- the SAME one preview_for() constructs
  // above, for the SAME reason (no live chord while browsing the grid).
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
    out.pitch[step] = note;
  }
  return out;
}

}  // namespace sonotron::preview
