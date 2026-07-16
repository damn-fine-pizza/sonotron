#include "grid_panel.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app_state.hpp"
#include "browser_model.hpp"
#include "imgui.h"
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

// The 6 v02 launch-grid rows (spec §2b), each mapped to a GridModel part-row
// index (so a launched cell addresses a real, stable ClipMatrix slot) and a
// track color. `audio` marks the pad row as the design's one "audio" row --
// it no longer selects a different preview widget (repeat-zone-real-
// contract.md STEP 3: pad has no real audio content yet, so it shows its
// real MIDI note pattern too, like every other row); `audio` is kept only
// for `fx.open_audio` bookkeeping, reserved for genuine future audio
// content. The GridModel has 9 role rows; these are the 6 the v02 grid
// surfaces.
struct V02Row {
  const char* name;
  std::size_t role_index;  // into GridModel / track_roles
  bool audio;
};
constexpr std::array<V02Row, 6> kRows = {{
    {"drums", 0, false},
    {"bass", 2, false},
    {"chord", 3, false},
    {"pad", 5, true},
    {"arp", 6, false},
    {"lead", 8, false},
}};

std::size_t cell_id(std::size_t role_index, std::size_t scene, std::size_t scene_count) {
  return role_index * scene_count + scene;
}

// Seeds a scattered demo clip pattern on the first frame so the procedural
// previews have content (same local-content path a browser drag uses;
// launching still sends real verbs). Deterministic labels -> deterministic
// previews.
void seed_demo(GridModel& model, SeqEditModel& seqedit, V02State& fx) {
  if (fx.seeded) {
    return;
  }
  fx.seeded = true;
  // Match the v02 design's default clip set + short curated labels exactly
  // (Sonotron v02 Workstation.dc.html) so the launch grid reads like the ref
  // (A/B/fil, wlk/sub, cmp/stab/out, swl, up/up2, vox/ld/end) — no truncation.
  struct DemoCell {
    std::size_t row;
    std::size_t scene;
    const char* label;
  };
  static constexpr std::array<DemoCell, 15> pattern = {{
      {0, 0, "A"},
      {0, 1, "B"},
      {0, 3, "fil"},
      {1, 0, "wlk"},
      {1, 2, "sub"},
      {2, 0, "cmp"},
      {2, 1, "stab"},
      {2, 4, "out"},
      {3, 1, "swl"},
      {3, 3, "swl"},
      {4, 0, "up"},
      {4, 2, "up2"},
      {5, 1, "vox"},
      {5, 2, "ld"},
      {5, 4, "end"},
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

  // Open the bass 'wlk' clip by default — the design's initial openAt {r:1,c:0}
  // — so Sequence Edit shows a populated (blue) piano-roll, not the empty hint.
  fx.open_row = 1;
  fx.open_cell = static_cast<int>(cell_id(kRows[1].role_index, 0, model.scene_count()));
  fx.open_audio = kRows[1].audio;
  fx.open_section = model.scene_section(0);
  seqedit.set_part_index(kRows[1].role_index);
  seqedit.set_clip_label("wlk");
}

// Draws one launch cell (custom draw-list) at the cursor; returns true on
// click. `filled` cells show a preview + label + (when playing) a beat-
// synced playhead. `pattern`/`approx` are the cell's REAL resolved content
// (preview::preview_for, computed by the caller) and its honesty flag
// (repeat-zone-real-contract.md STEP 4) -- ignored when `!filled`.
// `section_phase` is this cell's own playhead position in [0,1] (grid_
// model.hpp's section_playhead_phase, resolved by the caller ONLY for the
// active scene column's own cells), or the sentinel < 0 ("no playhead" --
// every other scene column, and the active one before the transport ever
// starts) computed once per frame by the caller (render_grid_panel), not
// here -- draw_cell stays a pure draw/click primitive with no bookkeeping of
// its own.
bool draw_cell(const char* id, float size, bool filled, const std::string& label,
               const ImVec4& color, const neon::ClipPattern& pattern, bool approx, bool playing,
               bool opened, const V02State& fx, float section_phase) {
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
  const float lbl_sz = 11.0F;
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

  // Beat-synchronized L->R playhead on the active scene's own playing cell
  // while running: fills 0->100% over the active scene's SECTION (owner-
  // locked behavior), reaching the right edge exactly at the section
  // boundary -- the same instant auto-song advances. `section_phase < 0`
  // (the sentinel, render_grid_panel's own per-cell resolution) parks it:
  // every non-active scene column, and the active one before bar() > 0.
  // Retired: the former neon::sweep_bar(..., fx.time, ...) call drove this
  // off ImGui's wall-clock frame time with a fixed ~1.7s period, unrelated
  // to tempo or the section length.
  if (playing && fx.playing && section_phase >= 0.0F) {
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

void render_header(V02State& fx, const AppState& app_state) {
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
    fx.cell_zoom = std::clamp(fx.cell_zoom - 9.0F, 34.0F, 88.0F);
  }
  ImGui::SameLine(0.0F, 4.0F);
  if (ImGui::SmallButton("+")) {
    fx.cell_zoom = std::clamp(fx.cell_zoom + 9.0F, 34.0F, 88.0F);
  }
}

// Repeat-Zone auto-song advance (SLICE 4b, docs/proposals/repeat-zone-real-
// contract.md's TIMING DECISION: GUI-DRIVEN, no new engine mechanism). Reads
// the live bar off app_state (already reduced from the "beat" heartbeat,
// app_state.hpp/app_state.cpp), decides via the PURE next_scene_to_launch()
// whether the active scene column's own section has finished, and if so
// fires the EXISTING `style section` verb (bar-quantized by the arranger
// itself, slice 4a) for the next column and advances fx's own active-scene
// bookkeeping. bar_just_advanced() guards against re-evaluating (and
// re-firing) more than once per bar: ImGui runs this every rendered frame,
// but the live bar only changes once per beat-heartbeat poll.
void update_auto_song(const GridModel& model, BrainSession& brain_session,
                      const AppState& app_state, V02State& fx, std::size_t scene_count) {
  const int current_bar = app_state.bar();
  // Captured BEFORE bar_just_advanced mutates auto_song_last_bar in place, so
  // it still holds the LAST bar this function actually evaluated (which, for
  // a session that just resumed after a transport stop, is a stale bar from
  // BEFORE the stop -- the core rewinds its bar counter to 0 on every fresh
  // Start, runtime/transport.hpp's "MIDI Start semantics: rewind to zero",
  // mirrored by AppState's own "stopped" reset).
  const int previous_last_bar = fx.auto_song_last_bar;
  if (!bar_just_advanced(current_bar, fx.auto_song_last_bar)) {
    return;
  }
  // Bar REWIND detected (current bar fell behind the last one evaluated): a
  // stop/restart cycle, not an ordinary forward tick. active_scene_start_bar
  // is still anchored to the pre-stop bar count, so leaving it as-is would
  // make bars_elapsed go deeply negative and stay there until the new
  // session's bar count climbs all the way back past the stale anchor --
  // "the active scene never advances". Re-anchor to the fresh bar so
  // bars_elapsed measures from the restart, not from the stale pre-stop bar.
  if (current_bar < previous_last_bar) {
    fx.active_scene_start_bar = current_bar;
  }
  const int bars_elapsed = current_bar - fx.active_scene_start_bar;
  const std::size_t active_scene_index =
      fx.active_scene >= 0 ? static_cast<std::size_t>(fx.active_scene) : 0;
  // Per-scene length (auto-song fix), NOT preview::section_bars: the style's
  // own section length was always 1 bar for every built-in style (preview.
  // hpp's own "every built-in style's own sections are 1 bar today"
  // comment), which is why the advance used to sprint one bar per scene
  // regardless of what the column actually held. model.scene_bars() gives
  // each scene column its own configurable length (default GridModel::
  // kDefaultSceneBars) -- this is the SAME lookup render_grid_panel's own
  // playhead uses below, so the sweep fills exactly over the length that
  // gates this advance.
  const int active_section_bars = model.scene_bars(active_scene_index);
  const std::optional<int> next =
      next_scene_to_launch(fx.auto_song, fx.playing, fx.active_scene, static_cast<int>(scene_count),
                           bars_elapsed, active_section_bars);
  if (!next.has_value()) {
    return;
  }
  fx.active_scene = *next;
  fx.active_scene_start_bar = current_bar;
  const std::string_view section_name =
      section_wire_name(model.scene_section(static_cast<std::size_t>(fx.active_scene)));
  if (!section_name.empty()) {
    brain_session.send("style section " + std::string(section_name));
  }
  // Root-cause fix: `style section` alone only selects WHICH section a
  // future launch will use -- it never fires the newly-active column's own
  // clips. The manual scene-header launch (render_scene_header_cell below)
  // already sends BOTH verbs together; auto-song's advance must mirror that
  // exactly, or the newly-active column's audio never actually starts (the
  // proven "scene 1 forever" bug -- fx.active_scene DID advance internally,
  // but nothing ever launched the column it pointed at).
  brain_session.send("launch scene " + std::to_string(fx.active_scene) + " quantize " +
                     std::to_string(kDefaultLaunchQuantizeBars));
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
                              const AppState& app_state, V02State& fx, std::size_t s, float cz,
                              float header_h) {
  ImGui::SameLine(0.0F, kCellGap);
  ImGui::PushID(static_cast<int>(s));
  const ImVec2 hp0 = ImGui::GetCursorScreenPos();

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
    const bool go = ImGui::InvisibleButton("head", ImVec2(cz, header_h));
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
    const std::vector<std::string> lines =
        wrap_scene_name(font, kSceneNameFontSize, name, cz - 6.0F);
    const float line_h = font->CalcTextSizeA(kSceneNameFontSize, 1.0e4F, 0.0F, "Ag").y;
    // SLICE 4b: the header's own text tint doubles as the "active scene"
    // indicator -- was hardcoded to column 0; now tracks fx.active_scene,
    // which auto-song's advance AND a manual scene launch both update, so
    // the highlight reflects whichever column is really current.
    const ImU32 name_color =
        neon::u32(static_cast<int>(s) == fx.active_scene ? theme::kText : theme::kTextSecondary);
    for (std::size_t i = 0; i < lines.size(); ++i) {
      hdl->AddText(font, kSceneNameFontSize,
                   ImVec2(hp0.x + 3.0F, hp0.y + 3.0F + static_cast<float>(i) * line_h), name_color,
                   lines[i].c_str());
    }
    const char* caret = "\xE2\x96\xB6";
    const ImVec2 caret_ts = font->CalcTextSizeA(kSceneNameFontSize, 1.0e4F, 0.0F, caret);
    hdl->AddText(font, kSceneNameFontSize, ImVec2(hp0.x + cz - caret_ts.x - 3.0F, hp0.y + 3.0F),
                 neon::u32(theme::kGreen), caret);
    const float uy = hp0.y + header_h - 2.0F;
    hdl->AddLine(ImVec2(hp0.x, uy), ImVec2(hp0.x + cz, uy), neon::u32(theme::kCyan, 0.25F), 2.0F);
    if (double_clicked) {
      fx.renaming_scene = static_cast<int>(s);
      std::snprintf(fx.rename_buffer.data(), fx.rename_buffer.size(), "%s", name.c_str());
      fx.rename_focus_pending = true;
    } else if (go) {
      // SLICE 4a item 5: the header's PRIMARY job is now applying this
      // column's own SectionType through the existing `style section`
      // verb (Param::kStyleSection, quantized to the next bar while
      // playing, immediate when stopped -- Engine::cmd_style's own
      // handling, not reimplemented here). The `launch scene ... quantize`
      // send (kSceneQuantize, the grid-column clip-launch verb) is KEPT
      // alongside it -- a genuinely different, still-useful effect (fires
      // every filled cell in this column) that this slice does not retire.
      // Real per-cell readback (app_state.clip_state) reports the launched
      // state on the NEXT poll(), so no local echo is written here -- see
      // render_grid_panel's own header comment.
      const std::string_view section_name = section_wire_name(model.scene_section(s));
      if (!section_name.empty()) {
        brain_session.send("style section " + std::string(section_name));
      }
      brain_session.send("launch scene " + std::to_string(s) + " quantize " +
                         std::to_string(kDefaultLaunchQuantizeBars));
      // SLICE 4b: a manual scene-column launch is also a real launch of
      // this scene, so it becomes auto-song's own "active scene" baseline
      // -- auto-song, if later armed (or already armed), measures
      // bars-elapsed from THIS launch, not a stale one.
      fx.active_scene = static_cast<int>(s);
      fx.active_scene_start_bar = app_state.bar();
    }
  }
  ImGui::PopID();
}

// Scene header row: a spacer over the label column, then one launch head per
// scene column (see render_scene_header_cell). `header_h` is computed ONCE
// here from the LONGEST wrapped name among every visible column, and shared
// by all of them -- every column must use the SAME header height, or the
// track rows below would start at a different Y per column.
void render_scene_header_row(GridModel& model, BrainSession& brain_session,
                             const AppState& app_state, V02State& fx, std::size_t scenes,
                             float cz) {
  ImFont* font = ImGui::GetFont();
  std::size_t max_lines = 1;
  for (std::size_t s = 0; s < scenes; ++s) {
    const std::string name(model.scene_name(s));
    const std::vector<std::string> lines =
        wrap_scene_name(font, kSceneNameFontSize, name, cz - 6.0F);
    max_lines = std::max(max_lines, lines.size());
  }
  const float line_h = font->CalcTextSizeA(kSceneNameFontSize, 1.0e4F, 0.0F, "Ag").y;
  const float header_h = std::max(cz * 0.5F, static_cast<float>(max_lines) * line_h + 6.0F);

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
void render_track_label(PartsModel& parts, BrainSession& brain_session, const V02Row& row,
                        bool any_solo, const ImVec4& color, V02State& fx, float cz) {
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
  const float name_row_h = ImGui::GetTextLineHeight() + 1.0F;
  const float name_cy = lp.y + name_row_h * 0.5F;
  const ImVec2 d0(lp.x + 2.0F, name_cy - 3.5F);
  const ImVec2 d1(lp.x + 9.0F, name_cy + 3.5F);
  neon::glow_rect(dl, d0, d1, color, 1.0F, dim ? 0.4F : 1.0F, fx.glow && !dim);
  dl->AddRectFilled(d0, d1, neon::u32(color, dim ? 0.4F : 1.0F), 1.0F);
  ImGui::SetCursorScreenPos(ImVec2(lp.x + 14.0F, name_cy - ImGui::GetTextLineHeight() * 0.5F));
  ImGui::TextColored(dim ? theme::kTextMuted : color, "%s", row.name);

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

// One launch cell for a track row: computes its real preview content, draws
// it (draw_cell), and handles both click (launch+open a filled cell, or fill
// an empty one with a local demo clip) and the browser style drag-drop
// target that registers a real ClipMatrix clip. `active_scene`/`active_
// section_phase` are render_grid_panel's own once-per-frame playhead
// resolution (grid_model.hpp's section_playhead_phase); only THIS column,
// when `s == active_scene`, actually draws it -- every other column passes
// draw_cell the "no playhead" sentinel, since only the active scene's own
// bookkeeping (V02State::active_scene_start_bar) is beat-synced to a real
// section boundary.
void render_track_cell(GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
                       BrainSession& brain_session, const AppState& app_state, V02State& fx,
                       const V02Row& row, std::size_t r, std::size_t s, const ImVec4& color,
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
  neon::ClipPattern pattern{};
  bool approx = false;
  if (filled) {
    const auto section = static_cast<preview::Section>(model.scene_section(s));
    const preview::PreviewPattern pp =
        preview::preview_for(fx.active_style, section, row.role_index);
    pattern = neon::clip_pattern_from_pitches(pp.pitch);
    approx = pp.approx;
  }
  const float section_phase = static_cast<int>(s) == active_scene ? active_section_phase : -1.0F;
  ImGui::PushID(static_cast<int>(s));
  const bool clicked = draw_cell("cell", cz, filled, cell.label, color, pattern, approx, playing,
                                 opened, fx, section_phase);
  if (clicked) {
    if (!filled) {
      // Empty -> add a local demo clip (no launch, no verb, no ClipMatrix
      // registration -- only a browser style drop registers for real, see
      // the drag-drop handler below). Short label like the design's
      // addClip, so the cell never shows a truncated name.
      model.set_cell(row.role_index, s, GridCellKind::kStyleSection, "clip");
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
      seqedit.set_part_index(row.role_index);
      seqedit.set_clip_label(cell.label);
    }
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
                      BrainSession& brain_session, const AppState& app_state, V02State& fx,
                      std::size_t r, std::size_t scenes, bool any_solo, float cz, int active_scene,
                      float active_section_phase) {
  const V02Row& row = kRows[r];
  const ImVec4& color = theme::kV02TrackColor[r];

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
                       BrainSession& brain_session, const AppState& app_state, V02State& fx) {
  seed_demo(model, seqedit, fx);
  render_header(fx, app_state);
  ImGui::Spacing();

  // Standard solo semantics: any part soloed makes the non-soloed rows read as
  // muted (dimmed). Derived from the shared PartsModel (same state the rail
  // mute/solo edits), so both surfaces stay consistent.
  const bool any_solo = any_part_soloed(parts);

  const float cz = fx.cell_zoom;
  const std::size_t scenes = std::min<std::size_t>(model.scene_count(), 5);

  // SLICE 4b: evaluate the auto-song advance BEFORE drawing the scene header
  // row below, so a fired advance is reflected in the SAME frame's active-
  // scene highlight rather than lagging one frame behind.
  update_auto_song(model, brain_session, app_state, fx, scenes);

  // Beat-synchronized playhead (owner-locked): resolved ONCE per frame, here,
  // from the active scene's own PER-SCENE length (model.scene_bars(), the
  // SAME lookup update_auto_song's advance decision uses -- auto-song fix:
  // this used to be preview::section_bars, the STYLE's section length,
  // which is 1 bar for every built-in style and so is NOT the length the
  // advance now gates on) and the authoritative beat/bar/pulse -- never
  // wall-clock time. `active_section_bars` deliberately duplicates update_
  // auto_song's own lookup (rather than sharing a helper) so this purely-
  // visual addition can never perturb the auto-song advance logic above it;
  // the two lookups must nonetheless stay identical, or the sweep would
  // reach the cell edge at a different bar than auto-song actually advances.
  const std::size_t active_scene_index =
      fx.active_scene >= 0 ? static_cast<std::size_t>(fx.active_scene) : 0;
  const int active_section_bars = model.scene_bars(active_scene_index);
  const float active_section_phase =
      section_playhead_phase(app_state.bar(), fx.active_scene_start_bar, app_state.beat_num(),
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
