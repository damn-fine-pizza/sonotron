#include "layout_renderer.hpp"

#include "imgui.h"
#include "arrangement.hpp"
#include "arrangement_panel.hpp"
#include "intention.hpp"
#include "intention_panel.hpp"

namespace sonotron {

namespace {

// Dispatches a zone to its live panel by id. "arrangement" and "intention"
// are filled so far — with mock data, because the brain is not wired yet (D38
// needs a client channel that does not exist). Every other zone stays a
// titled empty frame. This is the seam each future zone panel hooks into;
// when the brain arrives, the mock source here is swapped for state flowed in
// from main.
void render_zone_content(const Zone& zone) {
  if (zone.id == "arrangement") {
    render_arrangement_panel(mock_arrangement());
  } else if (zone.id == "intention") {
    render_intention_panel(mock_intention_state());
  }
}

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
