#pragma once

#include <cstdint>

#include "arrangrr/abi.hpp"
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

  constexpr Tick now() const noexcept { return now_; }
  constexpr const Transport& transport() const noexcept { return transport_; }
  const Timeline& timeline() const noexcept { return timeline_; }
  const ChordEngine& chords() const noexcept { return chords_; }
  const ChordSequencer& sequences() const noexcept { return seq_; }

  // Feeds raw MIDI bytes from an input port. Parsed messages are routed and
  // scheduled at the current tick; due events are flushed to the sink at the
  // end of the batch.
  void push_midi_in(std::uint8_t port, Span<const std::uint8_t> bytes, EventSink sink) {
    if (port >= kMaxPorts) return;
    for (std::uint8_t byte : bytes) {
      parsers_[port].feed(byte, [&](const MidiMessage& msg) {
        router_.route(port, msg, [&](std::uint8_t out_port, const MidiMessage& routed) {
          schedule_or_warn(out_port, now_, routed, sink);
        });
      });
    }
    flush(sink);
  }

  // Applies one binary command (D26). Sink receives any resulting events.
  void push_command(const Command& cmd, EventSink sink) {
    switch (cmd.param) {
      case Param::kTransportTempo:
        if (!transport_.set_bpm(static_cast<BpmX100>(cmd.a)))
          sink(OutEvent::warn(WarnCode::kBadArgument, now_));
        break;
      case Param::kTransportStart:
        transport_.start();
        emit_realtime(midi::kStart, sink);
        // MIDI convention: the first F8 follows FA immediately — the slave's
        // beat zero is this clock, not one period later.
        emit_realtime(midi::kClock, sink);
        if (seq_.playing()) (void)seq_.play(0);  // rebase to the new tick 0
        fire_timeline(0, sink);  // grid step 0 plays on start, like the F8
        fire_chord_seq(0, sink);
        flush(sink);
        sink(OutEvent::transport(static_cast<std::uint16_t>(transport_.state()), now_));
        break;
      case Param::kTransportStop:
        transport_.stop();
        if (seq_.playing()) {
          chords_.release([&](std::uint8_t port, const MidiMessage& msg) {
            schedule_or_warn(port, now_, msg, sink);
          });
          flush(sink);
        }
        emit_realtime(midi::kStop, sink);
        sink(OutEvent::transport(static_cast<std::uint16_t>(transport_.state()), now_));
        break;
      case Param::kTransportContinue:
        transport_.resume();
        emit_realtime(midi::kContinue, sink);
        sink(OutEvent::transport(static_cast<std::uint16_t>(transport_.state()), now_));
        break;
      case Param::kPanic:
        tracker_.panic([&](std::uint8_t port, const MidiMessage& msg) {
          schedule_or_warn(port, now_, msg, sink);
        });
        flush(sink);
        break;
      case Param::kRouteAdd: {
        const Route route{
            .in_port = static_cast<std::uint8_t>(cmd.a & 0xFF),
            .in_channel = static_cast<std::int8_t>((cmd.a >> 8) & 0xFF),
            .out_port = static_cast<std::uint8_t>(cmd.b & 0xFF),
            .out_channel = static_cast<std::int8_t>((cmd.b >> 8) & 0xFF),
            .pass = static_cast<std::uint8_t>(cmd.c & 0xFF),
        };
        if (route.in_port >= kMaxPorts || route.out_port >= kMaxPorts) {
          sink(OutEvent::warn(WarnCode::kBadArgument, now_));
        } else if (!router_.add(route)) {
          sink(OutEvent::warn(WarnCode::kRouteTableFull, now_));
        }
        break;
      }
      case Param::kRouteClear:
        router_.clear();
        break;
      case Param::kClockOutMask:
        clock_out_mask_ = static_cast<std::uint8_t>(cmd.a & 0xFF);
        break;
      case Param::kSeqNew:
        if (seq_.add_sequence(chords_.key()) < 0) {
          sink(OutEvent::warn(WarnCode::kSeqTableFull, now_));
        }
        break;
      case Param::kSeqUse:
        if (!seq_.use(cmd.idx)) sink(OutEvent::warn(WarnCode::kBadArgument, now_));
        break;
      case Param::kSeqRec:
        if (!seq_.start_record(now_)) sink(OutEvent::warn(WarnCode::kSeqEmpty, now_));
        break;
      case Param::kSeqStop:
        if (seq_.recording()) {
          (void)seq_.stop_record(now_, cmd.a > 0 ? static_cast<Tick>(cmd.a) : kTicksPerBar);
        } else {
          seq_.stop_playback([&] {
            chords_.release([&](std::uint8_t port, const MidiMessage& msg) {
              schedule_or_warn(port, now_, msg, sink);
            });
          });
          flush(sink);
        }
        break;
      case Param::kSeqAdd: {
        ChordSequence* seq = seq_.current();
        const auto note = static_cast<std::uint8_t>(cmd.a & 0x7F);
        const std::int8_t quality_ovr = static_cast<std::int8_t>((cmd.b & 0xFF) - 1);
        const auto vel = static_cast<std::uint8_t>((cmd.b >> 8) & 0x7F);
        if (seq == nullptr || cmd.a < 0 || cmd.a > 127 || cmd.c <= 0 || vel == 0 ||
            quality_ovr >= static_cast<std::int8_t>(kQualityCount)) {
          sink(OutEvent::warn(WarnCode::kBadArgument, now_));
          break;
        }
        const int degree =
            theory::degree_of(seq->key, static_cast<std::uint8_t>(note % 12));
        if (degree < 0) {
          sink(OutEvent::warn(WarnCode::kNotInKey, now_));
        } else if (!seq->append(static_cast<std::int8_t>(degree), quality_ovr, vel,
                                static_cast<Tick>(cmd.c))) {
          sink(OutEvent::warn(WarnCode::kBadArgument, now_));
        }
        break;
      }
      case Param::kSeqLoop:
        if (ChordSequence* seq = seq_.current(); seq != nullptr) {
          seq->loop = cmd.a != 0;
        } else {
          sink(OutEvent::warn(WarnCode::kBadArgument, now_));
        }
        break;
      case Param::kSeqPlay:
        if (!seq_.play(transport_.tick())) {
          sink(OutEvent::warn(WarnCode::kSeqEmpty, now_));
        } else if (transport_.playing()) {
          fire_chord_seq(transport_.tick(), sink);
          flush(sink);
        }
        break;
      case Param::kSeqTranspose:
        if (ChordSequence* seq = seq_.current(); seq == nullptr) {
          sink(OutEvent::warn(WarnCode::kBadArgument, now_));
        } else if (cmd.a >= 0) {
          if (cmd.a > 11 || cmd.b >= kModeCount) {
            sink(OutEvent::warn(WarnCode::kBadArgument, now_));
          } else {
            seq->transpose_to(static_cast<std::uint8_t>(cmd.a),
                              static_cast<std::int8_t>(cmd.b));
          }
        } else {
          seq->transpose_by(static_cast<std::int8_t>(cmd.c));
        }
        break;
      case Param::kSeqDel:
        if (ChordSequence* seq = seq_.current();
            seq == nullptr || !seq->remove(static_cast<std::size_t>(cmd.a))) {
          sink(OutEvent::warn(WarnCode::kBadArgument, now_));
        }
        break;
      case Param::kSeqClear:
        if (ChordSequence* seq = seq_.current(); seq != nullptr) {
          seq->clear();
        } else {
          sink(OutEvent::warn(WarnCode::kBadArgument, now_));
        }
        break;
      case Param::kTrackNew: {
        const auto port = static_cast<std::uint8_t>(cmd.b & 0xFF);
        const auto channel = static_cast<std::uint8_t>((cmd.b >> 8) & 0xFF);
        if (port >= kMaxPorts || channel > 15 ||
            cmd.a > static_cast<std::int32_t>(TrackRole::kCc)) {
          sink(OutEvent::warn(WarnCode::kBadArgument, now_));
        } else if (timeline_.add_track(static_cast<TrackRole>(cmd.a), port, channel) < 0) {
          sink(OutEvent::warn(WarnCode::kTrackTableFull, now_));
        }
        break;
      }
      case Param::kTrackStep: {
        const auto note = static_cast<std::uint8_t>(cmd.b & 0xFF);
        const auto vel = static_cast<std::uint8_t>((cmd.b >> 8) & 0xFF);
        if (!timeline_.set_step(cmd.idx, static_cast<std::size_t>(cmd.a), note, vel,
                                static_cast<std::uint16_t>(cmd.c))) {
          sink(OutEvent::warn(WarnCode::kBadArgument, now_));
        }
        break;
      }
      case Param::kTrackLength:
        if (!timeline_.set_length(cmd.idx, static_cast<std::size_t>(cmd.a))) {
          sink(OutEvent::warn(WarnCode::kBadArgument, now_));
        }
        break;
      case Param::kKeySet:
        if (cmd.a < 0 || cmd.a > 11 || cmd.b < 0 || cmd.b >= kModeCount) {
          sink(OutEvent::warn(WarnCode::kBadArgument, now_));
        } else {
          chords_.set_key(Key{static_cast<std::uint8_t>(cmd.a), static_cast<Mode>(cmd.b)});
        }
        break;
      case Param::kChordOut: {
        const auto port = static_cast<std::uint8_t>(cmd.a & 0xFF);
        const auto channel = static_cast<std::uint8_t>((cmd.a >> 8) & 0xFF);
        if (port >= kMaxPorts || channel > 15) {
          sink(OutEvent::warn(WarnCode::kBadArgument, now_));
        } else {
          chords_.set_output(port, channel);
        }
        break;
      }
      case Param::kChordHold:
        chords_.set_hold(cmd.a != 0);
        break;
      case Param::kChordMode:
        if (cmd.a < 0 || cmd.a >= kChordModeCount) {
          sink(OutEvent::warn(WarnCode::kBadArgument, now_));
        } else {
          chords_.set_mode(static_cast<ChordMode>(cmd.a));
        }
        break;
      case Param::kChordPlay: {
        const auto vel = static_cast<std::uint8_t>(cmd.c);
        // Up to 4 packed notes, zero-terminated (one per byte).
        std::uint8_t notes[4];
        std::uint8_t note_count = 0;
        for (int i = 0; i < 4; ++i) {
          const auto n = static_cast<std::uint8_t>((cmd.a >> (8 * i)) & 0xFF);
          if (n == 0) break;
          if (n > 127) {
            note_count = 0;
            break;
          }
          notes[note_count++] = n;
        }
        if (note_count == 0 || vel == 0 || vel > 127 || cmd.b >= kQualityCount) {
          sink(OutEvent::warn(WarnCode::kBadArgument, now_));
          break;
        }
        const auto schedule = [&](std::uint8_t port, const MidiMessage& msg) {
          schedule_or_warn(port, now_, msg, sink);
        };
        ChordResult r;
        switch (chords_.mode()) {
          case ChordMode::kSingle:
            r = chords_.play_single(notes[0], static_cast<std::int8_t>(cmd.b), vel,
                                    schedule);
            break;
          case ChordMode::kShell:
            r = chords_.play_shell(notes, note_count, static_cast<std::int8_t>(cmd.b),
                                   vel, schedule);
            break;
          case ChordMode::kDiatonic:
          default:
            r = chords_.play(notes[0], static_cast<std::int8_t>(cmd.b), vel, schedule);
            break;
        }
        if (r.degree < 0) {
          sink(OutEvent::warn(WarnCode::kNotInKey, now_));
          break;
        }
        // Recording captures only diatonic degrees (D28 functional storage);
        // keyless modes record when the root happens to fit the seq key.
        if (seq_.recording()) {
          const int deg =
              r.degree == static_cast<std::int8_t>(kNoDegree)
                  ? theory::degree_of(seq_.current()->key,
                                      static_cast<std::uint8_t>(r.root_note % 12))
                  : r.degree;
          if (deg >= 0) {
            const std::int8_t ovr =
                r.degree == static_cast<std::int8_t>(kNoDegree)
                    ? static_cast<std::int8_t>(r.quality)  // pin the resolved quality
                    : static_cast<std::int8_t>(cmd.b);
            seq_.capture(now_, static_cast<std::int8_t>(deg), ovr, vel);
          } else {
            sink(OutEvent::warn(WarnCode::kNotInKey, now_));
          }
        }
        sink(OutEvent::chord(chords_.out_port(), static_cast<std::uint8_t>(r.degree),
                             static_cast<std::uint8_t>(r.quality), r.root_note,
                             r.shape.count, vel, now_));
        flush(sink);
        break;
      }
      case Param::kChordStop:
        chords_.release([&](std::uint8_t port, const MidiMessage& msg) {
          schedule_or_warn(port, now_, msg, sink);
        });
        flush(sink);
        break;
      case Param::kTrackMute:
      case Param::kTrackSolo: {
        Track* t = timeline_.track(cmd.idx);
        if (t == nullptr) {
          sink(OutEvent::warn(WarnCode::kBadArgument, now_));
        } else if (cmd.param == Param::kTrackMute) {
          t->mute = cmd.a != 0;
        } else {
          t->solo = cmd.a != 0;
        }
        break;
      }
      default:
        sink(OutEvent::warn(WarnCode::kUnknownCommand, now_));
        break;
    }
  }

  // Schedules a message on the stream timeline at an absolute tick (used by
  // the host for @tick-scheduled script lines).
  void schedule_at(std::uint8_t port, Tick tick, const MidiMessage& msg, EventSink sink) {
    schedule_or_warn(port, tick, msg, sink);
    flush(sink);  // fire immediately if already due
  }

  // Advances stream time by `n` ticks, firing due events in D29 total order.
  void advance_ticks(std::uint32_t n, EventSink sink) {
    for (std::uint32_t i = 0; i < n; ++i) {
      ++now_;
      if (transport_.playing()) {
        transport_.advance_one();
        if (clock_out_mask_ != 0 && Transport::is_midi_clock_tick(transport_.tick())) {
          for (std::uint8_t p = 0; p < kMaxPorts; ++p) {
            if (clock_out_mask_ & (1u << p)) {
              schedule_or_warn(p, now_, MidiMessage::realtime(midi::kClock), sink);
            }
          }
        }
        fire_timeline(transport_.tick(), sink);
        fire_chord_seq(transport_.tick(), sink);
      }
      flush(sink);
    }
  }

 private:
  void schedule_or_warn(std::uint8_t port, Tick tick, const MidiMessage& msg, EventSink sink) {
    if (!scheduler_.schedule(port, tick, msg)) {
      sink(OutEvent::warn(WarnCode::kSchedulerFull, now_));
    }
  }

  // Transport realtime bytes (FA/FB/FC) go out immediately on clock ports.
  void emit_realtime(std::uint8_t status, EventSink sink) {
    for (std::uint8_t p = 0; p < kMaxPorts; ++p) {
      if (clock_out_mask_ & (1u << p)) {
        schedule_or_warn(p, now_, MidiMessage::realtime(status), sink);
      }
    }
    flush(sink);
  }

  void fire_timeline(Tick transport_tick, EventSink sink) {
    timeline_.on_tick(transport_tick,
                      [&](std::uint8_t port, TickOffset delay, const MidiMessage& msg) {
                        schedule_or_warn(port, now_ + static_cast<Tick>(delay), msg, sink);
                      });
  }

  void fire_chord_seq(Tick transport_tick, EventSink sink) {
    seq_.on_tick(
        transport_tick,
        [&](std::uint8_t root_note, ChordQuality quality, std::uint8_t degree,
            std::uint8_t vel) {
          chords_.sound(root_note, quality, vel,
                        [&](std::uint8_t port, const MidiMessage& msg) {
                          schedule_or_warn(port, now_, msg, sink);
                        });
          sink(OutEvent::chord(chords_.out_port(), degree,
                               static_cast<std::uint8_t>(quality), root_note,
                               theory::shape_of(quality).count, vel, now_));
        },
        [&] {
          chords_.release([&](std::uint8_t port, const MidiMessage& msg) {
            schedule_or_warn(port, now_, msg, sink);
          });
        });
  }

  void flush(EventSink sink) {
    scheduler_.pop_due(now_, [&](const ScheduledEvent& ev) {
      tracker_.observe(ev.port, ev.msg);
      sink(OutEvent::midi(ev.port, ev.msg, ev.tick));
    });
  }

  Tick now_ = 0;
  Transport transport_;
  MidiParser parsers_[kMaxPorts];
  Router router_;
  Timeline timeline_;
  ChordEngine chords_;
  ChordSequencer seq_;
  NoteTracker tracker_;
  OutScheduler<kSchedulerCapacity> scheduler_;
  std::uint8_t clock_out_mask_ = 0;  // off by default; enabled via kClockOutMask
};

}  // namespace arrangrr
