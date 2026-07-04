#include "groove_view.hpp"

#include <array>
#include <cstdio>

namespace arrangrr::host {

namespace {

struct GrooveRow {
  GrooveField field;
  const char* label;
  const char* hint;
};

constexpr std::array<GrooveRow, kGrooveRowCount> kRows = {{
    {.field = GrooveField::kSwing, .label = "swing", .hint = "off-beat push"},
    {.field = GrooveField::kHumanizeTiming, .label = "humanize t", .hint = "timing wobble"},
    {.field = GrooveField::kHumanizeVelocity, .label = "humanize v", .hint = "velocity wobble"},
    {.field = GrooveField::kAccent, .label = "accent", .hint = "downbeat emphasis"},
    {.field = GrooveField::kSwingGrid, .label = "swing grid", .hint = "8th / 16th offbeats"},
}};

std::uint8_t field_value(const GrooveParams& p, GrooveField field) {
  switch (field) {
    case GrooveField::kSwing:
      return p.swing;
    case GrooveField::kHumanizeTiming:
      return p.humanize_timing;
    case GrooveField::kHumanizeVelocity:
      return p.humanize_velocity;
    case GrooveField::kAccent:
      return p.accent;
    case GrooveField::kSwingGrid:
      return p.swing_grid;
    case GrooveField::kSeed:
      return 0;
  }
  return 0;
}

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

GrooveField groove_row_field(std::size_t index) {
  return kRows[index < kRows.size() ? index : 0].field;
}

std::vector<std::string> render_groove_panel(const GrooveParams& params, int selected, int cols,
                                             const UiStyle& style) {
  std::vector<std::string> out;
  for (std::size_t i = 0; i < kRows.size(); ++i) {
    const GrooveRow& row = kRows[i];
    const char cursor = (static_cast<int>(i) == selected) ? '>' : ' ';
    const std::uint8_t value = field_value(params, row.field);

    char buf[128];
    if (row.field == GrooveField::kSwingGrid) {
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
