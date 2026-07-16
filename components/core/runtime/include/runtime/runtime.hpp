#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

#include "runtime/out_scheduler.hpp"
#include "runtime/stage.hpp"
#include "runtime/transport.hpp"

// The Runtime driver (docs/design/orchestrator-pipeline-extraction.md §3.5,
// §14.3): owns the ONE Transport + the ONE OutScheduler (the D29 global
// total-order emission queue) and drives exactly one Stage instance in
// Phase 1. Deliberately generic (`Runtime<StageT>`, `advance_ticks`/
// `push_command` templated on the sink/command types): this header never
// names an arrangrr type (`Command`, `OutEvent`, `Param`) so the
// `runtime -> arrangrr` CMake edge never has a reason to exist (Decision A).
//
// Seam B1 (flush/NoteTracker) and B2 (push_command's direct scheduler calls):
// resolved per §14.3/§14.4 by injecting `m_scheduler` into the Stage BY
// REFERENCE (constructed once here, the single source of truth), so
// arrangrr's `flush()`/`schedule_or_warn`/`schedule_pattern`/
// `cancel_note_off` call sites keep their bodies byte-identical — only the
// declaration site of the member changes from a value to a reference.
//
// Seam B3 (transport): this Phase-1 implementation extends the SAME
// reference-injection precedent to `Transport` (see runtime/stage.hpp's
// header comment for the full rationale) rather than routing the four
// transport Params through a `Runtime::push_command` that would need to
// name `arrangrr::Param` — doing so would reopen exactly the cycle
// Decision A exists to kill. `push_command` here is therefore a fully
// generic, Command-agnostic passthrough to the stage; the stage's own
// `cmd_transport` mutates the shared `Transport&` directly, unchanged.

namespace runtime {

// `N` is the scheduler capacity (arrangrr::kSchedulerCapacity at every real
// call site) — a non-type template parameter, NOT an `arrangrr/config.hpp`
// include, so this header stays free of any arrangrr dependency (Decision A:
// no `runtime -> arrangrr` edge). The instantiator (components/hostrt,
// tests/arm-smoke's link_gate.cpp) already includes arrangrr/config.hpp for
// its own reasons and supplies the constant explicitly.
template <typename StageT, std::size_t N>
class Runtime {
 public:
  template <typename... StageArgs>
  explicit Runtime(StageArgs&&... args)
      : m_stage(m_scheduler, m_transport, std::forward<StageArgs>(args)...) {}

  arrangrr::Tick now() const noexcept { return m_now; }
  arrangrr::Transport& transport() noexcept { return m_transport; }
  const arrangrr::Transport& transport() const noexcept { return m_transport; }

  StageT& stage() noexcept { return m_stage; }
  const StageT& stage() const noexcept { return m_stage; }

  // Command-agnostic passthrough (never names arrangrr::Command/Param — see
  // header comment): the stage's own dispatch (unchanged) still owns every
  // Param, including the four transport ones, through its injected
  // Transport& reference.
  template <typename CommandT, typename SinkT>
  void push_command(const CommandT& cmd, SinkT sink) {
    m_stage.push_command(cmd, sink);
  }

  // Advances stream time by `n` ticks. Mirrors byte-for-byte the loop body
  // that used to live in Engine::advance_ticks (engine.hpp:185-216): the
  // stream tick always advances and is always flushed; the transport's own
  // musical tick advances, and the stage's per-tick fire loop runs, only
  // while playing.
  template <typename SinkT>
  void advance_ticks(std::uint32_t n, SinkT sink) {
    static_assert(StageLike<StageT, SinkT>,
                  "StageT must satisfy runtime::StageLike for this SinkT");
    for (std::uint32_t i = 0; i < n; ++i) {
      ++m_now;
      if (m_transport.playing()) {
        m_transport.advance_one();
      }
      const StageContext ctx{.now = m_now};
      m_stage.on_tick(ctx, sink);
      m_stage.flush(sink);
    }
  }

 private:
  arrangrr::Transport m_transport;
  arrangrr::OutScheduler<N> m_scheduler;
  StageT m_stage;  // constructed AFTER m_scheduler/m_transport (declaration order)
  arrangrr::Tick m_now = 0;
};

}  // namespace runtime
