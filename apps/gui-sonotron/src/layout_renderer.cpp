#include "layout_renderer.hpp"

#include <algorithm>
#include <cmath>

#include "app_state.hpp"
#include "browser_panel.hpp"
#include "grid_panel.hpp"
#include "imgui.h"
#include "intention_panel.hpp"
#include "neon_widgets.hpp"
#include "parts_panel.hpp"
#include "seqedit_panel.hpp"
#include "theme.hpp"
#include "transport_panel.hpp"

// v02 workstation renderer (v02-workstation-spec.md). This replaces the old
// JSON-fraction zone grid with the fixed 3-band v02 layout (transport rack
// 58px / working row flex / sequence edit 172px). The `Layout` argument is
// still accepted so main.cpp's call site, the JSON persistence, the window
// title and the configurable font size are untouched -- but the v02 layout is
// fixed by design, so the per-zone fractions are no longer consulted here.
// layout_model.cpp / layout_json.cpp (and their tests) are unchanged.

namespace sonotron {

namespace {

constexpr float kBandGap = 8.0F;
constexpr float kTransportH = 58.0F;
constexpr float kSeqEditH = 172.0F;
constexpr float kBrowserW = 210.0F;
constexpr float kRailW = 288.0F;

// Opens one rounded neon zone panel child; `scrolls` false pins it (fixed
// content must not wheel-scroll the few pixels its padding overflows).
void begin_zone(const char* id, const ImVec2& size, bool scrolls) {
  ImGuiWindowFlags flags =
      scrolls ? ImGuiWindowFlags_None
              : (ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::BeginChild(id, size, ImGuiChildFlags_Borders, flags);
}

void render_master_vu(const AppState& app_state, V02State& fx) {
  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::kInsetBg);
  ImGui::PushStyleColor(ImGuiCol_Border, theme::kBorderCyan);
  ImGui::BeginChild("master_vu", ImVec2(0.0F, 62.0F), ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  const bool playing = app_state.transport() == AppState::Transport::kPlaying;
  ImGui::TextColored(theme::kCyan, "MASTER");
  ImGui::SameLine();
  if (playing) {
    // Local-only animated dB readout -- there is no level metering on the wire.
    const float db = -9.0F + 6.0F * std::fabs(std::sin(fx.time * 2.0F));
    theme::text_bold_colored(theme::kGreen, "  %.1f dB", static_cast<double>(db));
  } else {
    ImGui::TextColored(theme::kTextMuted, "  \xE2\x80\x94");
  }
  ImGui::Spacing();
  neon::master_vu("vu", ImVec2(ImGui::GetContentRegionAvail().x, 22.0F), playing, fx.time, fx.glow);
  ImGui::EndChild();
  ImGui::PopStyleColor(2);
}

void render_rail(WorkstationState& state) {
  render_intention_panel(state.app_state, state.fx);
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  render_parts_panel(state.fx);
  render_master_vu(state.app_state, state.fx);
}

}  // namespace

void render_layout(const Layout& layout, WorkstationState& state) {
  (void)layout;  // v02 layout is fixed by design; see the file header comment.
  V02State& fx = state.fx;
  fx.time = static_cast<float>(ImGui::GetTime());
  fx.playing = state.app_state.transport() == AppState::Transport::kPlaying;

  // Neon background behind everything (radial glows + 40px grid overlay).
  const ImVec2 win_min = ImGui::GetWindowPos();
  const ImVec2 win_max(win_min.x + ImGui::GetWindowSize().x, win_min.y + ImGui::GetWindowSize().y);
  neon::background(ImGui::GetWindowDrawList(), win_min, win_max);

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(kBandGap, kBandGap));

  const ImVec2 avail = ImGui::GetContentRegionAvail();
  const float work_h = std::max(120.0F, avail.y - kTransportH - kSeqEditH - 2.0F * kBandGap);

  // Band 1: transport rack.
  begin_zone("band_transport", ImVec2(0.0F, kTransportH), /*scrolls=*/false);
  render_transport_panel(state.app_state, state.brain_session, fx);
  ImGui::EndChild();

  // Band 2: working row -- browser | hero | rail.
  const float hero_w = std::max(200.0F, avail.x - kBrowserW - kRailW - 2.0F * kBandGap);
  begin_zone("band_browser", ImVec2(kBrowserW, work_h), /*scrolls=*/false);
  render_browser_panel(state.browser, state.brain_session, state.app_state, fx);
  ImGui::EndChild();

  ImGui::SameLine(0.0F, kBandGap);
  begin_zone("band_hero", ImVec2(hero_w, work_h), /*scrolls=*/false);
  render_grid_panel(state.grid, state.seqedit, state.parts, state.brain_session, state.app_state,
                    fx);
  ImGui::EndChild();

  ImGui::SameLine(0.0F, kBandGap);
  begin_zone("band_rail", ImVec2(kRailW, work_h), /*scrolls=*/true);
  render_rail(state);
  ImGui::EndChild();

  // Band 3: sequence edit.
  begin_zone("band_seqedit", ImVec2(0.0F, kSeqEditH), /*scrolls=*/false);
  render_seqedit_panel(state.seqedit, fx);
  ImGui::EndChild();

  ImGui::PopStyleVar();
}

}  // namespace sonotron
