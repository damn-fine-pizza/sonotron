#include "panel_manager.hpp"

#include <algorithm>

#include "ui_style.hpp"

namespace arrangrr::host {

namespace {

// User-facing names, indexed by PanelId. "menu" is the help panel's name;
// "help" survives as a parse-only alias (see parse_panel_name).
constexpr std::array<const char*, kPanelCount> kPanelNames = {
    "events", "console", "styles", "chords", "piano", "menu", "filter", "empty",
};

// Fallback bottom-to-top grid order (also the focus_next cycle order): the
// default-open set sits at the bottom, extras above.
constexpr std::array<PanelId, kPanelCount> kDefaultOrder = {
    PanelId::kPiano,  PanelId::kConsole, PanelId::kStyles, PanelId::kChords,
    PanelId::kEvents, PanelId::kHelp,    PanelId::kFilter, PanelId::kEmpty,
};

constexpr std::size_t index_of(PanelId id) { return static_cast<std::size_t>(id); }

int default_height(PanelId id) {
  switch (id) {
    case PanelId::kEvents:
      return panel_layout::kEventsHeight;
    case PanelId::kConsole:
      return panel_layout::kConsoleHeight;
    case PanelId::kStyles:
      return panel_layout::kStylesHeight;
    case PanelId::kChords:
      return panel_layout::kChordsHeight;
    case PanelId::kPiano:
      return panel_layout::kPianoHeight;
    case PanelId::kHelp:
      return panel_layout::kHelpHeight;
    case PanelId::kFilter:
      return panel_layout::kFilterHeight;
    case PanelId::kEmpty:
      return panel_layout::kEmptyHeight;
  }
  return panel_layout::kEmptyHeight;
}

// Splits `budget` extra rows across cells weighted by `weights`, via
// largest-remainder; the last (top) cell wins ties, keeping the flex panel as
// tall as possible. Returns per-cell extra rows summing to max(budget, 0).
std::vector<int> distribute_budget(const std::vector<int>& weights, int budget) {
  const int n = static_cast<int>(weights.size());
  std::vector<int> extra(static_cast<std::size_t>(std::max(n, 0)), 0);
  if (budget <= 0 || n == 0) {
    return extra;
  }
  int total = 0;
  for (const int w : weights) {
    total += w;
  }
  if (total <= 0) {
    extra[static_cast<std::size_t>(n - 1)] = budget;  // no weights: give it to the top
    return extra;
  }
  std::vector<int> remainder(static_cast<std::size_t>(n));
  int assigned = 0;
  for (int k = 0; k < n; ++k) {
    const int w = weights[static_cast<std::size_t>(k)];
    extra[static_cast<std::size_t>(k)] = w * budget / total;
    remainder[static_cast<std::size_t>(k)] = w * budget % total;
    assigned += extra[static_cast<std::size_t>(k)];
  }
  int leftover = budget - assigned;
  while (leftover > 0) {
    int best = -1;
    for (int k = n - 1; k >= 0; --k) {
      if (remainder[static_cast<std::size_t>(k)] < 0) {
        continue;
      }
      if (best < 0 ||
          remainder[static_cast<std::size_t>(k)] > remainder[static_cast<std::size_t>(best)]) {
        best = k;
      }
    }
    if (best < 0) {
      extra[static_cast<std::size_t>(n - 1)] += leftover;
      break;
    }
    extra[static_cast<std::size_t>(best)] += 1;
    remainder[static_cast<std::size_t>(best)] = -1;
    --leftover;
  }
  return extra;
}

}  // namespace

const char* panel_name(PanelId id) { return kPanelNames[index_of(id)]; }

bool parse_panel_name(const std::string& name, PanelId& out) {
  if (name == "help") {  // backward-compatible alias for the "menu" panel
    out = PanelId::kHelp;
    return true;
  }
  for (std::size_t i = 0; i < kPanelCount; ++i) {
    if (name == kPanelNames[i]) {
      out = static_cast<PanelId>(i);
      return true;
    }
  }
  return false;
}

PanelManager::PanelManager() {
  for (std::size_t i = 0; i < kPanelCount; ++i) {
    const PanelId id = static_cast<PanelId>(i);
    m_panels[i].m_height = default_height(id);
  }
  at(PanelId::kEvents).m_scrolling = true;
  at(PanelId::kConsole).m_scrolling = true;
  m_order.assign(kDefaultOrder.begin(), kDefaultOrder.end());
}

const PanelManager::Panel& PanelManager::at(PanelId id) const { return m_panels[index_of(id)]; }

PanelManager::Panel& PanelManager::at(PanelId id) { return m_panels[index_of(id)]; }

void PanelManager::set_content(PanelId id, std::vector<std::string> lines) {
  at(id).m_lines = std::move(lines);
}

void PanelManager::append_line(PanelId id, std::string line) {
  std::vector<std::string>& backlog = at(id).m_backlog;
  backlog.push_back(std::move(line));
  if (backlog.size() > panel_layout::kBacklogCap) {
    const auto excess = static_cast<std::ptrdiff_t>(backlog.size() - panel_layout::kBacklogCap);
    backlog.erase(backlog.begin(), backlog.begin() + excess);
  }
}

void PanelManager::open(PanelId id) { at(id).m_visible = true; }

void PanelManager::close(PanelId id) {
  at(id).m_visible = false;
  if (m_focus == PanelFocus::kPanel && m_focused == id) {
    focus_repl();
  }
}

void PanelManager::toggle(PanelId id) {
  if (at(id).m_visible) {
    close(id);
  } else {
    open(id);
  }
}

void PanelManager::close_all() {
  for (Panel& panel : m_panels) {
    panel.m_visible = false;
  }
  focus_repl();
}

void PanelManager::focus(PanelId id) {
  open(id);
  m_focus = PanelFocus::kPanel;
  m_focused = id;
}

void PanelManager::focus_repl() {
  m_focus = PanelFocus::kRepl;
  m_focused = PanelId::kStyles;
}

void PanelManager::focus_next() {
  const std::vector<PanelId> vis = visible_order();
  if (vis.empty()) {
    focus_repl();
    return;
  }
  // Cycle repl -> first visible -> ... -> last visible -> repl.
  if (m_focus != PanelFocus::kPanel) {
    m_focus = PanelFocus::kPanel;
    m_focused = vis.front();
    return;
  }
  for (std::size_t i = 0; i < vis.size(); ++i) {
    if (vis[i] == m_focused) {
      if (i + 1 < vis.size()) {
        m_focused = vis[i + 1];
      } else {
        focus_repl();
      }
      return;
    }
  }
  m_focused = vis.front();
}

bool PanelManager::focus_number(int n) {
  const std::vector<PanelId> vis = visible_order();
  if (n < 1 || n > static_cast<int>(vis.size())) {
    return false;
  }
  focus(vis[static_cast<std::size_t>(n - 1)]);
  return true;
}

bool PanelManager::visible(PanelId id) const { return at(id).m_visible; }

bool PanelManager::any_visible() const {
  for (const Panel& panel : m_panels) {
    if (panel.m_visible) {
      return true;
    }
  }
  return false;
}

PanelFocus PanelManager::focus_kind() const { return m_focus; }

PanelId PanelManager::focused_panel() const { return m_focused; }

const std::vector<std::string>& PanelManager::content(PanelId id) const { return at(id).m_lines; }

void PanelManager::set_per_row(int per_row) {
  m_per_row =
      per_row == panel_layout::kTwoPerRow ? panel_layout::kTwoPerRow : panel_layout::kOnePerRow;
}

void PanelManager::toggle_layout() {
  set_per_row(m_per_row == panel_layout::kOnePerRow ? panel_layout::kTwoPerRow
                                                    : panel_layout::kOnePerRow);
}

int PanelManager::per_row() const { return m_per_row; }

void PanelManager::set_order(const std::vector<PanelId>& order) {
  std::vector<PanelId> next;
  std::array<bool, kPanelCount> seen{};
  for (const PanelId id : order) {
    if (!seen[index_of(id)]) {
      next.push_back(id);
      seen[index_of(id)] = true;
    }
  }
  // Any panel the caller left out keeps its fallback slot after the listed set,
  // so the order is always a full permutation.
  for (const PanelId id : kDefaultOrder) {
    if (!seen[index_of(id)]) {
      next.push_back(id);
    }
  }
  m_order = std::move(next);
}

void PanelManager::set_height(PanelId id, int rows) {
  at(id).m_height = rows > 0 ? rows : default_height(id);
}

void PanelManager::set_full_row(PanelId id, bool full) { at(id).m_full_row = full; }

std::vector<PanelId> PanelManager::visible_order() const {
  std::vector<PanelId> vis;
  for (const PanelId id : m_order) {
    if (at(id).m_visible) {
      vis.push_back(id);
    }
  }
  return vis;
}

int PanelManager::panel_number(PanelId id) const {
  const std::vector<PanelId> vis = visible_order();
  for (std::size_t i = 0; i < vis.size(); ++i) {
    if (vis[i] == id) {
      return static_cast<int>(i) + 1;
    }
  }
  return 0;
}

std::vector<PanelManager::Row> PanelManager::pair_rows(int cols) const {
  const std::vector<PanelId> vis = visible_order();
  // Two cells only fit when the terminal holds two min-width cells plus the
  // gutter; below that we never pair (a narrow cell would overflow the budget
  // its neighbour was promised) and fall back to one panel per row.
  const int min_pair_cols =
      2 * panel_layout::kMinCellWidth + static_cast<int>(panel_layout::kGutterWidth);
  const bool wide_enough = cols >= min_pair_cols;

  std::vector<Row> rows;
  std::size_t i = 0;
  while (i < vis.size()) {
    const PanelId a = vis[i];
    const bool pair = wide_enough && m_per_row == panel_layout::kTwoPerRow && !at(a).m_full_row &&
                      i + 1 < vis.size() && !at(vis[i + 1]).m_full_row;
    Row row;
    if (pair) {
      row.m_panels = {a, vis[i + 1]};
      row.m_count = panel_layout::kTwoPerRow;
      i += 2;
    } else {
      row.m_panels = {a, a};
      row.m_count = 1;
      i += 1;
    }
    rows.push_back(row);
  }
  return rows;
}

std::vector<PanelManager::Row> PanelManager::build_rows(int cols, int total_rows) const {
  std::vector<Row> rows = pair_rows(cols);
  for (Row& row : rows) {
    row.m_height = row.m_count == panel_layout::kTwoPerRow
                       ? std::max(at(row.m_panels[0]).m_height, at(row.m_panels[1]).m_height)
                       : at(row.m_panels[0]).m_height;
  }
  if (rows.empty() || total_rows <= 0) {
    return rows;
  }

  // Not enough room for even one line per row: keep the bottom-priority rows
  // (nearest the input) and drop the top ones, so the kept panels' titles
  // survive — instead of a blind tail-truncation that would eat piano/console.
  if (static_cast<int>(rows.size()) > total_rows) {
    rows.resize(static_cast<std::size_t>(total_rows));
  }
  const int n = static_cast<int>(rows.size());

  int total_target = 0;
  for (const Row& row : rows) {
    total_target += row.m_height;
  }
  if (total_target <= total_rows) {
    // Everything fits: the TOP row flexes to absorb the leftover height.
    rows[static_cast<std::size_t>(n - 1)].m_height += total_rows - total_target;
    return rows;
  }

  // Overflow: every row keeps its title (>= 1 line); the remaining budget is
  // handed out by target weight. Sum is exactly total_rows, no blind truncation.
  std::vector<int> weights(static_cast<std::size_t>(n));
  for (int k = 0; k < n; ++k) {
    weights[static_cast<std::size_t>(k)] = rows[static_cast<std::size_t>(k)].m_height;
  }
  const std::vector<int> extra = distribute_budget(weights, total_rows - n);
  for (int k = 0; k < n; ++k) {
    rows[static_cast<std::size_t>(k)].m_height = 1 + extra[static_cast<std::size_t>(k)];
  }
  return rows;
}

std::string PanelManager::title_line(PanelId id, int width, const UiStyle& style) const {
  const bool focused = m_focus == PanelFocus::kPanel && m_focused == id;
  std::string base = "-- ";
  base += panel_name(id);
  if (focused) {
    base += "*";
  }
  base += " ";

  const std::string number = "[" + std::to_string(panel_number(id)) + "]";

  std::string title = base;
  const int fill = width - static_cast<int>(base.size()) - static_cast<int>(number.size());
  if (fill > 0) {
    title.append(static_cast<std::size_t>(fill), '-');
    title += number;
  } else {
    // Too narrow for the number: dashes to the edge, drop the corner tag.
    if (static_cast<int>(title.size()) < width) {
      title.append(static_cast<std::size_t>(width - static_cast<int>(title.size())), '-');
    }
    title = ansi::visible_truncate(title, static_cast<std::size_t>(std::max(width, 0)));
  }

  const UiRole role = focused ? UiRole::kPanelTitleFocused : UiRole::kPanelTitle;
  return style.apply(role, title);
}

std::vector<std::string> PanelManager::render_cell(PanelId id, int width, int height,
                                                   const UiStyle& style) const {
  std::vector<std::string> out;
  if (height <= 0) {
    return out;
  }
  const int cell_w = std::max(width, panel_layout::kMinCellWidth);
  out.push_back(title_line(id, cell_w, style));

  const int content_rows = height - 1;
  const Panel& panel = at(id);
  const std::vector<std::string>& src = panel.m_scrolling ? panel.m_backlog : panel.m_lines;

  // Scrolling panels show the tail (a live log); static panels show the head.
  const int total = static_cast<int>(src.size());
  const int start = panel.m_scrolling && total > content_rows ? total - content_rows : 0;
  for (int r = 0; r < content_rows; ++r) {
    const int idx = start + r;
    if (idx >= 0 && idx < total) {
      out.push_back(ansi::visible_truncate(src[static_cast<std::size_t>(idx)],
                                           static_cast<std::size_t>(cell_w)));
    } else {
      out.emplace_back();  // blank padding keeps the cell exactly `height` tall
    }
  }
  return out;
}

int PanelManager::cell_width(PanelId id, int cols) const {
  // Widths depend only on pairing (per_row / full-row / order), not height.
  const std::vector<Row> rows = pair_rows(cols > 0 ? cols : 1);
  for (const Row& row : rows) {
    if (row.m_count == 1 && row.m_panels[0] == id) {
      return cols;
    }
    if (row.m_count == panel_layout::kTwoPerRow &&
        (row.m_panels[0] == id || row.m_panels[1] == id)) {
      const int left = (cols - static_cast<int>(panel_layout::kGutterWidth)) / 2;
      const int right = cols - static_cast<int>(panel_layout::kGutterWidth) - left;
      return row.m_panels[0] == id ? left : right;
    }
  }
  return cols;
}

std::vector<std::string> PanelManager::combined_lines(int cols, int rows,
                                                      const UiStyle& style) const {
  std::vector<std::string> out;
  if (rows <= 0) {
    return out;
  }
  const std::vector<Row> grid = build_rows(cols, rows);
  if (grid.empty()) {
    // Nothing visible: honour the "exactly `rows` lines" contract with a blank
    // pane instead of relying on the caller to clear it.
    out.assign(static_cast<std::size_t>(rows), std::string());
    return out;
  }

  // build_rows is bottom-to-top; paint top-to-bottom (reverse).
  for (auto it = grid.rbegin(); it != grid.rend(); ++it) {
    const Row& row = *it;
    if (row.m_count == 1) {
      std::vector<std::string> cell = render_cell(row.m_panels[0], cols, row.m_height, style);
      for (std::string& line : cell) {
        out.push_back(std::move(line));
      }
      continue;
    }
    const int left = (cols - static_cast<int>(panel_layout::kGutterWidth)) / 2;
    const int right = cols - static_cast<int>(panel_layout::kGutterWidth) - left;
    const std::vector<std::string> lcell = render_cell(row.m_panels[0], left, row.m_height, style);
    const std::vector<std::string> rcell = render_cell(row.m_panels[1], right, row.m_height, style);
    for (int r = 0; r < row.m_height; ++r) {
      const std::string lc =
          r < static_cast<int>(lcell.size()) ? lcell[static_cast<std::size_t>(r)] : std::string();
      const std::string rc =
          r < static_cast<int>(rcell.size()) ? rcell[static_cast<std::size_t>(r)] : std::string();
      out.push_back(ansi::visible_pad(lc, static_cast<std::size_t>(std::max(left, 0))) + " | " +
                    rc);
    }
  }

  // Safety net: the grid math already tiles to `rows`, but never overrun the
  // pane Console reserved for us.
  if (static_cast<int>(out.size()) > rows) {
    out.resize(static_cast<std::size_t>(rows));
  }
  while (static_cast<int>(out.size()) < rows) {
    out.emplace_back();
  }
  return out;
}

std::vector<std::string> PanelManager::list_lines() const {
  std::vector<std::string> out;
  out.push_back("panels:");
  for (const PanelId id : m_order) {
    std::string line = "  ";
    line += panel_name(id);
    line += at(id).m_visible ? "   open" : "   closed";
    if (m_focus == PanelFocus::kPanel && m_focused == id) {
      line += "  (focused)";
    }
    out.push_back(line);
  }
  if (m_focus == PanelFocus::kRepl) {
    out.push_back("  focus: repl");
  }
  return out;
}

std::vector<std::string> PanelManager::status_lines() const {
  std::string focus = "focus: ";
  focus += m_focus == PanelFocus::kRepl ? "repl" : panel_name(m_focused);

  std::string open;
  for (const PanelId id : visible_order()) {
    if (!open.empty()) {
      open += " ";
    }
    open += panel_name(id);
  }
  if (open.empty()) {
    open = "none";
  }

  return {"panel status: open [" + open + "]  " + focus + "  layout: " + std::to_string(m_per_row) +
          "/row"};
}

}  // namespace arrangrr::host
