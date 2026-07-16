#pragma once

#include <cstdint>
#include <vector>

#include "common/midi/message.hpp"
#include "common/time.hpp"

// A minimal, host-only Standard MIDI File WRITER — the counterpart to
// smf.hpp's parser. Emits a single-track (type-0) SMF: MThd + one MTrk,
// correct VLQ delta-times, a full status byte per event (no running status —
// simplicity over size; running status is optional per the SMF spec) and a
// trailing End-of-Track meta. Exists so a host session's own MIDI output
// (Restyle, Motif, ...) can be dumped to a .mid file and auditioned in any
// DAW/synth (phase-5 infrastructure — closes the by-ear-validation gap).

namespace arrstyle {

// One outgoing MIDI event ready to be serialized: an absolute tick (same
// resolution as arrangrr::kPpqn, D27) plus the raw channel-voice message.
// Mirrors the shape the core's OutEvent::midi / midisrc::SourceEvent already
// carry (tick + MidiMessage) — deliberately re-declared here rather than
// pulling in midi_source_stage.hpp's heavier scheduler/template machinery for
// what is otherwise a pure byte-serializer.
struct SmfEvent {
  arrangrr::Tick tick = 0;
  arrangrr::MidiMessage msg{};
};

// Serializes `events` into a type-0 Standard MIDI File. `events` must already
// be in non-decreasing tick order (ties keep the input's relative order,
// mirroring a stable merge) — the writer does not sort; an out-of-order tick
// is clamped to a zero delta rather than producing a corrupt (underflowed)
// VLQ. `division` sets the header's ticks-per-quarter-note (default:
// arrangrr::kPpqn, so tick values the caller already has need no rescaling).
// Only channel-voice/system-common messages are supported — exactly the
// shapes MidiMessage itself can represent (no SysEx payload, per its own
// header comment). System Realtime bytes (Clock/Start/Continue/Stop/...,
// arrangrr::midi::is_realtime) are silently DROPPED: they have no place in a
// Standard MIDI File track (playback regenerates transport, never stores it)
// and parse_smf itself does not model them as a channel-voice message — a
// clock-out-enabled live session's OutEvent::midi stream carries these.
std::vector<std::uint8_t> write_smf(
    const std::vector<SmfEvent>& events,
    std::uint16_t division = static_cast<std::uint16_t>(arrangrr::kPpqn));

}  // namespace arrstyle
