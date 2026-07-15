#include "intention_panel.hpp"

#include <algorithm>
#include <string>

#include "imgui.h"
#include "neon_widgets.hpp"
#include "theme.hpp"

namespace sonotron {

namespace {

// A chord card: a rounded inset with a small label and a big glowing chord.
void chord_card(const char* id, float width, const char* label, const ImVec4& color,
                const char* chord, bool lit, bool glow) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::kInsetBg);
  ImGui::PushStyleColor(ImGuiCol_Border, theme::kBorderCyan);
  ImGui::BeginChild(id, ImVec2(width, 60.0F), ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::TextColored(theme::kTextMuted, "%s", label);

  const ImVec2 p = ImGui::GetCursorScreenPos();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 ts = ImGui::CalcTextSize(chord);
  const ImVec2 at(p.x + 2.0F, p.y + 4.0F);
  if (lit && glow) {
    neon::glow_rect(dl, at, ImVec2(at.x + ts.x, at.y + ts.y), color, 2.0F, 1.0F, glow);
  }
  dl->AddText(at, neon::u32(lit ? color : theme::kTextDim), chord);
  ImGui::EndChild();
  ImGui::PopStyleColor(2);
}

}  // namespace

void render_intention_panel(const AppState& app_state, V02State& fx) {
  const bool live = app_state.harmony_active();
  ImGui::TextColored(theme::kGreen, "INTENTION");
  ImGui::SameLine();
  if (live) {
    ImGui::TextColored(theme::kGreen, "  \xE2\x97\x8F live");
  } else {
    ImGui::TextColored(theme::kTextMuted, "  at rest");
  }
  ImGui::Spacing();

  // FOLLOWS / NEXT chord cards (real chord-followed events).
  const float gap = 8.0F;
  const float card_w = (ImGui::GetContentRegionAvail().x - gap) * 0.5F;
  const bool follows_lit = live && app_state.chord_followed_current_valid();
  const char* follows = follows_lit ? app_state.chord_followed_current().c_str() : "\xE2\x80\x94";
  chord_card("card_follows", card_w, "FOLLOWS", theme::kGreen, follows, follows_lit, fx.glow);

  ImGui::SameLine(0.0F, gap);
  std::string next_label = "NEXT";
  if (fx.playing) {
    const int cd = 4 - ((std::max(1, app_state.beat_num()) - 1) % 4);
    next_label = "NEXT \xE2\x86\x92" + std::to_string(cd);
  }
  const bool next_lit = app_state.chord_followed_next_valid();
  const char* next = next_lit ? app_state.chord_followed_next().c_str() : "\xE2\x80\x94";
  chord_card("card_next", card_w, next_label.c_str(), theme::kAmber, next, next_lit, fx.glow);

  ImGui::Spacing();

  // XY pad: valence horizontal, energy vertical.
  const float xy_w = ImGui::GetContentRegionAvail().x;
  neon::xy_pad("xy", &fx.valence, &fx.energy, ImVec2(xy_w, xy_w / 1.7F), fx.glow);

  ImGui::Spacing();

  // Intention knobs. ENERGY/VALENCE share the XY backing values.
  const float knob_sz = 54.0F;
  const float total = ImGui::GetContentRegionAvail().x;
  const float offset = std::max(0.0F, (total - (knob_sz * 3.0F + 24.0F)) * 0.5F);
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
  ImGui::BeginGroup();
  neon::knob("k_energy", &fx.energy, theme::kCyan, "ENERGY", knob_sz, fx.glow);
  ImGui::SameLine(0.0F, 12.0F);
  neon::knob("k_tension", &fx.tension, theme::kAmber, "TENSION", knob_sz, fx.glow);
  ImGui::SameLine(0.0F, 12.0F);
  neon::knob("k_valence", &fx.valence, theme::kGreen, "VALENCE", knob_sz, fx.glow);
  ImGui::EndGroup();
}

}  // namespace sonotron
