#include "parts_panel.hpp"

#include <string>

#include "imgui.h"

namespace sonotron {

void render_parts_panel(PartsModel& model, BrainSession& brain_session) {
  for (std::size_t i = 0; i < PartsModel::kPartCount; ++i) {
    ImGui::PushID(static_cast<int>(i));
    const PartInfo& info = model.part(i);
    const std::string wire_token(model.part_wire_token(i));

    bool muted = info.muted;
    if (ImGui::Checkbox("M", &muted)) {
      model.toggle_mute(i);
      brain_session.send("part " + wire_token + " mute " + (muted ? "on" : "off"));
    }
    ImGui::SameLine();
    bool soloed = info.soloed;
    if (ImGui::Checkbox("S", &soloed)) {
      model.toggle_solo(i);
      brain_session.send("part " + wire_token + " solo " + (soloed ? "on" : "off"));
    }
    ImGui::SameLine();
    ImGui::Text("%s", std::string(model.part_label(i)).c_str());
    ImGui::SameLine();
    // Honest placeholder: no per-part program readback exists yet (§11.4).
    ImGui::TextDisabled("gm --");

    ImGui::PopID();
  }
}

}  // namespace sonotron
