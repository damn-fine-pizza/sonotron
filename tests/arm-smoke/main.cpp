// Bare firmware stub: the M0 gate is that the core cross-compiles and links
// freestanding for the STM32 anchor (D31/D33). Real board bring-up is M13.

#include "common/time.hpp"
#include "arrangrr/version.hpp"

namespace arrangrr {
bool engine_link_gate();  // engine_checks.cpp: instantiates the full Engine
}

int main() {
  arrangrr::TickAccumulator acc;
  acc.set_bpm(arrangrr::kDefaultBpm);
  // Pull the WHOLE core through the freestanding linker, not just common/:
  // the engine gate instantiates transport, parser, router, scheduler,
  // chord engine, sequencer, arranger and timeline (D20).
  const bool engine_ok = arrangrr::engine_link_gate();
  return (engine_ok && arrangrr::version_string() != nullptr &&
          acc.advance_us(1'000'000) == 1920)
             ? 0
             : 1;
}
