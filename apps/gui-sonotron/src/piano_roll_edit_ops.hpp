#pragma once

#include <cstddef>
#include <cstdint>

#include "step_pattern_model.hpp"

// Phase-2 piano-roll editing operations (docs/proposals/seqedit-piano-roll-
// phase2-design.md §3/§7): the pure step-array algebra + full-restate wire
// composition every gesture in seqedit_panel.cpp's interactive canvas
// (render_piano_roll_canvas) ends in. Deliberately UI-free (no ImGui type
// named here) so the trickiest piece -- tie-chain resize correctness -- is
// unit-testable directly against StepPatternModel, without a headless ImGui
// context (see tests/test_piano_roll_edit_ops.cpp). Lives in gui_sonotron_
// models (not gui_sonotron_layout) for exactly that reason: it depends on
// nothing but StepPatternModel + BrainSession, and BrainSession is a
// header-only pure-virtual interface (brain_session.hpp) -- calling
// session->send() through a pointer needs no extra link dependency, the
// same discipline seqedit_panel.cpp's own render_step_grid already relies
// on (gui_sonotron_layout never links gui_sonotron_brain either).

namespace sonotron {

class BrainSession;

// The full-restate gotcha (design doc §3, load-bearing): every `track step`
// wire command restates the WHOLE step -- both parsers (shell_music_
// commands.cpp, in_process_brain_session.cpp) default every omitted
// probability/ratchet/micro/tie token to neutral when the caller omits it.
// Every write path below composes and sends ALL SEVEN fields explicitly,
// every time (never conditionally), so a re-pitch/move/resize/velocity edit
// can never silently strip an existing lock. `session` may be nullptr (no
// BrainSession wired, e.g. a bare unit test or a SeqEditModel constructed
// without one) -- the local StepPatternModel echo still updates; nothing is
// sent, matching render_step_grid's own click-handler discipline. Returns
// false (no write at all, local or wire) when StepPatternModel::set_step
// itself rejects the step (invalid note/vel, or an audible step with
// gate==0).
bool write_step_full_restate(StepPatternModel& track, BrainSession* session, int track_idx,
                             std::size_t step_index, const StepPatternStep& step);

// Clears one step both locally and over the wire (`track step <idx> <n>
// clear`), same nullptr-session discipline as write_step_full_restate.
void clear_step_full_restate(StepPatternModel& track, BrainSession* session, int track_idx,
                             std::size_t step_index);

// --- Add/delete/move/re-pitch (design doc §3's simple gestures) ---

// Click-to-add: writes a NEW note at `step_index` (caller-supplied vel/gate,
// neutral locks), full-restate. Returns false if StepPatternModel::set_step
// rejects it (out-of-range note/vel/step_index, or gate==0 while audible).
bool add_note(StepPatternModel& track, BrainSession* session, int track_idx, std::size_t step_index,
              std::uint8_t note, std::uint8_t vel, std::uint16_t gate);

// Click/right-click-to-delete: clears `step_index`.
void delete_note(StepPatternModel& track, BrainSession* session, int track_idx,
                 std::size_t step_index);

// Vertical drag, same step: re-writes `step_index`'s note, preserving every
// other field (vel/gate/probability/ratchet/micro/tie) read from the step's
// CURRENT content before the write -- the exact full-restate discipline the
// design doc's gotcha demands. Returns false if the new note is rejected
// (out of [0,127]).
bool repitch_note(StepPatternModel& track, BrainSession* session, int track_idx,
                  std::size_t step_index, std::uint8_t new_note);

// Drag-to-a-different-step: clears `from_index`, then writes the SAME full
// content (note/vel/gate/locks) at `to_index`, full-restate, old-then-new in
// that order (design doc §3's own recommendation for the single-threaded,
// same-frame send order). If `to_index` already holds a note, it is
// OVERWRITTEN (matches the step-grid's own toggle semantics) -- the caller
// decides overwrite-vs-reject before calling; this always overwrites. A
// no-op (returns false, touches nothing) when `from_index == to_index` or
// `from_index` is already empty (vel == 0).
bool move_note(StepPatternModel& track, BrainSession* session, int track_idx,
               std::size_t from_index, std::size_t to_index);

// --- Velocity edit (design doc §3 "if cheap") ---

// Hover+scroll-wheel (or Alt+drag) velocity nudge: adjusts `step_index`'s
// vel by `delta` (positive or negative), clamped into [1,127] -- an already-
// silent step (vel == 0) is never touched (delta is a no-op there; there is
// no note to nudge), full-restate, preserving every other field. Returns
// false when `step_index` was already empty or out of range.
bool apply_velocity_delta(StepPatternModel& track, BrainSession* session, int track_idx,
                          std::size_t step_index, int delta);

// --- Resize / tie-chain (design doc §3's "Resize", the trickiest gesture) ---

// Finds the run-start index of the maximal same-note tied run containing
// `step_index` -- mirrors arrangrr::Timeline::suppressed_by_tie's own
// backward walk exactly (components/core/arrangrr/include/arrangrr/
// timeline/timeline.hpp), host-side, since gui-sonotron never includes the
// core (D38). A plain (non-tied) note is its own run start.
std::size_t tied_run_start(const StepPatternModel& track, std::size_t step_index);

// Finds a run's LAST index given its start -- mirrors arrangrr::Timeline::
// emit_step's own forward-scan exactly. `length` bounds the walk (never
// wraps past the track's own active length, matching the core's own loop
// seam -- a tie on the last pattern step never carries across the loop
// restart).
std::size_t tied_run_last(const StepPatternModel& track, std::size_t run_start, std::size_t length);

// Rewrites the tied run starting at `anchor` into a NEW shape ending at
// `new_last` (inclusive), releasing with `release_gate` ticks -- the exact
// run shape arrangrr::Timeline::emit_step/suppressed_by_tie expect (see
// this file's own .cpp for the full derivation). `old_last` is the run's
// CURRENT last index (from tied_run_last) before the resize: any step in
// `(new_last, old_last]` -- part of the OLD run being shrunk away -- is
// cleared back to empty, never left with an orphaned tie=true (design
// doc's own "no orphaned tie flags on discarded steps" rule). `anchor ==
// old_last == new_last` degrades to a single-step gate change (the design
// doc's simple "just grow gate" case) -- the SAME function handles both,
// since they differ only in how many steps the run spans.
//
// Preserves the ANCHOR's own probability/ratchet/micro: arrangrr::Timeline::
// on_tick consults ONLY the run-START step's own probability/ratchet/micro
// (timeline.hpp's on_tick/emit_step) for the whole run, so the anchor's
// existing lock must never be silently reset by a resize -- every OTHER
// touched step's own lock fields are functionally inert once tied (only
// note/vel/tie/gate matter for scheduling) and are written neutral.
//
// `note`/`vel` are the run's sounding note/velocity (normally read from the
// anchor's own pre-resize content by the caller).
void resize_note_run(StepPatternModel& track, BrainSession* session, int track_idx,
                     std::size_t anchor, std::size_t old_last, std::size_t new_last,
                     std::uint8_t note, std::uint8_t vel, std::uint16_t release_gate);

}  // namespace sonotron
