#include "layout_renderer.hpp"

#include <algorithm>
#include <cfloat>
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

// Neon workstation renderer (v02-workstation-spec.md). This replaces the old
// JSON-fraction zone grid with a fixed 3-band layout (transport rack 58px /
// working row flex / sequence edit, user-resizable). The `Layout` argument is
// still accepted so main.cpp's call site, the JSON persistence, the window
// title and the configurable font size are untouched -- but the band
// arrangement itself is fixed by design, so the per-zone fractions are no
// longer consulted here. layout_model.cpp / layout_json.cpp (and their
// tests) are unchanged.
//
// Owner items #9/#10: the browser, rail and sequence-edit bands can each be
// collapsed to a thin strip (freed space is reclaimed by their neighbor --
// the hero grid for browser/rail, the working row for sequence edit), and
// the sequence-edit band's height is user-draggable via a splitter instead
// of a hard-pinned constant. See UiState's `*_collapsed` / `seqedit_height`
// fields for the persisted-in-memory state.

namespace sonotron {

namespace {

constexpr float kBandGap = 8.0F;
constexpr float kTransportH = 58.0F;
constexpr float kBrowserW = 210.0F;
constexpr float kRailW = 288.0F;
constexpr float kCollapsedStripW = 28.0F;
constexpr float kCollapsedStripH = 28.0F;
constexpr float kSplitterThickness = 6.0F;
constexpr float kSeqEditMinH = 100.0F;
constexpr float kMinWorkH = 120.0F;

// Opens one rounded neon zone panel child; `scrolls` false pins it (fixed
// content must not wheel-scroll the few pixels its padding overflows).
void begin_zone(const char* id, const ImVec2& size, bool scrolls) {
  ImGuiWindowFlags flags =
      scrolls ? ImGuiWindowFlags_None
              : (ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::BeginChild(id, size, ImGuiChildFlags_Borders, flags);
}

// Small caret toggle button ("collapse"/"expand" affordance, owner item #9)
// drawn at the current cursor position; flips `collapsed` in place on click.
// Shared by all three collapsible bands below. ASCII glyphs ("+"/"-") rather
// than a Unicode caret, deliberately, to keep this a plain, low-risk affordance
// for a first pass (owner: "don't over-engineer").
void render_collapse_toggle(bool& collapsed, const char* id) {
  ImGui::PushID(id);
  if (ImGui::SmallButton(collapsed ? "+" : "-")) {
    collapsed = !collapsed;
  }
  ImGui::PopID();
}

// Drag splitter for the Sequence Edit band (owner item #10): a thin,
// full-width invisible-until-hovered strip directly above the band. Dragging
// it adjusts fx.seqedit_height by the INVERSE of the vertical mouse delta,
// because Sequence Edit sits at the BOTTOM of the layout -- dragging the
// splitter UP (negative mouse delta.y) must GROW the band, dragging it DOWN
// must SHRINK it. Clamped to [min_h, max_h] on every dragged frame so a fast
// drag can never leave the band, or the working row above it, smaller than
// their own floors.
void render_seqedit_splitter(UiState& fx, float min_h, float max_h) {
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0F, 0.0F, 0.0F, 0.0F));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::kBorderCyan);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme::kCyan);
  ImGui::Button("##seqedit_splitter", ImVec2(-FLT_MIN, kSplitterThickness));
  if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    fx.seqedit_height = std::clamp(fx.seqedit_height - ImGui::GetIO().MouseDelta.y, min_h, max_h);
  }
  if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
  }
  ImGui::PopStyleColor(3);
}

void render_master_vu(const AppState& app_state, UiState& fx) {
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
  (void)layout;  // layout is fixed by design; see the file header comment.
  UiState& fx = state.fx;
  fx.time = static_cast<float>(ImGui::GetTime());
  fx.playing = state.app_state.transport() == AppState::Transport::kPlaying;

  // Neon background behind everything (radial glows + 40px grid overlay).
  const ImVec2 win_min = ImGui::GetWindowPos();
  const ImVec2 win_max(win_min.x + ImGui::GetWindowSize().x, win_min.y + ImGui::GetWindowSize().y);
  neon::background(ImGui::GetWindowDrawList(), win_min, win_max);

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(kBandGap, kBandGap));

  const ImVec2 avail = ImGui::GetContentRegionAvail();

  // Sequence Edit resizable height (item #10): re-clamp every frame in case
  // the window shrank since the last drag, so a stale fx.seqedit_height can
  // never starve the working row below kMinWorkH.
  const float max_seqedit_h =
      std::max(kSeqEditMinH, avail.y - kTransportH - 2.0F * kBandGap - kMinWorkH);
  fx.seqedit_height = std::clamp(fx.seqedit_height, kSeqEditMinH, max_seqedit_h);
  const float seqedit_h = fx.seqedit_collapsed ? kCollapsedStripH : fx.seqedit_height;

  const float work_h = std::max(kMinWorkH, avail.y - kTransportH - seqedit_h - 2.0F * kBandGap);

  // Band 1: transport rack.
  begin_zone("band_transport", ImVec2(0.0F, kTransportH), /*scrolls=*/false);
  render_transport_panel(state.app_state, state.brain_session, fx);
  ImGui::EndChild();

  // Band 2: working row -- browser | hero | rail. Item #9: a collapsed side
  // band shrinks to a thin strip and the hero grid reclaims the freed width.
  const float browser_w = fx.browser_collapsed ? kCollapsedStripW : kBrowserW;
  const float rail_w = fx.intention_collapsed ? kCollapsedStripW : kRailW;
  const float hero_w = std::max(200.0F, avail.x - browser_w - rail_w - 2.0F * kBandGap);

  begin_zone("band_browser", ImVec2(browser_w, work_h), /*scrolls=*/false);
  render_collapse_toggle(fx.browser_collapsed, "browser_collapse");
  if (!fx.browser_collapsed) {
    render_browser_panel(state.browser, state.brain_session, state.app_state, fx);
  }
  ImGui::EndChild();

  ImGui::SameLine(0.0F, kBandGap);
  begin_zone("band_hero", ImVec2(hero_w, work_h), /*scrolls=*/false);
  render_grid_panel(state.grid, state.seqedit, state.parts, state.brain_session, state.app_state,
                    fx);
  ImGui::EndChild();

  ImGui::SameLine(0.0F, kBandGap);
  begin_zone("band_rail", ImVec2(rail_w, work_h), /*scrolls=*/!fx.intention_collapsed);
  render_collapse_toggle(fx.intention_collapsed, "intention_collapse");
  if (!fx.intention_collapsed) {
    render_rail(state);
  }
  ImGui::EndChild();

  // Band 3: sequence edit. Item #10: a drag splitter atop the band adjusts
  // fx.seqedit_height directly; item #9: collapsing shrinks it to a thin
  // strip and the working row above reclaims the freed height.
  if (!fx.seqedit_collapsed) {
    render_seqedit_splitter(fx, kSeqEditMinH, max_seqedit_h);
  }
  begin_zone("band_seqedit", ImVec2(0.0F, seqedit_h), /*scrolls=*/false);
  render_collapse_toggle(fx.seqedit_collapsed, "seqedit_collapse");
  if (!fx.seqedit_collapsed) {
    render_seqedit_panel(state.seqedit, fx);
  }
  ImGui::EndChild();

  ImGui::PopStyleVar();
}

}  // namespace sonotron
