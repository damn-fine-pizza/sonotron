#include "grid_panel.hpp"

#include <cfloat>
#include <cstddef>
#include <string>

#include "browser_model.hpp"
#include "imgui.h"

// Default launch quantize (ux-workstation.md §5 "1 bar / 2 bars / instant"):
// a bare click launches quantized to the next bar, not instant -- matches
// the doc's own launch-grid convention (a clip-launch surprise mid-bar is
// the wrong default for a live performance grid).
namespace sonotron {

namespace {

constexpr int kDefaultLaunchQuantizeBars = 1;

const char* cell_display_text(const GridCell& cell) {
  return cell.kind == GridCellKind::kEmpty ? "." : cell.label.c_str();
}

void render_scene_header(std::size_t scene_index, BrainSession& brain_session) {
  ImGui::TableSetColumnIndex(static_cast<int>(scene_index) + 1);
  ImGui::PushID(static_cast<int>(scene_index));
  ImGui::Text("Scene%zu", scene_index + 1);
  ImGui::SameLine();
  // "Scene ▶ all" (§3 wireframe): fans a column's launches out through the
  // real core clip primitive (`launch scene <n> quantize <q>`,
  // docs/design/clip-primitive-design.md).
  if (ImGui::SmallButton(">")) {
    brain_session.send("launch scene " + std::to_string(scene_index) + " quantize " +
                       std::to_string(kDefaultLaunchQuantizeBars));
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Launch this scene column, quantized to the next bar");
  }
  ImGui::PopID();
}

void render_cell(GridModel& model, std::size_t part_index, std::size_t scene_index,
                 BrainSession& brain_session) {
  ImGui::TableSetColumnIndex(static_cast<int>(scene_index) + 1);
  const std::size_t id = part_index * model.scene_count() + scene_index;
  ImGui::PushID(static_cast<int>(id));

  const GridCell& cell = model.cell(part_index, scene_index);
  if (ImGui::Button(cell_display_text(cell), ImVec2(-FLT_MIN, 0))) {
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
    ImGui::TextUnformatted(std::string(model.part_label(part)).c_str());
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
