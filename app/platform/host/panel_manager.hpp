#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

// Host-only multi-panel state (H1, docs/TUI_SPEC.md). Pure data + line
// composition: no terminal access, no ANSI. The composed block travels
// through the existing Shell::PanelHook into Console, whose global height
// cap still applies on top of the per-panel fairness cap below.

namespace arrangrr::host {

enum class PanelId {
  kHelp,
  kPiano,
  kFilter,
};

enum class PanelFocus {
  kRepl,
  kPanel,
};

inline constexpr std::size_t kPanelCount = 3;

namespace panel_layout {
// Fairness cap: with several panels stacked vertically, no single panel may
// starve the ones below it out of Console's shared panel area. Sized to fit
// the largest help topic (the command overview).
inline constexpr std::size_t kMaxLinesPerPanel = 12;
}  // namespace panel_layout

// Canonical user-facing panel names ("help", "piano", "filter").
const char* panel_name(PanelId id);

// Parses a user-facing panel name; returns false when unknown.
bool parse_panel_name(const std::string& name, PanelId& out);

class PanelManager {
 public:
  PanelManager();

  void set_content(PanelId id, std::vector<std::string> lines);
  void open(PanelId id);
  void close(PanelId id);
  void toggle(PanelId id);
  void close_all();

  // Focusing a hidden panel opens it: focus implies visibility.
  void focus(PanelId id);
  void focus_repl();
  // Cycles repl -> first visible panel -> ... -> repl.
  void focus_next();

  bool visible(PanelId id) const;
  bool any_visible() const;
  PanelFocus focus_kind() const;
  PanelId focused_panel() const;
  const std::vector<std::string>& content(PanelId id) const;

  // Visible panels stacked vertically, each preceded by an ASCII title rule;
  // the focused panel's title carries a '*' marker.
  std::vector<std::string> combined_lines() const;

  // One line per panel for `panel list` / `panel status`.
  std::vector<std::string> list_lines() const;
  std::vector<std::string> status_lines() const;

 private:
  struct Panel {
    bool m_visible = false;
    std::vector<std::string> m_lines;
  };

  const Panel& at(PanelId id) const;
  Panel& at(PanelId id);

  std::array<Panel, kPanelCount> m_panels;
  PanelFocus m_focus = PanelFocus::kRepl;
  PanelId m_focused = PanelId::kHelp;
};

}  // namespace arrangrr::host
