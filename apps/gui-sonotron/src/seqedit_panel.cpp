#include "seqedit_panel.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

#include "imgui.h"
#include "neon_widgets.hpp"
#include "preview.hpp"
#include "theme.hpp"
#include "track_roles.hpp"

namespace sonotron {

namespace {

// A small mode tab: cyan fill when active.
bool mode_tab(const char* label, bool active) {
  ImGui::PushStyleColor(
      ImGuiCol_Button,
      active ? ImVec4(theme::kCyan.x, theme::kCyan.y, theme::kCyan.z, 0.22F) : theme::kFrameBg);
  ImGui::PushStyleColor(ImGuiCol_Text, active ? theme::kCyan : theme::kTextSecondary);
  const bool clicked = ImGui::SmallButton(label);
  ImGui::PopStyleColor(2);
  return clicked;
}

// Left-hand per-instrument SHOW/HIDE sidebar width (owner task #1), a fixed
// width matching this file's own compact-toolbar spacing scale.
constexpr float kRoleListWidth = 92.0F;

// Draws one role's note pattern into the seq_canvas draw list: every voice
// at every step as a filled, rounded rect via the shared pitch_grid_cell()
// layout helper, optionally glowed. Shared by the opened role's own draw
// (glow honored, per `fx.glow`) and every other overlaid role's draw (glow
// always off, reduced alpha) so the per-step/per-voice loop is written
// exactly once instead of twice.
void draw_role_pattern(ImDrawList* dl, const ImVec2& p0, const ImVec2& p1, int total_steps,
                       const neon::ClipPattern& pat, const ImVec4& color, float alpha, bool glow) {
  const int kPitches = neon::ClipPattern::kPitches;
  const int kVoices = neon::ClipPattern::kMaxVoicesPerStep;
  for (int step = 0; step < total_steps; ++step) {
    for (int voice = 0; voice < kVoices; ++voice) {
      const int pitch = pat.pitch[static_cast<std::size_t>(step)][static_cast<std::size_t>(voice)];
      if (pitch < 0) {
        continue;
      }
      const neon::PitchCellRect r =
          neon::pitch_grid_cell(p0, p1, step, total_steps, pitch, kPitches);
      const ImVec2 b0(r.min.x + 2.0F, r.min.y + 2.0F);
      const ImVec2 b1(r.max.x - 2.0F, r.max.y - 2.0F);
      if (glow) {
        neon::glow_rect(dl, b0, b1, color, 3.0F, 0.7F, glow);
      }
      dl->AddRectFilled(b0, b1, neon::u32(color, alpha), 3.0F);
    }
  }
}

// Left-hand per-instrument SHOW/HIDE toggle list (owner task #1): one
// Checkbox per role, bound to SeqEditModel::role_visible()/set_role_visible(),
// text-tinted with theme::kRoleTint and the currently-opened role emphasized
// via a brighter alpha. A SEPARATE sibling child window from "seq_canvas" --
// see render_seqedit_panel's own call site comment for why.
void render_role_toggle_sidebar(SeqEditModel& model) {
  ImGui::BeginChild("seq_role_list", ImVec2(kRoleListWidth, 0), ImGuiChildFlags_None,
                    ImGuiWindowFlags_None);
  // Feature B item 4: a one-shot BULK action, not a persistent mode -- flips
  // every role's visibility to all-on, or solos the currently-opened role
  // (SeqEditModel::set_all_tracks_visible). The label reflects the state the
  // model is offering right now (all_tracks_shown()); pressing it flips to
  // the OTHER state.
  if (ImGui::SmallButton(model.all_tracks_shown() ? "all tracks" : "last track")) {
    model.set_all_tracks_visible(!model.all_tracks_shown());
  }
  ImGui::Spacing();
  for (std::size_t role = 0; role < kTrackRoleCount; ++role) {
    const bool emphasized = role == model.part_index();
    ImVec4 tint = theme::kRoleTint[role];
    tint.w = emphasized ? 1.0F : 0.55F;
    bool visible = model.role_visible(role);
    ImGui::PushStyleColor(ImGuiCol_Text, tint);
    if (ImGui::Checkbox(std::string(kTrackRoleLabels[role]).c_str(), &visible)) {
      model.set_role_visible(role, visible);
    }
    ImGui::PopStyleColor();
  }
  ImGui::EndChild();
}

}  // namespace

void render_seqedit_panel(SeqEditModel& model, const UiState& fx) {
  const bool open = fx.open_cell >= 0;
  const ImVec4 track_color =
      (fx.open_row >= 0 && fx.open_row < static_cast<int>(theme::kTrackColor.size()))
          ? theme::kTrackColor[fx.open_row]
          : theme::kCyan;

  // Real content (repeat-zone-real-contract.md "cell preview made real"):
  // the SAME preview_for(...) call grid_panel.cpp's mini-thumbnail uses,
  // keyed by the SAME (style, section, role) the opened cell represents --
  // model.part_index() is the role this clip was opened from, and (SLICE 4a)
  // fx.open_section is that cell's own COLUMN's SectionType byte (set by
  // grid_panel.cpp at every cell-open site, GridModel::scene_section), so
  // the two views match by construction. Only computed while a cell is
  // actually open.
  const preview::Section section = static_cast<preview::Section>(fx.open_section);
  const preview::PreviewPattern pp =
      open ? preview::preview_for(fx.active_style, section, model.part_index())
           : preview::PreviewPattern{};
  // Owner tasks #2/#3 (multi-bar widening): `bars` is a property of the
  // SECTION, not the role -- every one of the 9 roles' PreviewPattern for
  // this same (style, section) carries the IDENTICAL bars value, so it is
  // read once here off the opened role's own pattern and reused for every
  // overlay below instead of being recomputed per role.
  const neon::ClipPattern pat = neon::clip_pattern_from_pitches(pp.pitch, pp.bars);
  const int bars = std::clamp(pat.bars, 1, neon::ClipPattern::kMaxBars);
  const int total_steps = bars * neon::ClipPattern::kSteps;

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
  if (open && pp.approx) {
    // Honesty affordance (repeat-zone-real-contract.md STEP 4): a small,
    // muted marker, no new persistent chrome, for a preview that isn't the
    // exact runtime output (resolved against a placeholder harmony, and/or
    // a motif's repeat=0 statement skeleton only).
    ImGui::SameLine(0.0F, 6.0F);
    ImGui::TextColored(theme::kTextMuted, "~approx");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("approx preview (placeholder harmony, no live chord)");
    }
  }
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

  // Left-hand per-instrument SHOW/HIDE toggle list (owner task #1). A
  // SEPARATE sibling child window, rendered BEFORE "seq_canvas" with
  // ImGui::SameLine() so "seq_canvas" naturally occupies whatever content
  // region remains -- no manual offset math of the canvas rect itself, so
  // every existing p0/p1/avail computation inside "seq_canvas" stays correct
  // by construction. Only meaningful while a cell is open (nothing to
  // show/hide otherwise), matching the canvas's own early-return gating.
  if (open) {
    render_role_toggle_sidebar(model);
    ImGui::SameLine();
  }

  // Canvas.
  ImGui::BeginChild("seq_canvas", ImVec2(0, 0), ImGuiChildFlags_None,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  const ImVec2 p0 = ImGui::GetCursorScreenPos();
  const ImVec2 avail = ImGui::GetContentRegionAvail();
  const ImVec2 p1(p0.x + avail.x, p0.y + avail.y);
  ImDrawList* dl = ImGui::GetWindowDrawList();

  dl->AddRectFilled(p0, p1, neon::u32(theme::kInsetBg), 6.0F);

  // Vertical bar guides (owner tasks #2/#3: `bars` groups of 8 divisions
  // each, landing exactly on a bar boundary at a stronger alpha -- mirrors
  // the bar-divider legibility touch neon::clip_preview_pianoroll already
  // carries).
  const int total_divisions = bars * 8;
  for (int i = 1; i < total_divisions; ++i) {
    const float x = p0.x + avail.x * static_cast<float>(i) / static_cast<float>(total_divisions);
    const bool on_bar = (i % 8) == 0;
    dl->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y), neon::u32(theme::kCyan, on_bar ? 0.14F : 0.06F),
                1.0F);
  }

  if (!open) {
    const char* hint = "click a clip in the Repeat Zone to open it here";
    const ImVec2 ts = ImGui::CalcTextSize(hint);
    dl->AddText(ImVec2(p0.x + (avail.x - ts.x) * 0.5F, p0.y + (avail.y - ts.y) * 0.5F),
                neon::u32(theme::kTextMuted), hint);
    ImGui::EndChild();
    return;
  }

  // Feature B item 7 (WAV branch): a captured LoopBuffer clip has no
  // piano-roll content to overlay -- show the waveform preview instead.
  // Dormant in production today (nothing creates a kLoopBuffer cell yet,
  // Phase 7 Looper), wired now so the branch is ready the moment one
  // exists. Seeded off fx.open_cell so the same clip always draws the SAME
  // deterministic waveform.
  if (fx.open_wav) {
    neon::clip_preview_waveform(dl, p0, p1, static_cast<std::uint32_t>(fx.open_cell), track_color);
    if (fx.playing) {
      const float phase = std::fmod(fx.time, 2.0F) / 2.0F;
      const float x = p0.x + phase * avail.x;
      dl->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y), neon::u32(theme::kGreen), 1.5F);
    }
    ImGui::EndChild();
    return;
  }

  // Real content (repeat-zone-real-contract.md STEP 3): every row renders
  // its real resolved note pattern, never a label-hash. The pad row
  // (grid_panel.cpp's kRows[...].audio, `fx.open_audio`) has no real audio
  // content yet -- it IS a real MIDI role, so it renders its real note
  // pattern here too rather than a fake waveform; clip_preview_waveform
  // stays reserved for genuine future audio content (an owner-flagged
  // deviation from the design's audio->waveform mapping, see this
  // workstream's implementation report). STEP left->right, PITCH low->high
  // (pitch 0 at the bottom), via the SAME neon::pitch_grid_cell() helper the
  // launch-cell mini-preview uses (neon_widgets.cpp's clip_preview_pianoroll)
  // -- one shared formula, so the two views can never silently drift apart.
  // Every simultaneous voice at a step (owner bug #13, e.g. a drum kit's
  // kick+hihat both on beat 1) draws its OWN row block here too -- this is
  // the reference view the launch-cell mini-preview must match.
  //
  // Owner task #1: overlay ALL 9 scene-column track-role instruments of the
  // currently-opened section, color-coded per role via theme::kRoleTint,
  // with the CURRENTLY-OPENED role emphasized. Every OTHER visible role
  // draws first, at reduced alpha and no glow; the opened role
  // (model.part_index()) draws LAST, unchanged from before (same
  // `track_color`/0.85F/glow logic), so it visually sits on top.
  for (std::size_t role = 0; role < kTrackRoleCount; ++role) {
    if (role == model.part_index() || !model.role_visible(role)) {
      continue;
    }
    const preview::PreviewPattern role_pp = preview::preview_for(fx.active_style, section, role);
    const neon::ClipPattern role_pat = neon::clip_pattern_from_pitches(role_pp.pitch, role_pp.bars);
    draw_role_pattern(dl, p0, p1, total_steps, role_pat, theme::kRoleTint[role], 0.45F, false);
  }

  // The opened role draws LAST, unchanged from before (same track_color,
  // same 0.85F alpha, same fx.glow gating), so it visually sits on top of
  // every other overlaid role -- but it, too, honors its own sidebar toggle
  // (owner task #1: the SHOW/HIDE list covers ALL 9 instruments, including
  // whichever one is currently open, or the affordance would silently do
  // nothing for the one row a user is most likely to click).
  if (model.role_visible(model.part_index())) {
    draw_role_pattern(dl, p0, p1, total_steps, pat, track_color, 0.85F, fx.glow);
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
