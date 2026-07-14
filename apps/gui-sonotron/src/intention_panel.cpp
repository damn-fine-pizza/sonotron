#include "intention_panel.hpp"

#include "imgui.h"
#include "theme.hpp"

namespace sonotron {

namespace {

constexpr int kMeterCells = 10;
const ImVec2 kMeterCellSize{6.0F, 10.0F};

// Meter (components.jsx): a labeled fixed-cell block bar. The
// energy/tension/valence signal has no Director (node 10000) yet, so every
// call here renders the honest "disabled" state -- 0 cells lit, value text
// "--" -- rather than inferring a fake reading.
void render_placeholder_meter(const char* label, const ImVec4& tone) {
  ImGui::TextColored(theme::kTextSecondary, "%-8s", label);
  ImGui::SameLine();
  theme::render_meter_cells(kMeterCells, /*filled=*/0, tone, kMeterCellSize);
  ImGui::SameLine();
  ImGui::TextColored(theme::kTextMuted, "--");
}

}  // namespace

void render_intention_panel(const AppState& app_state) {
  const bool live = app_state.harmony_active();
  if (live) {
    ImGui::TextColored(theme::kGreenSoft, "Intention (live)");
  } else {
    ImGui::TextColored(theme::kTextMuted, "Intention (at rest)");
  }

  // ChordReadout (components.jsx), THE most important surface: bold GREEN
  // "follows" = the chord held this bar, lit only while the activity gate
  // is open (harmony_active()); bold AMBER "next" = the shift-staged chord
  // pending for next bar, independent of the gate (staging can happen at
  // rest). Dims to muted "--" when absent. Colour semantics:
  // ux-workstation.md §10.
  ImGui::TextColored(theme::kTextMuted, "follows");
  ImGui::SameLine();
  if (live && app_state.chord_followed_current_valid()) {
    theme::text_bold_colored(theme::kGreen, "%s", app_state.chord_followed_current().c_str());
  } else {
    ImGui::TextColored(theme::kTextMuted, "--");
  }

  ImGui::TextColored(theme::kTextMuted, "next   ");
  ImGui::SameLine();
  if (app_state.chord_followed_next_valid()) {
    theme::text_bold_colored(theme::kAmber, "%s", app_state.chord_followed_next().c_str());
  } else {
    ImGui::TextColored(theme::kTextMuted, "--");
  }

  ImGui::Spacing();

  // energy/tension/valence Meters (workstation-layout.md §3c): honest
  // placeholders -- no Director signal exists yet -- pinned at zero/"--"
  // rather than faked, each in its own tone (accent / pending-amber /
  // ok-green). NOTE: components.jsx's Meter doc also describes these as
  // "conductable" (click/drag an axis to set the intention target) once a
  // real target-setting command exists on the wire; that wiring is not
  // present here, so no click/drag hint is rendered -- this codebase's own
  // honesty convention (see this file's original placeholder comments)
  // treats an interaction hint for a non-interactive control as worse than
  // no hint at all.
  render_placeholder_meter("energy", theme::kAccent);
  render_placeholder_meter("tension", theme::kAmber);
  render_placeholder_meter("valence", theme::kGreen);
}

}  // namespace sonotron
