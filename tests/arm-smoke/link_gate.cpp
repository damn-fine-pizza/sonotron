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

#include "arrangrr/engine.hpp"
#include "runtime/runtime.hpp"

namespace arrangrr {

// Referenced from the firmware stub so the linker resolves the whole
// runtime + arranger stage.
bool engine_link_gate() {
  // static: the full state lives in .bss, not stack.
  static runtime::Runtime<Engine, kSchedulerCapacity> rt;
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

}  // namespace arrangrr
