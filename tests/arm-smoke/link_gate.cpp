// Freestanding gate for the PRODUCT, not just the primitives (D20: "a
// feature enters the core only if CI compiles it on both targets"): this TU
// instantiates the full runtime kernel + the arranger stage together —
// transport, parser, router, scheduler, chord engine, sequencer, arranger,
// timeline — and exercises the ABI entry points, so the arm cross-build
// certifies real object code for all of it.
//
// Phase-1 runtime extraction: relocated from
// components/arrangrr/src/engine_checks.cpp (which instantiated a bare
// Engine). Now that Transport/OutScheduler moved to components/runtime, this
// TU must link BOTH components together — a dependency `arrangrr`-the-library
// must never carry itself (docs/design/runtime-extraction-phase1-move-plan.md
// §6), hence its home is here, not back inside libarrangrr.a.
//
// Phase-4d (docs/design/orchestrator-pipeline-extraction.md §16.1/§16.6,
// phase4-execution-plan.md 4d): `Engine` now needs a `FollowedContext&` AND a
// `ChorddetStage&` (both Pipeline-owned in production); `engine_link_gate`
// keeps proving `Engine` ALONE cross-builds freestanding, owning its own
// externally-supplied FollowedContext + ChorddetStage instances (both
// dual-target/freestanding components in their own right, §16.1/§16.6).
// `pipeline_link_gate` is the NEW, additive proof (§16.7's arm-smoke test
// strategy: "gains a [chorddet]->[arrangrr] two-stage freestanding link-gate
// proof, additive to, not a replacement of, the existing single-stage
// smoke") that the REAL two-stage `Pipeline<ChorddetStage<N>, Engine>`
// composite -- the exact shape `hostrt::Shell` drives -- also cross-builds
// and links freestanding.

#include "arrangrr/engine.hpp"
#include "runtime/pipeline.hpp"
#include "runtime/runtime.hpp"

namespace arrangrr {

// Referenced from the firmware stub so the linker resolves the whole
// runtime + arranger stage.
bool engine_link_gate() {
  // static: the full state lives in .bss, not stack.
  static FollowedContext followed;
  static ChorddetStage<kMaxPorts> chorddet(followed);
  static runtime::Runtime<Engine, kSchedulerCapacity> rt(followed, chorddet);
  int events = 0;
  const Engine::EventSink sink = [&events](const OutEvent&) { ++events; };

  Command key;
  key.op = Op::kSet;
  key.param = Param::kKeySet;
  rt.push_command(key, sink);

  Command play;
  play.param = Param::kChordPlay;
  play.a = 62;
  play.b = -1;
  play.c = 100;
  rt.push_command(play, sink);

  const std::uint8_t bytes[] = {0x90, 60, 100};
  rt.stage().push_midi_in(0, Span<const std::uint8_t>(bytes), sink);

  Command start;
  start.param = Param::kTransportStart;
  rt.push_command(start, sink);
  rt.advance_ticks(kTicksPerBar, sink);

  return events > 0;
}

// Phase-4d: the real two-stage `[chorddet, arrangrr]` composite (the same
// shape production drives) cross-builds and links freestanding too --
// `push_midi_in` here exercises Pipeline's Seam-C fan-out (pipeline.hpp) all
// the way through the firmware toolchain, not just on host.
bool pipeline_link_gate() {
  static FollowedContext followed;
  static runtime::Runtime<runtime::Pipeline<ChorddetStage<kMaxPorts>, Engine>, kSchedulerCapacity>
      rt([](auto&, auto&) { return ChorddetStage<kMaxPorts>(followed); },
         [](auto& sched, auto& transport, auto& chorddet) {
           return Engine(sched, transport, followed, chorddet);
         });
  int events = 0;
  const Engine::EventSink sink = [&events](const OutEvent&) { ++events; };

  Command detect;
  detect.op = Op::kSet;
  detect.param = Param::kChordDetect;
  detect.a = 1;
  detect.b = 0;
  rt.push_command(detect, sink);

  const std::uint8_t bytes[] = {0x90, 60, 100};
  rt.stage().push_midi_in(0, Span<const std::uint8_t>(bytes), sink);

  Command start;
  start.param = Param::kTransportStart;
  rt.push_command(start, sink);
  rt.advance_ticks(kTicksPerBar, sink);

  return events > 0;
}

}  // namespace arrangrr
