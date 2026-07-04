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

inline constexpr const Style* kBuiltins[] = {&basic::kStyle};
inline constexpr std::uint8_t kBuiltinCount = 1;

}  // namespace styles
}  // namespace arrangrr
