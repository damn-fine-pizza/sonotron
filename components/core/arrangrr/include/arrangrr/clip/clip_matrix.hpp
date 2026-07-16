#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/config.hpp"
#include "arrangrr/timeline/timeline.hpp"  // TrackRole

// ClipMatrix: the Repeat-Zone launch primitive (Phase-5 Item #2,
// docs/design/clip-primitive-design.md). A clip is an enum-tag + index
// REFERENCE into content Engine already owns (a style section, a chord
// sequence, or a step track) plus a runtime launch state and a pending
// quantize window -- never a pointer, never a variant.
//
// SCOPE TRIPWIRE (design decision 5): ClipMatrix owns ONLY {content ref,
// launch state, pending boundary}. NO record field on Clip, NO capture()
// method here -- that is node 6000 (the Looper) territory, out of scope.
//
// ClipMatrix does not know about Arranger/ChordSequencer/Timeline: it is
// pure bookkeeping. Engine (the one place that already owns all three) reads
// a fired clip's {kind, content_index} and drives the underlying subsystem
// itself -- see Engine::apply_clip_content/fire_clips/cmd_clip (engine.cpp).
// This mirrors ArpeggiatorEngine's own placement: an Engine-owned VALUE
// member (not a Pipeline peer), because Clip orchestrates subsystems Engine
// already owns and shares no raw cross-stage data.

namespace arrangrr {

enum class ContentKind : std::uint8_t {
  kStyleSection = 0,
  kChordSequence = 1,
  kStepTrack = 2,
  // Phase 7 (node 6000, the Looper): a captured LoopBuffer slot
  // (arrangrr/loop/loop_buffer.hpp), launched exactly like any other clip --
  // ClipMatrix itself still never touches LoopBuffer directly (same scope
  // tripwire above); Engine::apply_clip_content is the ONE place that drives
  // it, mirroring kChordSequence's own dispatch shape.
  kLoopBuffer = 3,
};

// A clip's launch state. kArmed/kQueuedStop are the transient "counting down
// to the quantize boundary" states; ClipMatrix::on_bar promotes them to
// kPlaying/kStopped when their window closes (Engine::fire_clips, called
// from the SAME bar-boundary gate that promotes a staged chord (Transport::
// at_bar_boundary(), engine.hpp's on_tick -- decision 2).
enum class LaunchState : std::uint8_t {
  kStopped = 0,
  kArmed = 1,
  kPlaying = 2,
  kQueuedStop = 3,
};

// One grid cell's content reference + runtime launch state. 12 bytes, POD.
struct Clip {
  TrackRole part_role = TrackRole::kDrums;
  std::uint8_t scene_index = 0;
  ContentKind kind = ContentKind::kStyleSection;
  std::uint16_t content_index = 0;
  LaunchState state = LaunchState::kStopped;
  // n_bars: how many bar boundaries to count from arm() before firing --
  // Corelli's per-clip/per-request generalization of "next bar" to "next N
  // bars" (decision 2). Meaningless once kStopped/kPlaying; always >= 1.
  std::uint8_t n_bars = 1;
  // Phase 7 (node 8100 hardening, Torquato QA F3): the Transport::
  // bar_index() value this clip is due at, frozen at arm() time -- a
  // discrete bar COUNT, never a tick window. The PRIOR fix (node T0,
  // test_clip_matrix_live_meter_change_regression.cpp) froze a TICK window
  // on the first on_bar() check since arm() to stop a LATER meter change
  // from retuning an already-counting-down clip -- but the window itself was
  // still an absolute-tick match, so a meter change that already happened
  // BEFORE arm() (the F3 finding: the clip is armed on some scene reached via
  // a prior meter change) left the frozen window's own remainder
  // unreachable forever, permanently stuck at kArmed/kQueuedStop. A bar
  // COUNT has no such failure mode: "due at the Nth upcoming bar, counted
  // from bar_index() now" stays correct regardless of how long each of
  // those N bars turns out to be in ticks -- see runtime/transport.hpp's own
  // Transport::bar_index() comment. Meaningless once kStopped/kPlaying.
  std::uint32_t due_bar_index = 0;
};
static_assert(sizeof(Clip) == 12);

class ClipMatrix {
 public:
  // Registers a new clip slot. Returns its id (the pool's next sequential
  // index, mirroring ChordSequencer::add_sequence's own convention), or -1
  // when the pool is full.
  int add(TrackRole role, std::uint8_t scene, ContentKind kind,
          std::uint16_t content_index) noexcept {
    Clip c;
    c.part_role = role;
    c.scene_index = scene;
    c.kind = kind;
    c.content_index = content_index;
    if (!m_clips.push_back(c)) {
      return -1;
    }
    const std::size_t id = m_clips.size() - 1;
    m_used[id] = true;
    return static_cast<int>(id);
  }

  // Registers a clip AT an explicit id (Repeat-Zone binding contract, Shape A:
  // docs/proposals/repeat-zone-real-contract.md §3/§8b decision 1) instead of
  // taking whatever add()'s own sequential counter would have assigned --
  // lets a caller that already knows a STABLE id (the GUI's own
  // cell_id(role,scene)) address it directly rather than counting
  // registrations itself. The underlying pool (StaticVector) only supports
  // contiguous append, so reaching an id past the current tail fills every
  // intervening, not-yet-claimed index with an inert placeholder Clip
  // (m_used stays false for those -- get()/mutable_get() below treat an
  // unclaimed placeholder exactly like "no clip here", so it is never
  // launchable and never matched by a scene fan-out). A LATER add_at() call
  // that targets one of those still-unclaimed placeholder slots (arriving
  // out of row-major order, e.g. a lower cell_id filled after a higher one)
  // succeeds normally without growing the pool again. Returns false when
  // `id` is out of bounds (>= kMaxClips) or already claimed -- either by a
  // prior add()/add_at() (explicit ids are a ONE-TIME registration, this
  // primitive stays append-only, no retarget) -- the caller (Engine::
  // clip_add) turns that into a kBadArgument warn.
  bool add_at(std::size_t id, TrackRole role, std::uint8_t scene, ContentKind kind,
              std::uint16_t content_index) noexcept {
    if (id >= kMaxClips || (id < m_clips.size() && m_used[id])) {
      return false;
    }
    while (m_clips.size() <= id) {
      if (!m_clips.push_back(Clip{})) {
        return false;  // pool exhausted before reaching `id`
      }
    }
    Clip c;
    c.part_role = role;
    c.scene_index = scene;
    c.kind = kind;
    c.content_index = content_index;
    m_clips[id] = c;
    m_used[id] = true;
    return true;
  }

  // Both accessors return nullptr for an id past the pool's tail AND for an
  // unclaimed placeholder slot left behind by add_at() (m_used gates it) --
  // a slot nothing ever explicitly registered is not a real clip, regardless
  // of whether the underlying StaticVector already physically holds it.
  const Clip* get(std::size_t id) const noexcept {
    return (id < m_clips.size() && m_used[id]) ? &m_clips[id] : nullptr;
  }
  std::size_t size() const noexcept { return m_clips.size(); }

  // Arms clip `id` toward `target` (kPlaying or kStopped), quantized to the
  // next `n_bars`-bar boundary, counted from Transport's OWN bar_index AT
  // ARM TIME (`bar_index_now`, Phase 7 node 8100 hardening -- ClipMatrix
  // holds no Transport&, so the caller threads it explicitly, mirroring
  // BoundaryLatch::arm's own parameter). Callers with Boundary::kImmediate
  // apply the musical effect directly and call force() instead -- this is
  // for kNextBar/kNextNBars only. Returns false for an unknown id.
  bool arm(std::size_t id, LaunchState target, std::uint8_t n_bars,
           std::uint32_t bar_index_now = 0) noexcept {
    Clip* c = mutable_get(id);
    if (c == nullptr) {
      return false;
    }
    c->state = target == LaunchState::kPlaying ? LaunchState::kArmed : LaunchState::kQueuedStop;
    const std::uint8_t bars = n_bars < 1 ? std::uint8_t{1} : n_bars;
    c->n_bars = bars;
    c->due_bar_index = bar_index_now + static_cast<std::uint32_t>(bars - 1);
    return true;
  }

  // Immediate transition (Boundary::kImmediate): the caller has already
  // driven the underlying content itself; this only updates bookkeeping.
  bool force(std::size_t id, LaunchState state) noexcept {
    Clip* c = mutable_get(id);
    if (c == nullptr) {
      return false;
    }
    c->state = state;
    return true;
  }

  // Called from Engine::on_tick's EXISTING bar-boundary gate (Transport::
  // at_bar_boundary()), BEFORE fire_arranger (decision #2). Promotes every
  // armed/queued-stop clip whose bar countdown closes NOW (kArmed ->
  // kPlaying, kQueuedStop -> kStopped) and invokes `on_due(id, clip)` -- with
  // the NEW state already applied -- so the caller can trigger the
  // underlying content and emit the wire event. `bar_index_now` (Phase 7 node
  // 8100 hardening) is Transport::bar_index() AT THIS BOUNDARY; ClipMatrix
  // holds no Transport&, so Engine threads it explicitly (defaults to 0 so
  // any not-yet-updated 1-arg caller keeps matching arm()'s own default).
  //
  // Phase 7 node 8100 hardening (Torquato QA F3, following the node T0 fix
  // in test_clip_matrix_live_meter_change_regression.cpp): each clip's due
  // point is now a bar COUNT (Clip::due_bar_index), frozen at arm() time
  // directly -- not a tick window resolved lazily on the first on_bar()
  // check, which was still an absolute-tick match and could permanently miss
  // its own boundary if the meter had already changed before arm() (see
  // Clip::due_bar_index's own comment). A bar count has no such failure
  // mode. Under a stable meter this reproduces the identical fire tick as
  // before (bar N since arm is bar N since arm, tick-window or bar-count
  // alike), so every byte-identical golden is unaffected.
  template <typename Fn>
  void on_bar(Fn&& on_due, std::uint32_t bar_index_now = 0) {
    for (std::size_t id = 0; id < m_clips.size(); ++id) {
      Clip& c = m_clips[id];
      if (c.state != LaunchState::kArmed && c.state != LaunchState::kQueuedStop) {
        continue;
      }
      if (bar_index_now != c.due_bar_index) {
        continue;
      }
      c.state = c.state == LaunchState::kArmed ? LaunchState::kPlaying : LaunchState::kStopped;
      on_due(id, c);
    }
  }

 private:
  // Same m_used gate as get() above (arm()/force() route through this, so an
  // unclaimed add_at() placeholder can never be armed/forced into a real
  // launch state either).
  Clip* mutable_get(std::size_t id) noexcept {
    return (id < m_clips.size() && m_used[id]) ? &m_clips[id] : nullptr;
  }

  StaticVector<Clip, kMaxClips> m_clips;
  // Per-id "has add()/add_at() actually claimed this slot" flag -- separate
  // from Clip itself (which stays the pinned 12-byte POD, no room for a
  // registration marker) so add_at()'s padding placeholders are distinguishable
  // from a real registration without growing Clip's own footprint.
  std::array<bool, kMaxClips> m_used{};
};

}  // namespace arrangrr
