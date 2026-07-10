#include "layout_renderer.hpp"

#include "imgui.h"

namespace sonotron {

namespace {

// Dispatches a zone to its live panel by id. The abandoned concept panels
// (arrangement/intention mock) have been removed in the Fase 2 restart; the
// workstation zone panels (transport/browser/grid/seqedit/parts/intention)
// are added by later slices. Until then every zone stays a titled empty
// frame — this is the seam each future zone panel hooks into.
void render_zone_content(const Zone& zone) { (void)zone; }

}  // namespace

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
      ImGui::BeginChild(zone.id.c_str(), ImVec2(cell_width, row_height), ImGuiChildFlags_Borders,
                        ImGuiWindowFlags_NoScrollbar);
      ImGui::TextUnformatted(zone.title.c_str());
      ImGui::Separator();
      render_zone_content(zone);
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
