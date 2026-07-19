#include "seqedit_panel.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

#include "brain_session.hpp"
#include "grid_model.hpp"
#include "imgui.h"
#include "launch_rows.hpp"
#include "neon_widgets.hpp"
#include "piano_roll_edit_ops.hpp"
#include "piano_roll_gutter.hpp"
#include "preview.hpp"
#include "step_pattern_model.hpp"
#include "theme.hpp"
#include "track_roles.hpp"

namespace sonotron {

namespace {

// A small mode tab: cyan fill when active.
bool mode_tab(const char* label, bool active) {
  ImGui::PushStyleColor(
      ImGuiCol_Button,
      active ? ImVec4(theme::kCyan.x, theme::kCyan.y, theme::kCyan.z, 0.22F) : theme::kFrameBg);
  ImGui::PushStyleColor(ImGuiCol_Text, active ? theme::kCyan : theme::kTextSecondary);
  const bool clicked = ImGui::SmallButton(label);
  ImGui::PopStyleColor(2);
  return clicked;
}

// Piano-roll lane height in px (owner bug: "Sequence Edit still doesn't
// show all tracks" -- draw_role_pattern used to draw every visible role
// into the SAME shared rect on the SAME pitch axis, so all roles mutually
// occluded each other in one lane). Each of the 7 kRows roles always gets
// its own horizontal strip this tall; the canvas grows past the visible
// band and scrolls (see "seq_canvas"'s dropped NoScrollbar/NoScrollWithMouse
// flags below) rather than being squeezed to fit every lane on screen at
// once. Tall enough to fit a name line + a checkbox stacked in the left
// column (kLaneLabelW below) alongside the note-bar content on the right.
constexpr float kLaneH = 76.0F;

// Width in px of each lane's LEFT column: the role name label plus, right
// under it, the bars-visible checkbox (owner correction, 2026-07-18, of
// 026e3de's wrong per-lane "x" HIDE button -- see draw_piano_roll_lanes'
// own header comment below). Note-bar content starts at this x-offset from
// the lane's own left edge, clear of the label column. Sized for the
// longest rendered lane label ("Chord1"/"Chord2", kTrackRoleLabels) plus
// the small checkbox frame under it -- unchanged by the mixer-roles fix
// (2026-07-18), which added chord2 but did not lengthen the longest label.
constexpr float kLaneLabelW = 96.0F;

// Draws one role's note pattern into the seq_canvas draw list: every voice
// at every step as a filled, rounded rect via the shared pitch_grid_cell()
// layout helper, optionally glowed. Shared by the opened role's own draw
// (glow honored, per `fx.glow`) and every other overlaid role's draw (glow
// always off, reduced alpha) so the per-step/per-voice loop is written
// exactly once instead of twice.
void draw_role_pattern(ImDrawList* dl, const ImVec2& p0, const ImVec2& p1, int total_steps,
                       const neon::ClipPattern& pat, const ImVec4& color, float alpha, bool glow) {
  const int kPitches = neon::ClipPattern::kPitches;
  const int kVoices = neon::ClipPattern::kMaxVoicesPerStep;
  for (int step = 0; step < total_steps; ++step) {
    for (int voice = 0; voice < kVoices; ++voice) {
      const int pitch = pat.pitch[static_cast<std::size_t>(step)][static_cast<std::size_t>(voice)];
      if (pitch < 0) {
        continue;
      }
      const neon::PitchCellRect r =
          neon::pitch_grid_cell(p0, p1, step, total_steps, pitch, kPitches);
      const ImVec2 b0(r.min.x + 2.0F, r.min.y + 2.0F);
      const ImVec2 b1(r.max.x - 2.0F, r.max.y - 2.0F);
      if (glow) {
        neon::glow_rect(dl, b0, b1, color, 3.0F, 0.7F, glow);
      }
      dl->AddRectFilled(b0, b1, neon::u32(color, alpha), 3.0F);
    }
  }
}

// Width in px of the piano-key gutter drawn along the left edge of the
// editable piano-roll canvas (piano_roll_gutter.hpp), task #11 Phase 2
// (docs/proposals/seqedit-piano-roll-phase2-design.md §6). Thin by design --
// this is a "which row is which pitch" affordance, not a full keyboard
// widget -- so it costs little of the lane's own kLaneLabelW-trimmed width.
constexpr float kPianoKeyGutterW = 14.0F;

// Task #11 Phase 2 (Sequence Edit real note editing): default velocity/gate
// for a newly-added note. Mirrors render_step_grid's own kDefaultVel/
// kDefaultGate exactly (Phase-1's own click-to-toggle defaults) so a note
// added from either view starts out identical.
constexpr std::uint8_t kPianoRollDefaultVel = 100;
constexpr std::uint16_t kPianoRollDefaultGate = 120;  // half a step

// One drag gesture's in-flight state for the piano-roll canvas
// (render_piano_roll_canvas below). A SINGLE static instance is safe: only
// ONE piano-roll canvas (the open track's own lane) is ever interactive in
// a given frame, mirroring ImGui's own single g.ActiveId discipline -- this
// is NOT SeqEditModel state (that header's own comment requires it stay
// ImGui-free pure-data) and not per-widget ImGui storage (a drag spans
// multiple steps' worth of screen space, outliving any one InvisibleButton
// press-release pair only in the sense that the SAME button stays active
// throughout -- this struct exists to remember what was grabbed and its
// pre-drag content, needed only at commit time on release).
struct PianoRollDrag {
  enum class Mode : std::uint8_t { kNone, kPendingClick, kMove, kResize };
  Mode mode = Mode::kNone;
  std::size_t anchor = 0;      // resize: run-start index; move/click: pressed step
  std::size_t old_last = 0;    // resize only: the run's last index before the drag
  StepPatternStep original{};  // full content at press time (source for locks/note/vel)
  ImVec2 press_pos{};
};

PianoRollDrag& piano_roll_drag_state() {
  static PianoRollDrag state;
  return state;
}

// Fit-to-content-with-margin (design doc §6, owner-recommended, non-
// contested UX default): on first opening a step track in the piano-roll
// view, the pitch window fits [min(observed)-3, max(observed)+3], clamped
// to [0,127] -- or a sane general-purpose default (roughly C2-C7) when the
// track has no authored notes yet, rather than a degenerate single-row
// band. Recomputed only when SeqEditModel::piano_roll_fitted_track() no
// longer matches the currently open track (a DIFFERENT cell was opened, or
// none was) -- so a user's own manual pan/zoom of an already-fitted window
// survives frame to frame while the SAME track stays open.
void ensure_piano_roll_window_fitted(SeqEditModel& model, const StepPatternModel& track) {
  if (model.piano_roll_fitted_track() == model.open_step_track()) {
    return;
  }
  int lo = 127;
  int hi = 0;
  bool any = false;
  const int total =
      std::clamp(static_cast<int>(track.length()), 1, static_cast<int>(kStepPatternMaxSteps));
  for (int i = 0; i < total; ++i) {
    const StepPatternStep& s = track.step(static_cast<std::size_t>(i));
    if (s.vel == 0) {
      continue;
    }
    any = true;
    lo = std::min(lo, static_cast<int>(s.note));
    hi = std::max(hi, static_cast<int>(s.note));
  }
  if (any) {
    model.set_piano_roll_window(std::clamp(lo - 3, 0, 127), std::clamp(hi + 3, 0, 127));
  } else {
    model.set_piano_roll_window(36, 96);
  }
  model.set_piano_roll_fitted_track(model.open_step_track());
}

// Task #11 Phase 2 (Sequence Edit real note editing, roadmap node
// 11600/11610, docs/proposals/seqedit-piano-roll-phase2-design.md): the
// interactive absolute-pitch canvas for the OPEN step track's own lane.
// Unlike every other (read-only) role's draw_role_pattern lane, this reads
// StepPatternModel directly -- bypassing preview::preview_for_track/
// neon::ClipPattern's lossy relative-pitch compression entirely (design
// doc §5/§6) -- and writes it back through piano_roll_edit_ops.hpp's
// full-restate operations: add/delete/move/re-pitch/resize(tie-chain)/
// velocity, per the design doc §3's own gesture->wire mapping table.
//
// Pitch axis: absolute MIDI row = note (0-127), windowed to
// [model.piano_roll_low_note(), piano_roll_high_note()], invertibly 1:1
// screen-Y<->note -- the SAME row formula piano_roll_gutter.hpp's own
// keyboard-gutter draw uses (see that header's comment), so a note row
// always lines up with its gutter key.
//
// Step axis: bounded by StepPatternModel::length() (up to
// kStepPatternMaxSteps == 64) directly, NOT the 2-bar neon::ClipPattern::
// kMaxBars clamp render_step_grid's own kStep view still uses -- task #11
// Phase 2c, design doc §5's own "the clamp is a pre-flagged placeholder,
// Phase-2's own call to remove it for the new canvas" resolution.
//
// Gesture discrimination (an interaction-design call the design doc's own
// §3 explicitly leaves to whoever implements, both for "click vs drag" and
// for the resize/tie-chain trigger distance -- see this function's own
// per-branch comments below):
//  - press on an EMPTY step, release on the SAME step -> Add.
//  - press on an OCCUPIED note's run, release on the SAME (step, note)
//    -> Delete (a plain click with no movement).
//  - press on an OCCUPIED note's run body (not near its right edge),
//    release on the SAME step but a DIFFERENT note -> Re-pitch.
//  - press on an OCCUPIED note's run body, release on a DIFFERENT step
//    -> Move (optionally also re-pitched, if the release row differs too).
//  - press within kResizeEdgeGrabPx of an OCCUPIED run's rendered RIGHT
//    EDGE -> Resize: dragging within the run's own last step's column
//    just grows/shrinks its release gate (design doc's "just grow gate"
//    case); dragging across one or more FURTHER step-column boundaries
//    ties the crossed steps into the run (design doc's "held over N
//    sixteenths" tie-chain case) -- both are the SAME resize_note_run()
//    call, since they differ only in how many steps the target run spans.
//  - dragging FROM an empty step to a different step is left a no-op (the
//    design doc does not specify a drag-to-create gesture).
// Pure screen<->pitch/step conversion for one frame's piano-roll canvas,
// extracted out of render_piano_roll_canvas (readability-function-cognitive-
// complexity) -- the SAME formulas the lambdas used to compute inline, now
// callable from the press/release helper functions below too, without
// duplicating the geometry or re-deriving it from raw ImVec2/int arguments
// at each call site.
struct PianoRollGeometry {
  ImVec2 grid_p0;
  ImVec2 grid_p1;
  int low_note = 0;
  int high_note = 0;
  int total_steps = 1;
  float row_h = 1.0F;
  float col_w = 1.0F;

  float y_top_for_note(int note) const {
    return grid_p1.y - static_cast<float>(note - low_note + 1) * row_h;
  }
  float y_bottom_for_note(int note) const {
    return grid_p1.y - static_cast<float>(note - low_note) * row_h;
  }
  int note_from_y(float y) const {
    const int row = static_cast<int>(std::floor((grid_p1.y - y) / row_h));
    return std::clamp(low_note + row, low_note, high_note);
  }
  float x_for_step(std::size_t step) const { return grid_p0.x + col_w * static_cast<float>(step); }
  int step_from_x(float x) const {
    const int step = static_cast<int>((x - grid_p0.x) / col_w);
    return std::clamp(step, 0, total_steps - 1);
  }
};

PianoRollGeometry compute_piano_roll_geometry(const SeqEditModel& model, const ImVec2& grid_p0,
                                              const ImVec2& grid_p1, int total_steps) {
  PianoRollGeometry g;
  g.grid_p0 = grid_p0;
  g.grid_p1 = grid_p1;
  g.total_steps = total_steps;
  g.low_note = std::clamp(model.piano_roll_low_note(), 0, 127);
  g.high_note = std::clamp(model.piano_roll_high_note(), g.low_note, 127);
  const int rows = g.high_note - g.low_note + 1;
  g.row_h = (grid_p1.y - grid_p0.y) / static_cast<float>(rows);
  g.col_w = (grid_p1.x - grid_p0.x) / static_cast<float>(total_steps);
  return g;
}

// Draws every note RUN once (a maximal same-note tied chain draws as ONE
// continuous bar spanning its whole duration, never one rect per absorbed
// step) -- skips a step that is itself absorbed into an earlier run
// (tied_run_start(track, step) != step), it is drawn when the loop reaches
// its own run start. Extracted out of render_piano_roll_canvas
// (readability-function-cognitive-complexity), pure refactor.
void draw_piano_roll_notes(ImDrawList* dl, const StepPatternModel& track,
                           const PianoRollGeometry& g, const ImVec4& color, bool glow) {
  for (int step = 0; step < g.total_steps; ++step) {
    const std::size_t s = static_cast<std::size_t>(step);
    const StepPatternStep& cur = track.step(s);
    if (cur.vel == 0) {
      continue;
    }
    if (tied_run_start(track, s) != s) {
      continue;
    }
    const std::size_t last = tied_run_last(track, s, static_cast<std::size_t>(g.total_steps));
    const std::uint32_t duration_ticks =
        (static_cast<std::uint32_t>(last - s) * kStepPatternTicksPerStep) + track.step(last).gate;
    const float x0 = g.x_for_step(s);
    const float x1 =
        x0 + (static_cast<float>(duration_ticks) / static_cast<float>(kStepPatternTicksPerStep)) *
                 g.col_w;
    const float yt = g.y_top_for_note(cur.note);
    const float yb = g.y_bottom_for_note(cur.note);
    const ImVec2 b0(x0 + 1.0F, yt + 1.0F);
    const ImVec2 b1(std::max(x1 - 1.0F, x0 + 2.0F), yb - 1.0F);
    if (glow) {
      neon::glow_rect(dl, b0, b1, color, 3.0F, 0.7F, glow);
    }
    // 0.85F matches every other opened-role note-content treatment in this
    // file (draw_role_pattern's own "role == model.part_index()" branch) --
    // the shared house convention test_grid_cell_preview_vs_seqedit_ui_
    // automation.cpp's own note_color constant is keyed on, so the launch-
    // cell mini-preview and this canvas paint the SAME opened track's notes
    // in the SAME color, not merely the same hue at a different opacity.
    dl->AddRectFilled(b0, b1, neon::u32(color, 0.85F), 2.0F);
  }
}

// Press-classification for one piano-roll drag gesture -- extracted out of
// render_piano_roll_canvas (readability-function-cognitive-complexity), pure
// refactor, no behavior change. Sets `drag`'s mode/anchor/original from the
// step under the mouse at press time: an occupied step's own resize-edge
// (within kResizeEdgeGrabPx of the run's current end) starts a Resize, any
// other point on an occupied step starts a Move, and an empty step starts a
// PendingClick (resolved to Add-or-noop on release).
void handle_piano_roll_press(const StepPatternModel& track, const PianoRollGeometry& g,
                             ImVec2 mouse, PianoRollDrag& drag) {
  const std::size_t press_step = static_cast<std::size_t>(g.step_from_x(mouse.x));
  const StepPatternStep cur = track.step(press_step);
  drag.press_pos = mouse;
  if (cur.vel == 0) {
    drag.mode = PianoRollDrag::Mode::kPendingClick;
    drag.anchor = press_step;
    return;
  }
  const std::size_t rs = tied_run_start(track, press_step);
  const std::size_t old_last = tied_run_last(track, rs, static_cast<std::size_t>(g.total_steps));
  const StepPatternStep& start_step = track.step(rs);
  const std::uint32_t duration_ticks =
      (static_cast<std::uint32_t>(old_last - rs) * kStepPatternTicksPerStep) +
      track.step(old_last).gate;
  const float x0 = g.x_for_step(rs);
  const float x1 =
      x0 +
      (static_cast<float>(duration_ticks) / static_cast<float>(kStepPatternTicksPerStep)) * g.col_w;
  constexpr float kResizeEdgeGrabPx = 6.0F;
  if (std::abs(mouse.x - x1) <= kResizeEdgeGrabPx) {
    drag.mode = PianoRollDrag::Mode::kResize;
    drag.anchor = rs;
    drag.old_last = old_last;
    drag.original = start_step;
  } else {
    drag.mode = PianoRollDrag::Mode::kMove;
    drag.anchor = press_step;
    drag.original = cur;
  }
}

// Release commit for PianoRollDrag::Mode::kMove -- extracted out of
// handle_piano_roll_release (readability-function-cognitive-complexity),
// pure refactor, no behavior change. Dispatches to delete/re-pitch/move/
// move-and-re-pitch depending on how the release step/note compare to the
// pressed step/note, per this canvas's own gesture-mapping (design doc §3).
void commit_piano_roll_move(StepPatternModel& track, BrainSession* session, int track_idx,
                            const PianoRollDrag& drag, int release_note, std::size_t release_step) {
  if (release_step == drag.anchor && release_note == drag.original.note) {
    // A plain click on an occupied note, no movement at all: Delete.
    delete_note(track, session, track_idx, drag.anchor);
  } else if (release_step == drag.anchor) {
    repitch_note(track, session, track_idx, drag.anchor, static_cast<std::uint8_t>(release_note));
  } else if (static_cast<std::uint8_t>(release_note) == drag.original.note) {
    move_note(track, session, track_idx, drag.anchor, release_step);
  } else {
    // Moved AND re-pitched in the same drag (design doc's own Move model-op
    // is generic "set_step(newI, p, ...)" -- p may differ).
    StepPatternStep moved = drag.original;
    moved.note = static_cast<std::uint8_t>(release_note);
    clear_step_full_restate(track, session, track_idx, drag.anchor);
    write_step_full_restate(track, session, track_idx, release_step, moved);
  }
}

// Release commit for PianoRollDrag::Mode::kResize -- extracted out of
// handle_piano_roll_release (readability-function-cognitive-complexity),
// pure refactor, no behavior change. Computes the run's new absolute
// duration from the anchor to the current mouse X, clamped so it never
// overruns the pattern's own length or swallows a foreign already-authored
// step beyond the run being resized (design doc's own "don't visually
// overrun the next occupied step" rule, reused here for the tie-chain case).
void commit_piano_roll_resize(StepPatternModel& track, BrainSession* session, int track_idx,
                              const PianoRollGeometry& g, const PianoRollDrag& drag, ImVec2 mouse) {
  const float x0 = g.x_for_step(drag.anchor);
  const float target_ticks_f =
      std::max(1.0F, (mouse.x - x0) / g.col_w) * static_cast<float>(kStepPatternTicksPerStep);
  std::uint32_t target_ticks = static_cast<std::uint32_t>(std::max(1.0F, target_ticks_f));
  std::size_t boundary = static_cast<std::size_t>(g.total_steps) - 1;
  for (std::size_t i = std::max(drag.anchor + 1, drag.old_last + 1);
       i < static_cast<std::size_t>(g.total_steps); ++i) {
    if (track.step(i).vel > 0) {
      boundary = i - 1;
      break;
    }
  }
  const std::uint32_t max_ticks =
      static_cast<std::uint32_t>(boundary - drag.anchor + 1) * kStepPatternTicksPerStep;
  target_ticks = std::min(target_ticks, max_ticks);
  const std::uint32_t steps_full = (target_ticks - 1) / kStepPatternTicksPerStep;
  std::size_t new_last = drag.anchor + steps_full;
  std::uint16_t release_gate =
      static_cast<std::uint16_t>(target_ticks - steps_full * kStepPatternTicksPerStep);
  if (new_last > boundary) {
    new_last = boundary;
    release_gate = static_cast<std::uint16_t>(kStepPatternTicksPerStep);
  }
  resize_note_run(track, session, track_idx, drag.anchor, drag.old_last, new_last,
                  drag.original.note, drag.original.vel, release_gate);
}

// Release-commit dispatch for one piano-roll drag gesture -- extracted out
// of render_piano_roll_canvas (readability-function-cognitive-complexity),
// pure refactor, no behavior change.
void handle_piano_roll_release(StepPatternModel& track, BrainSession* session, int track_idx,
                               const PianoRollGeometry& g, ImVec2 mouse, PianoRollDrag& drag) {
  const int release_note = g.note_from_y(mouse.y);
  const std::size_t release_step = static_cast<std::size_t>(g.step_from_x(mouse.x));
  switch (drag.mode) {
    case PianoRollDrag::Mode::kPendingClick:
      if (release_step == drag.anchor) {
        add_note(track, session, track_idx, release_step, static_cast<std::uint8_t>(release_note),
                 kPianoRollDefaultVel, kPianoRollDefaultGate);
      }
      break;
    case PianoRollDrag::Mode::kMove:
      commit_piano_roll_move(track, session, track_idx, drag, release_note, release_step);
      break;
    case PianoRollDrag::Mode::kResize:
      commit_piano_roll_resize(track, session, track_idx, g, drag, mouse);
      break;
    case PianoRollDrag::Mode::kNone:
      break;
  }
  drag.mode = PianoRollDrag::Mode::kNone;
}

void render_piano_roll_canvas(SeqEditModel& model, StepPatternModel& track, int track_idx,
                              ImDrawList* dl, const ImVec2& lane_p0, const ImVec2& lane_p1,
                              const ImVec4& color, bool glow) {
  BrainSession* session = model.brain_session();
  const int total_steps =
      std::clamp(static_cast<int>(track.length()), 1, static_cast<int>(kStepPatternMaxSteps));

  const ImVec2 gutter_p0 = lane_p0;
  const ImVec2 gutter_p1(lane_p0.x + kPianoKeyGutterW, lane_p1.y);
  const ImVec2 grid_p0(lane_p0.x + kPianoKeyGutterW, lane_p0.y);
  const ImVec2 grid_p1 = lane_p1;
  const PianoRollGeometry g = compute_piano_roll_geometry(model, grid_p0, grid_p1, total_steps);

  draw_piano_key_gutter(dl, gutter_p0, gutter_p1, g.low_note, g.high_note);
  draw_piano_roll_notes(dl, track, g, color, glow);

  PianoRollDrag& drag = piano_roll_drag_state();
  const ImVec2 mouse = ImGui::GetIO().MousePos;

  ImGui::SetCursorScreenPos(grid_p0);
  ImGui::InvisibleButton("piano_roll_input", ImVec2(grid_p1.x - grid_p0.x, grid_p1.y - grid_p0.y));

  // Velocity edit (design doc §3 "if cheap"): Ctrl+scroll-wheel while
  // hovering nudges the hovered step's velocity by a few units per notch.
  // Gated on a held Ctrl (an implementer's interaction-design call, design
  // doc §3 explicitly leaves the exact trigger open) so a plain scroll
  // still scrolls the outer "seq_canvas" child normally -- stealing the
  // wheel unconditionally here would break scrolling to the OTHER lanes
  // whenever the mouse happens to hover the open track's own lane.
  if (ImGui::IsItemHovered() && drag.mode == PianoRollDrag::Mode::kNone && ImGui::GetIO().KeyCtrl) {
    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.0F) {
      constexpr int kVelocityStep = 3;
      const int delta = (wheel > 0.0F ? kVelocityStep : -kVelocityStep);
      const std::size_t hovered_step = static_cast<std::size_t>(g.step_from_x(mouse.x));
      apply_velocity_delta(track, session, track_idx, hovered_step, delta);
    }
  }

  if (ImGui::IsItemActivated()) {
    handle_piano_roll_press(track, g, mouse, drag);
  }

  if (ImGui::IsItemDeactivated()) {
    handle_piano_roll_release(track, session, track_idx, g, mouse, drag);
  }
}

// Lane-partition fix (owner bug: "Sequence Edit still doesn't show all
// tracks" -- overlaying every visible role into ONE shared rect made 9
// roles mutually occlude each other on the same pitch axis). Draws each
// `lane_row_indices[0..lane_count)` launch-grid row into its OWN horizontal
// strip of the canvas (`kLaneH` tall, stacked top to bottom in kRows order),
// a thin divider between lanes, and a small role-label glyph -- extracted
// out of render_seqedit_panel (readability-function-cognitive-complexity),
// pure refactor of task #1's original single-rect overlay loop, same
// color/alpha rules: the opened role (model.part_index()) keeps its full
// track_color/0.85F/glow treatment, every other role stays dimmed and
// unglowed.
//
// Owner decision (sidebar removal, 2026-07-18): the role name used to be
// drawn HERE *and* a second time in the now-deleted left-hand sidebar
// (render_role_toggle_sidebar). The sidebar is gone; each lane's header now
// also carries a checkbox DIRECTLY UNDER its name -- the per-role
// counterpart the sidebar's checkbox used to be, now living on the lane it
// actually names, wired straight to the same SeqEditModel::set_role_visible
// the sidebar called. `model` is therefore taken by non-const reference (it
// was const before, since the sidebar owned every mutation).
//
// Owner correction (2026-07-18) of an earlier wrong attempt (026e3de): that
// commit put a per-lane "x" SmallButton on the RIGHT of each lane, which
// hid the WHOLE LANE -- name included -- with a "+ track" popup as the only
// way back ("disappears forever"). The name label below is now UNCONDITIONAL
// for every one of the 7 kRows lanes, every frame -- see compute_piano_roll_
// lanes' own header comment, it no longer filters by role_visible() at all.
// The checkbox toggles ONLY whether THIS lane's BARS are drawn (below), in
// place, reversibly -- SeqEditModel::role_visible's own header comment
// documents this semantics change (was "lane exists", now "bars draw").
//
// Aretino review (2026-07-18, track-set + content divergence from the
// Repeat Zone): each lane's own `total_steps` and note content are now
// resolved from `grid`'s REAL cell content at (row.role_index, `scene`) via
// launch_rows.hpp's shared resolve_track_cell_preview() -- the SAME resolver
// grid_panel.cpp's own launch-cell mini-preview calls -- instead of a static
// style-table lookup keyed only by role. A step-track cell's lane therefore
// carries its OWN bar count (a step track's real length), not the single
// bar count of whatever role happens to be OPEN; an empty cell's lane is
// blank, matching the Repeat Zone's own blank "+"-cell mini-preview.
void draw_piano_roll_lanes(ImDrawList* dl, const ImVec2& p0, const ImVec2& p1,
                           const ImVec4& track_color, const UiState& fx, SeqEditModel& model,
                           const GridModel& grid, std::size_t scene,
                           const std::array<std::size_t, kRows.size()>& lane_row_indices,
                           std::size_t lane_count) {
  for (std::size_t i = 0; i < lane_count; ++i) {
    const GridRow& row = kRows[lane_row_indices[i]];
    const std::size_t role = row.role_index;
    const GridCell& cell = grid.cell(role, scene);
    const bool filled = cell.kind != GridCellKind::kEmpty;
    const TrackCellPreview preview =
        resolve_track_cell_preview(grid, model, row, scene, cell, filled, fx.active_style);
    const int lane_bars = std::clamp(preview.pattern.bars, 1, neon::ClipPattern::kMaxBars);
    const int lane_total_steps = lane_bars * neon::ClipPattern::kSteps;

    const ImVec2 lane_p0(p0.x, p0.y + static_cast<float>(i) * kLaneH);
    const ImVec2 lane_p1(p1.x, lane_p0.y + kLaneH);

    // Task #11 Phase 2 (design doc §6 "coexistence with the per-role lane
    // view"): the OPENED role's own lane, while a real step track is open
    // (model.open_step_track() >= 0), IS the interactive piano-roll canvas
    // -- it reads/writes StepPatternModel directly instead of the read-only
    // `preview`/draw_role_pattern path every other lane still uses.
    const bool is_open_track_lane = (role == model.part_index()) && model.open_step_track() >= 0;

    // Bars: only when this lane's own checkbox (below) is checked. Indented
    // past kLaneLabelW so they never draw under the name/checkbox column.
    // Unchecked leaves the name+checkbox in place and simply skips painting
    // any note content for this lane -- fully reversible, nothing removed.
    if (model.role_visible(role)) {
      const ImVec2 bars_p0(lane_p0.x + kLaneLabelW, lane_p0.y);
      if (is_open_track_lane) {
        StepPatternModel* open_track =
            model.step_tracks().track(static_cast<std::size_t>(model.open_step_track()));
        if (open_track != nullptr) {
          ensure_piano_roll_window_fitted(model, *open_track);
          render_piano_roll_canvas(model, *open_track, model.open_step_track(), dl, bars_p0,
                                   lane_p1, track_color, fx.glow);
        }
      } else if (role == model.part_index()) {
        draw_role_pattern(dl, bars_p0, lane_p1, lane_total_steps, preview.pattern, track_color,
                          0.85F, fx.glow);
      } else {
        draw_role_pattern(dl, bars_p0, lane_p1, lane_total_steps, preview.pattern,
                          theme::kRoleTint[role], 0.45F, false);
      }
    } else if (is_open_track_lane) {
      // Placeholder-when-hidden (owner-locked decision, design doc §6): the
      // opened role's lane is the ONLY way to edit the open step track, so
      // hiding its bars must never silently remove the editing surface --
      // an explicit placeholder replaces the canvas instead of forcing the
      // checkbox back on (the owner's preferred alternative to force-
      // visibility, preserving full hide-ability for every lane uniformly).
      const ImVec2 bars_p0(lane_p0.x + kLaneLabelW, lane_p0.y);
      const char* hint = "role hidden -- toggle lane visibility to edit";
      dl->AddText(ImVec2(bars_p0.x + 4.0F, lane_p0.y + kLaneH * 0.5F - 6.0F),
                  neon::u32(theme::kTextMuted), hint);
    }
    if (i > 0) {
      dl->AddLine(lane_p0, ImVec2(lane_p1.x, lane_p0.y), neon::u32(theme::kTextMuted, 0.18F), 1.0F);
    }
    dl->AddText(ImVec2(lane_p0.x + 4.0F, lane_p0.y + 2.0F),
                neon::u32(theme::kRoleTint[role], role == model.part_index() ? 0.95F : 0.6F),
                std::string(kTrackRoleLabels[role]).c_str());

    // Per-lane BARS-VISIBLE checkbox: a real ImGui widget (not draw-list-only,
    // unlike the rest of this function) so it is actually clickable -- laid
    // out directly UNDER the name label via SetCursorScreenPos, mirroring the
    // InvisibleButton idiom render_step_grid already uses to place widgets
    // over draw-list content inside this same "seq_canvas" child. PushID(role)
    // keeps every lane's checkbox ID distinct across the loop.
    ImGui::PushID(static_cast<int>(role));
    bool visible = model.role_visible(role);
    ImGui::SetCursorScreenPos(
        ImVec2(lane_p0.x + 4.0F, lane_p0.y + 2.0F + ImGui::GetTextLineHeight() + 2.0F));
    if (ImGui::Checkbox("##bars_visible", &visible)) {
      model.set_role_visible(role, visible);
    }
    ImGui::PopID();
  }
}

// Task #11 Phase 1 (Sequence Edit step sequencer, roadmap node 11600/11610):
// the interactive canvas for SeqEditView::kStep, active only when the open
// cell is a real step-track cell (model.open_step_track() >= 0). The visual
// layer reuses draw_role_pattern above (preview_for_track's own
// PreviewPattern fed through the identical neon::clip_pattern_from_pitches
// conversion every other view already uses), so a step track's on/off
// content draws through the SAME routine as the read-only style overlay --
// one shared formula, never a second bespoke renderer. The interactive
// layer is a row of per-step InvisibleButtons laid out on the identical
// column math draw_role_pattern used internally, drawn on top purely for
// hit-testing (they paint nothing themselves). A click toggles the step's
// single note slot: writes the local StepPatternStore echo AND sends the
// matching `track step ...` wire command through the SeqEditModel-held
// BrainSession* (see seqedit_model.hpp's own header comment for why this
// model uniquely holds one) -- this is the one path the anti-no-op contract
// test (test_step_track_end_to_end_contract.cpp) exercises end to end. A
// null BrainSession* (never wired, e.g. a unit test constructing this model
// bare) still updates the local echo but sends nothing -- a defensive
// no-op, never a crash.
void render_step_grid(SeqEditModel& model, StepPatternModel& track, ImDrawList* dl,
                      const ImVec2& p0, const ImVec2& p1, const ImVec4& color) {
  constexpr std::uint8_t kDefaultNote = 60;
  constexpr std::uint8_t kDefaultVel = 100;
  constexpr std::uint16_t kDefaultGate = 120;  // half a step; mirrors the translator's own default

  const preview::PreviewPattern pp = preview::preview_for_track(track);
  const neon::ClipPattern pat = neon::clip_pattern_from_pitches(pp.pitch, pp.bars);
  const int total_steps =
      std::clamp(pat.bars, 1, neon::ClipPattern::kMaxBars) * neon::ClipPattern::kSteps;
  draw_role_pattern(dl, p0, p1, total_steps, pat, color, 0.85F, /*glow=*/false);

  const int track_idx = model.open_step_track();
  BrainSession* session = model.brain_session();
  const float col_w = (p1.x - p0.x) / static_cast<float>(total_steps);
  for (int i = 0; i < total_steps; ++i) {
    ImGui::PushID(i);
    ImGui::SetCursorScreenPos(ImVec2(p0.x + col_w * static_cast<float>(i), p0.y));
    const bool clicked = ImGui::InvisibleButton("step", ImVec2(col_w, p1.y - p0.y));
    if (clicked && track_idx >= 0) {
      const bool was_on = track.step(static_cast<std::size_t>(i)).vel > 0;
      if (was_on) {
        track.clear_step(static_cast<std::size_t>(i));
        if (session != nullptr) {
          session->send("track step " + std::to_string(track_idx) + " " + std::to_string(i + 1) +
                        " clear");
        }
      } else if (track.set_step(static_cast<std::size_t>(i), kDefaultNote, kDefaultVel,
                                kDefaultGate)) {
        if (session != nullptr) {
          session->send("track step " + std::to_string(track_idx) + " " + std::to_string(i + 1) +
                        " " + std::to_string(kDefaultNote) + " " + std::to_string(kDefaultVel) +
                        " " + std::to_string(kDefaultGate));
        }
      }
    }
    ImGui::PopID();
  }
}

// Right-aligned mode tabs, sized to fit BOTH labels fully (a SmallButton is
// text + 2*FramePadding.x wide; reserve exactly that for each so neither
// "piano-roll" nor "step" is clipped, at any DPI/font size) -- extracted out
// of render_seqedit_panel (readability-function-cognitive-complexity), pure
// refactor, no behavior change. Task #11 Phase 1: the "step" tab only makes
// sense for a real step-track cell -- style-section cells have no kStep
// content, so it is hidden entirely otherwise, and the view falls back to
// kPianoRoll if a PRIOR step-track cell had left kStep selected before a
// different (non-step-track) cell was opened.
void render_mode_tabs(SeqEditModel& model) {
  ImGui::SameLine();
  const float pad2 = ImGui::GetStyle().FramePadding.x * 2.0F;
  const float w_pr = ImGui::CalcTextSize("piano-roll").x + pad2;
  const float w_st = ImGui::CalcTextSize("step").x + pad2;
  const float tab_gap = 6.0F;
  const bool has_step_track = model.open_step_track() >= 0;
  const float tabs_w = has_step_track ? (w_pr + tab_gap + w_st) : w_pr;
  ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - tabs_w);
  if (mode_tab("piano-roll", model.view() == SeqEditView::kPianoRoll)) {
    model.set_view(SeqEditView::kPianoRoll);
  }
  if (has_step_track) {
    ImGui::SameLine(0.0F, tab_gap);
    if (mode_tab("step", model.view() == SeqEditView::kStep)) {
      model.set_view(SeqEditView::kStep);
    }
  } else if (model.view() == SeqEditView::kStep) {
    model.set_view(SeqEditView::kPianoRoll);
  }
}

// Task #11 Phase 1: SeqEditView::kStep, with a real step-track cell open,
// branches the canvas into the interactive step grid instead of the
// read-only overlay below -- extracted out of render_seqedit_panel
// (readability-function-cognitive-complexity), pure refactor, no behavior
// change. Returns true when it fully handled (and ended) the canvas child,
// in which case the caller must return immediately without falling through
// to the read-only overlay below. kPianoRoll (and a kStep view left selected
// on a NON-step-track cell, which the mode-tab guard above already falls
// back to kPianoRoll for) keeps today's overlay COMPLETELY UNCHANGED -- that
// stays Phase-2's future editable canvas, per the owner's "entrambi a fasi"
// decision.
bool try_render_step_canvas(SeqEditModel& model, ImDrawList* dl, const ImVec2& p0, const ImVec2& p1,
                            const ImVec2& avail, const UiState& fx, const ImVec4& track_color) {
  if (model.view() != SeqEditView::kStep || model.open_step_track() < 0) {
    return false;
  }
  StepPatternModel* track =
      model.step_tracks().track(static_cast<std::size_t>(model.open_step_track()));
  if (track == nullptr) {
    return false;
  }
  render_step_grid(model, *track, dl, p0, p1, track_color);
  if (fx.playing) {
    const float phase = std::fmod(fx.time, 2.0F) / 2.0F;
    const float x = p0.x + phase * avail.x;
    dl->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y), neon::u32(theme::kGreen), 1.5F);
  }
  ImGui::EndChild();
  return true;
}

// Which of the 7 kRows launch rows get a piano-roll lane this frame --
// extracted out of render_seqedit_panel (readability-function-cognitive-
// complexity), pure refactor. Mirrors try_render_step_canvas's own guard
// (model.view() != kStep || open_step_track() < 0) so the two branches can
// never disagree about which one is about to run this frame: the WAV
// preview and the step-track canvas both keep their single full-height rect
// (an empty `lane_row_indices`/a 0 return degrades the caller's content
// height back to avail.y), only the read-only piano-roll overlay gets
// partitioned. Returns the lane count (0 when this frame isn't the
// piano-roll overlay at all). `lane_row_indices` holds INDICES INTO kRows
// (not raw TrackRole indices) -- draw_piano_roll_lanes needs the whole
// GridRow (role_index AND name) to resolve each lane's real content.
//
// Aretino review (2026-07-18): walks kRows (the Repeat Zone's own real
// rows), not every kTrackRoleCount role -- roles with no Repeat-Zone row at
// all used to render a lane that was a phantom with no corresponding launch
// cell. Mixer-roles fix (2026-07-18): kRows now has 7 rows (Perc/Chord2
// added, Lead removed to match the actually-routed default band) -- only
// Phrase and Lead have no Repeat-Zone row any more.
//
// Owner correction (2026-07-18) of 026e3de: the lane SET returned here is
// now ALWAYS every one of the 7 kRows roles, unconditionally -- role_
// visible() no longer filters which lanes EXIST (that was the wrong "x"
// button's job, now removed), it only gates whether draw_piano_roll_lanes
// paints a given lane's BARS. A lane's name therefore never disappears.
std::size_t compute_piano_roll_lanes(bool open, const UiState& fx, const SeqEditModel& model,
                                     std::array<std::size_t, kRows.size()>& lane_row_indices) {
  // `model` is no longer used to FILTER lanes (see the header comment above)
  // but stays a parameter for symmetry with the piano-roll gating condition
  // below, which reads model.view()/open_step_track() the same as before.
  const bool is_piano_roll =
      open && !fx.open_wav && (model.view() != SeqEditView::kStep || model.open_step_track() < 0);
  if (!is_piano_roll) {
    return 0;
  }
  for (std::size_t i = 0; i < kRows.size(); ++i) {
    lane_row_indices[i] = i;
  }
  return kRows.size();
}

// Finds the launch-grid row (kRows) for a given TrackRole index, or nullptr
// if this role is not one of the 7 real launch rows. Every OPENED cell's
// role always IS one (render_track_cell/grid_panel.cpp only ever opens a
// kRows role into Sequence Edit), so a nullptr here is purely defensive --
// it never happens in practice, but the caller (render_seqedit_panel)
// degrades to a blank/1-bar default rather than crashing if it ever did.
const GridRow* find_launch_row(std::size_t role_index) {
  for (const GridRow& row : kRows) {
    if (row.role_index == role_index) {
      return &row;
    }
  }
  return nullptr;
}

}  // namespace

void render_seqedit_panel(SeqEditModel& model, const UiState& fx, const GridModel& grid) {
  const bool open = fx.open_cell >= 0;
  const ImVec4 track_color =
      (fx.open_row >= 0 && fx.open_row < static_cast<int>(theme::kTrackColor.size()))
          ? theme::kTrackColor[fx.open_row]
          : theme::kCyan;
  const std::size_t scene = static_cast<std::size_t>(std::max(fx.open_scene, 0));

  // Aretino review (2026-07-18): the OPENED role's own content -- used only
  // for the header's "~approx" marker and the canvas-wide bar-guide division
  // count below -- is now resolved through the SAME launch_rows.hpp
  // resolve_track_cell_preview() every lane (draw_piano_roll_lanes) and the
  // Repeat Zone's own mini-preview (grid_panel.cpp's render_track_cell)
  // already share, against `grid`'s REAL cell at (role, `scene`), replacing
  // the previous static preview_for(fx.active_style, section, part_index())
  // lookup -- a step-track cell's own bar count now drives the guides
  // correctly instead of always reading the style table. Every OPENED cell's
  // role is always one of kRows (render_track_cell only ever opens one), so
  // find_launch_row() only returns nullptr defensively; a null falls back to
  // the resolver's own blank/1-bar default.
  const GridRow* opened_row = open ? find_launch_row(model.part_index()) : nullptr;
  TrackCellPreview opened_preview;
  if (opened_row != nullptr) {
    const GridCell& opened_cell = grid.cell(opened_row->role_index, scene);
    const bool opened_filled = opened_cell.kind != GridCellKind::kEmpty;
    opened_preview = resolve_track_cell_preview(grid, model, *opened_row, scene, opened_cell,
                                                opened_filled, fx.active_style);
  }
  int bars = std::clamp(opened_preview.pattern.bars, 1, neon::ClipPattern::kMaxBars);

  // Task #11 Phase 2c (design doc §5): once a real step track is open in
  // the piano-roll view, the bar-guide count must reflect its OWN authored
  // length (up to kStepPatternMaxSteps == 64), not the 2-bar preview clamp
  // above -- otherwise the vertical bar guides would only ever span 2 bars
  // while render_piano_roll_canvas's own editable canvas shows up to 4.
  if (open && model.view() == SeqEditView::kPianoRoll && model.open_step_track() >= 0) {
    const StepPatternModel* open_track =
        model.step_tracks().track(static_cast<std::size_t>(model.open_step_track()));
    if (open_track != nullptr) {
      const int steps = std::clamp(static_cast<int>(open_track->length()), 1,
                                   static_cast<int>(kStepPatternMaxSteps));
      bars = (steps + preview::kSteps - 1) / preview::kSteps;
    }
  }

  // Header.
  ImGui::TextColored(theme::kCyan, "SEQUENCE EDIT");
  ImGui::SameLine(0.0F, 12.0F);
  ImGui::TextColored(theme::kTextMuted, "part");
  ImGui::SameLine(0.0F, 4.0F);
  ImGui::TextColored(track_color, "%s", std::string(model.part_label()).c_str());
  ImGui::SameLine(0.0F, 12.0F);
  ImGui::TextColored(theme::kTextMuted, "clip");
  ImGui::SameLine(0.0F, 4.0F);
  ImGui::TextColored(theme::kText, "%s", model.clip_label().c_str());
  if (open && opened_preview.approx) {
    // Honesty affordance (repeat-zone-real-contract.md STEP 4): a small,
    // muted marker, no new persistent chrome, for a preview that isn't the
    // exact runtime output (resolved against a placeholder harmony, and/or
    // a motif's repeat=0 statement skeleton only).
    ImGui::SameLine(0.0F, 6.0F);
    ImGui::TextColored(theme::kTextMuted, "~approx");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("approx preview (placeholder harmony, no live chord)");
    }
  }
  ImGui::SameLine(0.0F, 12.0F);
  ImGui::TextColored(theme::kTextMuted, "grid 1/%d", model.grid_division());

  // Owner correction (2026-07-18) of 026e3de: there is no "+ track"
  // control any more -- a lane is never removed in the first place (see
  // compute_piano_roll_lanes/draw_piano_roll_lanes' own header comments), so
  // there is nothing to "re-add". Each lane's own checkbox is the entire
  // show/hide affordance now.
  render_mode_tabs(model);
  ImGui::Spacing();

  // Canvas. Owner bug ("Sequence Edit still doesn't show all tracks"): the
  // piano-roll view below now partitions the canvas into one horizontal LANE
  // per visible role instead of overlaying every role into this single
  // child's rect, so the content can outgrow the visible band -- scrollbar
  // and mouse-wheel scroll are both left ENABLED (dropped from the old
  // NoScrollbar | NoScrollWithMouse) so every lane stays reachable. The WAV
  // and step-track branches below still draw a single full-height rect (no
  // overflow, so no scrolling occurs for them either) -- unchanged.
  ImGui::BeginChild("seq_canvas", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_None);
  const ImVec2 p0 = ImGui::GetCursorScreenPos();
  const ImVec2 avail = ImGui::GetContentRegionAvail();
  ImDrawList* dl = ImGui::GetWindowDrawList();

  // Which of the 7 kRows launch rows get a piano-roll lane this frame, and
  // how tall the scrollable content is -- see compute_piano_roll_lanes's own
  // header comment.
  std::array<std::size_t, kRows.size()> lane_row_indices{};
  const std::size_t lane_count = compute_piano_roll_lanes(open, fx, model, lane_row_indices);
  const float content_h =
      lane_count > 0 ? std::max(avail.y, static_cast<float>(lane_count) * kLaneH) : avail.y;
  const ImVec2 p1(p0.x + avail.x, p0.y + content_h);

  dl->AddRectFilled(p0, p1, neon::u32(theme::kInsetBg), 6.0F);

  // Vertical bar guides (owner tasks #2/#3: `bars` groups of 8 divisions
  // each, landing exactly on a bar boundary at a stronger alpha -- mirrors
  // the bar-divider legibility touch neon::clip_preview_pianoroll already
  // carries). Spans the full scrollable content height so the guides read
  // through every lane, not just the first screenful.
  const int total_divisions = bars * 8;
  for (int i = 1; i < total_divisions; ++i) {
    const float x = p0.x + avail.x * static_cast<float>(i) / static_cast<float>(total_divisions);
    const bool on_bar = (i % 8) == 0;
    dl->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y), neon::u32(theme::kCyan, on_bar ? 0.14F : 0.06F),
                1.0F);
  }

  if (!open) {
    const char* hint = "click a clip in the Repeat Zone to open it here";
    const ImVec2 ts = ImGui::CalcTextSize(hint);
    dl->AddText(ImVec2(p0.x + (avail.x - ts.x) * 0.5F, p0.y + (avail.y - ts.y) * 0.5F),
                neon::u32(theme::kTextMuted), hint);
    ImGui::EndChild();
    return;
  }

  // Feature B item 7 (WAV branch): a captured LoopBuffer clip has no
  // piano-roll content to overlay -- show the waveform preview instead.
  // Dormant in production today (nothing creates a kLoopBuffer cell yet,
  // Phase 7 Looper), wired now so the branch is ready the moment one
  // exists. Seeded off fx.open_cell so the same clip always draws the SAME
  // deterministic waveform. p1 == avail-height here (lane_count is always 0
  // for this branch, is_piano_roll_lanes above already excludes fx.open_wav).
  if (fx.open_wav) {
    neon::clip_preview_waveform(dl, p0, p1, static_cast<std::uint32_t>(fx.open_cell), track_color);
    if (fx.playing) {
      const float phase = std::fmod(fx.time, 2.0F) / 2.0F;
      const float x = p0.x + phase * avail.x;
      dl->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y), neon::u32(theme::kGreen), 1.5F);
    }
    ImGui::EndChild();
    return;
  }

  if (try_render_step_canvas(model, dl, p0, p1, avail, fx, track_color)) {
    return;
  }

  // Real content (repeat-zone-real-contract.md STEP 3): every row renders
  // its real resolved note pattern, never a label-hash. The pad row
  // (grid_panel.cpp's kRows[...].audio, `fx.open_audio`) has no real audio
  // content yet -- it IS a real MIDI role, so it renders its real note
  // pattern here too rather than a fake waveform; clip_preview_waveform
  // stays reserved for genuine future audio content (an owner-flagged
  // deviation from the design's audio->waveform mapping, see this
  // workstream's implementation report). STEP left->right, PITCH low->high
  // (pitch 0 at the bottom of its OWN lane), via the SAME neon::
  // pitch_grid_cell() helper the launch-cell mini-preview uses (neon_
  // widgets.cpp's clip_preview_pianoroll) -- one shared formula, so the two
  // views can never silently drift apart. Every simultaneous voice at a step
  // (owner bug #13, e.g. a drum kit's kick+hihat both on beat 1) draws its
  // OWN row block here too.
  //
  // Owner task #1 + lane-partition fix ("Sequence Edit still doesn't show
  // all tracks"): every VISIBLE kRows role gets its OWN horizontal lane
  // (`lane_row_indices`/`kLaneH` above) instead of being overlaid into one
  // shared rect, where 9 roles used to mutually occlude each other on the
  // same pitch axis -- see draw_piano_roll_lanes's own header comment.
  draw_piano_roll_lanes(dl, p0, p1, track_color, fx, model, grid, scene, lane_row_indices,
                        lane_count);

  // Green playhead sweeping L->R while the opened clip plays, across the
  // full scrollable content height so it reads through every lane.
  if (fx.playing) {
    const float phase = std::fmod(fx.time, 2.0F) / 2.0F;
    const float x = p0.x + phase * avail.x;
    dl->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y), neon::u32(theme::kGreen), 1.5F);
  }

  // Registers the lanes' full content height with ImGui so the now-enabled
  // scrollbar/mouse-wheel scroll (this child's BeginChild flags above) can
  // actually reach every lane -- this is a draw-list-only child (no normal
  // widgets advance the cursor), so ImGui would otherwise never learn the
  // content extends past `avail.y`. A zero-size Dummy() (an actual submitted
  // item, not just a cursor move) is required here: ImGui's own
  // ErrorCheckUsingSetCursorPosToExtendParentBoundaries asserts if
  // SetCursorScreenPos alone is used to grow the parent's content bounds.
  if (lane_count > 0) {
    ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + content_h));
    ImGui::Dummy(ImVec2(0.0F, 0.0F));
  }

  ImGui::EndChild();
}

}  // namespace sonotron
