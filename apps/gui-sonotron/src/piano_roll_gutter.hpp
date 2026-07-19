#pragma once

#include "imgui.h"

// Sequence Edit Phase-2 piano-roll pitch-axis gutter (docs/proposals/
// seqedit-piano-roll-phase2-design.md §6): a thin piano-keyboard strip along
// the left edge of the piano-roll canvas, one row per MIDI note in
// [low_note, high_note] (inclusive), so a user can tell which absolute pitch
// each canvas row represents. Pure draw-list helper -- no ImGui widget, no
// model dependency, mirrors neon_widgets.hpp's own "reusable draw-list
// widget" discipline.

namespace sonotron {

// Draws one row per note in [low_note, high_note] (inclusive) within screen
// rect [min, max]. Row 0 (`low_note`) sits at the BOTTOM (`max.y`), row
// (high_note - low_note) at the TOP (`min.y`) -- this matches the "row =
// note, low->high bottom->top" pitch-axis convention the piano-roll canvas
// itself uses for its own note rows, so a note drawn by the canvas always
// lines up with the correct gutter row.
//
// Row rect for note `n` (0-based row height `row_h = (max.y - min.y) /
// (high_note - low_note + 1)`):
//   y_top    = min.y + (high_note - n) * row_h
//   y_bottom = y_top + row_h
//   x range  = [min.x, max.x]
//
// Black keys (`n % 12` in {1, 3, 6, 8, 10} -- C#, D#, F#, G#, A#) draw a
// darker fill; white keys (every other `n % 12`) draw a lighter fill. A
// thin 1px divider line separates every adjacent row. `low_note > high_note`
// is a defensive no-op (draws nothing) -- callers are expected to always
// pass a valid, non-empty window.
void draw_piano_key_gutter(ImDrawList* dl, const ImVec2& min, const ImVec2& max, int low_note,
                           int high_note);

}  // namespace sonotron
