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

inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=1, .patterns=Span<const StylePattern>(kIntroPatterns)},
    {.type=SectionType::kVarA, .bars=1, .patterns=Span<const StylePattern>(kVarAPatterns)},
    {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kVarBPatterns)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFillPatterns)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kEndPatterns)},
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

inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=1, .patterns=Span<const StylePattern>(kIntroPatterns)},
    {.type=SectionType::kVarA, .bars=1, .patterns=Span<const StylePattern>(kVarAPatterns)},
    {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kVarBPatterns)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFillPatterns)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kEndPatterns)},
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

inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=1, .patterns=Span<const StylePattern>(kIntroPatterns)},
    {.type=SectionType::kVarA, .bars=1, .patterns=Span<const StylePattern>(kVarAPatterns)},
    {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kVarBPatterns)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFillPatterns)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kEndPatterns)},
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

inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=1, .patterns=Span<const StylePattern>(kIntroPatterns)},
    {.type=SectionType::kVarA, .bars=1, .patterns=Span<const StylePattern>(kVarAPatterns)},
    {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kVarBPatterns)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFillPatterns)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kEndPatterns)},
};
inline constexpr Style kStyle{.name="ballad", .sections=Span<const StyleSection>(kSections)};

}  // namespace ballad

inline constexpr const Style* kBuiltins[] = {&basic::kStyle, &pop::kStyle, &rock::kStyle,
                                             &ballad::kStyle};
inline constexpr std::uint8_t kBuiltinCount = 4;

}  // namespace styles
}  // namespace arrangrr
