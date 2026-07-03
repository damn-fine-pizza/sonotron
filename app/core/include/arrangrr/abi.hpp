#pragma once

#include <cstdint>

#include "arrangrr/midi/message.hpp"
#include "arrangrr/common/time.hpp"

// Core binary ABI (D26): the core never parses JSON or strings. The host
// resolves L1 string paths to these POD commands; the core emits POD events.
// Both directions are trivially copyable and cross the boundary on ring
// buffers or direct calls.

namespace arrangrr {

enum class Op : std::uint8_t {
  kSet = 0,
  kDo = 1,
  kGet = 2,
};

// Flat M0 parameter/action ids (the full L1 catalog grows with milestones;
// ids are stable — never reuse a value).
enum class Param : std::uint16_t {
  kNone = 0,
  kTransportTempo = 1,     // set: a = bpm_x100
  kTransportStart = 2,     // do
  kTransportStop = 3,      // do
  kTransportContinue = 4,  // do
  kPanic = 5,              // do
  kRouteAdd = 6,           // do: a = in_port | (in_ch & 0xFF) << 8
                           //     b = out_port | (out_ch & 0xFF) << 8
                           //     c = pass mask (route_pass::*)
  kRouteClear = 7,         // do
  kClockOutMask = 8,       // set: a = bitmask of ports that receive F8/FA/FB/FC
  kTrackNew = 9,           // do: a = role, b = port | (channel_0based << 8)
  kTrackStep = 10,         // do: idx = track; a = step index (0-based)
                           //     b = note | (vel << 8)  (vel 0 clears the slot)
                           //     c = gate in scheduler ticks
  kTrackLength = 11,       // set: idx = track; a = steps (1..kMaxStepsPerTrack)
  kTrackMute = 12,         // set: idx = track; a = 0/1
  kTrackSolo = 13,         // set: idx = track; a = 0/1
  kKeySet = 14,            // set: a = root pitch class (0..11), b = Mode
  kChordPlay = 15,         // do: a = up to 4 packed notes, one per byte,
                           //         zero-terminated (single note == low byte)
                           //     b = quality override (-1 = smart/D19)
                           //     c = velocity (1..127)
  kChordStop = 16,         // do
  kChordHold = 17,         // set: a = 0/1
  kChordOut = 18,          // set: a = port | (channel_0based << 8)
  kSeqNew = 19,            // do: new sequence, reference key = current key
  kSeqUse = 20,            // do: idx = sequence index
  kSeqRec = 21,            // do: start recording into the current sequence
  kSeqAdd = 22,            // do: a = root note (interpreted in the seq key)
                           //     b = (quality_ovr + 1) | (velocity << 8)
                           //     c = duration in ticks (free, D14)
  kSeqLoop = 23,           // set: a = 0/1
  kSeqPlay = 24,           // do: start playback at the current transport tick
  kSeqStop = 25,           // do: stop recording (quantize-after, a = grid or
                           //     0 = one bar) or, if not recording, playback
  kSeqTranspose = 26,      // set: a = new root pc (D28 re-derive) with
                           //     b = mode (-1 keep), or a = -1 with
                           //     c = relative semitones
  kSeqDel = 27,            // do: a = step index (0-based)
  kSeqClear = 28,          // do
  kChordMode = 29,         // set: a = ChordMode (0 diatonic, 1 single, 2 shell)
  kStyleLoad = 30,         // do: a = builtin style index
  kStyleSection = 31,      // do: a = SectionType (quantized to the next bar
                           //     while playing, immediate otherwise)
  kStyleRoute = 32,        // set: a = TrackRole, b = port | (channel << 8)
};

struct Command {
  Op op = Op::kDo;
  Param param = Param::kNone;
  std::uint16_t idx = 0;  // collection index (D26): track, route, ... target
  std::int32_t a = 0;
  std::int32_t b = 0;
  std::int32_t c = 0;
};
static_assert(sizeof(Command) <= 20);

enum class WarnCode : std::uint16_t {
  kNone = 0,
  kSchedulerFull = 1,
  kRouteTableFull = 2,
  kUnknownCommand = 3,
  kBadArgument = 4,
  kTrackTableFull = 5,
  kNotInKey = 6,  // chord input note is chromatic to the key (D20: strict)
  kSeqTableFull = 7,
  kSeqEmpty = 8,  // play/record on a sequence with no usable content
};

// Event from core to host.
struct OutEvent {
  enum class Kind : std::uint8_t {
    kMidi = 0,       // msg on port, at tick
    kTransport = 1,  // code = TransportState
    kWarn = 2,       // code = WarnCode
    kChord = 3,      // code = degree | (ChordQuality << 8);
                     // msg = {input root note, chord tone count, velocity}
    kSection = 4,    // code = SectionType (arranger section change)
  };

  Kind kind = Kind::kMidi;
  std::uint8_t port = 0;
  MidiMessage msg{};
  Tick tick = 0;
  std::uint16_t code = 0;

  static constexpr OutEvent midi(std::uint8_t port, const MidiMessage& m, Tick t) noexcept {
    OutEvent e;
    e.kind = Kind::kMidi;
    e.port = port;
    e.msg = m;
    e.tick = t;
    return e;
  }
  static constexpr OutEvent transport(std::uint16_t state, Tick t) noexcept {
    OutEvent e;
    e.kind = Kind::kTransport;
    e.code = state;
    e.tick = t;
    return e;
  }
  static constexpr OutEvent chord(std::uint8_t port, std::uint8_t degree,
                                  std::uint8_t quality, std::uint8_t root_note,
                                  std::uint8_t count, std::uint8_t vel, Tick t) noexcept {
    OutEvent e;
    e.kind = Kind::kChord;
    e.port = port;
    e.msg = MidiMessage{root_note, count, vel};
    e.tick = t;
    e.code = static_cast<std::uint16_t>(degree | (quality << 8));
    return e;
  }
  static constexpr OutEvent section(std::uint16_t type, Tick t) noexcept {
    OutEvent e;
    e.kind = Kind::kSection;
    e.code = type;
    e.tick = t;
    return e;
  }
  static constexpr OutEvent warn(WarnCode code, Tick t) noexcept {
    OutEvent e;
    e.kind = Kind::kWarn;
    e.code = static_cast<std::uint16_t>(code);
    e.tick = t;
    return e;
  }
};
static_assert(sizeof(OutEvent) <= 16);

}  // namespace arrangrr
