#include "grid_panel.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app_state.hpp"
#include "browser_model.hpp"
#include "debug_log.hpp"
#include "imgui.h"
#include "launch_rows.hpp"
#include "neon_widgets.hpp"
#include "preview.hpp"
#include "theme.hpp"

namespace sonotron {

namespace {

constexpr int kDefaultLaunchQuantizeBars = 1;

// The design's label column is 64px (square dot + track-colored name). We keep
// the per-track M/S latches (owner: mute is per-track), compacted so the column
// stays as close to the design as legibility allows.
constexpr float kLabelColWidth = 86.0F;
constexpr float kCellGap = 7.0F;
// Owner: the per-track M/S latches were 13px, too small to read or tell
// apart. Enlarged to clearly legible squares, stacked BELOW the track name
// (render_track_label) rather than crammed inline with it.
constexpr float kLatchSize = 17.0F;
constexpr float kLatchGap = 3.0F;

// Repeat Zone zoom clamp (render_header's -/+ buttons): named constants so
// the min/max/step are never duplicated bare literals across the two clamp
// calls. Owner ask (docs/proposals/seqedit-column-view-and-zoom.md Feature
// A): raise the max by 3 more notches (88 -> 115 == 88 + 3*9). Task #12:
// raise it by 3 MORE notches on top of that (115 -> 142 == 115 + 3*9); the
// font ramp (kFontScaleMax below) is already saturated well below 115, so
// these extra notches only grow the cell footprint itself, not the text.
constexpr float kCellZoomMin = 34.0F;
constexpr float kCellZoomMax = 142.0F;
constexpr float kCellZoomStep = 9.0F;

// Task #11 Phase 1 (Sequence Edit step sequencer, roadmap node 11600/
// 11610): default port/channel for a NEWLY created step track, keyed by
// TrackRole index (track_roles.hpp) -- every step track lives on port 0;
// only the channel varies by role so two step tracks on different roles do
// not collide on the same MIDI channel by default (a real per-track mixer/
// channel picker is a later enhancement, out of Phase 1 scope). Loosely
// mirrors the CLI's own per-role default-band convention (in_process_brain_
// session.cpp's kDefaultStyleRoutes), 0-based here to match this
// translator's own bare-numeric `track new` convention. drums/perc share
// channel 9 (GM's percussion channel, 0-based) deliberately -- both are
// drum-kit-ish roles a real setup would typically route there too.
constexpr std::uint8_t kStepTrackPort = 0;
constexpr std::array<std::uint8_t, kTrackRoleCount> kStepTrackChannel = {9, 9, 1, 2, 3, 4, 5, 6, 7};

// A small neon M/S latch: a SQUARE (owner: "squares are fine", not the
// theme's usual pill-rounded ImGui::Button, which at this compact footprint
// rounds into a near-circle), solid tone fill when engaged, dark inset
// otherwise, with an explicit-size letter glyph. Drawn manually (InvisibleButton
// + draw-list), NOT ImGui::Button: Button's internal RenderTextClipped()
// centers/clips the label at the CURRENT (global, ~20px) font size, which is
// taller than this button -- the top of a clipped "S" reads as a bare hook,
// illegible (the actual bug behind the owner's "can't tell M/S apart"
// report). Drawing the glyph ourselves at a small explicit size (matching
// draw_cell's own ~11px compact-label convention) avoids that entirely.
bool grid_latch(const char* glyph, bool engaged, const ImVec4& tone) {
  const ImVec2 p0 = ImGui::GetCursorScreenPos();
  const bool clicked = ImGui::InvisibleButton(glyph, ImVec2(kLatchSize, kLatchSize));
  const bool hovered = ImGui::IsItemHovered();
  const ImVec2 p1(p0.x + kLatchSize, p0.y + kLatchSize);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const float rounding = 3.0F;
  dl->AddRectFilled(p0, p1, neon::u32(engaged ? tone : theme::kFrameBg, hovered ? 1.0F : 0.9F),
                    rounding);
  dl->AddRect(p0, p1, neon::u32(engaged ? tone : theme::kBorder, hovered ? 1.0F : 0.7F), rounding,
              0, 1.0F);
  ImFont* font = ImGui::GetFont();
  const float glyph_sz = 12.0F;
  const ImVec2 ts = font->CalcTextSizeA(glyph_sz, 1.0e4F, 0.0F, glyph);
  dl->AddText(font, glyph_sz,
              ImVec2(p0.x + (kLatchSize - ts.x) * 0.5F, p0.y + (kLatchSize - ts.y) * 0.5F),
              neon::u32(engaged ? theme::kAppBg : theme::kTextSecondary), glyph);
  return clicked;
}

// Owner: the scene-header name used to draw at the default font size,
// overflowing longer/renamed names past the column width. Rendered at the
// same small size draw_cell's own bottom label uses (11px), word-wrapped
// across multiple lines so it fits within the column instead.
constexpr float kSceneNameFontSize = 11.0F;

// Owner: "anche i font si ingrandiscono leggermente ad ogni livello di zoom,
// sii equilibrato" (fonts should also grow slightly at every zoom level,
// stay balanced) -- cell text grows with cell_zoom but SUB-LINEARLY, milder
// than the cell edge itself (which scales 1:1). A sqrt ramp, clamped to a
// mild [kFontScaleMin, kFontScaleMax] range, keeps text legible at the
// smallest zoom and noticeably-but-not-proportionally larger at the
// biggest, rather than growing in lockstep with the cell.
constexpr float kFontScaleReferenceZoom = 52.0F;  // UiState::cell_zoom's own default.
constexpr float kFontScaleMin = 0.85F;
constexpr float kFontScaleMax = 1.3F;

float cell_font_scale(float cell_zoom) {
  return std::clamp(std::sqrt(cell_zoom / kFontScaleReferenceZoom), kFontScaleMin, kFontScaleMax);
}

// Greedy word-wrap of `text` into lines that fit `max_width` at `font_size`
// -- mirrors the column-width fitting draw_cell's own bottom label already
// does (grid_panel.cpp), just line-by-line instead of truncating. A single
// word wider than `max_width` on its own is kept whole (never mid-word
// split) rather than overflowing onto more lines than the text actually has.
std::vector<std::string> wrap_scene_name(ImFont* font, float font_size, const std::string& text,
                                         float max_width) {
  std::vector<std::string> lines;
  std::string current;
  std::string word;
  auto flush_word = [&]() {
    if (word.empty()) {
      return;
    }
    const std::string candidate = current.empty() ? word : current + " " + word;
    if (current.empty() ||
        font->CalcTextSizeA(font_size, 1.0e4F, 0.0F, candidate.c_str()).x <= max_width) {
      current = candidate;
    } else {
      lines.push_back(current);
      current = word;
    }
    word.clear();
  };
  for (const char c : text) {
    if (c == ' ') {
      flush_word();
    } else {
      word.push_back(c);
    }
  }
  flush_word();
  if (!current.empty() || lines.empty()) {
    lines.push_back(current);
  }
  return lines;
}

// Task #6: the always-visible per-scene LENGTH stepper, a small "- N +" row
// below the scene name/caret/underline (owner lock: always visible, not an
// on-hover-only affordance). GridModel::kMaxSceneBars caps the value at one
// digit, so the number never needs more room than that -- these buttons only
// have to fit two single-digit-scale glyphs plus one digit within `cz`, which
// can be as small as 34px at the smallest zoom.
constexpr float kStepperBtnW = 12.0F;
constexpr float kStepperRowH = 14.0F;

// A single "-"/"+" stepper button: drawn manually (InvisibleButton + draw-
// list glyph), mirroring grid_latch's own reasoning above -- ImGui::Button's
// RenderTextClipped centers/clips its label at the CURRENT (global, ~20px)
// font size, illegible at this compact a footprint. `p0` is the button's own
// top-left corner in screen space; `glyph` doubles as this InvisibleButton's
// ID (unique within the caller's own PushID(scene_index) scope, same as
// grid_latch's "M"/"S" pair).
bool stepper_button(const char* glyph, ImVec2 p0, float font_size) {
  ImGui::SetCursorScreenPos(p0);
  const bool clicked = ImGui::InvisibleButton(glyph, ImVec2(kStepperBtnW, kStepperRowH));
  const bool hovered = ImGui::IsItemHovered();
  const ImVec2 p1(p0.x + kStepperBtnW, p0.y + kStepperRowH);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(p0, p1, neon::u32(theme::kFrameBg, hovered ? 1.0F : 0.85F), 2.0F);
  dl->AddRect(p0, p1, neon::u32(theme::kBorder, hovered ? 1.0F : 0.6F), 2.0F, 0, 1.0F);
  ImFont* font = ImGui::GetFont();
  const ImVec2 ts = font->CalcTextSizeA(font_size, 1.0e4F, 0.0F, glyph);
  dl->AddText(font, font_size,
              ImVec2(p0.x + (kStepperBtnW - ts.x) * 0.5F, p0.y + (kStepperRowH - ts.y) * 0.5F),
              neon::u32(theme::kTextSecondary), glyph);
  return clicked;
}

// Feature B (docs/proposals/seqedit-column-view-and-zoom.md) whole-column
// highlight for the scene-header cell: a subtle cyan tint under this header
// cell's own [hp0, hp0+(cz,header_h)] footprint, painted only while `s` is
// the column currently open in Sequence Edit (fx.open_scene). Drawn as a
// FILL (not a border) since nothing else in render_scene_header_cell paints
// an opaque background for it to be hidden under, unlike the launch cells
// below (see render_track_cell's own highlight, a border drawn ON TOP for
// exactly that reason). Alpha deliberately 0.13F, not the more obvious
// 0.10F: kTrackColor[0] (drums) IS theme::kCyan, and draw_cell's own "not
// playing" cell fill is exactly neon::u32(track_color, 0.10F) -- an
// identical packed color here would make the UI-automation harness's
// exact-color cell locator (find_color_clusters) pick up THIS header tint
// instead of the real drums/scene-0 launch cell whenever that column is
// open, silently clicking the wrong on-screen location (caught by
// test_grid_cell_launch_open_seqedit_ui_automation.cpp regressing). Pulled
// out of render_scene_header_cell as its own function (rather than an
// inline `if`) purely to keep that function's own cognitive-complexity
// score under clang-tidy's threshold -- it was already at 26/25 before this
// feature, so a fifth inline branch pushed it over.
void draw_scene_header_column_highlight(const UiState& fx, std::size_t s, ImVec2 hp0, float cz,
                                        float header_h) {
  if (fx.open_scene != static_cast<int>(s)) {
    return;
  }
  ImGui::GetWindowDrawList()->AddRectFilled(hp0, ImVec2(hp0.x + cz, hp0.y + header_h),
                                            neon::u32(theme::kCyan, 0.13F), 4.0F);
}

// GridRow/kRows moved to launch_rows.hpp (Fabrizio review, 2026-07-18): both
// this panel and seqedit_panel.cpp now share the SAME 7-row launch set, so
// Sequence Edit's lane set can never again drift from the Repeat Zone's own
// rows -- see that header's own comment for the full rationale.

std::size_t cell_id(std::size_t role_index, std::size_t scene, std::size_t scene_count) {
  return role_index * scene_count + scene;
}

// Song-mode Phase 1 (docs/proposals/song-mode-scenechain-adoption.md): a
// scene column counts as "populated" if any of the 7 launch-grid rows holds
// a real clip -- an empty column has nothing for a SceneChain step's own
// Performance to represent musically, so build_and_play_song (below) skips
// it rather than building an audible-but-empty step for it.
bool scene_is_populated(const GridModel& model, std::size_t scene_index) {
  for (const GridRow& row : kRows) {
    if (model.cell(row.role_index, scene_index).kind != GridCellKind::kEmpty) {
      return true;
    }
  }
  return false;
}

// Seeds a scattered demo clip pattern on the first frame so the procedural
// previews have content (same local-content path a browser drag uses;
// launching still sends real verbs). Deterministic labels -> deterministic
// previews.
//
// Owner bug #1 root-cause fix: this used to ONLY call GridModel::set_cell
// (host-side display state) -- the demo cells were never registered with the
// core's ClipMatrix, so `launch clip <id>`/`launch scene <n>` always warned
// (Engine::clip_request's `m_clips.get(id) == nullptr` branch, engine.cpp)
// and no cell could ever read back as playing (AppState::clip_state stuck at
// kStopped forever). Fixed by sending the SAME `clip add <role> <scene>
// style <section> id <n>` verb the browser drag-drop handler already sends
// (render_track_cell's BeginDragDropTarget block below) for every seeded
// demo cell, ONCE (guarded by the same `fx.seeded` latch this function
// already uses), after the scene sections are finalized (so the registered
// clip's SectionType matches what the column will actually apply/preview) --
// see the third loop below. `brain_session.send()` is a plain wire command
// even in --control mode, so this seeding works there too.
void seed_demo(GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
               BrainSession& brain_session, UiState& fx) {
  if (fx.seeded) {
    return;
  }
  fx.seeded = true;
  // Match the design's default clip set + short curated labels exactly
  // (Sonotron v02 Workstation.dc.html) so the launch grid reads like the ref
  // (A/B/fil, wlk/sub, cmp/stab/out, swl, up/up2, vox/ld/end) — no truncation.
  // Row positions below are kRows POSITIONS, not raw role_index (mirrors
  // this struct's own long-standing convention) -- mixer-roles fix
  // (2026-07-18) inserted `perc` at position 1 and `chord2` at position 4,
  // shifting bass/chord1/pad/arp one-to-two slots later than their OLD
  // positions (was 1/2/3/4, now 2/3/5/6); `lead`'s old position-5 demo cells
  // ("vox"/"ld"/"end") are dropped outright since `lead` no longer has a row
  // at all (the whole point of this fix -- it was never routed/audible).
  struct DemoCell {
    std::size_t row;
    std::size_t scene;
    const char* label;
  };
  static constexpr std::array<DemoCell, 12> pattern = {{
      {0, 0, "A"},
      {0, 1, "B"},
      {0, 3, "fil"},
      {2, 0, "wlk"},
      {2, 2, "sub"},
      {3, 0, "cmp"},
      {3, 1, "stab"},
      {3, 4, "out"},
      {5, 1, "swl"},
      {5, 3, "swl"},
      {6, 0, "up"},
      {6, 2, "up2"},
  }};
  for (const auto& [row, scene, label] : pattern) {
    if (row >= kRows.size() || scene >= model.scene_count()) {
      continue;
    }
    model.set_cell(kRows[row].role_index, scene, GridCellKind::kStyleSection, label);
  }

  // Root-cause fix: every scene column used to default to the SAME section
  // (GridModel::kDefaultSectionType == SectionType::kVarA), so selecting a
  // different column (or auto-song advancing) sent the identical `style
  // section varA` every time -- the arranger's live section never actually
  // changed, so the audio never changed either (owner-reported "always
  // scene 1"). Give the 5 demo columns 5 DISTINCT sections so a column
  // switch is audible; each built-in style authors genuinely different
  // per-section patterns (kVarBDrums != kVarADrums, etc.).
  struct DemoSection {
    std::size_t scene;
    preview::Section section;
    const char* name;
  };
  static constexpr std::array<DemoSection, 5> kDemoSections = {{
      {0, preview::Section::kIntro1, "Intro"},
      {1, preview::Section::kVarA, "Var A"},
      {2, preview::Section::kVarB, "Var B"},
      {3, preview::Section::kVarC, "Var C"},
      {4, preview::Section::kVarD, "Var D"},
  }};
  for (const auto& [scene, section, name] : kDemoSections) {
    if (scene >= model.scene_count()) {
      continue;
    }
    model.set_scene_section(scene, static_cast<std::uint8_t>(section));
    model.set_scene_name(scene, name);
  }

  // Owner bug #1 root-cause fix: register every seeded demo cell with the
  // core's ClipMatrix for real, at the SAME stable id a later launch
  // addresses (cell_id(role_index, scene, scene_count), the identical
  // formula render_track_cell/render_scene_header_cell use), so `launch
  // clip <id>`/`launch scene <n>` finds a real registered clip instead of
  // warning. Sent AFTER the scene-section loop above (not interleaved with
  // it), so each clip's registered SectionType matches the column's own
  // FINAL section rather than the kDefaultSectionType placeholder every
  // scene starts at.
  for (const DemoCell& entry : pattern) {
    if (entry.row >= kRows.size() || entry.scene >= model.scene_count()) {
      continue;
    }
    const std::size_t role_index = kRows[entry.row].role_index;
    const std::size_t scene = entry.scene;
    const std::string_view cell_section_name = section_wire_name(model.scene_section(scene));
    const std::string cell_section_arg =
        cell_section_name.empty() ? "varA" : std::string(cell_section_name);
    const std::size_t id = cell_id(role_index, scene, model.scene_count());
    brain_session.send("clip add " + std::string(parts.part_wire_token(role_index)) + " " +
                       std::to_string(scene) + " style " + cell_section_arg + " id " +
                       std::to_string(id));
  }

  // Open the bass 'wlk' clip by default — the design's initial openAt {r:1,c:0}
  // — so Sequence Edit shows a populated (blue) piano-roll, not the empty
  // hint. Bass now sits at kRows POSITION 2 (mixer-roles fix, 2026-07-18:
  // `perc` was inserted at position 1, ahead of it) -- the design's own
  // {r:1,c:0} reference predates that row-set change and names bass by its
  // OLD position, not a re-derived intent; row 2 is still bass here.
  fx.open_row = 2;
  fx.open_cell = static_cast<int>(cell_id(kRows[2].role_index, 0, model.scene_count()));
  fx.open_audio = kRows[2].audio;
  fx.open_section = model.scene_section(0);
  // Feature B: mirrors the real click path's own bookkeeping (render_track_
  // cell above) for this demo default-open cell -- column 0, never a WAV
  // (every demo cell is GridCellKind::kStyleSection).
  fx.open_scene = 0;
  fx.open_wav = false;
  seqedit.set_part_index(kRows[2].role_index);
  seqedit.set_clip_label("wlk");
  // Owner bug #1: mirrors render_track_cell's own fix below -- the initial
  // demo default-open cell must show every role too, not just the seeded
  // bass row.
  seqedit.set_all_tracks_visible(true);
}

// Draws one launch cell (custom draw-list) at the cursor; returns true on
// click. `filled` cells show a preview + label + a beat-synced playhead
// (see the playhead paint's own comment below for exactly which cell that
// is -- NOT simply "when playing"). `pattern`/`approx` are the cell's REAL
// resolved content (preview::preview_for, computed by the caller) and its
// honesty flag (repeat-zone-real-contract.md STEP 4) -- ignored when
// `!filled`. `playing` is this cell's own real ClipMatrix readback
// (AppState::clip_state(id) != kStopped); it still drives the "armed/lit"
// fill/border/glow/label appearance, but NOT the playhead paint (see below).
// `section_phase` is this cell's own playhead position in [0,1] (grid_
// model.hpp's section_playhead_phase, resolved by the caller ONLY for the
// active scene column's own cells), or the sentinel < 0 ("no playhead" --
// every other scene column, and the active one before the transport ever
// starts) computed once per frame by the caller (render_grid_panel), not
// here -- draw_cell stays a pure draw/click primitive with no bookkeeping of
// its own.
bool draw_cell(const char* id, float size, bool filled, const std::string& label,
               const ImVec4& color, const neon::ClipPattern& pattern, bool approx, bool playing,
               bool opened, const UiState& fx, float section_phase) {
  const ImVec2 p0 = ImGui::GetCursorScreenPos();
  const bool clicked = ImGui::InvisibleButton(id, ImVec2(size, size));
  const bool hovered = ImGui::IsItemHovered();
  const ImVec2 p1(p0.x + size, p0.y + size);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const float rounding = 7.0F;

  if (!filled) {
    dl->AddRectFilled(p0, p1, neon::u32(theme::kInsetBg), rounding);
    dl->AddRect(p0, p1, neon::u32(theme::kBorder, hovered ? 1.0F : 0.6F), rounding, 0, 1.0F);
    const ImVec2 ts = ImGui::CalcTextSize("+");
    dl->AddText(ImVec2(p0.x + (size - ts.x) * 0.5F, p0.y + (size - ts.y) * 0.5F),
                neon::u32(theme::kTextDim, hovered ? 1.5F : 1.0F), "+");
    return clicked;
  }

  // Filled: tinted fill (stronger when playing), track-color border + glow.
  if (playing && fx.glow) {
    neon::glow_rect(dl, p0, p1, color, rounding, 1.0F, fx.glow);
  }
  const float fill_a = playing ? 0.26F : (hovered ? 0.16F : 0.10F);
  dl->AddRectFilled(p0, p1, neon::u32(color, fill_a), rounding);
  dl->AddRect(p0, p1, neon::u32(color, playing ? 1.0F : 0.40F), rounding, 0, playing ? 1.6F : 1.0F);
  if (opened) {
    // Design: an inset ring in the TRACK color (not white).
    dl->AddRect(ImVec2(p0.x + 1.0F, p0.y + 1.0F), ImVec2(p1.x - 1.0F, p1.y - 1.0F),
                neon::u32(color, 0.9F), rounding - 1.0F, 0, 1.0F);
  }

  // Preview in a compact horizontal band (design: notes sit in a ~20%..78%
  // vertical band, above the label — not filling the whole cell). Real
  // content (repeat-zone-real-contract.md): `pattern` is the cell's own
  // resolved note data, the SAME the Sequence Edit canvas draws when this
  // cell is opened. The pad row (the design's only "audio" row) has no real
  // audio content yet, so it draws its real MIDI note pattern here too
  // (STEP 3 decision) rather than a fake waveform -- clip_preview_waveform
  // stays reserved for genuine future audio content.
  const ImVec2 in0(p0.x + 6.0F, p0.y + size * 0.22F);
  const ImVec2 in1(p1.x - 6.0F, p1.y - 14.0F);
  if (in1.y > in0.y + 4.0F) {
    neon::clip_preview_pianoroll(dl, in0, in1, pattern, color);
  }

  // Bottom-centered label "▶/▷ label" at the design's ~11px so short curated
  // names ("stab","up2") fit un-truncated and descenders (p/g/y) clear the
  // cell's bottom edge.
  ImFont* font = ImGui::GetFont();
  const float lbl_sz = 11.0F * cell_font_scale(size);
  std::string text = (playing ? "\xE2\x96\xB6 " : "\xE2\x96\xB7 ") + label;
  while (text.size() > 2 &&
         font->CalcTextSizeA(lbl_sz, 1.0e4F, 0.0F, text.c_str()).x > size - 6.0F) {
    text.pop_back();
  }
  const float tw = font->CalcTextSizeA(lbl_sz, 1.0e4F, 0.0F, text.c_str()).x;
  const float tx = std::max(p0.x + 3.0F, p0.x + (size - tw) * 0.5F);
  dl->PushClipRect(p0, p1, true);
  // Design: stopped label in the TRACK COLOR, playing label near-white.
  dl->AddText(font, lbl_sz, ImVec2(tx, p1.y - lbl_sz - 3.0F),
              neon::u32(playing ? theme::kText : color, 0.95F), text.c_str());
  dl->PopClipRect();

  // Beat-synchronized L->R playhead on the active scene's own FILLED cell
  // while the transport is running: fills 0->100% over the active scene's
  // SECTION (owner-locked behavior), reaching the right edge exactly at the
  // section boundary -- the same instant auto-song advances. `section_phase
  // < 0` (the sentinel, render_grid_panel's own per-cell resolution) parks
  // it: every non-active scene column, and the active one before bar() > 0.
  // Retired: the former neon::sweep_bar(..., fx.time, ...) call drove this
  // off ImGui's wall-clock frame time with a fixed ~1.7s period, unrelated
  // to tempo or the section length.
  //
  // Song-mode Phase 1 (docs/proposals/song-mode-scenechain-adoption.md,
  // design decision 4): the SceneChain that drives ordinary auto-song play
  // deliberately never touches ClipMatrix, so `playing` (this cell's own
  // real `AppState::clip_state(id) != kStopped` readback, still driving the
  // fill/border/glow/label appearance above -- that "armed/lit" signal stays
  // exactly as real and unchanged) is no longer a usable gate for the
  // playhead: no cell is EVER independently clip-armed by a plain SceneChain
  // advance, so gating the sweep on it made the sweep vanish the instant the
  // active section first moved off whichever single cell a user happened to
  // click by hand -- reintroducing the owner's original "no playhead sweep"
  // bug in a new form (see test_repeat_zone_owner_trace_replay.cpp's own
  // header comment for the full trace). The playhead's own two remaining
  // gates already say everything it needs without `playing`: this call site
  // is only ever reached for a `filled` cell (the `!filled` early return
  // above), and `section_phase` is already resolved by the caller to `>= 0`
  // for ONLY the active scene column's own cells (render_track_cell) -- so
  // `fx.playing && section_phase >= 0.0F` alone means exactly "this is a
  // filled cell in the active auto-song column, and the transport is
  // running", independent of whether ClipMatrix ever armed it.
  if (fx.playing && section_phase >= 0.0F) {
    neon::playhead_at(dl, p0, p1, section_phase, theme::kText);
  }

  // Honesty affordance (repeat-zone-real-contract.md STEP 4): a hover-only
  // tooltip, no new persistent chrome, for a preview that isn't the exact
  // runtime output (resolved against a placeholder harmony, and/or a
  // motif's repeat=0 statement skeleton only).
  if (approx && hovered) {
    ImGui::SetTooltip("approx preview (placeholder harmony, no live chord)");
  }
  return clicked;
}

void render_header(const GridModel& model, UiState& fx, const AppState& app_state,
                   std::size_t scene_count) {
  ImGui::TextColored(theme::kCyan, "REPEAT ZONE");

  // Auto-song toggle (repeat-zone-real-contract.md SLICE 4b, GUI-DRIVEN, no
  // engine mechanism/ABI verb): "un bottone in alto, sempre dentro repeat
  // zone" -- lives right beside the title, before the hint/zoom group.
  // Cyan-lit label "auto-song" when armed, muted "song" otherwise; same
  // transparent-button-with-tinted-text styling as the transport panel's own
  // glow toggle (transport_panel.cpp) so no new chrome is invented. Arming
  // resets `active_scene_start_bar`/`auto_song_last_bar` to the CURRENT bar,
  // so bars-elapsed measures fresh from the moment auto-song was switched on
  // rather than from whatever bar the active scene happened to launch at.
  ImGui::SameLine(0.0F, 10.0F);
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(theme::kCyan.x, theme::kCyan.y, theme::kCyan.z, 0.18F));
  ImGui::PushStyleColor(ImGuiCol_Text, fx.auto_song ? theme::kCyan : theme::kTextMuted);
  if (ImGui::SmallButton(fx.auto_song ? "auto-song" : "song")) {
    fx.auto_song = !fx.auto_song;
    if (fx.auto_song) {
      fx.active_scene_start_bar = app_state.bar();
      fx.auto_song_last_bar = app_state.bar();
    }
  }
  ImGui::PopStyleColor(3);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Auto-song: advance the active scene column at its section boundary (%s)",
                      fx.auto_song ? "on" : "off");
  }

  // Hint + zoom -/+ as one right-aligned group (design: the hint sits next to
  // the zoom control, not beside the title).
  const char* hint = "click = launch + open \xC2\xB7 scene \xE2\x96\xB6 = launch column";
  const float hint_w = ImGui::CalcTextSize(hint).x;
  const float zoom_w = 58.0F;
  ImGui::SameLine();
  ImGui::SetCursorPosX(
      std::max(ImGui::GetCursorPosX(), ImGui::GetContentRegionMax().x - zoom_w - hint_w - 8.0F));
  ImGui::TextColored(theme::kTextMuted, "%s", hint);
  ImGui::SameLine(0.0F, 8.0F);
  if (ImGui::SmallButton("-")) {
    fx.cell_zoom = std::clamp(fx.cell_zoom - kCellZoomStep, kCellZoomMin, kCellZoomMax);
  }
  ImGui::SameLine(0.0F, 4.0F);
  if (ImGui::SmallButton("+")) {
    fx.cell_zoom = std::clamp(fx.cell_zoom + kCellZoomStep, kCellZoomMin, kCellZoomMax);
  }

  // Owner ask: "visualizza in ui che scena sta suonando (metti numero: nome)"
  // -- a subtle status block, its OWN two rows (never disturbing the row
  // above), showing BOTH truths side by side because their disagreement IS
  // the root-cause diagnostic for the "never leaves intro1 audibly" bug: the
  // GUI's own launch intent (fx.active_scene + GridModel::scene_name -- what
  // auto-song/a manual scene launch believes is current) next to the
  // ENGINE's authoritative section readback (AppState::section(), fed by the
  // kSection wire event, app_state.cpp's kSection case). If they ever
  // diverge live, the bug is localized in one glance.
  //
  // FIXED HEIGHT (bug found verifying this task): both rows below are drawn
  // UNCONDITIONALLY -- an invisible placeholder Dummy stands in whenever the
  // real content does not apply (transport stopped / auto-song off) --
  // rather than the row being skipped outright. Skipping it used to make
  // this header's own height (and therefore the Y every scene-header/track
  // row below starts at) DEPEND on fx.playing/fx.auto_song: the instant the
  // transport started, the ENTIRE Repeat-Zone grid visibly jumped down by
  // one text line (and jumped back up on Stop) -- a real, user-visible
  // layout glitch, caught by test_repeat_zone_playhead_ui_automation.cpp
  // (it locates a cell's rect once, before Play, then asserts against it
  // after Play: the shift moved the real cell out from under the stale
  // rect, so the playhead assertion failed even though the playhead WAS
  // being drawn -- just one line lower than the rect it was checked
  // against). Reserving the same two-row footprint in every state removes
  // the shift entirely.
  const std::size_t active_scene_index =
      fx.active_scene >= 0 ? static_cast<std::size_t>(fx.active_scene) : 0;
  const float placeholder_row_h = ImGui::GetTextLineHeight();
  if (fx.playing) {
    const std::string engine_section = app_state.section().empty() || app_state.section() == "-"
                                           ? "\xE2\x80\x94"
                                           : app_state.section();
    ImGui::TextColored(
        theme::kTextSecondary, "\xE2\x96\xB6 %d: %s \xC2\xB7 engine: %s", fx.active_scene + 1,
        std::string(model.scene_name(active_scene_index)).c_str(), engine_section.c_str());
  } else {
    ImGui::Dummy(ImVec2(1.0F, placeholder_row_h));
  }

  // Owner issue (b): "next section" indicator. There is NO authoritative
  // upcoming/queued section on the wire today -- the core only reports the
  // section it has already COMMITTED to (the "kSection" OutEvent the row
  // above's "engine: %s" reads); the arranger's own pending/armed section is
  // not surfaced over the wire at all (confirmed against brain_event.cpp's
  // kSection decode and docs/proposals/ui-motion-extreme-2026-07.md §5.3:
  // "requested-vs-current section needs an 'armed section' field on
  // AppState... not implemented yet"). Rather than fake a readback or
  // silently show a stale echo, this shows the GUI's OWN next-scene INTENT
  // instead -- the same round-robin target auto-song's own next_scene_to_
  // launch() would advance to -- explicitly labelled "next (intent)" so it
  // can never be mistaken for an engine-confirmed fact. Only shown while
  // playing AND auto-song is armed: with auto-song off the active scene
  // never advances on its own, so there genuinely is no "next" to report.
  if (fx.playing && fx.auto_song && scene_count > 0) {
    // Reuse next_scene_to_launch itself (never re-hand-roll the wrap/hold
    // formula a second time here, per song-form-option-a-wiring-plan.md
    // §2.2): pass EQUAL placeholder bars values for the two timing
    // parameters -- the function only ever compares them relatively (has the
    // section "finished"?), so any equal pair asks "what would the decision
    // be if the boundary were reached right now," which is exactly this
    // display's own "next (intent)" question. Under THIS function's own outer
    // guard (auto_song && playing && scene_count > 0), the only way this can
    // return nullopt is the new last-column hold (song-form Option A) -- so
    // "no value" here can only mean "-> Ending," never one of next_scene_to_
    // launch's OTHER nullopt causes (those are already excluded above).
    const std::optional<int> next = next_scene_to_launch(
        fx.auto_song, fx.playing, fx.active_scene, static_cast<int>(scene_count),
        /*bars_elapsed_in_scene=*/1, /*active_scene_section_bars=*/1);
    if (next.has_value()) {
      const std::size_t next_scene_index = static_cast<std::size_t>(*next);
      ImGui::TextColored(theme::kTextDim, "  next (intent): %d: %s",
                         static_cast<int>(next_scene_index) + 1,
                         std::string(model.scene_name(next_scene_index)).c_str());
    } else {
      ImGui::TextColored(theme::kTextDim, "  next (intent): \xE2\x86\x92 Ending");
    }
  } else {
    ImGui::Dummy(ImVec2(1.0F, placeholder_row_h));
  }
}

// The real hold length behind `scene_index`'s column TODAY, in bars. Task #6:
// the per-scene LENGTH STEPPER (render_scene_header_cell's own "- N +"
// control) is now the single authoritative source for how long a scene
// column holds before auto-song advances -- GridModel::scene_bars, user-
// editable, no longer a vestigial/reserved field. This supersedes the
// earlier "SOURCE-OF-TRUTH TRANSITION" (owner task #3, see update_auto_
// song's own header comment for that history) that deliberately moved AWAY
// from scene_bars toward the style's own section length, because at the
// time nothing let the user edit it -- now something does. Shared by
// update_auto_song's threshold and render_grid_panel's own playhead so the
// two can never silently diverge on what "one hold" means.
int active_style_section_bars(const GridModel& model, std::size_t scene_index) {
  return model.scene_bars(scene_index);
}

// Task #5 (per-section REPEAT COUNT, Phase-1: host-only, ZERO ABI/core
// change): translates GridModel::scene_repeat's own value (1..kMaxSceneRepeat
// real counts, or the kSceneRepeatInfinite sentinel) into the `song build`
// wire line's own repeat token grammar -- a decimal string for a real count,
// or the literal `inf` for "hold forever" (in_process_brain_session.cpp's
// wire parser decodes `inf` back into its own kSongBuildRepeatInfinite
// constant, a SEPARATE numeric sentinel by design, D38: that file never
// reaches into grid_model.hpp).
std::string scene_repeat_wire_token(const GridModel& model, std::size_t scene_index) {
  const int repeat = model.scene_repeat(scene_index);
  if (repeat == GridModel::kSceneRepeatInfinite) {
    return "inf";
  }
  return std::to_string(repeat);
}

// Song-mode Phase 1 (docs/proposals/song-mode-scenechain-adoption.md, design
// decision 4): the double section-trigger this function used to send
// (`style section <name>` + `launch scene <n> quantize 1`, fighting each
// other -- see this function's own git history for the full redundancy
// finding) is retired. A manual scene-header click now builds and plays a
// ONE-STEP SceneChain for THIS column alone, through the SAME `song build`
// verb the multi-scene auto-song path uses (build_and_play_song, below) --
// in_process_brain_session.cpp's apply_song_build turns it into
// kSceneClear + one kSceneAdd + kScenePlay. A length-1 chain's own last-step
// HOLD (SceneChain's "last step holds, no implicit loop") degrades exactly
// to "select this section and stay there", the same fixed single-column loop
// the old double-send produced -- core-driven, no ClipMatrix involved.
//
// Task #5: this is also the RESUME gesture out of an infinite hold (apply_
// song_build's own header comment) -- a click here always rebuilds the WHOLE
// chain from scratch as a fresh one-step song, regardless of whatever an
// earlier auto-song build's own repeat/infinite state was doing, so it
// deliberately sends a plain repeat of "1" (not this column's own configured
// repeat -- moot anyway: a one-step chain's last (only) step already holds
// forever on its own, independent of any repeat count).
void activate_scene_column(const GridModel& model, BrainSession& brain_session, UiState& fx,
                           std::size_t scene_index, int current_bar) {
  const std::string_view section_name = section_wire_name(model.scene_section(scene_index));
  const std::string section_arg = section_name.empty() ? "varA" : std::string(section_name);
  brain_session.send("song build 1 " + section_arg + " " +
                     std::to_string(model.scene_bars(scene_index)) + " 1");
  // Shared bookkeeping every call site already needed afterward: the active
  // scene's own beat-synced playhead sweep (render_grid_panel's active_
  // section_phase) anchors from THIS bar, not a stale one.
  fx.active_scene = static_cast<int>(scene_index);
  fx.active_scene_start_bar = current_bar;
}

// Song-mode Phase 1: the ONLY way a multi-scene song is built and started.
// Collects every POPULATED scene column (scene_is_populated, above), in
// column order, into the one `song build <n> <section> <bars> <repeat> ...`
// line in_process_brain_session.cpp's engine thread turns into the full
// kSceneClear + N*kSceneAdd + kScenePlay ABI sequence the observable
// contract requires (song-mode-scenechain-adoption.md's own "Observable
// contract" section) -- see apply_song_build's own header comment for why
// that sequencing must happen on the engine thread and not here (only it can
// reach Engine::performances()). An all-empty grid sends nothing (there is
// no populated column to build a step from). Task #5: each populated
// column's own `<repeat>` token (scene_repeat_wire_token, above) is now part
// of this line -- apply_song_build expands it into that many consecutive
// kSceneAdd steps, or truncates the chain right there for an infinite one.
void build_and_play_song(const GridModel& model, BrainSession& brain_session,
                         std::size_t scene_count) {
  std::vector<std::size_t> populated;
  for (std::size_t s = 0; s < scene_count; ++s) {
    if (scene_is_populated(model, s)) {
      populated.push_back(s);
    }
  }

  if (populated.empty()) {
    return;
  }
  std::string line = "song build " + std::to_string(populated.size());
  for (const std::size_t s : populated) {
    const std::string_view section_name = section_wire_name(model.scene_section(s));
    line += " " + (section_name.empty() ? std::string("varA") : std::string(section_name));
    line += " " + std::to_string(model.scene_bars(s));
    line += " " + scene_repeat_wire_token(model, s);
  }
  brain_session.send(line);
}

// MASTER PLAY (owner issue a): the master ▶ (transport_panel.cpp's own Play
// pad, next to the "sonotron_" wordmark) only ever sent `transport start` --
// audio starts, but no scene column is ever launched, so no cell reads back
// as playing (AppState::clip_state stays kStopped for every cell) and the
// beat-synced playhead sweep (draw_cell's own `playing && fx.playing` gate)
// never appears until the user separately clicks a cell or a scene header.
// `fx.master_play_launched` is the once-per-press guard: cleared the instant
// the transport is observed NOT playing (AppState::transport(), the
// authoritative state), and set the instant this function actually fires --
// so a Play press launches exactly once, never re-firing on every subsequent
// frame while the transport keeps running, and re-arms cleanly on the next
// Stop/Play cycle.
//
// Song-mode Phase 1 (docs/proposals/song-mode-scenechain-adoption.md): the
// launch itself now branches on fx.auto_song, same as before (owner task #4/
// #36), but through the core-driven SceneChain rather than a hand-rolled
// per-frame advance:
//  - auto_song ON: a fresh Play starts the WHOLE song, core-driven end to
//    end, via build_and_play_song -- one `song build ...` line covering
//    every populated scene column in order. fx.active_scene is set to the
//    FIRST populated column (the chain's own step 0) so the header
//    highlight/playhead matches instantly; reconcile_active_scene (below)
//    keeps it in sync with the engine's own section readback from there on.
//  - auto_song OFF: a fixed single-scene loop on whichever column is
//    currently selected -- activate_scene_column builds a ONE-STEP chain
//    that holds there indefinitely (its own header comment).
void handle_master_play_launch(const GridModel& model, BrainSession& brain_session,
                               const AppState& app_state, UiState& fx, std::size_t scene_count) {
  const bool transport_playing_now = app_state.transport() == AppState::Transport::kPlaying;
  if (!transport_playing_now) {
    fx.master_play_launched = false;
    return;
  }
  if (fx.master_play_launched) {
    return;
  }
  fx.master_play_launched = true;

  if (fx.auto_song) {
    if (debug_enabled()) {
      debug_log("[dbg master-play] transport just started -> building song from populated scenes");
    }
    build_and_play_song(model, brain_session, scene_count);
    fx.active_scene = 0;
    for (std::size_t s = 0; s < scene_count; ++s) {
      if (scene_is_populated(model, s)) {
        fx.active_scene = static_cast<int>(s);
        break;
      }
    }
    fx.active_scene_start_bar = app_state.bar();
    return;
  }

  const std::size_t active_scene_index =
      fx.active_scene >= 0 ? static_cast<std::size_t>(fx.active_scene) : 0;
  if (debug_enabled()) {
    debug_log("[dbg master-play] transport just started -> launching active scene " +
              std::to_string(active_scene_index));
  }
  activate_scene_column(model, brain_session, fx, active_scene_index, app_state.bar());
}

// Song-mode Phase 1 (docs/proposals/song-mode-scenechain-adoption.md, design
// decision 4): replaces update_auto_song's own per-frame FSM. The core
// SceneChain now drives the actual advance and the host-cued Ending entirely
// on the engine thread (in_process_brain_session.cpp's run_engine, watching
// scenes().playing()'s own true->false edge) -- this function's only
// remaining job is keeping fx.active_scene (the GUI's OWN highlight/playhead
// anchor, render_scene_header_cell's name-color tint and render_grid_panel's
// beat-synced playhead) in sync with the engine's authoritative section
// readback (app_state.section(), fed by the kSection OutEvent), exactly the
// same divergence render_header's own "engine: %s" row already surfaces.
//
// Stays put if the CURRENT column's own section already matches (no
// spurious jump on a frame where nothing changed); otherwise picks the
// FIRST populated column, in order, whose section matches -- correct as
// long as no two populated columns share a SectionType (build_and_play_song
// builds the chain from these same columns in this same order, so this is
// the chain's own step order). While an Ending plays out (or once the song
// has ended), the readback becomes "ending1"/holds at the last real
// section, which matches no column -- this simply leaves fx.active_scene
// parked at its last known value, which is the wanted behavior.
void reconcile_active_scene(const GridModel& model, const AppState& app_state, UiState& fx,
                            std::size_t scene_count) {
  if (!fx.playing || scene_count == 0) {
    return;
  }
  const std::size_t current = fx.active_scene >= 0 ? static_cast<std::size_t>(fx.active_scene) : 0;
  if (current < scene_count &&
      section_wire_name(model.scene_section(current)) == app_state.section()) {
    return;
  }
  for (std::size_t s = 0; s < scene_count; ++s) {
    if (scene_is_populated(model, s) &&
        section_wire_name(model.scene_section(s)) == app_state.section()) {
      fx.active_scene = static_cast<int>(s);
      fx.active_scene_start_bar = app_state.bar();
      return;
    }
  }
}

// Standard solo semantics: any part soloed makes the non-soloed rows read as
// muted (dimmed). Derived from the shared PartsModel (same state the rail
// mute/solo edits), so both surfaces stay consistent.
bool any_part_soloed(const PartsModel& parts) {
  for (std::size_t i = 0; i < PartsModel::kPartCount; ++i) {
    if (parts.part(i).soloed) {
      return true;
    }
  }
  return false;
}

// One scene-header column: the plain-text "name ▶" launch head + cyan
// underline (design has no button pill), OR -- while renaming -- an inline
// ImGui InputText (repeat-zone-real-contract.md §4/§8b decision 3, OWNER
// LOCKED to host-only GridModel storage + scenes.json, no core touch) so the
// column can be renamed; commit on Enter/focus-loss, Esc cancels. Also the
// drop target (SLICE 4a item 4): a "variations" row dragged from the browser
// (kVariationDragPayloadId, browser_panel.cpp) sets THIS column's SectionType
// through GridModel -- host-side state only, no wire send here (the section
// only takes musical effect once the header's own ▶ below applies it, or a
// launch happens). Deliberately minimal per owner judgment: the scene's
// stored NAME is left untouched by a drop -- only the section changes.
void render_scene_header_cell(GridModel& model, BrainSession& brain_session,
                              const AppState& app_state, UiState& fx, std::size_t s, float cz,
                              float header_h) {
  ImGui::SameLine(0.0F, kCellGap);
  ImGui::PushID(static_cast<int>(s));
  const ImVec2 hp0 = ImGui::GetCursorScreenPos();
  const float font_size = kSceneNameFontSize * cell_font_scale(cz);
  draw_scene_header_column_highlight(fx, s, hp0, cz, header_h);

  if (fx.renaming_scene == static_cast<int>(s)) {
    // Inline rename in progress for THIS scene column: an InputText
    // replaces the header, occupying the same cz x header_h footprint the
    // InvisibleButton takes in the non-editing branch below.
    ImGui::SetCursorScreenPos(hp0);
    ImGui::SetNextItemWidth(cz);
    if (fx.rename_focus_pending) {
      ImGui::SetKeyboardFocusHere();
      fx.rename_focus_pending = false;
    }
    ImGui::InputText("##rename", fx.rename_buffer.data(), fx.rename_buffer.size());
    const bool escape_pressed = ImGui::IsKeyPressed(ImGuiKey_Escape);
    if (ImGui::IsItemDeactivated()) {
      if (!escape_pressed) {
        // Commit on Enter or on any other focus-loss (click elsewhere);
        // Escape (checked above) skips the write-back, so the header
        // reverts to its previously stored name.
        model.set_scene_name(s, std::string(fx.rename_buffer.data()));
      }
      fx.renaming_scene = -1;
    }
    // Reserve the rest of the non-editing header's height so the cell row
    // below never shifts while a rename is in progress.
    ImGui::Dummy(ImVec2(cz, std::max(0.0F, header_h - ImGui::GetFrameHeight())));
  } else {
    // Task #6 (+ task #5 below): reserve the bottom TWO kStepperRowH strips
    // -- the always-visible "- N +" length stepper, then the "- K +" REPEAT
    // stepper right below it -- and shrink the launch hit-region to the area
    // ABOVE both -- the SAME two-separate-hit-regions fix render_track_label
    // already applies for its own M/S latches below the track name, so
    // clicking either stepper can never also fire activate_scene_column via
    // the "head" InvisibleButton below.
    const float name_area_h = std::max(0.0F, header_h - kStepperRowH * 2.0F);
    const bool go = ImGui::InvisibleButton("head", ImVec2(cz, name_area_h));
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kVariationDragPayloadId)) {
        const std::uint8_t section = *static_cast<const std::uint8_t*>(payload->Data);
        model.set_scene_section(s, section);
      }
      ImGui::EndDragDropTarget();
    }
    const bool double_clicked =
        ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    const std::string name(model.scene_name(s));
    ImDrawList* hdl = ImGui::GetWindowDrawList();
    ImFont* font = ImGui::GetFont();
    // Owner: smaller (kSceneNameFontSize), word-wrapped, multi-line name so
    // a longer/renamed scene name fits within the column instead of
    // overflowing it at the default font size. The green launch caret sits
    // in its own fixed top-right corner (decoupled from the wrapped text's
    // own width, which can now span several lines).
    const std::vector<std::string> lines = wrap_scene_name(font, font_size, name, cz - 6.0F);
    const float line_h = font->CalcTextSizeA(font_size, 1.0e4F, 0.0F, "Ag").y;
    // SLICE 4b: the header's own text tint doubles as the "active scene"
    // indicator -- was hardcoded to column 0; now tracks fx.active_scene,
    // which auto-song's advance AND a manual scene launch both update, so
    // the highlight reflects whichever column is really current.
    const ImU32 name_color =
        neon::u32(static_cast<int>(s) == fx.active_scene ? theme::kText : theme::kTextSecondary);
    for (std::size_t i = 0; i < lines.size(); ++i) {
      hdl->AddText(font, font_size,
                   ImVec2(hp0.x + 3.0F, hp0.y + 3.0F + static_cast<float>(i) * line_h), name_color,
                   lines[i].c_str());
    }
    const char* caret = "\xE2\x96\xB6";
    const ImVec2 caret_ts = font->CalcTextSizeA(font_size, 1.0e4F, 0.0F, caret);
    hdl->AddText(font, font_size, ImVec2(hp0.x + cz - caret_ts.x - 3.0F, hp0.y + 3.0F),
                 neon::u32(theme::kGreen), caret);
    const float uy = hp0.y + name_area_h - 2.0F;
    hdl->AddLine(ImVec2(hp0.x, uy), ImVec2(hp0.x + cz, uy), neon::u32(theme::kCyan, 0.25F), 2.0F);

    // Task #6: the always-visible "- N +" length stepper, in the strip
    // reserved below the name/caret/underline -- its own separate
    // InvisibleButtons (stepper_button, above), never overlapping "head"'s
    // own hit region. model.set_scene_bars' own clamp (GridModel::
    // kMaxSceneBars) handles the 1..8 bound; no re-clamping needed here.
    const float stepper_y = hp0.y + name_area_h;
    const int bars = model.scene_bars(s);
    if (stepper_button("-", ImVec2(hp0.x, stepper_y), font_size)) {
      model.set_scene_bars(s, bars - 1);
    }
    const std::string bars_text = std::to_string(bars);
    const ImVec2 bars_ts = font->CalcTextSizeA(font_size, 1.0e4F, 0.0F, bars_text.c_str());
    hdl->AddText(
        font, font_size,
        ImVec2(hp0.x + cz * 0.5F - bars_ts.x * 0.5F, stepper_y + (kStepperRowH - bars_ts.y) * 0.5F),
        neon::u32(theme::kText), bars_text.c_str());
    if (stepper_button("+", ImVec2(hp0.x + cz - kStepperBtnW, stepper_y), font_size)) {
      model.set_scene_bars(s, bars + 1);
    }

    // Task #5: the always-visible "- K +" REPEAT-COUNT stepper, one row
    // below the bars stepper -- same layout formula, own InvisibleButton IDs
    // (PushID("repeat") keeps "-"/"+" here from colliding with the bars
    // stepper's own "-"/"+" IDs above, both scoped inside this cell's outer
    // PushID(s)). Displays "\xE2\x88\x9E" (infinity) once the value reaches
    // GridModel::kSceneRepeatInfinite -- model.set_scene_repeat's own clamp
    // handles the [1, kSceneRepeatInfinite] bound; no re-clamping needed
    // here, matching the bars stepper's own discipline.
    ImGui::PushID("repeat");
    const float repeat_stepper_y = stepper_y + kStepperRowH;
    const int repeat = model.scene_repeat(s);
    if (stepper_button("-", ImVec2(hp0.x, repeat_stepper_y), font_size)) {
      model.set_scene_repeat(s, repeat - 1);
    }
    const std::string repeat_text =
        repeat == GridModel::kSceneRepeatInfinite ? "\xE2\x88\x9E" : std::to_string(repeat);
    const ImVec2 repeat_ts = font->CalcTextSizeA(font_size, 1.0e4F, 0.0F, repeat_text.c_str());
    hdl->AddText(font, font_size,
                 ImVec2(hp0.x + cz * 0.5F - repeat_ts.x * 0.5F,
                        repeat_stepper_y + (kStepperRowH - repeat_ts.y) * 0.5F),
                 neon::u32(theme::kText), repeat_text.c_str());
    if (stepper_button("+", ImVec2(hp0.x + cz - kStepperBtnW, repeat_stepper_y), font_size)) {
      model.set_scene_repeat(s, repeat + 1);
    }
    ImGui::PopID();

    if (double_clicked) {
      fx.renaming_scene = static_cast<int>(s);
      std::snprintf(fx.rename_buffer.data(), fx.rename_buffer.size(), "%s", name.c_str());
      fx.rename_focus_pending = true;
    } else if (go) {
      // SLICE 4a item 5 / owner task #1: the header's PRIMARY job is
      // applying this column's own SectionType and firing its clips --
      // activate_scene_column (shared with the master-Play one-shot launch
      // and auto-song's own advance, see its own header comment for the full
      // redundancy finding against the arranger) sends `style section` +
      // `launch scene ... quantize` together and updates fx's own
      // active-scene bookkeeping. Real per-cell readback (app_state.
      // clip_state) reports the launched state on the NEXT poll(), so no
      // local echo is written here -- see render_grid_panel's own header
      // comment.
      //
      // SLICE 4b: a manual scene-column launch is also a real launch of
      // this scene, so it becomes auto-song's own "active scene" baseline
      // -- auto-song, if later armed (or already armed), measures
      // bars-elapsed from THIS launch, not a stale one (activate_scene_
      // column's own bookkeeping covers this).
      activate_scene_column(model, brain_session, fx, s, app_state.bar());
    }

    // Restore the row's Y anchor (the same trick render_track_label already
    // uses for its own M/S latches): the stepper buttons above repositioned
    // the cursor via SetCursorScreenPos, not SameLine, so ImGui's own
    // "current line" bookkeeping would otherwise drift below hp0 for the
    // NEXT scene column's own SameLine() call. Resetting to hp0 and
    // consuming the FULL header_h here keeps every column's header starting
    // at the identical Y, exactly like the lone "head" InvisibleButton alone
    // used to guarantee before this stepper existed.
    ImGui::SetCursorScreenPos(hp0);
    ImGui::Dummy(ImVec2(cz, header_h));
  }
  ImGui::PopID();
}

// Scene header row: a spacer over the label column, then one launch head per
// scene column (see render_scene_header_cell). `header_h` is computed ONCE
// here from the LONGEST wrapped name among every visible column, and shared
// by all of them -- every column must use the SAME header height, or the
// track rows below would start at a different Y per column. Task #6 adds one
// fixed extra `kStepperRowH` to this shared height, for the always-visible
// "- N +" length stepper every column now reserves below its own name/caret/
// underline (render_scene_header_cell); task #5 adds a SECOND `kStepperRowH`
// for the "- K +" repeat-count stepper right below it -- both reserved
// uniformly here, same as the wrapped-name height above, so neither ever
// shifts per-column either.
void render_scene_header_row(GridModel& model, BrainSession& brain_session,
                             const AppState& app_state, UiState& fx, std::size_t scenes, float cz) {
  ImFont* font = ImGui::GetFont();
  const float font_size = kSceneNameFontSize * cell_font_scale(cz);
  std::size_t max_lines = 1;
  for (std::size_t s = 0; s < scenes; ++s) {
    const std::string name(model.scene_name(s));
    const std::vector<std::string> lines = wrap_scene_name(font, font_size, name, cz - 6.0F);
    max_lines = std::max(max_lines, lines.size());
  }
  const float line_h = font->CalcTextSizeA(font_size, 1.0e4F, 0.0F, "Ag").y;
  const float header_h =
      std::max(cz * 0.5F, static_cast<float>(max_lines) * line_h + 6.0F) + kStepperRowH * 2.0F;

  ImGui::Dummy(ImVec2(kLabelColWidth, header_h));
  for (std::size_t s = 0; s < scenes; ++s) {
    render_scene_header_cell(model, brain_session, app_state, fx, s, cz, header_h);
  }
}

// Label cell for one track row: color dot + name on top, M/S latches below,
// all left of the row's launch cells. M/S latches wire the REAL `part <role>
// mute|solo on/off` L1 verb through `parts` -- they share PartsModel state
// with the rail mute/solo, and drive the standard solo-implies-others-muted
// dim here. Owner: the pair used to sit inline with the name at 13px, too
// small/cramped to read or tell apart -- stacking the name above the
// latches gives both rows their own full-width space for a bigger, clearly
// legible S/M pair.
void render_track_label(PartsModel& parts, BrainSession& brain_session, const GridRow& row,
                        bool any_solo, const ImVec4& color, UiState& fx, float cz) {
  const std::size_t role = row.role_index;
  const PartInfo& info = parts.part(role);
  const bool dim = (any_solo && !info.soloed) || info.muted;
  const ImVec2 lp = ImGui::GetCursorScreenPos();
  ImDrawList* dl = ImGui::GetWindowDrawList();

  // Row 1 (top): a 7x7 track-colored SQUARE (glow, design) + the track name
  // in the TRACK COLOR (design), sharing one text-line-height row. Kept
  // tight (minimal padding) so the stacked M/S row below still fits within
  // `cz` even at the smallest zoom (render_header's -/+ clamp bottoms out
  // at 34px).
  // Task #4: the role label used to render at a fixed size, ignoring
  // cell_zoom -- the same cell_font_scale() ramp the scene-header name and
  // the cell's own bottom label already follow now applies here too, so
  // drums/bass/chord/pad/arp/lead grow alongside the cells instead of
  // staying static while everything around them zooms.
  ImFont* font = ImGui::GetFont();
  const float name_font_size = ImGui::GetFontSize() * cell_font_scale(cz);
  const float name_row_h = name_font_size + 1.0F;
  const float name_cy = lp.y + name_row_h * 0.5F;
  const ImVec2 d0(lp.x + 2.0F, name_cy - 3.5F);
  const ImVec2 d1(lp.x + 9.0F, name_cy + 3.5F);
  neon::glow_rect(dl, d0, d1, color, 1.0F, dim ? 0.4F : 1.0F, fx.glow && !dim);
  dl->AddRectFilled(d0, d1, neon::u32(color, dim ? 0.4F : 1.0F), 1.0F);
  dl->AddText(font, name_font_size, ImVec2(lp.x + 14.0F, name_cy - name_font_size * 0.5F),
              neon::u32(dim ? theme::kTextMuted : color), row.name);

  // Row 2 (below the name): the M / S latches -- bigger, explicit-letter
  // squares (kLatchSize), unambiguously lit (tone fill) when engaged.
  const std::string token(parts.part_wire_token(role));
  const float latch_y = lp.y + name_row_h + kLatchGap;
  ImGui::SetCursorScreenPos(ImVec2(lp.x + 2.0F, latch_y));
  if (grid_latch("M", info.muted, theme::kPink)) {
    const bool was = info.muted;
    parts.toggle_mute(role);
    brain_session.send("part " + token + " mute " + (!was ? "on" : "off"));
  }
  ImGui::SetCursorScreenPos(ImVec2(lp.x + 2.0F + kLatchSize + kLatchGap, latch_y));
  if (grid_latch("S", info.soloed, theme::kAmber)) {
    const bool was = info.soloed;
    parts.toggle_solo(role);
    brain_session.send("part " + token + " solo " + (!was ? "on" : "off"));
  }
  ImGui::SetCursorScreenPos(lp);
  ImGui::Dummy(ImVec2(kLabelColWidth, cz));
}

// Task #11 Phase 1 (Sequence Edit step sequencer, roadmap node 11600/11610):
// extracted out of render_track_cell (readability-function-cognitive-
// complexity), pure refactor, no behavior change. A DISTINCT gesture from
// the plain click (which stays a deliberate no-op on an empty cell, owner
// bug #2) -- right-click an EMPTY cell to create a real step-track clip
// here. Pre-mutes the new track IMMEDIATELY (before it is ever registered as
// a clip): Timeline fires every registered track unconditionally, and
// Engine::apply_clip_content is the ONLY place that un-mutes on launch /
// re-mutes on stop (in_process_brain_session.cpp's own `track new` comment)
// -- an unmuted brand-new track would otherwise sound the instant the
// transport is already running. A full track pool (StepPatternStore::
// create_track returning -1) is a silent no-op, same "never crash on a full
// pool" discipline ClipMatrix/Timeline themselves use core-side.
void try_create_step_track_on_empty_cell(GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
                                         BrainSession& brain_session, const GridRow& row,
                                         std::size_t s, std::size_t id) {
  const std::size_t role = row.role_index;
  if (role >= kTrackRoleCount) {
    return;
  }
  const int track_idx =
      seqedit.step_tracks().create_track(role, kStepTrackPort, kStepTrackChannel[role]);
  if (track_idx < 0) {
    return;
  }
  const std::string role_token = std::string(parts.part_wire_token(role));
  brain_session.send("track new " + role_token + " " + std::to_string(kStepTrackPort) + " " +
                     std::to_string(kStepTrackChannel[role]));
  brain_session.send("track mute " + std::to_string(track_idx) + " on");
  brain_session.send("clip add " + role_token + " " + std::to_string(s) + " track " +
                     std::to_string(track_idx) + " id " + std::to_string(id));
  model.set_cell(row.role_index, s, GridCellKind::kStepTrack, "step", -1, track_idx);
}

// One launch cell for a track row: computes its real preview content, draws
// it (draw_cell), and handles both click (launch+open a filled cell, or fill
// an empty one with a local demo clip) and the browser style drag-drop
// target that registers a real ClipMatrix clip. `active_scene`/`active_
// section_phase` are render_grid_panel's own once-per-frame playhead
// resolution (grid_model.hpp's section_playhead_phase); only THIS column,
// when `s == active_scene`, actually draws it -- every other column passes
// draw_cell the "no playhead" sentinel, since only the active scene's own
// bookkeeping (UiState::active_scene_start_bar) is beat-synced to a real
// section boundary.
void render_track_cell(GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
                       BrainSession& brain_session, const AppState& app_state, UiState& fx,
                       const GridRow& row, std::size_t r, std::size_t s, const ImVec4& color,
                       float cz, int active_scene, float active_section_phase) {
  ImGui::SameLine(0.0F, kCellGap);
  const std::size_t id = cell_id(row.role_index, s, model.scene_count());
  const GridCell& cell = model.cell(row.role_index, s);
  const bool filled = cell.kind != GridCellKind::kEmpty;
  // Real per-cell readback (repeat-zone-real-contract.md §3): armed,
  // playing, and queued-stop all read as "lit" -- the same immediate
  // feedback the former click-time local echo gave, now backed by the
  // core's own "clip" OutEvent instead of a guess.
  const AppState::ClipLaunchState launch_state = app_state.clip_state(static_cast<int>(id));
  const bool playing = launch_state != AppState::ClipLaunchState::kStopped;
  const bool opened = fx.open_cell == static_cast<int>(id);
  // Real content (repeat-zone-real-contract.md "cell preview made real"): the
  // SAME preview_for(...) the opened cell's Sequence Edit canvas uses, keyed
  // by this cell's own role, the (today: single, browser-highlight) active
  // style, AND (SLICE 4a) THIS COLUMN's own SectionType (model.scene_section
  // (s)) instead of a hardcoded kVarA -- so a column carrying e.g. kVarB
  // previews visibly different content from one still at the kVarA default.
  // Empty cells never draw a preview, so this is only computed for a filled
  // one.
  const TrackCellPreview cell_preview =
      resolve_track_cell_preview(model, seqedit, row, s, cell, filled, fx.active_style);
  const neon::ClipPattern& pattern = cell_preview.pattern;
  const bool approx = cell_preview.approx;
  const float section_phase = static_cast<int>(s) == active_scene ? active_section_phase : -1.0F;
  ImGui::PushID(static_cast<int>(s));
  // Feature B: whole-column highlight -- captured BEFORE draw_cell (its own
  // InvisibleButton consumes/advances the cursor), so this cell's top-left
  // is known for the highlight ring drawn AFTER draw_cell below.
  const ImVec2 cell_p0 = ImGui::GetCursorScreenPos();
  const bool clicked = draw_cell("cell", cz, filled, cell.label, color, pattern, approx, playing,
                                 opened, fx, section_phase);
  if (fx.open_scene == static_cast<int>(s)) {
    // A BORDER, not a fill: draw_cell already paints an opaque (filled or
    // empty) background, which would hide a highlight drawn underneath --
    // this ring is drawn ON TOP instead, so it stays visible either way.
    ImGui::GetWindowDrawList()->AddRect(cell_p0, ImVec2(cell_p0.x + cz, cell_p0.y + cz),
                                        neon::u32(theme::kCyan, 0.55F), 7.0F, 0, 2.0F);
  }
  if (clicked) {
    if (!filled) {
      // Owner bug #2: clicking an EMPTY cell used to fabricate a fake
      // placeholder clip ("clip", kStyleSection) from the click alone -- no
      // launch, no verb, no real ClipMatrix registration behind it, just a
      // ghost that appeared in the grid. An empty-cell click is now a
      // genuine no-op; the only real way to fill a cell is the browser
      // style drag-drop target below, which registers a real ClipMatrix
      // clip.
    } else {
      // Filled -> real launch + open in Sequence Edit. The launched
      // state itself is read back for real (above), not echoed locally.
      brain_session.send("launch clip " + std::to_string(id) + " quantize " +
                         std::to_string(kDefaultLaunchQuantizeBars));
      fx.open_cell = static_cast<int>(id);
      fx.open_row = static_cast<int>(r);
      fx.open_audio = row.audio;
      // SLICE 4a: seqedit_panel.cpp's own preview_for() call needs THIS
      // column's SectionType (parity with the cell preview above).
      fx.open_section = model.scene_section(s);
      // Feature B (docs/proposals/seqedit-column-view-and-zoom.md): the
      // whole COLUMN is now the unit of selection -- stash it so the grid
      // can highlight every cell in this column, and flag a WAV/audio cell
      // (GridCellKind::kLoopBuffer -- nothing creates one yet, Phase 7
      // Looper) so Sequence Edit knows to show the waveform branch instead
      // of the piano-roll overlay.
      fx.open_scene = static_cast<int>(s);
      fx.open_wav = cell.kind == GridCellKind::kLoopBuffer;
      // Task #11 Phase 1: the SeqEditModel-side counterpart of fx.open_wav
      // above (ui_state.hpp itself is out of scope for this task) -- tells
      // render_seqedit_panel's own "step" tab/canvas whether THIS cell has
      // real StepPatternStore content to show.
      seqedit.set_open_step_track(cell.kind == GridCellKind::kStepTrack ? cell.step_track_index
                                                                        : -1);
      seqedit.set_part_index(row.role_index);
      seqedit.set_clip_label(cell.label);
      // Owner bug #1: cell-open used to SOLO the clicked role (only its
      // checkbox started ON, every other role hidden) -- Sequence Edit is
      // supposed to show every track by default. All roles now start
      // visible; the per-role checkboxes and the bulk "all tracks" toggle
      // still work exactly as before for narrowing the view afterwards.
      seqedit.set_all_tracks_visible(true);
    }
  }
  // Task #11 Phase 1 (Sequence Edit step sequencer, roadmap node 11600/
  // 11610): a DISTINCT gesture from the plain click above (which stays a
  // deliberate no-op on an empty cell, owner bug #2) -- right-click an EMPTY
  // cell to create a real step-track clip here. Checked AFTER draw_cell's
  // own InvisibleButton is still this scope's "last item" (draw_cell only
  // issues raw draw-list calls after it, never another ImGui widget), so
  // IsItemClicked(Right) validly targets the SAME cell region the left-click
  // check above already used. See try_create_step_track_on_empty_cell above
  // for the pre-mute rationale.
  if (!filled && ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
    try_create_step_track_on_empty_cell(model, seqedit, parts, brain_session, row, s, id);
  }
  // Drop target: a browser style drag fills this cell for real AND
  // registers a real ClipMatrix clip at this cell's own stable id
  // (repeat-zone-real-contract.md §3/§8b decision 1, Shape A) -- so a
  // later `launch clip <id>` actually addresses THIS cell's material
  // instead of an empty pool slot. Owner decision 2: drag-a-style is the
  // only content source real for this pass. FLAG-1 FIX (slice 4a's own
  // flag, folded into slice 4b): the registered clip now references THIS
  // COLUMN's own SectionType (model.scene_section(s)) instead of a
  // hardcoded kVarA, so launching this cell and launching its column
  // header both target the SAME section -- no latent misalignment
  // between the two launch paths. Falls back to "varA" only if the
  // column's stored section byte is somehow out of range (defensive;
  // set_scene_section() already keeps it in-range).
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kStyleDragPayloadId)) {
      const std::size_t style_index = *static_cast<const std::size_t*>(payload->Data);
      model.set_cell(row.role_index, s, GridCellKind::kStyleSection,
                     std::string(kBuiltinStyleNames[style_index]));
      const std::string_view cell_section_name = section_wire_name(model.scene_section(s));
      const std::string cell_section_arg =
          cell_section_name.empty() ? "varA" : std::string(cell_section_name);
      brain_session.send("clip add " + std::string(parts.part_wire_token(row.role_index)) + " " +
                         std::to_string(s) + " style " + cell_section_arg + " id " +
                         std::to_string(id));
    }
    ImGui::EndDragDropTarget();
  }
  ImGui::PopID();
}

// One full track row: the label cell (render_track_label) plus its launch
// cells (render_track_cell), one per scene column. `active_scene`/`active_
// section_phase` thread render_grid_panel's once-per-frame playhead
// resolution down to each cell (see render_track_cell's own comment).
void render_track_row(GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
                      BrainSession& brain_session, const AppState& app_state, UiState& fx,
                      std::size_t r, std::size_t scenes, bool any_solo, float cz, int active_scene,
                      float active_section_phase) {
  const GridRow& row = kRows[r];
  const ImVec4& color = theme::kTrackColor[r];

  ImGui::PushID(static_cast<int>(100 + r));
  render_track_label(parts, brain_session, row, any_solo, color, fx, cz);
  for (std::size_t s = 0; s < scenes; ++s) {
    render_track_cell(model, seqedit, parts, brain_session, app_state, fx, row, r, s, color, cz,
                      active_scene, active_section_phase);
  }
  ImGui::PopID();
}

}  // namespace

void render_grid_panel(GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
                       BrainSession& brain_session, const AppState& app_state, UiState& fx) {
  seed_demo(model, seqedit, parts, brain_session, fx);
  // Capped scene count (the launch grid only ever renders the first 5
  // scene columns, render_scene_header_row/render_track_row below) computed
  // ONCE, up front, so render_header's own "next (intent)" preview and the
  // update_auto_song/render_* calls further down all share the exact same
  // wrap-around modulus -- never a second, silently-divergent literal.
  const std::size_t scenes = std::min<std::size_t>(model.scene_count(), 5);
  render_header(model, fx, app_state, scenes);
  ImGui::Spacing();

  // Standard solo semantics: any part soloed makes the non-soloed rows read as
  // muted (dimmed). Derived from the shared PartsModel (same state the rail
  // mute/solo edits), so both surfaces stay consistent.
  const bool any_solo = any_part_soloed(parts);

  const float cz = fx.cell_zoom;

  // MASTER PLAY (issue a): if the transport just started, launch the active
  // scene column (or, with auto-song armed, the whole song) so its clips
  // read back as playing and the beat-synced sweep starts -- BEFORE
  // reconcile_active_scene, mirroring SLICE 4b's own "evaluate before
  // drawing the header" ordering so a fired launch is reflected in the SAME
  // frame's active-scene highlight.
  handle_master_play_launch(model, brain_session, app_state, fx, scenes);

  // Song-mode Phase 1: keep fx.active_scene in sync with the engine's own
  // section readback BEFORE drawing the scene header row below, so a real
  // SceneChain advance is reflected in the SAME frame's active-scene
  // highlight rather than lagging one frame behind.
  reconcile_active_scene(model, app_state, fx, scenes);

  // Beat-synchronized playhead (owner-locked: "the playhead fills 0->100%
  // over the SECTION"). SECOND source-of-truth transition (task #6, mirrors
  // update_auto_song's own header comment): this read model.scene_bars()
  // originally, then moved to the style's own real section length (owner
  // task #3, since scene_bars() had no editor and was a disconnected flat
  // default); it now reads active_style_section_bars(), which itself reads
  // model.scene_bars() again -- the SAME per-scene value the always-visible
  // stepper (render_scene_header_cell) writes, and the SAME value update_
  // auto_song's own advance threshold is built from, so the sweep still
  // always reaches the cell edge at exactly the bar auto-song advances.
  //
  // With no repeat multiplier anymore (task #6 dropped kDefaultSectionRepeats,
  // see update_auto_song's own header comment), one hold is exactly one
  // 0->100% sweep -- repeat_cycle_start_bar (grid_model.hpp) degrades
  // gracefully to a single-cycle pass-through in this case (there is only
  // ever one "repeat" per hold now), so it needs no code change here, only
  // this updated framing.
  const std::size_t active_scene_index =
      fx.active_scene >= 0 ? static_cast<std::size_t>(fx.active_scene) : 0;
  const int active_section_bars = active_style_section_bars(model, active_scene_index);
  const int repeat_start_bar =
      repeat_cycle_start_bar(app_state.bar(), fx.active_scene_start_bar, active_section_bars);
  const float active_section_phase =
      section_playhead_phase(app_state.bar(), repeat_start_bar, app_state.beat_num(),
                             app_state.pulse(), app_state.beats_per_bar(), active_section_bars);

  ImGui::BeginChild("grid_body", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_None);

  render_scene_header_row(model, brain_session, app_state, fx, scenes, cz);

  // Track rows.
  for (std::size_t r = 0; r < kRows.size(); ++r) {
    render_track_row(model, seqedit, parts, brain_session, app_state, fx, r, scenes, any_solo, cz,
                     fx.active_scene, active_section_phase);
  }

  ImGui::EndChild();
}

}  // namespace sonotron
