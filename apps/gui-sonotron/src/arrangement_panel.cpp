#include "arrangement_panel.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <vector>

#include "imgui.h"

namespace sonotron {

namespace {

// Per-lane color, distinct hues so the stacked bands read apart. Indexed by
// lane order (drums, bass, keys, pad, arp — see mock_arrangement()).
constexpr std::array<ImU32, 5> kLaneColors = {
    IM_COL32(0xE0, 0x6C, 0x5C, 0xFF),  // drums  — coral
    IM_COL32(0xE0, 0xA8, 0x40, 0xFF),  // bass   — amber
    IM_COL32(0x66, 0xC0, 0x7A, 0xFF),  // keys   — green
    IM_COL32(0x4C, 0xA8, 0xD0, 0xFF),  // pad    — blue
    IM_COL32(0x9A, 0x86, 0xD8, 0xFF),  // arp    — violet
};
constexpr ImU32 kMutedColor = IM_COL32(112, 112, 122, 255);
constexpr ImU32 kNowColor = IM_COL32(245, 245, 250, 255);
constexpr ImU32 kSectionColor = IM_COL32(210, 210, 220, 255);
constexpr ImU32 kGhostTextColor = IM_COL32(224, 220, 240, 255);
constexpr ImU32 kLabelColor = IM_COL32(245, 245, 248, 255);
constexpr float kNowFraction = 1.0F / 3.0F;  // NOW pinned this far from the left
constexpr float kFillFraction = 0.82F;       // how much of the height the fattest stack fills

// Draws text with a dark 1px halo so labels stay legible on any band color,
// bright or ghost-dimmed.
void draw_outlined_text(ImDrawList* draw, ImVec2 pos, ImU32 fill, const char* text) {
  const ImU32 halo = IM_COL32(0, 0, 0, 205);
  for (int dx = -1; dx <= 1; ++dx) {
    for (int dy = -1; dy <= 1; ++dy) {
      if (dx != 0 || dy != 0) {
        draw->AddText(ImVec2(pos.x + static_cast<float>(dx), pos.y + static_cast<float>(dy)), halo,
                      text);
      }
    }
  }
  draw->AddText(pos, fill, text);
}

// Smoothstep-interpolated density at a fractional bar, so the bands flow as
// curves rather than per-bar steps.
float density_smooth(const ArrangementLane& lane, float bar) {
  const int b0 = static_cast<int>(std::floor(bar));
  const float d0 = density_at(lane, b0);
  const float d1 = density_at(lane, b0 + 1);
  float t = bar - static_cast<float>(b0);
  t = t * t * (3.0F - 2.0F * t);
  return d0 + (d1 - d0) * t;
}

ImU32 lane_color(const ArrangementLane& lane, std::size_t index) {
  return lane.muted ? kMutedColor : kLaneColors[index % kLaneColors.size()];
}

// Geometry + sampled streamgraph for one frame. All screen-space; the sampled
// density buffer is shared by every draw pass so they agree on band shapes.
struct Graph {
  ImDrawList* draw = nullptr;
  float x0 = 0.0F;
  float y0 = 0.0F;
  float w = 0.0F;
  float h = 0.0F;
  float cy = 0.0F;
  float view_start = 0.0F;
  float horizon = 0.0F;
  int samples = 0;
  std::size_t lanes = 0;
  std::vector<float> dens;   // samples * lanes
  std::vector<float> total;  // samples
  float scale = 0.0F;

  float bar_to_x(float bar) const { return x0 + (bar - view_start) / horizon * w; }
  float sample_x(int i) const {
    return x0 + w * static_cast<float>(i) / static_cast<float>(samples - 1);
  }
  float sample_bar(int i) const {
    return view_start + horizon * static_cast<float>(i) / static_cast<float>(samples - 1);
  }
  float stack_top(int i) const { return cy - total[static_cast<std::size_t>(i)] * scale * 0.5F; }
  float d(int i, std::size_t l) const { return dens[static_cast<std::size_t>(i) * lanes + l]; }
  // y of the top edge of lane `l`'s band at sample `i` (bands stack downward).
  float band_top(int i, std::size_t l) const {
    float below = 0.0F;
    for (std::size_t b = 0; b < l; ++b) {
      below += d(i, b);
    }
    return stack_top(i) + below * scale;
  }
};

// Fills in just the screen geometry of a Graph; the caller sets the time
// mapping (horizon/view_start) and runs the sampling pass.
Graph make_graph_geometry(ImDrawList* draw, float x0, float y0, float w, float h) {
  Graph g;
  g.draw = draw;
  g.x0 = x0;
  g.y0 = y0;
  g.w = w;
  g.h = h;
  g.cy = y0 + h * 0.5F;
  return g;
}

// Index of the sample where lane `l` is thickest (for the inline label).
int thickest_sample(const Graph& g, std::size_t l) {
  int best_i = 0;
  float best = -1.0F;
  for (int i = 0; i < g.samples; ++i) {
    if (g.d(i, l) > best) {
      best = g.d(i, l);
      best_i = i;
    }
  }
  return best_i;
}

// First sample where lane `l` becomes audible (for the ghost move label), or
// -1 if it never does within the view.
int entrance_sample(const Graph& g, std::size_t l) {
  for (int i = 0; i < g.samples; ++i) {
    if (g.d(i, l) > 0.05F) {
      return i;
    }
  }
  return -1;
}

void draw_bands(const Graph& g, const Arrangement& arr) {
  for (std::size_t l = 0; l < g.lanes; ++l) {
    const ImU32 color = lane_color(arr.lanes[l], l);
    for (int i = 0; i + 1 < g.samples; ++i) {
      if (g.d(i, l) <= 0.001F && g.d(i + 1, l) <= 0.001F) {
        continue;
      }
      const float top0 = g.band_top(i, l);
      const float top1 = g.band_top(i + 1, l);
      const float bot0 = top0 + g.d(i, l) * g.scale;
      const float bot1 = top1 + g.d(i + 1, l) * g.scale;
      g.draw->AddQuadFilled(ImVec2(g.sample_x(i), top0), ImVec2(g.sample_x(i + 1), top1),
                            ImVec2(g.sample_x(i + 1), bot1), ImVec2(g.sample_x(i), bot0), color);
    }
  }
}

// One translucent wash over everything right of NOW, giving a clean vertical
// PAST|FUTURE boundary at the playhead.
void draw_future_wash(const Graph& g, float now_x) {
  if (now_x < g.x0 + g.w) {
    g.draw->AddRectFilled(ImVec2(std::max(now_x, g.x0), g.y0), ImVec2(g.x0 + g.w, g.y0 + g.h),
                          IM_COL32(16, 16, 20, 150));
  }
}

// Inline lane labels at each band's thickest point. Lanes with a ghost are
// skipped — their ghost label already names them.
void draw_inline_labels(const Graph& g, const Arrangement& arr) {
  for (std::size_t l = 0; l < g.lanes; ++l) {
    if (lane_has_ghost(arr.lanes[l])) {
      continue;
    }
    const int i = thickest_sample(g, l);
    std::array<char, 32> label{};
    std::snprintf(label.data(), label.size(), "%s%s", arr.lanes[l].name,
                  arr.lanes[l].muted ? " (muted)" : "");
    const ImVec2 text_size = ImGui::CalcTextSize(label.data());
    if (g.d(i, l) * g.scale < text_size.y * 1.15F) {
      continue;  // band too thin to hold its name legibly
    }
    const float mid = g.band_top(i, l) + g.d(i, l) * g.scale * 0.5F;
    draw_outlined_text(g.draw, ImVec2(g.sample_x(i) - text_size.x * 0.5F, mid - text_size.y * 0.5F),
                       kLabelColor, label.data());
  }
}

// Ghost move labels (e.g. "Pad enters -> 4") near each pending lane's entrance.
void draw_ghost_labels(const Graph& g, const Arrangement& arr) {
  for (std::size_t l = 0; l < g.lanes; ++l) {
    const int i = entrance_sample(g, l);
    if (!lane_has_ghost(arr.lanes[l]) || i < 0) {
      continue;
    }
    std::array<char, 64> text{};
    std::snprintf(text.data(), text.size(), "%s %s", arr.lanes[l].name, arr.lanes[l].ghost);
    const ImVec2 pos(std::min(g.sample_x(i) + 4.0F, g.x0 + g.w - 120.0F),
                     g.band_top(i, l) - ImGui::GetTextLineHeight() - 2.0F);
    draw_outlined_text(g.draw, pos, kGhostTextColor, text.data());
  }
}

// Section strip above the graph: a shaded span + centered label per section.
void draw_sections(const Graph& g, const Arrangement& arr, float strip_y0, float strip_h) {
  for (const Section& s : arr.sections) {
    const float sx0 = std::max(g.bar_to_x(static_cast<float>(s.start_bar)), g.x0);
    const float sx1 =
        std::min(g.bar_to_x(static_cast<float>(s.start_bar + s.length_bars)), g.x0 + g.w);
    if (sx1 <= sx0) {
      continue;
    }
    const float mid_bar =
        static_cast<float>(s.start_bar) + static_cast<float>(s.length_bars) * 0.5F;
    const bool future = mid_bar > arr.now_bar;
    const ImU32 shade = future ? IM_COL32(40, 40, 50, 120) : IM_COL32(56, 56, 68, 160);
    g.draw->AddRectFilled(ImVec2(sx0, strip_y0), ImVec2(sx1, strip_y0 + strip_h - 3.0F), shade,
                          3.0F);
    g.draw->AddLine(ImVec2(sx0, strip_y0), ImVec2(sx0, strip_y0 + strip_h - 3.0F),
                    IM_COL32(90, 90, 105, 200), 1.0F);
    const ImVec2 ts = ImGui::CalcTextSize(s.name);
    if (ts.x < sx1 - sx0) {
      g.draw->AddText(ImVec2((sx0 + sx1) * 0.5F - ts.x * 0.5F, strip_y0 + 1.0F),
                      future ? kGhostTextColor : kSectionColor, s.name);
    }
  }
}

// NOW playhead: a bright vertical line through the whole surface + tag.
void draw_now(const Graph& g, float strip_y0, float now_x) {
  if (now_x >= g.x0 && now_x <= g.x0 + g.w) {
    g.draw->AddLine(ImVec2(now_x, strip_y0), ImVec2(now_x, g.y0 + g.h), kNowColor, 2.0F);
    g.draw->AddText(ImVec2(now_x + 4.0F, g.y0 + 2.0F), kNowColor, "NOW");
  }
}

}  // namespace

void render_arrangement_panel(const Arrangement& arr) {
  // View zoom (bars visible across the width). The only state the panel owns:
  // an ephemeral view setting, not model data. Static is fine until a real
  // view-state object flows in from main.
  static float horizon_bars = 12.0F;

  const ImGuiStyle& style = ImGui::GetStyle();
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const ImVec2 avail = ImGui::GetContentRegionAvail();
  if (avail.x < 32.0F || avail.y < 32.0F || arr.lanes.empty()) {
    return;
  }

  const float section_h = ImGui::GetTextLineHeight() + 6.0F;
  const float slider_h = ImGui::GetFrameHeight() + style.ItemSpacing.y;
  const float graph_h = avail.y - section_h - slider_h;
  if (graph_h < 24.0F) {
    return;
  }

  Graph g = make_graph_geometry(draw, origin.x, origin.y + section_h, avail.x, graph_h);
  g.horizon = horizon_bars;
  g.view_start = arr.now_bar - horizon_bars * kNowFraction;
  g.lanes = arr.lanes.size();
  g.samples = std::clamp(static_cast<int>(g.w / 3.0F), 16, 512);

  // Pass 1: sample densities and size the vertical scale.
  g.dens.assign(static_cast<std::size_t>(g.samples) * g.lanes, 0.0F);
  g.total.assign(static_cast<std::size_t>(g.samples), 0.0F);
  float max_total = 0.0F;
  for (int i = 0; i < g.samples; ++i) {
    const float bar = g.sample_bar(i);
    float sum = 0.0F;
    for (std::size_t l = 0; l < g.lanes; ++l) {
      const float d = density_smooth(arr.lanes[l], bar);
      g.dens[static_cast<std::size_t>(i) * g.lanes + l] = d;
      sum += d;
    }
    g.total[static_cast<std::size_t>(i)] = sum;
    max_total = std::max(max_total, sum);
  }
  g.scale = max_total > 0.0F ? (g.h * kFillFraction) / max_total : 0.0F;

  const float now_x = g.bar_to_x(arr.now_bar);
  draw_bands(g, arr);
  draw_future_wash(g, now_x);
  draw_inline_labels(g, arr);
  draw_ghost_labels(g, arr);
  draw_sections(g, arr, origin.y, section_h);
  draw_now(g, origin.y, now_x);

  // Horizon (zoom) slider along the bottom.
  ImGui::SetCursorScreenPos(ImVec2(g.x0, g.y0 + g.h + style.ItemSpacing.y));
  ImGui::SetNextItemWidth(std::min(g.w * 0.5F, 280.0F));
  const float max_horizon = static_cast<float>(std::max(4, arr.total_bars));
  ImGui::SliderFloat("horizon", &horizon_bars, 4.0F, max_horizon, "%.0f bars");
  horizon_bars = std::clamp(horizon_bars, 4.0F, max_horizon);
}

}  // namespace sonotron
