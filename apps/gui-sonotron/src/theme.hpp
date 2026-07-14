#pragma once

#include <array>

#include "imgui.h"
#include "track_roles.hpp"

// sonotron ImGui theme -- the token-based color/geometry system extracted
// from the shipped GUI itself (design ref: tokens/colors.css,
// tokens/spacing.css -- both derived from THIS app's own
// ImGui::StyleColorsDark() baseline, the hardcoded ImVec4 accents the panels
// used to carry, and components/hostrt/ui_style.cpp's ANSI role theme).
// apply() overwrites the relevant ImGuiStyle knobs with the resolved
// sonotron values; the named constants below let every *_panel.cpp
// reference a semantic token instead of hardcoding an ImVec4, per the
// design doctrine: blue is the one interactive accent, everything else is
// meaning (green = followed/live/ok, amber = pending/next-bar/warning,
// red = error/disconnected, cyan = structure/titles).
//
// This header names ImVec4/ImGuiStyle but calls no ImGui:: widget function
// and sends no BrainSession command, so it lives in gui_sonotron_layout
// alongside layout_renderer.cpp without breaking the *_panel/*_model split
// (that invariant is about who calls ImGui:: widgets / sends commands, not
// who names a color constant).

namespace sonotron::theme {

// ---- Base neutrals (back-to-front) ----
inline constexpr ImVec4 kAppBg{0.1019608F, 0.1019608F, 0.1215686F, 1.0F};      // #1a1a1f (GL clear)
inline constexpr ImVec4 kWindowBg{0.0588235F, 0.0588235F, 0.0588235F, 1.0F};   // #0f0f0f
inline constexpr ImVec4 kPanelBg{0.0745098F, 0.0745098F, 0.0862745F, 1.0F};    // #131316
inline constexpr ImVec4 kPanelBg2{0.0901961F, 0.0901961F, 0.1058824F, 1.0F};   // #17171b
inline constexpr ImVec4 kPopupBg{0.0784314F, 0.0784314F, 0.0784314F, 1.0F};    // #141414
inline constexpr ImVec4 kMenuBarBg{0.1372549F, 0.1372549F, 0.1372549F, 1.0F};  // #232323
inline constexpr ImVec4 kTitleBg{0.0392157F, 0.0392157F, 0.0392157F, 1.0F};    // #0a0a0a

// ---- Frame (input) background: resolved translucent-blue-over-black ----
inline constexpr ImVec4 kFrameBg{0.1137255F, 0.1843137F, 0.2862745F, 1.0F};        // #1d2f49
inline constexpr ImVec4 kFrameBgHover{0.1529412F, 0.2666667F, 0.4078431F, 1.0F};   // #274468
inline constexpr ImVec4 kFrameBgActive{0.1764706F, 0.3294118F, 0.5254902F, 1.0F};  // #2d5486

// ---- Text ----
inline constexpr ImVec4 kText{1.0000000F, 1.0000000F, 1.0000000F, 1.0F};           // #ffffff
inline constexpr ImVec4 kTextSecondary{0.6588235F, 0.6588235F, 0.6901961F, 1.0F};  // #a8a8b0
inline constexpr ImVec4 kTextMuted{0.5019608F, 0.5019608F, 0.5019608F, 1.0F};      // #808080
inline constexpr ImVec4 kTextDim{0.4156863F, 0.4156863F, 0.4156863F, 1.0F};        // #6a6a6a

// ---- Borders / separators ----
inline constexpr ImVec4 kBorder{0.2274510F, 0.2274510F, 0.2627451F, 1.0F};        // #3a3a43
inline constexpr ImVec4 kBorderStrong{0.4313725F, 0.4313725F, 0.5019608F, 1.0F};  // #6e6e80

// ---- Interactive accent -- the one ImGui blue ----
inline constexpr ImVec4 kAccent{0.2588235F, 0.5882353F, 0.9803922F, 1.0F};        // #4296fa
inline constexpr ImVec4 kAccentRest{0.1725490F, 0.3529412F, 0.5803922F, 1.0F};    // #2c5a94
inline constexpr ImVec4 kAccentActive{0.0588235F, 0.5294118F, 0.9803922F, 1.0F};  // #0f87fa
inline constexpr ImVec4 kAccentGrab{0.2392157F, 0.5176471F, 0.8784314F, 1.0F};    // #3d84e0
inline constexpr ImVec4 kHeader{0.1529412F, 0.2901961F, 0.4705882F, 1.0F};        // #274a78

// ---- Semantic -- meaning-bearing, from the panel source ----
inline constexpr ImVec4 kGreen{0.3019608F, 0.8509804F, 0.3019608F, 1.0F};           // #4dd94d
inline constexpr ImVec4 kGreenConnected{0.2509804F, 0.8509804F, 0.3490196F, 1.0F};  // #40d959
inline constexpr ImVec4 kGreenSoft{0.5490196F, 0.8509804F, 0.5490196F, 1.0F};       // #8cd98c
inline constexpr ImVec4 kAmber{0.9019608F, 0.7019608F, 0.2000000F, 1.0F};           // #e6b333
inline constexpr ImVec4 kRed{0.8509804F, 0.3019608F, 0.3019608F, 1.0F};             // #d94d4d
inline constexpr ImVec4 kCyan{0.2627451F, 0.8509804F, 0.8509804F, 1.0F};            // #43d9d9

// ---- Meter fill (the block bars: energy/tension/valence, volume) ----
inline constexpr ImVec4 kMeterFill = kAccent;                                   // #4296fa
inline constexpr ImVec4 kMeterTrack{0.1490196F, 0.1490196F, 0.1725490F, 1.0F};  // #26262c

// Per-role tint ramp (workstation-layout.md: LaunchGrid rows, the opened
// clip). Order matches track_roles.hpp's kTrackRoleLabels exactly: Drums,
// Perc, Bass, Chord1, Chord2, Pad, Arp, Phrase, Lead.
inline constexpr std::array<ImVec4, kTrackRoleCount> kRoleTint = {
    kAccent, kAmber, kGreen, kCyan, kAccent, kGreenSoft, kCyan, kAmber, kGreen,
};

// Applies every color + geometry token to ImGui::GetStyle(). Call once at
// startup, AFTER ImGui::StyleColorsDark() (main.cpp): this overwrites the
// dark-theme baseline with the resolved sonotron values rather than
// building a style from scratch, so any ImGuiCol_/metric this function does
// not explicitly touch keeps ImGui's own sane dark default.
void apply();

// Renders `fmt` in `color` with a faked "bold" stroke: the real text is
// drawn once (normal cursor advance, exactly like ImGui::TextColored), then
// the same glyphs are stamped again 1px to the right directly on the draw
// list (no extra cursor advance). Only one font weight (Regular) is
// vendored/loaded (tokens/fonts.css: "500-700 are synthetic") and ImGui has
// no font-weight axis of its own, so this is the closest ImGui-side
// equivalent of a synthetic-bold fallback -- used for the readouts the
// design calls out as bold (ChordReadout's "follows"/"next" value,
// BeatReadout's bar/beat).
void text_bold_colored(const ImVec4& color, const char* fmt, ...) IM_FMTARGS(2);

// Renders a fixed-cell block Meter (the "cells" glyph bars components.jsx
// describes for energy/tension/valence and per-part volume): `cells` cells
// of `cell_size`, the first `filled` of them lit in `fill_color` over
// kMeterTrack, the rest a faint kBorder outline. Advances the cursor past
// the whole track via an ImGui::Dummy of its total size, so callers can
// ImGui::SameLine() a value readout right after it.
void render_meter_cells(int cells, int filled, const ImVec4& fill_color, const ImVec2& cell_size,
                        float gap = 2.0F);

}  // namespace sonotron::theme
