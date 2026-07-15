#pragma once

#include <array>
#include <cstdint>
#include <string_view>

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

// Deterministic 32-bit hash of a clip label -> the procedural preview seed
// (FNV-1a). The same label always yields the same waveform / piano-roll.
std::uint32_t hash_label(std::string_view label);

// The canonical deterministic note pattern for one clip: `kSteps` columns,
// each holding a pitch row in [0, kPitches) or -1 (a rest). This is the ONE
// generator shared by the launch-cell mini-preview (clip_preview_pianoroll)
// and the Sequence Edit canvas, so the SAME clip reads as the SAME melody in
// both -- the cell shows a step-cropped subset (first kCellSteps columns) of
// exactly the blocks the editor draws in full. Seed with hash_label(label).
struct ClipPattern {
  static constexpr int kSteps = 16;
  static constexpr int kPitches = 5;
  static constexpr int kCellSteps = 8;  // how many columns the mini-preview crops to
  std::array<int, kSteps> pitch{};      // pitch[step] in [0,kPitches) or -1 for a rest
};
ClipPattern clip_pattern(std::uint32_t seed);

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

// Procedural mini clip previews inside a launch cell's inner rect: a centered
// waveform envelope (audio/pad row) or a 16x5 dot piano-roll (midi rows), both
// deterministic from `seed` (hash_label).
void clip_preview_waveform(ImDrawList* dl, const ImVec2& min, const ImVec2& max,
                           std::uint32_t seed, const ImVec4& color);
void clip_preview_pianoroll(ImDrawList* dl, const ImVec2& min, const ImVec2& max,
                            std::uint32_t seed, const ImVec4& color);

// The L->R sweep bar animating across a playing+running cell (~1.7s loop).
void sweep_bar(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float time,
               const ImVec4& color);

}  // namespace sonotron::neon
