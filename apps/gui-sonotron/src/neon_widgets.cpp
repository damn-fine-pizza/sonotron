#include "neon_widgets.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "theme.hpp"

namespace sonotron::neon {

namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kDegToRad = kPi / 180.0F;

// A cheap deterministic 0..1 from a 32-bit state -- the procedural preview
// pseudo-random (a single xorshift step). Not for anything but visual seeding.
float rand01(std::uint32_t& state) {
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return static_cast<float>(state & 0xFFFFFFu) / static_cast<float>(0xFFFFFF);
}

}  // namespace

ImU32 u32(const ImVec4& color, float alpha_mul) {
  ImVec4 c = color;
  c.w = std::clamp(c.w * alpha_mul, 0.0F, 1.0F);
  return ImGui::ColorConvertFloat4ToU32(c);
}

ClipPattern clip_pattern_from_pitches(const std::array<int, ClipPattern::kSteps>& pitches) {
  ClipPattern out{};
  int lo = 128;
  int hi = -1;
  for (const int p : pitches) {
    if (p < 0) {
      continue;
    }
    lo = std::min(lo, p);
    hi = std::max(hi, p);
  }
  if (hi < lo) {
    // Every step is a rest: nothing to normalize (ClipPattern's own default
    // is all-zero, not all-rest, so this must be set explicitly).
    for (int& row : out.pitch) {
      row = -1;
    }
    return out;
  }
  constexpr int kMaxRow = ClipPattern::kPitches - 1;
  for (int step = 0; step < ClipPattern::kSteps; ++step) {
    const int p = pitches[static_cast<std::size_t>(step)];
    if (p < 0) {
      out.pitch[static_cast<std::size_t>(step)] = -1;
      continue;
    }
    if (hi == lo) {
      // A single distinct pitch (or a run of the same one): the middle row.
      out.pitch[static_cast<std::size_t>(step)] = kMaxRow / 2;
      continue;
    }
    const float t = static_cast<float>(p - lo) / static_cast<float>(hi - lo);
    out.pitch[static_cast<std::size_t>(step)] =
        std::clamp(static_cast<int>(t * static_cast<float>(kMaxRow) + 0.5F), 0, kMaxRow);
  }
  return out;
}

void glow_rect(ImDrawList* dl, const ImVec2& min, const ImVec2& max, const ImVec4& color,
               float rounding, float intensity, bool glow) {
  if (!glow) {
    return;
  }
  constexpr int kLayers = 4;
  for (int i = 0; i < kLayers; ++i) {
    const float expand = static_cast<float>(i + 1) * 2.5F;
    const float alpha = intensity * (1.0F - static_cast<float>(i) / kLayers) * 0.16F;
    dl->AddRect(ImVec2(min.x - expand, min.y - expand), ImVec2(max.x + expand, max.y + expand),
                u32(color, alpha), rounding + expand, 0, 2.0F);
  }
}

void glow_circle(ImDrawList* dl, const ImVec2& center, float radius, const ImVec4& color,
                 float intensity, bool glow) {
  if (!glow) {
    return;
  }
  constexpr int kLayers = 4;
  for (int i = 0; i < kLayers; ++i) {
    const float expand = static_cast<float>(i + 1) * 2.0F;
    const float alpha = intensity * (1.0F - static_cast<float>(i) / kLayers) * 0.18F;
    dl->AddCircle(center, radius + expand, u32(color, alpha), 0, 2.0F);
  }
}

void background(ImDrawList* dl, const ImVec2& min, const ImVec2& max) {
  const float w = max.x - min.x;
  const float h = max.y - min.y;

  // Base near-black fill (the host window bg is already this color; this keeps
  // the background self-contained if it is ever drawn over a lighter surface).
  dl->AddRectFilled(min, max, u32(theme::kAppBg));

  // Two faint radial glows, approximated as stacks of large translucent
  // circles: top-right cyan-ish blue, bottom-left teal (spec §"Global frame").
  const ImVec2 tr(min.x + w * 0.82F, min.y + h * 0.12F);
  const ImVec2 bl(min.x + w * 0.14F, min.y + h * 0.9F);
  const float glow_r = std::max(w, h) * 0.55F;
  constexpr int kRings = 6;
  for (int i = 0; i < kRings; ++i) {
    const float t = 1.0F - static_cast<float>(i) / kRings;  // brightest at the core
    const float r = glow_r * (static_cast<float>(i + 1) / kRings);
    dl->AddCircleFilled(tr, r, u32(theme::kBlue, 0.020F * t), 40);
    dl->AddCircleFilled(bl, r, u32(theme::kCyan, 0.018F * t), 40);
  }

  // 40px grid overlay (thin cyan lines, very low opacity).
  const ImU32 grid = u32(theme::kCyan, 0.05F);
  for (float x = min.x; x <= max.x; x += 40.0F) {
    dl->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), grid, 1.0F);
  }
  for (float y = min.y; y <= max.y; y += 40.0F) {
    dl->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), grid, 1.0F);
  }
}

bool pad_button(const char* id, const char* glyph, const ImVec2& size, const ImVec4& accent,
                bool filled, bool glow) {
  const ImVec2 p0 = ImGui::GetCursorScreenPos();
  const bool clicked = ImGui::InvisibleButton(id, size);
  const bool hovered = ImGui::IsItemHovered();
  const ImVec2 p1(p0.x + size.x, p0.y + size.y);
  ImDrawList* dl = ImGui::GetWindowDrawList();

  const float rounding = 9.0F;
  ImVec4 fill = filled ? accent : theme::kStopDark;
  if (hovered) {
    fill.x = std::min(1.0F, fill.x * 1.25F + 0.05F);
    fill.y = std::min(1.0F, fill.y * 1.25F + 0.05F);
    fill.z = std::min(1.0F, fill.z * 1.25F + 0.05F);
  }
  if (glow && (filled || hovered)) {
    glow_rect(dl, p0, p1, accent, rounding, hovered ? 1.2F : 0.8F, glow);
  }
  dl->AddRectFilled(p0, p1, u32(fill, filled ? 0.9F : 1.0F), rounding);
  dl->AddRect(p0, p1, u32(accent, filled ? 0.9F : 0.6F), rounding, 0, 1.5F);

  const ImVec4 glyph_col = filled ? theme::kAppBg : accent;
  const ImVec2 ts = ImGui::CalcTextSize(glyph);
  dl->AddText(ImVec2(p0.x + (size.x - ts.x) * 0.5F, p0.y + (size.y - ts.y) * 0.5F), u32(glyph_col),
              glyph);
  return clicked;
}

bool knob(const char* id, float* value, const ImVec4& color, const char* label, float size,
          bool glow) {
  ImGui::BeginGroup();
  ImGui::PushID(id);
  const ImVec2 p0 = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton("dial", ImVec2(size, size));

  bool changed = false;
  if (ImGui::IsItemActive()) {
    const float dy = ImGui::GetIO().MouseDelta.y;
    if (dy != 0.0F) {
      *value = std::clamp(*value - dy * 0.006F, 0.0F, 1.0F);
      changed = true;
    }
  }

  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 center(p0.x + size * 0.5F, p0.y + size * 0.5F);
  const float radius = size * 0.42F;
  const float hub = size * 0.30F;

  // Arc geometry: gap at the bottom, sweep 270deg from down-left (135deg),
  // clockwise (y-down) up the left, over the top, down the right (405deg).
  const float a_min = 135.0F * kDegToRad;
  const float a_max = 405.0F * kDegToRad;
  const float a_val = a_min + std::clamp(*value, 0.0F, 1.0F) * 270.0F * kDegToRad;

  // Dark hub + track arc.
  dl->AddCircleFilled(center, hub, u32(theme::kInsetBg), 32);
  dl->AddCircle(center, hub, u32(theme::kBorder), 32, 1.0F);
  dl->PathArcTo(center, radius, a_min, a_max, 48);
  dl->PathStroke(u32(theme::kBorder, 0.8F), 0, size * 0.10F);

  // Lit value arc + its glow.
  if (glow) {
    dl->PathArcTo(center, radius, a_min, a_val, 48);
    dl->PathStroke(u32(color, 0.30F), 0, size * 0.20F);
  }
  dl->PathArcTo(center, radius, a_min, a_val, 48);
  dl->PathStroke(u32(color), 0, size * 0.11F);

  // Pointer tick from the hub edge out to the arc at the value angle.
  const ImVec2 dir(std::cos(a_val), std::sin(a_val));
  dl->AddLine(ImVec2(center.x + dir.x * hub * 0.55F, center.y + dir.y * hub * 0.55F),
              ImVec2(center.x + dir.x * radius, center.y + dir.y * radius), u32(color), 2.5F);
  dl->AddCircleFilled(center, size * 0.05F, u32(color), 12);

  // Centered label underneath the dial.
  const ImVec2 ts = ImGui::CalcTextSize(label);
  ImGui::SetCursorScreenPos(ImVec2(p0.x + (size - ts.x) * 0.5F, p0.y + size + 3.0F));
  ImGui::TextColored(theme::kTextSecondary, "%s", label);

  ImGui::PopID();
  ImGui::EndGroup();
  return changed;
}

bool xy_pad(const char* id, float* valence, float* energy, const ImVec2& size, bool glow) {
  const ImVec2 p0 = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton(id, size);
  const ImVec2 p1(p0.x + size.x, p0.y + size.y);

  bool moved = false;
  if (ImGui::IsItemActive()) {
    const ImVec2 m = ImGui::GetIO().MousePos;
    *valence = std::clamp((m.x - p0.x) / size.x, 0.0F, 1.0F);
    *energy = std::clamp(1.0F - (m.y - p0.y) / size.y, 0.0F, 1.0F);
    moved = true;
  }

  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(p0, p1, u32(theme::kInsetBg), 8.0F);
  dl->AddRect(p0, p1, u32(theme::kCyan, 0.35F), 8.0F, 0, 1.0F);

  // Grid: quarters.
  const ImU32 grid = u32(theme::kCyan, 0.10F);
  for (int i = 1; i < 4; ++i) {
    const float fx = p0.x + size.x * static_cast<float>(i) / 4.0F;
    const float fy = p0.y + size.y * static_cast<float>(i) / 4.0F;
    dl->AddLine(ImVec2(fx, p0.y), ImVec2(fx, p1.y), grid, 1.0F);
    dl->AddLine(ImVec2(p0.x, fy), ImVec2(p1.x, fy), grid, 1.0F);
  }

  const ImVec2 dot(p0.x + *valence * size.x, p0.y + (1.0F - *energy) * size.y);
  // Radial glow following the dot.
  if (glow) {
    for (int i = 4; i >= 1; --i) {
      dl->AddCircleFilled(dot, static_cast<float>(i) * 6.0F, u32(theme::kBlue, 0.06F), 24);
    }
  }
  dl->AddCircleFilled(dot, 5.0F, u32(theme::kBlue), 20);
  dl->AddCircle(dot, 6.0F, u32(theme::kText, 0.6F), 20, 1.0F);

  // Axis labels (full words, matching the design): ENERGY vertical, VAL horizontal.
  const ImU32 lc = u32(theme::kTextMuted, 0.8F);
  const float mid_y = p0.y + size.y * 0.5F;
  const char* e_plus = "ENERGY +";
  const char* e_minus = "ENERGY \xE2\x88\x92";  // U+2212 minus, per design
  const char* v_minus = "VAL \xE2\x88\x92";
  const char* v_plus = "VAL +";
  dl->AddText(ImVec2(p0.x + (size.x - ImGui::CalcTextSize(e_plus).x) * 0.5F, p0.y + 3.0F), lc,
              e_plus);
  dl->AddText(ImVec2(p0.x + (size.x - ImGui::CalcTextSize(e_minus).x) * 0.5F, p1.y - 14.0F), lc,
              e_minus);
  dl->AddText(ImVec2(p0.x + 4.0F, mid_y - 7.0F), lc, v_minus);
  dl->AddText(ImVec2(p1.x - ImGui::CalcTextSize(v_plus).x - 4.0F, mid_y - 7.0F), lc, v_plus);
  return moved;
}

void master_vu(const char* id, const ImVec2& size, bool playing, float time, bool glow) {
  (void)id;
  const ImVec2 p0 = ImGui::GetCursorScreenPos();
  const ImVec2 p1(p0.x + size.x, p0.y + size.y);
  ImDrawList* dl = ImGui::GetWindowDrawList();

  dl->AddRectFilled(p0, p1, u32(theme::kInsetBg), 6.0F);

  constexpr int kSegments = 10;
  const float gap = 3.0F;
  const float seg_w = (size.x - gap * static_cast<float>(kSegments + 1)) / kSegments;
  for (int i = 0; i < kSegments; ++i) {
    // green (0..5) -> amber (6..8) -> pink (9): a level-meter ramp.
    const ImVec4 col = i < 6 ? theme::kGreen : (i < 9 ? theme::kAmber : theme::kPink);
    float level = 0.12F;
    if (playing) {
      const float phase = time * 3.0F + static_cast<float>(i) * 0.7F;
      level = 0.28F + 0.66F * std::fabs(std::sin(phase));
      // Taper the top segments so the meter usually reads mid, peaking rarely.
      level *= 1.0F - static_cast<float>(i) * 0.045F;
    }
    level = std::clamp(level, 0.06F, 1.0F);
    const float x0 = p0.x + gap + static_cast<float>(i) * (seg_w + gap);
    const float bar_h = (size.y - gap * 2.0F) * level;
    const ImVec2 b0(x0, p1.y - gap - bar_h);
    const ImVec2 b1(x0 + seg_w, p1.y - gap);
    if (glow && playing && level > 0.5F) {
      glow_rect(dl, b0, b1, col, 2.0F, 0.7F, glow);
    }
    dl->AddRectFilled(b0, b1, u32(col, playing ? 1.0F : 0.35F), 2.0F);
  }
}

void clip_preview_waveform(ImDrawList* dl, const ImVec2& min, const ImVec2& max, std::uint32_t seed,
                           const ImVec4& color) {
  const float w = max.x - min.x;
  const float h = max.y - min.y;
  const float cy = min.y + h * 0.5F;
  constexpr int kBars = 22;
  std::uint32_t s = seed;
  const float step = w / kBars;
  for (int i = 0; i < kBars; ++i) {
    const float amp = (0.15F + 0.85F * rand01(s)) * h * 0.42F;
    const float x = min.x + (static_cast<float>(i) + 0.5F) * step;
    dl->AddLine(ImVec2(x, cy - amp), ImVec2(x, cy + amp), u32(color, 0.85F), 1.6F);
  }
}

PitchCellRect pitch_grid_cell(const ImVec2& band_min, const ImVec2& band_max, int step, int steps,
                              int pitch, int pitches) {
  const float w = band_max.x - band_min.x;
  const float h = band_max.y - band_min.y;
  const float cw = w / static_cast<float>(steps);
  const float rh = h / static_cast<float>(pitches);
  const float x0 = band_min.x + static_cast<float>(step) * cw;
  // Pitch 0 sits at the BOTTOM of the band; row index counts up from there.
  const float y1 = band_max.y - static_cast<float>(pitch) * rh;
  return PitchCellRect{ImVec2(x0, y1 - rh), ImVec2(x0 + cw, y1)};
}

void clip_preview_pianoroll(ImDrawList* dl, const ImVec2& min, const ImVec2& max,
                            const ClipPattern& pat, const ImVec4& color) {
  // A little HORIZONTAL piano-roll showing a step-cropped view of `pat`: the
  // first kCellSteps columns, all kPitches rows, drawn at the SAME
  // (step, pitch) positions the Sequence Edit canvas uses -- via the shared
  // pitch_grid_cell() helper above, so a cell dot is a legible subset of the
  // editor's blocks, never a different melody (the two views can no longer
  // silently drift apart, since both compute the rect through one function).
  // STEP runs left->right (X), PITCH low->high (Y, pitch 0 at the bottom).
  const int steps = ClipPattern::kCellSteps;
  const int pitches = ClipPattern::kPitches;
  for (int step = 0; step < steps; ++step) {
    const int pitch = pat.pitch[step];
    if (pitch < 0) {
      continue;
    }
    int span = 1;
    while (step + span < steps && pat.pitch[step + span] == pitch && span < 2) {
      ++span;
    }
    // Notes as short horizontal BARS filling most of their step column
    // (design), not centered square dots; a run of the same pitch merges
    // into a 2-wide bar -- `r0`/`r1` are this bar's first/last step cell.
    const PitchCellRect r0 = pitch_grid_cell(min, max, step, steps, pitch, pitches);
    const PitchCellRect r1 = pitch_grid_cell(min, max, step + span - 1, steps, pitch, pitches);
    const float rh = r0.max.y - r0.min.y;
    const float bar_h = std::max(2.0F, rh * 0.5F);
    const float cy = (r0.min.y + r0.max.y) * 0.5F;
    const float x0 = r0.min.x + 0.5F;
    const float x1 = r1.max.x - 1.0F;
    dl->AddRectFilled(ImVec2(x0, cy - bar_h * 0.5F), ImVec2(x1, cy + bar_h * 0.5F),
                      u32(color, 0.85F), 1.5F);
    step += span - 1;
  }
}

void sweep_bar(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float time,
               const ImVec4& color) {
  const float period = 1.7F;
  const float phase = std::fmod(time, period) / period;  // 0..1 L->R
  const float x = min.x + phase * (max.x - min.x);
  dl->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), u32(color, 0.8F), 1.5F);
  dl->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), u32(color, 0.25F), 4.0F);
}

void playhead_at(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float phase01,
                 const ImVec4& color) {
  const float phase = std::clamp(phase01, 0.0F, 1.0F);
  const float x = min.x + phase * (max.x - min.x);
  dl->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), u32(color, 0.8F), 1.5F);
  dl->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), u32(color, 0.25F), 4.0F);
}

}  // namespace sonotron::neon
