#pragma once

#include <cstdint>

#include "arrangrr/arranger/groove.hpp"  // GrooveParams (per-style default feel, 9110)
#include "chorddet/theory.hpp"
#include "arrangrr/common/span.hpp"
#include "common/time.hpp"  // BpmX100, kDefaultBpm (per-style default tempo, 9120)
#include "arrangrr/timeline/timeline.hpp"

// Style model (D24 groundwork): sections hold DEGREE-RELATIVE patterns per
// role, resolved against the live chord at playback — the NTT idea in its
// honest core form. Content is constexpr data (D32): read-only styles cost
// zero RAM on the device (D33: flash, memory-mapped).

namespace arrangrr {

// Full section vocabulary from day one (ABI stability); the built-in demo
// style implements a subset.
enum class SectionType : std::uint8_t {
  kIntro1 = 0,
  kIntro2 = 1,
  kVarA = 2,
  kVarB = 3,
  kVarC = 4,
  kVarD = 5,
  kFillA = 6,
  kFillB = 7,
  kFillC = 8,
  kFillD = 9,
  kBreak = 10,
  kEnding1 = 11,
  kEnding2 = 12,
};
inline constexpr std::uint8_t kSectionTypeCount = 13;

constexpr bool section_is_variation(SectionType t) noexcept {
  return t >= SectionType::kVarA && t <= SectionType::kVarD;
}
constexpr bool section_is_fill(SectionType t) noexcept {
  return t >= SectionType::kFillA && t <= SectionType::kFillD;
}
constexpr bool section_is_intro(SectionType t) noexcept { return t <= SectionType::kIntro2; }
constexpr bool section_is_ending(SectionType t) noexcept { return t >= SectionType::kEnding1; }

enum class RolePolicy : std::uint8_t {
  kFixed = 0,      // literal MIDI notes (drums/percussion — never transposed)
  kChordTone = 1,  // tone = chord-tone index; resolved via NTT at playback
};

// Per-event note-source vocabulary (additive on top of RolePolicy): how the
// event's `tone` is interpreted when the role is NOT kFixed. kChordTone keeps
// the historical NTT behavior, so it is the default and every existing style
// table stays byte-for-byte identical. kScaleDegree and kInterval unlock
// key-diatonic lines and chromatic/interval figures without leaving the core's
// wrong-note-proof envelope.
enum class NoteSource : std::uint8_t {
  kChordTone = 0,    // tone = chord-tone index (CURRENT behavior; default)
  kScaleDegree = 1,  // tone = scale degree of the current KEY (diatonic)
  kInterval = 2,     // tone = signed semitone offset from the chord root
};

// Per-event generative gesture (D40 pipeline): how a single StyleEvent expands
// into one OR MORE timed notes at playback. kNone is the historical 1-event ->
// 1-note behavior and the default, so every existing style table stays
// byte-for-byte identical. Concrete gesture variants (strum, roll, arpeggiate)
// are defined by the gesture-expansion stage (`arranger/gesture.hpp`); the
// arranger fire loop expands the event through `gesture::expand` before
// resolving each produced note.
enum class ChordGesture : std::uint8_t {
  kNone = 0,       // no gesture: the event yields exactly one note (default)
  kStrumUp = 1,    // arpeggiate the chord low->high, fixed micro-stagger per tone
  kStrumDown = 2,  // arpeggiate the chord high->low, fixed micro-stagger per tone
  kRollUp = 3,     // spread the tones evenly across the gate, ascending
  kRollDown = 4,   // spread the tones evenly across the gate, descending
};

// Per-pattern voicing policy (D40 pipeline): whether the role's chord-tone
// notes are re-voiced for smooth voice-leading. kAsWritten reproduces today's
// behavior exactly (notes sound at their authored register) and is the default,
// so existing style tables are unchanged; kLead lets the VoicingEngine retain
// common tones and minimize motion between successive chords.
enum class VoicingPolicy : std::uint8_t {
  kAsWritten = 0,  // authored register, no re-voicing (default; historical)
  kLead = 1,       // smooth voice-leading (common-tone retention, minimal motion)
};

struct StyleEvent {
  std::uint16_t step;  // 16th-grid position within the section
  std::int8_t tone;    // kFixed: MIDI note; kChordTone: chord-tone index;
                       // kScaleDegree: key scale degree; kInterval: semitones
  std::int8_t octave;  // octave offset
  std::uint8_t vel;
  std::uint16_t gate;  // ticks
  // How `tone` is read at resolve time. Kept LAST with a default so existing
  // designated- AND positional-initializer tables stay valid and unchanged.
  NoteSource src = NoteSource::kChordTone;
  // Generative gesture. Fits the struct's existing tail padding (no size
  // growth), and kept last with a default so every style table is unchanged.
  ChordGesture gesture = ChordGesture::kNone;
};
// The event is the per-note flash unit (D33): its size is multiplied across
// every constexpr style table. Pin it so an accidental field/padding change is
// caught at compile time rather than silently growing flash.
static_assert(sizeof(StyleEvent) == 10, "StyleEvent must stay 10 bytes (flash budget, D33)");

struct StylePattern {
  TrackRole role;
  RolePolicy policy;
  Span<const StyleEvent> events;
  // Default GM voice for this role, emitted as a Program Change when the style
  // loads (on the role's route). -1 = leave the synth's current voice. Kept
  // last with a default so existing designated initializers stay valid.
  std::int16_t gm_program = -1;
  // Voice-leading policy for this part's chord-tone notes (D40). Kept last with
  // a default so existing designated initializers stay valid; fits existing
  // padding (no size growth).
  VoicingPolicy voicing = VoicingPolicy::kAsWritten;
};

struct StyleSection {
  SectionType type;
  std::uint8_t bars;
  Span<const StylePattern> patterns;
};

struct Style {
  const char* name;  // host display only; the core matches by index
  Span<const StyleSection> sections;
  // Default groove FEEL this style loads with (9110). Loading the style seeds
  // the arranger's live GrooveParams from this; later user `groove`-panel edits
  // override it until the next style load/switch. Kept as a defaulted member so
  // every existing constexpr style table stays valid AND — while all 16 builtins
  // keep the no-op all-zero default this pass — the arranger output stays
  // byte-identical. constexpr data in flash (D32/D33): zero RAM cost.
  GrooveParams groove{};
  // Default transport TEMPO this style loads with (9120). Scheduling is
  // tick-based, so this changes only the playback rate / tempo meta, never note
  // tick positions. Defaulted to kDefaultBpm and kept last so existing tables
  // are unchanged; a style load/switch seeds the transport bpm from it.
  BpmX100 tempo = kDefaultBpm;

  constexpr const StyleSection* find(SectionType t) const noexcept {
    for (const StyleSection& s : sections) {
      if (s.type == t) {
        return &s;
      }
    }
    return nullptr;
  }
};

namespace styles {

// Named constants so the pattern tables read as music, not magic numbers.
// GM drum notes (RolePolicy::kFixed — literal, never transposed).
inline constexpr std::int8_t kKick = 36;
inline constexpr std::int8_t kRimshot = 37;
inline constexpr std::int8_t kSnare = 38;
inline constexpr std::int8_t kClap = 39;
inline constexpr std::int8_t kClosedHat = 42;
inline constexpr std::int8_t kOpenHat = 46;
inline constexpr std::int8_t kCrash = 49;
inline constexpr std::int8_t kRide = 51;
// Toms (GM) for fills and tom grooves.
inline constexpr std::int8_t kTomFloor = 43;
inline constexpr std::int8_t kTomLow = 45;
inline constexpr std::int8_t kTomMid = 47;
inline constexpr std::int8_t kTomHi = 50;
// Latin / world percussion (GM) — makes the world grooves audibly distinct.
inline constexpr std::int8_t kSideStick = 37;  // alias of rimshot (side-stick cross)
inline constexpr std::int8_t kTambourine = 54;
inline constexpr std::int8_t kCowbell = 56;
inline constexpr std::int8_t kHiBongo = 60;
inline constexpr std::int8_t kLoBongo = 61;
inline constexpr std::int8_t kMuteHiConga = 62;
inline constexpr std::int8_t kOpenHiConga = 63;
inline constexpr std::int8_t kLoConga = 64;
inline constexpr std::int8_t kHiTimbale = 65;
inline constexpr std::int8_t kLoTimbale = 66;
inline constexpr std::int8_t kHiAgogo = 67;
inline constexpr std::int8_t kLoAgogo = 68;
inline constexpr std::int8_t kCabasa = 69;
inline constexpr std::int8_t kMaracas = 70;
inline constexpr std::int8_t kShortGuiro = 73;
inline constexpr std::int8_t kClaves = 75;
inline constexpr std::int8_t kHiWoodblock = 76;
inline constexpr std::int8_t kLoWoodblock = 77;

// Chord-tone indices (RolePolicy::kChordTone — resolved against the live chord).
inline constexpr std::int8_t kRoot = 0;
inline constexpr std::int8_t kThird = 1;
inline constexpr std::int8_t kFifth = 2;
inline constexpr std::int8_t kSeventh = 3;

// Gate lengths in ticks (PPQN=960: a 16th=240, a quarter=960, a 4/4 bar=3840).
inline constexpr std::uint16_t kGateStaccato = 50;
inline constexpr std::uint16_t kGateHat = 120;
inline constexpr std::uint16_t kGateStab = 200;
inline constexpr std::uint16_t kGate8th = 240;
inline constexpr std::uint16_t kGateBeat = 360;
inline constexpr std::uint16_t kGateHalfBar = 1800;
inline constexpr std::uint16_t kGateHeld = 3600;  // just under a full bar

}  // namespace styles
}  // namespace arrangrr
