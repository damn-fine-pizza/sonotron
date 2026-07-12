// Bare firmware stub: the M0 gate is that the core cross-compiles and links
// freestanding for the STM32 anchor (D31/D33). Real board bring-up is M13.

#include "arrangrr/version.hpp"
#include "common/time.hpp"

namespace arrangrr {
bool engine_link_gate();    // link_gate.cpp: instantiates the full Engine alone
bool pipeline_link_gate();  // link_gate.cpp: the real 2-stage [chorddet, arrangrr] pipeline
}  // namespace arrangrr

int main() {
  arrangrr::TickAccumulator acc;
  acc.set_bpm(arrangrr::kDefaultBpm);
  // Pull the WHOLE core through the freestanding linker, not just common/:
  // the engine gate instantiates transport, parser, router, scheduler,
  // chord engine, sequencer, arranger and timeline (D20). The pipeline gate
  // additionally proves the real 2-stage `Pipeline<ChorddetStage<N>, Engine>`
  // composite production drives cross-builds and links freestanding too
  // (docs/design/orchestrator-pipeline-extraction.md §16.7, Phase 4d).
  const bool engine_ok = arrangrr::engine_link_gate();
  const bool pipeline_ok = arrangrr::pipeline_link_gate();
  return (engine_ok && pipeline_ok && arrangrr::version_string() != nullptr &&
          acc.advance_us(1'000'000) == 1920)
             ? 0
             : 1;
}
