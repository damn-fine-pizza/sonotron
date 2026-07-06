#pragma once

#include <cstdint>

#include "arrangrr/abi.hpp"
#include "arrangrr/arp/arpeggiator.hpp"
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

// ChordFollow (the D47 harmony-source selector) and Producer now live with
// their single owner in arrangrr/chord/followed_context.hpp (included via
// chord_engine.hpp): the gate they drive is enforced inside FollowedContext,
// not threaded as bools through the engine.

// Dxx (un-defers D34(b) / §11 split): per-input-port harmony zone. This is the
// "observe-without-route" split as DATA — the zone of a whole port decides
// route-vs-suppress, riding the SAME parsed input path (no parallel
// push_harmony_in). A future pitch split_point can later subdivide one physical
// port into two logical zones without reshaping this.
//   - kMelody: note messages ROUTE/sound normally through m_router (and steer
//     only if the port is also the detect port -> the D34 FullKeyboard branch).
//   - kHarmony: note messages are SUPPRESSED from output (never reach m_router)
//     while still OBSERVED by the detector when it is the detect port -> silent
//     re-harmonization (the canonical Split arranger zone).
// Default kMelody keeps every port byte-identical to the legacy path.
enum class InputZone : std::uint8_t {
  kMelody = 0,   // routes/sounds; observed only as the D34 FullKeyboard branch
  kHarmony = 1,  // observed + suppressed from output (silent chord zone)
};

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
        // The arpeggiator CAPTURES notes on its input port (they feed the arp
        // instead of routing straight through); non-note messages and other
        // ports route normally. Chord detection still OBSERVES either way.
        // Capture ONLY while the transport is playing: the arp only sounds from
        // fire_arp (which runs when playing), so if it swallowed the keyboard
        // with the transport stopped the held keys would light up but never
        // sound. Stopped => notes pass through and play normally.
        const bool arp_captures =
            m_arp_enabled && m_transport.playing() && port == m_arp_in_port && is_note_message(msg);
        // A note on a kHarmony port is a silent chord-recognition gesture: it is
        // OBSERVED (below) but SUPPRESSED from output — it never reaches the
        // router. Non-note traffic (CC, sustain, program) still passes; only the
        // sounding notes are silenced. kMelody ports route exactly as before.
        const bool harmony_suppress =
            is_note_message(msg) && m_input_zone[port] == InputZone::kHarmony;
        if (arp_captures) {
          observe_arp_input(msg);
        } else if (!harmony_suppress) {
          m_router.route(port, msg, [&](std::uint8_t out_port, const MidiMessage& routed) {
            schedule_or_warn(out_port, m_now, routed, sink);
          });
        }
        if (m_chord_detect && port == m_chord_detect_port) {
          observe_chord_input(msg);
        }
      });
    }
    flush(sink);
  }

  // Live arpeggiator control: enable + input port (the keyboard it listens to).
  void set_arp_enabled(bool enabled, std::uint8_t in_port) noexcept {
    if (in_port < kMaxPorts) {
      m_arp_in_port = in_port;
    }
    if (!enabled && m_arp_enabled) {
      m_arp.panic();  // dropping the effect clears the held set
    }
    m_arp_enabled = enabled;
  }
  constexpr bool arp_enabled() const noexcept { return m_arp_enabled; }
  void set_arp_out(std::uint8_t port, std::uint8_t channel) noexcept {
    if (port < kMaxPorts && channel <= 15) {
      m_arp_out_port = port;
      m_arp_out_channel = channel;
    }
  }
  const ArpeggiatorEngine& arp() const noexcept { return m_arp; }
  ArpeggiatorEngine& arp() noexcept { return m_arp; }

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

  // Dxx: the harmony zone of a whole input port. kHarmony suppresses the port's
  // note output (silent chord recognition); kMelody routes/sounds. Independent
  // of set_chord_detect (which decides which port OBSERVES): the Split default
  // is the host's chords-panel port set to kHarmony AND to the detect port; a
  // detect port left kMelody is the D34 FullKeyboard branch (sounds + steers).
  // Host wiring (which port is the piano vs the chords panel) is next-phase.
  constexpr void set_input_zone(std::uint8_t port, InputZone zone) noexcept {
    if (port < kMaxPorts) {
      m_input_zone[port] = zone;
    }
  }
  constexpr InputZone input_zone(std::uint8_t port) const noexcept {
    return port < kMaxPorts ? m_input_zone[port] : InputZone::kMelody;
  }

  // D47 chord-follow selector: which producer may steer the followed chord.
  // Owned by the FollowedContext inside m_chords (the single gate).
  constexpr void set_chord_follow(ChordFollow follow) noexcept { m_chords.set_follow(follow); }
  constexpr ChordFollow chord_follow() const noexcept { return m_chords.follow(); }

  // Input model: a live-detect chord commits IMMEDIATELY by default (`current`
  // changes now); a SHIFT-staged one is quantized to the next bar (`next`).
  // The host sets this per keypress (shift held -> quantize). Immediate is the
  // default, so raw MIDI in without a host steers `current` at once.
  constexpr void set_detect_quantize(bool quantize) noexcept { m_detect_quantize = quantize; }
  constexpr bool detect_quantize() const noexcept { return m_detect_quantize; }
  // Live piano->chord detector state, for host display (how many keys are held
  // and how many are needed before a chord is named — fingered 3 vs single 1).
  constexpr std::uint8_t chord_held_count() const noexcept { return m_detector.held_count(); }
  constexpr std::uint8_t chord_min_notes() const noexcept { return m_detector.min_notes(); }

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
        // D53: at the bar downbeat, promote any SHIFT-staged chord into the
        // followed context BEFORE the arranger fires the bar — the same
        // bar-boundary point at which the arranger applies pending style/section
        // switches, ordered so the arranger reads the freshly-committed chord.
        if (m_transport.tick() % kTicksPerBar == 0) {
          m_chords.commit_bar();
        }
        fire_arranger(m_transport.tick(), sink);
        fire_arp(m_transport.tick(), sink);
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
  void cmd_voice(const Command& cmd, EventSink sink);  // program change (voice select)
  void cmd_arp(const Command& cmd, EventSink sink);    // live arpeggiator

  // cmd_chord case handlers, split out to keep cmd_chord's own cognitive
  // complexity under the clang-tidy gate (each case validates + dispatches on
  // its own, no behavior change from being inlined in the switch).
  void chord_key_set(const Command& cmd, EventSink sink);
  void chord_out(const Command& cmd, EventSink sink);
  void chord_mode(const Command& cmd, EventSink sink);
  void chord_detect_cmd(const Command& cmd, EventSink sink);
  void chord_follow_cmd(const Command& cmd, EventSink sink);
  void chord_input_zone(const Command& cmd, EventSink sink);
  void chord_play(const Command& cmd, EventSink sink);

  // cmd_seq case handlers split out for the same reason.
  void seq_add(const Command& cmd, EventSink sink);
  void seq_stop(const Command& cmd, EventSink sink);
  void seq_transpose(const Command& cmd, EventSink sink);

  // cmd_style case handlers split out for the same reason.
  void style_switch(const Command& cmd, EventSink sink);
  void style_route(const Command& cmd, EventSink sink);

  // Emits the loaded style's default per-role GM voices on their routes. Cheap
  // and idempotent (re-sending a Program Change is a no-op on the synth), so it
  // is safe to call after a style load, a style switch, or a route change —
  // covering both "route before load" and "route after load" orders.
  void apply_arranger_voices(EventSink sink) {
    m_arranger.emit_voices([&](std::uint8_t port, TickOffset, const MidiMessage& msg) {
      schedule_or_warn(port, m_now, msg, sink);
    });
    flush(sink);
  }

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
  // get truncated by the stale off. The compensating off is anchored to the
  // INCOMING on's scheduled tick (m_now + delay), NOT to m_now — otherwise a
  // ratchet, which schedules several hits of the same note in one on_tick with
  // increasing delay, would re-emit every sub-hit's off at m_now and leave a
  // string of bare re-attacks on an already-sounding note. cancel_note_off is
  // tick-aware: it only cancels an off that lands at or after the new on, so a
  // sub-hit whose gate ends before the next hit keeps its silent gap. For the
  // normal cross-step case delay == 0, so on_tick == m_now and the wire is
  // byte-identical to the pre-fix path (D29 still sorts the off before the on).
  void schedule_pattern(std::uint8_t port, TickOffset delay, const MidiMessage& msg,
                        EventSink sink) {
    const Tick on_tick = m_now + static_cast<Tick>(delay);
    if (msg.type() == midi::kNoteOn &&
        m_scheduler.cancel_note_off(port, msg.channel(), msg.d1, on_tick)) {
      schedule_or_warn(port, on_tick, MidiMessage::note_off(msg.channel(), msg.d1), sink);
    }
    schedule_or_warn(port, on_tick, msg, sink);
  }

  void fire_timeline(Tick transport_tick, EventSink sink) {
    m_timeline.on_tick(transport_tick,
                       [&](std::uint8_t port, TickOffset delay, const MidiMessage& msg) {
                         schedule_pattern(port, delay, msg, sink);
                       });
  }

  void fire_chord_seq(Tick transport_tick, EventSink sink) {
    // LIVE-PRIORITY arbitration (the engine default): while a live chord is
    // actively HELD on the detect port, the band follows that live chord and the
    // sequencer must COMP on it (sound its rhythm/voicing on the live root+quality)
    // WITHOUT publishing its own chord — no clash. On release the held set empties
    // and the sequencer's next fired step resumes committing normally. This dynamic
    // held-vs-released state cannot live in FollowedContext's static gate, so the
    // decision is made here. Under any other ChordFollow value (kAuto legacy,
    // kDetect/kSequencer/kManual explicit) the sequencer behaves exactly as before:
    // it sounds its own chord and its publish is arbitrated by the D47 gate.
    const bool live_priority = m_chords.follow() == ChordFollow::kLivePriority;
    const bool live_held = m_chord_detect && m_detector.held_count() > 0;
    const bool comp_on_live = live_priority && live_held && m_chords.state().valid;
    m_seq.on_tick(
        transport_tick,
        [&](std::uint8_t root_note, ChordQuality quality, std::uint8_t degree, std::uint8_t vel) {
          // While comping on a held live chord, sound the LIVE root+quality and do
          // NOT steer (the live chord already owns the followed context). Otherwise
          // sound the sequencer's own chord and let the D47 gate decide the publish.
          std::uint8_t sound_root = root_note;
          ChordQuality sound_quality = quality;
          bool steer = true;
          if (comp_on_live) {
            const ChordState& live = m_chords.state();
            sound_root = static_cast<std::uint8_t>(60 + live.root_pc);
            sound_quality = live.quality;
            steer = false;
          }
          m_chords.sound(
              sound_root, sound_quality, vel,
              [&](std::uint8_t port, const MidiMessage& msg) {
                schedule_or_warn(port, m_now, msg, sink);
              },
              Producer::kSequencer, steer, /*quantize=*/false);
          sink(OutEvent::chord(m_chords.out_port(), degree,
                               static_cast<std::uint8_t>(sound_quality), sound_root,
                               theory::shape_of(sound_quality).count, vel, m_now));
        },
        [&] {
          m_chords.release([&](std::uint8_t port, const MidiMessage& msg) {
            schedule_or_warn(port, m_now, msg, sink);
          });
        });
  }

  void fire_arranger(Tick transport_tick, EventSink sink) {
    const Arranger::TickResult r =
        m_arranger.on_tick(transport_tick, m_chords.key(), m_chords.state(),
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
  // on a GENUINELY NEW chord, steers the arranger's chord context. A NoteOn with
  // velocity 0 is a running-status release.
  //
  // Phantom-release fix (part 2): only a NoteOn that actually GREW the held set
  // may re-recognize and publish. A NoteOff — or a duplicate NoteOn — never
  // re-recognizes, so lifting fingers off an already-delivered chord one note at
  // a time can no longer pass through a 3-note subset that gets named a new
  // chord and committed at the next bar with no fresh press. The held set still
  // holds the delivered chord (chord memory); a truly new chord is formed only
  // by pressing more keys.
  void observe_chord_input(const MidiMessage& msg) {
    bool grew = false;
    if (msg.type() == midi::kNoteOn && msg.d2 > 0) {
      const std::uint8_t before = m_detector.held_count();
      m_detector.note_on(msg.d1);
      grew = m_detector.held_count() > before;
    } else if (msg.type() == midi::kNoteOff || (msg.type() == midi::kNoteOn && msg.d2 == 0)) {
      m_detector.note_off(msg.d1);
    } else {
      return;  // non-note messages leave the held set (and the chord) untouched
    }
    if (!grew) {
      return;  // a release or a redundant press never re-harmonizes
    }
    ChordState detected;
    if (m_detector.recognize(detected)) {
      // The detector always tracks its held set (so the panel can display it);
      // the owner's D47 gate decides whether Producer::kDetect actually steers.
      // Input model: immediate by default (commit_now), quantized to the next
      // bar when the host flags SHIFT (stage).
      m_chords.steer_detect(detected.root_pc, detected.quality, m_detect_quantize);
    }
  }

  static bool is_note_message(const MidiMessage& msg) {
    return msg.type() == midi::kNoteOn || msg.type() == midi::kNoteOff;
  }

  // Feeds a captured keyboard note into the live arpeggiator. A NoteOn vel 0 is
  // a release. The arp then plays the held set rhythmically from fire_arp.
  void observe_arp_input(const MidiMessage& msg) {
    if (msg.type() == midi::kNoteOn && msg.d2 > 0) {
      m_arp.note_on(msg.d1, msg.d2);
    } else {
      m_arp.note_off(msg.d1);
    }
  }

  // Clock-driven arpeggiator: on each transport tick it may emit the next arp
  // note on its output route (note-off follows after the gate).
  void fire_arp(Tick transport_tick, EventSink sink) {
    if (!m_arp_enabled) {
      return;
    }
    m_arp.on_tick(transport_tick,
                  [&](std::uint8_t note, std::uint8_t velocity, TickOffset gate) {
                    schedule_pattern(m_arp_out_port, 0,
                                     MidiMessage::note_on(m_arp_out_channel, note, velocity), sink);
                    schedule_pattern(m_arp_out_port, gate,
                                     MidiMessage::note_off(m_arp_out_channel, note), sink);
                  });
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
  bool m_detect_quantize = false;           // SHIFT staging: immediate by default
  std::uint8_t m_chord_detect_port = 0;     // input port feeding the detector
  InputZone m_input_zone[kMaxPorts] = {};   // Dxx: per-port harmony zone (kMelody default)
  ArpeggiatorEngine m_arp;                  // live keyboard arpeggiator
  bool m_arp_enabled = false;               // kArp: capture + play the input port
  std::uint8_t m_arp_in_port = 0;           // keyboard the arp listens to
  std::uint8_t m_arp_out_port = 0;          // where the arp plays
  std::uint8_t m_arp_out_channel = 0;       // 0-based
  NoteTracker m_tracker;
  OutScheduler<kSchedulerCapacity> m_scheduler;
  std::uint8_t m_clock_out_mask = 0;  // off by default; enabled via kClockOutMask
};

}  // namespace arrangrr
