#pragma once

#include <cstdint>

#include "chorddet/followed_context.hpp"  // ChordState, Producer (freestanding)
#include "common/midi/message.hpp"
#include "common/time.hpp"

// Core binary ABI (D26): the core never parses JSON or strings. The host
// resolves L1 string paths to these POD commands; the core emits POD events.
// Both directions are trivially copyable and cross the boundary on ring
// buffers or direct calls.
//
// ============================================================================
// v1 ABI — ADDITIVE-ONLY BASELINE, UNFROZEN for Phase 5 (owner, 2026-07-13).
// ----------------------------------------------------------------------------
// This command/event surface (node 0700) was locked append-only at the GUI
// freeze line (node 11720) through the Phase 3 extraction. The banner that
// used to sit here ("FROZEN v1 ABI BASELINE") is now STALE: the owner lifted
// the freeze for Phase 5 (docs/design/phase5-execution-plan.md, fork F3
// RESOLVED) -- `Op`/`Param`/`Command`/`OutEvent` may be reshaped wholesale for
// Phase-5 work, and `test_abi_frozen.cpp` is updated deliberately (not just
// appended) alongside a reshape, per that plan's gate discipline. The ONE
// carve-out: the in-flight Phase 3c extraction cutover still closes under the
// OLD additive-only discipline (see phase5-execution-plan.md's own note) --
// everything below this line is Phase-5-forward.
//
// Phase-5 Item #2 (docs/design/clip-primitive-design.md) is the first reshape
// spent from that budget: it adds a real `Boundary` field to `Command`
// (kImmediate/kNextBar/kNextNBars) and consolidates `kChordPlay`'s old
// `idx != 0` overload and `kStyleSwitch`'s old `c != 0` overload onto it
// (closes docs/design/hook-interface.md §0/item #3's "inconsistently spelled"
// finding) -- both fields still ride the exact SAME struct sizes as before
// (sizeof(Command) == 20, sizeof(OutEvent) == 16 unchanged: the reshape reuses
// what used to be alignment padding, not new bytes). Existing goldens stay
// byte-identical: Boundary's default (kImmediate == 0) reproduces each verb's
// pre-reshape effective behavior exactly (see chord_play/style_switch in
// engine.cpp).
//
// A BREAKING change of a value that already shipped in a RELEASED build
// (renumber, remove, re-semanticize, shrink, reorder) still bumps
// kProtocolVersion to 2 (version.hpp) -- the unfreeze lifts the in-tree,
// same-recompile discipline, not the "a released wire format never lies"
// discipline for anything actually shipped to a user.
// ============================================================================

namespace arrangrr {

enum class Op : std::uint8_t {
  kSet = 0,
  kDo = 1,
  kGet = 2,
};

// WHEN a `do` command takes effect relative to the transport's bar grid
// (Phase-5 Item #2, docs/design/clip-primitive-design.md decision 3): the
// single shared spelling for a "quantize-at-boundary" verb, consolidating
// three previously ad-hoc mechanisms (`kChordPlay`'s old `idx != 0` overload,
// `kStyleSwitch`'s old `c != 0` overload, and the new clip launch/stop/
// scene-quantize verbs below) onto ONE field instead of a fourth bespoke
// spelling (closes docs/design/hook-interface.md §0/item #3). The core still
// forces kImmediate whenever the transport is stopped for any verb where a
// queued command could never land without ticks (chord_play/style_switch/
// cmd_clip in engine.cpp all apply this the same way, unchanged from the
// pre-reshape behavior). kNextNBars generalizes "next bar" to "next N bars";
// N rides `Command::n_bars` (meaningful only for kNextNBars; 1 elsewhere).
enum class Boundary : std::uint8_t {
  kImmediate = 0,
  kNextBar = 1,
  kNextNBars = 2,
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
                           //     boundary = kImmediate (default) plays now;
                           //     kNextBar/kNextNBars stages it like a SHIFT
                           //     note (Phase-5 Item #2: retires the old
                           //     `idx != 0` overload -- idx is unused here now).
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
                           //     boundary = kImmediate forces a hard mid-bar
                           //     cut now; kNextBar defers to the next bar
                           //     while playing (stopped is always immediate
                           //     regardless -- a queued switch could never
                           //     land without ticks). Combined style +
                           //     section switch. (Phase-5 Item #2: retires
                           //     the old `c != 0`-is-immediate overload.)
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
  // Phase-5 Item #2 (docs/design/clip-primitive-design.md): the Repeat-Zone
  // launch primitive. `ClipMatrix` (arrangrr/clip/clip_matrix.hpp) is an
  // Engine-owned bounded POD pool; a clip is {TrackRole part_role,
  // scene_index, ContentKind, content_index} plus a runtime LaunchState --
  // never a pointer/variant. LIGHTER than the Looper (node 6000): arm/
  // launch/stop only, no record/overdub/capture.
  kClipAdd = 44,        // do: registers a new clip (host/script-only
                        //     plumbing -- the design's 3 launch/stop/
                        //     scene-quantize verbs need SOMETHING to
                        //     address; this mirrors kSeqNew's own
                        //     established convention of an implicit
                        //     SEQUENTIAL id with no return-value echo).
                        //     a = TrackRole part_role, b = scene_index,
                        //     c = ContentKind (low byte) |
                        //     (content_index << 8).
  kClipLaunch = 45,     // do: idx = clip id (ClipMatrix slot, assigned by
                        //     kClipAdd in registration order). boundary +
                        //     n_bars (kNextNBars only) decide when it
                        //     starts.
  kClipStop = 46,       // do: idx = clip id. boundary + n_bars decide
                        //     when it stops.
  kSceneQuantize = 47,  // do: idx = scene index. boundary + n_bars decide
                        //     when; launches every registered clip whose
                        //     scene_index matches (`launch scene <n>
                        //     quantize <q>`).
  // Phase-5 Item #9 (docs/phase5-design-reviews.md "Pad/Scene live ->
  // Performance"): pad banks (arrangrr/pad/pad_bank.hpp) are WRAPPER-ONLY --
  // each PadType fans out to an EXISTING verb (clip launch, an Arranger
  // section request, or a Performance recall); there is no new note/CC
  // emission engine. kPadAssign is host/script-only registration, mirroring
  // kClipAdd's own convention.
  kPadAssign = 48,          // do: idx = flat pad id (0..kMaxPads-1)
                            //     a = type | (mode << 8) | (sync << 16) | (pitch << 24)
                            //       (type: PadType, mode: PadMode, sync: Boundary,
                            //        pitch: PadPitch)
                            //     b = dest_port | (dest_channel << 8) | (n_bars << 16)
                            //       (dest_port/channel: RESERVED, not yet consumed by
                            //        any wrapper dispatch, see pad_bank.hpp; n_bars is
                            //        meaningful only when sync == kNextNBars)
                            //     c = source_idx | (source_aux << 24)
                            //       (source_idx meaning depends on `type`: ClipMatrix
                            //        clip id for kPhrase/kChord, scene index for
                            //        kSceneColumn, SectionType for kVariation/kFill,
                            //        PerformanceStore slot for kPerformance)
  kPadTrigger = 49,         // do: idx = flat pad id. Fires the pad per its OWN
                            //     assigned sync/n_bars (NOT this Command's own
                            //     boundary/n_bars, which are unused here) --
                            //     kHold launches (release stops); kToggle flips
                            //     launched/stopped; kOneShot/kLoop launch.
  kPadRelease = 50,         // do: idx = flat pad id. Ends a kHold pad's sounding
                            //     content; no-op for every other PadMode and for
                            //     kVariation/kFill/kPerformance pad types (they
                            //     have no reverse action).
  kPerformanceStore = 51,   // do: idx = slot (0..kMaxPerformances-1). Captures the
                            //     live rig (style/variation/routes/mute/solo/
                            //     groove/tempo/key/chord-mode/chord-follow/
                            //     playing chord-sequence) into the PerformanceStore.
  kPerformanceRecall = 52,  // do: idx = slot. Atomically applies a stored
                            //     Performance (validates every referenced id
                            //     FIRST; applies nothing on any failure).
                            //     boundary == kImmediate (or the transport
                            //     stopped) applies now; kNextBar/kNextNBars
                            //     arms a BoundaryLatch that lands the WHOLE
                            //     recall at the bar boundary (Engine::on_tick,
                            //     AFTER fire_clips/BEFORE fire_arranger).
  // Phase-5 Item #10 (docs/phase5-design-reviews.md "MIDI-FX insert chain",
  // node 5100/5200): the chain is addressed PER-ROLE (arrangrr/fx/
  // insert_chain.hpp's InsertChain, one per TrackRole -- the SAME ordinal
  // space as Arranger::m_routes -- NOT per Timeline Track; the owner's
  // 2026-07-14 concrete-shape review corrected the RESERVED block below,
  // which had drafted "per-track"). Grafted into Arranger::on_tick's D40
  // pipeline BEFORE groove::apply: each chain-produced fan-out note gets
  // groove recomputed at its OWN grid position, never the seed's (Corelli
  // must-fix -- preserves D16 determinism, existing goldens stay
  // byte-identical for an unconfigured/passthrough chain).
  kFxSet = 53,     // set: idx = TrackRole. a = slot (0..kMaxInserts-1),
                   //     b = InsertType. Re-activates the slot with that
                   //     type's fresh default params (drops any stale bits
                   //     left by a previous type in the slot's union).
  kFxParam = 54,   // set: idx = TrackRole. a = slot, b = param id (meaning
                   //     depends on the slot's CURRENT type -- see
                   //     InsertChain::set_param), c = value (clamped to the
                   //     field's own width, u8 or u16).
  kFxEnable = 55,  // set: idx = TrackRole. a = slot, b = 0/1.
  kFxClear = 56,   // do: idx = TrackRole. a = slot, or -1 = the whole chain.
  // Phase-6 Theme 3 Item #1 (docs/reflections/phase6-theme3-master-transpose-
  // scope.md, Decisions 1/2/3/5): a signed semitone offset applied LATE, to
  // the already-resolved ABSOLUTE note number, at the two places an absolute
  // note is born -- Arranger::resolve()'s kInterval/kScaleDegree/kChordTone
  // branches (kFixed roles/drums stay exempt for free) and ChordEngine::
  // sound() (so the band and the chord you press move together). The
  // detected chord root and Timeline step-track literal notes are untouched
  // by design.
  kMasterTranspose = 57,  // set: a = semitones, clamped/rejected outside
                          //     [-12, +12]. 0 is a no-op (the default).
};

// ============================================================================
// RESERVED — MIDI-FX / Transform chain, FUTURE growth (node 5100/5200).
// ----------------------------------------------------------------------------
// Phase-5 Item #10 implemented the chain CORE: kFxSet/kFxParam/kFxEnable/
// kFxClear above are live, addressed PER-ROLE (arrangrr/fx/insert_chain.hpp's
// InsertChain lives one-per-TrackRole -- corrected from this block's earlier
// "per-track" draft). STILL deferred by that item's locked v1 scope: FX
// persistence inside Performance (arrangrr/perf/performance.hpp does not
// carry the chain), and on_tick capability on Insert (groove-as-insert,
// arp-as-insert) -- either would land as a future format_version bump /
// additive ABI append, never a silent reshape of what is live above.
//
// kMaxInserts is the on-disk / ABI format maximum number of chain slots per
// role (the UI intentionally exposes only 4). Stable ABI surface.
inline constexpr std::uint16_t kMaxInserts = 8;
// ============================================================================

struct Command {
  Op op = Op::kDo;
  // Phase-5 Item #2 reshape: occupies what used to be alignment padding
  // between `op` and `param` (uint8_t then a 1-byte pad) -- sizeof(Command)
  // stays 20, unchanged (see the ABI banner at the top of this file).
  Boundary boundary = Boundary::kImmediate;
  Param param = Param::kNone;
  std::uint16_t idx = 0;  // collection index (D26): track, route, ... target
  // N for Boundary::kNextNBars (1..255); meaningless (and ignored) otherwise.
  // Also occupies former alignment padding (between `idx` and `a`) -- no size
  // change.
  std::uint8_t n_bars = 1;
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
  kSeqEmpty = 8,     // play/record on a sequence with no usable content
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
    // Phase-5 Item #2 (docs/design/clip-primitive-design.md): a ClipMatrix
    // cell's launch-state changed -- which cell is armed/playing/stopped.
    // Fired both when a launch/stop lands IMMEDIATELY (Command::boundary ==
    // kImmediate) and, for a queued one, TWICE: once announcing the armed/
    // queued-stop state right away, and again when ClipMatrix::on_bar
    // promotes it at the quantize boundary (engine.cpp's fire_clips).
    //   code       = clip id (ClipMatrix slot)
    //   msg.status = LaunchState (0 stopped, 1 armed, 2 playing, 3 queued-stop)
    kClip = 8,
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
    e.msg = MidiMessage{.status = root_note, .d1 = count, .d2 = vel};
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
  // Packs a kClip event (Phase-5 Item #2): `id` rides `code`, the numeric
  // LaunchState rides msg.status. Plain numeric packing like every other
  // factory here -- ClipMatrix's own LaunchState enum is a core-internal
  // type (arrangrr/clip/clip_matrix.hpp); the caller casts to std::uint8_t
  // the same way chord_out()/cmd_style() cast their own enums before calling
  // chord()/section() above.
  static constexpr OutEvent clip(std::uint16_t id, std::uint8_t state, Tick t) noexcept {
    OutEvent e;
    e.kind = Kind::kClip;
    e.code = id;
    e.msg = MidiMessage{.status = state, .d1 = 0, .d2 = 0};
    e.tick = t;
    return e;
  }
};
static_assert(sizeof(OutEvent) <= 16);

}  // namespace arrangrr
