#include "grid_panel.hpp"

#include <cfloat>
#include <cstddef>
#include <string>

#include "browser_model.hpp"
#include "imgui.h"

namespace sonotron {

namespace {

const char* cell_display_text(const GridCell& cell) {
  return cell.kind == GridCellKind::kEmpty ? "." : cell.label.c_str();
}

void render_scene_header(std::size_t scene_index) {
  ImGui::TableSetColumnIndex(static_cast<int>(scene_index) + 1);
  ImGui::PushID(static_cast<int>(scene_index));
  ImGui::Text("Scene%zu", scene_index + 1);
  ImGui::SameLine();
  // "Scene ▶ all" (§3 wireframe): an honest placeholder — fanning out a
  // column's launches is the same core-clip-primitive gap as a single-cell
  // launch (kGridLaunchWired, grid_model.hpp).
  ImGui::BeginDisabled(!kGridLaunchWired);
  ImGui::SmallButton(">");
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Scene launch awaits the core clip primitive (ux-workstation.md S11.3)");
  }
  ImGui::PopID();
}

void render_cell(GridModel& model, std::size_t part_index, std::size_t scene_index) {
  ImGui::TableSetColumnIndex(static_cast<int>(scene_index) + 1);
  ImGui::PushID(static_cast<int>(part_index * model.scene_count() + scene_index));

  const GridCell& cell = model.cell(part_index, scene_index);
  ImGui::Button(cell_display_text(cell), ImVec2(-FLT_MIN, 0));
  if (ImGui::IsItemHovered() && !kGridLaunchWired) {
    ImGui::SetTooltip("Launch awaits the core clip primitive (ux-workstation.md S11.3)");
  }

  // A real drop target: dropping a browser style sets the cell's content
  // for real (GridModel::set_cell), independent of the launch gap above.
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

void render_grid_panel(GridModel& model) {
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
    render_scene_header(scene);
  }

  for (std::size_t part = 0; part < model.part_count(); ++part) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(std::string(model.part_label(part)).c_str());
    for (std::size_t scene = 0; scene < model.scene_count(); ++scene) {
      render_cell(model, part, scene);
    }
  }

  ImGui::EndTable();

  if (model.scene_count() < GridModel::kMaxSceneCount && ImGui::SmallButton("+ Scene")) {
    model.add_scene();
  }
}

}  // namespace sonotron
