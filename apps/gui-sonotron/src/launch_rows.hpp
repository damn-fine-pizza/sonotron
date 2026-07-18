#pragma once

#include <array>
#include <cstddef>

#include "grid_model.hpp"
#include "neon_widgets.hpp"
#include "seqedit_model.hpp"

// The 6 REAL launch-grid rows (v02-workstation-spec.md §2b) shared by
// grid_panel.cpp (the Repeat Zone) and seqedit_panel.cpp (Sequence Edit) --
// lifted out of grid_panel.cpp (Fabrizio review, 2026-07-18: seqedit's own
// lane/track-set used to walk all 9 kTrackRoleCount roles from a STATIC
// style table, while the Repeat Zone only ever surfaces these 6 real rows
// resolved from REAL per-cell content -- three phantom lanes with no
// matching Repeat-Zone row, plus mismatched note content for a live
// step-track cell). Both panels now walk this ONE role set and resolve a
// cell's preview through the ONE resolver below, so the two views can never
// silently diverge again. GridModel has 9 role rows; these are the 6 the
// launch grid (and, following it, Sequence Edit) surfaces.

namespace sonotron {

struct GridRow {
  const char* name;
  std::size_t role_index;  // into GridModel / track_roles
  bool audio;
};

// `audio` marks the pad row as the design's one "audio" row -- it no longer
// selects a different preview widget (repeat-zone-real-contract.md STEP 3:
// pad has no real audio content yet, so it shows its real MIDI note pattern
// too, like every other row); `audio` is kept only for `fx.open_audio`
// bookkeeping, reserved for genuine future audio content.
inline constexpr std::array<GridRow, 6> kRows = {{
    {"drums", 0, false},
    {"bass", 2, false},
    {"chord", 3, false},
    {"pad", 5, true},
    {"arp", 6, false},
    {"lead", 8, false},
}};

// Result of resolving a track cell's mini-preview. An empty cell resolves to
// the default (blank, non-approx) pattern.
struct TrackCellPreview {
  neon::ClipPattern pattern{};
  bool approx = false;
};

// Task #11 Phase 1: a step-track cell's mini-preview is the pattern's OWN
// live content (preview_for_track), never the style-section table -- that
// table has nothing to do with this cell. A style-section cell keeps the
// existing preview_for(...) lookup unchanged. Shared VERBATIM by
// grid_panel.cpp's render_track_cell (the Repeat Zone's own mini-preview)
// and seqedit_panel.cpp's draw_piano_roll_lanes (the Sequence Edit canvas)
// -- ONE resolver, so a given cell's REAL content (blank/live-step/
// style-section) reads identically in both panels by construction.
TrackCellPreview resolve_track_cell_preview(const GridModel& model, const SeqEditModel& seqedit,
                                            const GridRow& row, std::size_t s, const GridCell& cell,
                                            bool filled, int active_style);

}  // namespace sonotron
