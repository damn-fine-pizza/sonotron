// Repeat-count Phase-2 (node 8100, docs/proposals/repeat-count-phase2-abi.md
// §3.3/§4): the CORE-primitive proof, through the real Engine/ABI, that a
// single kSceneAdd step's own repeat-count operand (Command::idx) holds a
// scene for K*n_bars bars before Engine::apply_scene_transition's real,
// observable effect (here: the Performance's tempo) actually advances --
// mirroring test_song_mode_repeat_count_contract.cpp's own claim for the
// GUI-side host-expand path (apps/gui-sonotron/tests/), but proven here
// against ONE kSceneAdd step with repeat_count=K, never an expanded chain of
// K steps, no apply_song_build, no GUI. Also proves the wire kSceneLap event
// (code/msg.status/msg.d1 packing) and, most importantly, THE regression
// guard the refire-avoidance design decision exists to prevent: an
// intermediate repeat lap must never re-invoke apply_scene_transition (and
// therefore never re-trigger Arranger::request_scene's phase/anchor logic,
// the stale-anchor bug class already on record for this codebase) -- proven
// via the observable fact that every genuine Performance recall
// unconditionally emits exactly one kSection + one kTimeSig event
// (Engine::emit_performance_confirmation), so that pair's count must stay
// flat across every intermediate lap and only step at a GENUINE transition.
//
// Subject is Engine's own cross-producer wiring (cmd_scene/fire_scene/
// apply_scene_transition, plus the new scene_add cmd.idx operand) -> the
// same functional precedent as test_scene.cpp/test_scene_hardening.cpp.

#include "arrangrr/scene/scene_chain.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"
#include "test_harness.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 2048>;

struct Band {
  test::TestEngine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0,
           std::uint16_t idx = 0, Op op = Op::kDo, Boundary boundary = Boundary::kImmediate,
           std::uint8_t n_bars = 1) {
    Command command;
    command.op = op;
    command.boundary = boundary;
    command.param = p;
    command.idx = idx;
    command.n_bars = n_bars;
    command.a = a;
    command.b = b;
    command.c = c;
    e.push_command(command, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void advance(std::uint32_t n) {
    e.advance_ticks(n, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  // Extends test_scene.cpp's own add_scene helper (same a/b/c packing) with
  // the repeat-count Phase-2 operand: it rides Command::idx, which the
  // pre-Phase-2 helper never threaded through (this file's whole reason to
  // exist). repeat_count == 0 (the default here) means "omitted" -- the
  // engine clamps that to 1, exactly like every pre-existing add_scene call.
  void add_scene(std::uint16_t performance_slot, std::uint8_t n_bars,
                 std::uint8_t beats_per_bar = 0,
                 SceneTransitionKind transition = SceneTransitionKind::kCut,
                 std::uint16_t repeat_count = 0) {
    cmd(Param::kSceneAdd, performance_slot,
        static_cast<std::int32_t>(n_bars) | (static_cast<std::int32_t>(beats_per_bar) << 8),
        static_cast<std::int32_t>(transition), repeat_count);
  }
  int warns() const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kWarn) {
        ++n;
      }
    }
    return n;
  }
  // Count of every kSection or kTimeSig event so far -- the regression guard
  // for the refire-avoidance decision (see this file's own header comment):
  // both fire exactly once per GENUINE Performance recall
  // (emit_performance_confirmation's own unconditional pair), never on an
  // intermediate repeat lap.
  int section_or_timesig_count() const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kSection || o.kind == OutEvent::Kind::kTimeSig) {
        ++n;
      }
    }
    return n;
  }
  struct LapEvent {
    std::uint16_t code;
    std::uint8_t lap;
    std::uint8_t repeat_count;
  };
  // Every kSceneLap event so far, in emission order.
  StaticVector<LapEvent, 16> scene_laps() const {
    StaticVector<LapEvent, 16> out;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kSceneLap) {
        CHECK(out.push_back(LapEvent{o.code, o.msg.status, o.msg.d1}));
      }
    }
    return out;
  }
};

Performance perf_with_tempo(std::uint16_t tempo_x100) {
  Performance p;
  p.tempo_x100 = tempo_x100;
  return p;
}

// (a)+(b)+(c) combined, driven through the SAME run: a finite repeat_count=4
// step holds for 4*n_bars(1) bars before the chain's own observable effect
// (tempo) genuinely advances; each of the first 3 completed bars emits
// exactly one kSceneLap (code=0, lap=1..3, repeat_count=4) and NO additional
// kSection/kTimeSig event beyond the pair the synchronous step-0 fire already
// produced; the 4th (final) bar is the genuine advance, producing exactly one
// more kSection/kTimeSig pair and no further kSceneLap event.
void test_scene_finite_repeat_holds_k_bars_emits_laps_no_transition_refire() {
  Band b;
  CHECK(b.e.performances().store(0, perf_with_tempo(9000)));
  CHECK(b.e.performances().store(1, perf_with_tempo(15000)));
  constexpr std::uint8_t kRepeat = 4;
  b.add_scene(0, 1, 0, SceneTransitionKind::kCut, kRepeat);  // step 0: n_bars=1, repeat_count=4
  b.add_scene(1, 1);                                         // step 1: default repeat_count=1
  CHECK(b.e.scenes().get(0) != nullptr);
  CHECK(b.e.scenes().get(0)->repeat_count == kRepeat);
  CHECK(b.warns() == 0);

  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.cmd(Param::kScenePlay);
  CHECK(b.e.transport().bpm() == 9000);  // step 0's Performance applied synchronously
  CHECK(b.e.scenes().current_index() == 0);
  const int after_play = b.section_or_timesig_count();
  CHECK(after_play == 2);  // exactly one kSection + one kTimeSig, the genuine fire

  // (a): the first K-1 = 3 completed bars are intermediate LAPS -- the step's
  // own n_bars(1) elapses each time, but the chain does NOT genuinely
  // advance (tempo/current_index unchanged), and (c) no additional
  // kSection/kTimeSig fires.
  for (std::uint8_t lap = 1; lap < kRepeat; ++lap) {
    b.advance(kTicksPerBar);
    CHECK(b.e.scenes().current_index() == 0);
    CHECK(b.e.transport().bpm() == 9000);
    CHECK(b.section_or_timesig_count() == after_play);  // (c) the critical regression guard
  }

  // (b): exactly K-1 kSceneLap events, in order, on step 0, echoing repeat_count.
  const auto laps = b.scene_laps();
  CHECK(laps.size() == kRepeat - 1);
  for (std::size_t i = 0; i < laps.size(); ++i) {
    CHECK(laps[i].code == 0);
    CHECK(laps[i].lap == static_cast<std::uint8_t>(i + 1));
    CHECK(laps[i].repeat_count == kRepeat);
  }

  // The Kth (final) bar: the genuine advance.
  b.advance(kTicksPerBar);
  CHECK(b.e.scenes().current_index() == 1);
  CHECK(b.e.transport().bpm() == 15000);
  CHECK(b.section_or_timesig_count() == after_play + 2);  // one more genuine kSection+kTimeSig pair
  CHECK(b.scene_laps().size() == kRepeat - 1);            // unchanged: no lap_fire on the final lap
}

// (d) A raw kSceneAdd with the repeat-count operand OMITTED (idx == 0)
// behaves exactly as every pre-existing scene_add call in test_scene.cpp
// already does: repeat_count clamps to 1, single play, no lap event ever.
void test_scene_repeat_count_omitted_idx_defaults_to_one() {
  Band b;
  CHECK(b.e.performances().store(0, perf_with_tempo(9000)));
  CHECK(b.e.performances().store(1, perf_with_tempo(15000)));
  b.add_scene(0, 1);  // repeat-count operand omitted
  b.add_scene(1, 1);
  CHECK(b.e.scenes().get(0) != nullptr);
  CHECK(b.e.scenes().get(0)->repeat_count == 1);

  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.cmd(Param::kScenePlay);
  b.advance(
      kTicksPerBar);  // step 0's own single bar -> genuine advance, exactly like before Phase-2
  CHECK(b.e.scenes().current_index() == 1);
  CHECK(b.e.transport().bpm() == 15000);
  for (const OutEvent& o : b.ev) {
    CHECK(o.kind != OutEvent::Kind::kSceneLap);  // no lap event, ever
  }
}

// (e) A raw kSceneAdd with the repeat-count operand set ABOVE 255 clamps
// down to kSceneRepeatInfinite (255) -- proven here at the ABI/clamp level
// (test_scene_repeat_cycling.cpp already proves the never-advances behavior
// structurally against the pure SceneChain primitive): advancing several
// bars never reaches a genuine transition, and every elapsed bar emits a
// kSceneLap instead, with no kSection refire (mirrors (c)'s own guard).
void test_scene_repeat_count_idx_above_255_clamps_to_infinite() {
  Band b;
  CHECK(b.e.performances().store(0, perf_with_tempo(9000)));
  CHECK(b.e.performances().store(1, perf_with_tempo(15000)));
  b.add_scene(0, 1, 0, SceneTransitionKind::kCut, 1000);  // idx clamps: 1000 > 255
  b.add_scene(1, 1);
  CHECK(b.e.scenes().get(0) != nullptr);
  CHECK(b.e.scenes().get(0)->repeat_count == kSceneRepeatInfinite);

  b.cmd(Param::kTransportStart);
  b.cmd(Param::kScenePlay);
  b.ev.clear();
  constexpr int kBars = 8;
  b.advance(kBars * kTicksPerBar);  // plenty of bars: never reaches a genuine transition
  CHECK(b.e.scenes().current_index() == 0);
  CHECK(b.e.transport().bpm() == 9000);
  CHECK(b.e.scenes().playing());
  CHECK(b.section_or_timesig_count() == 0);  // no refire at all across this whole window
  CHECK(b.scene_laps().size() == kBars);     // one lap per elapsed bar, forever
}

}  // namespace

int main() {
  test_scene_finite_repeat_holds_k_bars_emits_laps_no_transition_refire();
  test_scene_repeat_count_omitted_idx_defaults_to_one();
  test_scene_repeat_count_idx_above_255_clamps_to_infinite();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_scene_repeat_count: all OK\n");
  }
  return arrangrr::test::failures();
}
