#pragma once

#include <cstdint>

#include "chorddet/followed_context.hpp"  // ChordState, Producer (freestanding)
#include "common/time.hpp"
#include "common/midi/message.hpp"

// Core binary ABI (D26): the core never parses JSON or strings. The host
// resolves L1 string paths to these POD commands; the core emits POD events.
// Both directions are trivially copyable and cross the boundary on ring
// buffers or direct calls.
//
// ============================================================================
// FROZEN v1 ABI BASELINE — locked at the GUI freeze line (node 11720).
// ----------------------------------------------------------------------------
// This command/event surface (node 0700) is the STABLE v1 baseline the host GUI
// (node 11600) is built against. The single invariant is ADDITIVE-ONLY:
//   * Existing enumerator values are STABLE FOREVER. Never renumber, reuse,
//     remove, or re-semanticize an id that already ships. A shipped id keeps its
//     number and its meaning for the entire life of protocol v1.
//   * Growth is ONLY by APPENDING new enumerators at the end. Next free ids:
//     Param = 44, OutEvent::Kind = 8, WarnCode = 10 (== kWarnCodeCount).
//   * Command/OutEvent field order, types, and size are stable. New data must
//     ride existing reserved bits/fields or an APPENDED field, guarded by the
//     size static_asserts below. Never reorder or resize an existing field.
//   * A BREAKING change (renumber, remove, re-semanticize, shrink, reorder)
//     requires bumping kProtocolVersion to 2 (version.hpp) — never an in-place
//     edit of v1.
// The frozen values are pinned by test_abi_frozen.cpp; that test fails the build
// the instant this invariant is violated. If it fails, APPEND — do not edit.
// ----------------------------------------------------------------------------
// Phase 3a growth (docs/design/orchestrator-pipeline-extraction.md §17.3b):
// OutEvent::Kind::kParamState (id=7) is the first APPENDED Kind since the
// freeze — it rides the SAME 16-byte layout unchanged (no new field, no
// resize), additive-only exactly as the invariant above requires.
//
// Phase 3 growth (docs/design/orchestrator-pipeline-extraction.md §17.3a):
// Param::kNoteRaw (id=43) is the wire shape for a client-driven note gesture
// (piano/chords key -> note-on/off) that has no other L1-text equivalent —
// it is APPENDED, rides the SAME 20-byte Command layout unchanged (no new
// field, no resize), additive-only exactly as the invariant above requires.
// ============================================================================

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
                           //     c = gate in scheduler ticks.
                           //     Param-locks (opt-in, bit 31 of c set): also
                           //     b |= probability << 16 | ratchet << 24 |
                           //          tie << 28; c |= (micro & 0xFF) << 16.
                           //     micro is a FORWARD-only lay-back (0..127 ticks);
                           //     anticipation is deferred (needs step look-ahead).
                           //     Bit 31 clear => the neutral short form above.
  kTrackLength = 11,       // set: idx = track; a = steps (1..kMaxStepsPerTrack)
  kTrackMute = 12,         // set: idx = track; a = 0/1
  kTrackSolo = 13,         // set: idx = track; a = 0/1
  kKeySet = 14,            // set: a = root pitch class (0..11), b = Mode
  kChordPlay = 15,         // do: a = up to 4 packed notes, one per byte,
                           //         zero-terminated (single note == low byte)
                           //     b = quality override (-1 = smart/D19)
                           //     c = velocity (1..127)
                           //     idx = 0 immediate (default), != 0 SHIFT-quantized
                           //           (staged to the next bar like a shift note)
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
  kStyleSwitch = 33,       // do: a = builtin style index, b = SectionType,
                           //     c = immediate (0 = next bar while playing,
                           //     else a hard mid-bar cut; stopped is always
                           //     immediate). Combined style + section switch.
  kChordDetect = 34,       // set: a = 0/1 (live piano->chord detection: held
                           //     notes on the input port re-harmonize the
                           //     arranger, chord-memory hold-last), b = input
                           //     port. Distinct from the reserved kChordHold.
  kProgram = 35,           // set: a = GM program (0..127), b = port |
                           //     (channel_0based << 8). Sends a Program Change
                           //     so the arrangrr picks the voice, not just the
                           //     external synth.
  kPartMute = 36,          // set: a = TrackRole, b = 0/1. Live mute of one
                           //     arranger style part (the `parts` mixer).
  kPartSolo = 37,          // set: a = TrackRole, b = 0/1. Solo: when any part
                           //     is soloed, only soloed parts play.
  kGroove = 38,            // set: a = GrooveField (swing/humanize/accent/grid/
                           //     quantize/seed), b = value. The global groove feel.
  kArp = 39,               // set: a = ArpField (enabled/rate/direction/octaves/
                           //     gate/latch/seed), b = value. Live arpeggiator.
  kArpOut = 40,            // set: a = port | (channel_0based << 8). Where the
                           //     arpeggiator plays.
  kChordFollow = 41,       // set: a = ChordFollow (0 auto, 1 detect, 2 sequencer,
                           //     3 manual, 4 live-priority). Selects WHICH producer
                           //     may update the arranger-followed chord context.
                           //     kAuto keeps the legacy last-writer-wins behavior;
                           //     kLivePriority (engine default) lets a held live
                           //     chord beat a running sequencer. Values append-only.
  kInputZone = 42,         // set: a = input port, b = InputZone (0 melody, 1
                           //     harmony). kHarmony suppresses the port's note
                           //     output (silent chord recognition, the Split
                           //     zone); kMelody routes/sounds (default). Rides
                           //     the same parsed input path; the detect port
                           //     (kChordDetect) still decides who OBSERVES.
  kNoteRaw = 43,           // do: idx = input port (low byte) | (channel_0based
                           //     << 8, high byte); a = MIDI note (0..127);
                           //     b = velocity (1..127, ignored for note-off);
                           //     c = 1 note-on / 0 note-off. Wire shape for a
                           //     client-driven note gesture (the `note` L1
                           //     verb, docs/design/orchestrator-pipeline-
                           //     extraction.md §17.3a) -- a pure client's
                           //     equivalent of what surface_send_note()
                           //     already builds in-process. The host
                           //     translates this DIRECTLY into the same
                           //     3-byte MIDI note-on/off message and feeds it
                           //     through feed_midi(); it never reaches
                           //     Engine::push_command().
};

// ============================================================================
// RESERVED — MIDI-FX / Transform chain (node 5000/5100). NOT YET IMPLEMENTED.
// ----------------------------------------------------------------------------
// The SHAPE of the insert-chain ABI is pre-fixed at the GUI freeze line (node
// 11720) so the GUI (node 11600) is born aware of this surface and is not
// rebuilt when node 5000 lands. This increment assigns NO live enum values
// (ABI-none): kMaxInserts below is the only committed symbol; the kFx... Param
// ids described here do NOT exist yet and MUST NOT be added until node 5000 is
// implemented, at which point they are APPENDED as new Param enumerators (next
// free id = 43), honoring the additive-only freeze above.
//
// Chain model: a bounded chain of MIDI transforms, per-track first (per-zone is
// deferred). On disk / on the ABI the chain holds up to kMaxInserts slots; the
// UI exposes 4. When the verbs are appended, each future kFx... command is
// addressed as:
//     idx = track index
//     a   = insert slot (0 .. kMaxInserts-1)
//     b   = insert type + per-insert flags (e.g. on/off, order)
//     c   = insert parameter value
// Anticipated (RESERVED, unassigned) verbs, to append when node 5000 lands:
//     kFxSet    — set the insert type in a slot (a = slot, b = insert type)
//     kFxParam  — set an insert parameter (a = slot, b = param id, c = value)
//     kFxEnable — toggle an insert on/off (a = slot, b = 0/1)
//     kFxClear  — clear a slot / the whole chain (a = slot, -1 = all)
// These names/argument packings are documentation only for this increment.
//
// kMaxInserts is the on-disk / ABI format maximum number of chain slots per
// track (the UI intentionally exposes only 4). It is stable ABI surface even
// though the chain body is unimplemented.
inline constexpr std::uint16_t kMaxInserts = 8;
// ============================================================================

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
  kUnsupported = 9,  // parameter reserved by the ABI but not implemented yet
};
inline constexpr std::uint16_t kWarnCodeCount = 10;

// Event from core to host.
struct OutEvent {
  enum class Kind : std::uint8_t {
    kMidi = 0,       // msg on port, at tick
    kTransport = 1,  // code = TransportState
    kWarn = 2,       // code = WarnCode
    kChord = 3,      // code = degree | (ChordQuality << 8);
                     // msg = {input root note, chord tone count, velocity}
    kSection = 4,    // code = SectionType (arranger section change)
    // The followed harmonic context changed (from ANY producer):
    //   msg.status = cur  (root_pc | quality << 4)
    //   msg.d1     = next (root_pc | quality << 4)
    //   msg.d2     = cur.valid | next.valid << 1 | Producer << 2
    kChordFollowed = 5,
    // Transport heartbeat (P0-2): fires once per 24-PPQN clock pulse while the
    // transport plays, regardless of MIDI clock-out routing -- a host/GUI
    // event, not a scheduled MIDI byte. Lets a client draw a moving playhead.
    //   code       = bar (1-based; low 16 bits -- caps at 65535 bars, fine
    //                for M0)
    //   msg.status = beat  (1-based, 1..kBeatsPerBar)
    //   msg.d1     = pulse (0..23, the sub-beat 24-PPQN pulse index)
    //   msg.d2     = reserved (0)
    kBeat = 6,
    // Phase 3a (docs/design/orchestrator-pipeline-extraction.md §17.3b): the
    // CURRENT value of a state-bearing Param that no other OutEvent echoes --
    // groove/arp fields, per-part mute/solo, the active style, chord-detect
    // enable+port, chord-follow, chord-mode, and the current key. A pure
    // client cannot reconstruct these panels without this echo (the "hole in
    // the wire" §17.2 traces). Reuses the ALREADY-STABLE `Param` enum (the
    // `code` field) as the domain tag instead of minting seven-to-nine new
    // Kinds -- additive-only, no new field, no resize. Emitted server-side
    // whenever the matching cmd_* mutates state, and once per field on a
    // client's initial connect (a "state dump", mirroring the state.dump
    // file-I/O precedent -- here it is just "replay every current value
    // once"). Per-Param field meaning (`port` and `msg.status`/`msg.d1`;
    // `msg.d2` stays RESERVED for future growth, mirroring kChordFollowed/
    // kBeat's own precedent above):
    //   kGroove       port=GrooveField   status,d1 = value (16-bit LE;
    //                                    GrooveField::kSeed is TRUNCATED to
    //                                    its low 16 bits -- a display/
    //                                    reproduction aid, not the full
    //                                    32-bit engine seed)
    //   kArp          port=ArpField      status,d1 = value (16-bit LE, same
    //                                    kSeed truncation as kGroove)
    //   kPartMute     port=TrackRole     status    = 0/1 (muted)
    //   kPartSolo     port=TrackRole     status    = 0/1 (soloed)
    //   kStyleLoad    port=0 (unused)    status,d1 = builtin style index
    //                                    (16-bit LE)
    //   kChordDetect  port=input port    status    = 0/1 (enabled)
    //   kChordFollow  port=0 (unused)    status    = ChordFollow
    //   kChordMode    port=0 (unused)    status    = ChordMode
    //   kKeySet       port=0 (unused)    status = root pitch class (0..11),
    //                                    d1 = Mode
    kParamState = 7,
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
  static constexpr OutEvent chord(std::uint8_t port, std::uint8_t degree, std::uint8_t quality,
                                  std::uint8_t root_note, std::uint8_t count, std::uint8_t vel,
                                  Tick t) noexcept {
    OutEvent e;
    e.kind = Kind::kChord;
    e.port = port;
    e.msg = MidiMessage{.status=root_note, .d1=count, .d2=vel};
    e.tick = t;
    e.code = static_cast<std::uint16_t>(degree | (quality << 8));
    return e;
  }
  // Packs the followed-context change (D24/D53): the current followed chord and
  // the staged/pending next chord, plus the producer that caused the change.
  // Each ChordState rides one byte (root_pc in the low nibble, quality in the
  // high nibble); the valid latches and the 2-bit Producer share msg.d2. code
  // stays 0. Purely numeric — labels/pitch-class masks are a HOST concern.
  static constexpr OutEvent chord_followed(ChordState cur, ChordState next, Producer src,
                                           Tick t) noexcept {
    OutEvent e;
    e.kind = Kind::kChordFollowed;
    e.msg =
        MidiMessage{.status = static_cast<std::uint8_t>(
                        (cur.root_pc & 0x0F) | (static_cast<std::uint8_t>(cur.quality) << 4)),
                    .d1 = static_cast<std::uint8_t>((next.root_pc & 0x0F) |
                                                    (static_cast<std::uint8_t>(next.quality) << 4)),
                    .d2 = static_cast<std::uint8_t>((cur.valid ? 0x1 : 0) | (next.valid ? 0x2 : 0) |
                                                    (static_cast<std::uint8_t>(src) << 2))};
    e.tick = t;
    return e;
  }
  // Packs the transport heartbeat (P0-2): bar rides `code` (low 16 bits,
  // 1-based, caps at 65535 bars); beat (1-based, 1..kBeatsPerBar) and pulse
  // (0..23, the 24-PPQN sub-beat index) ride msg.status/msg.d1. Purely
  // numeric packing -- labels/layout are a HOST/GUI concern, mirroring
  // chord_followed above.
  static constexpr OutEvent beat(std::uint32_t bar, std::uint8_t beat, std::uint8_t pulse,
                                 Tick t) noexcept {
    OutEvent e;
    e.kind = Kind::kBeat;
    e.code = static_cast<std::uint16_t>(bar);
    e.msg = MidiMessage{.status = beat, .d1 = pulse, .d2 = 0};
    e.tick = t;
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
  // Packs a kParamState echo (Phase 3a, §17.3b): `param` rides `code` as the
  // domain tag; `sub` is the per-Param role/field selector (0 when unused,
  // see the Kind::kParamState comment above for the exact per-Param table);
  // `v0`/`v1` are the value bytes (v1 is 0 for single-byte values; v0|v1<<8
  // forms a 16-bit value for the wider fields). Purely numeric packing, like
  // every other OutEvent factory here -- labels are a HOST concern.
  static constexpr OutEvent param_state(Param param, std::uint8_t sub, std::uint8_t v0,
                                        std::uint8_t v1, Tick t) noexcept {
    OutEvent e;
    e.kind = Kind::kParamState;
    e.code = static_cast<std::uint16_t>(param);
    e.port = sub;
    e.msg = MidiMessage{.status = v0, .d1 = v1, .d2 = 0};
    e.tick = t;
    return e;
  }
};
static_assert(sizeof(OutEvent) <= 16);

}  // namespace arrangrr
