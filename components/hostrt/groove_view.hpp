#pragma once

#include <string>
#include <vector>

#include "arrangrr/arranger/groove.hpp"
#include "ui_style.hpp"

// Host-only renderer for the `groove` panel: the arranger's feel as tunable
// parameters (swing, humanize timing/velocity, accent, swing grid, quantize)
// shown as labelled bar meters. Pure: no terminal access; styling via UiStyle
// roles.

namespace arrangrr::host {

// The parameters shown, top to bottom (maps to swing/humanize/accent/grid/
// quantize; seed is set via the command / reseed key, not a slider).
inline constexpr std::size_t kGrooveRowCount = 6;

std::vector<std::string> render_groove_panel(const GrooveParams& params, int selected, int cols,
                                             const UiStyle& style);

// The GrooveField edited at panel row `index`.
GrooveField groove_row_field(std::size_t index);

}  // namespace arrangrr::host
