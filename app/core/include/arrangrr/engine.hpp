#pragma once

#include <cstdint>

#include "arrangrr/abi.hpp"
#include "arrangrr/common/function_ref.hpp"
#include "arrangrr/common/span.hpp"
#include "arrangrr/common/time.hpp"
#include "arrangrr/config.hpp"
#include "arrangrr/midi/parser.hpp"
#include "arrangrr/routing/note_tracker.hpp"
#include "arrangrr/routing/router.hpp"
#include "arrangrr/scheduler/out_scheduler.hpp"
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
        sink(OutEvent::transport(static_cast<std::uint16_t>(transport_.state()), now_));
        break;
      case Param::kTransportStop:
        transport_.stop();
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
  NoteTracker tracker_;
  OutScheduler<kSchedulerCapacity> scheduler_;
  std::uint8_t clock_out_mask_ = 0;  // off by default; enabled via kClockOutMask
};

}  // namespace arrangrr
