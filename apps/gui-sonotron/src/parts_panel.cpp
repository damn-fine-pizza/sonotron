#include "parts_panel.hpp"

#include <array>
#include <cstddef>
#include <string>

#include "imgui.h"
#include "neon_widgets.hpp"
#include "theme.hpp"

namespace sonotron {

namespace {

// The 3 v02 PARTS knobs (spec §2c): DRUMS/BASS/CHORD, mapped to the GridModel/
// PartsModel role rows whose wire tokens the M/S latches send.
struct V02Part {
  const char* label;
  std::size_t role_index;  // drums=0, bass=2, chord1=3 (track_roles order)
  ImVec4 color;
};

// A tiny M/S latch: solid tone when engaged, dark inset otherwise.
bool latch(const char* glyph, bool engaged, const ImVec4& tone) {
  ImGui::PushStyleColor(ImGuiCol_Button, engaged ? tone : theme::kFrameBg);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, engaged ? tone : theme::kFrameBgHover);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, engaged ? tone : theme::kFrameBgActive);
  ImGui::PushStyleColor(ImGuiCol_Text, engaged ? theme::kAppBg : theme::kTextSecondary);
  const bool clicked = ImGui::Button(glyph, ImVec2(20.0F, 18.0F));
  ImGui::PopStyleColor(4);
  return clicked;
}

}  // namespace

void render_parts_panel(PartsModel& model, BrainSession& brain_session, V02State& fx) {
  ImGui::TextColored(theme::kCyan, "PARTS");
  ImGui::SameLine();
  ImGui::TextColored(theme::kTextMuted, "  amount");
  ImGui::Spacing();

  const std::array<V02Part, 3> parts = {{
      {"DRUMS", 0, theme::kCyan},
      {"BASS", 2, theme::kBlue},
      {"CHORD", 3, theme::kGreen},
  }};

  const float knob_sz = 54.0F;
  const float total = ImGui::GetContentRegionAvail().x;
  const float offset = std::max(0.0F, (total - (knob_sz * 3.0F + 24.0F)) * 0.5F);
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

  ImGui::BeginGroup();
  for (std::size_t i = 0; i < parts.size(); ++i) {
    const V02Part& p = parts[i];
    ImGui::PushID(static_cast<int>(i));
    ImGui::BeginGroup();
    neon::knob("amount", &fx.part_amount[i], p.color, p.label, knob_sz, fx.glow);

    // Compact M / S latches beneath the knob (real verbs).
    const PartInfo& info = model.part(p.role_index);
    const std::string token(model.part_wire_token(p.role_index));
    const bool was_muted = info.muted;
    if (latch("M", was_muted, theme::kPink)) {
      model.toggle_mute(p.role_index);
      brain_session.send("part " + token + " mute " + (!was_muted ? "on" : "off"));
    }
    ImGui::SameLine(0.0F, 4.0F);
    const bool was_soloed = info.soloed;
    if (latch("S", was_soloed, theme::kAmber)) {
      model.toggle_solo(p.role_index);
      brain_session.send("part " + token + " solo " + (!was_soloed ? "on" : "off"));
    }
    ImGui::EndGroup();
    ImGui::PopID();
    if (i + 1 < parts.size()) {
      ImGui::SameLine(0.0F, 12.0F);
    }
  }
  ImGui::EndGroup();
}

}  // namespace sonotron
