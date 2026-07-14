#pragma once

#include <cstdint>

#include "arrangrr/arranger/groove.hpp"       // GrooveParams, groove::apply
#include "arrangrr/arranger/style_model.hpp"  // Style, StyleSection, StylePattern, VoicingPolicy
#include "arrangrr/arranger/voicing.hpp"      // NoteReq, VoicingState
#include "arrangrr/config.hpp"                // kSchedulerCapacity
#include "arrangrr/timeline/timeline.hpp"     // TrackRole, kTicksPerStep
#include "chorddet/followed_context.hpp"      // FollowedContext
#include "chorddet/stage.hpp"                 // ChorddetStage
#include "chorddet/theory.hpp"                // Key, ChordState, theory::
#include "common/midi/message.hpp"
#include "common/time.hpp"
#include "runtime/midi_parser.hpp"
#include "runtime/out_scheduler.hpp"
#include "runtime/stage.hpp"

// Restyle (roadmap 9320), first slice. Read the two design reviews before
// touching this file: docs/design/restyle-musical-scope.md (Ottorino, musical
// scope/transform definition) and docs/design/restyle-placement.md (Corelli,
// structural placement) -- they dovetail and this class implements their
// consensus verbatim, not a re-interpretation of either.
//
// WHAT: transforms an imported melody's OWN notes into a target style's
// idiom -- rhythm (snap to the 16th grid, then the style's own groove::apply)
// and register/voicing (chord tones only, through the SAME VoicingState/
// VoicingPolicy machinery the Arranger's own comping already uses) -- while
// preserving WHICH functional tone each input note plays. This is NOT
// Accompany (9310, which lays a generated band UNDER the melody, verbatim):
// here the melody's OWN part is reshaped.
//
// THE ONE GENUINELY NEW PIECE: an inverse NTT classifier (restyle::classify,
// below) -- absolute input pitch -> chord-tone index / scale degree, against
// the live ChordState/Key. Reharmonization is explicitly OUT OF SCOPE for
// this first slice (restyle-musical-scope.md §2): a note that is neither a
// chord tone nor in-key (chromatic/passing) passes through at its ORIGINAL
// pitch, only requantized -- never reharmonized. That is a deliberate,
// conservative choice, not an oversight.
//
// PLACEMENT: a new declared Pipeline stage inside `components/arrangrr`
// (restyle-placement.md §1), inserted between ChorddetStage and Engine in the
// (grown) orchestrator::AccompanyPipeline. Inert by default -- push_midi_in is
// a no-op until load_style() succeeds (the `restyle <style>` L1 verb, host-
// side, mirrors `midi-source load`) -- so a pipeline that never calls it stays
// byte-identical to one without a RestyleStage at all (the same convention
// midisrc::MidiSourceStage/ChorddetStage already establish).
//
// STATE INJECTED BY REFERENCE (the same triple-injection idiom already
// sanctioned -- Transport/OutScheduler §14.3, FollowedContext §16.2c):
// OutScheduler<kSchedulerCapacity>& to schedule the restyled notes;
// ChorddetStage<kPorts>& for its `key()` (the scale-aware Key the same
// `key ...` command already seeds into the detector, restyle-musical-
// scope.md §0.3's classify step needs Key + ChordState, not ChordState
// alone); FollowedContext& for the live ChordState (the SAME object
// ChorddetStage writes and the Arranger reads, same-tick visible, D53).
// RestyleStage never STEERS the followed chord (it only reads it) and never
// re-harmonizes -- see the scope gate above.
//
// OWN VoicingState instance (restyle-placement.md's reuse map): a distinct
// voice-leading lineage from the generative Arranger's own -- they are
// different musical streams and must not share memory.
//
// Dual-target/freestanding: every buffer here is fixed-size (a 128-entry
// pending-note table, one MidiParser per port), no heap, matching
// VoicingState/Arranger's own discipline. This header lives under
// components/arrangrr/include, so arrangrr's PUBLIC -fno-exceptions/-fno-rtti
// apply to every dependent TU automatically; its only real-world producer
// (midisrc::MidiSourceStage) is HOST-ONLY, but this class itself stays
// portable so a future non-SMF, live-input restyle mode never needs a
// rewrite (restyle-placement.md §4).

namespace arrangrr::restyle {

// The inverse of Arranger::resolve() (arranger.hpp): classifies one absolute
// input pitch against the live chord/key instead of generating a pitch FROM a
// tone index. kChordTone carries the chord-tone index (0..3: root/3rd/5th/
// 7th, per theory::shape_of); kScaleDegree carries the diatonic degree (0..6)
// when the pitch is in-key but not a chord tone; kNonChordTone (chromatic,
// out of both) carries no usable index -- the first-slice scope gate passes
// these through at their ORIGINAL pitch, never reharmonized.
enum class ToneKind : std::uint8_t {
  kChordTone = 0,
  kScaleDegree = 1,
  kNonChordTone = 2,
};

struct Classification {
  ToneKind kind = ToneKind::kNonChordTone;
  int index = -1;  // chord-tone index (0..3) or scale degree (0..6); -1 otherwise
};

// restyle-musical-scope.md §3.1: pc = pitch % 12, rel = (pc - chord.root_pc)
// mod 12; a match against theory::shape_of(chord.quality)'s offsets is a
// chord tone at that offset's index. Otherwise theory::degree_of(key, pc): a
// diatonic degree if in-key, else chromatic/passing (kNonChordTone). A chord
// tone is checked FIRST (root/3rd/5th/7th is the stronger functional read
// when both would match, e.g. a root that is also the key's tonic degree).
constexpr Classification classify(const Key& key, const ChordState& chord,
                                  std::uint8_t pitch) noexcept {
  const auto pc = static_cast<std::uint8_t>(pitch % 12);
  if (chord.valid) {
    const ChordShape shape = theory::shape_of(chord.quality);
    const auto rel = static_cast<std::uint8_t>((pc + 12 - chord.root_pc) % 12);
    for (std::uint8_t i = 0; i < shape.count; ++i) {
      if (shape.offsets[i] == rel) {
        return Classification{.kind = ToneKind::kChordTone, .index = i};
      }
    }
  }
  const int degree = theory::degree_of(key, pc);
  if (degree >= 0) {
    return Classification{.kind = ToneKind::kScaleDegree, .index = degree};
  }
  return Classification{};
}

// Rounds an absolute tick to its nearest kTicksPerStep (240-tick, 16th-grid)
// slot -- round-half-up, so a tie snaps forward (never negative, matching the
// core's forward-only micro-timing discipline elsewhere, timeline.hpp).
// restyle-musical-scope.md §1(a): the one genuinely new rhythmic primitive;
// groove::apply's OWN quantize field cannot do this (it only scales an offset
// IT computed, it cannot snap an arbitrary externally-timed tick).
constexpr Tick snap_to_grid(Tick tick) noexcept {
  const auto rem = static_cast<Tick>(tick % kTicksPerStep);
  const Tick base = tick - rem;
  return (rem * 2 >= kTicksPerStep) ? static_cast<Tick>(base + kTicksPerStep) : base;
}

// Default per-role register anchor (MIDI note of chord-tone 0 at octave 0).
// Deliberately duplicated from Arranger::kRoleAnchor (arranger.hpp) rather
// than shared: that table is private to a class with three existing
// byte-identical goldens (arranger_band/gesture/voicing) -- duplicating six
// lines here is lower risk than reopening that file. Keep in sync if it ever
// changes; the values are the same historical registers (D24).
constexpr int role_anchor(TrackRole role) noexcept {
  constexpr int kRoleAnchor[10] = {
      60,  // kDrums  (fixed; unused)
      60,  // kPerc   (fixed; unused)
      36,  // kBass
      60,  // kChord1
      60,  // kChord2
      48,  // kPad
      72,  // kArp
      72,  // kPhrase
      72,  // kLead
      60,  // kCc     (unused)
  };
  const auto idx = static_cast<std::uint8_t>(role);
  return idx < 10 ? kRoleAnchor[idx] : 60;
}

// Moves `note` by whole octaves to sit as close as possible to `anchor`,
// staying inside the MIDI range -- same math as VoicingState::nearest_octave
// (voicing.hpp), duplicated for the same reason as role_anchor() above (that
// method is private). Establishes the target style's characteristic register
// for the FIRST classified chord tone (VoicingState::voice() itself leaves a
// note untouched when it has no prior voicing to lead from -- see its own
// header comment -- so an anchor step is needed before it, not instead of
// it).
constexpr int nearest_octave_to(int note, int anchor) noexcept {
  while (note - anchor > 6 && note - 12 >= 0) {
    note -= 12;
  }
  while (anchor - note > 6 && note + 12 <= 127) {
    note += 12;
  }
  return note;
}

// Channel filter (roadmap 9320, second slice): restyle-musical-scope.md/
// -placement.md's first slice was deliberately channel-blind (see
// RestyleStage's own header comment history) -- a real bug, since a drum hit
// sharing a pitch class with a chord tone got musically reharmonized right
// alongside actual melody notes. One bit per MIDI channel (0..15); a set bit
// means "this channel is transformed by the style idiom", a clear bit means
// "pass this channel through verbatim" (RestyleStage still owns its routing
// once loaded -- the double-note guard disables the raw thru wholesale -- so
// an excluded channel is forwarded unmodified rather than silently dropped).
// Default excludes MIDI channel 9 (0-based; GM's standard percussion
// channel), the same channel the shared `tiny.mid` fixture's drum hit rides.
inline constexpr std::uint16_t kAllChannels = 0xFFFF;
inline constexpr std::uint8_t kDefaultExcludedChannel = 9;  // GM drum channel, 0-based
inline constexpr std::uint16_t kDefaultChannelMask =
    static_cast<std::uint16_t>(kAllChannels & ~(1u << kDefaultExcludedChannel));

constexpr bool channel_selected(std::uint16_t mask, std::uint8_t channel) noexcept {
  return (mask & static_cast<std::uint16_t>(1u << (channel & 0x0F))) != 0;
}

}  // namespace arrangrr::restyle

namespace arrangrr {

template <std::size_t kPorts>
class RestyleStage {
 public:
  // `port`/`channel` are the FIXED output route for every restyled note (a
  // load-time choice, mirroring midisrc::MidiSourceStage's own constructor
  // port argument -- restyle-placement.md §3: "no live command" for this
  // first slice). `chorddet`/`followed` are injected by reference (see the
  // header comment); RestyleStage is constructed AFTER ChorddetStage in the
  // Pipeline's declared order, so it may reference the already-built peer
  // (the same "declare A, construct B referencing A" idiom runtime/
  // pipeline.hpp documents).
  RestyleStage(OutScheduler<kSchedulerCapacity>& scheduler, ChorddetStage<kPorts>& chorddet,
               FollowedContext& followed, std::uint8_t port = 0, std::uint8_t channel = 0) noexcept
      : m_scheduler(scheduler),
        m_chorddet(chorddet),
        m_followed(followed),
        m_port(port),
        m_channel(static_cast<std::uint8_t>(channel & 0x0F)) {}

  // Loads the target style this stage restyles input notes into (mirrors
  // Arranger::load_style): resets the voicing memory so a style switch never
  // leads from a stale voicing. This stage stays INERT (push_midi_in a
  // no-op) until this succeeds -- the `restyle <style>` L1 verb, host-side.
  // `target_role` (roadmap 9320, second slice) is the role whose authored
  // VoicingPolicy/register anchor the transform reads -- optional at the
  // verb level (`restyle <style> [role]`), defaulting to the ORIGINAL fixed
  // kLead behavior when omitted, so every existing call site (and golden)
  // that never named a role keeps its exact prior meaning.
  bool load_style(const Style* style, TrackRole target_role = TrackRole::kLead) noexcept {
    if (style == nullptr) {
      return false;
    }
    m_style = style;
    m_target_role = target_role;
    m_voicing.reset();
    return true;
  }
  constexpr bool loaded() const noexcept { return m_style != nullptr; }
  constexpr const Style* current_style() const noexcept { return m_style; }
  constexpr TrackRole target_role() const noexcept { return m_target_role; }

  // Channel filter (roadmap 9320, second slice, see restyle::kDefaultChannelMask's
  // own comment above): which input MIDI channels this stage transforms.
  // Settable independent of load_style() -- the `restyle <style> [role]` L1
  // verb resets it to the sensible default on every call (host-side, see
  // shell_music_commands.cpp), but this stays a distinct setter so a future
  // caller can override the mask without forcing a style/role change too.
  void set_channel_mask(std::uint16_t mask) noexcept { m_channel_mask = mask; }
  constexpr std::uint16_t channel_mask() const noexcept { return m_channel_mask; }

  // Seam-C-shaped fan-out hook (the SAME shape ChorddetStage/the forward-flow
  // fan-out expect, runtime/pipeline.hpp): fed the raw wire bytes
  // MidiSourceStage's forward-flow produces for the melody-thru port. Own
  // MidiParser instance per port (never shared, matching chorddet's own
  // discipline -- running-status continuity per port). `on_steer` is
  // accepted for shape-parity with every other non-terminal stage but
  // unused: Restyle never steers the followed chord (harmony is read-only
  // here, per the scope gate).
  template <typename OnSteer>
  void push_midi_in(std::uint8_t port, const std::uint8_t* bytes, std::size_t count,
                    OnSteer&& /*on_steer*/) {
    if (port >= kPorts) {
      return;
    }
    for (std::size_t i = 0; i < count; ++i) {
      m_parsers[port].feed(bytes[i], [&](const MidiMessage& msg) { observe(msg); });
    }
  }

  // The STAGE port (runtime/stage.hpp's StageLike concept): caches the
  // current stream tick for push_midi_in's rhythmic snap (push_midi_in
  // itself is never handed a tick -- forward-flow's shape is
  // `(port, bytes, count)`, see pipeline.hpp). Pipeline's fixed declared
  // order fires MidiSourceStage's on_tick (which triggers forward-flow into
  // THIS stage's push_midi_in) BEFORE this stage's own on_tick runs the same
  // tick, so `m_now` here trails the true "now" by at most one raw tick
  // during that same call -- utterly negligible against the 240-tick
  // quantization grid this value only ever feeds (see snap_to_grid above),
  // and it never affects note-on/note-off PAIRING (both read the same
  // trailing value, so the derived gate is exact).
  template <typename SinkT>
  void on_tick(const runtime::StageContext& ctx, SinkT) noexcept {
    m_now = ctx.now;
  }
  template <typename SinkT>
  void flush(SinkT) noexcept {}

 private:
  // One classified-and-scheduled note-on awaiting its matching note-off,
  // keyed by the ORIGINAL input pitch (0..127, bounded, no heap). A retrigger
  // on the same pitch before its note-off arrives simply overwrites the slot
  // (last-wins) -- an accepted first-slice simplification (real melodic
  // input rarely retriggers a held pitch without an intervening release).
  struct Pending {
    bool active = false;
    bool passthrough = false;      // roadmap 9320, channel filter: emitted verbatim, no transform
    int voiced_note = -1;          // the post-anchor/voicing pitch actually scheduled (or the
                                   // ORIGINAL pitch, unchanged, when passthrough)
    std::uint8_t out_channel = 0;  // the channel the note-on was actually scheduled on
    Tick on_arrival = 0;           // the input's own (trailing, see on_tick) tick
    Tick final_on_tick = 0;        // where the (restyled or passthrough) note-on landed
  };

  void observe(const MidiMessage& msg) {
    if (!loaded()) {
      return;  // inert by default
    }
    if (msg.type() == midi::kNoteOn && msg.d2 > 0) {
      note_on(msg.d1, msg.d2, msg.channel());
    } else if (msg.type() == midi::kNoteOff || (msg.type() == midi::kNoteOn && msg.d2 == 0)) {
      note_off(msg.d1);
    }
  }

  // Reads the target style's OWN authored VoicingPolicy for m_target_role
  // (restyle-musical-scope.md §3.3: "run them through the target style's own
  // VoicingState ... under that role's authored VoicingPolicy"). Scans every
  // section for the FIRST pattern matching the role -- a deliberate first-
  // slice simplification: Restyle does not track the Arranger's own live
  // section/bar position (it has no bar-boundary concept of its own), so it
  // reads one REPRESENTATIVE policy from the loaded style rather than a
  // per-section-synced one. kAsWritten (identity, no re-voicing) when the
  // style authors no pattern for this role at all.
  VoicingPolicy target_voicing_policy() const noexcept {
    if (m_style == nullptr) {
      return VoicingPolicy::kAsWritten;
    }
    for (const StyleSection& section : m_style->sections) {
      for (const StylePattern& pattern : section.patterns) {
        if (pattern.role == m_target_role) {
          return pattern.voicing;
        }
      }
    }
    return VoicingPolicy::kAsWritten;
  }

  void note_on(std::uint8_t pitch, std::uint8_t vel, std::uint8_t channel) {
    Pending& p = m_pending[pitch];
    if (!restyle::channel_selected(m_channel_mask, channel)) {
      // Channel filter (roadmap 9320, second slice): this channel is outside
      // the mask (e.g. the drum channel by default) -- RestyleStage is still
      // the sole forwarder once loaded (the double-note guard disables the
      // raw thru wholesale), so the note is forwarded VERBATIM: original
      // pitch, original channel, original (unsnapped, un-grooved) tick. No
      // classification, no octave anchor, no groove -- exactly what the raw
      // melody-thru would have produced for this note.
      p.active = true;
      p.passthrough = true;
      p.voiced_note = pitch;
      p.out_channel = channel;
      p.on_arrival = m_now;
      p.final_on_tick = m_now;
      (void)m_scheduler.schedule(m_port, m_now, MidiMessage::note_on(channel, pitch, vel));
      return;
    }

    const Key& key = m_chorddet.key();
    const ChordState& chord = m_followed.state();
    const restyle::Classification c = restyle::classify(key, chord, pitch);

    int voiced = pitch;
    if (c.kind == restyle::ToneKind::kChordTone) {
      // Register/voicing transfer (chord tones only, restyle-musical-
      // scope.md §3.3): anchor-snap to the role's characteristic register
      // FIRST (VoicingState has nothing to establish it on the very first
      // note), then let the style's own VoicingPolicy refine continuity.
      voiced = restyle::nearest_octave_to(pitch, restyle::role_anchor(m_target_role));
      NoteReq req{.note = voiced, .vel = vel, .gate = 0, .gesture_delay = 0, .chord_tone = true};
      m_voicing.voice(m_target_role, target_voicing_policy(), &req, 1);
      voiced = req.note;
    }
    // else: kScaleDegree / kNonChordTone -- pass through at the ORIGINAL
    // pitch (no octave move, no reharmonization), per the scope gate.

    const Tick snapped = restyle::snap_to_grid(m_now);
    const auto step = static_cast<std::uint16_t>((snapped % kTicksPerBar) / kTicksPerStep);
    const GrooveOut g = groove::apply(m_style->groove, static_cast<std::uint8_t>(m_target_role),
                                      step, snapped, vel);
    const Tick final_on = static_cast<Tick>(snapped + static_cast<Tick>(g.timing_offset));

    p.active = true;
    p.passthrough = false;
    p.voiced_note = voiced;
    p.out_channel = m_channel;
    p.on_arrival = m_now;
    p.final_on_tick = final_on;

    (void)m_scheduler.schedule(
        m_port, final_on,
        MidiMessage::note_on(m_channel, static_cast<std::uint8_t>(voiced), g.velocity));
  }

  void note_off(std::uint8_t pitch) {
    Pending& p = m_pending[pitch];
    if (!p.active) {
      return;  // no matching note-on (e.g. arrived before load_style(), or already closed)
    }
    p.active = false;
    if (p.passthrough) {
      // Verbatim close, same channel filter reasoning as note_on above: the
      // ORIGINAL arrival tick, no gate recomputation (there is no snap to
      // preserve a gate through).
      (void)m_scheduler.schedule(
          m_port, m_now,
          MidiMessage::note_off(p.out_channel, static_cast<std::uint8_t>(p.voiced_note)));
      return;
    }
    // Gate preserved through the snap (groove::apply's own "note-on and
    // note-off share one offset" contract, groove.hpp): the ORIGINAL gate
    // rides the SAME final_on_tick. Clamped to at least 1 tick so a same-
    // tick on/off pair can never produce a stuck-note-adjacent zero-length
    // event (mirrors Timeline::set_step's own "gate 0 on an audible step" guard).
    const Tick gate = (m_now > p.on_arrival) ? (m_now - p.on_arrival) : Tick{1};
    const Tick final_off = static_cast<Tick>(p.final_on_tick + gate);
    (void)m_scheduler.schedule(
        m_port, final_off,
        MidiMessage::note_off(m_channel, static_cast<std::uint8_t>(p.voiced_note)));
  }

  OutScheduler<kSchedulerCapacity>& m_scheduler;
  ChorddetStage<kPorts>& m_chorddet;
  FollowedContext& m_followed;
  std::uint8_t m_port;
  std::uint8_t m_channel;
  const Style* m_style = nullptr;
  TrackRole m_target_role = TrackRole::kLead;  // roadmap 9320, second slice: `restyle`'s [role] arg
  std::uint16_t m_channel_mask = restyle::kDefaultChannelMask;  // roadmap 9320, channel filter
  VoicingState m_voicing;  // this stage's OWN voice-leading lineage (D40, distinct from Arranger's)
  MidiParser m_parsers[kPorts];
  Pending m_pending[128]{};
  Tick m_now = 0;
};

}  // namespace arrangrr
