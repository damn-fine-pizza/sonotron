#pragma once

#include <cstddef>

#include "runtime/stage.hpp"

// The Pipeline composite (docs/design/orchestrator-pipeline-extraction.md
// §16.3, Phase 4a): a fixed, declared chain of sub-stages that is ITSELF a
// StageLike, so `runtime::Runtime<StageT, N>` never has to learn about
// multi-stage composition -- it just drives one `Pipeline<StageTs...>`
// exactly the way it drives any other single StageT (`Runtime` itself is
// untouched by this header, zero lines changed).
//
// Deliberately the narrowest possible generalization -- a fixed, declared
// N-slot composite, NOT a dynamic graph (product-identity.md's "power of a
// DAW, never its free-for-all graph"). Storage is a private recursive chain
// (`detail::PipelineChain`), not `std::tuple`: every declared stage is
// constructed EXACTLY ONCE, in place, directly from the shared reference
// arguments Pipeline forwards to every stage -- no stage type is ever
// copied or moved, so a stage holding reference members (e.g.
// `arrangrr::Engine`'s injected `Transport&`/`OutScheduler&`) never needs to
// be move-constructible. Dual-target/freestanding: no heap, plain value
// members, same shape as `runtime::Runtime`.
//
// Construction contract (4a-scoped, deliberately simple): every declared
// stage is constructed from the SAME forwarded argument set -- e.g.
// `Pipeline<Engine>(scheduler&, transport&)` constructs the sole `Engine`
// from those exact two references, byte-identical to constructing `Engine`
// directly. A later N-stage Accompany pipeline (4b/4c) whose stages need
// heterogeneous per-stage constructor arguments (per the design doc's
// illustrative §16.3 sketch: MidiSource needs `(sched, transport)`,
// chorddet needs `(followed)`, arrangrr needs `(sched, transport,
// followed)`) will need this forwarding contract revisited -- flagged here,
// not solved now, since Phase 4a has exactly one real stage and no
// heterogeneous-args case to prove.
//
// on_tick fans out to every declared stage IN ORDER, passing the SAME
// ctx/sink -- mirroring the fixed fire order `Engine`'s own internal
// on_tick already uses (§16.2b), giving same-tick cross-stage visibility by
// construction, no lag.
//
// flush() delegates SOLELY to the LAST declared stage (the arrangrr stage,
// by pipeline convention `[MidiSource, chorddet, ..., arrangrr]`) --
// Corelli's correction #4 (§16.9 point 4): confirmed correct, not merely
// convenient (`NoteTracker` observes the OUTPUT stream; Panic must silence
// the melody thru too). The other stages have nothing of their own left to
// drain once the shared `OutScheduler` moved to Pipeline-level ownership.
//
// Degenerate 1-stage case (`Pipeline<Engine>`, Phase 4a): on_tick/flush both
// forward to the sole stage -- a transparent wrapper, byte-identical output
// to driving `Engine` directly through `Runtime<Engine, N>`.

namespace runtime {

namespace detail {

// Recursive storage for 2+ declared stages: this level owns `StageT` and
// inherits the storage (and behaviour) for the rest of the chain.
template <typename StageT, typename... Rest>
class PipelineChain : public PipelineChain<Rest...> {
  using Base = PipelineChain<Rest...>;

 public:
  template <typename... Args>
  explicit PipelineChain(Args&... args) : Base(args...), m_stage(args...) {}

  template <typename SinkT>
  void fire_on_tick(const StageContext& ctx, SinkT sink) {
    m_stage.on_tick(ctx, sink);
    Base::fire_on_tick(ctx, sink);
  }

  // Non-terminal levels never flush their own stage -- only the chain's
  // terminal (last-declared) stage does, see the specialization below.
  template <typename SinkT>
  void fire_flush(SinkT sink) {
    Base::fire_flush(sink);
  }

  template <std::size_t Index>
  decltype(auto) stage_at() noexcept {
    if constexpr (Index == 0) {
      return (m_stage);
    } else {
      return Base::template stage_at<Index - 1>();
    }
  }
  template <std::size_t Index>
  decltype(auto) stage_at() const noexcept {
    if constexpr (Index == 0) {
      return (m_stage);
    } else {
      return Base::template stage_at<Index - 1>();
    }
  }

 private:
  StageT m_stage;
};

// Terminal specialization: exactly one (the LAST declared) stage. This is
// the one whose `flush()` actually drains the shared `OutScheduler` --
// Corelli's correction #4 (§16.9 point 4).
template <typename StageT>
class PipelineChain<StageT> {
 public:
  template <typename... Args>
  explicit PipelineChain(Args&... args) : m_stage(args...) {}

  template <typename SinkT>
  void fire_on_tick(const StageContext& ctx, SinkT sink) {
    m_stage.on_tick(ctx, sink);
  }

  template <typename SinkT>
  void fire_flush(SinkT sink) {
    m_stage.flush(sink);
  }

  template <std::size_t Index>
  decltype(auto) stage_at() noexcept {
    static_assert(Index == 0, "Pipeline stage index out of range");
    return (m_stage);
  }
  template <std::size_t Index>
  decltype(auto) stage_at() const noexcept {
    static_assert(Index == 0, "Pipeline stage index out of range");
    return (m_stage);
  }

 private:
  StageT m_stage;
};

}  // namespace detail

template <typename... StageTs>
class Pipeline {
  static_assert(sizeof...(StageTs) > 0, "Pipeline requires at least one declared stage");

 public:
  // NOTE: `args` is deliberately never `std::forward`-ed past this
  // constructor -- unlike `Runtime`'s forwarding (which hands its args to
  // exactly ONE Stage constructor), Pipeline reuses the SAME shared
  // references to construct every declared stage in the chain, so each
  // argument must stay a valid lvalue across every use.
  template <typename... Args>
  explicit Pipeline(Args&&... args) : m_chain(args...) {}

  template <typename SinkT>
  void on_tick(const StageContext& ctx, SinkT sink) {
    m_chain.fire_on_tick(ctx, sink);
  }

  template <typename SinkT>
  void flush(SinkT sink) {
    m_chain.fire_flush(sink);
  }

  // Reaches one declared stage by its 0-based position in the pipeline --
  // the indirection every composition point (e.g. `hostrt::Shell`) needs
  // now that the StageT `Runtime` drives is the Pipeline, not the bare
  // arrangrr `Engine`.
  template <std::size_t Index>
  decltype(auto) stage() noexcept {
    return m_chain.template stage_at<Index>();
  }
  template <std::size_t Index>
  decltype(auto) stage() const noexcept {
    return m_chain.template stage_at<Index>();
  }

 private:
  detail::PipelineChain<StageTs...> m_chain;
};

}  // namespace runtime
