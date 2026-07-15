#include "grid_panel.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

#include "browser_model.hpp"
#include "imgui.h"
#include "neon_widgets.hpp"
#include "theme.hpp"

namespace sonotron {

namespace {

constexpr int kDefaultLaunchQuantizeBars = 1;
// Widened from 64px to fit the dot + M + S latches + the track name (issue 6).
constexpr float kLabelColWidth = 98.0F;
constexpr float kCellGap = 7.0F;
constexpr float kLatchSize = 16.0F;

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
// track color. Only the pad row is `audio` (waveform preview); the rest are
// midi (dot piano-roll). The GridModel has 9 role rows; these are the 6 the
// v02 grid surfaces.
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
void seed_demo(GridModel& model, V02State& fx) {
  if (fx.seeded) {
    return;
  }
  fx.seeded = true;
  const std::array<std::pair<std::size_t, std::size_t>, 9> pattern = {{
      {0, 0}, {0, 2}, {1, 0}, {2, 1}, {2, 3}, {3, 0}, {4, 2}, {5, 1}, {5, 4},
  }};
  for (const auto& [row, scene] : pattern) {
    if (row >= kRows.size() || scene >= model.scene_count()) {
      continue;
    }
    const std::string label = std::string(kRows[row].name) + std::to_string(scene + 1);
    model.set_cell(kRows[row].role_index, scene, GridCellKind::kStyleSection, label);
  }
}

// Draws one launch cell (custom draw-list) at the cursor; returns true on
// click. `filled` cells show a preview + label + (when playing) a sweep bar.
bool draw_cell(const char* id, float size, bool filled, const std::string& label,
               const ImVec4& color, bool is_audio, bool playing, bool opened, const V02State& fx) {
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
  dl->AddRect(p0, p1, neon::u32(color, playing ? 1.0F : 0.55F), rounding, 0,
              playing ? 1.6F : 1.0F);
  if (opened) {
    dl->AddRect(ImVec2(p0.x + 2.0F, p0.y + 2.0F), ImVec2(p1.x - 2.0F, p1.y - 2.0F),
                neon::u32(theme::kText, 0.7F), rounding - 2.0F, 0, 1.0F);
  }

  // Preview in the inner rect (leave the bottom strip for the label).
  const ImVec2 in0(p0.x + 4.0F, p0.y + 4.0F);
  const ImVec2 in1(p1.x - 4.0F, p1.y - 14.0F);
  const std::uint32_t seed = neon::hash_label(label);
  if (in1.y > in0.y + 4.0F) {
    if (is_audio) {
      neon::clip_preview_waveform(dl, in0, in1, seed, color);
    } else {
      neon::clip_preview_pianoroll(dl, in0, in1, seed, color);
    }
  }

  // Bottom-centered label: "▶ label" playing / "▷ label" stopped, truncated to
  // fit the cell width (a mini clip cell shows a clipped name, never a spill).
  std::string text = (playing ? "\xE2\x96\xB6 " : "\xE2\x96\xB7 ") + label;
  while (text.size() > 2 && ImGui::CalcTextSize(text.c_str()).x > size - 4.0F) {
    text.pop_back();
  }
  const ImVec2 ts = ImGui::CalcTextSize(text.c_str());
  const float tx = std::max(p0.x + 3.0F, p0.x + (size - ts.x) * 0.5F);
  dl->PushClipRect(p0, p1, true);
  dl->AddText(ImVec2(tx, p1.y - 13.0F), neon::u32(playing ? color : theme::kTextSecondary),
              text.c_str());
  dl->PopClipRect();

  // L->R sweep on a playing cell while running.
  if (playing && fx.playing) {
    neon::sweep_bar(dl, p0, p1, fx.time, theme::kText);
  }
  return clicked;
}

void render_header(V02State& fx) {
  ImGui::TextColored(theme::kCyan, "REPEAT ZONE");
  ImGui::SameLine(0.0F, 10.0F);
  ImGui::TextColored(theme::kTextMuted, "click to launch \xC2\xB7 one clip per row");

  // Zoom -/+ right-aligned.
  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 58.0F);
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
                       BrainSession& brain_session, V02State& fx) {
  seed_demo(model, fx);
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

  // Scene header row: a spacer over the label column, then "n ▶" launch heads.
  ImGui::Dummy(ImVec2(kLabelColWidth, cz * 0.5F));
  for (std::size_t s = 0; s < scenes; ++s) {
    ImGui::SameLine(0.0F, kCellGap);
    ImGui::PushID(static_cast<int>(s));
    ImGui::BeginGroup();
    ImGui::TextColored(theme::kTextSecondary, "%zu", s + 1);
    ImGui::SameLine(0.0F, 4.0F);
    ImGui::PushStyleColor(ImGuiCol_Text, theme::kGreen);
    const bool go = ImGui::SmallButton("\xE2\x96\xB6");
    ImGui::PopStyleColor();
    ImGui::EndGroup();
    if (go) {
      brain_session.send("launch scene " + std::to_string(s) + " quantize " +
                         std::to_string(kDefaultLaunchQuantizeBars));
      for (std::size_t r = 0; r < kRows.size(); ++r) {
        const GridCell& c = model.cell(kRows[r].role_index, s);
        fx.row_playing[r] = c.kind != GridCellKind::kEmpty ? static_cast<int>(s) : -1;
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
    dl->AddCircleFilled(ImVec2(lp.x + 5.0F, cy), 4.0F, neon::u32(color, dim ? 0.4F : 1.0F), 16);

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
    ImGui::TextColored(dim ? theme::kTextMuted : theme::kText, "%s", row.name);
    ImGui::SetCursorScreenPos(lp);
    ImGui::Dummy(ImVec2(kLabelColWidth, cz));

    for (std::size_t s = 0; s < scenes; ++s) {
      ImGui::SameLine(0.0F, kCellGap);
      const std::size_t id = cell_id(row.role_index, s, model.scene_count());
      const GridCell& cell = model.cell(row.role_index, s);
      const bool filled = cell.kind != GridCellKind::kEmpty;
      const bool playing = fx.row_playing[r] == static_cast<int>(s);
      const bool opened = fx.open_cell == static_cast<int>(id);
      ImGui::PushID(static_cast<int>(s));
      const bool clicked =
          draw_cell("cell", cz, filled, cell.label, color, row.audio, playing, opened, fx);
      if (clicked) {
        if (!filled) {
          // Empty -> add a local demo clip (no launch, no verb).
          const std::string label = std::string(row.name) + std::to_string(s + 1);
          model.set_cell(row.role_index, s, GridCellKind::kStyleSection, label);
        } else {
          // Filled -> real launch + open in Sequence Edit + local row echo.
          brain_session.send("launch clip " + std::to_string(id) + " quantize " +
                             std::to_string(kDefaultLaunchQuantizeBars));
          fx.row_playing[r] = playing ? -1 : static_cast<int>(s);
          fx.open_cell = static_cast<int>(id);
          fx.open_row = static_cast<int>(r);
          seqedit.set_part_index(row.role_index);
          seqedit.set_clip_label(cell.label);
        }
      }
      // Drop target: a browser style drag fills this cell for real.
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kStyleDragPayloadId)) {
          const std::size_t style_index = *static_cast<const std::size_t*>(payload->Data);
          model.set_cell(row.role_index, s, GridCellKind::kStyleSection,
                         std::string(kBuiltinStyleNames[style_index]));
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
