// Bare firmware stub: the M0 gate is that the core cross-compiles and links
// freestanding for the STM32 anchor (D31/D33). Real board bring-up is M13.

#include "arrangrr/version.hpp"
#include "common/time.hpp"

namespace arrangrr {
bool engine_link_gate();    // link_gate.cpp: instantiates the full Engine alone
bool pipeline_link_gate();  // link_gate.cpp: the real 2-stage [chorddet, arrangrr] pipeline
bool restyle_link_gate();   // link_gate.cpp: the 3-stage [chorddet, restyle, arrangrr] pipeline
bool motif_link_gate();     // link_gate.cpp: the motif engine (9210) firing through Arranger alone
bool clip_link_gate();      // link_gate.cpp: the clip/launch primitive (Phase-5 Item #2)
}  // namespace arrangrr

int main() {
  arrangrr::TickAccumulator acc;
  acc.set_bpm(arrangrr::kDefaultBpm);
  // Pull the WHOLE core through the freestanding linker, not just common/:
  // the engine gate instantiates transport, parser, router, scheduler,
  // chord engine, sequencer, arranger and timeline (D20). The pipeline gate
  // additionally proves the real 2-stage `Pipeline<ChorddetStage<N>, Engine>`
  // composite production drives cross-builds and links freestanding too
  // (docs/design/orchestrator-pipeline-extraction.md §16.7, Phase 4d). The
  // restyle gate (roadmap 9320) additionally proves RestyleStage itself
  // cross-builds and links freestanding (docs/design/restyle-placement.md §4).
  // The motif gate (roadmap 9210) additionally proves the motif engine's
  // generator/transform actually fires notes through Arranger on the real
  // target (docs/design/motif-engine-placement.md §1/§3). The clip gate
  // (Phase-5 Item #2) additionally proves the clip/launch primitive actually
  // fires through Engine::fire_clips on the real target (docs/design/
  // clip-primitive-design.md).
  const bool engine_ok = arrangrr::engine_link_gate();
  const bool pipeline_ok = arrangrr::pipeline_link_gate();
  const bool restyle_ok = arrangrr::restyle_link_gate();
  const bool motif_ok = arrangrr::motif_link_gate();
  const bool clip_ok = arrangrr::clip_link_gate();
  return (engine_ok && pipeline_ok && restyle_ok && motif_ok && clip_ok &&
          arrangrr::version_string() != nullptr && acc.advance_us(1'000'000) == 1920)
             ? 0
             : 1;
}
