#pragma once

#include <array>

#include "imgui.h"
#include "track_roles.hpp"

// sonotron ImGui theme -- the token-based color/geometry system extracted
// from the shipped GUI itself (design ref: tokens/colors.css,
// tokens/spacing.css -- both derived from THIS app's own
// ImGui::StyleColorsDark() baseline, the hardcoded ImVec4 accents the panels
// used to carry, and components/platform/hostrt/ui_style.cpp's ANSI role theme).
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

// ---- v02 base neutrals (near-black neon "hardware synth" surfaces) ----
// Redesign palette (v02-workstation-spec.md §Palette): the earlier flat
// terminal hexes are overridden with the v02 inline neon values. The named
// constants are kept (every *_panel.cpp references a semantic token, not a
// literal) -- only the resolved RGB moves.
inline constexpr ImVec4 kAppBg{0.0392157F, 0.0431373F, 0.0627451F, 1.0F};      // #0a0b10 (GL clear)
inline constexpr ImVec4 kWindowBg{0.0392157F, 0.0431373F, 0.0627451F, 1.0F};   // #0a0b10
inline constexpr ImVec4 kPanelBg{0.0470588F, 0.0549020F, 0.0823529F, 1.0F};    // #0c0e15
inline constexpr ImVec4 kPanelBg2{0.0431373F, 0.0509804F, 0.0784314F, 1.0F};   // #0b0d14
inline constexpr ImVec4 kInsetBg{0.0313725F, 0.0392157F, 0.0627451F, 1.0F};    // #080a10 (sunken)
inline constexpr ImVec4 kPopupBg{0.0470588F, 0.0549020F, 0.0823529F, 1.0F};    // #0c0e15
inline constexpr ImVec4 kMenuBarBg{0.0431373F, 0.0509804F, 0.0784314F, 1.0F};  // #0b0d14
inline constexpr ImVec4 kTitleBg{0.0313725F, 0.0392157F, 0.0627451F, 1.0F};    // #080a10

// ---- Frame (input) background: dark cyan-tinted inset ----
inline constexpr ImVec4 kFrameBg{0.0470588F, 0.0627451F, 0.0862745F, 1.0F};        // #0c1016
inline constexpr ImVec4 kFrameBgHover{0.0745098F, 0.1058824F, 0.1411765F, 1.0F};   // #131b24
inline constexpr ImVec4 kFrameBgActive{0.1098039F, 0.1568627F, 0.2078431F, 1.0F};  // #1c2835

// ---- Text ----
inline constexpr ImVec4 kText{0.8117647F, 0.8784314F, 0.9019608F, 1.0F};           // #cfe0e6
inline constexpr ImVec4 kTextSecondary{0.5607843F, 0.6274510F, 0.6705882F, 1.0F};  // #8fa0ab
inline constexpr ImVec4 kTextMuted{0.4274510F, 0.4862745F, 0.5254902F, 1.0F};      // #6d7c86
inline constexpr ImVec4 kTextDim{0.2274510F, 0.2705882F, 0.2980392F, 1.0F};        // #3a454c

// ---- Borders / separators ----
inline constexpr ImVec4 kBorder{0.1098039F, 0.1411765F, 0.1960784F, 1.0F};        // #1c2432
inline constexpr ImVec4 kBorderStrong{0.1333333F, 0.8784314F, 0.9019608F, 0.24F};  // cyan @ .24
inline constexpr ImVec4 kBorderCyan{0.1333333F, 0.8784314F, 0.9019608F, 0.14F};   // rgba(34,224,230,.14)

// ---- Interactive accent -- v02 blue (also the XY dot) ----
inline constexpr ImVec4 kAccent{0.1803922F, 0.6588235F, 1.0000000F, 1.0F};        // #2ea8ff
inline constexpr ImVec4 kAccentRest{0.1098039F, 0.3921569F, 0.6000000F, 1.0F};    // #2ea8ff @ .6
inline constexpr ImVec4 kAccentActive{0.3803922F, 0.7607843F, 1.0000000F, 1.0F};  // #61c2ff
inline constexpr ImVec4 kAccentGrab{0.1803922F, 0.6588235F, 1.0000000F, 1.0F};    // #2ea8ff
inline constexpr ImVec4 kHeader{0.1098039F, 0.2000000F, 0.3098039F, 1.0F};        // dim blue wash

// ---- Semantic -- v02 neon, meaning-bearing ----
inline constexpr ImVec4 kGreen{0.2392157F, 1.0000000F, 0.6274510F, 1.0F};           // #3dffa0
inline constexpr ImVec4 kGreenConnected{0.2392157F, 1.0000000F, 0.6274510F, 1.0F};  // #3dffa0
inline constexpr ImVec4 kGreenSoft{0.2392157F, 1.0000000F, 0.6274510F, 1.0F};       // #3dffa0
inline constexpr ImVec4 kAmber{1.0000000F, 0.7607843F, 0.3019608F, 1.0F};           // #ffc24d
inline constexpr ImVec4 kRed{1.0000000F, 0.2431373F, 0.6470588F, 1.0F};             // #ff3ea5 (pink=danger)
inline constexpr ImVec4 kCyan{0.1333333F, 0.8784314F, 0.9019608F, 1.0F};            // #22e0e6
inline constexpr ImVec4 kPink{1.0000000F, 0.2431373F, 0.6470588F, 1.0F};            // #ff3ea5
inline constexpr ImVec4 kBlue{0.1803922F, 0.6588235F, 1.0000000F, 1.0F};            // #2ea8ff
inline constexpr ImVec4 kLead{0.7764706F, 0.4705882F, 0.8666667F, 1.0F};            // #c678dd
inline constexpr ImVec4 kStopDark{0.0784314F, 0.0941176F, 0.1411765F, 1.0F};        // #141824

// ---- Meter fill (the block bars: energy/tension/valence, volume) ----
inline constexpr ImVec4 kMeterFill = kCyan;                                        // #22e0e6
inline constexpr ImVec4 kMeterTrack{0.0470588F, 0.0627451F, 0.0862745F, 1.0F};     // #0c1016

// Per-role tint ramp (workstation-layout.md: LaunchGrid rows, the opened
// clip). Order matches track_roles.hpp's kTrackRoleLabels exactly: Drums,
// Perc, Bass, Chord1, Chord2, Pad, Arp, Phrase, Lead.
inline constexpr std::array<ImVec4, kTrackRoleCount> kRoleTint = {
    kCyan, kAmber, kBlue, kGreen, kGreen, kAmber, kCyan, kLead, kLead,
};

// The 6 v02 launch-grid track colors (drums/bass/chord/pad/arp/lead), in the
// exact row order the v02 grid renders (v02-workstation-spec.md §2b): cyan,
// blue, green, amber, cyan, lead-purple.
inline constexpr std::array<ImVec4, 6> kV02TrackColor = {
    kCyan, kBlue, kGreen, kAmber, kCyan, kLead,
};

// Applies every color + geometry token to ImGui::GetStyle(). Call once at
// startup, AFTER ImGui::StyleColorsDark() (main.cpp): this overwrites the
// dark-theme baseline with the resolved sonotron values rather than
// building a style from scratch, so any ImGuiCol_/metric this function does
// not explicitly touch keeps ImGui's own sane dark default.
void apply();

// Renders `fmt` in `color` as a single crisp draw (exactly ImGui::TextColored),
// naming the readouts the design calls out as emphasised (ChordReadout's
// "follows"/"next" value, BeatReadout's bar/beat, the BPM readout). Only the
// Regular font weight is loaded (tokens/fonts.css: "500-700 are synthetic") and
// ImGui has no font-weight axis, so emphasis is carried by COLOR, not a faked
// weight: an earlier synthetic-bold double-stamp (a 1px-offset second draw)
// smeared the 13px monospace glyphs into a doubled, unreadable look and was
// removed. The helper is kept as the semantic seam for these readouts.
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
