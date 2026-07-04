#pragma once

#include <cstdint>

#include "arrangrr/abi.hpp"
#include "arrangrr/arranger/arranger.hpp"
#include "arrangrr/chord/chord_detector.hpp"
#include "arrangrr/chord/chord_engine.hpp"
#include "arrangrr/chord/chord_sequencer.hpp"
#include "arrangrr/common/function_ref.hpp"
#include "arrangrr/common/span.hpp"
#include "arrangrr/common/time.hpp"
#include "arrangrr/config.hpp"
#include "arrangrr/midi/parser.hpp"
#include "arrangrr/routing/note_tracker.hpp"
#include "arrangrr/routing/router.hpp"
#include "arrangrr/scheduler/out_scheduler.hpp"
#include "arrangrr/timeline/timeline.hpp"
#include "arrangrr/transport/transport.hpp"

// The engine: wires parser → router → scheduler → tracker behind the binary
// ABI. Time is injected (advance_ticks); the same code runs under the host
// real-clock thread, the virtual deterministic clock, and the STM32 timer.
//
// Two timelines (D29): `now_` is stream time and always advances with
// advance_ticks — scheduled events fire against it even when the transport is
// stopped (golden G1 plays chords with the transport idle). The transport's
// musical tick advances only while playing.

namespace arrangrr {

class Engine {
 public:
  // Monomorphic sink at the ABI boundary (D26): one instantiation, no
  // template bloat in flash; the callable is owned by the caller.
  using EventSink = FunctionRef<void(const OutEvent&)>;

  constexpr Tick now() const noexcept { return m_now; }
  constexpr const Transport& transport() const noexcept { return m_transport; }
  const Timeline& timeline() const noexcept { return m_timeline; }
  const ChordEngine& chords() const noexcept { return m_chords; }
  const ChordSequencer& sequences() const noexcept { return m_seq; }
  const Arranger& arranger() const noexcept { return m_arranger; }

  // Feeds raw MIDI bytes from an input port. Parsed messages are routed and
  // scheduled at the current tick; due events are flushed to the sink at the
  // end of the batch.
  void push_midi_in(std::uint8_t port, Span<const std::uint8_t> bytes, EventSink sink) {
    if (port >= kMaxPorts) {
      return;
    }
    for (std::uint8_t byte : bytes) {
      m_parsers[port].feed(byte, [&](const MidiMessage& msg) {
        m_router.route(port, msg, [&](std::uint8_t out_port, const MidiMessage& routed) {
          schedule_or_warn(out_port, m_now, routed, sink);
        });
        if (m_chord_detect && port == m_chord_detect_port) {
          observe_chord_input(msg);
        }
      });
    }
    flush(sink);
  }

  // Live piano->chord (kChordDetect): whether held notes on the detect port
  // re-harmonize the arranger. `port` selects which input keyboard is the
  // chord source. Toggling on resets the held-note set but never the latched
  // chord (chord memory persists).
  void set_chord_detect(bool enabled, std::uint8_t port) noexcept {
    if (port < kMaxPorts) {
      m_chord_detect_port = port;
    }
    if (enabled && !m_chord_detect) {
      m_detector.clear();
    }
    m_chord_detect = enabled;
  }
  constexpr bool chord_detect() const noexcept { return m_chord_detect; }

  // Applies one binary command (D26). Sink receives any resulting events.
  // Implemented in engine.cpp as per-domain handlers: the dispatch stays a
  // switch, the 300-line god-switch does not.
  void push_command(const Command& cmd, EventSink sink);

  // Schedules a message on the stream timeline at an absolute tick (used by
  // the host for @tick-scheduled script lines).
  void schedule_at(std::uint8_t port, Tick tick, const MidiMessage& msg, EventSink sink) {
    schedule_or_warn(port, tick, msg, sink);
    flush(sink);  // fire immediately if already due
  }

  // Advances stream time by `n` ticks, firing due events in D29 total order.
  void advance_ticks(std::uint32_t n, EventSink sink) {
    for (std::uint32_t i = 0; i < n; ++i) {
      ++m_now;
      if (m_transport.playing()) {
        m_transport.advance_one();
        if (m_clock_out_mask != 0 && Transport::is_midi_clock_tick(m_transport.tick())) {
          for (std::uint8_t p = 0; p < kMaxPorts; ++p) {
            if (m_clock_out_mask & (1u << p)) {
              schedule_or_warn(p, m_now, MidiMessage::realtime(midi::kClock), sink);
            }
          }
        }
        fire_timeline(m_transport.tick(), sink);
        fire_chord_seq(m_transport.tick(), sink);
        fire_arranger(m_transport.tick(), sink);
      }
      flush(sink);
    }
  }

 private:
  // Per-domain command handlers (engine.cpp).
  void cmd_transport(const Command& cmd, EventSink sink);
  void cmd_routing(const Command& cmd, EventSink sink);
  void cmd_chord(const Command& cmd, EventSink sink);
  void cmd_seq(const Command& cmd, EventSink sink);
  void cmd_track(const Command& cmd, EventSink sink);
  void cmd_style(const Command& cmd, EventSink sink);

  void schedule_or_warn(std::uint8_t port, Tick tick, const MidiMessage& msg, EventSink sink) {
    if (!m_scheduler.schedule(port, tick, msg)) {
      sink(OutEvent::warn(WarnCode::kSchedulerFull, m_now));
    }
  }

  // Transport realtime bytes (FA/FB/FC) go out immediately on clock ports.
  void emit_realtime(std::uint8_t status, EventSink sink) {
    for (std::uint8_t p = 0; p < kMaxPorts; ++p) {
      if (m_clock_out_mask & (1u << p)) {
        schedule_or_warn(p, m_now, MidiMessage::realtime(status), sink);
      }
    }
    flush(sink);
  }

  // Pattern-driven scheduling with retrigger care (§9.B): re-firing a note
  // whose previous NoteOff is still pending would either duplicate the on or
  // get truncated by the stale off. Close it now (D29 sorts the off first)
  // and tombstone the stale release.
  void schedule_pattern(std::uint8_t port, TickOffset delay, const MidiMessage& msg,
                        EventSink sink) {
    if (msg.type() == midi::kNoteOn &&
        m_scheduler.cancel_note_off(port, msg.channel(), msg.d1)) {
      schedule_or_warn(port, m_now, MidiMessage::note_off(msg.channel(), msg.d1), sink);
    }
    schedule_or_warn(port, m_now + static_cast<Tick>(delay), msg, sink);
  }

  void fire_timeline(Tick transport_tick, EventSink sink) {
    m_timeline.on_tick(transport_tick,
                       [&](std::uint8_t port, TickOffset delay, const MidiMessage& msg) {
                         schedule_pattern(port, delay, msg, sink);
                       });
  }

  void fire_chord_seq(Tick transport_tick, EventSink sink) {
    m_seq.on_tick(
        transport_tick,
        [&](std::uint8_t root_note, ChordQuality quality, std::uint8_t degree, std::uint8_t vel) {
          m_chords.sound(root_note, quality, vel, [&](std::uint8_t port, const MidiMessage& msg) {
            schedule_or_warn(port, m_now, msg, sink);
          });
          sink(OutEvent::chord(m_chords.out_port(), degree, static_cast<std::uint8_t>(quality),
                               root_note, theory::shape_of(quality).count, vel, m_now));
        },
        [&] {
          m_chords.release([&](std::uint8_t port, const MidiMessage& msg) {
            schedule_or_warn(port, m_now, msg, sink);
          });
        });
  }

  void fire_arranger(Tick transport_tick, EventSink sink) {
    const Arranger::TickResult r =
        m_arranger.on_tick(transport_tick, m_chords.state(),
                           [&](std::uint8_t port, TickOffset delay, const MidiMessage& msg) {
                             schedule_pattern(port, delay, msg, sink);
                           });
    if (r.section_changed) {
      sink(OutEvent::section(static_cast<std::uint16_t>(r.section), m_now));
    }
    if (r.stop_transport) {
      m_transport.stop();
      if (m_seq.playing()) {
        m_chords.release([&](std::uint8_t port, const MidiMessage& msg) {
          schedule_or_warn(port, m_now, msg, sink);
        });
      }
      emit_realtime(midi::kStop, sink);
      sink(OutEvent::transport(static_cast<std::uint16_t>(m_transport.state()), m_now));
    }
  }

  void flush(EventSink sink) {
    m_scheduler.pop_due(m_now, [&](const ScheduledEvent& ev) {
      m_tracker.observe(ev.port, ev.msg);
      sink(OutEvent::midi(ev.port, ev.msg, ev.tick));
    });
  }

  // Feeds one parsed message from the chord-detect port into the detector and,
  // on each successful recognition (>= a triad), steers the arranger's chord
  // context. A NoteOn with velocity 0 is a running-status release. Chord
  // memory: fewer notes recognize nothing, so the last chord holds.
  void observe_chord_input(const MidiMessage& msg) {
    if (msg.type() == midi::kNoteOn && msg.d2 > 0) {
      m_detector.note_on(msg.d1);
    } else if (msg.type() == midi::kNoteOff ||
               (msg.type() == midi::kNoteOn && msg.d2 == 0)) {
      m_detector.note_off(msg.d1);
    } else {
      return;  // non-note messages leave the held set (and the chord) untouched
    }
    ChordState detected;
    if (m_detector.recognize(detected)) {
      m_chords.set_context(detected.root_pc, detected.quality);
    }
  }

  Tick m_now = 0;
  Transport m_transport;
  MidiParser m_parsers[kMaxPorts];
  Router m_router;
  Timeline m_timeline;
  ChordEngine m_chords;
  ChordSequencer m_seq;
  Arranger m_arranger;
  ChordDetector m_detector;                 // live piano->chord held-note set
  bool m_chord_detect = false;              // kChordDetect: detection enabled
  std::uint8_t m_chord_detect_port = 0;     // input port feeding the detector
  NoteTracker m_tracker;
  OutScheduler<kSchedulerCapacity> m_scheduler;
  std::uint8_t m_clock_out_mask = 0;  // off by default; enabled via kClockOutMask
};

}  // namespace arrangrr
