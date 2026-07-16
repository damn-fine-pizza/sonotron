#include "grid_panel.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <string>

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
constexpr float kLatchSize = 13.0F;

// A small neon M/S latch: solid tone when engaged, dark inset otherwise.
bool grid_latch(const char* glyph, bool engaged, const ImVec4& tone) {
  ImGui::PushStyleColor(ImGuiCol_Button, engaged ? tone : theme::kFrameBg);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, engaged ? tone : theme::kFrameBgHover);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, engaged ? tone : theme::kFrameBgActive);
  ImGui::PushStyleColor(ImGuiCol_Text, engaged ? theme::kAppBg : theme::kTextSecondary);
  const bool clicked = ImGui::Button(glyph, ImVec2(kLatchSize, kLatchSize));
  ImGui::PopStyleColor(4);
  return clicked;
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
      {0, 0, "A"},   {0, 1, "B"},    {0, 3, "fil"},
      {1, 0, "wlk"}, {1, 2, "sub"},
      {2, 0, "cmp"}, {2, 1, "stab"}, {2, 4, "out"},
      {3, 1, "swl"}, {3, 3, "swl"},
      {4, 0, "up"},  {4, 2, "up2"},
      {5, 1, "vox"}, {5, 2, "ld"},   {5, 4, "end"},
  }};
  for (const auto& [row, scene, label] : pattern) {
    if (row >= kRows.size() || scene >= model.scene_count()) {
      continue;
    }
    model.set_cell(kRows[row].role_index, scene, GridCellKind::kStyleSection, label);
  }

  // Open the bass 'wlk' clip by default — the design's initial openAt {r:1,c:0}
  // — so Sequence Edit shows a populated (blue) piano-roll, not the empty hint.
  fx.open_row = 1;
  fx.open_cell = static_cast<int>(cell_id(kRows[1].role_index, 0, model.scene_count()));
  fx.open_audio = kRows[1].audio;
  seqedit.set_part_index(kRows[1].role_index);
  seqedit.set_clip_label("wlk");
}

// Draws one launch cell (custom draw-list) at the cursor; returns true on
// click. `filled` cells show a preview + label + (when playing) a sweep bar.
// `pattern`/`approx` are the cell's REAL resolved content (preview::
// preview_for, computed by the caller) and its honesty flag (repeat-zone-
// real-contract.md STEP 4) -- ignored when `!filled`.
bool draw_cell(const char* id, float size, bool filled, const std::string& label,
               const ImVec4& color, const neon::ClipPattern& pattern, bool approx, bool playing,
               bool opened, const V02State& fx) {
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
  dl->AddRect(p0, p1, neon::u32(color, playing ? 1.0F : 0.40F), rounding, 0,
              playing ? 1.6F : 1.0F);
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

  // L->R sweep on a playing cell while running.
  if (playing && fx.playing) {
    neon::sweep_bar(dl, p0, p1, fx.time, theme::kText);
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

void render_header(V02State& fx) {
  ImGui::TextColored(theme::kCyan, "REPEAT ZONE");

  // Hint + zoom -/+ as one right-aligned group (design: the hint sits next to
  // the zoom control, not beside the title).
  const char* hint = "click = launch + open \xC2\xB7 scene \xE2\x96\xB6 = launch column";
  const float hint_w = ImGui::CalcTextSize(hint).x;
  const float zoom_w = 58.0F;
  ImGui::SameLine();
  ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),
                                ImGui::GetContentRegionMax().x - zoom_w - hint_w - 8.0F));
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

}  // namespace

void render_grid_panel(GridModel& model, SeqEditModel& seqedit, PartsModel& parts,
                       BrainSession& brain_session, const AppState& app_state, V02State& fx) {
  seed_demo(model, seqedit, fx);
  render_header(fx);
  ImGui::Spacing();

  // Standard solo semantics: any part soloed makes the non-soloed rows read as
  // muted (dimmed). Derived from the shared PartsModel (same state the rail
  // mute/solo edits), so both surfaces stay consistent.
  bool any_solo = false;
  for (std::size_t i = 0; i < PartsModel::kPartCount; ++i) {
    if (parts.part(i).soloed) {
      any_solo = true;
      break;
    }
  }

  const float cz = fx.cell_zoom;
  const std::size_t scenes = std::min<std::size_t>(model.scene_count(), 5);

  ImGui::BeginChild("grid_body", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_None);

  // Scene header row: a spacer over the label column, then "name ▶" launch
  // heads drawn as plain text + a cyan underline (design has no button
  // pill). Double-clicking a header swaps it for an inline ImGui InputText
  // (repeat-zone-real-contract.md §4/§8b decision 3, OWNER LOCKED to
  // host-only GridModel storage + scenes.json, no core touch) so the column
  // can be renamed; commit on Enter/focus-loss, Esc cancels. The launch verb
  // and cyan underline are otherwise unchanged from before this rename
  // affordance existed.
  ImGui::Dummy(ImVec2(kLabelColWidth, cz * 0.5F));
  ImDrawList* hdl = ImGui::GetWindowDrawList();
  for (std::size_t s = 0; s < scenes; ++s) {
    ImGui::SameLine(0.0F, kCellGap);
    ImGui::PushID(static_cast<int>(s));
    const ImVec2 hp0 = ImGui::GetCursorScreenPos();

    if (fx.renaming_scene == static_cast<int>(s)) {
      // Inline rename in progress for THIS scene column: an InputText
      // replaces the header, occupying the same cz x cz*0.5 footprint the
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
      ImGui::Dummy(ImVec2(cz, std::max(0.0F, cz * 0.5F - ImGui::GetFrameHeight())));
    } else {
      const bool go = ImGui::InvisibleButton("head", ImVec2(cz, cz * 0.5F));
      const bool double_clicked =
          ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
      const std::string name(model.scene_name(s));
      const float ty = hp0.y + (cz * 0.5F - ImGui::GetTextLineHeight()) * 0.5F;
      hdl->AddText(ImVec2(hp0.x + 3.0F, ty),
                   neon::u32(s == 0 ? theme::kText : theme::kTextSecondary), name.c_str());
      hdl->AddText(ImVec2(hp0.x + 3.0F + ImGui::CalcTextSize(name.c_str()).x + 4.0F, ty),
                   neon::u32(theme::kGreen), "\xE2\x96\xB6");
      const float uy = hp0.y + cz * 0.5F - 2.0F;
      hdl->AddLine(ImVec2(hp0.x, uy), ImVec2(hp0.x + cz, uy), neon::u32(theme::kCyan, 0.25F),
                   2.0F);
      if (double_clicked) {
        fx.renaming_scene = static_cast<int>(s);
        std::snprintf(fx.rename_buffer.data(), fx.rename_buffer.size(), "%s", name.c_str());
        fx.rename_focus_pending = true;
      } else if (go) {
        // Real per-cell readback (app_state.clip_state) reports the launched
        // state on the NEXT poll(), so no local echo is written here -- see
        // render_grid_panel's own header comment.
        brain_session.send("launch scene " + std::to_string(s) + " quantize " +
                           std::to_string(kDefaultLaunchQuantizeBars));
      }
    }
    ImGui::PopID();
  }

  // Track rows.
  for (std::size_t r = 0; r < kRows.size(); ++r) {
    const V02Row& row = kRows[r];
    const ImVec4& color = theme::kV02TrackColor[r];

    // Label cell: color dot + M/S latches + name, all left of the cells.
    ImGui::PushID(static_cast<int>(100 + r));
    const std::size_t role = row.role_index;
    const PartInfo& info = parts.part(role);
    const bool dim = (any_solo && !info.soloed) || info.muted;
    const ImVec2 lp = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float cy = lp.y + cz * 0.5F;
    // Design: a 7x7 track-colored SQUARE (glow), not a circle.
    const ImVec2 d0(lp.x + 2.0F, cy - 3.5F);
    const ImVec2 d1(lp.x + 9.0F, cy + 3.5F);
    neon::glow_rect(dl, d0, d1, color, 1.0F, dim ? 0.4F : 1.0F, fx.glow && !dim);
    dl->AddRectFilled(d0, d1, neon::u32(color, dim ? 0.4F : 1.0F), 1.0F);

    // M / S latches (real `part <role> mute|solo on|off` verb), vertically
    // centered in the row's label column.
    const std::string token(parts.part_wire_token(role));
    const float by = cy - kLatchSize * 0.5F;
    ImGui::SetCursorScreenPos(ImVec2(lp.x + 12.0F, by));
    if (grid_latch("M", info.muted, theme::kPink)) {
      const bool was = info.muted;
      parts.toggle_mute(role);
      brain_session.send("part " + token + " mute " + (!was ? "on" : "off"));
    }
    ImGui::SetCursorScreenPos(ImVec2(lp.x + 12.0F + kLatchSize + 2.0F, by));
    if (grid_latch("S", info.soloed, theme::kAmber)) {
      const bool was = info.soloed;
      parts.toggle_solo(role);
      brain_session.send("part " + token + " solo " + (!was ? "on" : "off"));
    }

    ImGui::SetCursorScreenPos(ImVec2(lp.x + 12.0F + 2.0F * kLatchSize + 6.0F,
                                     cy - ImGui::GetTextLineHeight() * 0.5F));
    // Design: the track name is in the TRACK COLOR (not white).
    ImGui::TextColored(dim ? theme::kTextMuted : color, "%s", row.name);
    ImGui::SetCursorScreenPos(lp);
    ImGui::Dummy(ImVec2(kLabelColWidth, cz));

    for (std::size_t s = 0; s < scenes; ++s) {
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
      // Real content (repeat-zone-real-contract.md "cell preview made
      // real"): the SAME preview_for(...) the opened cell's Sequence Edit
      // canvas uses, keyed by this cell's own role and the (today: single,
      // browser-highlight) active style -- so the two views match by
      // construction. Empty cells never draw a preview, so this is only
      // computed for a filled one.
      neon::ClipPattern pattern{};
      bool approx = false;
      if (filled) {
        const preview::PreviewPattern pp =
            preview::preview_for(fx.active_style, preview::Section::kVarA, row.role_index);
        pattern = neon::clip_pattern_from_pitches(pp.pitch);
        approx = pp.approx;
      }
      ImGui::PushID(static_cast<int>(s));
      const bool clicked =
          draw_cell("cell", cz, filled, cell.label, color, pattern, approx, playing, opened, fx);
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
          seqedit.set_part_index(row.role_index);
          seqedit.set_clip_label(cell.label);
        }
      }
      // Drop target: a browser style drag fills this cell for real AND
      // registers a real ClipMatrix clip at this cell's own stable id
      // (repeat-zone-real-contract.md §3/§8b decision 1, Shape A) -- so a
      // later `launch clip <id>` actually addresses THIS cell's material
      // instead of an empty pool slot. Owner decision 2: drag-a-style is the
      // only content source real for this pass; the registered clip always
      // references SectionType::kVarA (the arranger's own default section --
      // the GUI has no per-style section identity to pick from, mirroring
      // `style switch`'s own established fallback, in_process_brain_
      // session.cpp).
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kStyleDragPayloadId)) {
          const std::size_t style_index = *static_cast<const std::size_t*>(payload->Data);
          model.set_cell(row.role_index, s, GridCellKind::kStyleSection,
                         std::string(kBuiltinStyleNames[style_index]));
          brain_session.send("clip add " + std::string(parts.part_wire_token(row.role_index)) +
                             " " + std::to_string(s) + " style varA id " + std::to_string(id));
        }
        ImGui::EndDragDropTarget();
      }
      ImGui::PopID();
    }
    ImGui::PopID();
  }

  ImGui::EndChild();
}

}  // namespace sonotron
