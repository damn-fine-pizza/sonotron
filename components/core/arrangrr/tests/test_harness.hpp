#pragma once

// Phase-4d test harness update (docs/design/orchestrator-pipeline-
// extraction.md §16.3/§16.4/§16.9, phase4-execution-plan.md 4d): `Engine`
// alone is no longer constructible (it now needs a `FollowedContext&` AND a
// `ChorddetStage&` too, both Pipeline-owned per 4d). `TestEngine` now bundles
// a `runtime::Runtime<runtime::Pipeline<ChorddetStage<kMaxPorts>, Engine>,
// N>` -- the SAME default (interactive) 2-stage pipeline shape
// `hostrt::Shell` drives -- behind `Engine`'s PRE-4d public surface, so the
// many existing arrangrr test files that construct a bare `TestEngine e;`
// and call `advance_ticks`/`push_command`/`push_midi_in`/`set_chord_detect`/
// etc. need zero changes at their own call sites: every method call keeps
// its exact old spelling, dispatched through the 2-stage pipeline instead of
// a bare Engine.
//
// Deliberate small duplication (not a shared library): components/core/runtime/
// tests/test_harness.hpp is a near-identical twin, matching the same
// per-component test-local-header precedent as test.hpp itself.

#include "arrangrr/engine.hpp"
#include "runtime/pipeline.hpp"
#include "runtime/runtime.hpp"

namespace arrangrr::test {

class TestEngine {
 public:
  using EventSink = Engine::EventSink;

  TestEngine()
      : m_runtime([this](auto&, auto&) { return ChorddetStage<kMaxPorts>(m_followed); },
                  [this](auto& sched, auto& transport, auto& chorddet) {
                    return Engine(sched, transport, m_followed, chorddet);
                  }) {}

  Tick now() const noexcept { return m_runtime.now(); }
  const Transport& transport() const noexcept { return m_runtime.transport(); }
  const Timeline& timeline() const noexcept { return engine().timeline(); }
  const ChordEngine& chords() const noexcept { return engine().chords(); }
  const ChordSequencer& sequences() const noexcept { return engine().sequences(); }
  const Arranger& arranger() const noexcept { return engine().arranger(); }
  const ClipMatrix& clips() const noexcept { return engine().clips(); }
  // Phase 7 (node 6000, the Looper) test seam: mirrors clips()/sequences()'
  // own const observation accessor -- pure forwarding to Engine::loops(),
  // already public production API.
  const LoopBuffer& loops() const noexcept { return engine().loops(); }
  // Phase 7 (node 8100, Scenes/song mode) test seam: mirrors loops()' own
  // pure-forwarding accessor, pure forwarding to Engine::scenes(), already
  // public production API.
  const SceneChain& scenes() const noexcept { return engine().scenes(); }
  // Phase 7 (node 6300, retroactive capture) test seam: mirrors loops()' own
  // pure-forwarding accessor, pure forwarding to Engine::retro_capture(),
  // already public production API.
  const RetroCaptureRing& retro_capture() const noexcept { return engine().retro_capture(); }
  // Phase-5 Item #9 test seam (Torquato): mirrors clips()' own const+mutable
  // accessor pair. PadEngine/PerformanceStore state is observed through the
  // ABI in every functional pad/perf test (test_pad.cpp/test_performance.cpp)
  // EXCEPT the atomicity tests, which need to inject a deliberately INVALID
  // Performance directly (capture_performance() can never itself produce an
  // out-of-range field, so the only way to exercise apply_performance's
  // validate-first-apply-nothing guard is to bypass capture and write a bad
  // record straight into the store). Pure forwarding to Engine::performances(),
  // which is already public production API -- no new logic, no behavior
  // change; only extends this test-only wrapper's surface.
  const PerformanceStore& performances() const noexcept { return engine().performances(); }
  PerformanceStore& performances() noexcept { return engine().performances(); }
  // Phase-6 Theme 3 Item #4: pure forwarding to Engine::pad_bank(), same
  // precedent as performances() above.
  std::uint16_t pad_bank() const noexcept { return engine().pad_bank(); }

  void push_midi_in(std::uint8_t port, Span<const std::uint8_t> bytes, EventSink sink) {
    m_runtime.stage().push_midi_in(port, bytes, sink);
  }

  void set_arp_enabled(bool enabled, std::uint8_t in_port) noexcept {
    engine().set_arp_enabled(enabled, in_port);
  }
  bool arp_enabled() const noexcept { return engine().arp_enabled(); }
  void set_arp_out(std::uint8_t port, std::uint8_t channel) noexcept {
    engine().set_arp_out(port, channel);
  }
  const ArpeggiatorEngine& arp() const noexcept { return engine().arp(); }
  ArpeggiatorEngine& arp() noexcept { return engine().arp(); }

  void set_chord_detect(bool enabled, std::uint8_t port) noexcept {
    engine().set_chord_detect(enabled, port);
  }
  bool chord_detect() const noexcept { return engine().chord_detect(); }

  void set_input_zone(std::uint8_t port, InputZone zone) noexcept {
    engine().set_input_zone(port, zone);
  }
  InputZone input_zone(std::uint8_t port) const noexcept { return engine().input_zone(port); }

  void set_chord_follow(ChordFollow follow) noexcept { engine().set_chord_follow(follow); }
  ChordFollow chord_follow() const noexcept { return engine().chord_follow(); }

  void set_detect_quantize(bool quantize) noexcept { engine().set_detect_quantize(quantize); }
  bool detect_quantize() const noexcept { return engine().detect_quantize(); }
  std::uint8_t chord_held_count() const noexcept { return engine().chord_held_count(); }
  std::uint8_t chord_min_notes() const noexcept { return engine().chord_min_notes(); }

  void push_command(const Command& cmd, EventSink sink) { engine().push_command(cmd, sink); }

  void schedule_at(std::uint8_t port, Tick tick, const MidiMessage& msg, EventSink sink) {
    engine().schedule_at(port, tick, msg, sink);
  }

  void advance_ticks(std::uint32_t n, EventSink sink) { m_runtime.advance_ticks(n, sink); }

 private:
  Engine& engine() noexcept { return m_runtime.stage().template stage<1>(); }
  const Engine& engine() const noexcept { return m_runtime.stage().template stage<1>(); }

  FollowedContext m_followed{};
  runtime::Runtime<runtime::Pipeline<ChorddetStage<kMaxPorts>, Engine>, kSchedulerCapacity>
      m_runtime;
};

}  // namespace arrangrr::test
