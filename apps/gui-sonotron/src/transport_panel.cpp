#include "transport_panel.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "imgui.h"
#include "neon_widgets.hpp"
#include "theme.hpp"

namespace sonotron {

namespace {

// A dark rounded inset "cluster" box (the v02 tempo / bar:beat readout
// housings, spec §1) as an inline child so several sit side by side on the
// rack's single row. Caller fills it with content, then calls end_inset().
// `content_width`, when >= 0, horizontally centers the upcoming content
// (its rendered width, as measured by the caller via ImGui::CalcTextSize)
// inside the inset instead of the default fixed 8px left inset padding.
void begin_inset(const char* id, float width, float height, float content_width = -1.0F) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::kInsetBg);
  ImGui::PushStyleColor(ImGuiCol_Border, theme::kBorderCyan);
  ImGui::BeginChild(id, ImVec2(width, height), ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  const float x = content_width >= 0.0F ? std::max(0.0F, (width - content_width) * 0.5F) : 8.0F;
  ImGui::SetCursorPos(ImVec2(x, (height - ImGui::GetTextLineHeight()) * 0.5F));
}

void end_inset() {
  ImGui::EndChild();
  ImGui::PopStyleColor(2);
}

// A thin vertical divider glyph between cluster fields.
void divider() {
  ImGui::SameLine(0.0F, 8.0F);
  ImGui::TextColored(theme::kTextDim, "|");
  ImGui::SameLine(0.0F, 8.0F);
}

// ENDING pad (roadmap task #37, docs/proposals/song-form-option-a-wiring-
// plan.md §3/§4): a dedicated cue-then-stop control, matching the Yamaha/Korg
// convention of a SEPARATE Ending button next to Stop (not a second Stop
// press). While playing, clicking it sends the bare, bar-quantized `style
// section ending1` verb ALONE -- never paired with `launch scene`, which
// would force an immediate (non-quantized) switch and defeat the "next
// measure" convention this button exists to match. The core's own one-shot
// rule (Arranger::on_tick's section_is_ending branch) then stops the
// transport once Ending1's own authored bars run out -- no new verb, no ABI
// change. `fx.ending_cued` is set here so grid_panel.cpp's update_auto_song
// can suppress its own advance until the transport is actually observed
// stopped (see V02State::ending_cued's own comment). While stopped, the cue
// is meaningless (nothing playing to end) -- the button is a no-op and
// visually dimmed (muted accent instead of amber) rather than sending
// anything. "END" is a plain ASCII label, not a glyph, since the vendored
// mono font has no dedicated ending/flag glyph. Kept as its own small
// function (mirroring begin_inset/end_inset/divider above) so the button's
// own gating logic does not add to render_transport_panel's own cognitive
// complexity.
void render_ending_pad(BrainSession& brain_session, V02State& fx, bool playing) {
  ImGui::SameLine(0.0F, 6.0F);
  const ImVec2 ending_pad(46.0F, 34.0F);
  const ImVec4& ending_accent = playing ? theme::kAmber : theme::kTextMuted;
  if (neon::pad_button("ending", "END", ending_pad, ending_accent, /*filled=*/false, fx.glow) &&
      playing) {
    brain_session.send("style section ending1");
    fx.ending_cued = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(playing ? "Ending -- cue Ending 1 at the next bar, then stop"
                              : "Ending -- only meaningful while playing");
  }
}

}  // namespace

void render_transport_panel(AppState& app_state, BrainSession& brain_session, V02State& fx) {
  const bool playing = app_state.transport() == AppState::Transport::kPlaying;
  const float row_h = 40.0F;
  ImGui::SetCursorPosY(std::max(6.0F, (ImGui::GetContentRegionAvail().y - row_h) * 0.5F));

  // Wordmark: cyan "sonotron" (glow) + a pink cursor underscore.
  ImGui::AlignTextToFramePadding();
  theme::text_bold_colored(theme::kCyan, "sonotron");
  ImGui::SameLine(0.0F, 0.0F);
  ImGui::TextColored(theme::kPink, "_");

  // Play / Stop / Panic neon pad buttons (38x34), real transport verbs.
  ImGui::SameLine(0.0F, 14.0F);
  const ImVec2 pad(38.0F, 34.0F);
  if (neon::pad_button("play", "\xE2\x96\xB6", pad, theme::kCyan, /*filled=*/true, fx.glow)) {
    brain_session.send("transport start");
    app_state.note_transport_sent(true);
  }
  ImGui::SameLine(0.0F, 6.0F);
  if (neon::pad_button("stop", "\xE2\x96\xA0", pad, theme::kTextSecondary, /*filled=*/false,
                       fx.glow)) {
    brain_session.send("transport stop");
    app_state.note_transport_sent(false);
  }
  ImGui::SameLine(0.0F, 6.0F);
  if (neon::pad_button("panic", "\xE2\x97\x89", pad, theme::kPink, /*filled=*/false, fx.glow)) {
    brain_session.send("panic");
  }
  render_ending_pad(brain_session, fx, playing);

  // Tempo / meter / key / transpose inset. BPM + transpose are real sends on
  // nudge; the DISPLAYED bpm/transpose are local intent (no readback), and
  // 4/4 + key are local-only display (BrainSnapshot fields are unwired).
  static int bpm = 120;
  static int transpose_semitones = 0;
  ImGui::SameLine(0.0F, 14.0F);
  begin_inset("tempo_inset", 348.0F, row_h);
  theme::text_bold_colored(theme::kText, "%d", bpm);
  ImGui::SameLine(0.0F, 3.0F);
  ImGui::TextColored(theme::kTextMuted, "BPM");
  const bool bpm_hovered = ImGui::IsItemHovered();
  divider();
  ImGui::TextColored(theme::kTextSecondary, "4/4");
  divider();
  ImGui::TextColored(theme::kTextMuted, "key");
  ImGui::SameLine(0.0F, 4.0F);
  ImGui::TextColored(theme::kGreen, "Cm");
  divider();
  ImGui::TextColored(theme::kTextMuted, "transp");
  ImGui::SameLine(0.0F, 4.0F);
  theme::text_bold_colored(theme::kText, "%+d", transpose_semitones);
  const bool transp_hovered = ImGui::IsItemHovered();
  end_inset();

  // Wheel / arrow nudge over the BPM field -> `bpm <n>` (20..400).
  if (bpm_hovered) {
    ImGui::SetTooltip("Tempo -- scroll or Up/Down to nudge BPM");
    int d = 0;
    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel > 0.0F || ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) {
      d += 1;
    }
    if (wheel < 0.0F || ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) {
      d -= 1;
    }
    if (d != 0) {
      bpm = std::clamp(bpm + d, 20, 400);
      brain_session.send("bpm " + std::to_string(bpm));
    }
  }
  // Wheel / arrow nudge over the transpose field -> `transpose <n>` (-12..12).
  if (transp_hovered) {
    ImGui::SetTooltip("Transpose -- scroll or Up/Down to nudge semitones");
    int d = 0;
    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel > 0.0F || ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) {
      d += 1;
    }
    if (wheel < 0.0F || ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) {
      d -= 1;
    }
    if (d != 0) {
      transpose_semitones = std::clamp(transpose_semitones + d, -12, 12);
      brain_session.send("transpose " + std::to_string(transpose_semitones));
    }
  }

  // bar:beat:pulse readout (REAL, from the kBeat heartbeat). "— : — : ··" when
  // stopped / never positioned. Horizontally centered in the inset: the
  // content width is measured up front (same glyphs about to be drawn) so
  // the SetCursorPos offset in begin_inset() lands the whole string in the
  // middle of the pill instead of flush against the 8px left padding.
  ImGui::SameLine(0.0F, 10.0F);
  if (app_state.bar() == 0) {
    static constexpr const char* kStoppedReadout = "\xE2\x80\x94 : \xE2\x80\x94 : \xC2\xB7\xC2\xB7";
    begin_inset("beat_inset", 150.0F, row_h, ImGui::CalcTextSize(kStoppedReadout).x);
    ImGui::TextColored(theme::kTextMuted, "%s", kStoppedReadout);
  } else {
    char bar_buf[8];
    char beat_buf[8];
    char pulse_buf[8];
    std::snprintf(bar_buf, sizeof(bar_buf), "%03d", app_state.bar());
    std::snprintf(beat_buf, sizeof(beat_buf), "%d", app_state.beat_num());
    std::snprintf(pulse_buf, sizeof(pulse_buf), "%02d", app_state.pulse());
    const float sep_w = ImGui::CalcTextSize(":").x;
    const float content_width = ImGui::CalcTextSize(bar_buf).x + 3.0F + sep_w + 3.0F +
                                ImGui::CalcTextSize(beat_buf).x + 3.0F + sep_w + 3.0F +
                                ImGui::CalcTextSize(pulse_buf).x;
    begin_inset("beat_inset", 150.0F, row_h, content_width);
    theme::text_bold_colored(theme::kPink, "%s", bar_buf);
    ImGui::SameLine(0.0F, 3.0F);
    ImGui::TextColored(theme::kTextDim, ":");
    ImGui::SameLine(0.0F, 3.0F);
    theme::text_bold_colored(theme::kText, "%s", beat_buf);
    ImGui::SameLine(0.0F, 3.0F);
    ImGui::TextColored(theme::kTextDim, ":");
    ImGui::SameLine(0.0F, 3.0F);
    ImGui::TextColored(theme::kTextMuted, "%s", pulse_buf);
  }
  end_inset();

  // Right-aligned: status dot + label, then the glow ⚙ toggle.
  const float right_w = 150.0F;
  ImGui::SameLine();
  ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetContentRegionMax().x - right_w));

  // Blinking status dot (blink gated on playing + glow), colored by transport.
  const ImVec4 dot_col = playing ? theme::kGreen : theme::kTextMuted;
  float dot_alpha = 1.0F;
  if (playing && fx.glow) {
    dot_alpha = 0.55F + 0.45F * std::fabs(std::sin(fx.time * 4.0F));
  }
  const float lh = ImGui::GetTextLineHeight();
  const ImVec2 c = ImGui::GetCursorScreenPos();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 center(c.x + 5.0F, c.y + lh * 0.5F);
  neon::glow_circle(dl, center, 4.0F, dot_col, dot_alpha, playing && fx.glow);
  dl->AddCircleFilled(center, 4.0F, neon::u32(dot_col, dot_alpha), 16);
  ImGui::Dummy(ImVec2(14.0F, lh));
  ImGui::SameLine(0.0F, 2.0F);
  ImGui::TextColored(dot_col, "%s", playing ? "playing" : "stopped");

  // Glow toggle: the ⚙ glyph is absent from the vendored mono font, so use a
  // geometric filled/hollow ring (present in the font, like ▶/■/◉) as the
  // options control -- lit ◉ = glow on, hollow ○ = glow off.
  ImGui::SameLine(0.0F, 10.0F);
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(theme::kCyan.x, theme::kCyan.y, theme::kCyan.z, 0.18F));
  ImGui::PushStyleColor(ImGuiCol_Text, fx.glow ? theme::kCyan : theme::kTextMuted);
  if (ImGui::Button(fx.glow ? "\xE2\x97\x89" : "\xE2\x97\x8B")) {  // ◉ / ○
    fx.glow = !fx.glow;
  }
  ImGui::PopStyleColor(3);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Toggle glow (%s)", fx.glow ? "on" : "off");
  }
}

}  // namespace sonotron
