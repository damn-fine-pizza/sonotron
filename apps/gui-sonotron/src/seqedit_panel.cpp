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

  // Right-aligned mode tabs, sized to fit BOTH labels fully (a SmallButton is
  // text + 2*FramePadding.x wide; reserve exactly that for each so neither
  // "piano-roll" nor "step" is clipped, at any DPI/font size).
  ImGui::SameLine();
  const float pad2 = ImGui::GetStyle().FramePadding.x * 2.0F;
  const float w_pr = ImGui::CalcTextSize("piano-roll").x + pad2;
  const float w_st = ImGui::CalcTextSize("step").x + pad2;
  const float tab_gap = 6.0F;
  ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - (w_pr + tab_gap + w_st));
  if (mode_tab("piano-roll", model.view() == SeqEditView::kPianoRoll)) {
    model.set_view(SeqEditView::kPianoRoll);
  }
  ImGui::SameLine(0.0F, tab_gap);
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

  // The pad row is the only AUDIO row (grid_panel.cpp's kRows[...].audio) --
  // its opened clip renders as a waveform, never a MIDI piano-roll. Every
  // other row renders the existing full piano-roll of track-colored blocks
  // from the SHARED ClipPattern (the same generator the launch-cell
  // mini-preview crops from) so the open clip's editor content corresponds
  // exactly to its cell preview. STEP left->right, PITCH low->high (pitch 0
  // at the bottom), matching the cell.
  if (fx.open_audio) {
    const ImVec2 in0(p0.x + 4.0F, p0.y + 4.0F);
    const ImVec2 in1(p1.x - 4.0F, p1.y - 4.0F);
    neon::clip_preview_waveform(dl, in0, in1, neon::hash_label(model.clip_label()), track_color);
  } else {
    const neon::ClipPattern pat = neon::clip_pattern(neon::hash_label(model.clip_label()));
    const int kSteps = neon::ClipPattern::kSteps;
    const int kPitches = neon::ClipPattern::kPitches;
    const float cw = avail.x / static_cast<float>(kSteps);
    const float rh = avail.y / static_cast<float>(kPitches);
    for (int step = 0; step < kSteps; ++step) {
      const int pitch = pat.pitch[step];
      if (pitch < 0) {
        continue;
      }
      const ImVec2 b0(p0.x + static_cast<float>(step) * cw + 2.0F,
                      p0.y + static_cast<float>(kPitches - 1 - pitch) * rh + 2.0F);
      const ImVec2 b1(b0.x + cw - 4.0F, b0.y + rh - 4.0F);
      if (fx.glow) {
        neon::glow_rect(dl, b0, b1, track_color, 3.0F, 0.7F, fx.glow);
      }
      dl->AddRectFilled(b0, b1, neon::u32(track_color, 0.85F), 3.0F);
    }
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
