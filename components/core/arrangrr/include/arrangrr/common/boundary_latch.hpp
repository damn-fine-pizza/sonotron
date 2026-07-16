#pragma once

#include <cstdint>

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
// apply_pending_performance_recall/perf_recall).
//
// Phase 7 (node 8100 hardening, Torquato QA F1/F2): the ORIGINAL shape armed
// a fixed TICK WINDOW (`n_bars * ticks_per_bar`, frozen at arm() time) and
// fired on an ABSOLUTE-modulo match (`t % window == 0`). That is unsafe under
// a variable meter: the window is a fixed DURATION, but the match is an
// absolute-from-zero comparison against a boundary sequence whose own
// spacing can change between arm() and the moment it fires -- if the
// transition tick is not itself a multiple of the NEW window, the frozen
// remainder can never be produced again, and the latch never fires (see
// runtime/transport.hpp's own Transport::bar_index() comment for the full
// worked argument). Re-based on Transport::bar_index() instead: a "bar" is a
// discrete COUNT of boundaries the re-anchored Transport gate has closed,
// never a tick window, so counting bars-until-due is immune to any
// ticks_per_bar change in between by construction.
//
// Header-only, POD, dual-target (no heap, no exceptions, freestanding-safe):
// works unchanged on host and arm-none-eabi.

namespace arrangrr {

struct BoundaryLatch {
  // The Transport::bar_index() value this latch is due at, while armed. 0 is
  // a perfectly ordinary value here (NOT a sentinel, unlike the old `window`
  // field) -- `pending` alone gates whether due() can ever return true, so a
  // default-constructed (never armed) latch stays inert regardless of what
  // due_bar_index happens to read.
  std::uint32_t due_bar_index = 0;
  bool pending = false;

  // Arms the latch to fire at the (n_bars)-th upcoming bar boundary, counted
  // from Transport's OWN bar_index AT ARM TIME (n_bars < 1 clamps to 1,
  // mirroring ClipMatrix::arm's own clamp). `bar_index_now` is threaded
  // explicitly by callers that hold a live Transport (BoundaryLatch itself
  // holds none) -- defaults to 0 so a caller that never threads a live value
  // (tests, or any not-yet-updated call site) arms as if the transport were
  // fresh, matching this type's own pre-hardening default behavior.
  void arm(std::uint8_t n_bars, std::uint32_t bar_index_now = 0) noexcept {
    const std::uint8_t bars = n_bars < 1 ? std::uint8_t{1} : n_bars;
    due_bar_index = bar_index_now + static_cast<std::uint32_t>(bars - 1);
    pending = true;
  }
  // True exactly on the ONE bar index this latch's countdown closes, while
  // armed.
  bool due(std::uint32_t bar_index_now) const noexcept {
    return pending && bar_index_now == due_bar_index;
  }
  // Consumes the arm (call once the caller has acted on a due() == true).
  void clear() noexcept { pending = false; }
};

}  // namespace arrangrr
