#include "intention_panel.hpp"

#include <array>
#include <cstdio>

#include "imgui.h"

namespace sonotron {

namespace {

// Per-axis accent color, chosen so the three axes read apart at a glance:
// energy = amber, tension = red, valence = teal. Indexed by kEnergy/
// kTension/kValence so the color and the axis stay in lockstep.
constexpr std::array<ImU32, 3> kAxisColors = {
    IM_COL32(0xF0, 0xA8, 0x30, 0xFF),  // energy  — amber
    IM_COL32(0xE0, 0x50, 0x50, 0xFF),  // tension — red
    IM_COL32(0x40, 0xC0, 0xA0, 0xFF),  // valence — teal
};

constexpr ImU32 kTrackColor = IM_COL32(38, 38, 46, 255);
constexpr ImU32 kTargetColor = IM_COL32(235, 235, 240, 255);
constexpr ImU32 kTextColor = IM_COL32(245, 245, 245, 255);

// Widest numeric readout the row can show ("100 -> 100"), used to reserve a
// fixed value column to the RIGHT of the bar so the numbers never collide
// with the target marker and stay right-aligned across all three axes.
constexpr const char* kWidestReadout = "100 -> 100";

// Draws one axis row: a fixed-width label, a track bar filled to `current`
// in the accent color with a bright vertical marker at `target`, and a
// "NN -> NN" readout in a fixed column to the bar's right. The bar is drawn
// by hand into the window draw list; an ImGui::Dummy then reserves the same
// space so ImGui's cursor advances past it for the next row.
void draw_axis_row(const IntentionAxis& axis, ImU32 accent, float label_width, float value_width) {
  const float current = clamp_unit(axis.current);
  const float target = clamp_unit(axis.target);

  const ImGuiStyle& style = ImGui::GetStyle();
  ImDrawList* draw = ImGui::GetWindowDrawList();

  ImGui::TextUnformatted(axis.label);
  ImGui::SameLine(label_width);

  const float bar_height = ImGui::GetTextLineHeight();
  const float row_width = ImGui::GetContentRegionAvail().x;
  const float gap = style.ItemInnerSpacing.x * 2.0F;
  const float bar_width = row_width - value_width - gap;
  const ImVec2 p0 = ImGui::GetCursorScreenPos();
  const ImVec2 p1 = ImVec2(p0.x + bar_width, p0.y + bar_height);
  const float rounding = bar_height * 0.25F;

  // Track, then the current fill on top of it.
  draw->AddRectFilled(p0, p1, kTrackColor, rounding);
  const float current_x = p0.x + bar_width * current;
  if (current_x > p0.x) {
    draw->AddRectFilled(p0, ImVec2(current_x, p1.y), accent, rounding);
  }

  // Target marker: a vertical line spilling slightly past the bar edges so
  // it stays visible even when it sits on top of the current fill.
  const float target_x = p0.x + bar_width * target;
  draw->AddLine(ImVec2(target_x, p0.y - 2.0F), ImVec2(target_x, p1.y + 2.0F), kTargetColor, 2.0F);

  // Numeric readout in its own right-hand column (ASCII "->" — the atlas
  // only rasterizes Basic Latin, no U+2192 arrow glyph yet).
  std::array<char, 32> readout{};
  std::snprintf(readout.data(), readout.size(), "%d -> %d",
                static_cast<int>(current * 100.0F + 0.5F),
                static_cast<int>(target * 100.0F + 0.5F));
  const ImVec2 text_size = ImGui::CalcTextSize(readout.data());
  const ImVec2 text_pos =
      ImVec2(p1.x + value_width + gap - text_size.x, p0.y + (bar_height - text_size.y) * 0.5F);
  draw->AddText(text_pos, kTextColor, readout.data());

  ImGui::Dummy(ImVec2(row_width, bar_height + style.ItemSpacing.y));
}

}  // namespace

void render_intention_panel(const IntentionState& state) {
  // Widest label anchors every bar's left edge; widest readout reserves the
  // right column — both fixed so the three tracks align and the numbers
  // right-align regardless of their digit count.
  const ImGuiStyle& style = ImGui::GetStyle();
  const float label_width = ImGui::CalcTextSize("TENSION").x + style.ItemInnerSpacing.x * 2.0F;
  const float value_width = ImGui::CalcTextSize(kWidestReadout).x;

  for (std::size_t i = 0; i < state.axes.size(); ++i) {
    draw_axis_row(state.axes[i], kAxisColors[i], label_width, value_width);
  }
}

}  // namespace sonotron
