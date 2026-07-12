#pragma once

// Phase-1 runtime-extraction test harness: `Engine` is no longer default-
// constructible on its own (Transport/OutScheduler are Runtime-owned and
// injected by reference, docs/design/orchestrator-pipeline-extraction.md
// §14.3/B3). `TestEngine` bundles a `runtime::Runtime<Engine, N>` behind
// Engine's PRE-Phase-1 public surface so the many existing arrangrr test
// files that construct a bare `Engine e;` and call `advance_ticks`/
// `push_command`/`transport()`/etc. need only swap the type name at the
// construction site — every method call keeps its exact old spelling.
//
// Deliberate small duplication (not a shared library): components/runtime/
// tests/test_harness.hpp is a near-identical twin, matching the same
// per-component test-local-header precedent as test.hpp itself.

#include "arrangrr/engine.hpp"
#include "runtime/runtime.hpp"

namespace arrangrr::test {

class TestEngine {
 public:
  using EventSink = Engine::EventSink;

  Tick now() const noexcept { return m_runtime.now(); }
  const Transport& transport() const noexcept { return m_runtime.transport(); }
  const Timeline& timeline() const noexcept { return m_runtime.stage().timeline(); }
  const ChordEngine& chords() const noexcept { return m_runtime.stage().chords(); }
  const ChordSequencer& sequences() const noexcept { return m_runtime.stage().sequences(); }
  const Arranger& arranger() const noexcept { return m_runtime.stage().arranger(); }

  void push_midi_in(std::uint8_t port, Span<const std::uint8_t> bytes, EventSink sink) {
    m_runtime.stage().push_midi_in(port, bytes, sink);
  }

  void set_arp_enabled(bool enabled, std::uint8_t in_port) noexcept {
    m_runtime.stage().set_arp_enabled(enabled, in_port);
  }
  bool arp_enabled() const noexcept { return m_runtime.stage().arp_enabled(); }
  void set_arp_out(std::uint8_t port, std::uint8_t channel) noexcept {
    m_runtime.stage().set_arp_out(port, channel);
  }
  const ArpeggiatorEngine& arp() const noexcept { return m_runtime.stage().arp(); }
  ArpeggiatorEngine& arp() noexcept { return m_runtime.stage().arp(); }

  void set_chord_detect(bool enabled, std::uint8_t port) noexcept {
    m_runtime.stage().set_chord_detect(enabled, port);
  }
  bool chord_detect() const noexcept { return m_runtime.stage().chord_detect(); }

  void set_input_zone(std::uint8_t port, InputZone zone) noexcept {
    m_runtime.stage().set_input_zone(port, zone);
  }
  InputZone input_zone(std::uint8_t port) const noexcept {
    return m_runtime.stage().input_zone(port);
  }

  void set_chord_follow(ChordFollow follow) noexcept { m_runtime.stage().set_chord_follow(follow); }
  ChordFollow chord_follow() const noexcept { return m_runtime.stage().chord_follow(); }

  void set_detect_quantize(bool quantize) noexcept { m_runtime.stage().set_detect_quantize(quantize); }
  bool detect_quantize() const noexcept { return m_runtime.stage().detect_quantize(); }
  std::uint8_t chord_held_count() const noexcept { return m_runtime.stage().chord_held_count(); }
  std::uint8_t chord_min_notes() const noexcept { return m_runtime.stage().chord_min_notes(); }

  void push_command(const Command& cmd, EventSink sink) { m_runtime.push_command(cmd, sink); }

  void schedule_at(std::uint8_t port, Tick tick, const MidiMessage& msg, EventSink sink) {
    m_runtime.stage().schedule_at(port, tick, msg, sink);
  }

  void advance_ticks(std::uint32_t n, EventSink sink) { m_runtime.advance_ticks(n, sink); }

 private:
  runtime::Runtime<Engine, kSchedulerCapacity> m_runtime;
};

}  // namespace arrangrr::test
