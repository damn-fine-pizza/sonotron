#include "grid_panel.hpp"

#include <cfloat>
#include <cstddef>
#include <string>

#include "browser_model.hpp"
#include "imgui.h"
#include "theme.hpp"

// Default launch quantize (ux-workstation.md §5 "1 bar / 2 bars / instant"):
// a bare click launches quantized to the next bar, not instant -- matches
// the doc's own launch-grid convention (a clip-launch surprise mid-bar is
// the wrong default for a live performance grid).
namespace sonotron {

namespace {

constexpr int kDefaultLaunchQuantizeBars = 1;
constexpr float kRoleSwatchSize = 6.0F;

// Part row label (components.jsx's LaunchGrid): a small tone swatch + the
// part name, colored per the ROLE_TONES ramp (theme.hpp's kRoleTint).
void render_part_row_label(GridModel& model, std::size_t part_index) {
  const ImVec4& tone = theme::kRoleTint[part_index % theme::kRoleTint.size()];
  const float line_height = ImGui::GetTextLineHeight();
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const float swatch_y = pos.y + (line_height - kRoleSwatchSize) * 0.5F;
  ImGui::GetWindowDrawList()->AddRectFilled(
      ImVec2(pos.x, swatch_y), ImVec2(pos.x + kRoleSwatchSize, swatch_y + kRoleSwatchSize),
      ImGui::ColorConvertFloat4ToU32(tone));
  ImGui::Dummy(ImVec2(kRoleSwatchSize + 4.0F, line_height));
  ImGui::SameLine(0.0F, 4.0F);
  ImGui::TextColored(theme::kTextSecondary, "%s",
                     std::string(model.part_label(part_index)).c_str());
}

void render_scene_header(std::size_t scene_index, BrainSession& brain_session) {
  ImGui::TableSetColumnIndex(static_cast<int>(scene_index) + 1);
  ImGui::PushID(static_cast<int>(scene_index));
  ImGui::TextColored(theme::kTextSecondary, "Scene%zu", scene_index + 1);
  ImGui::SameLine();
  // "Scene ▶ all" (§3 wireframe): fans a column's launches out through the
  // real core clip primitive (`launch scene <n> quantize <q>`,
  // docs/design/clip-primitive-design.md). Green launch glyph
  // (readme.md's iconography: ▶ is the launch/scene marker).
  ImGui::PushStyleColor(ImGuiCol_Text, theme::kGreen);
  const bool clicked = ImGui::SmallButton("\xE2\x96\xB6");  // ▶
  ImGui::PopStyleColor();
  if (clicked) {
    brain_session.send("launch scene " + std::to_string(scene_index) + " quantize " +
                       std::to_string(kDefaultLaunchQuantizeBars));
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Launch this scene column, quantized to the next bar");
  }
  ImGui::PopID();
}

// LaunchCell (components.jsx): a square tile per-role tinted when filled,
// a dim "·" empty-cell drop target otherwise. NOTE: there is no per-cell
// armed/playing runtime signal on the wire yet -- GridModel only holds
// authored content (grid_model.hpp's own header comment: "a live per-cell
// armed/playing indicator ... is follow-up work", app_state.cpp's kClip
// case). So the "playing" tone/border and the sweeping-playhead line
// components.jsx also describes for LaunchCell are NOT rendered here --
// this is a known, already-documented gap, not something faked.
void render_cell(GridModel& model, std::size_t part_index, std::size_t scene_index,
                 BrainSession& brain_session) {
  ImGui::TableSetColumnIndex(static_cast<int>(scene_index) + 1);
  const std::size_t id = part_index * model.scene_count() + scene_index;
  ImGui::PushID(static_cast<int>(id));

  const GridCell& cell = model.cell(part_index, scene_index);
  const bool filled = cell.kind != GridCellKind::kEmpty;
  const ImVec4& tone = theme::kRoleTint[part_index % theme::kRoleTint.size()];

  if (filled) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(tone.x, tone.y, tone.z, 0.12F));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(tone.x, tone.y, tone.z, 0.30F));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(tone.x, tone.y, tone.z, 0.45F));
    ImGui::PushStyleColor(ImGuiCol_Border, tone);
  } else {
    ImGui::PushStyleColor(ImGuiCol_Button, theme::kWindowBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(theme::kAccent.x, theme::kAccent.y, theme::kAccent.z, 0.15F));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          ImVec4(theme::kAccent.x, theme::kAccent.y, theme::kAccent.z, 0.25F));
    ImGui::PushStyleColor(ImGuiCol_Border, theme::kBorder);
  }
  ImGui::PushStyleColor(ImGuiCol_Text, filled ? theme::kText : theme::kTextDim);

  // Filled: "▶ <label>" (readme.md's launch glyph); empty: the dim leaf
  // glyph "·", read as a drop target for a dragged browser style.
  const std::string label = filled ? ("\xE2\x96\xB6 " + cell.label) : "\xC2\xB7";
  const bool clicked = ImGui::Button(label.c_str(), ImVec2(-FLT_MIN, 0));
  ImGui::PopStyleColor(5);
  if (clicked) {
    // Real core clip primitive (Phase-5 Item #2): `id` addresses a ClipMatrix
    // slot the same way this cell's own ImGui PushID does.
    brain_session.send("launch clip " + std::to_string(id) + " quantize " +
                       std::to_string(kDefaultLaunchQuantizeBars));
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Launch, quantized to the next bar");
  }

  // A real drop target: dropping a browser style sets the cell's content
  // for real (GridModel::set_cell), independent of the launch above.
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kStyleDragPayloadId)) {
      const std::size_t style_index = *static_cast<const std::size_t*>(payload->Data);
      model.set_cell(part_index, scene_index, GridCellKind::kStyleSection,
                     std::string(kBuiltinStyleNames[style_index]));
    }
    ImGui::EndDragDropTarget();
  }

  ImGui::PopID();
}

}  // namespace

void render_grid_panel(GridModel& model, BrainSession& brain_session) {
  const int column_count = static_cast<int>(model.scene_count()) + 1;
  if (!ImGui::BeginTable("grid", column_count,
                         ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame)) {
    return;
  }
  ImGui::TableSetupColumn("Part", ImGuiTableColumnFlags_WidthFixed, 70.0F);
  for (std::size_t scene = 0; scene < model.scene_count(); ++scene) {
    ImGui::TableSetupColumn("");
  }

  ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
  ImGui::TableSetColumnIndex(0);
  ImGui::TextUnformatted("");
  for (std::size_t scene = 0; scene < model.scene_count(); ++scene) {
    render_scene_header(scene, brain_session);
  }

  for (std::size_t part = 0; part < model.part_count(); ++part) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    render_part_row_label(model, part);
    for (std::size_t scene = 0; scene < model.scene_count(); ++scene) {
      render_cell(model, part, scene, brain_session);
    }
  }

  ImGui::EndTable();

  if (model.scene_count() < GridModel::kMaxSceneCount && ImGui::SmallButton("+ Scene")) {
    model.add_scene();
  }
}

}  // namespace sonotron
