#include "layout_renderer.hpp"

#include "imgui.h"

namespace sonotron {

void render_layout(const Layout& layout) {
  const std::vector<RowGeometry> rows = compute_rows(layout);
  const ImVec2 avail = ImGui::GetContentRegionAvail();

  for (std::size_t row_index = 0; row_index < rows.size(); ++row_index) {
    const RowGeometry& row = rows[row_index];
    const float row_height = avail.y * row.height_fraction;
    ImGui::PushID(static_cast<int>(row_index));

    for (std::size_t cell_index = 0; cell_index < row.cells.size(); ++cell_index) {
      const ZoneGeometry& cell = row.cells[cell_index];
      const Zone& zone = layout.zones[cell.zone_index];
      const float cell_width = avail.x * cell.width_fraction;

      ImGui::PushID(static_cast<int>(cell_index));
      ImGui::BeginChild(zone.id.c_str(), ImVec2(cell_width, row_height), ImGuiChildFlags_Borders);
      ImGui::TextUnformatted(zone.title.c_str());
      ImGui::Separator();
      // Zone content is deliberately empty — a titled frame only, for now.
      ImGui::EndChild();
      ImGui::PopID();

      if (cell_index + 1 < row.cells.size()) {
        ImGui::SameLine();
      }
    }

    ImGui::PopID();
  }
}

}  // namespace sonotron
