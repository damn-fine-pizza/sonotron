#include "intention_panel.hpp"

#include "imgui.h"

namespace sonotron {

void render_intention_panel(const AppState& app_state) {
  const bool live = app_state.harmony_active();
  if (live) {
    ImGui::TextColored(ImVec4(0.55F, 0.85F, 0.55F, 1.0F), "Intention (live)");
  } else {
    ImGui::TextDisabled("Intention (at rest)");
  }

  // Harmony visualizer (gap P0-1, kChordFollowed): GREEN = the chord followed
  // this bar, lit only while the activity gate is open (harmony_active());
  // AMBER = the shift-staged chord pending for next bar, independent of the
  // gate (staging can happen at rest). Colour semantics: ux-workstation.md §10.
  if (live && app_state.chord_followed_current_valid()) {
    ImGui::TextColored(ImVec4(0.30F, 0.85F, 0.30F, 1.0F), "follows  %s",
                       app_state.chord_followed_current().c_str());
  } else {
    ImGui::TextDisabled("follows  --");
  }
  if (app_state.chord_followed_next_valid()) {
    ImGui::TextColored(ImVec4(0.90F, 0.70F, 0.20F, 1.0F), "next     %s",
                       app_state.chord_followed_next().c_str());
  } else {
    ImGui::TextDisabled("next     --");
  }

  // Honest placeholders: no Director (node 10000) means there is no real
  // energy/tension/valence signal yet — pinned at zero rather than faked.
  ImGui::TextDisabled("energy   [----------] --");
  ImGui::TextDisabled("tension  [----------] --");
  ImGui::TextDisabled("valence  o unknown");
}

}  // namespace sonotron
