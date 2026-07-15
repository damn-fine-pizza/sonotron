#include "seqedit_panel.hpp"

#include <cmath>
#include <cstdint>
#include <string>

#include "imgui.h"
#include "neon_widgets.hpp"
#include "theme.hpp"

namespace sonotron {

namespace {

// A small mode tab: cyan fill when active.
bool mode_tab(const char* label, bool active) {
  ImGui::PushStyleColor(ImGuiCol_Button,
                        active ? ImVec4(theme::kCyan.x, theme::kCyan.y, theme::kCyan.z, 0.22F)
                               : theme::kFrameBg);
  ImGui::PushStyleColor(ImGuiCol_Text, active ? theme::kCyan : theme::kTextSecondary);
  const bool clicked = ImGui::SmallButton(label);
  ImGui::PopStyleColor(2);
  return clicked;
}

float pr_rand(std::uint32_t& s) {
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return static_cast<float>(s & 0xFFFFFFu) / static_cast<float>(0xFFFFFF);
}

}  // namespace

void render_seqedit_panel(SeqEditModel& model, const V02State& fx) {
  const bool open = fx.open_cell >= 0;
  const ImVec4 track_color =
      (fx.open_row >= 0 && fx.open_row < static_cast<int>(theme::kV02TrackColor.size()))
          ? theme::kV02TrackColor[fx.open_row]
          : theme::kCyan;

  // Header.
  ImGui::TextColored(theme::kCyan, "SEQUENCE EDIT");
  ImGui::SameLine(0.0F, 12.0F);
  ImGui::TextColored(theme::kTextMuted, "part");
  ImGui::SameLine(0.0F, 4.0F);
  ImGui::TextColored(track_color, "%s", std::string(model.part_label()).c_str());
  ImGui::SameLine(0.0F, 12.0F);
  ImGui::TextColored(theme::kTextMuted, "clip");
  ImGui::SameLine(0.0F, 4.0F);
  ImGui::TextColored(theme::kText, "%s", model.clip_label().c_str());
  ImGui::SameLine(0.0F, 12.0F);
  ImGui::TextColored(theme::kTextMuted, "grid 1/%d", model.grid_division());

  // Right-aligned mode tabs.
  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 130.0F);
  if (mode_tab("piano-roll", model.view() == SeqEditView::kPianoRoll)) {
    model.set_view(SeqEditView::kPianoRoll);
  }
  ImGui::SameLine(0.0F, 4.0F);
  if (mode_tab("step", model.view() == SeqEditView::kStep)) {
    model.set_view(SeqEditView::kStep);
  }
  ImGui::Spacing();

  // Canvas.
  ImGui::BeginChild("seq_canvas", ImVec2(0, 0), ImGuiChildFlags_None,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  const ImVec2 p0 = ImGui::GetCursorScreenPos();
  const ImVec2 avail = ImGui::GetContentRegionAvail();
  const ImVec2 p1(p0.x + avail.x, p0.y + avail.y);
  ImDrawList* dl = ImGui::GetWindowDrawList();

  dl->AddRectFilled(p0, p1, neon::u32(theme::kInsetBg), 6.0F);

  // Vertical bar guides (8 divisions).
  for (int i = 1; i < 8; ++i) {
    const float x = p0.x + avail.x * static_cast<float>(i) / 8.0F;
    dl->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y), neon::u32(theme::kCyan, 0.06F), 1.0F);
  }

  if (!open) {
    const char* hint = "click a clip in the Repeat Zone to open it here";
    const ImVec2 ts = ImGui::CalcTextSize(hint);
    dl->AddText(ImVec2(p0.x + (avail.x - ts.x) * 0.5F, p0.y + (avail.y - ts.y) * 0.5F),
                neon::u32(theme::kTextMuted), hint);
    ImGui::EndChild();
    return;
  }

  // 16x8 piano-roll of track-colored blocks, deterministic from the clip label.
  constexpr int kSteps = 16;
  constexpr int kPitches = 8;
  const float cw = avail.x / kSteps;
  const float rh = avail.y / kPitches;
  std::uint32_t s = neon::hash_label(model.clip_label());
  for (int step = 0; step < kSteps; ++step) {
    if (pr_rand(s) < 0.4F) {
      continue;
    }
    const int pitch = static_cast<int>(pr_rand(s) * kPitches) % kPitches;
    const ImVec2 b0(p0.x + static_cast<float>(step) * cw + 2.0F,
                    p0.y + static_cast<float>(pitch) * rh + 2.0F);
    const ImVec2 b1(b0.x + cw - 4.0F, b0.y + rh - 4.0F);
    if (fx.glow) {
      neon::glow_rect(dl, b0, b1, track_color, 3.0F, 0.7F, fx.glow);
    }
    dl->AddRectFilled(b0, b1, neon::u32(track_color, 0.85F), 3.0F);
  }

  // Green playhead sweeping L->R while the opened clip plays.
  if (fx.playing) {
    const float phase = std::fmod(fx.time, 2.0F) / 2.0F;
    const float x = p0.x + phase * avail.x;
    dl->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y), neon::u32(theme::kGreen), 1.5F);
  }

  ImGui::EndChild();
}

}  // namespace sonotron
