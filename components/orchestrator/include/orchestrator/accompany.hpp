#pragma once

#include "arrangrr/engine.hpp"
#include "chorddet/stage.hpp"
#include "midisrc/midi_source_stage.hpp"
#include "runtime/pipeline.hpp"

// Accompany (roadmap node 9310), docs/design/orchestrator-pipeline-
// extraction.md §16.3/§16.5/§16.8, phase4-execution-plan.md 4d: the concrete
// 3-stage pipeline `[MIDI-source] -> [chorddet] -> [arrangrr]`. This header
// is `components/orchestrator`'s first real content (it stops being an empty
// slot) -- it NAMES and composes the topology; it does not reimplement time
// or the D29 total order (that stays `runtime::OutScheduler`/`Runtime`).
//
// chorddet is ordered BEFORE arrangrr so its harmonic context is visible the
// SAME tick (D53) the melody-thru note that triggered it arrives; the
// MIDI-source stage comes first because it is the one stage that PRODUCES
// (via on_tick, replaying the loaded file) rather than reacting to inbound
// bytes -- Pipeline's own fixed declared order (pipeline.hpp) fires every
// stage's on_tick in this same sequence every tick.
//
// This is also the EXACT pipeline shape `hostrt::Shell` drives (both the
// interactive default AND the `midi-source load <path>` L1-verb-driven
// Accompany golden category ride the SAME type, docs/design/
// orchestrator-pipeline-extraction.md §16.7): the MIDI-source stage is
// constructed inert (nothing loaded) by default, so a pipeline that never
// loads a file stays byte-identical to a bare `[chorddet, arrangrr]` pair
// (midisrc::MidiSourceStage's own guarantee, §16.5's header comment).
// `components/orchestrator` is what NAMES this shared shape; it does not
// duplicate it per consumer.

namespace orchestrator {

template <std::size_t N>
using AccompanyPipeline =
    runtime::Pipeline<midisrc::MidiSourceStage<N>, arrangrr::ChorddetStage<arrangrr::kMaxPorts>,
                      arrangrr::Engine>;

// 0-based stage indices into `AccompanyPipeline` -- named here once so every
// consumer (hostrt::Shell, a future sonotron-server/GUI wiring) reaches a
// declared stage the same documented way instead of a bare magic number.
inline constexpr std::size_t kMidiSourceStageIndex = 0;
inline constexpr std::size_t kChorddetStageIndex = 1;
inline constexpr std::size_t kArrangrrStageIndex = 2;

}  // namespace orchestrator
