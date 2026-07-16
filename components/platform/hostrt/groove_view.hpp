#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ui_style.hpp"

// Host-only renderer for the `groove` panel: the arranger's feel as tunable
// parameters (swing, humanize timing/velocity, accent, swing grid, quantize)
// shown as labelled bar meters. Pure: no terminal access; styling via UiStyle
// roles.
//
// Seam D (docs/design/orchestrator-pipeline-extraction.md §17.2): decoupled
// from the core `arrangrr::GrooveParams`/`GrooveField` -- `GrooveViewParams`
// below redeclares the same field shape, arrangrr-free. Today (Phase 3a) the
// caller (`Shell`) fills it straight off the live in-process
// `Arranger::groove_params()`; a future pure client (Phase 3b/3c) fills the
// SAME shape from parsed `kParamState` events. `seed` is intentionally
// omitted -- the panel never renders it (it is set via the reseed key /
// command, not shown as a row).

namespace arrangrr::host {

// The parameters shown, top to bottom (swing/humanize t/humanize v/accent/
// grid/quantize).
inline constexpr std::size_t kGrooveRowCount = 6;

struct GrooveViewParams {
  std::uint8_t swing = 0;              // 0..100 %
  std::uint8_t humanize_timing = 0;    // 0..100 %
  std::uint8_t humanize_velocity = 0;  // 0..100 %
  std::uint8_t accent = 0;             // 0..100 %
  std::uint8_t swing_grid = 8;         // 8 or 16
  std::uint8_t quantize = 0;           // 0..100 %
};

std::vector<std::string> render_groove_panel(const GrooveViewParams& params, int selected, int cols,
                                             const UiStyle& style);

}  // namespace arrangrr::host
