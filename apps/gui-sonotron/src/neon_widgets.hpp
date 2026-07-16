#pragma once

#include <array>
#include <cstdint>

#include "imgui.h"

// Reusable Dear ImGui draw-list widgets for the v02 "neon / hardware-synth"
// workstation redesign (v02-workstation-spec.md §"Widgets NEW"). Everything
// here is pure ImDrawList / immediate-mode input -- no BrainSession, no model
// dependency -- so it lives in gui_sonotron_layout next to theme.cpp and the
// panels, and every panel that needs a knob / XY pad / VU / glow shares one
// implementation instead of re-rolling draw code per zone.
//
// The GLOW SYSTEM: every helper that can glow takes a `glow` bool. When false
// the halo draws are skipped (shadow -> none), honoring the one global
// V02State::glow flag the whole UI passes down. Animations take an explicit
// `time` (ImGui frame clock) and a `playing`/gate flag so the caller decides
// when motion is allowed -- the widgets never read the clock themselves.

namespace sonotron::neon {

// Packs an ImVec4 into ImU32, scaling alpha by `alpha_mul` (clamped) -- the
// idiom the draw-list halo/wash layers use to fade a token color.
ImU32 u32(const ImVec4& color, float alpha_mul = 1.0F);

// The canonical rendered note pattern for one clip: `kSteps` columns, each
// holding a pitch ROW in [0, kPitches) or -1 (a rest). This is the ONE shape
// shared by the launch-cell mini-preview (clip_preview_pianoroll) and the
// Sequence Edit canvas, so the SAME clip reads as the SAME melody in both --
// the cell shows a step-cropped subset (first kCellSteps columns) of exactly
// the blocks the editor draws in full. STEP runs left->right, PITCH low->high
// (row 0 at the bottom).
//
// Real-content cell preview (repeat-zone-real-contract.md): populate this
// from real resolved note data via clip_pattern_from_pitches() below, fed by
// gui_sonotron_preview's preview_for() -- never from a label hash. This
// header stays dependency-free (no arrangrr, no gui_sonotron_preview): the
// conversion below takes plain absolute MIDI pitches (or -1 for a rest), the
// same shape gui_sonotron_preview::PreviewPattern::pitch already has.
struct ClipPattern {
  static constexpr int kSteps = 16;
  static constexpr int kPitches = 5;
  static constexpr int kCellSteps = 8;  // how many columns the mini-preview crops to
  std::array<int, kSteps> pitch{};      // pitch[step] in [0,kPitches) or -1 for a rest
};

// Normalizes a REAL absolute-MIDI-pitch pattern (-1 = rest, e.g. gui_
// sonotron_preview::PreviewPattern::pitch) into a ClipPattern (rows in
// [0,kPitches) or -1), linearly scaling the OBSERVED pitch span onto the
// fixed row band -- a pattern's lowest sounding note always draws at row 0,
// its highest at kPitches-1, so melodic contour stays legible regardless of
// the role's actual register. A pattern with one distinct pitch (or none)
// maps everything to the middle row. Both callers (grid_panel.cpp's
// mini-preview, seqedit_panel.cpp's canvas) feed this the SAME preview_for()
// output for a given cell, so the two views match by construction.
ClipPattern clip_pattern_from_pitches(const std::array<int, ClipPattern::kSteps>& pitches);

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
void clip_preview_waveform(ImDrawList* dl, const ImVec2& min, const ImVec2& max,
                           std::uint32_t seed, const ImVec4& color);

// A little horizontal piano-roll inside a launch cell's inner rect: a
// step-cropped view (the first kCellSteps columns) of `pattern`, drawn at
// the SAME (step, row) positions the Sequence Edit canvas uses, so a cell
// dot is a legible subset of the editor's blocks, never a different melody.
// STEP runs left->right (X), ROW low->high (Y, row 0 at the bottom).
// `pattern` is real content (clip_pattern_from_pitches() above), never a
// hash-seeded generator -- the caller resolves it once per cell/frame.
void clip_preview_pianoroll(ImDrawList* dl, const ImVec2& min, const ImVec2& max,
                            const ClipPattern& pattern, const ImVec4& color);

// The L->R sweep bar animating across a playing+running cell (~1.7s loop).
void sweep_bar(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float time,
               const ImVec4& color);

}  // namespace sonotron::neon
