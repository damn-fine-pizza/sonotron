// Repeat-count Phase-2 (node 8100, docs/proposals/repeat-count-phase2-abi.md
// §3.3/§4): pure, Engine-free UNIT tests for SceneChain::on_bar's own
// lap-cycling semantics -- SceneStep::repeat_count, SceneChain::LapFn, and
// the new on_bar(TransitionFn, LapFn) signature. Subject is the SINGLE class
// SceneChain, direct instantiation, no Engine/ABI involved at all (mirrors
// test_function_ref.cpp's own pure-unit precedent) -> unit, per this
// directory's own three-metric doctrine (CMakeLists.txt lines 1-17). The
// cross-producer, ABI-level proof (the real kSceneLap OutEvent, the
// no-transition-refire regression guard, and the cmd.idx clamp behavior)
// lives in the sibling functional test, test_scene_repeat_count.cpp.

#include "arrangrr/scene/scene_chain.hpp"

#include "test.hpp"

namespace {

using namespace arrangrr;

// (a) Byte-identity/regression guard against the new lap machinery: with
// repeat_count left at its default (1), TransitionFn fires on every step
// completion exactly as before, and lap_fire is NEVER invoked across a full
// multi-step chain traversal (including the chain-ends-with-no-fire() case
// for the last step, unchanged from pre-Phase-2 behavior).
void test_scene_repeat_default_is_transition_every_bar_no_laps() {
  SceneChain chain;
  SceneStep step;
  step.n_bars = 1;
  CHECK(chain.add_scene(step));
  CHECK(chain.add_scene(step));
  CHECK(chain.add_scene(step));

  int transition_calls = 0;
  int lap_calls = 0;
  auto fire = [&](std::size_t, const SceneStep&) { ++transition_calls; };
  auto lap = [&](std::size_t, std::uint8_t, std::uint8_t) { ++lap_calls; };

  CHECK(chain.play(fire));  // synchronous step-0 fire
  CHECK(transition_calls == 1);
  CHECK(chain.current_index() == 0);

  chain.on_bar(fire, lap);  // step 0 -> step 1
  CHECK(transition_calls == 2);
  CHECK(chain.current_index() == 1);

  chain.on_bar(fire, lap);  // step 1 -> step 2
  CHECK(transition_calls == 3);
  CHECK(chain.current_index() == 2);

  chain.on_bar(fire, lap);  // step 2 is last: chain ends, no fire() (pre-existing rule)
  CHECK(transition_calls == 3);
  CHECK(!chain.playing());

  CHECK(lap_calls == 0);  // lap_fire never invoked anywhere in this traversal
}

// (b) A 2-step chain, step 0 holds 3 laps of its own 1-bar length before the
// chain genuinely advances: the first K-1 on_bar() calls each fire lap_fire
// (never TransitionFn, current_index() unchanged); the Kth call fires
// TransitionFn (a genuine advance, never lap_fire).
void test_scene_finite_repeat_holds_k_laps_then_advances() {
  SceneChain chain;
  SceneStep step0;
  step0.n_bars = 1;
  step0.repeat_count = 3;
  SceneStep step1;
  step1.n_bars = 1;
  CHECK(chain.add_scene(step0));
  CHECK(chain.add_scene(step1));

  int transition_calls = 0;
  struct LapRecord {
    std::size_t step_index;
    std::uint8_t lap;
    std::uint8_t repeat_count;
  };
  LapRecord laps[4]{};
  int lap_count = 0;

  auto fire = [&](std::size_t, const SceneStep&) { ++transition_calls; };
  auto lap = [&](std::size_t step_index, std::uint8_t l, std::uint8_t rc) {
    laps[lap_count++] = LapRecord{.step_index = step_index, .lap = l, .repeat_count = rc};
  };

  CHECK(chain.play(fire));
  CHECK(transition_calls == 1);

  chain.on_bar(fire, lap);  // lap 1 of 3
  CHECK(transition_calls == 1);
  CHECK(chain.current_index() == 0);
  CHECK(lap_count == 1);
  CHECK(laps[0].step_index == 0);
  CHECK(laps[0].lap == 1);
  CHECK(laps[0].repeat_count == 3);

  chain.on_bar(fire, lap);  // lap 2 of 3
  CHECK(transition_calls == 1);
  CHECK(chain.current_index() == 0);
  CHECK(lap_count == 2);
  CHECK(laps[1].step_index == 0);
  CHECK(laps[1].lap == 2);
  CHECK(laps[1].repeat_count == 3);

  chain.on_bar(fire, lap);  // lap 3 (the FINAL lap): a genuine advance
  CHECK(transition_calls == 2);
  CHECK(chain.current_index() == 1);
  CHECK(lap_count == 2);  // unchanged: no lap_fire on the final completion
}

// (c) The hold-last-step-forever-ish interaction: a single-step chain whose
// only (and therefore last) step holds 2 laps before the chain ends. After
// the first on_bar() call: one lap fired, playing() still true, no fire()
// call. After the second: playing() becomes false (chain ended), and fire()
// is STILL never called -- mirrors the pre-existing "chain ended, no fire()"
// behavior for the last step, now gated behind the repeat count.
void test_scene_repeat_count_on_last_step_holds_then_chain_ends() {
  SceneChain chain;
  SceneStep step;
  step.n_bars = 1;
  step.repeat_count = 2;
  CHECK(chain.add_scene(step));

  int transition_calls = 0;
  int lap_calls = 0;
  std::uint8_t last_lap = 0;
  auto fire = [&](std::size_t, const SceneStep&) { ++transition_calls; };
  auto lap = [&](std::size_t step_index, std::uint8_t l, std::uint8_t rc) {
    ++lap_calls;
    last_lap = l;
    CHECK(step_index == 0);
    CHECK(rc == 2);
  };

  CHECK(chain.play(fire));  // synchronous step-0 fire
  CHECK(transition_calls == 1);

  chain.on_bar(fire, lap);  // first call: lap 1 of 2
  CHECK(lap_calls == 1);
  CHECK(last_lap == 1);
  CHECK(chain.playing());
  CHECK(transition_calls == 1);  // unchanged for this call

  chain.on_bar(fire, lap);  // second call: the final lap -> chain ends, no fire()
  CHECK(!chain.playing());
  CHECK(transition_calls == 1);  // still unchanged: chain-end never calls fire()
  CHECK(lap_calls == 1);         // no lap_fire on the final, chain-ending completion
}

// (d) A step (not necessarily the last one in the chain) with
// repeat_count == kSceneRepeatInfinite never reaches a final lap: every
// completed hold fires lap_fire with a monotonically increasing lap number
// and repeat_count echoed as 255; TransitionFn is never called;
// current_index() never changes; playing() stays true throughout.
void test_scene_infinite_repeat_never_advances() {
  SceneChain chain;
  SceneStep step0;
  step0.n_bars = 1;
  step0.repeat_count = kSceneRepeatInfinite;
  SceneStep step1;  // present to prove step0 is NOT merely "the last step"
  step1.n_bars = 1;
  CHECK(chain.add_scene(step0));
  CHECK(chain.add_scene(step1));

  int transition_calls = 0;
  int lap_calls = 0;
  std::uint8_t last_lap = 0;
  auto fire = [&](std::size_t, const SceneStep&) { ++transition_calls; };
  auto lap = [&](std::size_t step_index, std::uint8_t l, std::uint8_t rc) {
    ++lap_calls;
    CHECK(step_index == 0);
    CHECK(rc == kSceneRepeatInfinite);
    CHECK(l == static_cast<std::uint8_t>(last_lap + 1));  // monotonically increasing, 1-based
    last_lap = l;
  };

  CHECK(chain.play(fire));
  CHECK(transition_calls == 1);

  for (int i = 0; i < 8; ++i) {
    chain.on_bar(fire, lap);
    CHECK(chain.current_index() == 0);
    CHECK(chain.playing());
    CHECK(transition_calls == 1);  // never advances
  }
  CHECK(lap_calls == 8);
  CHECK(last_lap == 8);
}

// (e) Defensive floor: repeat_count == 0 (should never occur via the ABI's
// own clamp) behaves exactly like repeat_count == 1 -- an immediate genuine
// advance on the first on_bar() call, no lap ever fired. Mirrors the file's
// own pre-existing n_bars == 0 -> treated-as-1 defensive one-liner.
void test_scene_repeat_count_zero_is_treated_as_one() {
  SceneChain chain;
  SceneStep step0;
  step0.n_bars = 1;
  step0.repeat_count = 0;
  SceneStep step1;
  step1.n_bars = 1;
  CHECK(chain.add_scene(step0));
  CHECK(chain.add_scene(step1));

  int transition_calls = 0;
  int lap_calls = 0;
  auto fire = [&](std::size_t, const SceneStep&) { ++transition_calls; };
  auto lap = [&](std::size_t, std::uint8_t, std::uint8_t) { ++lap_calls; };

  CHECK(chain.play(fire));
  CHECK(transition_calls == 1);

  chain.on_bar(fire, lap);  // repeat_count==0 treated as 1: immediate genuine advance
  CHECK(transition_calls == 2);
  CHECK(chain.current_index() == 1);
  CHECK(lap_calls == 0);  // no lap ever fired
}

}  // namespace

int main() {
  test_scene_repeat_default_is_transition_every_bar_no_laps();
  test_scene_finite_repeat_holds_k_laps_then_advances();
  test_scene_repeat_count_on_last_step_holds_then_chain_ends();
  test_scene_infinite_repeat_never_advances();
  test_scene_repeat_count_zero_is_treated_as_one();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_scene_repeat_cycling: all OK\n");
  }
  return arrangrr::test::failures();
}
