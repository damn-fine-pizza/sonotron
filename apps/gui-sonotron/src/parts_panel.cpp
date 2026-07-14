#include "parts_panel.hpp"

#include <string>

#include "imgui.h"
#include "theme.hpp"

namespace sonotron {

namespace {

constexpr float kLatchSize = 22.0F;
constexpr int kVolumeMeterCells = 8;
const ImVec2 kVolumeMeterCellSize{6.0F, 10.0F};

// Toggle (components.jsx): a square M/S latch. Engaged: solid `tone` fill,
// bold glyph inverted to the dark surface (#0f0f0f); rest: FrameBg fill,
// secondary-gray glyph, --sn-border outline (FrameBorderSize=1 draws that
// globally, theme.cpp). Returns true on click.
bool render_latch(const char* glyph, bool engaged, const ImVec4& tone) {
  ImGui::PushStyleColor(ImGuiCol_Button, engaged ? tone : theme::kFrameBg);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, engaged ? tone : theme::kFrameBgHover);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, engaged ? tone : theme::kFrameBgActive);
  ImGui::PushStyleColor(ImGuiCol_Text, engaged ? theme::kWindowBg : theme::kTextSecondary);
  const bool clicked = ImGui::Button(glyph, ImVec2(kLatchSize, kLatchSize));
  ImGui::PopStyleColor(4);
  return clicked;
}

}  // namespace

void render_parts_panel(PartsModel& model, BrainSession& brain_session) {
  bool any_solo = false;
  for (std::size_t i = 0; i < PartsModel::kPartCount; ++i) {
    if (model.part(i).soloed) {
      any_solo = true;
      break;
    }
  }

  for (std::size_t i = 0; i < PartsModel::kPartCount; ++i) {
    ImGui::PushID(static_cast<int>(i));
    const PartInfo& info = model.part(i);
    const std::string wire_token(model.part_wire_token(i));

    // PartRow (components.jsx): dimmed when another part is soloed.
    const bool dimmed = any_solo && !info.soloed;
    if (dimmed) {
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.4F);
    }

    ImGui::TextColored(theme::kText, "%-7s", std::string(model.part_label(i)).c_str());
    ImGui::SameLine();

    const bool was_muted = info.muted;
    if (render_latch("M", was_muted, theme::kRed)) {
      model.toggle_mute(i);
      brain_session.send("part " + wire_token + " mute " + (!was_muted ? "on" : "off"));
    }
    ImGui::SameLine();
    const bool was_soloed = info.soloed;
    if (render_latch("S", was_soloed, theme::kAmber)) {
      model.toggle_solo(i);
      brain_session.send("part " + wire_token + " solo " + (!was_soloed ? "on" : "off"));
    }

    ImGui::SameLine();
    // Volume block meter (components.jsx's PartRow): no per-part volume
    // readback exists yet either (same gap as gm_program below), so this
    // renders the honest empty track rather than a fabricated level.
    theme::render_meter_cells(kVolumeMeterCells, /*filled=*/0, theme::kMeterFill,
                              kVolumeMeterCellSize);

    ImGui::SameLine();
    // Honest placeholder: no per-part program readback exists yet (§11.4).
    if (info.gm_program >= 0) {
      ImGui::TextColored(theme::kText, "gm %02d", info.gm_program);
    } else {
      ImGui::TextColored(theme::kTextMuted, "gm --");
    }

    if (dimmed) {
      ImGui::PopStyleVar();
    }

    ImGui::PopID();
  }
}

}  // namespace sonotron
