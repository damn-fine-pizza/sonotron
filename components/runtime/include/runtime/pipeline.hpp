#pragma once

#include <cstddef>
#include <tuple>
#include <utility>

#include "runtime/stage.hpp"

// The Pipeline composite (docs/design/orchestrator-pipeline-extraction.md
// §16.3, Phase 4a/4d): a fixed, declared chain of sub-stages that is ITSELF a
// StageLike, so `runtime::Runtime<StageT, N>` never has to learn about
// multi-stage composition -- it just drives one `Pipeline<StageTs...>`
// exactly the way it drives any other single StageT (`Runtime` itself is
// untouched, zero lines changed since 4a).
//
// Deliberately the narrowest possible generalization -- a fixed, declared
// N-slot composite, NOT a dynamic graph (product-identity.md's "power of a
// DAW, never its free-for-all graph"). Storage is a private recursive chain
// (`detail::PipelineChain`), COMPOSITION-based (not inheritance, since 4d):
// each level's OWN stage is declared -- and therefore constructed -- BEFORE
// the remainder of the chain, so a LATER-declared stage's constructor can
// safely take a reference to an EARLIER sibling's ALREADY-CONSTRUCTED
// instance (e.g. arrangrr's Engine needs the already-built chorddet peer).
// This mirrors byte-for-byte the "declare/construct A, then construct B
// referencing A" idiom `runtime::Runtime` already uses for Transport/
// OutScheduler ahead of its own Stage, one level up.
//
// Phase-4d resolution of 4a's flagged "same forwarded args to every stage"
// limitation (§16.9 point 5, §16.3): each declared stage is now constructed
// from a FACTORY callable of its OWN (heterogeneous per stage, e.g. the
// design doc's illustrative shape: MidiSource needs `(sched, transport)`,
// chorddet needs `(followed)`, arrangrr needs `(sched, transport, followed,
// chorddet)`) instead of one uniform arg set forwarded identically to every
// stage. Every factory is called with `(scheduler, transport, stage_0, ...,
// stage_{i-1})` -- the two references `Runtime` always injects, PLUS every
// sibling already constructed at an earlier declared position -- and must
// return its StageT BY VALUE: C++17 guaranteed copy elision places the
// result directly into the chain's storage, so no move/copy of StageT is
// ever actually required (safe even though a stage may hold reference
// members, e.g. `Engine`'s injected `Transport&`/`OutScheduler&`).
//
// on_tick fans out to every declared stage IN ORDER, passing the SAME
// ctx/sink -- mirroring the fixed fire order `Engine`'s own internal
// on_tick already uses, giving same-tick cross-stage visibility by
// construction, no lag (D53).
//
// flush() delegates SOLELY to the LAST declared stage (the arrangrr stage,
// by pipeline convention `[MidiSource, chorddet, ..., arrangrr]`) --
// Corelli's correction #4 (§16.9 point 4): confirmed correct, not merely
// convenient (`NoteTracker` observes the OUTPUT stream; Panic must silence
// the melody thru too). The other stages have nothing of their own left to
// drain once the shared `OutScheduler` moved to Pipeline-level ownership.
//
// push_midi_in (NEW, 4d, §16.4 "Seam C"): the ONE inbound-MIDI fan-out
// point. Every declared stage OTHER than the last is offered the same raw
// bytes through a chorddet-shaped `push_midi_in(port, ptr, count, on_steer)`
// hook IF it has one (an `if constexpr` SFINAE check -- a stage without one,
// e.g. a MIDI-source stage that only ever PRODUCES via on_tick, is silently
// skipped, never called, never required to grow one); the terminal stage
// gets the arrangrr-shaped `push_midi_in(port, bytes, sink)` overload,
// unchanged (routing + flush, no chorddet awareness left inside it). Each
// non-terminal hook's `on_steer` callback forwards straight into the
// terminal stage's OWN `emit_chord_followed` -- the dedup-latched emit point
// stays unique there, never duplicated here (Corelli's §16.9 points 2/3).
//
// push_command delegates SOLELY to the terminal (arrangrr) stage -- the
// same "terminal owns it" convention flush() uses: Command/Param are
// arrangrr's own ABI vocabulary, never understood by a chorddet or
// MIDI-source stage (D43).
//
// Degenerate 1-stage case (`Pipeline<Engine>`): on_tick/flush/push_midi_in/
// push_command all forward to the sole stage -- a transparent wrapper,
// byte-identical output to driving `Engine` directly through
// `Runtime<Engine, N>`.

namespace runtime {

namespace detail {

// Non-terminal level: `StageT` (THIS level's declared stage) is declared --
// and therefore constructed -- BEFORE `m_rest` (composition, not
// inheritance), so `m_rest`'s stages may reference `m_stage` by the time
// THEY construct.
template <typename StageT, typename... Rest>
class PipelineChain {
 public:
  // `prior` is a tuple of references to `(scheduler, transport, every stage
  // already constructed by an earlier level)`. `factory` is invoked with
  // `prior`'s contents unpacked (std::apply) and must return a `StageT`
  // prvalue (see header comment: guaranteed copy elision, no actual
  // move/copy ever required). `rest_factories` are threaded down to the
  // remainder of the chain, each later factory additionally seeing THIS
  // stage once it exists.
  template <typename PriorTuple, typename FactoryT, typename... RestFactories>
  explicit PipelineChain(PriorTuple prior, FactoryT&& factory, RestFactories&&... rest_factories)
      : m_stage(std::apply(std::forward<FactoryT>(factory), prior)),
        m_rest(std::tuple_cat(prior, std::tie(m_stage)),
               std::forward<RestFactories>(rest_factories)...) {}

  template <typename SinkT>
  void fire_on_tick(const StageContext& ctx, SinkT sink) {
    m_stage.on_tick(ctx, sink);
    m_rest.fire_on_tick(ctx, sink);
  }

  // Non-terminal levels never flush their own stage -- only the chain's
  // terminal (last-declared) stage does, see the specialization below.
  template <typename SinkT>
  void fire_flush(SinkT sink) {
    m_rest.fire_flush(sink);
  }

  // Seam C fan-out (§16.4): offer THIS stage the raw bytes only if it has a
  // matching chorddet-shaped hook (SFINAE, never a hard requirement), then
  // recurse into the rest of the chain. `on_steer` is a NAMED local (built
  // once, unconditionally, even for a stage that turns out to have no
  // matching hook -- cheap, just a reference capture) rather than a
  // lambda-expression written inline inside the `requires` clause: a lambda
  // literal defined inside an ad-hoc requires-expression may not capture an
  // enclosing function's local variables (ill-formed under Clang, though
  // GCC accepts it) -- naming it first and only REFERRING to it inside
  // `requires {}` keeps the check itself free of any new lambda-expression.
  template <typename BytesT, typename SinkT, typename TerminalT>
  void fire_push_midi_in(std::uint8_t port, BytesT bytes, SinkT sink, TerminalT& terminal) {
    auto on_steer = [&](auto producer) { terminal.emit_chord_followed(producer, sink); };
    if constexpr (requires { m_stage.push_midi_in(port, bytes.data(), bytes.size(), on_steer); }) {
      m_stage.push_midi_in(port, bytes.data(), bytes.size(), on_steer);
    }
    m_rest.fire_push_midi_in(port, bytes, sink, terminal);
  }

  template <std::size_t Index>
  decltype(auto) stage_at() noexcept {
    if constexpr (Index == 0) {
      return (m_stage);
    } else {
      return m_rest.template stage_at<Index - 1>();
    }
  }
  template <std::size_t Index>
  decltype(auto) stage_at() const noexcept {
    if constexpr (Index == 0) {
      return (m_stage);
    } else {
      return m_rest.template stage_at<Index - 1>();
    }
  }

 private:
  StageT m_stage;                 // constructed FIRST (declaration order)
  PipelineChain<Rest...> m_rest;  // constructed SECOND -- may reference m_stage
};

// Terminal specialization: exactly one (the LAST declared) stage -- the
// arrangrr stage by pipeline convention. This is the one whose `flush()`
// actually drains the shared `OutScheduler`, and whose `push_midi_in`/
// `push_command` are the arrangrr-shaped overloads Pipeline delegates to.
template <typename StageT>
class PipelineChain<StageT> {
 public:
  template <typename PriorTuple, typename FactoryT>
  explicit PipelineChain(PriorTuple prior, FactoryT&& factory)
      : m_stage(std::apply(std::forward<FactoryT>(factory), prior)) {}

  template <typename SinkT>
  void fire_on_tick(const StageContext& ctx, SinkT sink) {
    m_stage.on_tick(ctx, sink);
  }

  template <typename SinkT>
  void fire_flush(SinkT sink) {
    m_stage.flush(sink);
  }

  template <typename BytesT, typename SinkT, typename TerminalT>
  void fire_push_midi_in(std::uint8_t port, BytesT bytes, SinkT sink, TerminalT&) {
    m_stage.push_midi_in(port, bytes, sink);
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
  // `scheduler`/`transport` are whatever `runtime::Runtime<Pipeline<...>, N>`
  // (untouched by this design) always prepends to its ONE Stage's ctor args
  // -- deduced generically here (never `arrangrr::OutScheduler<N>`/
  // `Transport` spelled by name) so this header keeps naming no arrangrr
  // type, the same discipline runtime/stage.hpp already documents.
  // `factories` is ONE callable PER declared stage; see the header comment
  // above for the full construction contract.
  template <typename SchedulerT, typename TransportT, typename... Factories>
  explicit Pipeline(SchedulerT& scheduler, TransportT& transport, Factories&&... factories)
      : m_chain(std::tie(scheduler, transport), std::forward<Factories>(factories)...) {}

  template <typename SinkT>
  void on_tick(const StageContext& ctx, SinkT sink) {
    m_chain.fire_on_tick(ctx, sink);
  }

  template <typename SinkT>
  void flush(SinkT sink) {
    m_chain.fire_flush(sink);
  }

  // See the header comment ("push_midi_in") for the fan-out contract.
  template <typename BytesT, typename SinkT>
  void push_midi_in(std::uint8_t port, BytesT bytes, SinkT sink) {
    auto& terminal = stage<sizeof...(StageTs) - 1>();
    m_chain.fire_push_midi_in(port, bytes, sink, terminal);
  }

  // See the header comment ("push_command") for the delegation contract.
  template <typename CommandT, typename SinkT>
  void push_command(const CommandT& cmd, SinkT sink) {
    stage<sizeof...(StageTs) - 1>().push_command(cmd, sink);
  }

  // Reaches one declared stage by its 0-based position in the pipeline --
  // the indirection every composition point (e.g. `hostrt::Shell`) needs
  // now that the StageT `Runtime` drives is the Pipeline, not a bare stage.
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
