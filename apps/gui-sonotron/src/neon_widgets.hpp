#pragma once

#include <array>
#include <cstdint>

#include "imgui.h"

// Reusable Dear ImGui draw-list widgets for the "neon / hardware-synth"
// workstation redesign (v02-workstation-spec.md §"Widgets NEW"). Everything
// here is pure ImDrawList / immediate-mode input -- no BrainSession, no model
// dependency -- so it lives in gui_sonotron_layout next to theme.cpp and the
// panels, and every panel that needs a knob / XY pad / VU / glow shares one
// implementation instead of re-rolling draw code per zone.
//
// The GLOW SYSTEM: every helper that can glow takes a `glow` bool. When false
// the halo draws are skipped (shadow -> none), honoring the one global
// UiState::glow flag the whole UI passes down. Animations take an explicit
// `time` (ImGui frame clock) and a `playing`/gate flag so the caller decides
// when motion is allowed -- the widgets never read the clock themselves.

namespace sonotron::neon {

// Packs an ImVec4 into ImU32, scaling alpha by `alpha_mul` (clamped) -- the
// idiom the draw-list halo/wash layers use to fade a token color.
ImU32 u32(const ImVec4& color, float alpha_mul = 1.0F);

// The canonical rendered note pattern for one clip: `kSteps` columns, each
// holding up to `kMaxVoicesPerStep` simultaneous pitch ROWS in [0, kPitches)
// or -1 (an empty voice slot / rest) -- e.g. a drum kit's kick and hihat
// both landing on the same step draw as two distinct rows, never collapsed
// into one (owner bug #13). This is the ONE shape shared by the launch-cell
// mini-preview (clip_preview_pianoroll) and the Sequence Edit canvas, so the
// SAME clip reads as the SAME melody in both -- the cell shows the SAME
// kSteps columns the editor draws in full, just denser (narrower columns in
// the same band). STEP runs left->right, PITCH low->high (row 0 at the
// bottom).
//
// Real-content cell preview (repeat-zone-real-contract.md): populate this
// from real resolved note data via clip_pattern_from_pitches() below, fed by
// gui_sonotron_preview's preview_for() -- never from a label hash. This
// header stays dependency-free (no arrangrr, no gui_sonotron_preview): the
// conversion below takes plain absolute MIDI pitches (or -1 for a rest), the
// same shape gui_sonotron_preview::PreviewPattern::pitch already has.
struct ClipPattern {
  static constexpr int kSteps = 16;
  // Mirrors gui_sonotron_preview::kMaxBars (hand-copied literal, same
  // discipline as kSteps itself): the longest built-in section length
  // observed across the style corpus (Wave-1 style-depth's basic::kVarA /
  // kIntro1, 2 bars). Owner bug #2/#3 fix: a cell's preview used to show
  // only bar 1 of a multi-bar section; both the grid mini-preview and the
  // Sequence Edit canvas now walk every bar the section actually has.
  static constexpr int kMaxBars = 2;
  static constexpr int kMaxSteps = kSteps * kMaxBars;
  static constexpr int kPitches = 5;
  // Owner bug #2 fix: clip_preview_pianoroll() below no longer crops to this
  // -- it now visits all `bars * kSteps` columns, exactly like the Sequence
  // Edit canvas, so the two views never structurally diverge. Kept as a
  // named constant only for the UI-automation test harness's own coarse
  // column-occupancy sampling (apps/gui-sonotron/tests/test_grid_cell_
  // preview_vs_seqedit_ui_automation.cpp), not as a product crop anymore.
  static constexpr int kCellSteps = 8;
  // Mirrors gui_sonotron_preview::kMaxVoicesPerStep (hand-copied literal,
  // same discipline as kSteps itself): the most simultaneous voices (e.g. a
  // drum kit's kick+hihat landing on the same step) a single step column
  // carries. Owner bug #13 fix: a step's voices used to collapse into ONE
  // `pitch[step]` int, so a drum kit's kick/snare colliding with its hat bed
  // silently lost every voice but the last one resolved, flattening the
  // whole clip to a single drawn level.
  static constexpr int kMaxVoicesPerStep = 4;
  // pitch[step][voice] in [0,kPitches) or -1 (no voice in this slot / rest).
  // Sized kMaxSteps (every bar of the widest built-in section); only the
  // first `bars * kSteps` columns are real content, mirroring gui_sonotron_
  // preview::PreviewPattern's own (pitch, bars) shape exactly.
  std::array<std::array<int, kMaxVoicesPerStep>, kMaxSteps> pitch{};
  // How many of kMaxBars this pattern actually carries (1 or 2 today). Both
  // renderers below (clip_preview_pianoroll here, and seqedit_panel.cpp's
  // own canvas loop) use this to bound their column count instead of always
  // assuming kMaxSteps, so a 1-bar section still shows a 1-bar-wide preview,
  // not a half-empty 2-bar one.
  int bars = 1;
};

// Normalizes a REAL absolute-MIDI-pitch pattern (-1 = rest, e.g. gui_
// sonotron_preview::PreviewPattern::pitch) into a ClipPattern (rows in
// [0,kPitches) or -1), linearly scaling the OBSERVED pitch span (across
// EVERY voice at every step, across ALL `bars` bars) onto the fixed row
// band -- a pattern's lowest sounding note always draws at row 0, its
// highest at kPitches-1, so melodic contour stays legible regardless of the
// role's actual register. A pattern with one distinct pitch (or none) maps
// everything to the middle row. Both callers (grid_panel.cpp's mini-preview,
// seqedit_panel.cpp's canvas) feed this the SAME preview_for() output (pitch
// AND bars) for a given cell, so the two views match by construction.
ClipPattern clip_pattern_from_pitches(
    const std::array<std::array<int, ClipPattern::kMaxVoicesPerStep>, ClipPattern::kMaxSteps>&
        pitches,
    int bars);

// Soft outer glow behind a rounded-rect element: a few expanding translucent
// outlines on `dl`, honoring `glow` (no-op when false). Draw BEFORE the
// element so the halo sits underneath it.
void glow_rect(ImDrawList* dl, const ImVec2& min, const ImVec2& max, const ImVec4& color,
               float rounding, float intensity, bool glow);

// Soft outer glow behind a circular element (expanding translucent rings).
void glow_circle(ImDrawList* dl, const ImVec2& center, float radius, const ImVec4& color,
                 float intensity, bool glow);

// The neon background: two faint radial glows (top-right cyan, bottom-left
// teal) + a 40px grid overlay, drawn into [min,max] on `dl` behind everything.
void background(ImDrawList* dl, const ImVec2& min, const ImVec2& max);

// A rounded neon "pad" button (transport rack). Drawn at the current cursor;
// advances it by `size`. `glyph` is centered; `accent` tints border/glow and
// (when `filled`) the fill. Returns true on click.
bool pad_button(const char* id, const char* glyph, const ImVec2& size, const ImVec4& accent,
                bool filled, bool glow);

// Rotary knob: an arc from -135deg sweeping value*270deg over a dark hub + a
// colored pointer tick, dragged VERTICALLY (up = +). Draws a centered label
// underneath. `value` in [0,1]; returns true if it changed this frame.
bool knob(const char* id, float* value, const ImVec4& color, const char* label, float size,
          bool glow);

// XY pad: a draggable blue dot with a radial glow over a gridded inset.
// `valence` = horizontal [0,1], `energy` = vertical [0,1] (up = more). Drawn at
// the current cursor over `size`; returns true if the dot moved this frame.
bool xy_pad(const char* id, float* valence, float* energy, const ImVec2& size, bool glow);

// Master VU: a 10-segment horizontal EQ meter (green->amber->pink), each bar
// animating while `playing`, flat at rest. Drawn at the current cursor over
// `size`.
void master_vu(const char* id, const ImVec2& size, bool playing, float time, bool glow);

// Procedural mini waveform envelope, seed-driven amplitude bars -- reserved
// for GENUINE future audio content (a real captured LoopBuffer envelope);
// no current caller feeds it real audio data (repeat-zone-real-contract.md:
// the pad row shows its real MIDI note pattern instead, via
// clip_preview_pianoroll below, since there is no real audio content yet).
void clip_preview_waveform(ImDrawList* dl, const ImVec2& min, const ImVec2& max, std::uint32_t seed,
                           const ImVec4& color);

// One (step, pitch) grid-cell rect within [band_min, band_max], given
// `steps` columns and `pitches` rows -- the ONE shared layout helper BOTH
// the launch-cell mini-preview (clip_preview_pianoroll below) and the
// Sequence Edit canvas (seqedit_panel.cpp) call, so a note at a given
// (step, pitch) ALWAYS lands at the identical relative position in both
// views by construction (sharing one function), rather than by two
// independently hand-written formulas that could silently drift apart on a
// future edit to either call site. STEP runs left->right (X), PITCH
// low->high (Y -- pitch 0 sits at the BOTTOM of the band).
struct PitchCellRect {
  ImVec2 min;
  ImVec2 max;
};
PitchCellRect pitch_grid_cell(const ImVec2& band_min, const ImVec2& band_max, int step, int steps,
                              int pitch, int pitches);

// A little horizontal piano-roll inside a launch cell's inner rect: ALL
// `pattern.bars * kSteps` columns of `pattern` (every bar the section
// actually has, owner bug #2/#3, compressed into the SAME band width so a
// 2-bar section reads denser, never truncated to bar 1), drawn at the SAME
// (step, row) positions the Sequence Edit canvas uses, so a cell dot is the
// SAME content the editor draws in full, never a half-cropped subset. Every
// simultaneous voice at a step (kMaxVoicesPerStep of them, e.g. a drum kit's
// kick+hihat both on beat 1) draws its OWN row-bar, never collapsed to a
// single level (owner bug #13).
// STEP runs left->right (X), ROW low->high (Y, row 0 at the bottom).
// `pattern` is real content (clip_pattern_from_pitches() above), never a
// hash-seeded generator -- the caller resolves it once per cell/frame.
void clip_preview_pianoroll(ImDrawList* dl, const ImVec2& min, const ImVec2& max,
                            const ClipPattern& pattern, const ImVec4& color);

// The L->R sweep bar animating across a playing+running cell (~1.7s loop).
// Wall-clock only (`time` == ImGui's frame clock) -- NOT tempo-synced. Kept
// for API stability (no other caller has been removed from this repo), but
// grid_panel.cpp's launch-cell playhead now draws with playhead_at() below
// instead, which is beat-synced to the ACTIVE SCENE's own section length.
void sweep_bar(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float time,
               const ImVec4& color);

// The beat-synchronized launch-cell playhead: a vertical bar at the explicit
// `phase01` position (0 = left edge, 1 = right edge) across [min,max] --
// visually identical to sweep_bar's own two-pass line (a crisp core + a
// soft wash), but driven by a caller-computed phase (grid_model.hpp's
// section_playhead_phase, itself sourced from the authoritative beat/bar/
// pulse) instead of a fixed wall-clock period. `phase01` is clamped to
// [0,1] defensively; the caller is expected to only invoke this once it has
// already checked for the "no playhead" sentinel (< 0).
void playhead_at(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float phase01,
                 const ImVec4& color);

}  // namespace sonotron::neon
