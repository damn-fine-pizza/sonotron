#include "panel_manager.hpp"

#include "ui_style.hpp"

namespace arrangrr::host {

namespace {

constexpr std::array<const char*, kPanelCount> kPanelNames = {"help", "piano", "filter"};

// Fixed vertical stacking order (also the focus_next cycle order).
constexpr std::array<PanelId, kPanelCount> kPanelOrder = {
    PanelId::kHelp,
    PanelId::kPiano,
    PanelId::kFilter,
};

constexpr std::size_t index_of(PanelId id) { return static_cast<std::size_t>(id); }

std::string title_rule(PanelId id, bool focused, const UiStyle& style) {
  std::string title = "-- ";
  title += kPanelNames[index_of(id)];

  if (focused) {
    title += "*";
  }
  title += " --";

  const UiRole role = focused ? UiRole::kPanelTitleFocused : UiRole::kPanelTitle;
  return style.apply(role, title);
}

}  // namespace

const char* panel_name(PanelId id) { return kPanelNames[index_of(id)]; }

bool parse_panel_name(const std::string& name, PanelId& out) {
  for (std::size_t i = 0; i < kPanelCount; ++i) {
    if (name == kPanelNames[i]) {
      out = kPanelOrder[i];
      return true;
    }
  }
  return false;
}

PanelManager::PanelManager() = default;

const PanelManager::Panel& PanelManager::at(PanelId id) const { return m_panels[index_of(id)]; }

PanelManager::Panel& PanelManager::at(PanelId id) { return m_panels[index_of(id)]; }

void PanelManager::set_content(PanelId id, std::vector<std::string> lines) {
  at(id).m_lines = std::move(lines);
}

void PanelManager::open(PanelId id) { at(id).m_visible = true; }

void PanelManager::close(PanelId id) {
  at(id).m_visible = false;

  // Focus must never point at a hidden panel.
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
  m_focused = PanelId::kHelp;
}

void PanelManager::focus_next() {
  // Cycle: repl -> first visible -> next visible -> ... -> repl.
  std::size_t start = 0;
  if (m_focus == PanelFocus::kPanel) {
    start = index_of(m_focused) + 1;
  }

  for (std::size_t i = start; i < kPanelCount; ++i) {
    if (m_panels[i].m_visible) {
      m_focus = PanelFocus::kPanel;
      m_focused = kPanelOrder[i];
      return;
    }
  }
  focus_repl();
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

void PanelManager::set_layout(PanelLayout layout) { m_layout = layout; }

void PanelManager::toggle_layout() {
  m_layout =
      m_layout == PanelLayout::kVertical ? PanelLayout::kSideBySide : PanelLayout::kVertical;
}

PanelLayout PanelManager::layout() const { return m_layout; }

// One panel's title rule + capped content: the unit both layouts compose.
std::vector<std::string> PanelManager::panel_block(PanelId id, const UiStyle& style) const {
  const Panel& panel = at(id);
  std::vector<std::string> out;

  const bool focused = m_focus == PanelFocus::kPanel && m_focused == id;
  out.push_back(title_rule(id, focused, style));

  // Per-panel fairness cap: a long panel must not push its neighbours out of
  // Console's shared panel area (its global cap still applies).
  const std::size_t shown = panel.m_lines.size() > panel_layout::kMaxLinesPerPanel
                                ? panel_layout::kMaxLinesPerPanel - 1
                                : panel.m_lines.size();
  for (std::size_t i = 0; i < shown; ++i) {
    out.push_back(panel.m_lines[i]);
  }
  if (shown < panel.m_lines.size()) {
    out.push_back("  ... (" + std::to_string(panel.m_lines.size() - shown) + " more lines)");
  }

  return out;
}

std::vector<std::string> PanelManager::stacked_lines(const UiStyle& style) const {
  std::vector<std::string> out;

  for (PanelId id : kPanelOrder) {
    if (!at(id).m_visible) {
      continue;
    }

    const std::vector<std::string> block = panel_block(id, style);
    out.insert(out.end(), block.begin(), block.end());
  }
  return out;
}

std::vector<std::string> PanelManager::side_by_side_lines(int terminal_columns,
                                                          const UiStyle& style) const {
  // Collect the visible panels: the first two go in columns, the rest stack.
  std::vector<PanelId> shown;
  for (PanelId id : kPanelOrder) {
    if (at(id).m_visible) {
      shown.push_back(id);
    }
  }

  const std::vector<std::string> left = panel_block(shown[0], style);
  const std::vector<std::string> right = panel_block(shown[1], style);

  const std::size_t width = static_cast<std::size_t>(terminal_columns);
  const std::size_t column = (width - panel_layout::kSideGutterWidth) / 2;

  std::vector<std::string> out;
  const std::size_t rows = left.size() > right.size() ? left.size() : right.size();
  for (std::size_t i = 0; i < rows; ++i) {
    const std::string left_cell =
        i < left.size() ? ansi::visible_truncate(left[i], column) : std::string();
    const std::string right_cell =
        i < right.size() ? ansi::visible_truncate(right[i], column) : std::string();

    out.push_back(ansi::visible_pad(left_cell, column) + " | " + right_cell);
  }

  // A third visible panel stacks below the pair.
  for (std::size_t p = 2; p < shown.size(); ++p) {
    const std::vector<std::string> block = panel_block(shown[p], style);
    out.insert(out.end(), block.begin(), block.end());
  }

  return out;
}

std::vector<std::string> PanelManager::combined_lines(int terminal_columns,
                                                      const UiStyle& style) const {
  // Side-by-side needs two visible panels and enough width; anything else
  // falls back to the vertical stack (H3 resize contract).
  std::size_t visible_count = 0;
  for (const Panel& panel : m_panels) {
    if (panel.m_visible) {
      ++visible_count;
    }
  }

  if (m_layout == PanelLayout::kSideBySide && visible_count >= 2 &&
      terminal_columns >= panel_layout::kSideMinColumns) {
    return side_by_side_lines(terminal_columns, style);
  }

  return stacked_lines(style);
}

std::vector<std::string> PanelManager::list_lines() const {
  std::vector<std::string> out;
  out.push_back("panels:");

  for (PanelId id : kPanelOrder) {
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
  if (m_focus == PanelFocus::kRepl) {
    focus += "repl";
  } else {
    focus += panel_name(m_focused);
  }

  std::string open;
  for (PanelId id : kPanelOrder) {
    if (at(id).m_visible) {
      if (!open.empty()) {
        open += " ";
      }
      open += panel_name(id);
    }
  }
  if (open.empty()) {
    open = "none";
  }

  const char* layout = m_layout == PanelLayout::kVertical ? "vertical" : "side";
  return {"panel status: open [" + open + "]  " + focus + "  layout: " + layout};
}

}  // namespace arrangrr::host
