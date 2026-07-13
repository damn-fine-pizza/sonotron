#pragma once

#include <cstddef>
#include <cstdint>

#include "chorddet/chord_detector.hpp"
#include "chorddet/followed_context.hpp"
#include "common/midi/message.hpp"
#include "runtime/midi_parser.hpp"
#include "runtime/stage.hpp"

// The chorddet Stage-adapter (docs/design/orchestrator-pipeline-extraction.md
// §16.1/§16.4, Phase-4b, "Seam C"): live piano->chord recognition, promoted
// out of arrangrr::Engine::push_midi_in/observe_chord_input into its own
// peer. Dual-target/freestanding (no heap): a compile-time-capacity array of
// its OWN MidiParser instances -- Seam C explicitly rejects sharing
// arrangrr's routing parser or its parsed MidiMessage ("each stage keeps its
// OWN MidiParser instance", never a shared fourth cross-stage singleton).
//
// FollowedContext is injected BY REFERENCE (the same shared-reference idiom
// already used for Transport/OutScheduler, §14.3): this stage WRITES the
// followed chord (steer, via FollowedContext::stage()/commit_now(), replacing
// today's Engine::observe_chord_input) and the peer arrangrr stage READS it
// (state()/pending(), through ChordEngine) -- same-tick visible (D53) because
// it is the SAME object, not a message delayed a tick.
//
// This sub-phase (4b) keeps chorddet's own on_tick/flush degenerate no-ops:
// chorddet has no per-tick work of its own (recognition happens purely on
// inbound push_midi_in); the StageLike shape is implemented anyway so a
// future dual-target [chorddet]->[arrangrr] two-stage Pipeline (§16.1/§16.6)
// can drive it identically to every other stage without a shape change.
//
// `kPorts` is a template parameter (not arrangrr::kMaxPorts) so this header
// never needs to include arrangrr/config.hpp -- chorddet must not depend on
// arrangrr (D43); arrangrr instantiates this with its own kMaxPorts instead.

namespace arrangrr {

template <std::size_t kPorts>
class ChorddetStage {
 public:
  explicit ChorddetStage(FollowedContext& followed) noexcept : m_followed(followed) {}

  // Live piano->chord config, byte-identical semantics to the pre-4b
  // Engine::set_chord_detect: toggling ON resets the held note set but never
  // the latched chord (chord memory persists).
  void set_enabled(bool enabled, std::uint8_t port) noexcept {
    if (port < kPorts) {
      m_port = port;
    }
    if (enabled && !m_enabled) {
      m_detector.clear();
    }
    m_enabled = enabled;
  }
  constexpr bool enabled() const noexcept { return m_enabled; }

  // SHIFT-staging toggle for the live-detect steer (immediate commit_now by
  // default; staged to the next bar when quantized), pre-4b
  // Engine::set_detect_quantize/detect_quantize.
  constexpr void set_quantize(bool quantize) noexcept { m_quantize = quantize; }
  constexpr bool quantize() const noexcept { return m_quantize; }

  constexpr void set_key(const Key& key) noexcept { m_detector.set_key(key); }
  // Read-only access to the current key (roadmap 9320, Restyle): forwards to
  // the detector's own getter -- see chord_detector.hpp's comment.
  constexpr const Key& key() const noexcept { return m_detector.key(); }
  constexpr void set_min_notes(std::uint8_t n) noexcept { m_detector.set_min_notes(n); }
  constexpr void set_single_finger(bool on) noexcept { m_detector.set_single_finger(on); }
  // Releases every held note (panic): the held set resets, the latched chord
  // (chord memory) is untouched -- mirrors ChordDetector::clear() exactly.
  constexpr void clear() noexcept { m_detector.clear(); }

  constexpr std::uint8_t held_count() const noexcept { return m_detector.held_count(); }
  constexpr std::uint8_t min_notes() const noexcept { return m_detector.min_notes(); }

  // Seam C fan-out hook: fed the SAME raw bytes arrangrr::Engine::push_midi_in
  // receives for `port`, through THIS stage's OWN MidiParser (never Engine's
  // routing parser) so the two concerns never share parser state. Every port
  // is always fed (mirroring the pre-4b Engine's own always-parse-every-port
  // shape) so per-port running-status continuity holds even across a
  // set_enabled port change mid-stream; only a genuinely new chord (the
  // phantom-release-safe "only a NoteOn that GREW the held set may
  // re-recognize" rule, moved byte-for-byte from Engine::observe_chord_input)
  // invokes `on_steer(Producer::kDetect)`.
  template <typename OnSteer>
  void push_midi_in(std::uint8_t port, const std::uint8_t* bytes, std::size_t count,
                    OnSteer&& on_steer) {
    if (port >= kPorts) {
      return;
    }
    for (std::size_t i = 0; i < count; ++i) {
      m_parsers[port].feed(bytes[i], [&](const MidiMessage& msg) {
        if (m_enabled && port == m_port) {
          observe(msg, on_steer);
        }
      });
    }
  }

  // The STAGE port (runtime/stage.hpp's StageLike concept): degenerate
  // no-ops for 4b -- chorddet has no per-tick work of its own (recognition
  // is entirely inbound-driven, above). Kept so a future Pipeline can drive
  // this stage exactly like every other one, with no shape change later.
  template <typename SinkT>
  void on_tick(const runtime::StageContext&, SinkT) noexcept {}
  template <typename SinkT>
  void flush(SinkT) noexcept {}

 private:
  // Body moved byte-for-byte from arrangrr::Engine::observe_chord_input: a
  // NoteOn with velocity 0 is a running-status release; only a NoteOn that
  // actually GREW the held set may re-recognize and steer (the
  // phantom-release fix -- lifting fingers one at a time off an already
  // delivered chord must never re-harmonize on a passing subset).
  template <typename OnSteer>
  void observe(const MidiMessage& msg, OnSteer&& on_steer) {
    bool grew = false;
    if (msg.type() == midi::kNoteOn && msg.d2 > 0) {
      const std::uint8_t before = m_detector.held_count();
      m_detector.note_on(msg.d1);
      grew = m_detector.held_count() > before;
    } else if (msg.type() == midi::kNoteOff || (msg.type() == midi::kNoteOn && msg.d2 == 0)) {
      m_detector.note_off(msg.d1);
      return;  // a release never re-harmonizes
    } else {
      return;  // non-note messages leave the held set (and the chord) untouched
    }
    if (!grew) {
      return;  // a redundant press never re-harmonizes
    }
    ChordState detected;
    if (m_detector.recognize(detected)) {
      steer(detected);
      on_steer(Producer::kDetect);
    }
  }

  // Publishes the recognized chord through the shared FollowedContext owner,
  // mirroring ChordEngine::steer_detect exactly (immediate commit_now by
  // default, shift-staged when quantized). ChordDetector::recognize already
  // fills root_pc/quality/valid, so no re-derivation is needed here.
  constexpr void steer(const ChordState& chord) noexcept {
    if (m_quantize) {
      m_followed.stage(Producer::kDetect, chord);
    } else {
      m_followed.commit_now(Producer::kDetect, chord);
    }
  }

  FollowedContext& m_followed;
  ChordDetector m_detector;               // live piano->chord held-note set
  MidiParser m_parsers[kPorts];           // chorddet's OWN parsers (Seam C)
  bool m_enabled = false;                 // kChordDetect: detection enabled
  bool m_quantize = false;                // SHIFT staging: immediate by default
  std::uint8_t m_port = 0;                // input port feeding the detector
};

}  // namespace arrangrr
