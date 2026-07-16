#include "parts_panel.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

#include "imgui.h"
#include "neon_widgets.hpp"
#include "theme.hpp"

namespace sonotron {

namespace {

// The 3 v02 PARTS "amount" knobs (spec §2c): DRUMS/BASS/CHORD. Amount is a
// local-only intent; mute/solo is per-track and lives on the grid rows (§2b).
struct V02Part {
  const char* label;
  ImVec4 color;
};

}  // namespace

void render_parts_panel(V02State& fx) {
  ImGui::TextColored(theme::kCyan, "PARTS");
  ImGui::SameLine();
  ImGui::TextColored(theme::kTextMuted, "  amount");
  ImGui::Spacing();

  const std::array<V02Part, 3> parts = {{
      {.label = "DRUMS", .color = theme::kCyan},
      {.label = "BASS", .color = theme::kBlue},
      {.label = "CHORD", .color = theme::kGreen},
  }};

  const float knob_sz = 54.0F;
  const float total = ImGui::GetContentRegionAvail().x;
  const float offset = std::max(0.0F, (total - (knob_sz * 3.0F + 24.0F)) * 0.5F);
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

  ImGui::BeginGroup();
  for (std::size_t i = 0; i < parts.size(); ++i) {
    const V02Part& p = parts[i];
    ImGui::PushID(static_cast<int>(i));
    neon::knob("amount", &fx.part_amount[i], p.color, p.label, knob_sz, fx.glow);
    ImGui::PopID();
    if (i + 1 < parts.size()) {
      ImGui::SameLine(0.0F, 12.0F);
    }
  }
  ImGui::EndGroup();
}

}  // namespace sonotron
