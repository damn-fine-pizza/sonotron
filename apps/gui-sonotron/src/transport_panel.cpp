#include "transport_panel.hpp"

#include <string>

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
  if (app_state.bar() == 0) {
    // No position yet: never played, or Stop parked the playhead (app_state
    // resets bar/beat/pulse to 0 on a "stopped" transport event).
    ImGui::TextDisabled("| bar -- . beat --");
  } else {
    // Live playhead (P0-2): bar/beat from the core's kBeat heartbeat, plus a
    // sub-beat pulse count so movement is visible WITHIN a beat, not only on
    // the beat boundary.
    ImGui::Text("| bar %d . beat %d .%02d", app_state.bar(), app_state.beat_num(),
                app_state.pulse());
  }

  // Phase-6 Theme 3 Item #1 (docs/reflections/phase6-theme3-master-
  // transpose-scope.md): a small, discoverable global-transpose control --
  // fire-and-forget over the same BrainSession command path `style load`
  // uses (in_process_brain_session.cpp's command_line_to_command / hostrt's
  // Shell both recognize `transpose <-12..12>`). The GUI holds no
  // authoritative state (app_state.hpp's own design note), so this control's
  // own value is local UI state only, not re-derived from the event stream.
  static int transpose_semitones = 0;
  ImGui::SetNextItemWidth(160.0F);
  if (ImGui::SliderInt("Transpose", &transpose_semitones, -12, 12)) {
    brain_session.send("transpose " + std::to_string(transpose_semitones));
  }
}

}  // namespace sonotron
