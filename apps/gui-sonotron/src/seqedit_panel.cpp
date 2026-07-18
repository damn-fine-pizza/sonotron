#include "seqedit_panel.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

#include "brain_session.hpp"
#include "imgui.h"
#include "neon_widgets.hpp"
#include "preview.hpp"
#include "step_pattern_model.hpp"
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

// Left-hand per-instrument SHOW/HIDE sidebar width (owner task #1): a FLOOR,
// not a fixed width -- render_role_toggle_sidebar widens past this whenever
// the widest role label + its checkbox box + inner spacing + both window
// paddings need more, so no label (e.g. "Chord2"/"Phrase") is ever clipped
// at the current font/DPI, while narrow-label runs keep this compact minimum.
// The floor is set a comfortable notch above the natural fit of today's
// widest default-font label (owner 2026-07-18: "enlarge the left section a
// bit") so the section reads visibly wider now, not only when a font-scaled
// label happens to demand it.
constexpr float kRoleListWidth = 118.0F;

// The role labels render a notch LARGER than the rest of the UI (owner,
// 2026-07-18: the default-size labels read as "microscopic" here) -- applied
// via SetWindowFontScale on the seq_role_list child ONLY, so the header and
// canvas keep the base font.
constexpr float kRoleFontScale = 1.15F;

// Piano-roll lane height in px (owner bug: "Sequence Edit still doesn't
// show all tracks" -- draw_role_pattern used to draw every visible role
// into the SAME shared rect on the SAME pitch axis, so all roles mutually
// occluded each other in one lane). Each visible role now gets its own
// horizontal strip this tall; the canvas grows past the visible band and
// scrolls (see "seq_canvas"'s dropped NoScrollbar/NoScrollWithMouse flags
// below) rather than being squeezed to fit every lane on screen at once.
constexpr float kLaneH = 76.0F;

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

// Lane-partition fix (owner bug: "Sequence Edit still doesn't show all
// tracks" -- overlaying every visible role into ONE shared rect made 9
// roles mutually occlude each other on the same pitch axis). Draws each
// `lane_roles[0..lane_count)` role into its OWN horizontal strip of the
// canvas (`kLaneH` tall, stacked top to bottom in role order), a thin
// divider between lanes, and a small role-label glyph -- extracted out of
// render_seqedit_panel (readability-function-cognitive-complexity), pure
// refactor of task #1's original single-rect overlay loop, same color/alpha
// rules: the opened role (model.part_index()) keeps its full track_color/
// 0.85F/glow treatment, every other role stays dimmed and unglowed.
void draw_piano_roll_lanes(ImDrawList* dl, const ImVec2& p0, const ImVec2& p1, int total_steps,
                           const neon::ClipPattern& pat, const ImVec4& track_color,
                           const UiState& fx, const SeqEditModel& model, preview::Section section,
                           const std::array<std::size_t, kTrackRoleCount>& lane_roles,
                           std::size_t lane_count) {
  for (std::size_t i = 0; i < lane_count; ++i) {
    const std::size_t role = lane_roles[i];
    const ImVec2 lane_p0(p0.x, p0.y + static_cast<float>(i) * kLaneH);
    const ImVec2 lane_p1(p1.x, lane_p0.y + kLaneH);
    if (role == model.part_index()) {
      draw_role_pattern(dl, lane_p0, lane_p1, total_steps, pat, track_color, 0.85F, fx.glow);
    } else {
      const preview::PreviewPattern role_pp = preview::preview_for(fx.active_style, section, role);
      const neon::ClipPattern role_pat =
          neon::clip_pattern_from_pitches(role_pp.pitch, role_pp.bars);
      draw_role_pattern(dl, lane_p0, lane_p1, total_steps, role_pat, theme::kRoleTint[role], 0.45F,
                        false);
    }
    if (i > 0) {
      dl->AddLine(lane_p0, ImVec2(lane_p1.x, lane_p0.y), neon::u32(theme::kTextMuted, 0.18F), 1.0F);
    }
    dl->AddText(ImVec2(lane_p0.x + 4.0F, lane_p0.y + 2.0F),
                neon::u32(theme::kRoleTint[role], role == model.part_index() ? 0.95F : 0.6F),
                std::string(kTrackRoleLabels[role]).c_str());
  }
}

// Task #11 Phase 1 (Sequence Edit step sequencer, roadmap node 11600/11610):
// the interactive canvas for SeqEditView::kStep, active only when the open
// cell is a real step-track cell (model.open_step_track() >= 0). The visual
// layer reuses draw_role_pattern above (preview_for_track's own
// PreviewPattern fed through the identical neon::clip_pattern_from_pitches
// conversion every other view already uses), so a step track's on/off
// content draws through the SAME routine as the read-only style overlay --
// one shared formula, never a second bespoke renderer. The interactive
// layer is a row of per-step InvisibleButtons laid out on the identical
// column math draw_role_pattern used internally, drawn on top purely for
// hit-testing (they paint nothing themselves). A click toggles the step's
// single note slot: writes the local StepPatternStore echo AND sends the
// matching `track step ...` wire command through the SeqEditModel-held
// BrainSession* (see seqedit_model.hpp's own header comment for why this
// model uniquely holds one) -- this is the one path the anti-no-op contract
// test (test_step_track_end_to_end_contract.cpp) exercises end to end. A
// null BrainSession* (never wired, e.g. a unit test constructing this model
// bare) still updates the local echo but sends nothing -- a defensive
// no-op, never a crash.
void render_step_grid(SeqEditModel& model, StepPatternModel& track, ImDrawList* dl,
                      const ImVec2& p0, const ImVec2& p1, const ImVec4& color) {
  constexpr std::uint8_t kDefaultNote = 60;
  constexpr std::uint8_t kDefaultVel = 100;
  constexpr std::uint16_t kDefaultGate = 120;  // half a step; mirrors the translator's own default

  const preview::PreviewPattern pp = preview::preview_for_track(track);
  const neon::ClipPattern pat = neon::clip_pattern_from_pitches(pp.pitch, pp.bars);
  const int total_steps =
      std::clamp(pat.bars, 1, neon::ClipPattern::kMaxBars) * neon::ClipPattern::kSteps;
  draw_role_pattern(dl, p0, p1, total_steps, pat, color, 0.85F, /*glow=*/false);

  const int track_idx = model.open_step_track();
  BrainSession* session = model.brain_session();
  const float col_w = (p1.x - p0.x) / static_cast<float>(total_steps);
  for (int i = 0; i < total_steps; ++i) {
    ImGui::PushID(i);
    ImGui::SetCursorScreenPos(ImVec2(p0.x + col_w * static_cast<float>(i), p0.y));
    const bool clicked = ImGui::InvisibleButton("step", ImVec2(col_w, p1.y - p0.y));
    if (clicked && track_idx >= 0) {
      const bool was_on = track.step(static_cast<std::size_t>(i)).vel > 0;
      if (was_on) {
        track.clear_step(static_cast<std::size_t>(i));
        if (session != nullptr) {
          session->send("track step " + std::to_string(track_idx) + " " + std::to_string(i + 1) +
                        " clear");
        }
      } else if (track.set_step(static_cast<std::size_t>(i), kDefaultNote, kDefaultVel,
                                kDefaultGate)) {
        if (session != nullptr) {
          session->send("track step " + std::to_string(track_idx) + " " + std::to_string(i + 1) +
                        " " + std::to_string(kDefaultNote) + " " + std::to_string(kDefaultVel) +
                        " " + std::to_string(kDefaultGate));
        }
      }
    }
    ImGui::PopID();
  }
}

// Right-aligned mode tabs, sized to fit BOTH labels fully (a SmallButton is
// text + 2*FramePadding.x wide; reserve exactly that for each so neither
// "piano-roll" nor "step" is clipped, at any DPI/font size) -- extracted out
// of render_seqedit_panel (readability-function-cognitive-complexity), pure
// refactor, no behavior change. Task #11 Phase 1: the "step" tab only makes
// sense for a real step-track cell -- style-section cells have no kStep
// content, so it is hidden entirely otherwise, and the view falls back to
// kPianoRoll if a PRIOR step-track cell had left kStep selected before a
// different (non-step-track) cell was opened.
void render_mode_tabs(SeqEditModel& model) {
  ImGui::SameLine();
  const float pad2 = ImGui::GetStyle().FramePadding.x * 2.0F;
  const float w_pr = ImGui::CalcTextSize("piano-roll").x + pad2;
  const float w_st = ImGui::CalcTextSize("step").x + pad2;
  const float tab_gap = 6.0F;
  const bool has_step_track = model.open_step_track() >= 0;
  const float tabs_w = has_step_track ? (w_pr + tab_gap + w_st) : w_pr;
  ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - tabs_w);
  if (mode_tab("piano-roll", model.view() == SeqEditView::kPianoRoll)) {
    model.set_view(SeqEditView::kPianoRoll);
  }
  if (has_step_track) {
    ImGui::SameLine(0.0F, tab_gap);
    if (mode_tab("step", model.view() == SeqEditView::kStep)) {
      model.set_view(SeqEditView::kStep);
    }
  } else if (model.view() == SeqEditView::kStep) {
    model.set_view(SeqEditView::kPianoRoll);
  }
}

// Task #11 Phase 1: SeqEditView::kStep, with a real step-track cell open,
// branches the canvas into the interactive step grid instead of the
// read-only overlay below -- extracted out of render_seqedit_panel
// (readability-function-cognitive-complexity), pure refactor, no behavior
// change. Returns true when it fully handled (and ended) the canvas child,
// in which case the caller must return immediately without falling through
// to the read-only overlay below. kPianoRoll (and a kStep view left selected
// on a NON-step-track cell, which the mode-tab guard above already falls
// back to kPianoRoll for) keeps today's overlay COMPLETELY UNCHANGED -- that
// stays Phase-2's future editable canvas, per the owner's "entrambi a fasi"
// decision.
bool try_render_step_canvas(SeqEditModel& model, ImDrawList* dl, const ImVec2& p0, const ImVec2& p1,
                            const ImVec2& avail, const UiState& fx, const ImVec4& track_color) {
  if (model.view() != SeqEditView::kStep || model.open_step_track() < 0) {
    return false;
  }
  StepPatternModel* track =
      model.step_tracks().track(static_cast<std::size_t>(model.open_step_track()));
  if (track == nullptr) {
    return false;
  }
  render_step_grid(model, *track, dl, p0, p1, track_color);
  if (fx.playing) {
    const float phase = std::fmod(fx.time, 2.0F) / 2.0F;
    const float x = p0.x + phase * avail.x;
    dl->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y), neon::u32(theme::kGreen), 1.5F);
  }
  ImGui::EndChild();
  return true;
}

// Left-hand per-instrument SHOW/HIDE toggle list (owner task #1): one
// Checkbox per role, bound to SeqEditModel::role_visible()/set_role_visible(),
// text-tinted with theme::kRoleTint and the currently-opened role emphasized
// via a brighter alpha. A SEPARATE sibling child window from "seq_canvas" --
// see render_seqedit_panel's own call site comment for why.
void render_role_toggle_sidebar(SeqEditModel& model) {
  // Size the sidebar to the WIDEST role label so none is clipped (owner,
  // 2026-07-18), mirroring the mode-tab CalcTextSize+FramePadding idiom used
  // in the header below. A Checkbox lays out as: a square box (one frame
  // height) + ItemInnerSpacing.x + the label text; the top bulk-action
  // SmallButton is text + 2*FramePadding.x. Text advances and the checkbox
  // square scale with the font, so both are measured at the base font and
  // multiplied by kRoleFontScale; ScrollbarSize is reserved so the vertical
  // scrollbar (rows overflow the fixed-height band and scroll -- owner's
  // accepted trade for full-size labels) never sits over a label. Never below
  // kRoleListWidth (the floor).
  const ImGuiStyle& style = ImGui::GetStyle();
  const float scaled_box = ImGui::GetFontSize() * kRoleFontScale + style.FramePadding.y * 2.0F;
  float widest_label = 0.0F;
  for (std::size_t role = 0; role < kTrackRoleCount; ++role) {
    widest_label =
        std::max(widest_label, ImGui::CalcTextSize(std::string(kTrackRoleLabels[role]).c_str()).x);
  }
  const float row_w = scaled_box + style.ItemInnerSpacing.x + widest_label * kRoleFontScale;
  const float btn_w =
      std::max(ImGui::CalcTextSize("all tracks").x, ImGui::CalcTextSize("last track").x) *
          kRoleFontScale +
      style.FramePadding.x * 2.0F;
  const float list_w =
      std::max(kRoleListWidth,
               std::max(row_w, btn_w) + style.WindowPadding.x * 2.0F + style.ScrollbarSize + 4.0F);
  // Scrollbar ENABLED (default flags): the nine rows + bulk button overflow
  // the fixed-height band (layout_renderer.cpp kSeqEditH), and the owner
  // prefers scrolling to fit-shrunk, unreadable labels.
  ImGui::BeginChild("seq_role_list", ImVec2(list_w, 0), ImGuiChildFlags_None,
                    ImGuiWindowFlags_None);
  ImGui::SetWindowFontScale(kRoleFontScale);
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
  ImGui::SetWindowFontScale(1.0F);
  ImGui::EndChild();
}

// Which visible roles get a piano-roll lane this frame -- extracted out of
// render_seqedit_panel (readability-function-cognitive-complexity), pure
// refactor. Mirrors try_render_step_canvas's own guard (model.view() !=
// kStep || open_step_track() < 0) so the two branches can never disagree
// about which one is about to run this frame: the WAV preview and the
// step-track canvas both keep their single full-height rect (an empty
// `lane_roles`/a 0 return degrades the caller's content height back to
// avail.y), only the read-only piano-roll overlay gets partitioned. Returns
// the lane count (0 when this frame isn't the piano-roll overlay at all).
std::size_t compute_piano_roll_lanes(bool open, const UiState& fx, const SeqEditModel& model,
                                     std::array<std::size_t, kTrackRoleCount>& lane_roles) {
  const bool is_piano_roll =
      open && !fx.open_wav && (model.view() != SeqEditView::kStep || model.open_step_track() < 0);
  if (!is_piano_roll) {
    return 0;
  }
  std::size_t lane_count = 0;
  for (std::size_t role = 0; role < kTrackRoleCount; ++role) {
    if (model.role_visible(role)) {
      lane_roles[lane_count++] = role;
    }
  }
  return lane_count;
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

  render_mode_tabs(model);
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

  // Canvas. Owner bug ("Sequence Edit still doesn't show all tracks"): the
  // piano-roll view below now partitions the canvas into one horizontal LANE
  // per visible role instead of overlaying every role into this single
  // child's rect, so the content can outgrow the visible band -- scrollbar
  // and mouse-wheel scroll are both left ENABLED (dropped from the old
  // NoScrollbar | NoScrollWithMouse) so every lane stays reachable. The WAV
  // and step-track branches below still draw a single full-height rect (no
  // overflow, so no scrolling occurs for them either) -- unchanged.
  ImGui::BeginChild("seq_canvas", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_None);
  const ImVec2 p0 = ImGui::GetCursorScreenPos();
  const ImVec2 avail = ImGui::GetContentRegionAvail();
  ImDrawList* dl = ImGui::GetWindowDrawList();

  // Which visible roles get a piano-roll lane this frame, and how tall the
  // scrollable content is -- see compute_piano_roll_lanes's own header
  // comment.
  std::array<std::size_t, kTrackRoleCount> lane_roles{};
  const std::size_t lane_count = compute_piano_roll_lanes(open, fx, model, lane_roles);
  const float content_h =
      lane_count > 0 ? std::max(avail.y, static_cast<float>(lane_count) * kLaneH) : avail.y;
  const ImVec2 p1(p0.x + avail.x, p0.y + content_h);

  dl->AddRectFilled(p0, p1, neon::u32(theme::kInsetBg), 6.0F);

  // Vertical bar guides (owner tasks #2/#3: `bars` groups of 8 divisions
  // each, landing exactly on a bar boundary at a stronger alpha -- mirrors
  // the bar-divider legibility touch neon::clip_preview_pianoroll already
  // carries). Spans the full scrollable content height so the guides read
  // through every lane, not just the first screenful.
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
  // deterministic waveform. p1 == avail-height here (lane_count is always 0
  // for this branch, is_piano_roll_lanes above already excludes fx.open_wav).
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

  if (try_render_step_canvas(model, dl, p0, p1, avail, fx, track_color)) {
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
  // (pitch 0 at the bottom of its OWN lane), via the SAME neon::
  // pitch_grid_cell() helper the launch-cell mini-preview uses (neon_
  // widgets.cpp's clip_preview_pianoroll) -- one shared formula, so the two
  // views can never silently drift apart. Every simultaneous voice at a step
  // (owner bug #13, e.g. a drum kit's kick+hihat both on beat 1) draws its
  // OWN row block here too.
  //
  // Owner task #1 + lane-partition fix ("Sequence Edit still doesn't show
  // all tracks"): every VISIBLE role gets its OWN horizontal lane
  // (`lane_roles`/`kLaneH` above) instead of being overlaid into one shared
  // rect, where 9 roles used to mutually occlude each other on the same
  // pitch axis -- see draw_piano_roll_lanes's own header comment.
  draw_piano_roll_lanes(dl, p0, p1, total_steps, pat, track_color, fx, model, section, lane_roles,
                        lane_count);

  // Green playhead sweeping L->R while the opened clip plays, across the
  // full scrollable content height so it reads through every lane.
  if (fx.playing) {
    const float phase = std::fmod(fx.time, 2.0F) / 2.0F;
    const float x = p0.x + phase * avail.x;
    dl->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y), neon::u32(theme::kGreen), 1.5F);
  }

  // Registers the lanes' full content height with ImGui so the now-enabled
  // scrollbar/mouse-wheel scroll (this child's BeginChild flags above) can
  // actually reach every lane -- this is a draw-list-only child (no normal
  // widgets advance the cursor), so ImGui would otherwise never learn the
  // content extends past `avail.y`. A zero-size Dummy() (an actual submitted
  // item, not just a cursor move) is required here: ImGui's own
  // ErrorCheckUsingSetCursorPosToExtendParentBoundaries asserts if
  // SetCursorScreenPos alone is used to grow the parent's content bounds.
  if (lane_count > 0) {
    ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + content_h));
    ImGui::Dummy(ImVec2(0.0F, 0.0F));
  }

  ImGui::EndChild();
}

}  // namespace sonotron
