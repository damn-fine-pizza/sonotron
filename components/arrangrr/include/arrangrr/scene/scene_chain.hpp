#pragma once

#include <cstdint>

#include "arrangrr/common/function_ref.hpp"
#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/config.hpp"
#include "common/time.hpp"  // TimeSig (Phase 7, node T0)

// SceneChain: node 8100 ("Scenes / song mode" -- docs/reflections/
// phase7-scope-6000-8100-clip-timeline-seam.md, §2 Option C, §3 "8100").
// Engine-owned peer of ChordSequencer, NOT wired through ClipMatrix (Fork B,
// RESOLVED = own transport) -- distinct from the GUI's own grid-column
// "Scene" (kSceneQuantize/kSceneColumn), performance.hpp's own header
// tripwire already draws that line.
//
// A SceneStep references a PerformanceStore slot INDEX, never an embedded
// Performance copy (§2/§4): SceneChain itself never touches PerformanceStore/
// Transport (the same scope discipline ChordSequencer/ClipMatrix/LoopBuffer
// already observe with respect to Arranger/PerformanceStore) -- the caller
// (Engine::fire_scene/scene_play) is the ONE place that translates a
// transition into Engine::apply_performance(...) + Transport::set_time_sig(...).
//
// Timebase discipline: unlike ChordSequencer/LoopBuffer (which track an
// absolute Tick position and are driven every transport tick), SceneChain
// advances on a per-BAR counter, incremented by ONE call to on_bar() per bar
// boundary (Engine::on_tick's existing `tick % ticks_per_bar == 0` gate,
// sibling to apply_pending_performance_recall) -- this sidesteps needing to
// reconcile an absolute tick position across a chain whose own steps change
// ticks_per_bar mid-song (T0): a bar is a bar, counted one at a time,
// regardless of how long it is in ticks.
//
// A chain is a single LINEAR song (no implicit loop back to step 0 at the
// end) -- the last scene simply holds once the chain stops advancing;
// looping the whole song is a host-level "chain-play again" gesture, not a
// SceneChain-internal concept, matching 8100's "song mode" framing.

namespace arrangrr {

// How a scene transition itself is meant to land (Fork B's own driver applies
// the Performance + TimeSig either way; `transition` is metadata a future
// crossfade/host-visual feature can read). Only kCut is implemented today: a
// hard switch, Performance + TimeSig land exactly on the transition bar.
enum class SceneTransitionKind : std::uint8_t {
  kCut = 0,
};

// One entry in the chain. Field order is widest-first (TimeSig, alignof 4,
// then the two u8/u16 tail fields) so sizeof(SceneStep) == 12 with no padding
// -- matches LoopEvent/ChordStep's own 12 B chord-relative-payload precedent.
struct SceneStep {
  TimeSig time_sig{};                  // per-scene meter (T0); defaults to 4/4 (byte-identity)
  std::uint16_t performance_slot = 0;  // PerformanceStore slot index (never an embedded copy)
  std::uint8_t n_bars = 1;             // bars this scene holds before the chain advances
  SceneTransitionKind transition = SceneTransitionKind::kCut;
};
static_assert(sizeof(SceneStep) == 12,
              "SceneStep: matches LoopEvent/ChordStep's own 12 B precedent (D33)");

class SceneChain {
 public:
  // Fired at every scene TRANSITION, including the synchronous "step 0" fire
  // at play() time (Runtime::advance_ticks only ever calls Engine::on_tick
  // for FUTURE ticks, so step 0 would otherwise never be applied -- mirrors
  // LoopBuffer/ChordSequencer's own immediate-launch-fires-synchronously
  // precedent). The caller applies the step; SceneChain never does.
  using TransitionFn = FunctionRef<void(std::size_t step_index, const SceneStep& step)>;

  // ---- chain authoring ---------------------------------------------------
  [[nodiscard]] bool add_scene(const SceneStep& step) noexcept { return m_steps.push_back(step); }
  void clear() noexcept {
    m_steps.clear();
    m_playing = false;
    m_index = 0;
    m_bars_elapsed = 0;
  }
  std::size_t count() const noexcept { return m_steps.size(); }
  const SceneStep* get(std::size_t idx) const noexcept {
    return idx < m_steps.size() ? &m_steps[idx] : nullptr;
  }

  // ---- transport (Fork B: own transport, not ClipMatrix) -----------------
  bool playing() const noexcept { return m_playing; }
  std::size_t current_index() const noexcept { return m_index; }

  // Starts the chain from step 0. Fires SYNCHRONOUSLY (see TransitionFn's own
  // comment above) so the caller's apply_performance/set_time_sig land on the
  // SAME tick the play command was issued. False (no-op) on an empty chain.
  bool play(TransitionFn fire) noexcept {
    if (m_steps.empty()) {
      return false;
    }
    m_playing = true;
    m_index = 0;
    m_bars_elapsed = 0;
    fire(m_index, m_steps[m_index]);
    return true;
  }
  // Stops advancing; does not reset the chain's own content or position (a
  // subsequent play() always restarts from step 0, mirroring ChordSequencer's
  // own play() rebase-to-tick-0 discipline).
  void stop() noexcept { m_playing = false; }

  // Call once per BAR boundary while the chain is playing (Engine::fire_scene,
  // called from on_tick's existing bar-boundary gate). Advances the current
  // step's own bar counter; once it reaches the step's n_bars, moves to the
  // next step and fires its transition. A chain that reaches its last step
  // simply stops advancing (no implicit loop, see this file's own header
  // comment) -- the last scene's Performance/TimeSig stay in effect.
  void on_bar(TransitionFn fire) {
    if (!m_playing) {
      return;
    }
    const SceneStep& cur = m_steps[m_index];
    const std::uint8_t hold_bars = cur.n_bars == 0 ? std::uint8_t{1} : cur.n_bars;
    ++m_bars_elapsed;
    if (m_bars_elapsed < hold_bars) {
      return;
    }
    m_bars_elapsed = 0;
    if (m_index + 1 >= m_steps.size()) {
      m_playing = false;  // chain ended: holds the last scene, stops advancing
      return;
    }
    ++m_index;
    fire(m_index, m_steps[m_index]);
  }

 private:
  StaticVector<SceneStep, kMaxScenes> m_steps;
  bool m_playing = false;
  std::size_t m_index = 0;
  std::uint32_t m_bars_elapsed = 0;
};

}  // namespace arrangrr
