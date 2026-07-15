#pragma once

#include <cstdint>

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/config.hpp"
#include "chorddet/theory.hpp"
#include "common/time.hpp"

// LoopEvent: the recorded payload of node 6000 (the Looper), the note-level
// peer of ChordSequence (chord/chord_sequence.hpp). Phase 7 owner directive
// (docs/reflections/phase7-scope-6000-8100-clip-timeline-seam.md, Fork A,
// RESOLVED functional/chord-relative): a captured event is NEVER an absolute
// MIDI pitch -- it stores a chord-tone/scale-degree/interval-relative
// position, mirroring Arranger::resolve()'s own NTT vocabulary
// (arranger/arranger.hpp's NoteSource/tone/octave), so re-harmonizing a
// captured loop when the followed chord changes is the SAME resolve-against-
// current-context call every generative pattern already makes, not a
// heuristic re-transposition after the fact (6500 is deferred, but this
// storage shape is what makes it near-free later, per the owner's own
// framing of Fork A).
//
// decompose_note()/resolve_note() are exact inverses of each other for an
// UNCHANGED chord/key (round-trip identity: capture then play back under the
// same harmonic context reproduces the original absolute pitch bit-for-bit),
// and resolve_note() re-derives against whatever chord/key is CURRENT at
// playback time -- so a loop played under a different chord than the one it
// was captured under re-harmonizes automatically, with no extra machinery.

namespace arrangrr {

// How a captured note's relative position is interpreted at playback,
// mirroring Arranger's own NoteSource (arranger/arranger.hpp) at note-level
// granularity rather than per-style-pattern granularity.
enum class LoopNoteSource : std::uint8_t {
  kChordTone = 0,    // tone = chord-shape tone index, octave = octave delta
  kScaleDegree = 1,  // tone = scale degree (key-diatonic, chord-independent)
  kInterval = 2,     // tone = raw signed semitone offset from the chord (or
                     //        key, if no chord) root; ALWAYS succeeds --
                     //        the fallback for a chromatic/off-chord note.
};

// One captured note. `duration == 0` is the OPEN sentinel while the note is
// still held during live capture (mirrors ChordStep's own prev->duration==0
// convention, chord/chord_sequence.hpp) -- it is always closed to a non-zero
// value before playback ever reads it (LoopBuffer::stop_record).
struct LoopEvent {
  Tick start = 0;          // relative to the loop start
  Tick duration = 0;       // free length in ticks (D14)
  std::int8_t tone = 0;    // meaning depends on `source`
  std::int8_t octave = 0;  // octave delta (kChordTone/kScaleDegree only)
  std::uint8_t velocity = 100;
  LoopNoteSource source = LoopNoteSource::kInterval;
};
static_assert(sizeof(LoopEvent) == 12);  // matches ChordStep's own 12 B chord-relative precedent

// Decomposes an absolute MIDI note (0..127) into a chord-tone/scale-degree/
// interval-relative position against the CURRENT chord/key context -- the
// capture-time "reverse resolve" step. Always succeeds (kInterval is a total
// fallback), so every played note is capturable, in or out of key.
constexpr void decompose_note(std::uint8_t note, const ChordState& chord, const Key& key,
                              std::int8_t& tone, std::int8_t& octave,
                              LoopNoteSource& source) noexcept {
  if (chord.valid) {
    const ChordShape shape = theory::shape_of(chord.quality);
    for (std::uint8_t k = 0; k < shape.count; ++k) {
      const int raw = static_cast<int>(note) - chord.root_pc - shape.offsets[k];
      if (raw % 12 == 0) {
        tone = static_cast<std::int8_t>(k);
        octave = static_cast<std::int8_t>(raw / 12);
        source = LoopNoteSource::kChordTone;
        return;
      }
    }
  }
  const int degree = theory::degree_of(key, static_cast<std::uint8_t>(note % 12));
  if (degree >= 0) {
    const int semis = theory::degree_to_semitones(key.mode, degree);
    const int raw = static_cast<int>(note) - key.root_pc - semis;
    if (raw % 12 == 0) {
      tone = static_cast<std::int8_t>(degree);
      octave = static_cast<std::int8_t>(raw / 12);
      source = LoopNoteSource::kScaleDegree;
      return;
    }
  }
  // Fallback: a literal interval from the chord root (or key root with no
  // chord) -- always succeeds, so a chromatic passing tone is never dropped.
  const std::uint8_t anchor_pc = chord.valid ? chord.root_pc : key.root_pc;
  tone = static_cast<std::int8_t>(static_cast<int>(note) - anchor_pc);
  octave = 0;
  source = LoopNoteSource::kInterval;
}

// The inverse of decompose_note(): re-derives an absolute MIDI note from a
// captured event against a (possibly DIFFERENT, re-harmonized) chord/key.
// Returns -1 when the event cannot resolve under the given context (no
// chord for a kChordTone event) or the result falls outside 0..127 (drop,
// not fold -- same discipline as Arranger::resolve()).
constexpr int resolve_note(const LoopEvent& ev, const ChordState& chord, const Key& key) noexcept {
  switch (ev.source) {
    case LoopNoteSource::kChordTone: {
      if (!chord.valid) {
        return -1;
      }
      const ChordShape shape = theory::shape_of(chord.quality);
      if (shape.count == 0 || ev.tone < 0) {
        return -1;
      }
      const std::uint8_t idx = static_cast<std::uint8_t>(ev.tone) % shape.count;
      const int note = chord.root_pc + shape.offsets[idx] + 12 * ev.octave;
      return (note < 0 || note > 127) ? -1 : note;
    }
    case LoopNoteSource::kScaleDegree: {
      const int note =
          key.root_pc + theory::degree_to_semitones(key.mode, ev.tone) + 12 * ev.octave;
      return (note < 0 || note > 127) ? -1 : note;
    }
    case LoopNoteSource::kInterval:
    default: {
      const std::uint8_t anchor_pc = chord.valid ? chord.root_pc : key.root_pc;
      const int note = anchor_pc + ev.tone + 12 * ev.octave;
      return (note < 0 || note > 127) ? -1 : note;
    }
  }
}

// 6400: how a loop's playback length is derived. kAuto (the default) uses
// the furthest captured event end; kFixed pins an explicit tick length
// (useful to pre-declare "this is a 4-bar loop" before it is fully played);
// kQuantized snaps the auto/content length UP to the next multiple of
// `quantize_grid` -- mirrors Track::length (timeline.hpp) and ChordSequence's
// own length()/loop precedent, generalized with an explicit mode selector
// since a note-level loop's natural length is ambiguous in more ways than a
// single-voice chord sequence's.
enum class LoopLengthMode : std::uint8_t {
  kAuto = 0,
  kFixed = 1,
  kQuantized = 2,
};

// LoopClip: one recorded loop's content -- the note-level peer of
// ChordSequence (mirrors its FORM: free-duration steps, quantize-after,
// loop). Owned by LoopBuffer's pool (loop_buffer.hpp), never directly by
// ClipMatrix (same scope tripwire ChordSequence/Timeline already observe).
class LoopClip {
 public:
  // A captured loop repeats by default -- the entire point of node 6000 is
  // to loop what was played; ChordSequence defaults to false (a progression
  // is often played once through) because it is a DIFFERENT primitive.
  bool loop = true;
  LoopLengthMode length_mode = LoopLengthMode::kAuto;
  Tick fixed_length = 0;              // meaningful only when length_mode == kFixed
  Tick quantize_grid = kTicksPerBar;  // meaningful only when length_mode == kQuantized

  constexpr std::size_t count() const noexcept { return m_events.size(); }
  constexpr const LoopEvent& event(std::size_t i) const noexcept { return m_events[i]; }
  constexpr Span<const LoopEvent> events() const noexcept { return m_events.span(); }
  constexpr bool empty() const noexcept { return m_events.empty(); }
  // The bounded event pool's own fixed capacity (node 6300, retroactive
  // capture's own grab() reads this to know how many of a dense window's
  // events it can actually keep -- see retro_capture.hpp).
  static constexpr std::size_t capacity() noexcept { return kMaxLoopEvents; }

  // The raw, content-derived length: the furthest event end. Unlike
  // ChordSequence::length() (which trusts the LAST pushed step, valid only
  // for its strictly-sequential single-voice chain), a LoopClip is
  // POLYPHONIC -- events are not necessarily pushed in start order (overdub
  // appends later material that can start earlier than existing content) --
  // so this scans every event's own end.
  constexpr Tick content_length() const noexcept {
    Tick max_end = 0;
    for (const LoopEvent& ev : m_events) {
      const Tick end = ev.start + ev.duration;
      if (end > max_end) {
        max_end = end;
      }
    }
    return max_end;
  }

  // The EFFECTIVE playback length (6400): kFixed/kQuantized reshape the raw
  // content length per `length_mode`.
  constexpr Tick length() const noexcept {
    switch (length_mode) {
      case LoopLengthMode::kFixed:
        return fixed_length;
      case LoopLengthMode::kQuantized: {
        const Tick raw = content_length();
        if (quantize_grid == 0 || raw == 0) {
          return raw;
        }
        return ((raw + quantize_grid - 1) / quantize_grid) * quantize_grid;
      }
      case LoopLengthMode::kAuto:
      default:
        return content_length();
    }
  }

  // Raw append during recording (start/duration given by the recorder;
  // duration == 0 is the still-open sentinel). Mirrors ChordSequence::record.
  [[nodiscard]] constexpr bool record(const LoopEvent& ev) noexcept {
    return m_events.push_back(ev);
  }

  // Mutable access to an in-flight (still-open) event, so the recorder can
  // close its duration on the matching note-off (mirrors ChordSequencer's own
  // seq->last() pattern, generalized to random access since several notes
  // can be held open concurrently here).
  constexpr LoopEvent* mutable_event(std::size_t i) noexcept {
    return i < m_events.size() ? &m_events[i] : nullptr;
  }

  constexpr void clear() noexcept { m_events.clear(); }

  // Removes ONE captured event. Unlike ChordSequence::remove (which closes a
  // start-time gap because its steps form one strictly-sequential chain),
  // a LoopClip's events are polyphonic/independent -- deleting one never
  // shifts any other event's own start.
  constexpr bool remove(std::size_t i) noexcept {
    if (i >= m_events.size()) {
      return false;
    }
    m_events.erase(i);
    return true;
  }

  // Quantize-after (6200, D14 live entry): reuses ChordSequence::quantize's
  // OWN rounding algorithm (snap to the nearest grid multiple, keep at least
  // one grid unit) -- adapted per-event rather than from consecutive-start
  // gaps, because a polyphonic buffer has no single total ordering of
  // "the next step" the way ChordSequence's one-voice chain does.
  constexpr void quantize(Tick grid = kTicksPerBar) noexcept {
    if (grid == 0) {
      return;
    }
    for (LoopEvent& ev : m_events) {
      ev.start = round_to_grid(ev.start, grid);
      const Tick snapped_duration = round_to_grid(ev.duration, grid);
      ev.duration = snapped_duration == 0 ? grid : snapped_duration;
    }
  }

 private:
  static constexpr Tick round_to_grid(Tick value, Tick grid) noexcept {
    return ((value + grid / 2) / grid) * grid;
  }

  StaticVector<LoopEvent, kMaxLoopEvents> m_events;
};

}  // namespace arrangrr
