#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Real-content cell preview (repeat-zone-real-contract.md, "cell preview
// made real" pass, owner-approved full-fidelity). Resolves a role's
// StylePattern in a given style/section against a canonical PLACEHOLDER
// harmony (a C-major key + tonic triad -- there is no live chord while the
// transport is stopped and the user is just browsing the grid) through the
// arranger's OWN NTT kernel (arrangrr::Arranger::resolve, hoisted PUBLIC for
// exactly this reuse), so the launch-cell mini-preview and the Sequence Edit
// canvas both draw the SAME real note data the engine would actually play,
// never a re-implementation and never a decorative label-hash.
//
// HOST-ONLY, but MAY include arrangrr/ headers (Corelli §15 review
// resolution item 1's SECOND permitted library, alongside gui_sonotron_
// engine -- see apps/gui-sonotron/CMakeLists.txt's gui_sonotron_preview
// target comment). ONLY preview.cpp actually does; THIS header names zero
// arrangrr/core types (the same pimpl-by-convention discipline in_process_
// brain_session.hpp already uses for the exact same reason), so grid_panel.
// cpp/seqedit_panel.cpp can call preview_for() below without ever writing
// "arrangrr" in their own source -- the core-free invariant apps/gui-
// sonotron/CMakeLists.txt pins for gui_sonotron_layout stays intact.

namespace sonotron::preview {

// One bar at the 16th-grid convention every built-in style uses (mirrors
// arrangrr::kMaxMotifLen -- a hand-copied literal, the same "read-only
// literal copy, not an invented number" discipline track_roles.hpp already
// uses for TrackRole; this header must not include arrangrr/ to name the
// real constant). Every built-in style's own sections are one bar (bars=1),
// so this window never truncates real content.
inline constexpr int kSteps = 16;

// Section vocabulary, numerically IDENTICAL to arrangrr::SectionType (same
// mirror discipline as kSteps above) so preview.cpp can `static_cast`
// between them with no translation table. Values not yet reachable from the
// GUI (grid_panel.cpp only ever registers kVarA today) are still spelled
// out for completeness/future-proofing, at zero cost.
enum class Section : std::uint8_t {
  kIntro1 = 0,
  kIntro2 = 1,
  kVarA = 2,
  kVarB = 3,
  kVarC = 4,
  kVarD = 5,
  kFillA = 6,
  kFillB = 7,
  kFillC = 8,
  kFillD = 9,
  kBreak = 10,
  kEnding1 = 11,
  kEnding2 = 12,
};

// One resolved preview pattern: `pitch[step]` is the REAL absolute MIDI
// note number arrangrr::Arranger::resolve() (or, for a motif-driven
// pattern, arrangrr::motif::generate()'s repeat=0 seed resolved the same
// way) produces for that step, or -1 for a rest/silence. Deterministic:
// the same (style_index, section, role_index) always yields the same
// pattern (no seed, no randomness -- it is exactly what the style table
// says, resolved against a fixed placeholder harmony).
struct PreviewPattern {
  std::array<int, kSteps> pitch{};
  // True when this preview is APPROXIMATE, not the exact runtime output:
  // every non-kFixed role resolves against the placeholder harmony above
  // (there is no live chord while stopped) and/or, for a motif-driven
  // pattern, only ever shows the repeat=0 STATEMENT skeleton (a later
  // repeat's call-and-response transform is live `Arranger` state this
  // preview has no access to). kFixed roles (drums/perc) are literal
  // regardless of harmony or repeat -- never approximate.
  bool approx = false;
};

// Resolves `role_index`'s StylePattern in `section` of arrangrr::styles::
// kBuiltins[style_index] against the canonical placeholder harmony,
// through the arranger's own NTT kernel. `role_index` mirrors track_roles.
// hpp's own TrackRole index convention (0=kDrums..8=kLead; the core's
// TrackRole also has an index-9 kCc, which has no grid row and is simply
// out of range for this preview). Returns an all-rest pattern (approx =
// false) for any out-of-range argument, or for an in-range role that
// simply has no content in that section -- an honestly empty preview, not
// an error.
PreviewPattern preview_for(int style_index, Section section, std::size_t role_index);

// The number of BARS `section` holds in `arrangrr::styles::kBuiltins[
// style_index]` (arrangrr::StyleSection::bars, same hand-copied-mirror
// discipline as Section/kSteps above). Used by the Repeat-Zone auto-song
// advance decision (repeat-zone-real-contract.md SLICE 4b) to know when the
// active scene column's own content has played out. Every built-in style's
// sections are 1 bar today (see preview.hpp's own kSteps comment), so 1 is
// also the honest fallback for an out-of-range `style_index` or a `section`
// absent from that style -- never a crash, never a stall (a 0-or-negative
// bar count would make the caller's "elapsed >= length" check trivially and
// permanently true).
int section_bars(int style_index, Section section);

// ---------------------------------------------------------------------------
// Loop-content preview (Phase 7, node 6000, the Looper -- docs/proposals/
// looper-in-gui-contract.md §7 item 8). A recorded loop's content is
// genuinely per-instance RUNTIME state (a live arrangrr::LoopBuffer slot),
// unlike preview_for()'s static built-in Style tables above -- there is no
// core Engine reachable from this core-free header (D38, same discipline
// preview_for's own header comment already documents). `LoopPreviewEvent`
// is therefore a small, hand-copied field-for-field mirror of arrangrr::
// LoopEvent (arrangrr/loop/loop_event.hpp), the same discipline `Section`
// above already uses for arrangrr::SectionType -- a caller that DOES have
// direct LoopBuffer access (e.g. the engine-linking half of the GUI, or a
// future engine-thread query) copies `LoopBuffer::get(slot_id)`'s LoopClip
// events into this plain array; preview_for_loop() never touches
// arrangrr::LoopBuffer itself, so this header still names zero arrangrr/
// core types.

// Numerically IDENTICAL to arrangrr::LoopNoteSource (loop_event.hpp), same
// mirror discipline as Section/kSteps above.
enum class LoopNoteSource : std::uint8_t {
  kChordTone = 0,
  kScaleDegree = 1,
  kInterval = 2,
};

// Field-for-field mirror of arrangrr::LoopEvent (loop_event.hpp): `start`/
// `duration` are raw ticks (arrangrr::Tick is a std::uint32_t); `tone`/
// `octave` widen LoopEvent's std::int8_t fields to plain int (no core type
// named here, same discipline as PreviewPattern::pitch's own plain `int`).
struct LoopPreviewEvent {
  std::uint32_t start = 0;
  std::uint32_t duration = 0;
  int tone = 0;
  int octave = 0;
  int velocity = 100;
  LoopNoteSource source = LoopNoteSource::kInterval;
};

// Resolves a captured loop's events against the SAME canonical placeholder
// harmony preview_for() uses above (a C-major key + tonic triad -- there is
// no live chord while the transport is stopped and the user is just
// browsing the grid), through the identical chord/key-relative resolution
// arrangrr::resolve_note performs at real playback time (Engine::fire_loop's
// own LoopBuffer::on_tick call, loop_buffer.hpp:322-391) -- never a
// re-implementation. Always APPROXIMATE (PreviewPattern::approx == true):
// even a kInterval event (which does not strictly need a chord) is still
// resolved against the placeholder harmony, not the loop's live one, exactly
// like every non-kFixed style-section preview above. Only events landing
// inside the first bar (the same one-bar, kSteps-wide window preview_for()
// uses) are shown; an event starting at or past one bar is silently dropped,
// mirroring preview_for()'s own "outside the one-bar preview window" note.
// `events`/`count` follow the same plain-pointer-plus-length shape as
// arrangrr::Span (this header cannot name that core type either) --
// `events == nullptr` or `count == 0` yields an honestly empty, all-rest
// pattern, never a crash.
PreviewPattern preview_for_loop(const LoopPreviewEvent* events, std::size_t count);

}  // namespace sonotron::preview
