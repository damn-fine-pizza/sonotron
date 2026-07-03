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
    {0, 36, 0, 110, 120}, {4, 38, 0, 100, 120}, {8, 36, 0, 105, 120}, {12, 38, 0, 100, 120},
    {0, 42, 0, 70, 60},   {2, 42, 0, 60, 60},   {4, 42, 0, 70, 60},   {6, 42, 0, 60, 60},
    {8, 42, 0, 70, 60},   {10, 42, 0, 60, 60},  {12, 42, 0, 70, 60},  {14, 42, 0, 60, 60},
};
inline constexpr StyleEvent kVarABass[] = {
    {0, 0, 0, 100, 220},  // root
    {4, 2, 0, 85, 220},   // fifth
    {8, 0, 0, 95, 220},
    {12, 2, 0, 85, 220},
};
inline constexpr StyleEvent kVarAChord[] = {
    // Comp stabs on beats 1 and 3: full stack (tones 0..3).
    {0, 0, 0, 80, 360}, {0, 1, 0, 80, 360}, {0, 2, 0, 80, 360}, {0, 3, 0, 80, 360},
    {8, 0, 0, 75, 360}, {8, 1, 0, 75, 360}, {8, 2, 0, 75, 360}, {8, 3, 0, 75, 360},
};
inline constexpr StylePattern kVarAPatterns[] = {
    {TrackRole::kDrums, RolePolicy::kFixed, Span<const StyleEvent>(kVarADrums)},
    {TrackRole::kBass, RolePolicy::kChordTone, Span<const StyleEvent>(kVarABass)},
    {TrackRole::kChord1, RolePolicy::kChordTone, Span<const StyleEvent>(kVarAChord)},
};

inline constexpr StyleEvent kVarBDrums[] = {
    {0, 36, 0, 115, 120},  {4, 38, 0, 105, 120}, {7, 36, 0, 90, 120}, {8, 36, 0, 110, 120},
    {12, 38, 0, 105, 120}, {0, 42, 0, 75, 50},   {1, 42, 0, 55, 50},  {2, 42, 0, 65, 50},
    {3, 42, 0, 55, 50},    {4, 42, 0, 75, 50},   {5, 42, 0, 55, 50},  {6, 42, 0, 65, 50},
    {7, 42, 0, 55, 50},    {8, 42, 0, 75, 50},   {9, 42, 0, 55, 50},  {10, 42, 0, 65, 50},
    {11, 42, 0, 55, 50},   {12, 42, 0, 75, 50},  {13, 42, 0, 55, 50}, {14, 42, 0, 65, 50},
    {15, 42, 0, 55, 50},
};
inline constexpr StyleEvent kVarBBass[] = {
    {0, 0, 0, 105, 200}, {2, 0, 0, 70, 100},  {4, 1, 0, 90, 200},
    {8, 2, 0, 95, 200},  {10, 0, 1, 75, 100}, {12, 3, 0, 90, 200},
};
inline constexpr StyleEvent kVarBChord[] = {
    {2, 0, 0, 78, 200},  {2, 1, 0, 78, 200},  {2, 2, 0, 78, 200},  {6, 0, 0, 72, 200},
    {6, 1, 0, 72, 200},  {6, 2, 0, 72, 200},  {10, 0, 0, 78, 200}, {10, 1, 0, 78, 200},
    {10, 2, 0, 78, 200}, {14, 0, 0, 72, 200}, {14, 1, 0, 72, 200}, {14, 2, 0, 72, 200},
};
inline constexpr StylePattern kVarBPatterns[] = {
    {TrackRole::kDrums, RolePolicy::kFixed, Span<const StyleEvent>(kVarBDrums)},
    {TrackRole::kBass, RolePolicy::kChordTone, Span<const StyleEvent>(kVarBBass)},
    {TrackRole::kChord1, RolePolicy::kChordTone, Span<const StyleEvent>(kVarBChord)},
};

inline constexpr StyleEvent kIntroDrums[] = {
    {8, 42, 0, 60, 60},
    {10, 42, 0, 65, 60},
    {12, 42, 0, 70, 60},
    {14, 42, 0, 80, 60},
};
inline constexpr StyleEvent kIntroBass[] = {
    {0, 0, 0, 90, 3600},  // held root pickup
};
inline constexpr StylePattern kIntroPatterns[] = {
    {TrackRole::kDrums, RolePolicy::kFixed, Span<const StyleEvent>(kIntroDrums)},
    {TrackRole::kBass, RolePolicy::kChordTone, Span<const StyleEvent>(kIntroBass)},
};

inline constexpr StyleEvent kFillDrums[] = {
    {0, 36, 0, 110, 120}, {4, 38, 0, 100, 120},  {8, 38, 0, 90, 100},
    {10, 38, 0, 95, 100}, {12, 38, 0, 105, 100}, {14, 38, 0, 115, 100},
};
inline constexpr StyleEvent kFillBass[] = {
    {0, 0, 0, 100, 220},
    {8, 2, 0, 90, 220},
};
inline constexpr StylePattern kFillPatterns[] = {
    {TrackRole::kDrums, RolePolicy::kFixed, Span<const StyleEvent>(kFillDrums)},
    {TrackRole::kBass, RolePolicy::kChordTone, Span<const StyleEvent>(kFillBass)},
};

inline constexpr StyleEvent kEndDrums[] = {
    {0, 36, 0, 115, 120},
    {0, 49, 0, 110, 1800},
};
inline constexpr StyleEvent kEndBass[] = {
    {0, 0, 0, 100, 3600},
};
inline constexpr StyleEvent kEndChord[] = {
    {0, 0, 0, 85, 3600},
    {0, 1, 0, 85, 3600},
    {0, 2, 0, 85, 3600},
    {0, 3, 0, 85, 3600},
};
inline constexpr StylePattern kEndPatterns[] = {
    {TrackRole::kDrums, RolePolicy::kFixed, Span<const StyleEvent>(kEndDrums)},
    {TrackRole::kBass, RolePolicy::kChordTone, Span<const StyleEvent>(kEndBass)},
    {TrackRole::kChord1, RolePolicy::kChordTone, Span<const StyleEvent>(kEndChord)},
};

inline constexpr StyleSection kSections[] = {
    {SectionType::kIntro1, 1, Span<const StylePattern>(kIntroPatterns)},
    {SectionType::kVarA, 1, Span<const StylePattern>(kVarAPatterns)},
    {SectionType::kVarB, 1, Span<const StylePattern>(kVarBPatterns)},
    {SectionType::kFillA, 1, Span<const StylePattern>(kFillPatterns)},
    {SectionType::kEnding1, 1, Span<const StylePattern>(kEndPatterns)},
};

inline constexpr Style kStyle{"basic", Span<const StyleSection>(kSections)};

}  // namespace basic

inline constexpr const Style* kBuiltins[] = {&basic::kStyle};
inline constexpr std::uint8_t kBuiltinCount = 1;

}  // namespace styles
}  // namespace arrangrr
