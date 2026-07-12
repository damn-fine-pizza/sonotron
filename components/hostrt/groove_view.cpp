#include "groove_view.hpp"

#include <array>
#include <cstdio>

namespace arrangrr::host {

namespace {

struct GrooveRow {
  const char* label;
  const char* hint;
};

// Display order matches GrooveViewParams' field order (and the panel's own
// long-standing row order); row `i` reads/writes field `i` below.
constexpr std::array<GrooveRow, kGrooveRowCount> kRows = {{
    {.label = "swing", .hint = "off-beat push"},
    {.label = "humanize t", .hint = "timing wobble"},
    {.label = "humanize v", .hint = "velocity wobble"},
    {.label = "accent", .hint = "downbeat emphasis"},
    {.label = "swing grid", .hint = "8th / 16th offbeats"},
    {.label = "quantize", .hint = "pull timing to grid"},
}};

std::uint8_t row_value(const GrooveViewParams& p, std::size_t row) {
  switch (row) {
    case 0:
      return p.swing;
    case 1:
      return p.humanize_timing;
    case 2:
      return p.humanize_velocity;
    case 3:
      return p.accent;
    case 4:
      return p.swing_grid;
    case 5:
      return p.quantize;
    default:
      return 0;
  }
}

bool row_is_swing_grid(std::size_t row) { return row == 4; }

// A 10-cell bar for a 0..100 percent value.
std::string bar(std::uint8_t pct) {
  const int filled = (pct > 100 ? 100 : pct) / 10;
  std::string s = "[";
  for (int i = 0; i < 10; ++i) {
    s += (i < filled) ? '#' : '-';
  }
  s += "]";
  return s;
}

}  // namespace

std::vector<std::string> render_groove_panel(const GrooveViewParams& params, int selected, int cols,
                                             const UiStyle& style) {
  std::vector<std::string> out;
  for (std::size_t i = 0; i < kRows.size(); ++i) {
    const GrooveRow& row = kRows[i];
    const char cursor = (static_cast<int>(i) == selected) ? '>' : ' ';
    const std::uint8_t value = row_value(params, i);

    char buf[128];
    if (row_is_swing_grid(i)) {
      std::snprintf(buf, sizeof(buf), "%c %-11s   grid: %-4u  %s", cursor, row.label,
                    static_cast<unsigned>(value), row.hint);
    } else {
      std::snprintf(buf, sizeof(buf), "%c %-11s %s %3u%%  %s", cursor, row.label,
                    bar(value).c_str(), static_cast<unsigned>(value), row.hint);
    }
    std::string line(buf);
    if (static_cast<int>(i) == selected) {
      line = style.apply(UiRole::kSuccess, line);
    }
    out.push_back(ansi::visible_truncate(line, static_cast<std::size_t>(cols > 0 ? cols : 0)));
  }
  out.push_back("up/down param | left/right adjust | r reseed | TAB exit");
  return out;
}

}  // namespace arrangrr::host
