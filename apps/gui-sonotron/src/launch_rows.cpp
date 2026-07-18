#include "launch_rows.hpp"

#include "preview.hpp"
#include "step_pattern_model.hpp"

namespace sonotron {

TrackCellPreview resolve_track_cell_preview(const GridModel& model, const SeqEditModel& seqedit,
                                            const GridRow& row, std::size_t s, const GridCell& cell,
                                            bool filled, int active_style) {
  TrackCellPreview result;
  if (!filled) {
    // Owner bug (caught by test_grid_cell_preview_vs_seqedit_ui_automation's
    // empty-cell pin, 2026-07-18): ClipPattern::pitch value-initializes to
    // 0, not the "-1 = no voice" sentinel every OTHER producer of this shape
    // (preview_for/preview_for_track below) explicitly fills before
    // returning. grid_panel.cpp's own draw_cell happens to gate the whole
    // pianoroll-preview draw call behind `filled` and never reaches this
    // pattern at all, but seqedit_panel.cpp's draw_piano_roll_lanes draws
    // EVERY visible lane's pattern unconditionally (an empty cell's lane is
    // blank BY CONTENT, not by a skipped draw call) -- an un-sentineled
    // zero pattern would therefore have painted a full grid of phantom
    // pitch-0 notes for every empty cell's lane. Filling -1 here makes this
    // function's own "blank" case genuinely blank for every caller, not
    // reliant on a caller-side gate that not every caller has.
    for (auto& slots : result.pattern.pitch) {
      slots.fill(-1);
    }
    return result;
  }
  if (cell.kind == GridCellKind::kStepTrack) {
    if (const StepPatternModel* track =
            seqedit.step_tracks().track(static_cast<std::size_t>(cell.step_track_index))) {
      const preview::PreviewPattern pp = preview::preview_for_track(*track);
      result.pattern = neon::clip_pattern_from_pitches(pp.pitch, pp.bars);
    }
    return result;
  }
  const auto section = static_cast<preview::Section>(model.scene_section(s));
  const preview::PreviewPattern pp = preview::preview_for(active_style, section, row.role_index);
  result.pattern = neon::clip_pattern_from_pitches(pp.pitch, pp.bars);
  result.approx = pp.approx;
  return result;
}

}  // namespace sonotron
