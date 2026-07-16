#pragma once

#include <concepts>

#include "common/time.hpp"

// The STAGE port (docs/design/orchestrator-pipeline-extraction.md §3.6, §14.2):
// the fixed, declarative contract a `Runtime<StageT>` drives once per tick.
// Deliberately a concept/template, NOT a virtual base class (DESIGN.md:577 —
// "virtual only at the HAL boundaries"; a per-tick stage dispatch is the hot
// path, not a HAL boundary) and deliberately NOT naming any arrangrr type
// (Command/OutEvent/Param): naming those here would need arrangrr/abi.hpp,
// reopening the `runtime -> arrangrr` cycle Decision A (components/common)
// exists to kill. `SinkT` stays a free template parameter, deduced at each
// call site (arrangrr::Engine::EventSink today) — this header never includes
// anything arrangrr-specific.

namespace runtime {

// Per-tick context `Runtime` hands to the stage it drives. Phase-1 shape,
// deliberately narrower than the illustrative StageContext in the design
// doc's §3.6/§14.2: it carries ONLY the stream tick. The design doc's fuller
// sketch (transport_tick/transport_playing/is_clock_pulse/transport_state/
// transport_position, all precomputed so "the Stage never reaches into
// Transport") assumed the Stage holds no live Transport access at all. This
// Phase-1 implementation instead injects `arrangrr::Transport&` into the
// arrangrr Stage-adapter by reference (the SAME "ownership does not forbid
// calling a method on the shared instance" precedent §14.3 already sanctions
// for the scheduler) because arrangrr's OWN command dispatch (`kSeqPlay`,
// `kStyleSection`, `style_switch`, `apply_style_tempo`) reads/writes Transport
// from OUTSIDE the on_tick path too, a gap the addendum's on_tick-only sketch
// did not cover. See runtime-extraction-phase1-move-plan discussion in the
// implementor's report for the full rationale; flagged as a Phase-1
// simplification to revisit if/when a second, non-Transport-aware stage
// needs the wider StageContext shape.
struct StageContext {
  arrangrr::Tick now;  // stream tick — always advances, injected every call
};

// `StageT` must be tick-callable (advance one stream tick, react if the
// transport happens to be playing) and flush-able (drain its due-event
// queue) against SOME sink callable type `SinkT` — the sink's concrete type
// is never named here.
template <typename StageT, typename SinkT>
concept StageLike = requires(StageT& s, const StageContext& ctx, SinkT sink) {
  { s.on_tick(ctx, sink) } -> std::same_as<void>;
  { s.flush(sink) } -> std::same_as<void>;
};

}  // namespace runtime
