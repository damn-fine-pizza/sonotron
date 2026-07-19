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
// then the u8/u16 tail fields, `repeat_count` last) -- sizeof(SceneStep) ==
// 16 (13 B of live fields, compiler-padded to the next multiple of alignof
// 4). Repeat-count Phase-2 (docs/proposals/repeat-count-phase2-abi.md §2/§4)
// grew this from its original 12 B (the old LoopEvent/ChordStep-precedent
// size, now stale prose): the extra 3 B is new TRAILING padding, not a
// reshuffle of any existing field's offset -- SceneStep/SceneChain were born
// entirely inside the Phase-5-unfrozen window and never shipped (§1.2), so
// there is no v1 shape to preserve here, only this current one.
struct SceneStep {
  TimeSig time_sig{};                  // per-scene meter (T0); defaults to 4/4 (byte-identity)
  std::uint16_t performance_slot = 0;  // PerformanceStore slot index (never an embedded copy)
  std::uint8_t n_bars = 1;             // bars this scene holds before the chain advances
  SceneTransitionKind transition = SceneTransitionKind::kCut;
  // Repeat-count Phase-2: 1 (default) plays this step once before the chain
  // advances -- byte-identical to every step that existed before this field
  // was added. K in [2, 254] holds the step for K laps of n_bars bars each
  // before advancing. kSceneRepeatInfinite (255, below) holds the step
  // forever, mirroring GridModel::kSceneRepeatInfinite's own "one past the
  // max finite value" sentinel convention one layer up (host: max finite 8,
  // sentinel 9; core: max finite 254, sentinel 255 -- the u8 ceiling itself).
  // SceneChain::on_bar's own comment documents the exact cycling semantics.
  std::uint8_t repeat_count = 1;
};
static_assert(sizeof(SceneStep) == 16,
              "SceneStep: 13 B of live fields, compiler-padded to 16 (repeat-count Phase-2)");

// The sentinel `SceneStep::repeat_count` value meaning "hold this step
// forever" -- the u8 ceiling, one past the highest LEGAL finite repeat count
// (254). Mirrors GridModel::kSceneRepeatInfinite's own "one past max"
// convention (apps/gui-sonotron/src/grid_model.hpp) rather than inventing a
// new one core-side, per docs/proposals/repeat-count-phase2-abi.md §4.
inline constexpr std::uint8_t kSceneRepeatInfinite = 255;

class SceneChain {
 public:
  // Fired at every scene TRANSITION, including the synchronous "step 0" fire
  // at play() time (Runtime::advance_ticks only ever calls Engine::on_tick
  // for FUTURE ticks, so step 0 would otherwise never be applied -- mirrors
  // LoopBuffer/ChordSequencer's own immediate-launch-fires-synchronously
  // precedent). The caller applies the step; SceneChain never does.
  using TransitionFn = FunctionRef<void(std::size_t step_index, const SceneStep& step)>;

  // Repeat-count Phase-2 (docs/proposals/repeat-count-phase2-abi.md §3.3's
  // refire fork, RESOLVED = suppress): fired on every INTERMEDIATE repeat
  // lap -- same step, m_index unchanged, TransitionFn NOT fired for that same
  // on_bar() call -- so a repeat lap never re-triggers apply_scene_transition
  // (and therefore never re-triggers Arranger::request_scene's phase/anchor
  // logic, the exact stale-anchor bug class already on record for this
  // codebase). `lap` is 1-based; for a finite repeat_count it never reaches
  // repeat_count itself (the FINAL lap is a genuine advance, reported via
  // TransitionFn instead of this callback); for kSceneRepeatInfinite it
  // climbs without bound (the caller decides whether/how to saturate it for
  // display -- Engine::fire_scene saturates the wire payload at 255).
  using LapFn =
      FunctionRef<void(std::size_t step_index, std::uint8_t lap, std::uint8_t repeat_count)>;

  // ---- chain authoring ---------------------------------------------------
  [[nodiscard]] bool add_scene(const SceneStep& step) noexcept { return m_steps.push_back(step); }
  void clear() noexcept {
    m_steps.clear();
    m_playing = false;
    m_index = 0;
    m_bars_elapsed = 0;
    m_lap = 0;
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
    m_lap = 0;
    fire(m_index, m_steps[m_index]);
    return true;
  }
  // Stops advancing; does not reset the chain's own content or position (a
  // subsequent play() always restarts from step 0, mirroring ChordSequencer's
  // own play() rebase-to-tick-0 discipline).
  void stop() noexcept { m_playing = false; }

  // Call once per BAR boundary while the chain is playing (Engine::fire_scene,
  // called from on_tick's existing bar-boundary gate). Advances the current
  // step's own bar counter; once it reaches the step's n_bars, one LAP of the
  // step's repeat_count has completed (repeat-count Phase-2). A step with the
  // default repeat_count(1) always completes its own (and only) lap on the
  // FIRST such boundary, byte-identical to the pre-Phase-2 behavior: this
  // genuinely advances to the next step, firing its transition -- `lap_fire`
  // is never called. A step with repeat_count == K > 1 fires `lap_fire`
  // (never `fire`) on each of its first K-1 completed laps, then genuinely
  // advances (firing `fire`, never `lap_fire`) on the Kth. kSceneRepeatInfinite
  // never reaches a Kth lap: every completed lap fires `lap_fire` forever,
  // `fire` is never called, and the chain never advances past this step. A
  // chain that reaches its last step (genuinely, i.e. not mid-repeat) simply
  // stops advancing (no implicit loop, see this file's own header comment)
  // -- the last scene's Performance/TimeSig stay in effect, exactly as
  // before repeat-count Phase-2 (this holds even if the last step's own
  // repeat_count is > 1: its own repeat cycle completes first, THEN the
  // chain ends, mirroring the pre-existing "no fire() on chain end" shape).
  void on_bar(TransitionFn fire, LapFn lap_fire) {
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
    const std::uint8_t repeat_count = cur.repeat_count == 0 ? std::uint8_t{1} : cur.repeat_count;
    if (repeat_count == kSceneRepeatInfinite) {
      ++m_lap;
      lap_fire(m_index, saturate_lap(m_lap), repeat_count);
      return;  // never advances: the sentinel means "hold this step forever"
    }
    ++m_lap;
    if (m_lap < static_cast<std::uint32_t>(repeat_count)) {
      lap_fire(m_index, static_cast<std::uint8_t>(m_lap), repeat_count);
      return;  // an INTERMEDIATE lap: same step, no transition fired
    }
    m_lap = 0;  // the FINAL lap: a genuine advance, exactly like repeat_count == 1
    if (m_index + 1 >= m_steps.size()) {
      m_playing = false;  // chain ended: holds the last scene, stops advancing
      return;
    }
    ++m_index;
    fire(m_index, m_steps[m_index]);
  }

 private:
  // The wire lap payload (OutEvent::scene_lap's msg.status) is one byte; an
  // infinite-repeat step's own internal lap counter (m_lap, uint32_t so it
  // never wraps) can climb past 255 -- saturate the DISPLAY value there, a
  // cosmetic cap only (mirrors kBeat's own "caps at 65535 bars" precedent,
  // abi.hpp), never a behavior cap: the step keeps holding regardless.
  static std::uint8_t saturate_lap(std::uint32_t lap) noexcept {
    return lap > 255 ? std::uint8_t{255} : static_cast<std::uint8_t>(lap);
  }

  StaticVector<SceneStep, kMaxScenes> m_steps;
  bool m_playing = false;
  std::size_t m_index = 0;
  std::uint32_t m_bars_elapsed = 0;
  std::uint32_t m_lap = 0;  // laps completed so far on the CURRENT step (repeat-count Phase-2)
};

}  // namespace arrangrr
