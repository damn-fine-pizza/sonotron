#include "intention_panel.hpp"

#include "imgui.h"

namespace sonotron {

void render_intention_panel(const AppState& app_state) {
  if (app_state.harmony_active()) {
    ImGui::TextColored(ImVec4(0.55F, 0.85F, 0.55F, 1.0F), "Intention (live)");
  } else {
    ImGui::TextDisabled("Intention (at rest)");
  }

  // Honest placeholders: no Director (node 10000) means there is no real
  // energy/tension/valence signal yet — pinned at zero rather than faked.
  ImGui::TextDisabled("energy   [----------] --");
  ImGui::TextDisabled("tension  [----------] --");
  ImGui::TextDisabled("valence  o unknown");
}

}  // namespace sonotron
