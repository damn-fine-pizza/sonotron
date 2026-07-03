// Freestanding gate for the PRODUCT, not just the primitives (D20: "a
// feature enters the core only if CI compiles it on both targets"): this TU
// instantiates the full Engine — transport, parser, router, scheduler, chord
// engine, sequencer, arranger, timeline — and exercises the ABI entry points,
// so the arm cross-build certifies real object code for all of it.

#include "arrangrr/engine.hpp"

namespace arrangrr {

// Referenced from the firmware stub so the linker resolves the whole engine.
bool engine_link_gate() {
  static Engine engine;  // static: the full state lives in .bss, not stack
  int events = 0;
  const Engine::EventSink sink = [&events](const OutEvent&) { ++events; };

  Command key;
  key.op = Op::kSet;
  key.param = Param::kKeySet;
  engine.push_command(key, sink);

  Command play;
  play.param = Param::kChordPlay;
  play.a = 62;
  play.b = -1;
  play.c = 100;
  engine.push_command(play, sink);

  const std::uint8_t bytes[] = {0x90, 60, 100};
  engine.push_midi_in(0, Span<const std::uint8_t>(bytes), sink);

  Command start;
  start.param = Param::kTransportStart;
  engine.push_command(start, sink);
  engine.advance_ticks(kTicksPerBar, sink);

  return events > 0;
}

}  // namespace arrangrr
