#include "layout_renderer.hpp"

#include <cfloat>

#include "browser_panel.hpp"
#include "grid_panel.hpp"
#include "imgui.h"
#include "intention_panel.hpp"
#include "parts_panel.hpp"
#include "seqedit_panel.hpp"
#include "transport_panel.hpp"

namespace sonotron {

namespace {

// Dispatches a zone to its live panel by id (G3, docs/design/
// gui-fase2-mechanical-plan.md). The abandoned concept panels (arrangement/
// intention mock) were removed in the Fase 2 restart; the G2 transport
// stopgap (a bare Connected/Disconnected line) is now fully superseded by
// transport_panel.cpp, which owns the whole transport row. A zone id with
// no matching case falls through to the G1 fallback (title + separator,
// empty body) — this keeps a stale/hand-edited layout.json safe to render.
//
// The ids handled below MUST stay in sync with is_renderable_zone_id()
// (layout_model.hpp) — the single shared authority layout_json.cpp's
// loader also consults, so its notion of "valid id" cannot silently drift
// from what this dispatch can actually draw. The early return below ties
// the "unknown id -> empty G1 frame" fallback directly to that same
// predicate rather than duplicating the id list a second time here.
void render_zone_content(const Zone& zone, WorkstationState& state) {
  if (!is_renderable_zone_id(zone.id)) {
    return;
  }
  if (zone.id == "transport") {
    render_transport_panel(state.app_state, state.brain_session);
  } else if (zone.id == "browser") {
    render_browser_panel(state.browser, state.brain_session);
  } else if (zone.id == "grid") {
    render_grid_panel(state.grid, state.brain_session);
  } else if (zone.id == "seqedit") {
    render_seqedit_panel(state.seqedit);
  } else if (zone.id == "parts") {
    render_parts_panel(state.parts, state.brain_session);
  } else if (zone.id == "intention") {
    render_intention_panel(state.app_state);
  }
}

}  // namespace

namespace {

// Draws one zone as a titled, bordered child of the given size at the
// current cursor, then dispatches its live content. Shared by the
// single-zone and stacked-column paths so both frames look identical.
//
// The "transport" zone is a deliberate exception to the title-line +
// separator + body shape every other zone uses: default_layout() gives it
// only a thin, single-line-tall row (§3's wireframe packs play/stop, tempo,
// key, style, connection status and the bar·beat readout onto ONE line,
// with no "Transport" heading of its own) — so transport_panel.cpp owns the
// whole line instead of sharing it with a title + separator that would not
// fit.
void render_zone_frame(const Zone& zone, const ImVec2& size, WorkstationState& state) {
  ImGui::BeginChild(zone.id.c_str(), size, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
  if (zone.id == "transport") {
    render_zone_content(zone, state);
  } else {
    ImGui::TextUnformatted(zone.title.c_str());
    ImGui::Separator();
    render_zone_content(zone, state);
  }
  ImGui::EndChild();
}

}  // namespace

void render_layout(const Layout& layout, WorkstationState& state) {
  const std::vector<RowGeometry> rows = compute_rows(layout);
  const ImVec2 avail = ImGui::GetContentRegionAvail();

  for (std::size_t row_index = 0; row_index < rows.size(); ++row_index) {
    const RowGeometry& row = rows[row_index];
    const float row_height = avail.y * row.height_fraction;
    ImGui::PushID(static_cast<int>(row_index));

    for (std::size_t cell_index = 0; cell_index < row.cells.size(); ++cell_index) {
      const ZoneGeometry& cell = row.cells[cell_index];
      const float cell_width = avail.x * cell.width_fraction;

      ImGui::PushID(static_cast<int>(cell_index));
      if (cell.stack.size() == 1) {
        render_zone_frame(layout.zones[cell.stack.front().zone_index],
                          ImVec2(cell_width, row_height), state);
      } else {
        // Nested vertical split: stack the cell's zones top-to-bottom inside
        // a fixed-width column, each sized by its height_fraction of the row.
        ImGui::BeginChild("stack", ImVec2(cell_width, row_height), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoScrollbar);
        for (std::size_t i = 0; i < cell.stack.size(); ++i) {
          const StackedZone& item = cell.stack[i];
          ImGui::PushID(static_cast<int>(i));
          render_zone_frame(layout.zones[item.zone_index],
                            ImVec2(-FLT_MIN, row_height * item.height_fraction), state);
          ImGui::PopID();
        }
        ImGui::EndChild();
      }
      ImGui::PopID();

      if (cell_index + 1 < row.cells.size()) {
        ImGui::SameLine();
      }
    }

    ImGui::PopID();
  }
}

}  // namespace sonotron
