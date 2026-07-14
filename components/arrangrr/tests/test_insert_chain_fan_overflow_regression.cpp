// Torquato QA regression lock (Phase-5 Item #10, MIDI-FX insert chain, node
// 5100/5200): InsertChain::apply's overflow guard, for a chain stage whose
// COMBINED fan-out across MULTIPLE pending input notes (an earlier stage's
// own fan-out) exceeds kMaxChainFan.
//
// FIXED by Nazzareno: the guard used to be an unconditionally-true assert
// condition (ARR_ASSERT(next_count < kMaxChainFan) reached only when
// next_count == kMaxChainFan already, i.e. an unconditional trap/SIGILL --
// this file originally pinned that crash and was EXPECTED to abort the
// process). It now breaks out of the inner accumulation loop instead and
// silently TRUNCATES, matching apply()'s own documented "bounded, no UB"
// contract. This file is now a NORMAL anti-regression lock (CHECK-based,
// green), proving the bounded/no-crash/no-OOB contract instead of pinning
// a crash. Kept in its own minimal target (mirrors
// test_performance_style_id_regression.cpp's isolation precedent) so a
// future re-break of this specific guard is caught in isolation from
// test_insert_chain.cpp's unrelated coverage.
//
// Scenario (unchanged from the original crash pin, verified against the
// ACTUAL fixed output, not assumed): slot 0 = Echo(repeats=1, delay_ticks=1)
// on a single input note produces 2 pending notes (offsets 0 and 1). slot 1
// = Echo(repeats=7, delay_ticks=1) would want up to 8 notes PER pending
// note (16 total) -- for the FIRST pending note it fills all 8 slots
// exactly (offsets 0..7, no decay since vel_decay == 255); for the SECOND
// pending note, `room` is already 0 before a single note of it is
// processed, so that entire second note's contribution is dropped (the
// truncation drops the whole tail pending note, not a partial slice of
// it -- confirmed by inspecting the actual output below, not assumed from
// reading the source).

#include "arrangrr/fx/insert_chain.hpp"

#include "test.hpp"

using namespace arrangrr;

int main() {
  InsertChain chain;
  chain.set_type(0, InsertType::kEcho);
  chain.set_param(0, /*repeats=*/0, 1);
  chain.set_param(0, /*vel_decay=*/1, 255);
  chain.set_param(0, /*delay_ticks=*/2, 1);

  chain.set_type(1, InsertType::kEcho);
  chain.set_param(1, /*repeats=*/0, 7);
  chain.set_param(1, /*vel_decay=*/1, 255);
  chain.set_param(1, /*delay_ticks=*/2, 1);

  const FxNote in{.note = 60, .vel = 100, .gate = 100, .offset = 0};
  FxNote out[kMaxChainFan];
  const int n = chain.apply(in, out, kMaxChainFan, FxContext{});

  // Bounded: the combined 16-note demand truncates to EXACTLY the fan cap,
  // never more (this is the core "no UB, bounded" contract) and never less
  // (the first pending note's own fan-out fits exactly, so nothing is lost
  // beneath the cap).
  CHECK(n == kMaxChainFan);
  // No OOB: the returned count never exceeds the caller-supplied max_out
  // (here max_out == kMaxChainFan, so this restates the bound above from
  // the caller's own contract -- the return value is the ONLY thing a
  // caller can use to know how much of `out` was written, so this is the
  // load-bearing no-overrun guarantee).
  CHECK(n <= kMaxChainFan);

  // Front-of-fan survivors: the WHOLE first pending note's 8-note expansion
  // (offsets 0..7 at delay_ticks == 1, vel_decay == 255 == no decay) fills
  // the entire result; the second pending note (which would have wanted its
  // own offsets 1..8) is dropped in its entirety, not partially mixed in --
  // verified against the actual fixed output, not assumed.
  for (int i = 0; i < n; ++i) {
    CHECK(out[i].note == 60);
    CHECK(out[i].vel == 100);
    CHECK(out[i].gate == 100);
    CHECK(out[i].offset == i);
  }

  return arrangrr::test::failures();
}
