#include "seqedit_panel.hpp"

#include <string>

#include "imgui.h"
#include "track_roles.hpp"

namespace sonotron {

namespace {

void render_toolbar(SeqEditModel& model) {
  ImGui::TextUnformatted("Part:");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(100.0F);
  const std::string current_label(model.part_label());
  if (ImGui::BeginCombo("##seqedit_part", current_label.c_str())) {
    for (std::size_t i = 0; i < kTrackRoleCount; ++i) {
      const bool selected = (i == model.part_index());
      if (ImGui::Selectable(std::string(kTrackRoleLabels[i]).c_str(), selected)) {
        model.set_part_index(i);
      }
    }
    ImGui::EndCombo();
  }

  ImGui::SameLine();
  ImGui::TextUnformatted("Clip:");
  ImGui::SameLine();
  ImGui::TextUnformatted(model.clip_label().c_str());

  ImGui::SameLine();
  bool armed = model.record_armed();
  if (ImGui::Checkbox("rec", &armed)) {
    model.set_record_armed(armed);
  }

  ImGui::SameLine();
  ImGui::Text("grid 1/%d", model.grid_division());

  ImGui::SameLine();
  bool piano_roll = model.view() == SeqEditView::kPianoRoll;
  if (ImGui::RadioButton("piano-roll", piano_roll)) {
    model.set_view(SeqEditView::kPianoRoll);
  }
  ImGui::SameLine();
  if (ImGui::RadioButton("step", !piano_roll)) {
    model.set_view(SeqEditView::kStep);
  }
}

}  // namespace

void render_seqedit_panel(SeqEditModel& model) {
  render_toolbar(model);
  ImGui::Separator();
  // Honest placeholder: no live Track step data is modeled/rendered yet
  // (§7 A7's `track step ...` verb — a later slice).
  ImGui::TextDisabled("(note canvas awaits Track step editing)");
}

}  // namespace sonotron
