#pragma once

#include <cstddef>
#include <cstdint>

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/config.hpp"
#include "arrangrr/timeline/timeline.hpp"  // TrackRole
#include "common/time.hpp"                 // Tick, kTicksPerBar

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
};

// A clip's launch state. kArmed/kQueuedStop are the transient "counting down
// to the quantize boundary" states; ClipMatrix::on_bar promotes them to
// kPlaying/kStopped when their window closes (Engine::fire_clips, called
// from the SAME `tick % kTicksPerBar == 0` gate that promotes a staged
// chord, engine.hpp's on_tick -- decision 2).
enum class LaunchState : std::uint8_t {
  kStopped = 0,
  kArmed = 1,
  kPlaying = 2,
  kQueuedStop = 3,
};

// One grid cell's content reference + runtime launch state. 8 bytes, POD.
struct Clip {
  TrackRole part_role = TrackRole::kDrums;
  std::uint8_t scene_index = 0;
  ContentKind kind = ContentKind::kStyleSection;
  std::uint16_t content_index = 0;
  LaunchState state = LaunchState::kStopped;
  // Quantize window while armed/queued-stop: the clip is due when
  // `transport_tick % (n_bars * kTicksPerBar) == 0` -- Corelli's per-clip/
  // per-request generalization of "next bar" to "next N bars" (decision 2).
  // No new clock: the SAME Transport tick every other boundary check already
  // reuses. Meaningless once kStopped/kPlaying; always >= 1.
  std::uint8_t n_bars = 1;
};
static_assert(sizeof(Clip) == 8);

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
    return static_cast<int>(m_clips.size() - 1);
  }

  const Clip* get(std::size_t id) const noexcept {
    return id < m_clips.size() ? &m_clips[id] : nullptr;
  }
  std::size_t size() const noexcept { return m_clips.size(); }

  // Arms clip `id` toward `target` (kPlaying or kStopped), quantized to the
  // next `n_bars`-bar boundary. Callers with Boundary::kImmediate apply the
  // musical effect directly and call force() instead -- this is for
  // kNextBar/kNextNBars only. Returns false for an unknown id.
  bool arm(std::size_t id, LaunchState target, std::uint8_t n_bars) noexcept {
    Clip* c = mutable_get(id);
    if (c == nullptr) {
      return false;
    }
    c->state = target == LaunchState::kPlaying ? LaunchState::kArmed : LaunchState::kQueuedStop;
    c->n_bars = n_bars < 1 ? 1 : n_bars;
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

  // Called from Engine::on_tick's EXISTING `tick % ticks_per_bar == 0` block,
  // BEFORE fire_arranger (decision #2). Promotes every armed/queued-stop
  // clip whose quantize window closes on `transport_tick` (kArmed ->
  // kPlaying, kQueuedStop -> kStopped) and invokes `on_due(id, clip)` -- with
  // the NEW state already applied -- so the caller can trigger the
  // underlying content and emit the wire event. `ticks_per_bar` (Phase 7,
  // node T0) is the CURRENT bar length; ClipMatrix holds no Transport&, so
  // Engine threads it explicitly (defaults to the compile-time kTicksPerBar
  // so every pre-existing 2-arg caller, e.g. unit tests, keeps computing the
  // exact same window).
  template <typename Fn>
  void on_bar(Tick transport_tick, Fn&& on_due, Tick ticks_per_bar = kTicksPerBar) {
    for (std::size_t id = 0; id < m_clips.size(); ++id) {
      Clip& c = m_clips[id];
      if (c.state != LaunchState::kArmed && c.state != LaunchState::kQueuedStop) {
        continue;
      }
      const Tick window = static_cast<Tick>(c.n_bars) * ticks_per_bar;
      if (transport_tick % window != 0) {
        continue;
      }
      c.state = c.state == LaunchState::kArmed ? LaunchState::kPlaying : LaunchState::kStopped;
      on_due(id, c);
    }
  }

 private:
  Clip* mutable_get(std::size_t id) noexcept {
    return id < m_clips.size() ? &m_clips[id] : nullptr;
  }

  StaticVector<Clip, kMaxClips> m_clips;
};

}  // namespace arrangrr
