#include "transport_panel.hpp"

#include <algorithm>
#include <string>

#include "imgui.h"
#include "theme.hpp"

namespace sonotron {

namespace {

// Ghost-variant button chrome (components.jsx's Button `ghost`): a
// transparent rest fill (FrameBorderSize=1 + ImGuiCol_Border already draws
// the `--sn-border` outline globally, theme.cpp), a translucent-accent
// hover/press wash, and a secondary-gray label -- used for Stop, the
// non-primary transport verb on this row. Caller must PopStyleColor(4)
// after the widget.
void push_ghost_button_style() {
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0F, 0.0F, 0.0F, 0.0F));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(theme::kAccent.x, theme::kAccent.y, theme::kAccent.z, 0.18F));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        ImVec4(theme::kAccent.x, theme::kAccent.y, theme::kAccent.z, 0.30F));
  ImGui::PushStyleColor(ImGuiCol_Text, theme::kTextSecondary);
}

// Danger-variant button chrome (components.jsx's Button `danger`): a
// translucent-red rest fill strengthening to solid red on hover and a
// darkened red on press -- used for Panic, the one destructive verb here.
// Caller must PopStyleColor(3) after the widget.
void push_danger_button_style() {
  ImGui::PushStyleColor(ImGuiCol_Button,
                        ImVec4(theme::kRed.x, theme::kRed.y, theme::kRed.z, 0.34F));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::kRed);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(theme::kRed.x * 0.55F, theme::kRed.y * 0.55F,
                                                      theme::kRed.z * 0.55F, 1.0F));
}

// StatusDot (components.jsx): a filled circular dot -- the design's one
// deliberately non-square shape -- plus a colored connection label.
void render_status_dot(bool connected) {
  const ImVec4& color = connected ? theme::kGreenConnected : theme::kRed;
  const float line_height = ImGui::GetTextLineHeight();
  const float radius = line_height * 0.22F;
  const ImVec2 cursor = ImGui::GetCursorScreenPos();
  const ImVec2 center(cursor.x + radius, cursor.y + line_height * 0.5F);
  ImGui::GetWindowDrawList()->AddCircleFilled(center, radius,
                                              ImGui::ColorConvertFloat4ToU32(color));
  ImGui::Dummy(ImVec2(radius * 2.0F + 6.0F, line_height));
  ImGui::SameLine(0.0F, 4.0F);
  ImGui::TextColored(color, "%s", connected ? "Connected" : "Disconnected");
}

}  // namespace

void render_transport_panel(AppState& app_state, BrainSession& brain_session) {
  render_status_dot(app_state.connected());

  // Play (default accent chrome, from the global theme) / Stop (ghost) /
  // Panic (danger) -- components.jsx's Button variants, iconography from
  // readme.md's glyph set (play/stop markers).
  ImGui::SameLine();
  if (ImGui::Button("\xE2\x96\xB6 Play")) {
    brain_session.send("transport start");
    app_state.note_transport_sent(true);
  }

  ImGui::SameLine();
  push_ghost_button_style();
  const bool stop_clicked = ImGui::Button("\xE2\x96\xA0 Stop");
  ImGui::PopStyleColor(4);
  if (stop_clicked) {
    brain_session.send("transport stop");
    app_state.note_transport_sent(false);
  }

  ImGui::SameLine();
  push_danger_button_style();
  const bool panic_clicked = ImGui::Button("Panic");
  ImGui::PopStyleColor(3);
  if (panic_clicked) {
    brain_session.send("panic");
  }

  ImGui::SameLine();
  const char* transport_label = "stopped";
  if (app_state.transport() == AppState::Transport::kPlaying) {
    transport_label = "playing";
  } else if (app_state.transport() == AppState::Transport::kPaused) {
    transport_label = "paused";
  }
  ImGui::TextColored(theme::kTextSecondary, "| %s | Section: %s", transport_label,
                     app_state.section().c_str());

  // BeatReadout (components.jsx): "bar N . beat M .PP" -- bar/beat bold in
  // text, the sub-beat pulse dimmed to secondary; parked to a disabled
  // "bar -- . beat --" placeholder at rest.
  ImGui::SameLine();
  if (app_state.bar() == 0) {
    ImGui::TextColored(theme::kTextMuted, "| bar -- . beat --");
  } else {
    ImGui::TextColored(theme::kTextSecondary, "|");
    ImGui::SameLine(0.0F, 4.0F);
    theme::text_bold_colored(theme::kText, "bar %d . beat %d", app_state.bar(),
                             app_state.beat_num());
    ImGui::SameLine(0.0F, 2.0F);
    ImGui::TextColored(theme::kTextSecondary, ".%02d", app_state.pulse());
  }

  // Tempo nudge (user request): the BPM readout IS the control -- while
  // hovering it, the mouse wheel or the Up/Down arrows nudge the tempo by 1
  // BPM, sent as the `bpm <N>` L1 verb over the same BrainSession path (see
  // in_process_brain_session.cpp's command_line_to_command, mirroring hostrt's
  // own `bpm` verb for --control). Like the Transpose control below, the GUI
  // holds no authoritative state (app_state.hpp), so this value is local UI
  // intent only -- it can drift from the tempo a `style load` sets until a
  // live-bpm event is wired (follow-up). Clamped to the engine's 20..400 range.
  // Styled as a Slider-like readout (components.jsx's Slider: bold value,
  // secondary-gray unit label) even though it is not a drag/track control.
  static int bpm = 120;
  ImGui::SameLine();
  ImGui::BeginGroup();
  ImGui::TextColored(theme::kTextSecondary, "|");
  ImGui::SameLine(0.0F, 4.0F);
  theme::text_bold_colored(theme::kAccent, "%d", bpm);
  ImGui::SameLine(0.0F, 2.0F);
  ImGui::TextColored(theme::kTextSecondary, "BPM");
  ImGui::EndGroup();
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Tempo -- scroll the wheel or press Up/Down to nudge BPM");
    int delta = 0;
    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel > 0.0F) {
      delta += 1;
    } else if (wheel < 0.0F) {
      delta -= 1;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, /*repeat=*/true)) {
      delta += 1;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, /*repeat=*/true)) {
      delta -= 1;
    }
    if (delta != 0) {
      bpm = std::clamp(bpm + delta, 20, 400);
      brain_session.send("bpm " + std::to_string(bpm));
    }
  }

  // Phase-6 Theme 3 Item #1 (docs/reflections/phase6-theme3-master-
  // transpose-scope.md): a small, discoverable global-transpose control --
  // fire-and-forget over the same BrainSession command path `style load`
  // uses (in_process_brain_session.cpp's command_line_to_command / hostrt's
  // Shell both recognize `transpose <-12..12>`). The GUI holds no
  // authoritative state (app_state.hpp's own design note), so this control's
  // own value is local UI state only, not re-derived from the event stream.
  // Track/grab colors come from the global theme (FrameBg/SliderGrab).
  static int transpose_semitones = 0;
  ImGui::SetNextItemWidth(160.0F);
  if (ImGui::SliderInt("Transpose", &transpose_semitones, -12, 12)) {
    brain_session.send("transpose " + std::to_string(transpose_semitones));
  }
}

}  // namespace sonotron
