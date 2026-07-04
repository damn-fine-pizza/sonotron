#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "ui_style.hpp"

// Host-only uniform panel/grid model (H3, docs/TUI_SPEC.md). Everything on
// screen except the input line is a panel; the status bar is a fixed line just
// above the input. Pure data + line composition: no terminal access, no raw
// ANSI (styling goes through UiStyle roles). The composed grid — EXACTLY the
// requested number of rows — travels through Shell::PanelHook into Console,
// which paints it verbatim over rows 1..H-2.

namespace arrangrr::host {

// Every panel is first-class and toggled through the `panel` command. The
// enum order is the FALLBACK bottom-to-top grid order; a ~/.arrangrr.rc file
// (or set_order) can override it.
enum class PanelId {
  kEvents,   // MIDI log (scrolling)
  kConsole,  // command echo + output + errors (scrolling)
  kStyles,   // style + variations + key + the interactive chooser (always present)
  kChords,   // chord detection (placeholder)
  kPiano,    // simulated keyboard
  kHelp,     // contextual menu / help (off by default)
  kFilter,   // event filter summary (off by default)
  kEmpty,    // blank spacer (off by default)
};

inline constexpr std::size_t kPanelCount = 8;

enum class PanelFocus {
  kRepl,
  kPanel,
};

namespace panel_layout {
// Two-per-row needs a usable cell; the shared gutter is " | ".
inline constexpr std::size_t kGutterWidth = 3;

// Scrolling panels keep a deep append backlog and render only their cell-height
// tail, so they read like a scrolling log without a real scroll region.
inline constexpr std::size_t kBacklogCap = 512;

inline constexpr int kOnePerRow = 1;
inline constexpr int kTwoPerRow = 2;

// Default cell heights (title row included). The top grid row always flexes to
// fill the remaining height regardless of these.
inline constexpr int kEventsHeight = 6;
inline constexpr int kConsoleHeight = 6;
inline constexpr int kStylesHeight = 9;
inline constexpr int kChordsHeight = 4;
inline constexpr int kPianoHeight = 7;
inline constexpr int kHelpHeight = 12;  // fixed panel below events: fit the contextual help
inline constexpr int kFilterHeight = 3;
inline constexpr int kEmptyHeight = 1;

inline constexpr int kMinCellWidth = 8;  // below this a title is barely legible
}  // namespace panel_layout

// Canonical user-facing panel names ("events", "console", "styles", ...).
const char* panel_name(PanelId id);

// Parses a user-facing panel name; returns false when unknown. "help" is a
// backward-compatible alias for the "menu" panel (PanelId::kHelp).
bool parse_panel_name(const std::string& name, PanelId& out);

class PanelManager {
 public:
  PanelManager();

  // Static panel body (piano keyboard, help text, chooser render, ...).
  void set_content(PanelId id, std::vector<std::string> lines);
  // Appends one line to a scrolling panel's backlog (events, console); the tail
  // is what renders. A no-op-safe bound keeps memory flat over long sessions.
  void append_line(PanelId id, std::string line);

  void open(PanelId id);
  void close(PanelId id);
  void toggle(PanelId id);
  void close_all();

  // Focusing a hidden panel opens it: focus implies visibility.
  void focus(PanelId id);
  void focus_repl();
  // Cycles repl -> first visible panel -> ... -> repl (bottom-to-top order).
  void focus_next();
  void focus_prev();  // reverse of focus_next (SHIFT+TAB)
  // Focuses the visible panel at 1-based grid position `n` (TAB+digit). Returns
  // false when no visible panel holds that number.
  bool focus_number(int n);

  bool visible(PanelId id) const;
  bool any_visible() const;
  PanelFocus focus_kind() const;
  PanelId focused_panel() const;
  const std::vector<std::string>& content(PanelId id) const;

  // 1 or 2 panels per grid row (layout1 / layout2). toggle_layout flips them.
  void set_per_row(int per_row);
  void toggle_layout();
  int per_row() const;

  // Declarative overrides (~/.arrangrr.rc). set_order takes a bottom-to-top
  // list; ids omitted keep their fallback slot after the listed ones.
  void set_order(const std::vector<PanelId>& order);
  void set_height(PanelId id, int rows);
  void set_full_row(PanelId id, bool full);

  // The visible panels composed into EXACTLY `rows` lines at `cols` columns:
  // a bottom-to-top grid whose TOP row flexes to fill the leftover height, the
  // others taking their declared/default height. Each panel carries an ASCII
  // title rule ending in its 1-based grid number ("-- styles -----[3]"); the
  // focused panel's title carries a '*'. Titles style through `style`.
  std::vector<std::string> combined_lines(int cols, int rows, const UiStyle& style = {}) const;

  // The width a panel's cell renders at in the current grid (a two-per-row
  // panel gets half; the piano regenerates its keyboard at this width).
  int cell_width(PanelId id, int cols) const;

  // 1-based grid position of a visible panel (bottom-to-top), or 0 when hidden.
  int panel_number(PanelId id) const;

  // One line per panel for `panel list` / `panel status`.
  std::vector<std::string> list_lines() const;
  std::vector<std::string> status_lines() const;

 private:
  struct Panel {
    bool m_visible = false;
    bool m_scrolling = false;
    bool m_full_row = false;
    int m_height = 0;  // target cell rows (title included)
    std::vector<std::string> m_lines;
    std::vector<std::string> m_backlog;
  };

  // A laid-out grid row: one or two panels plus its resolved pixel height.
  struct Row {
    std::array<PanelId, panel_layout::kTwoPerRow> m_panels{};
    int m_count = 0;
    int m_height = 0;
  };

  const Panel& at(PanelId id) const;
  Panel& at(PanelId id);
  std::vector<PanelId> visible_order() const;
  // Groups visible panels into rows (which panels share a row): width-aware,
  // heights NOT set. Two-per-row only when the terminal is wide enough for two
  // min-width cells + the gutter, else one-per-row.
  std::vector<Row> pair_rows(int cols) const;
  // pair_rows + height allocation (top row flexes; proportional shrink that
  // keeps every title when short) so the result tiles to exactly `total_rows`.
  std::vector<Row> build_rows(int cols, int total_rows) const;
  std::vector<std::string> render_cell(PanelId id, int width, int height,
                                       const UiStyle& style) const;
  std::string title_line(PanelId id, int width, const UiStyle& style) const;

  std::array<Panel, kPanelCount> m_panels;
  std::vector<PanelId> m_order;  // bottom-to-top grid order
  PanelFocus m_focus = PanelFocus::kRepl;
  PanelId m_focused = PanelId::kStyles;
  int m_per_row = panel_layout::kOnePerRow;
};

}  // namespace arrangrr::host
