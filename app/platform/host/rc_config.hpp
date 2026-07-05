#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "panel_manager.hpp"

// Dependency-free parser for ~/.arrangrr.rc (host-only). The file overrides the
// panel grid: which panels are open, their bottom-to-top order, per-panel
// heights, full-row flags, and the layout (1 or 2 panels per row). A missing
// file yields the built-in defaults (an empty config that changes nothing).
//
// Grammar (line-based, '#' starts a comment; blank lines ignored):
//   layout = 2                 -> 1 or 2 panels per row
//   [panels]                   -> begins the ordered panel list (bottom-to-top)
//     piano                    -> open at its default height
//     console = 6              -> open with a declared height (rows)
//     *events                  -> a full-row panel (never paired in layout 2)
//     empty                    -> a blank spacer panel
//   # anything                 -> comment
// Unknown panel names produce a warning and are ignored.

namespace arrangrr::host {

struct RcPanel {
  PanelId id{};
  bool full_row = false;
  int height = 0;  // 0 = the panel's default height
};

struct RcConfig {
  bool has_layout = false;
  int per_row = panel_layout::kOnePerRow;
  std::vector<RcPanel> order;  // bottom-to-top, recognized panels only
  std::vector<std::string> warnings;
};

// Parses a config from any stream. Never throws; malformed lines warn.
RcConfig parse_rc(std::istream& in);

// Loads and parses `path`. A missing/unreadable file returns a default config
// (no layout override, empty order): the caller keeps its built-in defaults.
RcConfig load_rc(const std::string& path);

}  // namespace arrangrr::host
