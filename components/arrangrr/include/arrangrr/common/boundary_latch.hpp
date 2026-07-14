#pragma once

#include <cstdint>

#include "common/time.hpp"  // Tick, kTicksPerBar

// BoundaryLatch: the ONE shared "arm now, fire when a boundary tick arrives"
// primitive (Phase-5 design review, docs/phase5-design-reviews.md "Pad/Scene
// live -> Performance" #9, Corelli fix #2). Before this, arm-at-boundary
// logic was hand-rolled independently in at least three places (Arranger's
// m_pending/m_pending_valid, ClipMatrix's kArmed/kQueuedStop LaunchState +
// per-clip n_bars window, FollowedContext's stage()/commit_bar() chord
// staging) -- introducing a Performance-recall pending state as a FOURTH
// bespoke copy was the thing to avoid.
//
// This item (#9) is the first NEW caller (Engine::m_perf_recall, engine.cpp's
// apply_pending_performance_recall/perf_recall). ClipMatrix/Arranger/
// ChordEngine are NOT retrofitted onto this type now -- that would risk
// perturbing their existing byte-identical goldens for no behavior change;
// they could adopt it later (left as a one-line note here, not a TODO in
// their own files, since it is a purely internal, non-committing observation).
//
// Header-only, POD, dual-target (no heap, no exceptions, freestanding-safe):
// works unchanged on host and arm-none-eabi.

namespace arrangrr {

struct BoundaryLatch {
  // The quantize window in ticks: due() fires every time `t % window == 0`
  // once armed, mirroring ClipMatrix::on_bar's own per-clip n_bars window
  // check. 0 = not armed (the sentinel due() also checks, so a
  // default-constructed latch is inert).
  Tick window = 0;
  bool pending = false;

  // Arms the latch for the next `n_bars`-bar boundary (n_bars < 1 clamped to
  // 1, mirroring ClipMatrix::arm's own clamp).
  void arm(std::uint8_t n_bars) noexcept {
    window = static_cast<Tick>(n_bars < 1 ? 1 : n_bars) * kTicksPerBar;
    pending = true;
  }
  // True exactly on the tick this latch's window closes, while armed.
  bool due(Tick t) const noexcept { return pending && window != 0 && t % window == 0; }
  // Consumes the arm (call once the caller has acted on a due() == true).
  void clear() noexcept { pending = false; }
};

}  // namespace arrangrr
