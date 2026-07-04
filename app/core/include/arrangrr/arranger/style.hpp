#pragma once

#include <cstdint>

#include "arrangrr/chord/theory.hpp"
#include "arrangrr/common/span.hpp"
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

struct StyleEvent {
  std::uint16_t step;  // 16th-grid position within the section
  std::int8_t tone;    // kFixed: MIDI note; kChordTone: chord-tone index
  std::int8_t octave;  // octave offset
  std::uint8_t vel;
  std::uint16_t gate;  // ticks
};

struct StylePattern {
  TrackRole role;
  RolePolicy policy;
  Span<const StyleEvent> events;
  // Default GM voice for this role, emitted as a Program Change when the style
  // loads (on the role's route). -1 = leave the synth's current voice. Kept
  // last with a default so existing designated initializers stay valid.
  std::int16_t gm_program = -1;
};

struct StyleSection {
  SectionType type;
  std::uint8_t bars;
  Span<const StylePattern> patterns;
};

struct Style {
  const char* name;  // host display only; the core matches by index
  Span<const StyleSection> sections;

  constexpr const StyleSection* find(SectionType t) const noexcept {
    for (const StyleSection& s : sections) {
      if (s.type == t) {
        return &s;
      }
    }
    return nullptr;
  }
};

// ---------------------------------------------------------------------------
// Built-in demo style "basic": 4/4, one-bar sections, three roles.
// GM drums on the fixed role: kick 36, snare 38, closed hat 42, crash 49.
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

namespace basic {

inline constexpr StyleEvent kVarADrums[] = {
    {.step=0, .tone=36, .octave=0, .vel=110, .gate=120}, {.step=4, .tone=38, .octave=0, .vel=100, .gate=120}, {.step=8, .tone=36, .octave=0, .vel=105, .gate=120}, {.step=12, .tone=38, .octave=0, .vel=100, .gate=120},
    {.step=0, .tone=42, .octave=0, .vel=70, .gate=60},   {.step=2, .tone=42, .octave=0, .vel=60, .gate=60},   {.step=4, .tone=42, .octave=0, .vel=70, .gate=60},   {.step=6, .tone=42, .octave=0, .vel=60, .gate=60},
    {.step=8, .tone=42, .octave=0, .vel=70, .gate=60},   {.step=10, .tone=42, .octave=0, .vel=60, .gate=60},  {.step=12, .tone=42, .octave=0, .vel=70, .gate=60},  {.step=14, .tone=42, .octave=0, .vel=60, .gate=60},
};
inline constexpr StyleEvent kVarABass[] = {
    {.step=0, .tone=0, .octave=0, .vel=100, .gate=220},  // root
    {.step=4, .tone=2, .octave=0, .vel=85, .gate=220},   // fifth
    {.step=8, .tone=0, .octave=0, .vel=95, .gate=220},
    {.step=12, .tone=2, .octave=0, .vel=85, .gate=220},
};
inline constexpr StyleEvent kVarAChord[] = {
    // Comp stabs on beats 1 and 3: full stack (tones 0..3).
    {.step=0, .tone=0, .octave=0, .vel=80, .gate=360}, {.step=0, .tone=1, .octave=0, .vel=80, .gate=360}, {.step=0, .tone=2, .octave=0, .vel=80, .gate=360}, {.step=0, .tone=3, .octave=0, .vel=80, .gate=360},
    {.step=8, .tone=0, .octave=0, .vel=75, .gate=360}, {.step=8, .tone=1, .octave=0, .vel=75, .gate=360}, {.step=8, .tone=2, .octave=0, .vel=75, .gate=360}, {.step=8, .tone=3, .octave=0, .vel=75, .gate=360},
};
inline constexpr StylePattern kVarAPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarADrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarABass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarAChord)},
};

inline constexpr StyleEvent kVarBDrums[] = {
    {.step=0, .tone=36, .octave=0, .vel=115, .gate=120},  {.step=4, .tone=38, .octave=0, .vel=105, .gate=120}, {.step=7, .tone=36, .octave=0, .vel=90, .gate=120}, {.step=8, .tone=36, .octave=0, .vel=110, .gate=120},
    {.step=12, .tone=38, .octave=0, .vel=105, .gate=120}, {.step=0, .tone=42, .octave=0, .vel=75, .gate=50},   {.step=1, .tone=42, .octave=0, .vel=55, .gate=50},  {.step=2, .tone=42, .octave=0, .vel=65, .gate=50},
    {.step=3, .tone=42, .octave=0, .vel=55, .gate=50},    {.step=4, .tone=42, .octave=0, .vel=75, .gate=50},   {.step=5, .tone=42, .octave=0, .vel=55, .gate=50},  {.step=6, .tone=42, .octave=0, .vel=65, .gate=50},
    {.step=7, .tone=42, .octave=0, .vel=55, .gate=50},    {.step=8, .tone=42, .octave=0, .vel=75, .gate=50},   {.step=9, .tone=42, .octave=0, .vel=55, .gate=50},  {.step=10, .tone=42, .octave=0, .vel=65, .gate=50},
    {.step=11, .tone=42, .octave=0, .vel=55, .gate=50},   {.step=12, .tone=42, .octave=0, .vel=75, .gate=50},  {.step=13, .tone=42, .octave=0, .vel=55, .gate=50}, {.step=14, .tone=42, .octave=0, .vel=65, .gate=50},
    {.step=15, .tone=42, .octave=0, .vel=55, .gate=50},
};
inline constexpr StyleEvent kVarBBass[] = {
    {.step=0, .tone=0, .octave=0, .vel=105, .gate=200}, {.step=2, .tone=0, .octave=0, .vel=70, .gate=100},  {.step=4, .tone=1, .octave=0, .vel=90, .gate=200},
    {.step=8, .tone=2, .octave=0, .vel=95, .gate=200},  {.step=10, .tone=0, .octave=1, .vel=75, .gate=100}, {.step=12, .tone=3, .octave=0, .vel=90, .gate=200},
};
inline constexpr StyleEvent kVarBChord[] = {
    {.step=2, .tone=0, .octave=0, .vel=78, .gate=200},  {.step=2, .tone=1, .octave=0, .vel=78, .gate=200},  {.step=2, .tone=2, .octave=0, .vel=78, .gate=200},  {.step=6, .tone=0, .octave=0, .vel=72, .gate=200},
    {.step=6, .tone=1, .octave=0, .vel=72, .gate=200},  {.step=6, .tone=2, .octave=0, .vel=72, .gate=200},  {.step=10, .tone=0, .octave=0, .vel=78, .gate=200}, {.step=10, .tone=1, .octave=0, .vel=78, .gate=200},
    {.step=10, .tone=2, .octave=0, .vel=78, .gate=200}, {.step=14, .tone=0, .octave=0, .vel=72, .gate=200}, {.step=14, .tone=1, .octave=0, .vel=72, .gate=200}, {.step=14, .tone=2, .octave=0, .vel=72, .gate=200},
};
inline constexpr StylePattern kVarBPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarBDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarBBass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarBChord)},
};

inline constexpr StyleEvent kIntroDrums[] = {
    {.step=8, .tone=42, .octave=0, .vel=60, .gate=60},
    {.step=10, .tone=42, .octave=0, .vel=65, .gate=60},
    {.step=12, .tone=42, .octave=0, .vel=70, .gate=60},
    {.step=14, .tone=42, .octave=0, .vel=80, .gate=60},
};
inline constexpr StyleEvent kIntroBass[] = {
    {.step=0, .tone=0, .octave=0, .vel=90, .gate=3600},  // held root pickup
};
inline constexpr StylePattern kIntroPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIntroDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntroBass)},
};

inline constexpr StyleEvent kFillDrums[] = {
    {.step=0, .tone=36, .octave=0, .vel=110, .gate=120}, {.step=4, .tone=38, .octave=0, .vel=100, .gate=120},  {.step=8, .tone=38, .octave=0, .vel=90, .gate=100},
    {.step=10, .tone=38, .octave=0, .vel=95, .gate=100}, {.step=12, .tone=38, .octave=0, .vel=105, .gate=100}, {.step=14, .tone=38, .octave=0, .vel=115, .gate=100},
};
inline constexpr StyleEvent kFillBass[] = {
    {.step=0, .tone=0, .octave=0, .vel=100, .gate=220},
    {.step=8, .tone=2, .octave=0, .vel=90, .gate=220},
};
inline constexpr StylePattern kFillPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFillDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)},
};

inline constexpr StyleEvent kEndDrums[] = {
    {.step=0, .tone=36, .octave=0, .vel=115, .gate=120},
    {.step=0, .tone=49, .octave=0, .vel=110, .gate=1800},
};
inline constexpr StyleEvent kEndBass[] = {
    {.step=0, .tone=0, .octave=0, .vel=100, .gate=3600},
};
inline constexpr StyleEvent kEndChord[] = {
    {.step=0, .tone=0, .octave=0, .vel=85, .gate=3600},
    {.step=0, .tone=1, .octave=0, .vel=85, .gate=3600},
    {.step=0, .tone=2, .octave=0, .vel=85, .gate=3600},
    {.step=0, .tone=3, .octave=0, .vel=85, .gate=3600},
};
inline constexpr StylePattern kEndPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kEndDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEndBass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEndChord)},
};

// intro2: a fuller one-bar intro — full backbeat over a steady hat bed.
inline constexpr StyleEvent kIntro2Drums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=100, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=92, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=98, .gate=120},
    {.step=0, .tone=kClosedHat, .octave=0, .vel=70, .gate=60}, {.step=2, .tone=kClosedHat, .octave=0, .vel=58, .gate=60}, {.step=4, .tone=kClosedHat, .octave=0, .vel=70, .gate=60}, {.step=6, .tone=kClosedHat, .octave=0, .vel=58, .gate=60},
    {.step=8, .tone=kClosedHat, .octave=0, .vel=70, .gate=60}, {.step=10, .tone=kClosedHat, .octave=0, .vel=58, .gate=60}, {.step=12, .tone=kClosedHat, .octave=0, .vel=70, .gate=60}, {.step=14, .tone=kClosedHat, .octave=0, .vel=64, .gate=60},
};
inline constexpr StyleEvent kIntro2Bass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=92, .gate=kGateHeld}};
inline constexpr StyleEvent kIntro2Chord[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=76, .gate=kGateBeat}, {.step=0, .tone=kThird, .octave=0, .vel=76, .gate=kGateBeat}, {.step=0, .tone=kFifth, .octave=0, .vel=76, .gate=kGateBeat},
    {.step=8, .tone=kRoot, .octave=0, .vel=74, .gate=kGateBeat}, {.step=8, .tone=kThird, .octave=0, .vel=74, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=74, .gate=kGateBeat},
};
inline constexpr StylePattern kIntro2Patterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIntro2Drums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntro2Bass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntro2Chord)},
};

// Fills B..D: progressively busier one-bar tom/snare fills, all shared bass.
inline constexpr StyleEvent kFillBDrums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=100, .gate=120}, {.step=6, .tone=kSnare, .octave=0, .vel=96, .gate=120},
    {.step=8, .tone=kTomHi, .octave=0, .vel=100, .gate=120}, {.step=10, .tone=kTomMid, .octave=0, .vel=104, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=108, .gate=120}, {.step=14, .tone=kTomLow, .octave=0, .vel=112, .gate=120},
};
inline constexpr StyleEvent kFillCDrums[] = {
    {.step=0, .tone=kSnare, .octave=0, .vel=96, .gate=100}, {.step=2, .tone=kSnare, .octave=0, .vel=90, .gate=100}, {.step=4, .tone=kTomHi, .octave=0, .vel=100, .gate=100}, {.step=6, .tone=kTomHi, .octave=0, .vel=98, .gate=100},
    {.step=8, .tone=kTomMid, .octave=0, .vel=104, .gate=100}, {.step=10, .tone=kTomMid, .octave=0, .vel=102, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=110, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=114, .gate=100},
};
inline constexpr StyleEvent kFillDDrums[] = {
    {.step=0, .tone=kSnare, .octave=0, .vel=92, .gate=50}, {.step=1, .tone=kSnare, .octave=0, .vel=86, .gate=50}, {.step=2, .tone=kSnare, .octave=0, .vel=96, .gate=50}, {.step=3, .tone=kSnare, .octave=0, .vel=90, .gate=50},
    {.step=4, .tone=kSnare, .octave=0, .vel=100, .gate=50}, {.step=5, .tone=kSnare, .octave=0, .vel=94, .gate=50}, {.step=6, .tone=kTomHi, .octave=0, .vel=104, .gate=50}, {.step=7, .tone=kTomHi, .octave=0, .vel=98, .gate=50},
    {.step=8, .tone=kTomMid, .octave=0, .vel=106, .gate=50}, {.step=9, .tone=kTomMid, .octave=0, .vel=100, .gate=50}, {.step=10, .tone=kTomLow, .octave=0, .vel=110, .gate=50}, {.step=11, .tone=kTomLow, .octave=0, .vel=104, .gate=50},
    {.step=12, .tone=kTomFloor, .octave=0, .vel=114, .gate=50}, {.step=13, .tone=kTomFloor, .octave=0, .vel=108, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=118, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=120, .gate=240},
};
inline constexpr StylePattern kFillBPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFillBDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)},
};
inline constexpr StylePattern kFillCPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFillCDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)},
};
inline constexpr StylePattern kFillDPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFillDDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)},
};

// ending2: fuller resolving cadence — crash, held chord and bass, mid snare hit.
inline constexpr StyleEvent kEnd2Drums[] = {
    {.step=0, .tone=kCrash, .octave=0, .vel=112, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=116, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=100, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=108, .gate=120},
};
inline constexpr StyleEvent kEnd2Bass[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=104, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=96, .gate=kGateHeld}};
inline constexpr StyleEvent kEnd2Chord[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=90, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=90, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=90, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=90, .gate=kGateHeld},
};
inline constexpr StylePattern kEnd2Patterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kEnd2Drums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEnd2Bass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEnd2Chord)},
};

// varC: half-time feel — backbeat pulled to beat 3, wide space, quarter hats.
inline constexpr StyleEvent kVarCDrums[] = {{.step=0, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=104, .gate=120}, {.step=0, .tone=kClosedHat, .octave=0, .vel=68, .gate=60}, {.step=4, .tone=kClosedHat, .octave=0, .vel=58, .gate=60}, {.step=8, .tone=kClosedHat, .octave=0, .vel=68, .gate=60}, {.step=12, .tone=kClosedHat, .octave=0, .vel=58, .gate=60}};
inline constexpr StyleEvent kVarCBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=96, .gate=kGateHalfBar}, {.step=8, .tone=kFifth, .octave=0, .vel=88, .gate=kGateHalfBar}};
inline constexpr StyleEvent kVarCChord[] = {{.step=0, .tone=kRoot, .octave=0, .vel=74, .gate=kGateHalfBar}, {.step=0, .tone=kThird, .octave=0, .vel=74, .gate=kGateHalfBar}, {.step=0, .tone=kFifth, .octave=0, .vel=74, .gate=kGateHalfBar}, {.step=8, .tone=kThird, .octave=0, .vel=70, .gate=kGateHalfBar}, {.step=8, .tone=kFifth, .octave=0, .vel=70, .gate=kGateHalfBar}, {.step=8, .tone=kSeventh, .octave=0, .vel=70, .gate=kGateHalfBar}};
inline constexpr StylePattern kVarCPatterns[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarCDrums)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarCBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarCChord)}};
// varD: peak — driving eighth bass, kick pushes, 16th hats, full four-note stabs.
inline constexpr StyleEvent kVarDDrums[] = {{.step=0, .tone=kCrash, .octave=0, .vel=104, .gate=kGateBeat}, {.step=0, .tone=kKick, .octave=0, .vel=115, .gate=120}, {.step=3, .tone=kKick, .octave=0, .vel=92, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=11, .tone=kKick, .octave=0, .vel=92, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=110, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=110, .gate=120}, {.step=0, .tone=kClosedHat, .octave=0, .vel=78, .gate=50}, {.step=1, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=2, .tone=kClosedHat, .octave=0, .vel=68, .gate=50}, {.step=3, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=78, .gate=50}, {.step=5, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=6, .tone=kClosedHat, .octave=0, .vel=68, .gate=50}, {.step=7, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=78, .gate=50}, {.step=9, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=68, .gate=50}, {.step=11, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=78, .gate=50}, {.step=13, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=14, .tone=kClosedHat, .octave=0, .vel=68, .gate=50}, {.step=15, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}};
inline constexpr StyleEvent kVarDBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=108, .gate=kGate8th}, {.step=2, .tone=kRoot, .octave=1, .vel=82, .gate=kGateHat}, {.step=4, .tone=kFifth, .octave=0, .vel=98, .gate=kGate8th}, {.step=6, .tone=kFifth, .octave=1, .vel=82, .gate=kGateHat}, {.step=8, .tone=kRoot, .octave=0, .vel=104, .gate=kGate8th}, {.step=10, .tone=kRoot, .octave=1, .vel=82, .gate=kGateHat}, {.step=12, .tone=kFifth, .octave=0, .vel=98, .gate=kGate8th}, {.step=14, .tone=kSeventh, .octave=0, .vel=90, .gate=kGateHat}};
inline constexpr StyleEvent kVarDChord[] = {{.step=0, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=0, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}, {.step=4, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=4, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=4, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}, {.step=4, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=8, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=8, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}, {.step=12, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=12, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=12, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}, {.step=12, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateStab}};
inline constexpr StylePattern kVarDPatterns[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarDDrums)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarDBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarDChord)}};

inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=1, .patterns=Span<const StylePattern>(kIntroPatterns)},
    {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIntro2Patterns)},
    {.type=SectionType::kVarA, .bars=1, .patterns=Span<const StylePattern>(kVarAPatterns)},
    {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kVarBPatterns)},
    {.type=SectionType::kVarC, .bars=1, .patterns=Span<const StylePattern>(kVarCPatterns)},
    {.type=SectionType::kVarD, .bars=1, .patterns=Span<const StylePattern>(kVarDPatterns)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFillPatterns)},
    {.type=SectionType::kFillB, .bars=1, .patterns=Span<const StylePattern>(kFillBPatterns)},
    {.type=SectionType::kFillC, .bars=1, .patterns=Span<const StylePattern>(kFillCPatterns)},
    {.type=SectionType::kFillD, .bars=1, .patterns=Span<const StylePattern>(kFillDPatterns)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kEndPatterns)},
    {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kEnd2Patterns)},
};

inline constexpr Style kStyle{.name="basic", .sections=Span<const StyleSection>(kSections)};

}  // namespace basic

// ---------------------------------------------------------------------------
// "pop": straight 8ths, clean backbeat. Kick on 1 & 3, snare on 2 & 4, steady
// closed-hat 8ths, simple root-fifth bass, triad stabs on the off-beats.
// Bright and mid-velocity.
namespace pop {

inline constexpr StyleEvent kVarADrums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=102, .gate=kGateHat},  {.step=8, .tone=kKick, .octave=0, .vel=98, .gate=kGateHat},
    {.step=4, .tone=kSnare, .octave=0, .vel=100, .gate=kGateHat}, {.step=12, .tone=kSnare, .octave=0, .vel=100, .gate=kGateHat},
    {.step=0, .tone=kClosedHat, .octave=0, .vel=78, .gate=kGateHat},  {.step=2, .tone=kClosedHat, .octave=0, .vel=64, .gate=kGateHat},
    {.step=4, .tone=kClosedHat, .octave=0, .vel=78, .gate=kGateHat},  {.step=6, .tone=kClosedHat, .octave=0, .vel=64, .gate=kGateHat},
    {.step=8, .tone=kClosedHat, .octave=0, .vel=78, .gate=kGateHat},  {.step=10, .tone=kClosedHat, .octave=0, .vel=64, .gate=kGateHat},
    {.step=12, .tone=kClosedHat, .octave=0, .vel=78, .gate=kGateHat}, {.step=14, .tone=kClosedHat, .octave=0, .vel=64, .gate=kGateHat},
};
inline constexpr StyleEvent kVarABass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=98, .gate=kGate8th},  {.step=4, .tone=kFifth, .octave=0, .vel=88, .gate=kGate8th},
    {.step=8, .tone=kRoot, .octave=0, .vel=94, .gate=kGate8th},  {.step=12, .tone=kFifth, .octave=0, .vel=88, .gate=kGate8th},
};
inline constexpr StyleEvent kVarAChord[] = {
    // Off-beat triad stabs (root/3rd/5th) — the classic pop upstroke.
    {.step=2, .tone=kRoot, .octave=0, .vel=88, .gate=kGateStab},  {.step=2, .tone=kThird, .octave=0, .vel=88, .gate=kGateStab},  {.step=2, .tone=kFifth, .octave=0, .vel=88, .gate=kGateStab},
    {.step=6, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab},  {.step=6, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab},  {.step=6, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab},
    {.step=10, .tone=kRoot, .octave=0, .vel=88, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=88, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=88, .gate=kGateStab},
    {.step=14, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab},
};
inline constexpr StylePattern kVarAPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarADrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarABass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarAChord)},
};

inline constexpr StyleEvent kVarBDrums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=104, .gate=kGateHat},  {.step=6, .tone=kKick, .octave=0, .vel=92, .gate=kGateHat},
    {.step=8, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat},  {.step=10, .tone=kKick, .octave=0, .vel=90, .gate=kGateHat},
    {.step=4, .tone=kSnare, .octave=0, .vel=102, .gate=kGateHat}, {.step=12, .tone=kSnare, .octave=0, .vel=102, .gate=kGateHat},
    {.step=4, .tone=kClap, .octave=0, .vel=90, .gate=kGateHat},   {.step=12, .tone=kClap, .octave=0, .vel=90, .gate=kGateHat},
    {.step=0, .tone=kClosedHat, .octave=0, .vel=80, .gate=kGateHat},  {.step=2, .tone=kClosedHat, .octave=0, .vel=66, .gate=kGateHat},
    {.step=4, .tone=kClosedHat, .octave=0, .vel=80, .gate=kGateHat},  {.step=6, .tone=kClosedHat, .octave=0, .vel=66, .gate=kGateHat},
    {.step=8, .tone=kClosedHat, .octave=0, .vel=80, .gate=kGateHat},  {.step=10, .tone=kClosedHat, .octave=0, .vel=66, .gate=kGateHat},
    {.step=12, .tone=kClosedHat, .octave=0, .vel=80, .gate=kGateHat}, {.step=14, .tone=kOpenHat, .octave=0, .vel=78, .gate=kGate8th},
};
inline constexpr StyleEvent kVarBBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=100, .gate=kGate8th}, {.step=6, .tone=kRoot, .octave=0, .vel=78, .gate=kGateHat},
    {.step=8, .tone=kFifth, .octave=0, .vel=92, .gate=kGate8th}, {.step=10, .tone=kRoot, .octave=1, .vel=80, .gate=kGateHat},
    {.step=12, .tone=kFifth, .octave=0, .vel=88, .gate=kGate8th},
};
inline constexpr StyleEvent kVarBChord[] = {
    // Fuller four-note stabs on every off-beat.
    {.step=2, .tone=kRoot, .octave=0, .vel=90, .gate=kGateStab},  {.step=2, .tone=kThird, .octave=0, .vel=90, .gate=kGateStab},  {.step=2, .tone=kFifth, .octave=0, .vel=90, .gate=kGateStab},  {.step=2, .tone=kSeventh, .octave=0, .vel=90, .gate=kGateStab},
    {.step=6, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab},  {.step=6, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab},  {.step=6, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab},
    {.step=10, .tone=kRoot, .octave=0, .vel=90, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=90, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=90, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=90, .gate=kGateStab},
    {.step=14, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab},
};
inline constexpr StylePattern kVarBPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarBDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarBBass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarBChord)},
};

inline constexpr StyleEvent kIntroDrums[] = {
    // Hat pickup crescendo into the downbeat.
    {.step=8, .tone=kClosedHat, .octave=0, .vel=62, .gate=kGateHat},  {.step=10, .tone=kClosedHat, .octave=0, .vel=70, .gate=kGateHat},
    {.step=12, .tone=kClosedHat, .octave=0, .vel=78, .gate=kGateHat}, {.step=14, .tone=kClosedHat, .octave=0, .vel=88, .gate=kGateHat},
};
inline constexpr StyleEvent kIntroBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=90, .gate=kGateHeld},
};
inline constexpr StylePattern kIntroPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIntroDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntroBass)},
};

inline constexpr StyleEvent kFillDrums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat},  {.step=4, .tone=kSnare, .octave=0, .vel=96, .gate=kGateHat},
    {.step=8, .tone=kSnare, .octave=0, .vel=88, .gate=100},       {.step=10, .tone=kSnare, .octave=0, .vel=94, .gate=100},
    {.step=12, .tone=kSnare, .octave=0, .vel=102, .gate=100},     {.step=14, .tone=kSnare, .octave=0, .vel=110, .gate=100},
};
inline constexpr StyleEvent kFillBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=98, .gate=kGate8th}, {.step=8, .tone=kFifth, .octave=0, .vel=88, .gate=kGate8th},
};
inline constexpr StylePattern kFillPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFillDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)},
};

inline constexpr StyleEvent kEndDrums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=110, .gate=kGateHat},
    {.step=0, .tone=kCrash, .octave=0, .vel=104, .gate=kGateHalfBar},
};
inline constexpr StyleEvent kEndBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=98, .gate=kGateHeld},
};
inline constexpr StyleEvent kEndChord[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=88, .gate=kGateHeld},  {.step=0, .tone=kThird, .octave=0, .vel=88, .gate=kGateHeld},
    {.step=0, .tone=kFifth, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=88, .gate=kGateHeld},
};
inline constexpr StylePattern kEndPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kEndDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEndBass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEndChord)},
};

inline constexpr StyleEvent kIntro2Drums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat}, {.step=8, .tone=kKick, .octave=0, .vel=96, .gate=kGateHat}, {.step=4, .tone=kClap, .octave=0, .vel=92, .gate=kGateHat}, {.step=12, .tone=kClap, .octave=0, .vel=92, .gate=kGateHat},
    {.step=0, .tone=kClosedHat, .octave=0, .vel=74, .gate=kGateHat}, {.step=2, .tone=kClosedHat, .octave=0, .vel=60, .gate=kGateHat}, {.step=4, .tone=kClosedHat, .octave=0, .vel=74, .gate=kGateHat}, {.step=6, .tone=kClosedHat, .octave=0, .vel=60, .gate=kGateHat},
    {.step=8, .tone=kClosedHat, .octave=0, .vel=74, .gate=kGateHat}, {.step=10, .tone=kClosedHat, .octave=0, .vel=60, .gate=kGateHat}, {.step=12, .tone=kClosedHat, .octave=0, .vel=74, .gate=kGateHat}, {.step=14, .tone=kOpenHat, .octave=0, .vel=76, .gate=kGate8th},
};
inline constexpr StyleEvent kIntro2Chord[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=78, .gate=kGateBeat}, {.step=0, .tone=kThird, .octave=0, .vel=78, .gate=kGateBeat}, {.step=0, .tone=kFifth, .octave=0, .vel=78, .gate=kGateBeat},
    {.step=8, .tone=kRoot, .octave=0, .vel=76, .gate=kGateBeat}, {.step=8, .tone=kThird, .octave=0, .vel=76, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=76, .gate=kGateBeat},
};
inline constexpr StylePattern kIntro2Patterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIntro2Drums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntroBass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntro2Chord)},
};

inline constexpr StyleEvent kFillBDrums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat}, {.step=4, .tone=kSnare, .octave=0, .vel=98, .gate=120}, {.step=6, .tone=kClap, .octave=0, .vel=90, .gate=120},
    {.step=8, .tone=kSnare, .octave=0, .vel=92, .gate=120}, {.step=10, .tone=kTomMid, .octave=0, .vel=98, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=104, .gate=120}, {.step=14, .tone=kTomLow, .octave=0, .vel=110, .gate=120},
};
inline constexpr StyleEvent kFillCDrums[] = {
    {.step=0, .tone=kSnare, .octave=0, .vel=94, .gate=100}, {.step=2, .tone=kSnare, .octave=0, .vel=88, .gate=100}, {.step=4, .tone=kClap, .octave=0, .vel=98, .gate=100}, {.step=6, .tone=kTomHi, .octave=0, .vel=96, .gate=100},
    {.step=8, .tone=kTomMid, .octave=0, .vel=102, .gate=100}, {.step=10, .tone=kTomMid, .octave=0, .vel=100, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=108, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=114, .gate=100},
};
inline constexpr StyleEvent kFillDDrums[] = {
    {.step=0, .tone=kSnare, .octave=0, .vel=92, .gate=50}, {.step=1, .tone=kSnare, .octave=0, .vel=86, .gate=50}, {.step=2, .tone=kSnare, .octave=0, .vel=98, .gate=50}, {.step=3, .tone=kSnare, .octave=0, .vel=90, .gate=50},
    {.step=4, .tone=kClap, .octave=0, .vel=102, .gate=50}, {.step=5, .tone=kSnare, .octave=0, .vel=94, .gate=50}, {.step=6, .tone=kTomHi, .octave=0, .vel=104, .gate=50}, {.step=7, .tone=kTomHi, .octave=0, .vel=98, .gate=50},
    {.step=8, .tone=kTomMid, .octave=0, .vel=106, .gate=50}, {.step=9, .tone=kTomMid, .octave=0, .vel=100, .gate=50}, {.step=10, .tone=kTomLow, .octave=0, .vel=110, .gate=50}, {.step=11, .tone=kTomLow, .octave=0, .vel=104, .gate=50},
    {.step=12, .tone=kTomFloor, .octave=0, .vel=114, .gate=50}, {.step=13, .tone=kTomFloor, .octave=0, .vel=108, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=118, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=120, .gate=kGate8th},
};
inline constexpr StylePattern kFillBPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFillBDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)},
};
inline constexpr StylePattern kFillCPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFillCDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)},
};
inline constexpr StylePattern kFillDPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFillDDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)},
};

inline constexpr StyleEvent kEnd2Drums[] = {
    {.step=0, .tone=kCrash, .octave=0, .vel=108, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=112, .gate=kGateHat}, {.step=8, .tone=kClap, .octave=0, .vel=98, .gate=kGateHat}, {.step=8, .tone=kKick, .octave=0, .vel=104, .gate=kGateHat},
};
inline constexpr StyleEvent kEnd2Bass[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=100, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=92, .gate=kGateHeld}};
inline constexpr StyleEvent kEnd2Chord[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=88, .gate=kGateHeld},
};
inline constexpr StylePattern kEnd2Patterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kEnd2Drums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEnd2Bass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEnd2Chord)},
};

// varC: four-on-the-floor dance-pop pump — a different lift from the backbeat A/B.
inline constexpr StyleEvent kVarCDrums[] = {{.step=0, .tone=kKick, .octave=0, .vel=104, .gate=kGateHat}, {.step=4, .tone=kKick, .octave=0, .vel=98, .gate=kGateHat}, {.step=8, .tone=kKick, .octave=0, .vel=102, .gate=kGateHat}, {.step=12, .tone=kKick, .octave=0, .vel=98, .gate=kGateHat}, {.step=4, .tone=kClap, .octave=0, .vel=90, .gate=kGateHat}, {.step=12, .tone=kClap, .octave=0, .vel=90, .gate=kGateHat}, {.step=0, .tone=kClosedHat, .octave=0, .vel=74, .gate=kGateHat}, {.step=2, .tone=kClosedHat, .octave=0, .vel=62, .gate=kGateHat}, {.step=4, .tone=kClosedHat, .octave=0, .vel=74, .gate=kGateHat}, {.step=6, .tone=kClosedHat, .octave=0, .vel=62, .gate=kGateHat}, {.step=8, .tone=kClosedHat, .octave=0, .vel=74, .gate=kGateHat}, {.step=10, .tone=kClosedHat, .octave=0, .vel=62, .gate=kGateHat}, {.step=12, .tone=kClosedHat, .octave=0, .vel=74, .gate=kGateHat}, {.step=14, .tone=kClosedHat, .octave=0, .vel=62, .gate=kGateHat}};
inline constexpr StyleEvent kVarCBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=96, .gate=kGate8th}, {.step=4, .tone=kFifth, .octave=0, .vel=86, .gate=kGate8th}, {.step=8, .tone=kRoot, .octave=0, .vel=94, .gate=kGate8th}, {.step=12, .tone=kFifth, .octave=0, .vel=86, .gate=kGate8th}};
inline constexpr StyleEvent kVarCChord[] = {{.step=2, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=82, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStab}};
inline constexpr StylePattern kVarCPatterns[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarCDrums)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarCBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarCChord)}};
// varD: peak — kick pushes, clap-doubled backbeat, 16th hats, driving bass, full 7th stabs.
inline constexpr StyleEvent kVarDDrums[] = {{.step=0, .tone=kKick, .octave=0, .vel=106, .gate=kGateHat}, {.step=6, .tone=kKick, .octave=0, .vel=92, .gate=kGateHat}, {.step=8, .tone=kKick, .octave=0, .vel=102, .gate=kGateHat}, {.step=10, .tone=kKick, .octave=0, .vel=92, .gate=kGateHat}, {.step=4, .tone=kSnare, .octave=0, .vel=104, .gate=kGateHat}, {.step=12, .tone=kSnare, .octave=0, .vel=104, .gate=kGateHat}, {.step=4, .tone=kClap, .octave=0, .vel=94, .gate=kGateHat}, {.step=12, .tone=kClap, .octave=0, .vel=94, .gate=kGateHat}, {.step=0, .tone=kClosedHat, .octave=0, .vel=80, .gate=50}, {.step=1, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=2, .tone=kClosedHat, .octave=0, .vel=70, .gate=50}, {.step=3, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=80, .gate=50}, {.step=5, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=6, .tone=kClosedHat, .octave=0, .vel=70, .gate=50}, {.step=7, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=80, .gate=50}, {.step=9, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=70, .gate=50}, {.step=11, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=80, .gate=50}, {.step=13, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=14, .tone=kOpenHat, .octave=0, .vel=80, .gate=kGate8th}};
inline constexpr StyleEvent kVarDBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=104, .gate=kGate8th}, {.step=2, .tone=kRoot, .octave=1, .vel=82, .gate=kGateHat}, {.step=4, .tone=kFifth, .octave=0, .vel=94, .gate=kGate8th}, {.step=6, .tone=kRoot, .octave=0, .vel=84, .gate=kGateHat}, {.step=8, .tone=kRoot, .octave=0, .vel=100, .gate=kGate8th}, {.step=10, .tone=kRoot, .octave=1, .vel=82, .gate=kGateHat}, {.step=12, .tone=kFifth, .octave=0, .vel=94, .gate=kGate8th}, {.step=14, .tone=kSeventh, .octave=0, .vel=88, .gate=kGateHat}};
inline constexpr StyleEvent kVarDChord[] = {{.step=2, .tone=kRoot, .octave=0, .vel=90, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=90, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=90, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=90, .gate=kGateStab}, {.step=6, .tone=kRoot, .octave=0, .vel=86, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=86, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=86, .gate=kGateStab}, {.step=6, .tone=kSeventh, .octave=0, .vel=86, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=90, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=90, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=90, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=90, .gate=kGateStab}, {.step=14, .tone=kRoot, .octave=0, .vel=86, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=86, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=86, .gate=kGateStab}, {.step=14, .tone=kSeventh, .octave=0, .vel=86, .gate=kGateStab}};
inline constexpr StylePattern kVarDPatterns[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarDDrums)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarDBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarDChord)}};

inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=1, .patterns=Span<const StylePattern>(kIntroPatterns)},
    {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIntro2Patterns)},
    {.type=SectionType::kVarA, .bars=1, .patterns=Span<const StylePattern>(kVarAPatterns)},
    {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kVarBPatterns)},
    {.type=SectionType::kVarC, .bars=1, .patterns=Span<const StylePattern>(kVarCPatterns)},
    {.type=SectionType::kVarD, .bars=1, .patterns=Span<const StylePattern>(kVarDPatterns)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFillPatterns)},
    {.type=SectionType::kFillB, .bars=1, .patterns=Span<const StylePattern>(kFillBPatterns)},
    {.type=SectionType::kFillC, .bars=1, .patterns=Span<const StylePattern>(kFillCPatterns)},
    {.type=SectionType::kFillD, .bars=1, .patterns=Span<const StylePattern>(kFillDPatterns)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kEndPatterns)},
    {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kEnd2Patterns)},
};
inline constexpr Style kStyle{.name="pop", .sections=Span<const StyleSection>(kSections)};

}  // namespace pop

// ---------------------------------------------------------------------------
// "rock": driving and heavier. Busy kick, hard snare backbeat, 8th hats with
// open-hat accents, a crash on the varB downbeat, power (root+fifth) bass with
// octave pushes, power stabs. High velocity.
namespace rock {

inline constexpr StyleEvent kVarADrums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=118, .gate=kGateHat},  {.step=3, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat},
    {.step=8, .tone=kKick, .octave=0, .vel=114, .gate=kGateHat},  {.step=11, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat},
    {.step=4, .tone=kSnare, .octave=0, .vel=116, .gate=kGateHat}, {.step=12, .tone=kSnare, .octave=0, .vel=116, .gate=kGateHat},
    {.step=0, .tone=kClosedHat, .octave=0, .vel=90, .gate=kGateHat},  {.step=2, .tone=kClosedHat, .octave=0, .vel=76, .gate=kGateHat},
    {.step=4, .tone=kClosedHat, .octave=0, .vel=90, .gate=kGateHat},  {.step=6, .tone=kOpenHat, .octave=0, .vel=88, .gate=kGate8th},
    {.step=8, .tone=kClosedHat, .octave=0, .vel=90, .gate=kGateHat},  {.step=10, .tone=kClosedHat, .octave=0, .vel=76, .gate=kGateHat},
    {.step=12, .tone=kClosedHat, .octave=0, .vel=90, .gate=kGateHat}, {.step=14, .tone=kOpenHat, .octave=0, .vel=88, .gate=kGate8th},
};
inline constexpr StyleEvent kVarABass[] = {
    // Power root with an octave push on the off-beats.
    {.step=0, .tone=kRoot, .octave=0, .vel=116, .gate=kGate8th},  {.step=2, .tone=kRoot, .octave=1, .vel=96, .gate=kGateHat},
    {.step=4, .tone=kRoot, .octave=0, .vel=110, .gate=kGate8th},  {.step=6, .tone=kRoot, .octave=1, .vel=96, .gate=kGateHat},
    {.step=8, .tone=kFifth, .octave=0, .vel=112, .gate=kGate8th}, {.step=10, .tone=kFifth, .octave=1, .vel=96, .gate=kGateHat},
    {.step=12, .tone=kRoot, .octave=0, .vel=110, .gate=kGate8th}, {.step=14, .tone=kRoot, .octave=1, .vel=96, .gate=kGateHat},
};
inline constexpr StyleEvent kVarAChord[] = {
    // Power stabs: root + fifth only, held across the beat.
    {.step=0, .tone=kRoot, .octave=0, .vel=110, .gate=kGateBeat}, {.step=0, .tone=kFifth, .octave=0, .vel=110, .gate=kGateBeat},
    {.step=8, .tone=kRoot, .octave=0, .vel=108, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=108, .gate=kGateBeat},
};
inline constexpr StylePattern kVarAPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarADrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarABass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarAChord)},
};

inline constexpr StyleEvent kVarBDrums[] = {
    {.step=0, .tone=kCrash, .octave=0, .vel=118, .gate=kGateBeat},  // crash on the downbeat
    {.step=0, .tone=kKick, .octave=0, .vel=120, .gate=kGateHat},    {.step=2, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat},
    {.step=3, .tone=kKick, .octave=0, .vel=104, .gate=kGateHat},    {.step=8, .tone=kKick, .octave=0, .vel=116, .gate=kGateHat},
    {.step=10, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat},   {.step=11, .tone=kKick, .octave=0, .vel=104, .gate=kGateHat},
    {.step=4, .tone=kSnare, .octave=0, .vel=118, .gate=kGateHat},   {.step=12, .tone=kSnare, .octave=0, .vel=118, .gate=kGateHat},
    {.step=2, .tone=kOpenHat, .octave=0, .vel=92, .gate=kGate8th},  {.step=6, .tone=kOpenHat, .octave=0, .vel=92, .gate=kGate8th},
    {.step=10, .tone=kOpenHat, .octave=0, .vel=92, .gate=kGate8th}, {.step=14, .tone=kOpenHat, .octave=0, .vel=92, .gate=kGate8th},
};
inline constexpr StyleEvent kVarBBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=120, .gate=kGate8th},  {.step=2, .tone=kRoot, .octave=1, .vel=100, .gate=kGateHat},
    {.step=3, .tone=kRoot, .octave=0, .vel=100, .gate=kGateHat},  {.step=4, .tone=kFifth, .octave=0, .vel=112, .gate=kGate8th},
    {.step=8, .tone=kRoot, .octave=0, .vel=116, .gate=kGate8th},  {.step=10, .tone=kSeventh, .octave=0, .vel=100, .gate=kGateHat},
    {.step=12, .tone=kFifth, .octave=0, .vel=112, .gate=kGate8th},
};
inline constexpr StyleEvent kVarBChord[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=112, .gate=kGateBeat},  {.step=0, .tone=kFifth, .octave=0, .vel=112, .gate=kGateBeat},  {.step=0, .tone=kRoot, .octave=1, .vel=112, .gate=kGateBeat},
    {.step=8, .tone=kRoot, .octave=0, .vel=110, .gate=kGateBeat},  {.step=8, .tone=kFifth, .octave=0, .vel=110, .gate=kGateBeat},  {.step=8, .tone=kRoot, .octave=1, .vel=110, .gate=kGateBeat},
    {.step=12, .tone=kRoot, .octave=0, .vel=106, .gate=kGateStab}, {.step=12, .tone=kFifth, .octave=0, .vel=106, .gate=kGateStab},
};
inline constexpr StylePattern kVarBPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarBDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarBBass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarBChord)},
};

inline constexpr StyleEvent kIntroDrums[] = {
    {.step=12, .tone=kSnare, .octave=0, .vel=96, .gate=100},  {.step=13, .tone=kSnare, .octave=0, .vel=104, .gate=100},
    {.step=14, .tone=kSnare, .octave=0, .vel=112, .gate=100}, {.step=15, .tone=kSnare, .octave=0, .vel=120, .gate=100},
};
inline constexpr StyleEvent kIntroBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=110, .gate=kGateHeld},
};
inline constexpr StylePattern kIntroPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIntroDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntroBass)},
};

inline constexpr StyleEvent kFillDrums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=116, .gate=kGateHat}, {.step=4, .tone=kSnare, .octave=0, .vel=110, .gate=100},
    {.step=6, .tone=kSnare, .octave=0, .vel=104, .gate=100},     {.step=8, .tone=kSnare, .octave=0, .vel=108, .gate=100},
    {.step=10, .tone=kSnare, .octave=0, .vel=112, .gate=100},    {.step=12, .tone=kSnare, .octave=0, .vel=116, .gate=100},
    {.step=13, .tone=kSnare, .octave=0, .vel=118, .gate=100},    {.step=14, .tone=kSnare, .octave=0, .vel=120, .gate=100},
    {.step=15, .tone=kSnare, .octave=0, .vel=120, .gate=100},
};
inline constexpr StyleEvent kFillBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=114, .gate=kGate8th}, {.step=8, .tone=kFifth, .octave=0, .vel=108, .gate=kGate8th},
};
inline constexpr StylePattern kFillPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFillDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)},
};

inline constexpr StyleEvent kEndDrums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=120, .gate=kGateHat},
    {.step=0, .tone=kCrash, .octave=0, .vel=118, .gate=kGateHalfBar},
};
inline constexpr StyleEvent kEndBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=116, .gate=kGateHeld},
};
inline constexpr StyleEvent kEndChord[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=112, .gate=kGateHeld},  {.step=0, .tone=kFifth, .octave=0, .vel=112, .gate=kGateHeld},
    {.step=0, .tone=kRoot, .octave=1, .vel=112, .gate=kGateHeld},
};
inline constexpr StylePattern kEndPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kEndDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEndBass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEndChord)},
};

inline constexpr StyleEvent kIntro2Drums[] = {
    {.step=0, .tone=kCrash, .octave=0, .vel=110, .gate=kGateBeat}, {.step=0, .tone=kKick, .octave=0, .vel=118, .gate=kGateHat}, {.step=8, .tone=kKick, .octave=0, .vel=112, .gate=kGateHat}, {.step=4, .tone=kSnare, .octave=0, .vel=112, .gate=kGateHat}, {.step=12, .tone=kSnare, .octave=0, .vel=114, .gate=kGateHat},
    {.step=0, .tone=kClosedHat, .octave=0, .vel=88, .gate=kGateHat}, {.step=2, .tone=kClosedHat, .octave=0, .vel=74, .gate=kGateHat}, {.step=4, .tone=kClosedHat, .octave=0, .vel=88, .gate=kGateHat}, {.step=6, .tone=kClosedHat, .octave=0, .vel=74, .gate=kGateHat},
    {.step=8, .tone=kClosedHat, .octave=0, .vel=88, .gate=kGateHat}, {.step=10, .tone=kClosedHat, .octave=0, .vel=74, .gate=kGateHat}, {.step=12, .tone=kClosedHat, .octave=0, .vel=88, .gate=kGateHat}, {.step=14, .tone=kOpenHat, .octave=0, .vel=86, .gate=kGate8th},
};
inline constexpr StyleEvent kIntro2Chord[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=104, .gate=kGateBeat}, {.step=0, .tone=kFifth, .octave=0, .vel=104, .gate=kGateBeat}, {.step=8, .tone=kRoot, .octave=0, .vel=102, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=102, .gate=kGateBeat},
};
inline constexpr StylePattern kIntro2Patterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIntro2Drums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntroBass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntro2Chord)},
};

inline constexpr StyleEvent kFillBDrums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=118, .gate=kGateHat}, {.step=4, .tone=kSnare, .octave=0, .vel=112, .gate=120}, {.step=6, .tone=kSnare, .octave=0, .vel=106, .gate=120},
    {.step=8, .tone=kTomHi, .octave=0, .vel=110, .gate=120}, {.step=10, .tone=kTomMid, .octave=0, .vel=112, .gate=120}, {.step=12, .tone=kTomLow, .octave=0, .vel=116, .gate=120}, {.step=14, .tone=kTomFloor, .octave=0, .vel=120, .gate=120},
};
inline constexpr StyleEvent kFillCDrums[] = {
    {.step=0, .tone=kTomHi, .octave=0, .vel=110, .gate=100}, {.step=2, .tone=kTomHi, .octave=0, .vel=106, .gate=100}, {.step=4, .tone=kTomMid, .octave=0, .vel=112, .gate=100}, {.step=6, .tone=kTomMid, .octave=0, .vel=108, .gate=100},
    {.step=8, .tone=kSnare, .octave=0, .vel=114, .gate=100}, {.step=10, .tone=kSnare, .octave=0, .vel=110, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=116, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=120, .gate=100},
};
inline constexpr StyleEvent kFillDDrums[] = {
    {.step=0, .tone=kSnare, .octave=0, .vel=104, .gate=50}, {.step=1, .tone=kSnare, .octave=0, .vel=98, .gate=50}, {.step=2, .tone=kTomHi, .octave=0, .vel=108, .gate=50}, {.step=3, .tone=kTomHi, .octave=0, .vel=102, .gate=50},
    {.step=4, .tone=kTomMid, .octave=0, .vel=110, .gate=50}, {.step=5, .tone=kTomMid, .octave=0, .vel=104, .gate=50}, {.step=6, .tone=kTomLow, .octave=0, .vel=112, .gate=50}, {.step=7, .tone=kTomLow, .octave=0, .vel=106, .gate=50},
    {.step=8, .tone=kTomFloor, .octave=0, .vel=114, .gate=50}, {.step=9, .tone=kTomFloor, .octave=0, .vel=108, .gate=50}, {.step=10, .tone=kSnare, .octave=0, .vel=116, .gate=50}, {.step=11, .tone=kSnare, .octave=0, .vel=110, .gate=50},
    {.step=12, .tone=kSnare, .octave=0, .vel=118, .gate=50}, {.step=13, .tone=kSnare, .octave=0, .vel=114, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=120, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=120, .gate=kGate8th},
};
inline constexpr StylePattern kFillBPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFillBDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)},
};
inline constexpr StylePattern kFillCPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFillCDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)},
};
inline constexpr StylePattern kFillDPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFillDDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)},
};

inline constexpr StyleEvent kEnd2Drums[] = {
    {.step=0, .tone=kCrash, .octave=0, .vel=120, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=120, .gate=kGateHat}, {.step=8, .tone=kSnare, .octave=0, .vel=114, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=116, .gate=kGateHat}, {.step=12, .tone=kCrash, .octave=0, .vel=112, .gate=kGateBeat},
};
inline constexpr StyleEvent kEnd2Bass[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=118, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=110, .gate=kGateHeld}};
inline constexpr StyleEvent kEnd2Chord[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=114, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=114, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=1, .vel=114, .gate=kGateHeld},
};
inline constexpr StylePattern kEnd2Patterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kEnd2Drums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEnd2Bass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEnd2Chord)},
};

// varC: half-time stomp — huge kick, backbeat on beat 3, open-hat quarters, held power chord.
inline constexpr StyleEvent kVarCDrums[] = {{.step=0, .tone=kCrash, .octave=0, .vel=110, .gate=kGateBeat}, {.step=0, .tone=kKick, .octave=0, .vel=120, .gate=kGateHat}, {.step=4, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat}, {.step=8, .tone=kKick, .octave=0, .vel=116, .gate=kGateHat}, {.step=8, .tone=kSnare, .octave=0, .vel=118, .gate=kGateHat}, {.step=0, .tone=kOpenHat, .octave=0, .vel=90, .gate=kGate8th}, {.step=4, .tone=kOpenHat, .octave=0, .vel=86, .gate=kGate8th}, {.step=8, .tone=kOpenHat, .octave=0, .vel=90, .gate=kGate8th}, {.step=12, .tone=kOpenHat, .octave=0, .vel=86, .gate=kGate8th}};
inline constexpr StyleEvent kVarCBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=116, .gate=kGate8th}, {.step=4, .tone=kRoot, .octave=1, .vel=96, .gate=kGateHat}, {.step=8, .tone=kFifth, .octave=0, .vel=112, .gate=kGate8th}, {.step=12, .tone=kRoot, .octave=1, .vel=96, .gate=kGateHat}};
inline constexpr StyleEvent kVarCChord[] = {{.step=0, .tone=kRoot, .octave=0, .vel=110, .gate=kGateHalfBar}, {.step=0, .tone=kFifth, .octave=0, .vel=110, .gate=kGateHalfBar}, {.step=8, .tone=kRoot, .octave=0, .vel=108, .gate=kGateHalfBar}, {.step=8, .tone=kFifth, .octave=0, .vel=108, .gate=kGateHalfBar}};
inline constexpr StylePattern kVarCPatterns[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarCDrums)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarCBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarCChord)}};
// varD: peak — four-on-the-floor eighth kick, crash, open-hat accents, power+7th stabs.
inline constexpr StyleEvent kVarDDrums[] = {{.step=0, .tone=kCrash, .octave=0, .vel=118, .gate=kGateBeat}, {.step=0, .tone=kKick, .octave=0, .vel=120, .gate=kGateHat}, {.step=2, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat}, {.step=4, .tone=kKick, .octave=0, .vel=110, .gate=kGateHat}, {.step=6, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat}, {.step=8, .tone=kKick, .octave=0, .vel=116, .gate=kGateHat}, {.step=10, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat}, {.step=12, .tone=kKick, .octave=0, .vel=110, .gate=kGateHat}, {.step=14, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat}, {.step=4, .tone=kSnare, .octave=0, .vel=118, .gate=kGateHat}, {.step=12, .tone=kSnare, .octave=0, .vel=118, .gate=kGateHat}, {.step=2, .tone=kOpenHat, .octave=0, .vel=92, .gate=kGate8th}, {.step=6, .tone=kOpenHat, .octave=0, .vel=92, .gate=kGate8th}, {.step=10, .tone=kOpenHat, .octave=0, .vel=92, .gate=kGate8th}, {.step=14, .tone=kOpenHat, .octave=0, .vel=92, .gate=kGate8th}};
inline constexpr StyleEvent kVarDBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=120, .gate=kGate8th}, {.step=2, .tone=kRoot, .octave=1, .vel=100, .gate=kGateHat}, {.step=4, .tone=kFifth, .octave=0, .vel=112, .gate=kGate8th}, {.step=6, .tone=kRoot, .octave=1, .vel=100, .gate=kGateHat}, {.step=8, .tone=kRoot, .octave=0, .vel=116, .gate=kGate8th}, {.step=10, .tone=kSeventh, .octave=0, .vel=100, .gate=kGateHat}, {.step=12, .tone=kFifth, .octave=0, .vel=112, .gate=kGate8th}, {.step=14, .tone=kRoot, .octave=1, .vel=100, .gate=kGateHat}};
inline constexpr StyleEvent kVarDChord[] = {{.step=0, .tone=kRoot, .octave=0, .vel=114, .gate=kGateBeat}, {.step=0, .tone=kFifth, .octave=0, .vel=114, .gate=kGateBeat}, {.step=0, .tone=kSeventh, .octave=0, .vel=114, .gate=kGateBeat}, {.step=0, .tone=kRoot, .octave=1, .vel=114, .gate=kGateBeat}, {.step=8, .tone=kRoot, .octave=0, .vel=112, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=112, .gate=kGateBeat}, {.step=8, .tone=kSeventh, .octave=0, .vel=112, .gate=kGateBeat}, {.step=8, .tone=kRoot, .octave=1, .vel=112, .gate=kGateBeat}, {.step=12, .tone=kRoot, .octave=0, .vel=108, .gate=kGateStab}, {.step=12, .tone=kFifth, .octave=0, .vel=108, .gate=kGateStab}, {.step=12, .tone=kSeventh, .octave=0, .vel=108, .gate=kGateStab}};
inline constexpr StylePattern kVarDPatterns[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarDDrums)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarDBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarDChord)}};

inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=1, .patterns=Span<const StylePattern>(kIntroPatterns)},
    {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIntro2Patterns)},
    {.type=SectionType::kVarA, .bars=1, .patterns=Span<const StylePattern>(kVarAPatterns)},
    {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kVarBPatterns)},
    {.type=SectionType::kVarC, .bars=1, .patterns=Span<const StylePattern>(kVarCPatterns)},
    {.type=SectionType::kVarD, .bars=1, .patterns=Span<const StylePattern>(kVarDPatterns)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFillPatterns)},
    {.type=SectionType::kFillB, .bars=1, .patterns=Span<const StylePattern>(kFillBPatterns)},
    {.type=SectionType::kFillC, .bars=1, .patterns=Span<const StylePattern>(kFillCPatterns)},
    {.type=SectionType::kFillD, .bars=1, .patterns=Span<const StylePattern>(kFillDPatterns)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kEndPatterns)},
    {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kEnd2Patterns)},
};
inline constexpr Style kStyle{.name="rock", .sections=Span<const StyleSection>(kSections)};

}  // namespace rock

// ---------------------------------------------------------------------------
// "ballad": slow, sparse, gentle. Soft kick with a rimshot backbeat and an
// occasional ride, the chord spread as a rising arpeggio (tones 0..3 across the
// bar) instead of a block stab, a soft legato root bass. Low velocity, long
// gates.
namespace ballad {

inline constexpr StyleEvent kVarADrums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=72, .gate=kGateHat},
    {.step=4, .tone=kRimshot, .octave=0, .vel=60, .gate=kGateHat},   {.step=12, .tone=kRimshot, .octave=0, .vel=62, .gate=kGateHat},
    {.step=0, .tone=kRide, .octave=0, .vel=58, .gate=kGate8th},      {.step=8, .tone=kRide, .octave=0, .vel=56, .gate=kGate8th},
};
inline constexpr StyleEvent kVarABass[] = {
    // Legato root under the whole bar.
    {.step=0, .tone=kRoot, .octave=0, .vel=74, .gate=kGateHeld},
};
inline constexpr StyleEvent kVarAChord[] = {
    // Rising arpeggio: one chord tone per beat, each sustaining so they stack.
    {.step=0, .tone=kRoot, .octave=0, .vel=70, .gate=kGateHeld},
    {.step=4, .tone=kThird, .octave=0, .vel=68, .gate=kGateHalfBar},
    {.step=8, .tone=kFifth, .octave=0, .vel=72, .gate=kGateHalfBar},
    {.step=12, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateHalfBar},
};
inline constexpr StylePattern kVarAPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarADrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarABass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarAChord)},
};

inline constexpr StyleEvent kVarBDrums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=76, .gate=kGateHat},     {.step=8, .tone=kKick, .octave=0, .vel=70, .gate=kGateHat},
    {.step=4, .tone=kRimshot, .octave=0, .vel=64, .gate=kGateHat},  {.step=12, .tone=kRimshot, .octave=0, .vel=66, .gate=kGateHat},
    {.step=0, .tone=kRide, .octave=0, .vel=60, .gate=kGate8th},     {.step=4, .tone=kRide, .octave=0, .vel=54, .gate=kGate8th},
    {.step=8, .tone=kRide, .octave=0, .vel=60, .gate=kGate8th},     {.step=12, .tone=kRide, .octave=0, .vel=54, .gate=kGate8th},
};
inline constexpr StyleEvent kVarBBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=76, .gate=kGateHalfBar}, {.step=8, .tone=kFifth, .octave=0, .vel=72, .gate=kGateHalfBar},
};
inline constexpr StyleEvent kVarBChord[] = {
    // Denser arpeggio on the 8ths, still sustaining into a spread chord.
    {.step=0, .tone=kRoot, .octave=0, .vel=74, .gate=kGateHeld},    {.step=2, .tone=kThird, .octave=0, .vel=66, .gate=kGateHalfBar},
    {.step=4, .tone=kFifth, .octave=0, .vel=72, .gate=kGateHalfBar}, {.step=6, .tone=kSeventh, .octave=0, .vel=64, .gate=kGateHalfBar},
    {.step=8, .tone=kThird, .octave=1, .vel=70, .gate=kGateHalfBar}, {.step=12, .tone=kFifth, .octave=0, .vel=66, .gate=kGateHalfBar},
};
inline constexpr StylePattern kVarBPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarBDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarBBass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarBChord)},
};

inline constexpr StyleEvent kIntroDrums[] = {
    {.step=0, .tone=kRide, .octave=0, .vel=52, .gate=kGate8th}, {.step=8, .tone=kRide, .octave=0, .vel=58, .gate=kGate8th},
};
inline constexpr StyleEvent kIntroBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=70, .gate=kGateHeld},
};
inline constexpr StyleEvent kIntroChord[] = {
    {.step=8, .tone=kRoot, .octave=0, .vel=62, .gate=kGateHalfBar},  {.step=12, .tone=kThird, .octave=0, .vel=64, .gate=kGateHalfBar},
};
inline constexpr StylePattern kIntroPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIntroDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntroBass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntroChord)},
};

inline constexpr StyleEvent kFillDrums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=74, .gate=kGateHat},   {.step=8, .tone=kRimshot, .octave=0, .vel=66, .gate=100},
    {.step=10, .tone=kRimshot, .octave=0, .vel=72, .gate=100},    {.step=12, .tone=kSnare, .octave=0, .vel=78, .gate=100},
    {.step=14, .tone=kSnare, .octave=0, .vel=84, .gate=100},
};
inline constexpr StyleEvent kFillBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=74, .gate=kGateHeld},
};
inline constexpr StylePattern kFillPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFillDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)},
};

inline constexpr StyleEvent kEndDrums[] = {
    {.step=0, .tone=kCrash, .octave=0, .vel=80, .gate=kGateHeld},
    {.step=0, .tone=kKick, .octave=0, .vel=78, .gate=kGateHat},
};
inline constexpr StyleEvent kEndBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=76, .gate=kGateHeld},
};
inline constexpr StyleEvent kEndChord[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=72, .gate=kGateHeld},  {.step=0, .tone=kThird, .octave=0, .vel=72, .gate=kGateHeld},
    {.step=0, .tone=kFifth, .octave=0, .vel=72, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateHeld},
};
inline constexpr StylePattern kEndPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kEndDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEndBass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEndChord)},
};

inline constexpr StyleEvent kIntro2Drums[] = {
    {.step=0, .tone=kRide, .octave=0, .vel=54, .gate=kGate8th}, {.step=4, .tone=kRide, .octave=0, .vel=48, .gate=kGate8th}, {.step=8, .tone=kRide, .octave=0, .vel=56, .gate=kGate8th}, {.step=12, .tone=kRide, .octave=0, .vel=50, .gate=kGate8th},
    {.step=0, .tone=kKick, .octave=0, .vel=66, .gate=kGateHat}, {.step=12, .tone=kRimshot, .octave=0, .vel=58, .gate=kGateHat},
};
inline constexpr StyleEvent kIntro2Chord[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=62, .gate=kGateHeld}, {.step=4, .tone=kThird, .octave=0, .vel=60, .gate=kGateHalfBar}, {.step=8, .tone=kFifth, .octave=0, .vel=64, .gate=kGateHalfBar}, {.step=12, .tone=kSeventh, .octave=0, .vel=58, .gate=kGateHalfBar},
};
inline constexpr StylePattern kIntro2Patterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIntro2Drums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntroBass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntro2Chord)},
};

inline constexpr StyleEvent kFillBDrums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=72, .gate=kGateHat}, {.step=8, .tone=kRimshot, .octave=0, .vel=64, .gate=120}, {.step=10, .tone=kRimshot, .octave=0, .vel=70, .gate=120}, {.step=12, .tone=kTomMid, .octave=0, .vel=74, .gate=120}, {.step=14, .tone=kTomLow, .octave=0, .vel=80, .gate=120},
};
inline constexpr StyleEvent kFillCDrums[] = {
    {.step=0, .tone=kTomHi, .octave=0, .vel=68, .gate=100}, {.step=4, .tone=kTomHi, .octave=0, .vel=70, .gate=100}, {.step=6, .tone=kTomMid, .octave=0, .vel=74, .gate=100}, {.step=8, .tone=kTomMid, .octave=0, .vel=76, .gate=100},
    {.step=10, .tone=kTomLow, .octave=0, .vel=80, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=82, .gate=100}, {.step=14, .tone=kSnare, .octave=0, .vel=88, .gate=100},
};
inline constexpr StyleEvent kFillDDrums[] = {
    {.step=0, .tone=kTomHi, .octave=0, .vel=70, .gate=100}, {.step=2, .tone=kTomHi, .octave=0, .vel=66, .gate=100}, {.step=4, .tone=kTomMid, .octave=0, .vel=74, .gate=100}, {.step=6, .tone=kTomMid, .octave=0, .vel=70, .gate=100},
    {.step=8, .tone=kTomLow, .octave=0, .vel=80, .gate=100}, {.step=10, .tone=kTomLow, .octave=0, .vel=76, .gate=100}, {.step=12, .tone=kTomFloor, .octave=0, .vel=86, .gate=100}, {.step=13, .tone=kSnare, .octave=0, .vel=90, .gate=100}, {.step=14, .tone=kSnare, .octave=0, .vel=96, .gate=100}, {.step=15, .tone=kSnare, .octave=0, .vel=100, .gate=100},
};
inline constexpr StylePattern kFillBPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFillBDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)},
};
inline constexpr StylePattern kFillCPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFillCDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)},
};
inline constexpr StylePattern kFillDPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFillDDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)},
};

inline constexpr StyleEvent kEnd2Drums[] = {
    {.step=0, .tone=kCrash, .octave=0, .vel=82, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=80, .gate=kGateHat}, {.step=8, .tone=kRide, .octave=0, .vel=56, .gate=kGate8th},
};
inline constexpr StyleEvent kEnd2Bass[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=78, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=70, .gate=kGateHeld}};
inline constexpr StyleEvent kEnd2Chord[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=72, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=72, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=72, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=1, .vel=68, .gate=kGateHeld},
};
inline constexpr StylePattern kEnd2Patterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kEnd2Drums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEnd2Bass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEnd2Chord)},
};

// varC: flowing 6/8-lilt broken arpeggio — a rolling feel vs A/B's long sustains.
inline constexpr StyleEvent kVarCDrums[] = {{.step=0, .tone=kKick, .octave=0, .vel=72, .gate=kGateHat}, {.step=8, .tone=kKick, .octave=0, .vel=66, .gate=kGateHat}, {.step=4, .tone=kRimshot, .octave=0, .vel=60, .gate=kGateHat}, {.step=12, .tone=kRimshot, .octave=0, .vel=62, .gate=kGateHat}, {.step=0, .tone=kRide, .octave=0, .vel=58, .gate=kGate8th}, {.step=2, .tone=kRide, .octave=0, .vel=50, .gate=kGate8th}, {.step=4, .tone=kRide, .octave=0, .vel=56, .gate=kGate8th}, {.step=6, .tone=kRide, .octave=0, .vel=50, .gate=kGate8th}, {.step=8, .tone=kRide, .octave=0, .vel=58, .gate=kGate8th}, {.step=10, .tone=kRide, .octave=0, .vel=50, .gate=kGate8th}, {.step=12, .tone=kRide, .octave=0, .vel=56, .gate=kGate8th}, {.step=14, .tone=kRide, .octave=0, .vel=50, .gate=kGate8th}};
inline constexpr StyleEvent kVarCBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=74, .gate=kGateHalfBar}, {.step=8, .tone=kFifth, .octave=-1, .vel=70, .gate=kGateHalfBar}};
inline constexpr StyleEvent kVarCChord[] = {{.step=0, .tone=kRoot, .octave=0, .vel=66, .gate=kGate8th}, {.step=2, .tone=kThird, .octave=0, .vel=62, .gate=kGate8th}, {.step=4, .tone=kFifth, .octave=0, .vel=66, .gate=kGate8th}, {.step=6, .tone=kSeventh, .octave=0, .vel=62, .gate=kGate8th}, {.step=8, .tone=kRoot, .octave=1, .vel=66, .gate=kGate8th}, {.step=10, .tone=kThird, .octave=1, .vel=62, .gate=kGate8th}, {.step=12, .tone=kFifth, .octave=0, .vel=64, .gate=kGate8th}, {.step=14, .tone=kSeventh, .octave=0, .vel=60, .gate=kGate8th}};
inline constexpr StylePattern kVarCPatterns[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarCDrums)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarCBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarCChord)}};
// varD: fullest — snare backbeat, ride eighths, gentle walking bass, full spread 7th chord.
inline constexpr StyleEvent kVarDDrums[] = {{.step=0, .tone=kKick, .octave=0, .vel=80, .gate=kGateHat}, {.step=8, .tone=kKick, .octave=0, .vel=76, .gate=kGateHat}, {.step=4, .tone=kSnare, .octave=0, .vel=84, .gate=kGateHat}, {.step=12, .tone=kSnare, .octave=0, .vel=86, .gate=kGateHat}, {.step=0, .tone=kRide, .octave=0, .vel=64, .gate=kGate8th}, {.step=2, .tone=kRide, .octave=0, .vel=56, .gate=kGate8th}, {.step=4, .tone=kRide, .octave=0, .vel=64, .gate=kGate8th}, {.step=6, .tone=kRide, .octave=0, .vel=56, .gate=kGate8th}, {.step=8, .tone=kRide, .octave=0, .vel=64, .gate=kGate8th}, {.step=10, .tone=kRide, .octave=0, .vel=56, .gate=kGate8th}, {.step=12, .tone=kRide, .octave=0, .vel=64, .gate=kGate8th}, {.step=14, .tone=kRide, .octave=0, .vel=56, .gate=kGate8th}};
inline constexpr StyleEvent kVarDBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=80, .gate=kGateBeat}, {.step=4, .tone=kThird, .octave=0, .vel=74, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=78, .gate=kGateBeat}, {.step=12, .tone=kSeventh, .octave=0, .vel=74, .gate=kGateBeat}};
inline constexpr StyleEvent kVarDChord[] = {{.step=0, .tone=kRoot, .octave=0, .vel=74, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=72, .gate=kGateHeld}, {.step=4, .tone=kFifth, .octave=0, .vel=72, .gate=kGateHalfBar}, {.step=6, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateHalfBar}, {.step=8, .tone=kThird, .octave=1, .vel=70, .gate=kGateHalfBar}, {.step=8, .tone=kFifth, .octave=0, .vel=68, .gate=kGateHalfBar}, {.step=12, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateHalfBar}, {.step=12, .tone=kRoot, .octave=1, .vel=64, .gate=kGateHalfBar}};
inline constexpr StylePattern kVarDPatterns[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarDDrums)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarDBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarDChord)}};

inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=1, .patterns=Span<const StylePattern>(kIntroPatterns)},
    {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIntro2Patterns)},
    {.type=SectionType::kVarA, .bars=1, .patterns=Span<const StylePattern>(kVarAPatterns)},
    {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kVarBPatterns)},
    {.type=SectionType::kVarC, .bars=1, .patterns=Span<const StylePattern>(kVarCPatterns)},
    {.type=SectionType::kVarD, .bars=1, .patterns=Span<const StylePattern>(kVarDPatterns)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFillPatterns)},
    {.type=SectionType::kFillB, .bars=1, .patterns=Span<const StylePattern>(kFillBPatterns)},
    {.type=SectionType::kFillC, .bars=1, .patterns=Span<const StylePattern>(kFillCPatterns)},
    {.type=SectionType::kFillD, .bars=1, .patterns=Span<const StylePattern>(kFillDPatterns)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kEndPatterns)},
    {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kEnd2Patterns)},
};
inline constexpr Style kStyle{.name="ballad", .sections=Span<const StyleSection>(kSections)};

}  // namespace ballad

// ===========================================================================
// New genre library. Every style below defines the full 10-section vocabulary
// (intro1/intro2, varA/varB, fillA..D, ending1/ending2). Drums stay on kFixed
// GM notes; bass + chord stay on kChordTone so NTT transposes them live.
// ===========================================================================

// ---------------------------------------------------------------------------
// "funk": syncopated 16th kick, ghost-snare backbeat, tight 16th hats, staccato
// single-note root-pop bass, muted dominant stabs on the off-16ths.
namespace funk {
inline constexpr StyleEvent kHeldBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=104, .gate=kGateHeld}};
inline constexpr StyleEvent kFillBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=104, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=98, .gate=kGateStab}};

inline constexpr StyleEvent kIn1D[] = {
    {.step=10, .tone=kClosedHat, .octave=0, .vel=64, .gate=50}, {.step=11, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=12, .tone=kSnare, .octave=0, .vel=80, .gate=100}, {.step=14, .tone=kSnare, .octave=0, .vel=92, .gate=100}, {.step=15, .tone=kKick, .octave=0, .vel=100, .gate=120},
};
inline constexpr StylePattern kIn1P[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn1D)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)},
};
inline constexpr StyleEvent kIn2D[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=6, .tone=kKick, .octave=0, .vel=96, .gate=120}, {.step=10, .tone=kKick, .octave=0, .vel=98, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=104, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=104, .gate=120},
    {.step=0, .tone=kClosedHat, .octave=0, .vel=78, .gate=50}, {.step=2, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=74, .gate=50}, {.step=6, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=78, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=74, .gate=50}, {.step=14, .tone=kOpenHat, .octave=0, .vel=70, .gate=120},
};
inline constexpr StyleEvent kIn2C[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateStab},
    {.step=10, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=10, .tone=kThird, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=10, .tone=kSeventh, .octave=0, .vel=76, .gate=kGateStaccato},
};
inline constexpr StylePattern kIn2P[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn2D)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIn2C)},
};
inline constexpr StyleEvent kAD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=115, .gate=120}, {.step=3, .tone=kKick, .octave=0, .vel=92, .gate=120}, {.step=6, .tone=kKick, .octave=0, .vel=100, .gate=120}, {.step=10, .tone=kKick, .octave=0, .vel=98, .gate=120},
    {.step=4, .tone=kSnare, .octave=0, .vel=110, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=110, .gate=120}, {.step=7, .tone=kSnare, .octave=0, .vel=48, .gate=50}, {.step=11, .tone=kSnare, .octave=0, .vel=46, .gate=50},
    {.step=0, .tone=kClosedHat, .octave=0, .vel=80, .gate=50}, {.step=2, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=72, .gate=50}, {.step=6, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=80, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=72, .gate=50}, {.step=14, .tone=kClosedHat, .octave=0, .vel=58, .gate=50},
};
inline constexpr StyleEvent kAB[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=112, .gate=kGateStaccato}, {.step=3, .tone=kRoot, .octave=0, .vel=90, .gate=kGateStaccato}, {.step=6, .tone=kRoot, .octave=1, .vel=96, .gate=kGateStaccato}, {.step=8, .tone=kFifth, .octave=0, .vel=100, .gate=kGateStaccato}, {.step=10, .tone=kRoot, .octave=0, .vel=94, .gate=kGateStaccato}, {.step=14, .tone=kSeventh, .octave=0, .vel=90, .gate=kGateStaccato},
};
inline constexpr StyleEvent kAC[] = {
    {.step=2, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStaccato}, {.step=2, .tone=kThird, .octave=0, .vel=84, .gate=kGateStaccato}, {.step=2, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStaccato},
    {.step=7, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=7, .tone=kThird, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=7, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStaccato},
    {.step=10, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStaccato}, {.step=10, .tone=kThird, .octave=0, .vel=84, .gate=kGateStaccato}, {.step=10, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStaccato},
};
inline constexpr StylePattern kAP[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kAD)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAB)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAC)},
};
inline constexpr StyleEvent kBD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=118, .gate=120}, {.step=3, .tone=kKick, .octave=0, .vel=96, .gate=120}, {.step=6, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=108, .gate=120}, {.step=10, .tone=kKick, .octave=0, .vel=100, .gate=120}, {.step=13, .tone=kKick, .octave=0, .vel=94, .gate=120},
    {.step=4, .tone=kSnare, .octave=0, .vel=114, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=114, .gate=120}, {.step=2, .tone=kSnare, .octave=0, .vel=46, .gate=50}, {.step=7, .tone=kSnare, .octave=0, .vel=50, .gate=50}, {.step=11, .tone=kSnare, .octave=0, .vel=48, .gate=50}, {.step=15, .tone=kSnare, .octave=0, .vel=52, .gate=50},
    {.step=0, .tone=kClosedHat, .octave=0, .vel=82, .gate=50}, {.step=2, .tone=kOpenHat, .octave=0, .vel=72, .gate=120}, {.step=4, .tone=kClosedHat, .octave=0, .vel=74, .gate=50}, {.step=6, .tone=kOpenHat, .octave=0, .vel=72, .gate=120}, {.step=8, .tone=kClosedHat, .octave=0, .vel=82, .gate=50}, {.step=10, .tone=kOpenHat, .octave=0, .vel=72, .gate=120}, {.step=12, .tone=kClosedHat, .octave=0, .vel=74, .gate=50}, {.step=14, .tone=kOpenHat, .octave=0, .vel=74, .gate=120},
};
inline constexpr StyleEvent kBB[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=115, .gate=kGateStaccato}, {.step=2, .tone=kRoot, .octave=1, .vel=92, .gate=kGateStaccato}, {.step=3, .tone=kRoot, .octave=0, .vel=94, .gate=kGateStaccato}, {.step=6, .tone=kFifth, .octave=0, .vel=98, .gate=kGateStaccato}, {.step=8, .tone=kRoot, .octave=0, .vel=104, .gate=kGateStaccato}, {.step=10, .tone=kSeventh, .octave=0, .vel=96, .gate=kGateStaccato}, {.step=11, .tone=kRoot, .octave=1, .vel=90, .gate=kGateStaccato}, {.step=14, .tone=kFifth, .octave=0, .vel=94, .gate=kGateStaccato},
};
inline constexpr StyleEvent kBC[] = {
    {.step=2, .tone=kRoot, .octave=0, .vel=86, .gate=kGateStaccato}, {.step=2, .tone=kThird, .octave=0, .vel=86, .gate=kGateStaccato}, {.step=2, .tone=kFifth, .octave=0, .vel=86, .gate=kGateStaccato}, {.step=2, .tone=kSeventh, .octave=0, .vel=86, .gate=kGateStaccato},
    {.step=6, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=6, .tone=kThird, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=6, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateStaccato},
    {.step=10, .tone=kRoot, .octave=0, .vel=86, .gate=kGateStaccato}, {.step=10, .tone=kThird, .octave=0, .vel=86, .gate=kGateStaccato}, {.step=10, .tone=kFifth, .octave=0, .vel=86, .gate=kGateStaccato}, {.step=10, .tone=kSeventh, .octave=0, .vel=86, .gate=kGateStaccato},
    {.step=14, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=14, .tone=kThird, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=14, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateStaccato},
};
inline constexpr StylePattern kBP[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kBD)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBB)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBC)},
};
inline constexpr StyleEvent kFAD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=104, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=96, .gate=120}, {.step=10, .tone=kSnare, .octave=0, .vel=100, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=106, .gate=120}, {.step=14, .tone=kSnare, .octave=0, .vel=112, .gate=120},
};
inline constexpr StyleEvent kFBD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=2, .tone=kSnare, .octave=0, .vel=90, .gate=100}, {.step=4, .tone=kSnare, .octave=0, .vel=100, .gate=100}, {.step=6, .tone=kTomHi, .octave=0, .vel=96, .gate=100}, {.step=8, .tone=kTomMid, .octave=0, .vel=102, .gate=100}, {.step=10, .tone=kSnare, .octave=0, .vel=104, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=108, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=112, .gate=100},
};
inline constexpr StyleEvent kFCD[] = {
    {.step=0, .tone=kSnare, .octave=0, .vel=92, .gate=50}, {.step=2, .tone=kSnare, .octave=0, .vel=86, .gate=50}, {.step=3, .tone=kSnare, .octave=0, .vel=90, .gate=50}, {.step=4, .tone=kTomHi, .octave=0, .vel=100, .gate=50}, {.step=6, .tone=kTomHi, .octave=0, .vel=96, .gate=50}, {.step=8, .tone=kTomMid, .octave=0, .vel=104, .gate=50}, {.step=10, .tone=kTomMid, .octave=0, .vel=100, .gate=50}, {.step=12, .tone=kTomLow, .octave=0, .vel=110, .gate=50}, {.step=14, .tone=kTomFloor, .octave=0, .vel=114, .gate=50},
};
inline constexpr StyleEvent kFDD[] = {
    {.step=0, .tone=kSnare, .octave=0, .vel=92, .gate=50}, {.step=1, .tone=kSnare, .octave=0, .vel=86, .gate=50}, {.step=2, .tone=kSnare, .octave=0, .vel=96, .gate=50}, {.step=3, .tone=kSnare, .octave=0, .vel=90, .gate=50}, {.step=4, .tone=kSnare, .octave=0, .vel=100, .gate=50}, {.step=5, .tone=kTomHi, .octave=0, .vel=98, .gate=50}, {.step=6, .tone=kTomHi, .octave=0, .vel=102, .gate=50}, {.step=7, .tone=kTomMid, .octave=0, .vel=100, .gate=50},
    {.step=8, .tone=kTomMid, .octave=0, .vel=106, .gate=50}, {.step=9, .tone=kTomLow, .octave=0, .vel=104, .gate=50}, {.step=10, .tone=kTomLow, .octave=0, .vel=108, .gate=50}, {.step=11, .tone=kTomFloor, .octave=0, .vel=110, .gate=50}, {.step=12, .tone=kTomFloor, .octave=0, .vel=114, .gate=50}, {.step=13, .tone=kSnare, .octave=0, .vel=116, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=118, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=120, .gate=kGate8th},
};
inline constexpr StylePattern kFAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)}};
inline constexpr StylePattern kFBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)}};
inline constexpr StylePattern kFCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)}};
inline constexpr StylePattern kFDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)}};
inline constexpr StyleEvent kE1D[] = {{.step=0, .tone=kKick, .octave=0, .vel=116, .gate=120}, {.step=0, .tone=kCrash, .octave=0, .vel=108, .gate=kGateHalfBar}};
inline constexpr StyleEvent kE1B[] = {{.step=0, .tone=kRoot, .octave=0, .vel=110, .gate=kGateHeld}};
inline constexpr StyleEvent kE1C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=88, .gate=kGateHeld}};
inline constexpr StylePattern kE1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1C)}};
inline constexpr StyleEvent kE2D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=114, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=118, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=110, .gate=120}};
inline constexpr StyleEvent kE2B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=112, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=100, .gate=kGateHeld}};
inline constexpr StyleEvent kE2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=90, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=90, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=90, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=90, .gate=kGateHeld}};
inline constexpr StylePattern kE2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2C)}};
// varC: "on the one" half-time funk — heavy downbeat, backbeat on 3, ghost snares.
inline constexpr StyleEvent kCD[] = {{.step=0, .tone=kKick, .octave=0, .vel=118, .gate=120}, {.step=6, .tone=kKick, .octave=0, .vel=88, .gate=120}, {.step=10, .tone=kKick, .octave=0, .vel=92, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=110, .gate=120}, {.step=3, .tone=kSnare, .octave=0, .vel=44, .gate=50}, {.step=13, .tone=kSnare, .octave=0, .vel=46, .gate=50}, {.step=0, .tone=kClosedHat, .octave=0, .vel=74, .gate=50}, {.step=2, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=70, .gate=50}, {.step=6, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=74, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=70, .gate=50}, {.step=14, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}};
inline constexpr StyleEvent kCC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=0, .tone=kThird, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=0, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=8, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=8, .tone=kThird, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=8, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStaccato}};
inline constexpr StylePattern kCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAB)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCC)}};
// varD: peak double-pump — busiest kit, 16th hats with opens, driving bass, full 7th stabs.
inline constexpr StyleEvent kDD[] = {{.step=0, .tone=kKick, .octave=0, .vel=120, .gate=120}, {.step=3, .tone=kKick, .octave=0, .vel=96, .gate=120}, {.step=6, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=10, .tone=kKick, .octave=0, .vel=100, .gate=120}, {.step=13, .tone=kKick, .octave=0, .vel=94, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=116, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=116, .gate=120}, {.step=2, .tone=kSnare, .octave=0, .vel=46, .gate=50}, {.step=7, .tone=kSnare, .octave=0, .vel=50, .gate=50}, {.step=11, .tone=kSnare, .octave=0, .vel=48, .gate=50}, {.step=15, .tone=kSnare, .octave=0, .vel=52, .gate=50}, {.step=0, .tone=kClosedHat, .octave=0, .vel=82, .gate=50}, {.step=2, .tone=kOpenHat, .octave=0, .vel=72, .gate=120}, {.step=4, .tone=kClosedHat, .octave=0, .vel=74, .gate=50}, {.step=6, .tone=kOpenHat, .octave=0, .vel=72, .gate=120}, {.step=8, .tone=kClosedHat, .octave=0, .vel=82, .gate=50}, {.step=10, .tone=kOpenHat, .octave=0, .vel=72, .gate=120}, {.step=12, .tone=kClosedHat, .octave=0, .vel=74, .gate=50}, {.step=14, .tone=kOpenHat, .octave=0, .vel=74, .gate=120}};
inline constexpr StyleEvent kDC[] = {{.step=2, .tone=kRoot, .octave=0, .vel=86, .gate=kGateStaccato}, {.step=2, .tone=kThird, .octave=0, .vel=86, .gate=kGateStaccato}, {.step=2, .tone=kFifth, .octave=0, .vel=86, .gate=kGateStaccato}, {.step=2, .tone=kSeventh, .octave=0, .vel=86, .gate=kGateStaccato}, {.step=6, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=6, .tone=kThird, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=6, .tone=kSeventh, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=10, .tone=kRoot, .octave=0, .vel=86, .gate=kGateStaccato}, {.step=10, .tone=kThird, .octave=0, .vel=86, .gate=kGateStaccato}, {.step=10, .tone=kFifth, .octave=0, .vel=86, .gate=kGateStaccato}, {.step=10, .tone=kSeventh, .octave=0, .vel=86, .gate=kGateStaccato}, {.step=14, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=14, .tone=kThird, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=14, .tone=kSeventh, .octave=0, .vel=82, .gate=kGateStaccato}};
inline constexpr StylePattern kDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBB)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kDC)}};
inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=1, .patterns=Span<const StylePattern>(kIn1P)}, {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIn2P)},
    {.type=SectionType::kVarA, .bars=1, .patterns=Span<const StylePattern>(kAP)}, {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kBP)},
    {.type=SectionType::kVarC, .bars=1, .patterns=Span<const StylePattern>(kCP)}, {.type=SectionType::kVarD, .bars=1, .patterns=Span<const StylePattern>(kDP)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFAP)}, {.type=SectionType::kFillB, .bars=1, .patterns=Span<const StylePattern>(kFBP)}, {.type=SectionType::kFillC, .bars=1, .patterns=Span<const StylePattern>(kFCP)}, {.type=SectionType::kFillD, .bars=1, .patterns=Span<const StylePattern>(kFDP)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kE1P)}, {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kE2P)},
};
inline constexpr Style kStyle{.name="funk", .sections=Span<const StyleSection>(kSections)};
}  // namespace funk

// ---------------------------------------------------------------------------
// "disco": four-on-the-floor kick, off-beat open hats, clap backbeat, the
// classic octave-jumping eighth bass, off-beat string-stab triads.
namespace disco {
inline constexpr StyleEvent kHeldBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=100, .gate=kGateHeld}};
inline constexpr StyleEvent kFillBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=100, .gate=kGate8th}, {.step=8, .tone=kRoot, .octave=1, .vel=94, .gate=kGate8th}};
inline constexpr StyleEvent kIn1D[] = {{.step=8, .tone=kOpenHat, .octave=0, .vel=64, .gate=120}, {.step=10, .tone=kOpenHat, .octave=0, .vel=70, .gate=120}, {.step=12, .tone=kOpenHat, .octave=0, .vel=78, .gate=120}, {.step=14, .tone=kOpenHat, .octave=0, .vel=88, .gate=120}};
inline constexpr StylePattern kIn1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)}};
inline constexpr StyleEvent kIn2D[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=4, .tone=kKick, .octave=0, .vel=106, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=12, .tone=kKick, .octave=0, .vel=106, .gate=120},
    {.step=2, .tone=kOpenHat, .octave=0, .vel=72, .gate=120}, {.step=6, .tone=kOpenHat, .octave=0, .vel=72, .gate=120}, {.step=10, .tone=kOpenHat, .octave=0, .vel=72, .gate=120}, {.step=14, .tone=kOpenHat, .octave=0, .vel=72, .gate=120}, {.step=4, .tone=kClap, .octave=0, .vel=96, .gate=120}, {.step=12, .tone=kClap, .octave=0, .vel=96, .gate=120},
};
inline constexpr StyleEvent kIn2C[] = {{.step=2, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=76, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=76, .gate=kGateStab}};
inline constexpr StylePattern kIn2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIn2C)}};
inline constexpr StyleEvent kAD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=4, .tone=kKick, .octave=0, .vel=108, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=12, .tone=kKick, .octave=0, .vel=108, .gate=120},
    {.step=4, .tone=kClap, .octave=0, .vel=98, .gate=120}, {.step=12, .tone=kClap, .octave=0, .vel=98, .gate=120},
    {.step=0, .tone=kClosedHat, .octave=0, .vel=70, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=70, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=70, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=70, .gate=50},
    {.step=2, .tone=kOpenHat, .octave=0, .vel=80, .gate=120}, {.step=6, .tone=kOpenHat, .octave=0, .vel=80, .gate=120}, {.step=10, .tone=kOpenHat, .octave=0, .vel=80, .gate=120}, {.step=14, .tone=kOpenHat, .octave=0, .vel=80, .gate=120},
};
inline constexpr StyleEvent kAB[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=104, .gate=kGate8th}, {.step=2, .tone=kRoot, .octave=1, .vel=90, .gate=kGate8th}, {.step=4, .tone=kRoot, .octave=0, .vel=100, .gate=kGate8th}, {.step=6, .tone=kRoot, .octave=1, .vel=90, .gate=kGate8th}, {.step=8, .tone=kFifth, .octave=0, .vel=100, .gate=kGate8th}, {.step=10, .tone=kFifth, .octave=1, .vel=90, .gate=kGate8th}, {.step=12, .tone=kRoot, .octave=0, .vel=100, .gate=kGate8th}, {.step=14, .tone=kRoot, .octave=1, .vel=90, .gate=kGate8th},
};
inline constexpr StyleEvent kAC[] = {
    {.step=2, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=82, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStab}, {.step=6, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=82, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStab}, {.step=14, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStab},
};
inline constexpr StylePattern kAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAB)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAC)}};
inline constexpr StyleEvent kBD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=114, .gate=120}, {.step=4, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=114, .gate=120}, {.step=12, .tone=kKick, .octave=0, .vel=110, .gate=120},
    {.step=4, .tone=kClap, .octave=0, .vel=100, .gate=120}, {.step=12, .tone=kClap, .octave=0, .vel=100, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=90, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=90, .gate=120},
    {.step=0, .tone=kClosedHat, .octave=0, .vel=68, .gate=50}, {.step=2, .tone=kOpenHat, .octave=0, .vel=82, .gate=120}, {.step=4, .tone=kClosedHat, .octave=0, .vel=68, .gate=50}, {.step=6, .tone=kOpenHat, .octave=0, .vel=82, .gate=120}, {.step=8, .tone=kClosedHat, .octave=0, .vel=68, .gate=50}, {.step=10, .tone=kOpenHat, .octave=0, .vel=82, .gate=120}, {.step=12, .tone=kClosedHat, .octave=0, .vel=68, .gate=50}, {.step=14, .tone=kOpenHat, .octave=0, .vel=82, .gate=120},
    {.step=0, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=8, .tone=kTambourine, .octave=0, .vel=60, .gate=50},
};
inline constexpr StyleEvent kBB[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=108, .gate=kGate8th}, {.step=2, .tone=kRoot, .octave=1, .vel=92, .gate=kGate8th}, {.step=4, .tone=kThird, .octave=0, .vel=98, .gate=kGate8th}, {.step=6, .tone=kThird, .octave=1, .vel=90, .gate=kGate8th}, {.step=8, .tone=kFifth, .octave=0, .vel=102, .gate=kGate8th}, {.step=10, .tone=kFifth, .octave=1, .vel=90, .gate=kGate8th}, {.step=12, .tone=kRoot, .octave=0, .vel=100, .gate=kGate8th}, {.step=14, .tone=kSeventh, .octave=0, .vel=92, .gate=kGate8th},
};
inline constexpr StyleEvent kBC[] = {
    {.step=2, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}, {.step=6, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}, {.step=14, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab},
};
inline constexpr StylePattern kBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBB)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBC)}};
inline constexpr StyleEvent kFAD[] = {{.step=0, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=4, .tone=kClap, .octave=0, .vel=100, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=96, .gate=120}, {.step=10, .tone=kSnare, .octave=0, .vel=100, .gate=120}, {.step=12, .tone=kOpenHat, .octave=0, .vel=90, .gate=120}, {.step=14, .tone=kOpenHat, .octave=0, .vel=98, .gate=120}};
inline constexpr StyleEvent kFBD[] = {{.step=0, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=98, .gate=100}, {.step=6, .tone=kSnare, .octave=0, .vel=92, .gate=100}, {.step=8, .tone=kTomHi, .octave=0, .vel=100, .gate=100}, {.step=10, .tone=kTomMid, .octave=0, .vel=104, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=108, .gate=100}, {.step=14, .tone=kClap, .octave=0, .vel=110, .gate=100}};
inline constexpr StyleEvent kFCD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=94, .gate=100}, {.step=2, .tone=kSnare, .octave=0, .vel=88, .gate=100}, {.step=4, .tone=kTomHi, .octave=0, .vel=100, .gate=100}, {.step=6, .tone=kTomHi, .octave=0, .vel=96, .gate=100}, {.step=8, .tone=kTomMid, .octave=0, .vel=104, .gate=100}, {.step=10, .tone=kTomMid, .octave=0, .vel=100, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=110, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=114, .gate=100}};
inline constexpr StyleEvent kFDD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=92, .gate=50}, {.step=1, .tone=kSnare, .octave=0, .vel=86, .gate=50}, {.step=2, .tone=kSnare, .octave=0, .vel=96, .gate=50}, {.step=3, .tone=kSnare, .octave=0, .vel=90, .gate=50}, {.step=4, .tone=kSnare, .octave=0, .vel=100, .gate=50}, {.step=5, .tone=kSnare, .octave=0, .vel=94, .gate=50}, {.step=6, .tone=kTomHi, .octave=0, .vel=104, .gate=50}, {.step=7, .tone=kTomHi, .octave=0, .vel=98, .gate=50}, {.step=8, .tone=kTomMid, .octave=0, .vel=106, .gate=50}, {.step=9, .tone=kTomMid, .octave=0, .vel=100, .gate=50}, {.step=10, .tone=kTomLow, .octave=0, .vel=110, .gate=50}, {.step=11, .tone=kTomLow, .octave=0, .vel=104, .gate=50}, {.step=12, .tone=kTomFloor, .octave=0, .vel=114, .gate=50}, {.step=14, .tone=kClap, .octave=0, .vel=118, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=120, .gate=kGate8th}};
inline constexpr StylePattern kFAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)}};
inline constexpr StylePattern kFBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)}};
inline constexpr StylePattern kFCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)}};
inline constexpr StylePattern kFDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)}};
inline constexpr StyleEvent kE1D[] = {{.step=0, .tone=kKick, .octave=0, .vel=114, .gate=120}, {.step=0, .tone=kCrash, .octave=0, .vel=106, .gate=kGateHalfBar}};
inline constexpr StyleEvent kE1B[] = {{.step=0, .tone=kRoot, .octave=0, .vel=106, .gate=kGateHeld}};
inline constexpr StyleEvent kE1C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=86, .gate=kGateHeld}};
inline constexpr StylePattern kE1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1C)}};
inline constexpr StyleEvent kE2D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=112, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=116, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=8, .tone=kClap, .octave=0, .vel=100, .gate=120}};
inline constexpr StyleEvent kE2B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=110, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=98, .gate=kGateHeld}};
inline constexpr StyleEvent kE2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=88, .gate=kGateHeld}};
inline constexpr StylePattern kE2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2C)}};
// varC: breakdown — bare four-on-the-floor, clap backbeat, off-beat open hats, space.
inline constexpr StyleEvent kCD[] = {{.step=0, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=4, .tone=kKick, .octave=0, .vel=108, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=12, .tone=kKick, .octave=0, .vel=108, .gate=120}, {.step=4, .tone=kClap, .octave=0, .vel=98, .gate=120}, {.step=12, .tone=kClap, .octave=0, .vel=98, .gate=120}, {.step=2, .tone=kOpenHat, .octave=0, .vel=80, .gate=120}, {.step=6, .tone=kOpenHat, .octave=0, .vel=80, .gate=120}, {.step=10, .tone=kOpenHat, .octave=0, .vel=80, .gate=120}, {.step=14, .tone=kOpenHat, .octave=0, .vel=80, .gate=120}};
inline constexpr StyleEvent kCC[] = {{.step=2, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStab}};
inline constexpr StylePattern kCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAB)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCC)}};
// varD: peak — snare-doubled claps, 16th hat carpet with opens, tambourine, full 7th stabs.
inline constexpr StyleEvent kDD[] = {{.step=0, .tone=kKick, .octave=0, .vel=116, .gate=120}, {.step=4, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=116, .gate=120}, {.step=12, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=4, .tone=kClap, .octave=0, .vel=102, .gate=120}, {.step=12, .tone=kClap, .octave=0, .vel=102, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=90, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=90, .gate=120}, {.step=2, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}, {.step=6, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}, {.step=10, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}, {.step=14, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}, {.step=0, .tone=kClosedHat, .octave=0, .vel=68, .gate=50}, {.step=1, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=3, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=68, .gate=50}, {.step=5, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=7, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=68, .gate=50}, {.step=9, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=11, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=68, .gate=50}, {.step=13, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=15, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=0, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=8, .tone=kTambourine, .octave=0, .vel=60, .gate=50}};
inline constexpr StyleEvent kDC[] = {{.step=2, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}, {.step=6, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}, {.step=6, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}, {.step=14, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}, {.step=14, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateStab}};
inline constexpr StylePattern kDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBB)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kDC)}};
inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=1, .patterns=Span<const StylePattern>(kIn1P)}, {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIn2P)},
    {.type=SectionType::kVarA, .bars=1, .patterns=Span<const StylePattern>(kAP)}, {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kBP)},
    {.type=SectionType::kVarC, .bars=1, .patterns=Span<const StylePattern>(kCP)}, {.type=SectionType::kVarD, .bars=1, .patterns=Span<const StylePattern>(kDP)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFAP)}, {.type=SectionType::kFillB, .bars=1, .patterns=Span<const StylePattern>(kFBP)}, {.type=SectionType::kFillC, .bars=1, .patterns=Span<const StylePattern>(kFCP)}, {.type=SectionType::kFillD, .bars=1, .patterns=Span<const StylePattern>(kFDP)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kE1P)}, {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kE2P)},
};
inline constexpr Style kStyle{.name="disco", .sections=Span<const StyleSection>(kSections)};
}  // namespace disco

// ---------------------------------------------------------------------------
// "house": four-on-the-floor with off-beat open hats and a shaker, clap on the
// backbeat, a driving off-beat eighth bass and sustained 7th organ stabs.
namespace house {
inline constexpr StyleEvent kHeldBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=100, .gate=kGateHeld}};
inline constexpr StyleEvent kFillBass[] = {{.step=2, .tone=kRoot, .octave=0, .vel=98, .gate=kGate8th}, {.step=10, .tone=kFifth, .octave=0, .vel=94, .gate=kGate8th}};
inline constexpr StyleEvent kIn1D[] = {{.step=8, .tone=kCabasa, .octave=0, .vel=58, .gate=50}, {.step=10, .tone=kCabasa, .octave=0, .vel=64, .gate=50}, {.step=12, .tone=kCabasa, .octave=0, .vel=72, .gate=50}, {.step=14, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}};
inline constexpr StylePattern kIn1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)}};
inline constexpr StyleEvent kIn2D[] = {{.step=0, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=4, .tone=kKick, .octave=0, .vel=108, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=12, .tone=kKick, .octave=0, .vel=108, .gate=120}, {.step=2, .tone=kOpenHat, .octave=0, .vel=74, .gate=120}, {.step=6, .tone=kOpenHat, .octave=0, .vel=74, .gate=120}, {.step=10, .tone=kOpenHat, .octave=0, .vel=74, .gate=120}, {.step=14, .tone=kOpenHat, .octave=0, .vel=74, .gate=120}};
inline constexpr StyleEvent kIn2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=74, .gate=kGateBeat}, {.step=0, .tone=kThird, .octave=0, .vel=74, .gate=kGateBeat}, {.step=0, .tone=kFifth, .octave=0, .vel=74, .gate=kGateBeat}, {.step=0, .tone=kSeventh, .octave=0, .vel=74, .gate=kGateBeat}, {.step=8, .tone=kRoot, .octave=0, .vel=72, .gate=kGateBeat}, {.step=8, .tone=kThird, .octave=0, .vel=72, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=72, .gate=kGateBeat}};
inline constexpr StylePattern kIn2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIn2C)}};
inline constexpr StyleEvent kAD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=114, .gate=120}, {.step=4, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=114, .gate=120}, {.step=12, .tone=kKick, .octave=0, .vel=110, .gate=120},
    {.step=4, .tone=kClap, .octave=0, .vel=96, .gate=120}, {.step=12, .tone=kClap, .octave=0, .vel=96, .gate=120},
    {.step=2, .tone=kOpenHat, .octave=0, .vel=82, .gate=120}, {.step=6, .tone=kOpenHat, .octave=0, .vel=82, .gate=120}, {.step=10, .tone=kOpenHat, .octave=0, .vel=82, .gate=120}, {.step=14, .tone=kOpenHat, .octave=0, .vel=82, .gate=120},
    {.step=0, .tone=kCabasa, .octave=0, .vel=56, .gate=50}, {.step=2, .tone=kCabasa, .octave=0, .vel=50, .gate=50}, {.step=4, .tone=kCabasa, .octave=0, .vel=56, .gate=50}, {.step=6, .tone=kCabasa, .octave=0, .vel=50, .gate=50}, {.step=8, .tone=kCabasa, .octave=0, .vel=56, .gate=50}, {.step=10, .tone=kCabasa, .octave=0, .vel=50, .gate=50}, {.step=12, .tone=kCabasa, .octave=0, .vel=56, .gate=50}, {.step=14, .tone=kCabasa, .octave=0, .vel=50, .gate=50},
};
inline constexpr StyleEvent kAB[] = {{.step=2, .tone=kRoot, .octave=0, .vel=100, .gate=kGate8th}, {.step=6, .tone=kRoot, .octave=0, .vel=96, .gate=kGate8th}, {.step=10, .tone=kFifth, .octave=0, .vel=98, .gate=kGate8th}, {.step=14, .tone=kRoot, .octave=0, .vel=96, .gate=kGate8th}};
inline constexpr StyleEvent kAC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=80, .gate=kGateBeat}, {.step=0, .tone=kThird, .octave=0, .vel=80, .gate=kGateBeat}, {.step=0, .tone=kFifth, .octave=0, .vel=80, .gate=kGateBeat}, {.step=0, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateBeat}, {.step=8, .tone=kRoot, .octave=0, .vel=78, .gate=kGateBeat}, {.step=8, .tone=kThird, .octave=0, .vel=78, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=78, .gate=kGateBeat}, {.step=8, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateBeat}};
inline constexpr StylePattern kAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAB)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAC)}};
inline constexpr StyleEvent kBD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=116, .gate=120}, {.step=4, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=116, .gate=120}, {.step=12, .tone=kKick, .octave=0, .vel=112, .gate=120},
    {.step=4, .tone=kClap, .octave=0, .vel=100, .gate=120}, {.step=12, .tone=kClap, .octave=0, .vel=100, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=88, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=88, .gate=120},
    {.step=2, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}, {.step=6, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}, {.step=10, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}, {.step=14, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}, {.step=15, .tone=kOpenHat, .octave=0, .vel=70, .gate=120},
    {.step=0, .tone=kCabasa, .octave=0, .vel=58, .gate=50}, {.step=1, .tone=kCabasa, .octave=0, .vel=48, .gate=50}, {.step=2, .tone=kCabasa, .octave=0, .vel=54, .gate=50}, {.step=3, .tone=kCabasa, .octave=0, .vel=48, .gate=50}, {.step=4, .tone=kCabasa, .octave=0, .vel=58, .gate=50}, {.step=6, .tone=kCabasa, .octave=0, .vel=54, .gate=50}, {.step=8, .tone=kCabasa, .octave=0, .vel=58, .gate=50}, {.step=10, .tone=kCabasa, .octave=0, .vel=54, .gate=50}, {.step=12, .tone=kCabasa, .octave=0, .vel=58, .gate=50}, {.step=14, .tone=kCabasa, .octave=0, .vel=54, .gate=50},
};
inline constexpr StyleEvent kBB[] = {{.step=2, .tone=kRoot, .octave=0, .vel=104, .gate=kGate8th}, {.step=6, .tone=kThird, .octave=0, .vel=96, .gate=kGate8th}, {.step=8, .tone=kFifth, .octave=0, .vel=98, .gate=kGate8th}, {.step=10, .tone=kFifth, .octave=0, .vel=94, .gate=kGate8th}, {.step=14, .tone=kSeventh, .octave=0, .vel=96, .gate=kGate8th}};
inline constexpr StyleEvent kBC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=82, .gate=kGateStab}, {.step=0, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=82, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStab}, {.step=6, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=82, .gate=kGateStab}, {.step=8, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStab}, {.step=8, .tone=kSeventh, .octave=0, .vel=82, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStab}, {.step=14, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStab}};
inline constexpr StylePattern kBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBB)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBC)}};
inline constexpr StyleEvent kFAD[] = {{.step=0, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=4, .tone=kClap, .octave=0, .vel=100, .gate=120}, {.step=8, .tone=kClap, .octave=0, .vel=94, .gate=120}, {.step=10, .tone=kClap, .octave=0, .vel=98, .gate=120}, {.step=12, .tone=kOpenHat, .octave=0, .vel=92, .gate=120}, {.step=14, .tone=kOpenHat, .octave=0, .vel=100, .gate=120}};
inline constexpr StyleEvent kFBD[] = {{.step=0, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=96, .gate=100}, {.step=6, .tone=kCabasa, .octave=0, .vel=70, .gate=50}, {.step=8, .tone=kSnare, .octave=0, .vel=92, .gate=100}, {.step=10, .tone=kSnare, .octave=0, .vel=98, .gate=100}, {.step=12, .tone=kClap, .octave=0, .vel=104, .gate=100}, {.step=14, .tone=kOpenHat, .octave=0, .vel=100, .gate=120}};
inline constexpr StyleEvent kFCD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=92, .gate=100}, {.step=2, .tone=kClap, .octave=0, .vel=90, .gate=100}, {.step=4, .tone=kSnare, .octave=0, .vel=98, .gate=100}, {.step=6, .tone=kClap, .octave=0, .vel=96, .gate=100}, {.step=8, .tone=kSnare, .octave=0, .vel=102, .gate=100}, {.step=10, .tone=kClap, .octave=0, .vel=100, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=108, .gate=100}, {.step=14, .tone=kClap, .octave=0, .vel=112, .gate=100}};
inline constexpr StyleEvent kFDD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=90, .gate=50}, {.step=1, .tone=kClap, .octave=0, .vel=86, .gate=50}, {.step=2, .tone=kSnare, .octave=0, .vel=94, .gate=50}, {.step=3, .tone=kClap, .octave=0, .vel=90, .gate=50}, {.step=4, .tone=kSnare, .octave=0, .vel=98, .gate=50}, {.step=5, .tone=kClap, .octave=0, .vel=94, .gate=50}, {.step=6, .tone=kSnare, .octave=0, .vel=100, .gate=50}, {.step=7, .tone=kClap, .octave=0, .vel=96, .gate=50}, {.step=8, .tone=kSnare, .octave=0, .vel=104, .gate=50}, {.step=9, .tone=kClap, .octave=0, .vel=100, .gate=50}, {.step=10, .tone=kSnare, .octave=0, .vel=106, .gate=50}, {.step=11, .tone=kClap, .octave=0, .vel=102, .gate=50}, {.step=12, .tone=kSnare, .octave=0, .vel=110, .gate=50}, {.step=13, .tone=kSnare, .octave=0, .vel=114, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=118, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=120, .gate=kGate8th}};
inline constexpr StylePattern kFAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)}};
inline constexpr StylePattern kFBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)}};
inline constexpr StylePattern kFCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)}};
inline constexpr StylePattern kFDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass)}};
inline constexpr StyleEvent kE1D[] = {{.step=0, .tone=kKick, .octave=0, .vel=114, .gate=120}, {.step=0, .tone=kCrash, .octave=0, .vel=104, .gate=kGateHalfBar}};
inline constexpr StyleEvent kE1B[] = {{.step=0, .tone=kRoot, .octave=0, .vel=104, .gate=kGateHeld}};
inline constexpr StyleEvent kE1C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=84, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=84, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=84, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateHeld}};
inline constexpr StylePattern kE1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1C)}};
inline constexpr StyleEvent kE2D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=112, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=116, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=8, .tone=kClap, .octave=0, .vel=98, .gate=120}};
inline constexpr StyleEvent kE2B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=108, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=96, .gate=kGateHeld}};
inline constexpr StyleEvent kE2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=86, .gate=kGateHeld}};
inline constexpr StylePattern kE2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2C)}};
// varC: deep-house stripped — four-on-the-floor, off-beat open hats, sustained 7th pad.
inline constexpr StyleEvent kCD[] = {{.step=0, .tone=kKick, .octave=0, .vel=114, .gate=120}, {.step=4, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=114, .gate=120}, {.step=12, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=2, .tone=kOpenHat, .octave=0, .vel=82, .gate=120}, {.step=6, .tone=kOpenHat, .octave=0, .vel=82, .gate=120}, {.step=10, .tone=kOpenHat, .octave=0, .vel=82, .gate=120}, {.step=14, .tone=kOpenHat, .octave=0, .vel=82, .gate=120}};
inline constexpr StyleEvent kCC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=74, .gate=kGateBeat}, {.step=0, .tone=kThird, .octave=0, .vel=74, .gate=kGateBeat}, {.step=0, .tone=kFifth, .octave=0, .vel=74, .gate=kGateBeat}, {.step=0, .tone=kSeventh, .octave=0, .vel=74, .gate=kGateBeat}, {.step=8, .tone=kRoot, .octave=0, .vel=72, .gate=kGateBeat}, {.step=8, .tone=kThird, .octave=0, .vel=72, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=72, .gate=kGateBeat}, {.step=8, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateBeat}};
inline constexpr StylePattern kCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAB)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCC)}};
// varD: peak — clap+snare backbeat, off-beat opens, 16th cabasa carpet, full 7th stabs.
inline constexpr StyleEvent kDD[] = {{.step=0, .tone=kKick, .octave=0, .vel=116, .gate=120}, {.step=4, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=116, .gate=120}, {.step=12, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=4, .tone=kClap, .octave=0, .vel=100, .gate=120}, {.step=12, .tone=kClap, .octave=0, .vel=100, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=88, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=88, .gate=120}, {.step=2, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}, {.step=6, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}, {.step=10, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}, {.step=14, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}, {.step=0, .tone=kCabasa, .octave=0, .vel=58, .gate=50}, {.step=1, .tone=kCabasa, .octave=0, .vel=48, .gate=50}, {.step=2, .tone=kCabasa, .octave=0, .vel=54, .gate=50}, {.step=3, .tone=kCabasa, .octave=0, .vel=48, .gate=50}, {.step=4, .tone=kCabasa, .octave=0, .vel=58, .gate=50}, {.step=5, .tone=kCabasa, .octave=0, .vel=48, .gate=50}, {.step=6, .tone=kCabasa, .octave=0, .vel=54, .gate=50}, {.step=7, .tone=kCabasa, .octave=0, .vel=48, .gate=50}, {.step=8, .tone=kCabasa, .octave=0, .vel=58, .gate=50}, {.step=9, .tone=kCabasa, .octave=0, .vel=48, .gate=50}, {.step=10, .tone=kCabasa, .octave=0, .vel=54, .gate=50}, {.step=11, .tone=kCabasa, .octave=0, .vel=48, .gate=50}, {.step=12, .tone=kCabasa, .octave=0, .vel=58, .gate=50}, {.step=13, .tone=kCabasa, .octave=0, .vel=48, .gate=50}, {.step=14, .tone=kCabasa, .octave=0, .vel=54, .gate=50}, {.step=15, .tone=kCabasa, .octave=0, .vel=48, .gate=50}};
inline constexpr StyleEvent kDC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=0, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStab}, {.step=6, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=8, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=8, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStab}, {.step=14, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStab}};
inline constexpr StylePattern kDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBB)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kDC)}};
inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=1, .patterns=Span<const StylePattern>(kIn1P)}, {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIn2P)},
    {.type=SectionType::kVarA, .bars=1, .patterns=Span<const StylePattern>(kAP)}, {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kBP)},
    {.type=SectionType::kVarC, .bars=1, .patterns=Span<const StylePattern>(kCP)}, {.type=SectionType::kVarD, .bars=1, .patterns=Span<const StylePattern>(kDP)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFAP)}, {.type=SectionType::kFillB, .bars=1, .patterns=Span<const StylePattern>(kFBP)}, {.type=SectionType::kFillC, .bars=1, .patterns=Span<const StylePattern>(kFCP)}, {.type=SectionType::kFillD, .bars=1, .patterns=Span<const StylePattern>(kFDP)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kE1P)}, {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kE2P)},
};
inline constexpr Style kStyle{.name="house", .sections=Span<const StyleSection>(kSections)};
}  // namespace house

// ---------------------------------------------------------------------------
// "swing": jazz ride pattern with a swung feel, pedal-hat on 2 & 4, feathered
// kick, walking quarter-note bass and sparse rootless off-beat comping.
namespace swing {
inline constexpr StyleEvent kHeldBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=84, .gate=kGateHeld}};
inline constexpr StyleEvent kWalkBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=86, .gate=kGateBeat}, {.step=4, .tone=kThird, .octave=0, .vel=80, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=84, .gate=kGateBeat}, {.step=12, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateBeat}};
inline constexpr StyleEvent kIn1D[] = {{.step=8, .tone=kRide, .octave=0, .vel=56, .gate=120}, {.step=11, .tone=kRide, .octave=0, .vel=50, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=60, .gate=120}, {.step=15, .tone=kRide, .octave=0, .vel=54, .gate=120}};
inline constexpr StylePattern kIn1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)}};
inline constexpr StyleEvent kIn2D[] = {{.step=0, .tone=kRide, .octave=0, .vel=64, .gate=120}, {.step=4, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=7, .tone=kRide, .octave=0, .vel=52, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=62, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=15, .tone=kRide, .octave=0, .vel=52, .gate=120}, {.step=4, .tone=kClosedHat, .octave=0, .vel=54, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=54, .gate=50}};
inline constexpr StyleEvent kIn2C[] = {{.step=6, .tone=kThird, .octave=0, .vel=68, .gate=kGateStab}, {.step=6, .tone=kSeventh, .octave=0, .vel=68, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=66, .gate=kGateStab}, {.step=14, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateStab}};
inline constexpr StylePattern kIn2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kWalkBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIn2C)}};
inline constexpr StyleEvent kAD[] = {
    {.step=0, .tone=kRide, .octave=0, .vel=72, .gate=120}, {.step=4, .tone=kRide, .octave=0, .vel=64, .gate=120}, {.step=7, .tone=kRide, .octave=0, .vel=56, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=70, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=64, .gate=120}, {.step=15, .tone=kRide, .octave=0, .vel=56, .gate=120},
    {.step=4, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=0, .tone=kKick, .octave=0, .vel=40, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=38, .gate=50},
};
inline constexpr StyleEvent kAC[] = {{.step=6, .tone=kThird, .octave=0, .vel=70, .gate=kGateStab}, {.step=6, .tone=kSeventh, .octave=0, .vel=70, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=68, .gate=kGateStab}, {.step=14, .tone=kSeventh, .octave=0, .vel=68, .gate=kGateStab}};
inline constexpr StylePattern kAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kWalkBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAC)}};
inline constexpr StyleEvent kBD[] = {
    {.step=0, .tone=kRide, .octave=0, .vel=76, .gate=120}, {.step=3, .tone=kRide, .octave=0, .vel=54, .gate=120}, {.step=4, .tone=kRide, .octave=0, .vel=68, .gate=120}, {.step=7, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=74, .gate=120}, {.step=11, .tone=kRide, .octave=0, .vel=54, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=68, .gate=120}, {.step=15, .tone=kRide, .octave=0, .vel=58, .gate=120},
    {.step=4, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=0, .tone=kKick, .octave=0, .vel=44, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=42, .gate=50}, {.step=6, .tone=kSnare, .octave=0, .vel=52, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=56, .gate=50},
};
inline constexpr StyleEvent kBC[] = {{.step=2, .tone=kThird, .octave=0, .vel=68, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=68, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=72, .gate=kGateStab}, {.step=6, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}, {.step=11, .tone=kThird, .octave=0, .vel=66, .gate=kGateStab}, {.step=11, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=70, .gate=kGateStab}, {.step=14, .tone=kSeventh, .octave=0, .vel=70, .gate=kGateStab}};
inline constexpr StylePattern kBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kWalkBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBC)}};
inline constexpr StyleEvent kFAD[] = {{.step=0, .tone=kRide, .octave=0, .vel=70, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=76, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=70, .gate=120}, {.step=11, .tone=kSnare, .octave=0, .vel=74, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=80, .gate=120}, {.step=15, .tone=kSnare, .octave=0, .vel=86, .gate=120}};
inline constexpr StyleEvent kFBD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=76, .gate=100}, {.step=3, .tone=kSnare, .octave=0, .vel=70, .gate=100}, {.step=4, .tone=kTomHi, .octave=0, .vel=82, .gate=100}, {.step=7, .tone=kTomMid, .octave=0, .vel=80, .gate=100}, {.step=8, .tone=kSnare, .octave=0, .vel=84, .gate=100}, {.step=11, .tone=kTomLow, .octave=0, .vel=86, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=90, .gate=100}, {.step=15, .tone=kTomFloor, .octave=0, .vel=94, .gate=100}};
inline constexpr StyleEvent kFCD[] = {{.step=0, .tone=kTomHi, .octave=0, .vel=80, .gate=100}, {.step=3, .tone=kTomHi, .octave=0, .vel=76, .gate=100}, {.step=4, .tone=kSnare, .octave=0, .vel=84, .gate=100}, {.step=7, .tone=kSnare, .octave=0, .vel=80, .gate=100}, {.step=8, .tone=kTomMid, .octave=0, .vel=88, .gate=100}, {.step=11, .tone=kTomLow, .octave=0, .vel=90, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=94, .gate=100}, {.step=15, .tone=kTomFloor, .octave=0, .vel=98, .gate=100}};
inline constexpr StyleEvent kFDD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=80, .gate=50}, {.step=2, .tone=kSnare, .octave=0, .vel=74, .gate=50}, {.step=3, .tone=kTomHi, .octave=0, .vel=84, .gate=50}, {.step=4, .tone=kTomHi, .octave=0, .vel=88, .gate=50}, {.step=6, .tone=kTomMid, .octave=0, .vel=86, .gate=50}, {.step=7, .tone=kTomMid, .octave=0, .vel=90, .gate=50}, {.step=8, .tone=kTomLow, .octave=0, .vel=92, .gate=50}, {.step=10, .tone=kTomLow, .octave=0, .vel=88, .gate=50}, {.step=11, .tone=kTomFloor, .octave=0, .vel=94, .gate=50}, {.step=12, .tone=kSnare, .octave=0, .vel=98, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=104, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=110, .gate=kGate8th}};
inline constexpr StylePattern kFAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kWalkBass)}};
inline constexpr StylePattern kFBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kWalkBass)}};
inline constexpr StylePattern kFCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kWalkBass)}};
inline constexpr StylePattern kFDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kWalkBass)}};
inline constexpr StyleEvent kE1D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=80, .gate=kGateHalfBar}, {.step=0, .tone=kKick, .octave=0, .vel=70, .gate=50}};
inline constexpr StyleEvent kE1B[] = {{.step=0, .tone=kRoot, .octave=0, .vel=84, .gate=kGateHeld}};
inline constexpr StyleEvent kE1C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=74, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=74, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=74, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=74, .gate=kGateHeld}};
inline constexpr StylePattern kE1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1C)}};
inline constexpr StyleEvent kE2D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=74, .gate=50}, {.step=8, .tone=kRide, .octave=0, .vel=62, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=58, .gate=120}};
inline constexpr StyleEvent kE2B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=78, .gate=kGateHeld}};
inline constexpr StyleEvent kE2C[] = {{.step=0, .tone=kThird, .octave=0, .vel=76, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=76, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=76, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=1, .vel=72, .gate=kGateHeld}};
inline constexpr StylePattern kE2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2C)}};
// varC: two-feel — laid-back quarter ride, feathered kick, pedal hat, sparse comp.
inline constexpr StyleEvent kCD[] = {{.step=0, .tone=kRide, .octave=0, .vel=68, .gate=120}, {.step=4, .tone=kRide, .octave=0, .vel=60, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=66, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=60, .gate=120}, {.step=0, .tone=kKick, .octave=0, .vel=42, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=40, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=54, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=54, .gate=50}};
inline constexpr StyleEvent kCC[] = {{.step=2, .tone=kThird, .octave=0, .vel=68, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=68, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=66, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateStab}};
inline constexpr StylePattern kCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kWalkBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCC)}};
// varD: shout chorus — busy swung ride, hats on 2 & 4, kick bombs, snare comps, full rootless 7ths.
inline constexpr StyleEvent kDD[] = {{.step=0, .tone=kRide, .octave=0, .vel=78, .gate=120}, {.step=3, .tone=kRide, .octave=0, .vel=56, .gate=120}, {.step=4, .tone=kRide, .octave=0, .vel=72, .gate=120}, {.step=7, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=76, .gate=120}, {.step=11, .tone=kRide, .octave=0, .vel=56, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=72, .gate=120}, {.step=15, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=4, .tone=kClosedHat, .octave=0, .vel=62, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=62, .gate=50}, {.step=0, .tone=kKick, .octave=0, .vel=50, .gate=50}, {.step=4, .tone=kKick, .octave=0, .vel=44, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=48, .gate=50}, {.step=12, .tone=kKick, .octave=0, .vel=44, .gate=50}, {.step=2, .tone=kSnare, .octave=0, .vel=54, .gate=50}, {.step=6, .tone=kSnare, .octave=0, .vel=58, .gate=50}, {.step=10, .tone=kSnare, .octave=0, .vel=54, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=60, .gate=50}};
inline constexpr StyleEvent kDC[] = {{.step=2, .tone=kThird, .octave=0, .vel=74, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=74, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=74, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=72, .gate=kGateStab}, {.step=6, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=74, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=74, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=74, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=72, .gate=kGateStab}, {.step=14, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}};
inline constexpr StylePattern kDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kWalkBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kDC)}};
inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=1, .patterns=Span<const StylePattern>(kIn1P)}, {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIn2P)},
    {.type=SectionType::kVarA, .bars=1, .patterns=Span<const StylePattern>(kAP)}, {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kBP)},
    {.type=SectionType::kVarC, .bars=1, .patterns=Span<const StylePattern>(kCP)}, {.type=SectionType::kVarD, .bars=1, .patterns=Span<const StylePattern>(kDP)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFAP)}, {.type=SectionType::kFillB, .bars=1, .patterns=Span<const StylePattern>(kFBP)}, {.type=SectionType::kFillC, .bars=1, .patterns=Span<const StylePattern>(kFCP)}, {.type=SectionType::kFillD, .bars=1, .patterns=Span<const StylePattern>(kFDP)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kE1P)}, {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kE2P)},
};
inline constexpr Style kStyle{.name="swing", .sections=Span<const StyleSection>(kSections)};
}  // namespace swing

// ---------------------------------------------------------------------------
// "bossa": gentle side-stick bossa-nova cross-stick, soft surdo kick, brushed
// hats, a two-feel root-fifth bass and lush syncopated 7th comping.
namespace bossa {
inline constexpr StyleEvent kHeldBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=80, .gate=kGateHeld}};
inline constexpr StyleEvent kTwoBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=84, .gate=kGate8th}, {.step=6, .tone=kFifth, .octave=-1, .vel=78, .gate=kGate8th}, {.step=8, .tone=kRoot, .octave=0, .vel=82, .gate=kGate8th}, {.step=14, .tone=kFifth, .octave=-1, .vel=76, .gate=kGate8th}};
inline constexpr StyleEvent kIn1D[] = {{.step=8, .tone=kSideStick, .octave=0, .vel=54, .gate=50}, {.step=11, .tone=kSideStick, .octave=0, .vel=58, .gate=50}, {.step=14, .tone=kSideStick, .octave=0, .vel=64, .gate=50}};
inline constexpr StylePattern kIn1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)}};
inline constexpr StyleEvent kIn2D[] = {{.step=0, .tone=kSideStick, .octave=0, .vel=64, .gate=50}, {.step=3, .tone=kSideStick, .octave=0, .vel=56, .gate=50}, {.step=6, .tone=kSideStick, .octave=0, .vel=58, .gate=50}, {.step=8, .tone=kSideStick, .octave=0, .vel=62, .gate=50}, {.step=11, .tone=kSideStick, .octave=0, .vel=56, .gate=50}, {.step=14, .tone=kSideStick, .octave=0, .vel=58, .gate=50}, {.step=0, .tone=kKick, .octave=0, .vel=60, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=58, .gate=50}};
inline constexpr StyleEvent kIn2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=66, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=66, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=64, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=64, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=64, .gate=kGateStab}};
inline constexpr StylePattern kIn2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kTwoBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIn2C)}};
inline constexpr StyleEvent kAD[] = {
    {.step=0, .tone=kSideStick, .octave=0, .vel=70, .gate=50}, {.step=3, .tone=kSideStick, .octave=0, .vel=60, .gate=50}, {.step=6, .tone=kSideStick, .octave=0, .vel=62, .gate=50}, {.step=8, .tone=kSideStick, .octave=0, .vel=68, .gate=50}, {.step=11, .tone=kSideStick, .octave=0, .vel=60, .gate=50}, {.step=14, .tone=kSideStick, .octave=0, .vel=62, .gate=50},
    {.step=0, .tone=kKick, .octave=0, .vel=64, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=60, .gate=50},
    {.step=2, .tone=kClosedHat, .octave=0, .vel=48, .gate=50}, {.step=6, .tone=kClosedHat, .octave=0, .vel=44, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=48, .gate=50}, {.step=14, .tone=kClosedHat, .octave=0, .vel=44, .gate=50},
};
inline constexpr StyleEvent kAC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=70, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=70, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=70, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=66, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=66, .gate=kGateStab}, {.step=6, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateStab}, {.step=11, .tone=kRoot, .octave=0, .vel=68, .gate=kGateStab}, {.step=11, .tone=kThird, .octave=0, .vel=68, .gate=kGateStab}, {.step=11, .tone=kSeventh, .octave=0, .vel=68, .gate=kGateStab}};
inline constexpr StylePattern kAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kTwoBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAC)}};
inline constexpr StyleEvent kBD[] = {
    {.step=0, .tone=kSideStick, .octave=0, .vel=72, .gate=50}, {.step=3, .tone=kSideStick, .octave=0, .vel=62, .gate=50}, {.step=6, .tone=kSideStick, .octave=0, .vel=64, .gate=50}, {.step=8, .tone=kSideStick, .octave=0, .vel=70, .gate=50}, {.step=10, .tone=kSideStick, .octave=0, .vel=58, .gate=50}, {.step=11, .tone=kSideStick, .octave=0, .vel=62, .gate=50}, {.step=14, .tone=kSideStick, .octave=0, .vel=64, .gate=50},
    {.step=0, .tone=kKick, .octave=0, .vel=66, .gate=50}, {.step=4, .tone=kKick, .octave=0, .vel=54, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=62, .gate=50}, {.step=12, .tone=kKick, .octave=0, .vel=54, .gate=50},
    {.step=0, .tone=kClosedHat, .octave=0, .vel=50, .gate=50}, {.step=2, .tone=kClosedHat, .octave=0, .vel=46, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=50, .gate=50}, {.step=6, .tone=kClosedHat, .octave=0, .vel=46, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=50, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=46, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=50, .gate=50}, {.step=14, .tone=kClosedHat, .octave=0, .vel=46, .gate=50},
};
inline constexpr StyleEvent kBB[] = {{.step=0, .tone=kRoot, .octave=0, .vel=86, .gate=kGate8th}, {.step=4, .tone=kFifth, .octave=-1, .vel=78, .gate=kGate8th}, {.step=6, .tone=kFifth, .octave=-1, .vel=76, .gate=kGate8th}, {.step=8, .tone=kRoot, .octave=0, .vel=84, .gate=kGate8th}, {.step=12, .tone=kThird, .octave=0, .vel=78, .gate=kGate8th}, {.step=14, .tone=kFifth, .octave=-1, .vel=76, .gate=kGate8th}};
inline constexpr StyleEvent kBC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=72, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=0, .tone=kFifth, .octave=0, .vel=72, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}, {.step=3, .tone=kThird, .octave=0, .vel=66, .gate=kGateStab}, {.step=3, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=68, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=68, .gate=kGateStab}, {.step=6, .tone=kSeventh, .octave=0, .vel=68, .gate=kGateStab}, {.step=11, .tone=kRoot, .octave=0, .vel=70, .gate=kGateStab}, {.step=11, .tone=kThird, .octave=0, .vel=70, .gate=kGateStab}, {.step=11, .tone=kSeventh, .octave=0, .vel=70, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=64, .gate=kGateStab}, {.step=14, .tone=kSeventh, .octave=0, .vel=64, .gate=kGateStab}};
inline constexpr StylePattern kBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBB)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBC)}};
inline constexpr StyleEvent kFAD[] = {{.step=0, .tone=kSideStick, .octave=0, .vel=70, .gate=50}, {.step=4, .tone=kSideStick, .octave=0, .vel=66, .gate=50}, {.step=8, .tone=kTomHi, .octave=0, .vel=72, .gate=100}, {.step=10, .tone=kTomMid, .octave=0, .vel=76, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=80, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=84, .gate=100}};
inline constexpr StyleEvent kFBD[] = {{.step=0, .tone=kTomHi, .octave=0, .vel=70, .gate=100}, {.step=3, .tone=kTomHi, .octave=0, .vel=66, .gate=100}, {.step=6, .tone=kTomMid, .octave=0, .vel=74, .gate=100}, {.step=8, .tone=kTomMid, .octave=0, .vel=76, .gate=100}, {.step=11, .tone=kTomLow, .octave=0, .vel=80, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=86, .gate=100}};
inline constexpr StyleEvent kFCD[] = {{.step=0, .tone=kSideStick, .octave=0, .vel=68, .gate=50}, {.step=2, .tone=kTomHi, .octave=0, .vel=74, .gate=100}, {.step=4, .tone=kTomHi, .octave=0, .vel=76, .gate=100}, {.step=6, .tone=kTomMid, .octave=0, .vel=78, .gate=100}, {.step=8, .tone=kTomMid, .octave=0, .vel=80, .gate=100}, {.step=10, .tone=kTomLow, .octave=0, .vel=84, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=86, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=90, .gate=100}};
inline constexpr StyleEvent kFDD[] = {{.step=0, .tone=kTomHi, .octave=0, .vel=72, .gate=50}, {.step=2, .tone=kTomHi, .octave=0, .vel=68, .gate=50}, {.step=4, .tone=kTomMid, .octave=0, .vel=76, .gate=50}, {.step=6, .tone=kTomMid, .octave=0, .vel=72, .gate=50}, {.step=8, .tone=kTomLow, .octave=0, .vel=80, .gate=50}, {.step=9, .tone=kTomLow, .octave=0, .vel=76, .gate=50}, {.step=10, .tone=kTomFloor, .octave=0, .vel=84, .gate=50}, {.step=11, .tone=kTomFloor, .octave=0, .vel=80, .gate=50}, {.step=12, .tone=kSideStick, .octave=0, .vel=88, .gate=50}, {.step=13, .tone=kSideStick, .octave=0, .vel=92, .gate=50}, {.step=14, .tone=kSideStick, .octave=0, .vel=98, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=100, .gate=kGate8th}};
inline constexpr StylePattern kFAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kTwoBass)}};
inline constexpr StylePattern kFBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kTwoBass)}};
inline constexpr StylePattern kFCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kTwoBass)}};
inline constexpr StylePattern kFDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kTwoBass)}};
inline constexpr StyleEvent kE1D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=76, .gate=kGateHalfBar}, {.step=0, .tone=kKick, .octave=0, .vel=64, .gate=50}};
inline constexpr StyleEvent kE1B[] = {{.step=0, .tone=kRoot, .octave=0, .vel=80, .gate=kGateHeld}};
inline constexpr StyleEvent kE1C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=70, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=70, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=70, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=70, .gate=kGateHeld}};
inline constexpr StylePattern kE1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1C)}};
inline constexpr StyleEvent kE2D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=84, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=68, .gate=50}, {.step=8, .tone=kSideStick, .octave=0, .vel=60, .gate=50}};
inline constexpr StyleEvent kE2B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=82, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=74, .gate=kGateHeld}};
inline constexpr StyleEvent kE2C[] = {{.step=0, .tone=kThird, .octave=0, .vel=72, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=72, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=1, .vel=68, .gate=kGateHeld}};
inline constexpr StylePattern kE2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2C)}};
// varC: partido-alto stripped — bare cross-stick clave and soft surdo, wide space.
inline constexpr StyleEvent kCD[] = {{.step=0, .tone=kSideStick, .octave=0, .vel=70, .gate=50}, {.step=3, .tone=kSideStick, .octave=0, .vel=60, .gate=50}, {.step=6, .tone=kSideStick, .octave=0, .vel=62, .gate=50}, {.step=10, .tone=kSideStick, .octave=0, .vel=58, .gate=50}, {.step=11, .tone=kSideStick, .octave=0, .vel=60, .gate=50}, {.step=14, .tone=kSideStick, .octave=0, .vel=62, .gate=50}, {.step=0, .tone=kKick, .octave=0, .vel=64, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=60, .gate=50}};
inline constexpr StyleEvent kCC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=68, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=68, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=68, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=66, .gate=kGateStab}, {.step=8, .tone=kFifth, .octave=0, .vel=66, .gate=kGateStab}, {.step=8, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateStab}};
inline constexpr StylePattern kCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kTwoBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCC)}};
// varD: fullest — clave + walking surdo + brushed hats, lush syncopated 7th comping.
inline constexpr StyleEvent kDD[] = {{.step=0, .tone=kSideStick, .octave=0, .vel=72, .gate=50}, {.step=3, .tone=kSideStick, .octave=0, .vel=62, .gate=50}, {.step=6, .tone=kSideStick, .octave=0, .vel=64, .gate=50}, {.step=8, .tone=kSideStick, .octave=0, .vel=70, .gate=50}, {.step=10, .tone=kSideStick, .octave=0, .vel=58, .gate=50}, {.step=11, .tone=kSideStick, .octave=0, .vel=62, .gate=50}, {.step=14, .tone=kSideStick, .octave=0, .vel=64, .gate=50}, {.step=0, .tone=kKick, .octave=0, .vel=66, .gate=50}, {.step=4, .tone=kKick, .octave=0, .vel=54, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=62, .gate=50}, {.step=12, .tone=kKick, .octave=0, .vel=54, .gate=50}, {.step=0, .tone=kClosedHat, .octave=0, .vel=50, .gate=50}, {.step=2, .tone=kClosedHat, .octave=0, .vel=46, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=50, .gate=50}, {.step=6, .tone=kClosedHat, .octave=0, .vel=46, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=50, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=46, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=50, .gate=50}, {.step=14, .tone=kClosedHat, .octave=0, .vel=46, .gate=50}};
inline constexpr StyleEvent kDC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=72, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=0, .tone=kFifth, .octave=0, .vel=72, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}, {.step=3, .tone=kThird, .octave=0, .vel=66, .gate=kGateStab}, {.step=3, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=68, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=68, .gate=kGateStab}, {.step=6, .tone=kSeventh, .octave=0, .vel=68, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=70, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=70, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=70, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=64, .gate=kGateStab}, {.step=14, .tone=kSeventh, .octave=0, .vel=64, .gate=kGateStab}};
inline constexpr StylePattern kDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBB)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kDC)}};
inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=1, .patterns=Span<const StylePattern>(kIn1P)}, {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIn2P)},
    {.type=SectionType::kVarA, .bars=1, .patterns=Span<const StylePattern>(kAP)}, {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kBP)},
    {.type=SectionType::kVarC, .bars=1, .patterns=Span<const StylePattern>(kCP)}, {.type=SectionType::kVarD, .bars=1, .patterns=Span<const StylePattern>(kDP)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFAP)}, {.type=SectionType::kFillB, .bars=1, .patterns=Span<const StylePattern>(kFBP)}, {.type=SectionType::kFillC, .bars=1, .patterns=Span<const StylePattern>(kFCP)}, {.type=SectionType::kFillD, .bars=1, .patterns=Span<const StylePattern>(kFDP)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kE1P)}, {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kE2P)},
};
inline constexpr Style kStyle{.name="bossa", .sections=Span<const StyleSection>(kSections)};
}  // namespace bossa

// ---------------------------------------------------------------------------
// "samba": driving surdo kick on the backbeat, busy tamborim 16ths, agogo
// bells, a syncopated root-fifth bass and short percussive stabs.
namespace samba {
inline constexpr StyleEvent kHeldBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=94, .gate=kGateHeld}};
inline constexpr StyleEvent kSyncBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=96, .gate=kGate8th}, {.step=4, .tone=kFifth, .octave=-1, .vel=100, .gate=kGate8th}, {.step=6, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStaccato}, {.step=8, .tone=kRoot, .octave=0, .vel=92, .gate=kGate8th}, {.step=12, .tone=kFifth, .octave=-1, .vel=100, .gate=kGate8th}, {.step=14, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStaccato}};
inline constexpr StyleEvent kIn1D[] = {{.step=8, .tone=kHiAgogo, .octave=0, .vel=70, .gate=50}, {.step=10, .tone=kLoAgogo, .octave=0, .vel=64, .gate=50}, {.step=12, .tone=kHiAgogo, .octave=0, .vel=76, .gate=50}, {.step=14, .tone=kLoAgogo, .octave=0, .vel=80, .gate=50}};
inline constexpr StylePattern kIn1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)}};
inline constexpr StyleEvent kIn2D[] = {{.step=4, .tone=kKick, .octave=0, .vel=100, .gate=50}, {.step=12, .tone=kKick, .octave=0, .vel=104, .gate=50}, {.step=0, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=2, .tone=kTambourine, .octave=0, .vel=54, .gate=50}, {.step=4, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=6, .tone=kTambourine, .octave=0, .vel=54, .gate=50}, {.step=8, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=10, .tone=kTambourine, .octave=0, .vel=54, .gate=50}, {.step=12, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=14, .tone=kTambourine, .octave=0, .vel=54, .gate=50}};
inline constexpr StyleEvent kIn2C[] = {{.step=2, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=2, .tone=kThird, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=2, .tone=kFifth, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=10, .tone=kRoot, .octave=0, .vel=74, .gate=kGateStaccato}, {.step=10, .tone=kThird, .octave=0, .vel=74, .gate=kGateStaccato}, {.step=10, .tone=kFifth, .octave=0, .vel=74, .gate=kGateStaccato}};
inline constexpr StylePattern kIn2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSyncBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIn2C)}};
inline constexpr StyleEvent kAD[] = {
    {.step=4, .tone=kKick, .octave=0, .vel=108, .gate=50}, {.step=12, .tone=kKick, .octave=0, .vel=112, .gate=50}, {.step=0, .tone=kKick, .octave=0, .vel=76, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=76, .gate=50},
    {.step=0, .tone=kTambourine, .octave=0, .vel=64, .gate=50}, {.step=1, .tone=kTambourine, .octave=0, .vel=48, .gate=50}, {.step=2, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=3, .tone=kTambourine, .octave=0, .vel=48, .gate=50}, {.step=4, .tone=kTambourine, .octave=0, .vel=64, .gate=50}, {.step=5, .tone=kTambourine, .octave=0, .vel=48, .gate=50}, {.step=6, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=7, .tone=kTambourine, .octave=0, .vel=48, .gate=50}, {.step=8, .tone=kTambourine, .octave=0, .vel=64, .gate=50}, {.step=9, .tone=kTambourine, .octave=0, .vel=48, .gate=50}, {.step=10, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=11, .tone=kTambourine, .octave=0, .vel=48, .gate=50}, {.step=12, .tone=kTambourine, .octave=0, .vel=64, .gate=50}, {.step=13, .tone=kTambourine, .octave=0, .vel=48, .gate=50}, {.step=14, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=15, .tone=kTambourine, .octave=0, .vel=48, .gate=50},
    {.step=0, .tone=kHiAgogo, .octave=0, .vel=72, .gate=50}, {.step=3, .tone=kLoAgogo, .octave=0, .vel=64, .gate=50}, {.step=6, .tone=kHiAgogo, .octave=0, .vel=68, .gate=50}, {.step=8, .tone=kHiAgogo, .octave=0, .vel=72, .gate=50}, {.step=11, .tone=kLoAgogo, .octave=0, .vel=64, .gate=50}, {.step=14, .tone=kHiAgogo, .octave=0, .vel=68, .gate=50},
};
inline constexpr StyleEvent kAC[] = {{.step=2, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=2, .tone=kThird, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=2, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=6, .tone=kThird, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=6, .tone=kFifth, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=6, .tone=kSeventh, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=10, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=10, .tone=kThird, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=10, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=14, .tone=kThird, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=14, .tone=kFifth, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=14, .tone=kSeventh, .octave=0, .vel=76, .gate=kGateStaccato}};
inline constexpr StylePattern kAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSyncBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAC)}};
inline constexpr StyleEvent kBD[] = {
    {.step=4, .tone=kKick, .octave=0, .vel=112, .gate=50}, {.step=12, .tone=kKick, .octave=0, .vel=116, .gate=50}, {.step=0, .tone=kKick, .octave=0, .vel=84, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=84, .gate=50}, {.step=7, .tone=kKick, .octave=0, .vel=70, .gate=50}, {.step=15, .tone=kKick, .octave=0, .vel=70, .gate=50},
    {.step=0, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=1, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=2, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=3, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=4, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=5, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=6, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=7, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=8, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=9, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=10, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=11, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=12, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=13, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=14, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=15, .tone=kTambourine, .octave=0, .vel=50, .gate=50},
    {.step=2, .tone=kOpenHiConga, .octave=0, .vel=70, .gate=50}, {.step=6, .tone=kLoConga, .octave=0, .vel=72, .gate=50}, {.step=10, .tone=kOpenHiConga, .octave=0, .vel=70, .gate=50}, {.step=14, .tone=kLoConga, .octave=0, .vel=74, .gate=50},
};
inline constexpr StyleEvent kBC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=0, .tone=kThird, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=0, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=0, .tone=kSeventh, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=3, .tone=kThird, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=3, .tone=kSeventh, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=6, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=6, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=10, .tone=kThird, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=10, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=13, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=13, .tone=kFifth, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=14, .tone=kThird, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=14, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStaccato}};
inline constexpr StylePattern kBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSyncBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBC)}};
inline constexpr StyleEvent kFAD[] = {{.step=0, .tone=kKick, .octave=0, .vel=100, .gate=50}, {.step=4, .tone=kTomHi, .octave=0, .vel=90, .gate=100}, {.step=6, .tone=kTomHi, .octave=0, .vel=86, .gate=100}, {.step=8, .tone=kTomMid, .octave=0, .vel=94, .gate=100}, {.step=10, .tone=kTomMid, .octave=0, .vel=90, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=100, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=106, .gate=100}};
inline constexpr StyleEvent kFBD[] = {{.step=0, .tone=kTomHi, .octave=0, .vel=90, .gate=100}, {.step=2, .tone=kTomHi, .octave=0, .vel=86, .gate=100}, {.step=4, .tone=kTomMid, .octave=0, .vel=94, .gate=100}, {.step=6, .tone=kTomMid, .octave=0, .vel=90, .gate=100}, {.step=8, .tone=kTomLow, .octave=0, .vel=98, .gate=100}, {.step=10, .tone=kTomLow, .octave=0, .vel=94, .gate=100}, {.step=12, .tone=kTomFloor, .octave=0, .vel=104, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=108, .gate=100}};
inline constexpr StyleEvent kFCD[] = {{.step=0, .tone=kOpenHiConga, .octave=0, .vel=88, .gate=50}, {.step=2, .tone=kLoConga, .octave=0, .vel=84, .gate=50}, {.step=4, .tone=kOpenHiConga, .octave=0, .vel=90, .gate=50}, {.step=6, .tone=kLoConga, .octave=0, .vel=86, .gate=50}, {.step=8, .tone=kTomHi, .octave=0, .vel=96, .gate=50}, {.step=10, .tone=kTomMid, .octave=0, .vel=100, .gate=50}, {.step=12, .tone=kTomLow, .octave=0, .vel=104, .gate=50}, {.step=14, .tone=kTomFloor, .octave=0, .vel=110, .gate=50}};
inline constexpr StyleEvent kFDD[] = {{.step=0, .tone=kTomHi, .octave=0, .vel=90, .gate=50}, {.step=1, .tone=kTomHi, .octave=0, .vel=86, .gate=50}, {.step=2, .tone=kTomMid, .octave=0, .vel=92, .gate=50}, {.step=3, .tone=kTomMid, .octave=0, .vel=88, .gate=50}, {.step=4, .tone=kTomLow, .octave=0, .vel=96, .gate=50}, {.step=5, .tone=kTomLow, .octave=0, .vel=92, .gate=50}, {.step=6, .tone=kTomFloor, .octave=0, .vel=100, .gate=50}, {.step=7, .tone=kTomFloor, .octave=0, .vel=96, .gate=50}, {.step=8, .tone=kSnare, .octave=0, .vel=102, .gate=50}, {.step=9, .tone=kSnare, .octave=0, .vel=98, .gate=50}, {.step=10, .tone=kSnare, .octave=0, .vel=104, .gate=50}, {.step=11, .tone=kSnare, .octave=0, .vel=100, .gate=50}, {.step=12, .tone=kSnare, .octave=0, .vel=108, .gate=50}, {.step=13, .tone=kSnare, .octave=0, .vel=112, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=116, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=120, .gate=kGate8th}};
inline constexpr StylePattern kFAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSyncBass)}};
inline constexpr StylePattern kFBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSyncBass)}};
inline constexpr StylePattern kFCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSyncBass)}};
inline constexpr StylePattern kFDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSyncBass)}};
inline constexpr StyleEvent kE1D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=100, .gate=kGateHalfBar}, {.step=0, .tone=kKick, .octave=0, .vel=104, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=96, .gate=50}};
inline constexpr StyleEvent kE1B[] = {{.step=0, .tone=kRoot, .octave=0, .vel=100, .gate=kGateHeld}};
inline constexpr StyleEvent kE1C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=84, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=84, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=84, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateHeld}};
inline constexpr StylePattern kE1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1C)}};
inline constexpr StyleEvent kE2D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=110, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=108, .gate=50}, {.step=4, .tone=kKick, .octave=0, .vel=100, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=104, .gate=50}, {.step=12, .tone=kKick, .octave=0, .vel=100, .gate=50}};
inline constexpr StyleEvent kE2B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=104, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=94, .gate=kGateHeld}};
inline constexpr StyleEvent kE2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=86, .gate=kGateHeld}};
inline constexpr StylePattern kE2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2C)}};
// varC: batucada breakdown — surdo backbeat and agogo clave only, tamborim dropped.
inline constexpr StyleEvent kCD[] = {{.step=4, .tone=kKick, .octave=0, .vel=108, .gate=50}, {.step=12, .tone=kKick, .octave=0, .vel=112, .gate=50}, {.step=0, .tone=kHiAgogo, .octave=0, .vel=72, .gate=50}, {.step=3, .tone=kLoAgogo, .octave=0, .vel=64, .gate=50}, {.step=6, .tone=kHiAgogo, .octave=0, .vel=68, .gate=50}, {.step=8, .tone=kHiAgogo, .octave=0, .vel=72, .gate=50}, {.step=11, .tone=kLoAgogo, .octave=0, .vel=64, .gate=50}, {.step=14, .tone=kHiAgogo, .octave=0, .vel=68, .gate=50}};
inline constexpr StyleEvent kCC[] = {{.step=2, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=2, .tone=kThird, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=2, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=10, .tone=kThird, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=10, .tone=kFifth, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=10, .tone=kSeventh, .octave=0, .vel=76, .gate=kGateStaccato}};
inline constexpr StylePattern kCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSyncBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCC)}};
// varD: peak escola — full surdo, 16th tamborim, congas and agogo, full 7th stabs.
inline constexpr StyleEvent kDD[] = {{.step=4, .tone=kKick, .octave=0, .vel=114, .gate=50}, {.step=12, .tone=kKick, .octave=0, .vel=118, .gate=50}, {.step=0, .tone=kKick, .octave=0, .vel=84, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=84, .gate=50}, {.step=7, .tone=kKick, .octave=0, .vel=70, .gate=50}, {.step=15, .tone=kKick, .octave=0, .vel=70, .gate=50}, {.step=0, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=1, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=2, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=3, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=4, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=5, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=6, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=7, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=8, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=9, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=10, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=11, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=12, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=13, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=14, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=15, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=2, .tone=kOpenHiConga, .octave=0, .vel=72, .gate=50}, {.step=6, .tone=kLoConga, .octave=0, .vel=74, .gate=50}, {.step=10, .tone=kOpenHiConga, .octave=0, .vel=72, .gate=50}, {.step=14, .tone=kLoConga, .octave=0, .vel=76, .gate=50}, {.step=0, .tone=kHiAgogo, .octave=0, .vel=74, .gate=50}, {.step=8, .tone=kHiAgogo, .octave=0, .vel=74, .gate=50}};
inline constexpr StyleEvent kDC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStaccato}, {.step=0, .tone=kThird, .octave=0, .vel=84, .gate=kGateStaccato}, {.step=0, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStaccato}, {.step=0, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStaccato}, {.step=6, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=6, .tone=kThird, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=6, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=10, .tone=kThird, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=10, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=10, .tone=kSeventh, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=14, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=14, .tone=kThird, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=14, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=14, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateStaccato}};
inline constexpr StylePattern kDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSyncBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kDC)}};
inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=1, .patterns=Span<const StylePattern>(kIn1P)}, {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIn2P)},
    {.type=SectionType::kVarA, .bars=1, .patterns=Span<const StylePattern>(kAP)}, {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kBP)},
    {.type=SectionType::kVarC, .bars=1, .patterns=Span<const StylePattern>(kCP)}, {.type=SectionType::kVarD, .bars=1, .patterns=Span<const StylePattern>(kDP)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFAP)}, {.type=SectionType::kFillB, .bars=1, .patterns=Span<const StylePattern>(kFBP)}, {.type=SectionType::kFillC, .bars=1, .patterns=Span<const StylePattern>(kFCP)}, {.type=SectionType::kFillD, .bars=1, .patterns=Span<const StylePattern>(kFDP)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kE1P)}, {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kE2P)},
};
inline constexpr Style kStyle{.name="samba", .sections=Span<const StyleSection>(kSections)};
}  // namespace samba

// ---------------------------------------------------------------------------
// "reggae": one-drop — kick and snare together on beat 3, off-beat skank
// upstroke chords, sparse syncopated root bass, hats on the off-beats.
namespace reggae {
inline constexpr StyleEvent kHeldBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=90, .gate=kGateHeld}};
inline constexpr StyleEvent kRootBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=96, .gate=kGate8th}, {.step=6, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=8, .tone=kFifth, .octave=-1, .vel=92, .gate=kGate8th}, {.step=11, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStab}};
inline constexpr StyleEvent kIn1D[] = {{.step=10, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=14, .tone=kClosedHat, .octave=0, .vel=62, .gate=50}, {.step=8, .tone=kSideStick, .octave=0, .vel=58, .gate=50}};
inline constexpr StylePattern kIn1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)}};
inline constexpr StyleEvent kIn2D[] = {{.step=8, .tone=kKick, .octave=0, .vel=100, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=92, .gate=120}, {.step=2, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=6, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=14, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}};
inline constexpr StyleEvent kIn2C[] = {{.step=2, .tone=kRoot, .octave=0, .vel=74, .gate=kGateStaccato}, {.step=2, .tone=kThird, .octave=0, .vel=74, .gate=kGateStaccato}, {.step=2, .tone=kFifth, .octave=0, .vel=74, .gate=kGateStaccato}, {.step=6, .tone=kRoot, .octave=0, .vel=72, .gate=kGateStaccato}, {.step=6, .tone=kThird, .octave=0, .vel=72, .gate=kGateStaccato}, {.step=6, .tone=kFifth, .octave=0, .vel=72, .gate=kGateStaccato}, {.step=10, .tone=kRoot, .octave=0, .vel=74, .gate=kGateStaccato}, {.step=10, .tone=kThird, .octave=0, .vel=74, .gate=kGateStaccato}, {.step=10, .tone=kFifth, .octave=0, .vel=74, .gate=kGateStaccato}, {.step=14, .tone=kRoot, .octave=0, .vel=72, .gate=kGateStaccato}, {.step=14, .tone=kThird, .octave=0, .vel=72, .gate=kGateStaccato}, {.step=14, .tone=kFifth, .octave=0, .vel=72, .gate=kGateStaccato}};
inline constexpr StylePattern kIn2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kRootBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIn2C)}};
inline constexpr StyleEvent kAD[] = {
    {.step=8, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=100, .gate=120},
    {.step=2, .tone=kClosedHat, .octave=0, .vel=62, .gate=50}, {.step=6, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=62, .gate=50}, {.step=14, .tone=kClosedHat, .octave=0, .vel=58, .gate=50},
    {.step=0, .tone=kSideStick, .octave=0, .vel=54, .gate=50}, {.step=4, .tone=kSideStick, .octave=0, .vel=50, .gate=50},
};
inline constexpr StyleEvent kAC[] = {{.step=2, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=2, .tone=kThird, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=2, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=6, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=6, .tone=kThird, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=6, .tone=kFifth, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=10, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=10, .tone=kThird, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=10, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=14, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=14, .tone=kThird, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=14, .tone=kFifth, .octave=0, .vel=76, .gate=kGateStaccato}};
inline constexpr StylePattern kAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kRootBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAC)}};
inline constexpr StyleEvent kBD[] = {
    {.step=8, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=104, .gate=120}, {.step=0, .tone=kKick, .octave=0, .vel=78, .gate=50},
    {.step=0, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=2, .tone=kOpenHat, .octave=0, .vel=68, .gate=120}, {.step=4, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=6, .tone=kOpenHat, .octave=0, .vel=68, .gate=120}, {.step=8, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=10, .tone=kOpenHat, .octave=0, .vel=68, .gate=120}, {.step=12, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=14, .tone=kOpenHat, .octave=0, .vel=68, .gate=120},
    {.step=4, .tone=kSideStick, .octave=0, .vel=54, .gate=50}, {.step=12, .tone=kSideStick, .octave=0, .vel=56, .gate=50},
};
inline constexpr StyleEvent kBC[] = {{.step=2, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=2, .tone=kThird, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=2, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=2, .tone=kSeventh, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=4, .tone=kRoot, .octave=0, .vel=70, .gate=kGateStaccato}, {.step=4, .tone=kThird, .octave=0, .vel=70, .gate=kGateStaccato}, {.step=4, .tone=kFifth, .octave=0, .vel=70, .gate=kGateStaccato}, {.step=6, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=6, .tone=kThird, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=6, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=10, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=10, .tone=kThird, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=10, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=12, .tone=kRoot, .octave=0, .vel=70, .gate=kGateStaccato}, {.step=12, .tone=kThird, .octave=0, .vel=70, .gate=kGateStaccato}, {.step=12, .tone=kFifth, .octave=0, .vel=70, .gate=kGateStaccato}, {.step=14, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=14, .tone=kThird, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=14, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStaccato}};
inline constexpr StylePattern kBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kRootBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBC)}};
inline constexpr StyleEvent kFAD[] = {{.step=0, .tone=kSideStick, .octave=0, .vel=64, .gate=50}, {.step=4, .tone=kSideStick, .octave=0, .vel=68, .gate=50}, {.step=8, .tone=kSnare, .octave=0, .vel=90, .gate=120}, {.step=10, .tone=kSnare, .octave=0, .vel=94, .gate=120}, {.step=12, .tone=kTomLow, .octave=0, .vel=98, .gate=120}, {.step=14, .tone=kTomFloor, .octave=0, .vel=104, .gate=120}};
inline constexpr StyleEvent kFBD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=88, .gate=100}, {.step=4, .tone=kTomHi, .octave=0, .vel=92, .gate=100}, {.step=6, .tone=kTomHi, .octave=0, .vel=88, .gate=100}, {.step=8, .tone=kTomMid, .octave=0, .vel=96, .gate=100}, {.step=10, .tone=kTomMid, .octave=0, .vel=92, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=100, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=106, .gate=100}};
inline constexpr StyleEvent kFCD[] = {{.step=0, .tone=kTomHi, .octave=0, .vel=90, .gate=100}, {.step=2, .tone=kTomHi, .octave=0, .vel=86, .gate=100}, {.step=4, .tone=kTomMid, .octave=0, .vel=94, .gate=100}, {.step=6, .tone=kTomMid, .octave=0, .vel=90, .gate=100}, {.step=8, .tone=kSnare, .octave=0, .vel=98, .gate=100}, {.step=10, .tone=kSnare, .octave=0, .vel=94, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=102, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=108, .gate=100}};
inline constexpr StyleEvent kFDD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=88, .gate=50}, {.step=2, .tone=kSnare, .octave=0, .vel=82, .gate=50}, {.step=3, .tone=kSnare, .octave=0, .vel=86, .gate=50}, {.step=4, .tone=kTomHi, .octave=0, .vel=94, .gate=50}, {.step=6, .tone=kTomHi, .octave=0, .vel=90, .gate=50}, {.step=8, .tone=kTomMid, .octave=0, .vel=98, .gate=50}, {.step=10, .tone=kTomLow, .octave=0, .vel=102, .gate=50}, {.step=12, .tone=kTomFloor, .octave=0, .vel=108, .gate=50}, {.step=13, .tone=kSnare, .octave=0, .vel=112, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=116, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=120, .gate=kGate8th}};
inline constexpr StylePattern kFAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kRootBass)}};
inline constexpr StylePattern kFBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kRootBass)}};
inline constexpr StylePattern kFCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kRootBass)}};
inline constexpr StylePattern kFDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kRootBass)}};
inline constexpr StyleEvent kE1D[] = {{.step=8, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=104, .gate=120}, {.step=8, .tone=kCrash, .octave=0, .vel=100, .gate=kGateHalfBar}};
inline constexpr StyleEvent kE1B[] = {{.step=0, .tone=kRoot, .octave=0, .vel=96, .gate=kGateHeld}};
inline constexpr StyleEvent kE1C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=84, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=84, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=84, .gate=kGateHeld}};
inline constexpr StylePattern kE1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1C)}};
inline constexpr StyleEvent kE2D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=108, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=104, .gate=120}};
inline constexpr StyleEvent kE2B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=100, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=90, .gate=kGateHeld}};
inline constexpr StyleEvent kE2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=86, .gate=kGateHeld}};
inline constexpr StylePattern kE2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2C)}};
// varC: steppers — four-on-the-floor kick under the backbeat, off-beat hats and skank.
inline constexpr StyleEvent kCD[] = {{.step=0, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=4, .tone=kKick, .octave=0, .vel=100, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=106, .gate=120}, {.step=12, .tone=kKick, .octave=0, .vel=100, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=100, .gate=120}, {.step=2, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=6, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=14, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}};
inline constexpr StyleEvent kCC[] = {{.step=2, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=2, .tone=kThird, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=2, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=6, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=6, .tone=kThird, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=6, .tone=kFifth, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=10, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=10, .tone=kThird, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=10, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=14, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=14, .tone=kThird, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=14, .tone=kFifth, .octave=0, .vel=76, .gate=kGateStaccato}};
inline constexpr StylePattern kCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kRootBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCC)}};
// varD: rockers peak — busy one-drop with ghost snares, open hats, double 7th skank.
inline constexpr StyleEvent kDD[] = {{.step=8, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=0, .tone=kKick, .octave=0, .vel=84, .gate=50}, {.step=8, .tone=kSnare, .octave=0, .vel=104, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=52, .gate=50}, {.step=12, .tone=kSnare, .octave=0, .vel=54, .gate=50}, {.step=0, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=2, .tone=kOpenHat, .octave=0, .vel=68, .gate=120}, {.step=4, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=6, .tone=kOpenHat, .octave=0, .vel=68, .gate=120}, {.step=8, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=10, .tone=kOpenHat, .octave=0, .vel=68, .gate=120}, {.step=12, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=14, .tone=kOpenHat, .octave=0, .vel=68, .gate=120}, {.step=4, .tone=kSideStick, .octave=0, .vel=54, .gate=50}, {.step=12, .tone=kSideStick, .octave=0, .vel=56, .gate=50}};
inline constexpr StyleEvent kDC[] = {{.step=2, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=2, .tone=kThird, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=2, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=2, .tone=kSeventh, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=4, .tone=kRoot, .octave=0, .vel=70, .gate=kGateStaccato}, {.step=4, .tone=kThird, .octave=0, .vel=70, .gate=kGateStaccato}, {.step=4, .tone=kFifth, .octave=0, .vel=70, .gate=kGateStaccato}, {.step=6, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=6, .tone=kThird, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=6, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=10, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=10, .tone=kThird, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=10, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=10, .tone=kSeventh, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=14, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=14, .tone=kThird, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=14, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStaccato}};
inline constexpr StylePattern kDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kRootBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kDC)}};
inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=1, .patterns=Span<const StylePattern>(kIn1P)}, {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIn2P)},
    {.type=SectionType::kVarA, .bars=1, .patterns=Span<const StylePattern>(kAP)}, {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kBP)},
    {.type=SectionType::kVarC, .bars=1, .patterns=Span<const StylePattern>(kCP)}, {.type=SectionType::kVarD, .bars=1, .patterns=Span<const StylePattern>(kDP)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFAP)}, {.type=SectionType::kFillB, .bars=1, .patterns=Span<const StylePattern>(kFBP)}, {.type=SectionType::kFillC, .bars=1, .patterns=Span<const StylePattern>(kFCP)}, {.type=SectionType::kFillD, .bars=1, .patterns=Span<const StylePattern>(kFDP)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kE1P)}, {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kE2P)},
};
inline constexpr Style kStyle{.name="reggae", .sections=Span<const StyleSection>(kSections)};
}  // namespace reggae

// ---------------------------------------------------------------------------
// "country": boom-chick train beat — kick on 1 & 3, snare on 2 & 4, brushed
// eighth hats, an alternating root-fifth "boom-chick" bass and backbeat strums.
namespace country {
inline constexpr StyleEvent kHeldBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=88, .gate=kGateHeld}};
inline constexpr StyleEvent kBoomBass[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=96, .gate=kGateBeat}, {.step=4, .tone=kFifth, .octave=-1, .vel=86, .gate=kGateBeat}, {.step=8, .tone=kRoot, .octave=-1, .vel=94, .gate=kGateBeat}, {.step=12, .tone=kFifth, .octave=-1, .vel=86, .gate=kGateBeat}};
inline constexpr StyleEvent kIn1D[] = {{.step=8, .tone=kClosedHat, .octave=0, .vel=54, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=12, .tone=kSnare, .octave=0, .vel=72, .gate=100}, {.step=14, .tone=kSnare, .octave=0, .vel=84, .gate=100}};
inline constexpr StylePattern kIn1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)}};
inline constexpr StyleEvent kIn2D[] = {{.step=0, .tone=kKick, .octave=0, .vel=98, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=94, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=90, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=92, .gate=120}, {.step=0, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=2, .tone=kClosedHat, .octave=0, .vel=50, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=6, .tone=kClosedHat, .octave=0, .vel=50, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=50, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=14, .tone=kClosedHat, .octave=0, .vel=50, .gate=50}};
inline constexpr StyleEvent kIn2C[] = {{.step=4, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStab}, {.step=4, .tone=kThird, .octave=0, .vel=76, .gate=kGateStab}, {.step=4, .tone=kFifth, .octave=0, .vel=76, .gate=kGateStab}, {.step=12, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStab}, {.step=12, .tone=kThird, .octave=0, .vel=76, .gate=kGateStab}, {.step=12, .tone=kFifth, .octave=0, .vel=76, .gate=kGateStab}};
inline constexpr StylePattern kIn2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoomBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIn2C)}};
inline constexpr StyleEvent kAD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=100, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=98, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=98, .gate=120},
    {.step=0, .tone=kClosedHat, .octave=0, .vel=62, .gate=50}, {.step=2, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=62, .gate=50}, {.step=6, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=62, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=62, .gate=50}, {.step=14, .tone=kClosedHat, .octave=0, .vel=52, .gate=50},
};
inline constexpr StyleEvent kAC[] = {{.step=4, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=4, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=4, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}, {.step=12, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=12, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=12, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}};
inline constexpr StylePattern kAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoomBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAC)}};
inline constexpr StyleEvent kBD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=106, .gate=120}, {.step=6, .tone=kKick, .octave=0, .vel=88, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=102, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=100, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=100, .gate=120},
    {.step=0, .tone=kClosedHat, .octave=0, .vel=64, .gate=50}, {.step=1, .tone=kClosedHat, .octave=0, .vel=48, .gate=50}, {.step=2, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=3, .tone=kClosedHat, .octave=0, .vel=48, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=64, .gate=50}, {.step=5, .tone=kClosedHat, .octave=0, .vel=48, .gate=50}, {.step=6, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=7, .tone=kClosedHat, .octave=0, .vel=48, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=64, .gate=50}, {.step=9, .tone=kClosedHat, .octave=0, .vel=48, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=11, .tone=kClosedHat, .octave=0, .vel=48, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=64, .gate=50}, {.step=13, .tone=kClosedHat, .octave=0, .vel=48, .gate=50}, {.step=14, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=15, .tone=kClosedHat, .octave=0, .vel=48, .gate=50},
};
inline constexpr StyleEvent kBB[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=98, .gate=kGate8th}, {.step=2, .tone=kFifth, .octave=-1, .vel=80, .gate=kGate8th}, {.step=4, .tone=kFifth, .octave=-1, .vel=88, .gate=kGate8th}, {.step=6, .tone=kThird, .octave=0, .vel=80, .gate=kGate8th}, {.step=8, .tone=kRoot, .octave=-1, .vel=96, .gate=kGate8th}, {.step=10, .tone=kFifth, .octave=-1, .vel=80, .gate=kGate8th}, {.step=12, .tone=kFifth, .octave=-1, .vel=88, .gate=kGate8th}, {.step=14, .tone=kSeventh, .octave=-1, .vel=82, .gate=kGate8th}};
inline constexpr StyleEvent kBC[] = {{.step=4, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStab}, {.step=4, .tone=kThird, .octave=0, .vel=82, .gate=kGateStab}, {.step=4, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStab}, {.step=6, .tone=kRoot, .octave=0, .vel=70, .gate=kGateStaccato}, {.step=6, .tone=kThird, .octave=0, .vel=70, .gate=kGateStaccato}, {.step=6, .tone=kFifth, .octave=0, .vel=70, .gate=kGateStaccato}, {.step=12, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStab}, {.step=12, .tone=kThird, .octave=0, .vel=82, .gate=kGateStab}, {.step=12, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStab}, {.step=14, .tone=kRoot, .octave=0, .vel=70, .gate=kGateStaccato}, {.step=14, .tone=kThird, .octave=0, .vel=70, .gate=kGateStaccato}, {.step=14, .tone=kFifth, .octave=0, .vel=70, .gate=kGateStaccato}};
inline constexpr StylePattern kBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBB)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBC)}};
inline constexpr StyleEvent kFAD[] = {{.step=0, .tone=kKick, .octave=0, .vel=102, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=96, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=90, .gate=120}, {.step=10, .tone=kSnare, .octave=0, .vel=94, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=100, .gate=120}, {.step=14, .tone=kSnare, .octave=0, .vel=106, .gate=120}};
inline constexpr StyleEvent kFBD[] = {{.step=0, .tone=kKick, .octave=0, .vel=102, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=96, .gate=100}, {.step=6, .tone=kSnare, .octave=0, .vel=90, .gate=100}, {.step=8, .tone=kTomHi, .octave=0, .vel=94, .gate=100}, {.step=10, .tone=kTomMid, .octave=0, .vel=98, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=102, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=108, .gate=100}};
inline constexpr StyleEvent kFCD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=92, .gate=100}, {.step=2, .tone=kSnare, .octave=0, .vel=86, .gate=100}, {.step=4, .tone=kTomHi, .octave=0, .vel=96, .gate=100}, {.step=6, .tone=kTomHi, .octave=0, .vel=92, .gate=100}, {.step=8, .tone=kTomMid, .octave=0, .vel=100, .gate=100}, {.step=10, .tone=kTomMid, .octave=0, .vel=96, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=106, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=112, .gate=100}};
inline constexpr StyleEvent kFDD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=90, .gate=50}, {.step=1, .tone=kSnare, .octave=0, .vel=84, .gate=50}, {.step=2, .tone=kSnare, .octave=0, .vel=94, .gate=50}, {.step=3, .tone=kSnare, .octave=0, .vel=88, .gate=50}, {.step=4, .tone=kTomHi, .octave=0, .vel=98, .gate=50}, {.step=6, .tone=kTomHi, .octave=0, .vel=94, .gate=50}, {.step=8, .tone=kTomMid, .octave=0, .vel=102, .gate=50}, {.step=10, .tone=kTomLow, .octave=0, .vel=106, .gate=50}, {.step=12, .tone=kTomFloor, .octave=0, .vel=110, .gate=50}, {.step=13, .tone=kSnare, .octave=0, .vel=114, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=118, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=120, .gate=kGate8th}};
inline constexpr StylePattern kFAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoomBass)}};
inline constexpr StylePattern kFBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoomBass)}};
inline constexpr StylePattern kFCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoomBass)}};
inline constexpr StylePattern kFDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoomBass)}};
inline constexpr StyleEvent kE1D[] = {{.step=0, .tone=kKick, .octave=0, .vel=106, .gate=120}, {.step=0, .tone=kCrash, .octave=0, .vel=100, .gate=kGateHalfBar}};
inline constexpr StyleEvent kE1B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=98, .gate=kGateHeld}};
inline constexpr StyleEvent kE1C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=84, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=84, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=84, .gate=kGateHeld}};
inline constexpr StylePattern kE1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1C)}};
inline constexpr StyleEvent kE2D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=108, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=108, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=100, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=100, .gate=120}};
inline constexpr StyleEvent kE2B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=102, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=92, .gate=kGateHeld}};
inline constexpr StyleEvent kE2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=1, .vel=82, .gate=kGateHeld}};
inline constexpr StylePattern kE2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2C)}};
// varC: half-time ballad feel — backbeat on beat 3, brushed quarter hats, sustained triad.
inline constexpr StyleEvent kCD[] = {{.step=0, .tone=kKick, .octave=0, .vel=102, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=98, .gate=120}, {.step=0, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}};
inline constexpr StyleEvent kCC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=78, .gate=kGateBeat}, {.step=0, .tone=kThird, .octave=0, .vel=78, .gate=kGateBeat}, {.step=0, .tone=kFifth, .octave=0, .vel=78, .gate=kGateBeat}, {.step=8, .tone=kRoot, .octave=0, .vel=76, .gate=kGateBeat}, {.step=8, .tone=kThird, .octave=0, .vel=76, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=76, .gate=kGateBeat}};
inline constexpr StylePattern kCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoomBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCC)}};
// varD: double-time train peak — kick pushes, snare 2 & 4, 16th hats, full 7th backbeat strums.
inline constexpr StyleEvent kDD[] = {{.step=0, .tone=kKick, .octave=0, .vel=106, .gate=120}, {.step=6, .tone=kKick, .octave=0, .vel=88, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=102, .gate=120}, {.step=10, .tone=kKick, .octave=0, .vel=88, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=102, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=102, .gate=120}, {.step=0, .tone=kClosedHat, .octave=0, .vel=64, .gate=50}, {.step=1, .tone=kClosedHat, .octave=0, .vel=48, .gate=50}, {.step=2, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=3, .tone=kClosedHat, .octave=0, .vel=48, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=64, .gate=50}, {.step=5, .tone=kClosedHat, .octave=0, .vel=48, .gate=50}, {.step=6, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=7, .tone=kClosedHat, .octave=0, .vel=48, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=64, .gate=50}, {.step=9, .tone=kClosedHat, .octave=0, .vel=48, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=11, .tone=kClosedHat, .octave=0, .vel=48, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=64, .gate=50}, {.step=13, .tone=kClosedHat, .octave=0, .vel=48, .gate=50}, {.step=14, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=15, .tone=kClosedHat, .octave=0, .vel=48, .gate=50}};
inline constexpr StyleEvent kDC[] = {{.step=4, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=4, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=4, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=4, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}, {.step=6, .tone=kRoot, .octave=0, .vel=70, .gate=kGateStaccato}, {.step=6, .tone=kThird, .octave=0, .vel=70, .gate=kGateStaccato}, {.step=6, .tone=kFifth, .octave=0, .vel=70, .gate=kGateStaccato}, {.step=12, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=12, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=12, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=12, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}, {.step=14, .tone=kRoot, .octave=0, .vel=70, .gate=kGateStaccato}, {.step=14, .tone=kThird, .octave=0, .vel=70, .gate=kGateStaccato}, {.step=14, .tone=kFifth, .octave=0, .vel=70, .gate=kGateStaccato}};
inline constexpr StylePattern kDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBB)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kDC)}};
inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=1, .patterns=Span<const StylePattern>(kIn1P)}, {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIn2P)},
    {.type=SectionType::kVarA, .bars=1, .patterns=Span<const StylePattern>(kAP)}, {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kBP)},
    {.type=SectionType::kVarC, .bars=1, .patterns=Span<const StylePattern>(kCP)}, {.type=SectionType::kVarD, .bars=1, .patterns=Span<const StylePattern>(kDP)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFAP)}, {.type=SectionType::kFillB, .bars=1, .patterns=Span<const StylePattern>(kFBP)}, {.type=SectionType::kFillC, .bars=1, .patterns=Span<const StylePattern>(kFCP)}, {.type=SectionType::kFillD, .bars=1, .patterns=Span<const StylePattern>(kFDP)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kE1P)}, {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kE2P)},
};
inline constexpr Style kStyle{.name="country", .sections=Span<const StyleSection>(kSections)};
}  // namespace country

// ---------------------------------------------------------------------------
// "blues": slow 12/8 shuffle — triplet ride on the long-short eighths, soft
// kick/snare, a boogie root-fifth-seventh bass and dominant-7 shuffle stabs.
namespace blues {
inline constexpr StyleEvent kHeldBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=82, .gate=kGateHeld}};
inline constexpr StyleEvent kBoogieBass[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=90, .gate=kGate8th}, {.step=4, .tone=kFifth, .octave=-1, .vel=82, .gate=kGate8th}, {.step=7, .tone=kSeventh, .octave=-1, .vel=78, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=-1, .vel=88, .gate=kGate8th}, {.step=12, .tone=kFifth, .octave=-1, .vel=82, .gate=kGate8th}, {.step=15, .tone=kSeventh, .octave=-1, .vel=78, .gate=kGateStab}};
inline constexpr StyleEvent kIn1D[] = {{.step=8, .tone=kRide, .octave=0, .vel=56, .gate=120}, {.step=11, .tone=kRide, .octave=0, .vel=48, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=60, .gate=120}, {.step=15, .tone=kRide, .octave=0, .vel=52, .gate=120}};
inline constexpr StylePattern kIn1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)}};
inline constexpr StyleEvent kIn2D[] = {{.step=0, .tone=kRide, .octave=0, .vel=64, .gate=120}, {.step=3, .tone=kRide, .octave=0, .vel=50, .gate=120}, {.step=4, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=7, .tone=kRide, .octave=0, .vel=50, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=62, .gate=120}, {.step=11, .tone=kRide, .octave=0, .vel=50, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=15, .tone=kRide, .octave=0, .vel=50, .gate=120}, {.step=0, .tone=kKick, .octave=0, .vel=80, .gate=100}, {.step=8, .tone=kKick, .octave=0, .vel=78, .gate=100}, {.step=4, .tone=kSnare, .octave=0, .vel=76, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=78, .gate=100}};
inline constexpr StyleEvent kIn2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=72, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=70, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=70, .gate=kGateStab}, {.step=8, .tone=kSeventh, .octave=0, .vel=70, .gate=kGateStab}};
inline constexpr StylePattern kIn2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIn2C)}};
inline constexpr StyleEvent kAD[] = {
    {.step=0, .tone=kRide, .octave=0, .vel=74, .gate=120}, {.step=3, .tone=kRide, .octave=0, .vel=52, .gate=120}, {.step=4, .tone=kRide, .octave=0, .vel=66, .gate=120}, {.step=7, .tone=kRide, .octave=0, .vel=52, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=72, .gate=120}, {.step=11, .tone=kRide, .octave=0, .vel=52, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=66, .gate=120}, {.step=15, .tone=kRide, .octave=0, .vel=52, .gate=120},
    {.step=0, .tone=kKick, .octave=0, .vel=88, .gate=100}, {.step=8, .tone=kKick, .octave=0, .vel=84, .gate=100}, {.step=4, .tone=kSnare, .octave=0, .vel=94, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=96, .gate=100},
};
inline constexpr StyleEvent kAC[] = {{.step=3, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStab}, {.step=3, .tone=kThird, .octave=0, .vel=76, .gate=kGateStab}, {.step=3, .tone=kSeventh, .octave=0, .vel=76, .gate=kGateStab}, {.step=11, .tone=kRoot, .octave=0, .vel=74, .gate=kGateStab}, {.step=11, .tone=kThird, .octave=0, .vel=74, .gate=kGateStab}, {.step=11, .tone=kSeventh, .octave=0, .vel=74, .gate=kGateStab}};
inline constexpr StylePattern kAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAC)}};
inline constexpr StyleEvent kBD[] = {
    {.step=0, .tone=kRide, .octave=0, .vel=78, .gate=120}, {.step=3, .tone=kRide, .octave=0, .vel=56, .gate=120}, {.step=4, .tone=kRide, .octave=0, .vel=70, .gate=120}, {.step=7, .tone=kRide, .octave=0, .vel=56, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=76, .gate=120}, {.step=11, .tone=kRide, .octave=0, .vel=56, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=70, .gate=120}, {.step=15, .tone=kRide, .octave=0, .vel=56, .gate=120},
    {.step=0, .tone=kKick, .octave=0, .vel=92, .gate=100}, {.step=7, .tone=kKick, .octave=0, .vel=76, .gate=100}, {.step=8, .tone=kKick, .octave=0, .vel=90, .gate=100}, {.step=4, .tone=kSnare, .octave=0, .vel=98, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=100, .gate=100}, {.step=15, .tone=kSnare, .octave=0, .vel=60, .gate=50},
};
inline constexpr StyleEvent kBC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStab}, {.step=3, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=3, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=8, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStab}, {.step=11, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=11, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}};
inline constexpr StylePattern kBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBC)}};
inline constexpr StyleEvent kFAD[] = {{.step=0, .tone=kKick, .octave=0, .vel=90, .gate=100}, {.step=4, .tone=kSnare, .octave=0, .vel=90, .gate=100}, {.step=8, .tone=kSnare, .octave=0, .vel=84, .gate=100}, {.step=11, .tone=kSnare, .octave=0, .vel=88, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=94, .gate=100}, {.step=15, .tone=kSnare, .octave=0, .vel=100, .gate=100}};
inline constexpr StyleEvent kFBD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=84, .gate=100}, {.step=3, .tone=kSnare, .octave=0, .vel=78, .gate=100}, {.step=4, .tone=kTomHi, .octave=0, .vel=88, .gate=100}, {.step=7, .tone=kTomMid, .octave=0, .vel=86, .gate=100}, {.step=8, .tone=kSnare, .octave=0, .vel=90, .gate=100}, {.step=11, .tone=kTomLow, .octave=0, .vel=92, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=96, .gate=100}, {.step=15, .tone=kTomFloor, .octave=0, .vel=100, .gate=100}};
inline constexpr StyleEvent kFCD[] = {{.step=0, .tone=kTomHi, .octave=0, .vel=84, .gate=100}, {.step=3, .tone=kTomHi, .octave=0, .vel=80, .gate=100}, {.step=4, .tone=kTomMid, .octave=0, .vel=88, .gate=100}, {.step=7, .tone=kTomMid, .octave=0, .vel=84, .gate=100}, {.step=8, .tone=kTomLow, .octave=0, .vel=92, .gate=100}, {.step=11, .tone=kTomLow, .octave=0, .vel=88, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=96, .gate=100}, {.step=15, .tone=kTomFloor, .octave=0, .vel=102, .gate=100}};
inline constexpr StyleEvent kFDD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=84, .gate=50}, {.step=2, .tone=kSnare, .octave=0, .vel=78, .gate=50}, {.step=3, .tone=kTomHi, .octave=0, .vel=88, .gate=50}, {.step=4, .tone=kTomHi, .octave=0, .vel=92, .gate=50}, {.step=6, .tone=kTomMid, .octave=0, .vel=90, .gate=50}, {.step=7, .tone=kTomMid, .octave=0, .vel=94, .gate=50}, {.step=8, .tone=kTomLow, .octave=0, .vel=96, .gate=50}, {.step=10, .tone=kTomLow, .octave=0, .vel=92, .gate=50}, {.step=11, .tone=kTomFloor, .octave=0, .vel=98, .gate=50}, {.step=12, .tone=kSnare, .octave=0, .vel=102, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=108, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=112, .gate=kGate8th}};
inline constexpr StylePattern kFAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass)}};
inline constexpr StylePattern kFBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass)}};
inline constexpr StylePattern kFCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass)}};
inline constexpr StylePattern kFDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass)}};
inline constexpr StyleEvent kE1D[] = {{.step=0, .tone=kKick, .octave=0, .vel=94, .gate=100}, {.step=0, .tone=kCrash, .octave=0, .vel=90, .gate=kGateHalfBar}};
inline constexpr StyleEvent kE1B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=90, .gate=kGateHeld}};
inline constexpr StyleEvent kE1C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=80, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=80, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateHeld}};
inline constexpr StylePattern kE1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1C)}};
inline constexpr StyleEvent kE2D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=98, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=94, .gate=100}, {.step=8, .tone=kKick, .octave=0, .vel=88, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=92, .gate=100}};
inline constexpr StyleEvent kE2B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=94, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=-1, .vel=82, .gate=kGateHeld}};
inline constexpr StyleEvent kE2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=82, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=82, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=82, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=82, .gate=kGateHeld}};
inline constexpr StylePattern kE2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2C)}};
// varC: slow-drag stop-time — bare ride and kick/snare on the beats, wide space.
inline constexpr StyleEvent kCD[] = {{.step=0, .tone=kRide, .octave=0, .vel=70, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=66, .gate=120}, {.step=0, .tone=kKick, .octave=0, .vel=86, .gate=100}, {.step=8, .tone=kSnare, .octave=0, .vel=90, .gate=100}};
inline constexpr StyleEvent kCC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=76, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=76, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=74, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=74, .gate=kGateStab}, {.step=8, .tone=kSeventh, .octave=0, .vel=74, .gate=kGateStab}};
inline constexpr StylePattern kCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCC)}};
// varD: peak shuffle — full triplet ride, driving kick/snare, full dominant-7 stabs.
inline constexpr StyleEvent kDD[] = {{.step=0, .tone=kRide, .octave=0, .vel=80, .gate=120}, {.step=3, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=4, .tone=kRide, .octave=0, .vel=72, .gate=120}, {.step=7, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=78, .gate=120}, {.step=11, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=72, .gate=120}, {.step=15, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=0, .tone=kKick, .octave=0, .vel=94, .gate=100}, {.step=7, .tone=kKick, .octave=0, .vel=78, .gate=100}, {.step=8, .tone=kKick, .octave=0, .vel=92, .gate=100}, {.step=4, .tone=kSnare, .octave=0, .vel=100, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=102, .gate=100}, {.step=15, .tone=kSnare, .octave=0, .vel=62, .gate=50}};
inline constexpr StyleEvent kDC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=0, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateStab}, {.step=3, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=3, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=8, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}, {.step=8, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateStab}, {.step=11, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=11, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}};
inline constexpr StylePattern kDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kDC)}};
inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=1, .patterns=Span<const StylePattern>(kIn1P)}, {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIn2P)},
    {.type=SectionType::kVarA, .bars=1, .patterns=Span<const StylePattern>(kAP)}, {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kBP)},
    {.type=SectionType::kVarC, .bars=1, .patterns=Span<const StylePattern>(kCP)}, {.type=SectionType::kVarD, .bars=1, .patterns=Span<const StylePattern>(kDP)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFAP)}, {.type=SectionType::kFillB, .bars=1, .patterns=Span<const StylePattern>(kFBP)}, {.type=SectionType::kFillC, .bars=1, .patterns=Span<const StylePattern>(kFCP)}, {.type=SectionType::kFillD, .bars=1, .patterns=Span<const StylePattern>(kFDP)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kE1P)}, {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kE2P)},
};
inline constexpr Style kStyle{.name="blues", .sections=Span<const StyleSection>(kSections)};
}  // namespace blues

// ---------------------------------------------------------------------------
// "shuffle": driving Texas shuffle — bouncy closed-hat shuffle eighths, hard
// backbeat, a bouncing root-fifth shuffle bass and bright triad shuffle stabs.
namespace shuffle {
inline constexpr StyleEvent kHeldBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=96, .gate=kGateHeld}};
inline constexpr StyleEvent kShufBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=104, .gate=kGate8th}, {.step=3, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=4, .tone=kFifth, .octave=0, .vel=96, .gate=kGate8th}, {.step=7, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=102, .gate=kGate8th}, {.step=11, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=12, .tone=kFifth, .octave=0, .vel=96, .gate=kGate8th}, {.step=15, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}};
inline constexpr StyleEvent kIn1D[] = {{.step=8, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=11, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=12, .tone=kSnare, .octave=0, .vel=80, .gate=100}, {.step=15, .tone=kSnare, .octave=0, .vel=92, .gate=100}};
inline constexpr StylePattern kIn1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)}};
inline constexpr StyleEvent kIn2D[] = {{.step=0, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=100, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=98, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=98, .gate=120}, {.step=0, .tone=kClosedHat, .octave=0, .vel=66, .gate=50}, {.step=3, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=62, .gate=50}, {.step=7, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=66, .gate=50}, {.step=11, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=62, .gate=50}, {.step=15, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}};
inline constexpr StyleEvent kIn2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=0, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=8, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStab}};
inline constexpr StylePattern kIn2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kShufBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIn2C)}};
inline constexpr StyleEvent kAD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=106, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=108, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=108, .gate=120},
    {.step=0, .tone=kClosedHat, .octave=0, .vel=72, .gate=50}, {.step=3, .tone=kClosedHat, .octave=0, .vel=54, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=68, .gate=50}, {.step=7, .tone=kClosedHat, .octave=0, .vel=54, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=72, .gate=50}, {.step=11, .tone=kClosedHat, .octave=0, .vel=54, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=68, .gate=50}, {.step=15, .tone=kClosedHat, .octave=0, .vel=54, .gate=50},
};
inline constexpr StyleEvent kAC[] = {{.step=3, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=3, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=3, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=11, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStab}, {.step=11, .tone=kThird, .octave=0, .vel=82, .gate=kGateStab}, {.step=11, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStab}};
inline constexpr StylePattern kAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kShufBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAC)}};
inline constexpr StyleEvent kBD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=114, .gate=120}, {.step=7, .tone=kKick, .octave=0, .vel=92, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=110, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=110, .gate=120}, {.step=15, .tone=kSnare, .octave=0, .vel=64, .gate=50},
    {.step=0, .tone=kOpenHat, .octave=0, .vel=78, .gate=120}, {.step=3, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=4, .tone=kOpenHat, .octave=0, .vel=74, .gate=120}, {.step=7, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=8, .tone=kOpenHat, .octave=0, .vel=78, .gate=120}, {.step=11, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=12, .tone=kOpenHat, .octave=0, .vel=74, .gate=120}, {.step=15, .tone=kClosedHat, .octave=0, .vel=56, .gate=50},
};
inline constexpr StyleEvent kBC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=86, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=86, .gate=kGateStab}, {.step=0, .tone=kFifth, .octave=0, .vel=86, .gate=kGateStab}, {.step=3, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStab}, {.step=3, .tone=kThird, .octave=0, .vel=76, .gate=kGateStab}, {.step=3, .tone=kSeventh, .octave=0, .vel=76, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=86, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=86, .gate=kGateStab}, {.step=8, .tone=kFifth, .octave=0, .vel=86, .gate=kGateStab}, {.step=11, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStab}, {.step=11, .tone=kThird, .octave=0, .vel=76, .gate=kGateStab}, {.step=11, .tone=kSeventh, .octave=0, .vel=76, .gate=kGateStab}};
inline constexpr StylePattern kBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kShufBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBC)}};
inline constexpr StyleEvent kFAD[] = {{.step=0, .tone=kKick, .octave=0, .vel=108, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=100, .gate=100}, {.step=8, .tone=kSnare, .octave=0, .vel=92, .gate=100}, {.step=11, .tone=kSnare, .octave=0, .vel=96, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=104, .gate=100}, {.step=15, .tone=kSnare, .octave=0, .vel=110, .gate=100}};
inline constexpr StyleEvent kFBD[] = {{.step=0, .tone=kKick, .octave=0, .vel=108, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=100, .gate=100}, {.step=7, .tone=kSnare, .octave=0, .vel=94, .gate=100}, {.step=8, .tone=kTomHi, .octave=0, .vel=98, .gate=100}, {.step=11, .tone=kTomMid, .octave=0, .vel=102, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=106, .gate=100}, {.step=15, .tone=kTomFloor, .octave=0, .vel=112, .gate=100}};
inline constexpr StyleEvent kFCD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=96, .gate=100}, {.step=3, .tone=kSnare, .octave=0, .vel=90, .gate=100}, {.step=4, .tone=kTomHi, .octave=0, .vel=100, .gate=100}, {.step=7, .tone=kTomHi, .octave=0, .vel=96, .gate=100}, {.step=8, .tone=kTomMid, .octave=0, .vel=104, .gate=100}, {.step=11, .tone=kTomLow, .octave=0, .vel=108, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=104, .gate=100}, {.step=15, .tone=kTomFloor, .octave=0, .vel=114, .gate=100}};
inline constexpr StyleEvent kFDD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=94, .gate=50}, {.step=2, .tone=kSnare, .octave=0, .vel=88, .gate=50}, {.step=3, .tone=kSnare, .octave=0, .vel=92, .gate=50}, {.step=4, .tone=kTomHi, .octave=0, .vel=102, .gate=50}, {.step=6, .tone=kTomHi, .octave=0, .vel=98, .gate=50}, {.step=7, .tone=kTomMid, .octave=0, .vel=100, .gate=50}, {.step=8, .tone=kTomMid, .octave=0, .vel=106, .gate=50}, {.step=10, .tone=kTomLow, .octave=0, .vel=108, .gate=50}, {.step=11, .tone=kTomLow, .octave=0, .vel=104, .gate=50}, {.step=12, .tone=kTomFloor, .octave=0, .vel=114, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=118, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=120, .gate=kGate8th}};
inline constexpr StylePattern kFAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kShufBass)}};
inline constexpr StylePattern kFBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kShufBass)}};
inline constexpr StylePattern kFCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kShufBass)}};
inline constexpr StylePattern kFDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kShufBass)}};
inline constexpr StyleEvent kE1D[] = {{.step=0, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=0, .tone=kCrash, .octave=0, .vel=104, .gate=kGateHalfBar}};
inline constexpr StyleEvent kE1B[] = {{.step=0, .tone=kRoot, .octave=0, .vel=104, .gate=kGateHeld}};
inline constexpr StyleEvent kE1C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=86, .gate=kGateHeld}};
inline constexpr StylePattern kE1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1C)}};
inline constexpr StyleEvent kE2D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=112, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=104, .gate=120}};
inline constexpr StyleEvent kE2B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=108, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=98, .gate=kGateHeld}};
inline constexpr StyleEvent kE2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=88, .gate=kGateHeld}};
inline constexpr StylePattern kE2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2C)}};
// varC: half-time shuffle — backbeat on beat 3 with ghost notes, bouncy shuffle hats.
inline constexpr StyleEvent kCD[] = {{.step=0, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=108, .gate=120}, {.step=3, .tone=kSnare, .octave=0, .vel=50, .gate=50}, {.step=6, .tone=kSnare, .octave=0, .vel=48, .gate=50}, {.step=11, .tone=kSnare, .octave=0, .vel=52, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=50, .gate=50}, {.step=0, .tone=kClosedHat, .octave=0, .vel=66, .gate=50}, {.step=3, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=62, .gate=50}, {.step=7, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=66, .gate=50}, {.step=11, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=62, .gate=50}, {.step=15, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}};
inline constexpr StyleEvent kCC[] = {{.step=3, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStab}, {.step=3, .tone=kThird, .octave=0, .vel=82, .gate=kGateStab}, {.step=3, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStab}, {.step=11, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=11, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=11, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}};
inline constexpr StylePattern kCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kShufBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCC)}};
// varD: peak Texas shuffle — open-hat shuffle, hard backbeat, kick pushes, full 7th stabs.
inline constexpr StyleEvent kDD[] = {{.step=0, .tone=kKick, .octave=0, .vel=116, .gate=120}, {.step=7, .tone=kKick, .octave=0, .vel=94, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=112, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=112, .gate=120}, {.step=15, .tone=kSnare, .octave=0, .vel=66, .gate=50}, {.step=0, .tone=kOpenHat, .octave=0, .vel=80, .gate=120}, {.step=3, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=4, .tone=kOpenHat, .octave=0, .vel=76, .gate=120}, {.step=7, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=8, .tone=kOpenHat, .octave=0, .vel=80, .gate=120}, {.step=11, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=12, .tone=kOpenHat, .octave=0, .vel=76, .gate=120}, {.step=15, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}};
inline constexpr StyleEvent kDC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=88, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=88, .gate=kGateStab}, {.step=0, .tone=kFifth, .octave=0, .vel=88, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=88, .gate=kGateStab}, {.step=3, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=3, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=3, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=88, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=88, .gate=kGateStab}, {.step=8, .tone=kFifth, .octave=0, .vel=88, .gate=kGateStab}, {.step=8, .tone=kSeventh, .octave=0, .vel=88, .gate=kGateStab}, {.step=11, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=11, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=11, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStab}};
inline constexpr StylePattern kDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kShufBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kDC)}};
inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=1, .patterns=Span<const StylePattern>(kIn1P)}, {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIn2P)},
    {.type=SectionType::kVarA, .bars=1, .patterns=Span<const StylePattern>(kAP)}, {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kBP)},
    {.type=SectionType::kVarC, .bars=1, .patterns=Span<const StylePattern>(kCP)}, {.type=SectionType::kVarD, .bars=1, .patterns=Span<const StylePattern>(kDP)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFAP)}, {.type=SectionType::kFillB, .bars=1, .patterns=Span<const StylePattern>(kFBP)}, {.type=SectionType::kFillC, .bars=1, .patterns=Span<const StylePattern>(kFCP)}, {.type=SectionType::kFillD, .bars=1, .patterns=Span<const StylePattern>(kFDP)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kE1P)}, {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kE2P)},
};
inline constexpr Style kStyle{.name="shuffle", .sections=Span<const StyleSection>(kSections)};
}  // namespace shuffle

// ---------------------------------------------------------------------------
// "latin": salsa/mambo — cowbell on the beats, tumbao congas, timbale accents,
// an anticipated tumbao bass and a syncopated montuno guajeo comping figure.
namespace latin {
inline constexpr StyleEvent kHeldBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=94, .gate=kGateHeld}};
inline constexpr StyleEvent kTumbao[] = {{.step=6, .tone=kFifth, .octave=-1, .vel=98, .gate=kGate8th}, {.step=8, .tone=kSeventh, .octave=-1, .vel=88, .gate=kGateStab}, {.step=12, .tone=kRoot, .octave=0, .vel=100, .gate=kGate8th}, {.step=14, .tone=kFifth, .octave=-1, .vel=90, .gate=kGateStab}, {.step=0, .tone=kRoot, .octave=-1, .vel=86, .gate=kGateStab}};
inline constexpr StyleEvent kIn1D[] = {{.step=8, .tone=kOpenHiConga, .octave=0, .vel=66, .gate=50}, {.step=10, .tone=kLoConga, .octave=0, .vel=62, .gate=50}, {.step=12, .tone=kOpenHiConga, .octave=0, .vel=72, .gate=50}, {.step=14, .tone=kOpenHiConga, .octave=0, .vel=80, .gate=50}};
inline constexpr StylePattern kIn1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)}};
inline constexpr StyleEvent kIn2D[] = {{.step=0, .tone=kCowbell, .octave=0, .vel=78, .gate=50}, {.step=4, .tone=kCowbell, .octave=0, .vel=70, .gate=50}, {.step=8, .tone=kCowbell, .octave=0, .vel=78, .gate=50}, {.step=12, .tone=kCowbell, .octave=0, .vel=70, .gate=50}, {.step=2, .tone=kOpenHiConga, .octave=0, .vel=66, .gate=50}, {.step=6, .tone=kLoConga, .octave=0, .vel=70, .gate=50}, {.step=10, .tone=kOpenHiConga, .octave=0, .vel=66, .gate=50}, {.step=14, .tone=kLoConga, .octave=0, .vel=72, .gate=50}};
inline constexpr StyleEvent kIn2C[] = {{.step=6, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=6, .tone=kThird, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=6, .tone=kFifth, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=12, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=12, .tone=kThird, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=12, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStaccato}};
inline constexpr StylePattern kIn2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kTumbao)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIn2C)}};
inline constexpr StyleEvent kAD[] = {
    {.step=0, .tone=kCowbell, .octave=0, .vel=84, .gate=50}, {.step=4, .tone=kCowbell, .octave=0, .vel=74, .gate=50}, {.step=8, .tone=kCowbell, .octave=0, .vel=84, .gate=50}, {.step=12, .tone=kCowbell, .octave=0, .vel=74, .gate=50},
    {.step=2, .tone=kOpenHiConga, .octave=0, .vel=72, .gate=50}, {.step=3, .tone=kMuteHiConga, .octave=0, .vel=58, .gate=50}, {.step=6, .tone=kLoConga, .octave=0, .vel=76, .gate=50}, {.step=8, .tone=kOpenHiConga, .octave=0, .vel=70, .gate=50}, {.step=10, .tone=kOpenHiConga, .octave=0, .vel=72, .gate=50}, {.step=11, .tone=kMuteHiConga, .octave=0, .vel=58, .gate=50}, {.step=14, .tone=kLoConga, .octave=0, .vel=78, .gate=50},
    {.step=4, .tone=kHiTimbale, .octave=0, .vel=64, .gate=50}, {.step=12, .tone=kLoTimbale, .octave=0, .vel=66, .gate=50},
};
inline constexpr StyleEvent kAC[] = {{.step=2, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=2, .tone=kThird, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=2, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=6, .tone=kThird, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=6, .tone=kFifth, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=6, .tone=kSeventh, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=8, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=8, .tone=kThird, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=8, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=11, .tone=kThird, .octave=0, .vel=74, .gate=kGateStaccato}, {.step=11, .tone=kFifth, .octave=0, .vel=74, .gate=kGateStaccato}, {.step=14, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=14, .tone=kThird, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=14, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStaccato}};
inline constexpr StylePattern kAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kTumbao)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAC)}};
inline constexpr StyleEvent kBD[] = {
    {.step=0, .tone=kCowbell, .octave=0, .vel=88, .gate=50}, {.step=2, .tone=kCowbell, .octave=0, .vel=68, .gate=50}, {.step=4, .tone=kCowbell, .octave=0, .vel=78, .gate=50}, {.step=6, .tone=kCowbell, .octave=0, .vel=68, .gate=50}, {.step=8, .tone=kCowbell, .octave=0, .vel=88, .gate=50}, {.step=10, .tone=kCowbell, .octave=0, .vel=68, .gate=50}, {.step=12, .tone=kCowbell, .octave=0, .vel=78, .gate=50}, {.step=14, .tone=kCowbell, .octave=0, .vel=68, .gate=50},
    {.step=0, .tone=kLoConga, .octave=0, .vel=72, .gate=50}, {.step=2, .tone=kOpenHiConga, .octave=0, .vel=76, .gate=50}, {.step=3, .tone=kOpenHiConga, .octave=0, .vel=70, .gate=50}, {.step=6, .tone=kLoConga, .octave=0, .vel=78, .gate=50}, {.step=8, .tone=kOpenHiConga, .octave=0, .vel=74, .gate=50}, {.step=10, .tone=kOpenHiConga, .octave=0, .vel=76, .gate=50}, {.step=11, .tone=kOpenHiConga, .octave=0, .vel=70, .gate=50}, {.step=14, .tone=kLoConga, .octave=0, .vel=80, .gate=50},
    {.step=4, .tone=kHiTimbale, .octave=0, .vel=70, .gate=50}, {.step=7, .tone=kLoTimbale, .octave=0, .vel=64, .gate=50}, {.step=12, .tone=kHiTimbale, .octave=0, .vel=70, .gate=50}, {.step=15, .tone=kLoTimbale, .octave=0, .vel=66, .gate=50},
};
inline constexpr StyleEvent kBC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=0, .tone=kThird, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=0, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=2, .tone=kThird, .octave=0, .vel=74, .gate=kGateStaccato}, {.step=2, .tone=kFifth, .octave=0, .vel=74, .gate=kGateStaccato}, {.step=4, .tone=kRoot, .octave=1, .vel=76, .gate=kGateStaccato}, {.step=6, .tone=kThird, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=6, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=6, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=8, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=8, .tone=kThird, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=8, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=10, .tone=kThird, .octave=0, .vel=74, .gate=kGateStaccato}, {.step=10, .tone=kFifth, .octave=0, .vel=74, .gate=kGateStaccato}, {.step=12, .tone=kRoot, .octave=1, .vel=76, .gate=kGateStaccato}, {.step=14, .tone=kThird, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=14, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=14, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStaccato}};
inline constexpr StylePattern kBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kTumbao)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBC)}};
inline constexpr StyleEvent kFAD[] = {{.step=0, .tone=kHiTimbale, .octave=0, .vel=88, .gate=50}, {.step=4, .tone=kHiTimbale, .octave=0, .vel=84, .gate=50}, {.step=8, .tone=kLoTimbale, .octave=0, .vel=90, .gate=50}, {.step=10, .tone=kLoTimbale, .octave=0, .vel=94, .gate=50}, {.step=12, .tone=kLoConga, .octave=0, .vel=98, .gate=50}, {.step=14, .tone=kLoConga, .octave=0, .vel=104, .gate=50}};
inline constexpr StyleEvent kFBD[] = {{.step=0, .tone=kHiTimbale, .octave=0, .vel=88, .gate=50}, {.step=2, .tone=kHiTimbale, .octave=0, .vel=84, .gate=50}, {.step=4, .tone=kLoTimbale, .octave=0, .vel=92, .gate=50}, {.step=6, .tone=kLoTimbale, .octave=0, .vel=88, .gate=50}, {.step=8, .tone=kTomHi, .octave=0, .vel=96, .gate=50}, {.step=10, .tone=kTomMid, .octave=0, .vel=100, .gate=50}, {.step=12, .tone=kTomLow, .octave=0, .vel=104, .gate=50}, {.step=14, .tone=kTomFloor, .octave=0, .vel=108, .gate=50}};
inline constexpr StyleEvent kFCD[] = {{.step=0, .tone=kLoConga, .octave=0, .vel=86, .gate=50}, {.step=2, .tone=kOpenHiConga, .octave=0, .vel=90, .gate=50}, {.step=4, .tone=kHiTimbale, .octave=0, .vel=92, .gate=50}, {.step=6, .tone=kHiTimbale, .octave=0, .vel=88, .gate=50}, {.step=8, .tone=kLoTimbale, .octave=0, .vel=96, .gate=50}, {.step=10, .tone=kLoTimbale, .octave=0, .vel=92, .gate=50}, {.step=12, .tone=kTomLow, .octave=0, .vel=104, .gate=50}, {.step=14, .tone=kTomFloor, .octave=0, .vel=110, .gate=50}};
inline constexpr StyleEvent kFDD[] = {{.step=0, .tone=kHiTimbale, .octave=0, .vel=90, .gate=50}, {.step=1, .tone=kHiTimbale, .octave=0, .vel=86, .gate=50}, {.step=2, .tone=kLoTimbale, .octave=0, .vel=94, .gate=50}, {.step=3, .tone=kLoTimbale, .octave=0, .vel=90, .gate=50}, {.step=4, .tone=kTomHi, .octave=0, .vel=98, .gate=50}, {.step=5, .tone=kTomHi, .octave=0, .vel=94, .gate=50}, {.step=6, .tone=kTomMid, .octave=0, .vel=100, .gate=50}, {.step=7, .tone=kTomMid, .octave=0, .vel=96, .gate=50}, {.step=8, .tone=kTomLow, .octave=0, .vel=104, .gate=50}, {.step=9, .tone=kTomLow, .octave=0, .vel=100, .gate=50}, {.step=10, .tone=kTomFloor, .octave=0, .vel=108, .gate=50}, {.step=11, .tone=kTomFloor, .octave=0, .vel=104, .gate=50}, {.step=12, .tone=kCowbell, .octave=0, .vel=110, .gate=50}, {.step=13, .tone=kCowbell, .octave=0, .vel=106, .gate=50}, {.step=14, .tone=kCowbell, .octave=0, .vel=114, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=120, .gate=kGate8th}};
inline constexpr StylePattern kFAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kTumbao)}};
inline constexpr StylePattern kFBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kTumbao)}};
inline constexpr StylePattern kFCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kTumbao)}};
inline constexpr StylePattern kFDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kTumbao)}};
inline constexpr StyleEvent kE1D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=104, .gate=kGateHalfBar}, {.step=0, .tone=kCowbell, .octave=0, .vel=88, .gate=50}, {.step=8, .tone=kLoConga, .octave=0, .vel=84, .gate=50}};
inline constexpr StyleEvent kE1B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=100, .gate=kGateHeld}};
inline constexpr StyleEvent kE1C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=84, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=84, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=84, .gate=kGateHeld}};
inline constexpr StylePattern kE1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1C)}};
inline constexpr StyleEvent kE2D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=112, .gate=kGateHeld}, {.step=0, .tone=kCowbell, .octave=0, .vel=90, .gate=50}, {.step=8, .tone=kLoConga, .octave=0, .vel=86, .gate=50}, {.step=12, .tone=kHiTimbale, .octave=0, .vel=94, .gate=50}};
inline constexpr StyleEvent kE2B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=104, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=94, .gate=kGateHeld}};
inline constexpr StyleEvent kE2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=86, .gate=kGateHeld}};
inline constexpr StylePattern kE2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2C)}};
// varC: cha-cha — lighter cowbell on the beats, simple conga slaps, sparser montuno.
inline constexpr StyleEvent kCD[] = {{.step=0, .tone=kCowbell, .octave=0, .vel=78, .gate=50}, {.step=4, .tone=kCowbell, .octave=0, .vel=70, .gate=50}, {.step=8, .tone=kCowbell, .octave=0, .vel=78, .gate=50}, {.step=12, .tone=kCowbell, .octave=0, .vel=70, .gate=50}, {.step=2, .tone=kOpenHiConga, .octave=0, .vel=66, .gate=50}, {.step=6, .tone=kLoConga, .octave=0, .vel=70, .gate=50}, {.step=10, .tone=kOpenHiConga, .octave=0, .vel=66, .gate=50}, {.step=14, .tone=kLoConga, .octave=0, .vel=72, .gate=50}, {.step=6, .tone=kHiTimbale, .octave=0, .vel=62, .gate=50}, {.step=14, .tone=kLoTimbale, .octave=0, .vel=64, .gate=50}};
inline constexpr StyleEvent kCC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=0, .tone=kThird, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=0, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=6, .tone=kThird, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=6, .tone=kFifth, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=6, .tone=kSeventh, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=10, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=10, .tone=kThird, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=10, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStaccato}};
inline constexpr StylePattern kCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kTumbao)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCC)}};
// varD: mambo peak — cowbell eighths, busy congas and timbales, full 7th montuno.
inline constexpr StyleEvent kDD[] = {{.step=0, .tone=kCowbell, .octave=0, .vel=88, .gate=50}, {.step=2, .tone=kCowbell, .octave=0, .vel=68, .gate=50}, {.step=4, .tone=kCowbell, .octave=0, .vel=78, .gate=50}, {.step=6, .tone=kCowbell, .octave=0, .vel=68, .gate=50}, {.step=8, .tone=kCowbell, .octave=0, .vel=88, .gate=50}, {.step=10, .tone=kCowbell, .octave=0, .vel=68, .gate=50}, {.step=12, .tone=kCowbell, .octave=0, .vel=78, .gate=50}, {.step=14, .tone=kCowbell, .octave=0, .vel=68, .gate=50}, {.step=0, .tone=kLoConga, .octave=0, .vel=72, .gate=50}, {.step=2, .tone=kOpenHiConga, .octave=0, .vel=76, .gate=50}, {.step=3, .tone=kOpenHiConga, .octave=0, .vel=70, .gate=50}, {.step=6, .tone=kLoConga, .octave=0, .vel=78, .gate=50}, {.step=8, .tone=kOpenHiConga, .octave=0, .vel=74, .gate=50}, {.step=10, .tone=kOpenHiConga, .octave=0, .vel=76, .gate=50}, {.step=11, .tone=kOpenHiConga, .octave=0, .vel=70, .gate=50}, {.step=14, .tone=kLoConga, .octave=0, .vel=80, .gate=50}, {.step=4, .tone=kHiTimbale, .octave=0, .vel=70, .gate=50}, {.step=7, .tone=kLoTimbale, .octave=0, .vel=64, .gate=50}, {.step=12, .tone=kHiTimbale, .octave=0, .vel=70, .gate=50}, {.step=15, .tone=kLoTimbale, .octave=0, .vel=66, .gate=50}};
inline constexpr StyleEvent kDC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStaccato}, {.step=0, .tone=kThird, .octave=0, .vel=84, .gate=kGateStaccato}, {.step=0, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStaccato}, {.step=0, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStaccato}, {.step=2, .tone=kThird, .octave=0, .vel=74, .gate=kGateStaccato}, {.step=2, .tone=kFifth, .octave=0, .vel=74, .gate=kGateStaccato}, {.step=6, .tone=kThird, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=6, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=6, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=8, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStaccato}, {.step=8, .tone=kThird, .octave=0, .vel=84, .gate=kGateStaccato}, {.step=8, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStaccato}, {.step=8, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStaccato}, {.step=10, .tone=kThird, .octave=0, .vel=74, .gate=kGateStaccato}, {.step=10, .tone=kFifth, .octave=0, .vel=74, .gate=kGateStaccato}, {.step=14, .tone=kThird, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=14, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=14, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStaccato}};
inline constexpr StylePattern kDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kTumbao)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kDC)}};
inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=1, .patterns=Span<const StylePattern>(kIn1P)}, {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIn2P)},
    {.type=SectionType::kVarA, .bars=1, .patterns=Span<const StylePattern>(kAP)}, {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kBP)},
    {.type=SectionType::kVarC, .bars=1, .patterns=Span<const StylePattern>(kCP)}, {.type=SectionType::kVarD, .bars=1, .patterns=Span<const StylePattern>(kDP)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFAP)}, {.type=SectionType::kFillB, .bars=1, .patterns=Span<const StylePattern>(kFBP)}, {.type=SectionType::kFillC, .bars=1, .patterns=Span<const StylePattern>(kFCP)}, {.type=SectionType::kFillD, .bars=1, .patterns=Span<const StylePattern>(kFDP)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kE1P)}, {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kE2P)},
};
inline constexpr Style kStyle{.name="latin", .sections=Span<const StyleSection>(kSections)};
}  // namespace latin

// ---------------------------------------------------------------------------
// "motown": soul backbeat — kick 1 & 3, cracking snare 2 & 4, tambourine on the
// eighths, a melodic eighth-note soul bass line and bright off-beat triad stabs.
namespace motown {
inline constexpr StyleEvent kHeldBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=94, .gate=kGateHeld}};
inline constexpr StyleEvent kSoulBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=100, .gate=kGate8th}, {.step=4, .tone=kThird, .octave=0, .vel=88, .gate=kGate8th}, {.step=6, .tone=kFifth, .octave=0, .vel=84, .gate=kGate8th}, {.step=8, .tone=kFifth, .octave=0, .vel=94, .gate=kGate8th}, {.step=10, .tone=kRoot, .octave=1, .vel=86, .gate=kGate8th}, {.step=12, .tone=kThird, .octave=0, .vel=90, .gate=kGate8th}, {.step=14, .tone=kSeventh, .octave=0, .vel=84, .gate=kGate8th}};
inline constexpr StyleEvent kIn1D[] = {{.step=8, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=10, .tone=kTambourine, .octave=0, .vel=64, .gate=50}, {.step=12, .tone=kSnare, .octave=0, .vel=80, .gate=100}, {.step=14, .tone=kSnare, .octave=0, .vel=92, .gate=100}};
inline constexpr StylePattern kIn1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)}};
inline constexpr StyleEvent kIn2D[] = {{.step=0, .tone=kKick, .octave=0, .vel=100, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=96, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=98, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=98, .gate=120}, {.step=0, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=2, .tone=kTambourine, .octave=0, .vel=54, .gate=50}, {.step=4, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=6, .tone=kTambourine, .octave=0, .vel=54, .gate=50}, {.step=8, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=10, .tone=kTambourine, .octave=0, .vel=54, .gate=50}, {.step=12, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=14, .tone=kTambourine, .octave=0, .vel=54, .gate=50}};
inline constexpr StyleEvent kIn2C[] = {{.step=2, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=76, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=76, .gate=kGateStab}};
inline constexpr StylePattern kIn2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSoulBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIn2C)}};
inline constexpr StyleEvent kAD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=106, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=102, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=108, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=108, .gate=120},
    {.step=0, .tone=kTambourine, .octave=0, .vel=64, .gate=50}, {.step=2, .tone=kTambourine, .octave=0, .vel=56, .gate=50}, {.step=4, .tone=kTambourine, .octave=0, .vel=64, .gate=50}, {.step=6, .tone=kTambourine, .octave=0, .vel=56, .gate=50}, {.step=8, .tone=kTambourine, .octave=0, .vel=64, .gate=50}, {.step=10, .tone=kTambourine, .octave=0, .vel=56, .gate=50}, {.step=12, .tone=kTambourine, .octave=0, .vel=64, .gate=50}, {.step=14, .tone=kTambourine, .octave=0, .vel=56, .gate=50},
};
inline constexpr StyleEvent kAC[] = {{.step=2, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=82, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStab}, {.step=6, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=82, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStab}, {.step=14, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStab}};
inline constexpr StylePattern kAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSoulBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAC)}};
inline constexpr StyleEvent kBD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=108, .gate=120}, {.step=6, .tone=kKick, .octave=0, .vel=90, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=110, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=110, .gate=120}, {.step=10, .tone=kSnare, .octave=0, .vel=58, .gate=50},
    {.step=0, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=1, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=2, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=3, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=4, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=5, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=6, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=7, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=8, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=9, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=10, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=11, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=12, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=13, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=14, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=15, .tone=kTambourine, .octave=0, .vel=52, .gate=50},
};
inline constexpr StyleEvent kBC[] = {{.step=2, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}, {.step=6, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=8, .tone=kFifth, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=10, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}, {.step=14, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}};
inline constexpr StylePattern kBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSoulBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBC)}};
inline constexpr StyleEvent kFAD[] = {{.step=0, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=100, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=92, .gate=120}, {.step=10, .tone=kSnare, .octave=0, .vel=96, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=104, .gate=120}, {.step=14, .tone=kSnare, .octave=0, .vel=110, .gate=120}};
inline constexpr StyleEvent kFBD[] = {{.step=0, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=98, .gate=100}, {.step=6, .tone=kSnare, .octave=0, .vel=92, .gate=100}, {.step=8, .tone=kTomHi, .octave=0, .vel=96, .gate=100}, {.step=10, .tone=kTomMid, .octave=0, .vel=100, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=104, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=110, .gate=100}};
inline constexpr StyleEvent kFCD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=94, .gate=100}, {.step=2, .tone=kSnare, .octave=0, .vel=88, .gate=100}, {.step=4, .tone=kTomHi, .octave=0, .vel=98, .gate=100}, {.step=6, .tone=kTomHi, .octave=0, .vel=94, .gate=100}, {.step=8, .tone=kTomMid, .octave=0, .vel=102, .gate=100}, {.step=10, .tone=kTomMid, .octave=0, .vel=98, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=108, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=114, .gate=100}};
inline constexpr StyleEvent kFDD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=92, .gate=50}, {.step=1, .tone=kSnare, .octave=0, .vel=86, .gate=50}, {.step=2, .tone=kSnare, .octave=0, .vel=96, .gate=50}, {.step=3, .tone=kSnare, .octave=0, .vel=90, .gate=50}, {.step=4, .tone=kTomHi, .octave=0, .vel=100, .gate=50}, {.step=6, .tone=kTomHi, .octave=0, .vel=96, .gate=50}, {.step=8, .tone=kTomMid, .octave=0, .vel=104, .gate=50}, {.step=10, .tone=kTomLow, .octave=0, .vel=108, .gate=50}, {.step=12, .tone=kTomFloor, .octave=0, .vel=112, .gate=50}, {.step=13, .tone=kSnare, .octave=0, .vel=114, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=118, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=120, .gate=kGate8th}};
inline constexpr StylePattern kFAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSoulBass)}};
inline constexpr StylePattern kFBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSoulBass)}};
inline constexpr StylePattern kFCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSoulBass)}};
inline constexpr StylePattern kFDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSoulBass)}};
inline constexpr StyleEvent kE1D[] = {{.step=0, .tone=kKick, .octave=0, .vel=106, .gate=120}, {.step=0, .tone=kCrash, .octave=0, .vel=100, .gate=kGateHalfBar}};
inline constexpr StyleEvent kE1B[] = {{.step=0, .tone=kRoot, .octave=0, .vel=98, .gate=kGateHeld}};
inline constexpr StyleEvent kE1C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=84, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=84, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=84, .gate=kGateHeld}};
inline constexpr StylePattern kE1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1C)}};
inline constexpr StyleEvent kE2D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=110, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=108, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=100, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=104, .gate=120}};
inline constexpr StyleEvent kE2B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=104, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=94, .gate=kGateHeld}};
inline constexpr StyleEvent kE2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=86, .gate=kGateHeld}};
inline constexpr StylePattern kE2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2C)}};
// varC: half-time soul — backbeat on beat 3, tambourine eighths, sustained triad.
inline constexpr StyleEvent kCD[] = {{.step=0, .tone=kKick, .octave=0, .vel=106, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=108, .gate=120}, {.step=0, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=2, .tone=kTambourine, .octave=0, .vel=54, .gate=50}, {.step=4, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=6, .tone=kTambourine, .octave=0, .vel=54, .gate=50}, {.step=8, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=10, .tone=kTambourine, .octave=0, .vel=54, .gate=50}, {.step=12, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=14, .tone=kTambourine, .octave=0, .vel=54, .gate=50}};
inline constexpr StyleEvent kCC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=78, .gate=kGateBeat}, {.step=0, .tone=kThird, .octave=0, .vel=78, .gate=kGateBeat}, {.step=0, .tone=kFifth, .octave=0, .vel=78, .gate=kGateBeat}, {.step=8, .tone=kRoot, .octave=0, .vel=76, .gate=kGateBeat}, {.step=8, .tone=kThird, .octave=0, .vel=76, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=76, .gate=kGateBeat}};
inline constexpr StylePattern kCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSoulBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCC)}};
// varD: peak soul — kick pushes, cracking snare with ghost, 16th tambourine, full 7th stabs.
inline constexpr StyleEvent kDD[] = {{.step=0, .tone=kKick, .octave=0, .vel=108, .gate=120}, {.step=6, .tone=kKick, .octave=0, .vel=90, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=10, .tone=kKick, .octave=0, .vel=88, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=112, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=112, .gate=120}, {.step=10, .tone=kSnare, .octave=0, .vel=58, .gate=50}, {.step=0, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=1, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=2, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=3, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=4, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=5, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=6, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=7, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=8, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=9, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=10, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=11, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=12, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=13, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=14, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=15, .tone=kTambourine, .octave=0, .vel=52, .gate=50}};
inline constexpr StyleEvent kDC[] = {{.step=2, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}, {.step=6, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}, {.step=14, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}};
inline constexpr StylePattern kDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSoulBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kDC)}};
inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=1, .patterns=Span<const StylePattern>(kIn1P)}, {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIn2P)},
    {.type=SectionType::kVarA, .bars=1, .patterns=Span<const StylePattern>(kAP)}, {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kBP)},
    {.type=SectionType::kVarC, .bars=1, .patterns=Span<const StylePattern>(kCP)}, {.type=SectionType::kVarD, .bars=1, .patterns=Span<const StylePattern>(kDP)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFAP)}, {.type=SectionType::kFillB, .bars=1, .patterns=Span<const StylePattern>(kFBP)}, {.type=SectionType::kFillC, .bars=1, .patterns=Span<const StylePattern>(kFCP)}, {.type=SectionType::kFillD, .bars=1, .patterns=Span<const StylePattern>(kFDP)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kE1P)}, {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kE2P)},
};
inline constexpr Style kStyle{.name="motown", .sections=Span<const StyleSection>(kSections)};
}  // namespace motown

inline constexpr const Style* kBuiltins[] = {
    &basic::kStyle,  &pop::kStyle,    &rock::kStyle,    &ballad::kStyle,
    &funk::kStyle,   &disco::kStyle,  &house::kStyle,   &swing::kStyle,
    &bossa::kStyle,  &samba::kStyle,  &reggae::kStyle,  &country::kStyle,
    &blues::kStyle,  &shuffle::kStyle, &latin::kStyle,  &motown::kStyle,
};
inline constexpr std::uint8_t kBuiltinCount = 16;

}  // namespace styles
}  // namespace arrangrr
