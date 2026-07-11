#include "transport_panel.hpp"

#include "imgui.h"

namespace sonotron {

void render_transport_panel(AppState& app_state, BrainSession& brain_session) {
  if (app_state.connected()) {
    ImGui::TextColored(ImVec4(0.25F, 0.85F, 0.35F, 1.0F), "* Connected");
  } else {
    ImGui::TextColored(ImVec4(0.85F, 0.30F, 0.30F, 1.0F), "* Disconnected");
  }

  ImGui::SameLine();
  if (ImGui::Button("Play")) {
    brain_session.send("transport start");
    app_state.note_transport_sent(true);
  }
  ImGui::SameLine();
  if (ImGui::Button("Stop")) {
    brain_session.send("transport stop");
    app_state.note_transport_sent(false);
  }
  ImGui::SameLine();
  if (ImGui::Button("Panic")) {
    brain_session.send("panic");
  }

  ImGui::SameLine();
  const char* transport_label = "stopped";
  if (app_state.transport() == AppState::Transport::kPlaying) {
    transport_label = "playing";
  } else if (app_state.transport() == AppState::Transport::kPaused) {
    transport_label = "paused";
  }
  ImGui::Text("| %s | Section: %s", transport_label, app_state.section().c_str());

  ImGui::SameLine();
  // Honest placeholder: no real position exists on the wire until the core
  // strand's kBeat/kPosition heartbeat lands (ux-workstation.md §11 P0-2).
  ImGui::TextDisabled("| bar -- . beat -- (playhead awaits core kBeat)");
}

}  // namespace sonotron
