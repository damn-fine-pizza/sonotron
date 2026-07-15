#include "transport_panel.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "imgui.h"
#include "neon_widgets.hpp"
#include "theme.hpp"

namespace sonotron {

namespace {

// A dark rounded inset "cluster" box (the v02 tempo / bar:beat readout
// housings, spec §1) as an inline child so several sit side by side on the
// rack's single row. Caller fills it with content, then calls end_inset().
void begin_inset(const char* id, float width, float height) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::kInsetBg);
  ImGui::PushStyleColor(ImGuiCol_Border, theme::kBorderCyan);
  ImGui::BeginChild(id, ImVec2(width, height), ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::SetCursorPos(ImVec2(8.0F, (height - ImGui::GetTextLineHeight()) * 0.5F));
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

  // Tempo / meter / key / transpose inset. BPM + transpose are real sends on
  // nudge; the DISPLAYED bpm/transpose are local intent (no readback), and
  // 4/4 + key are local-only display (BrainSnapshot fields are unwired).
  static int bpm = 120;
  static int transpose_semitones = 0;
  ImGui::SameLine(0.0F, 14.0F);
  begin_inset("tempo_inset", 300.0F, row_h);
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
  // stopped / never positioned.
  ImGui::SameLine(0.0F, 10.0F);
  begin_inset("beat_inset", 118.0F, row_h);
  if (app_state.bar() == 0) {
    ImGui::TextColored(theme::kTextMuted, "\xE2\x80\x94 : \xE2\x80\x94 : \xC2\xB7\xC2\xB7");
  } else {
    theme::text_bold_colored(theme::kPink, "%03d", app_state.bar());
    ImGui::SameLine(0.0F, 3.0F);
    ImGui::TextColored(theme::kTextDim, ":");
    ImGui::SameLine(0.0F, 3.0F);
    theme::text_bold_colored(theme::kText, "%d", app_state.beat_num());
    ImGui::SameLine(0.0F, 3.0F);
    ImGui::TextColored(theme::kTextDim, ":");
    ImGui::SameLine(0.0F, 3.0F);
    ImGui::TextColored(theme::kTextMuted, "%02d", app_state.pulse());
  }
  end_inset();

  // Right-aligned: status dot + label, then the glow ⚙ toggle.
  const float right_w = 150.0F;
  ImGui::SameLine();
  ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),
                                ImGui::GetContentRegionMax().x - right_w));

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
