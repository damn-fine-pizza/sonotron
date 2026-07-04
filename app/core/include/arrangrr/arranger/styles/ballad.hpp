#pragma once

#include "arrangrr/arranger/style_model.hpp"

namespace arrangrr {
namespace styles {


// ---------------------------------------------------------------------------
// "ballad": slow, sparse, gentle. Soft kick with a rimshot backbeat and an
// occasional ride, the chord spread as a rising arpeggio (tones 0..3 across the
// bar) instead of a block stab, a soft legato root bass. Low velocity, long
// gates.
namespace ballad {

// ---------------------------------------------------------------------------
// Fuller-band roles (ADDITIVE): a soft strings pad under the whole bar, a
// gentle E.Piano second comp, a harp arpeggio for sparkle and a whisper of
// percussion. Everything stays low-velocity, long-gated — the ballad must not
// get busy. kPad -> GM 48 (Strings), kChord2 -> GM 4 (E.Piano1), kArp -> GM 46
// (Harp); kPerc rides the drum channel.
inline constexpr std::int16_t kPadVoice = 48;
inline constexpr std::int16_t kChord2Voice = 4;
inline constexpr std::int16_t kArpVoice = 46;
inline constexpr StyleEvent kPadTriad[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=50, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=48, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=50, .gate=kGateHeld},
};
inline constexpr StyleEvent kPad7[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=52, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=50, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=52, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=46, .gate=kGateHeld},
};
// E.Piano: a soft block on beat 1, or beats 1 & 3 in the fuller variations.
inline constexpr StyleEvent kChord2Beat1[] = {
    {.step=0, .tone=kThird, .octave=0, .vel=52, .gate=kGateHalfBar}, {.step=0, .tone=kFifth, .octave=0, .vel=52, .gate=kGateHalfBar},
};
inline constexpr StyleEvent kChord2Beat13[] = {
    {.step=0, .tone=kThird, .octave=0, .vel=54, .gate=kGateBeat}, {.step=0, .tone=kFifth, .octave=0, .vel=54, .gate=kGateBeat},
    {.step=8, .tone=kThird, .octave=0, .vel=50, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=50, .gate=kGateBeat},
};
// Harp: a gentle rising arpeggio, high register.
inline constexpr StyleEvent kArpHarp[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=58, .gate=kGate8th}, {.step=2, .tone=kThird, .octave=0, .vel=52, .gate=kGate8th}, {.step=4, .tone=kFifth, .octave=0, .vel=56, .gate=kGate8th}, {.step=6, .tone=kSeventh, .octave=0, .vel=52, .gate=kGate8th},
    {.step=8, .tone=kRoot, .octave=1, .vel=58, .gate=kGate8th}, {.step=10, .tone=kSeventh, .octave=0, .vel=52, .gate=kGate8th}, {.step=12, .tone=kFifth, .octave=0, .vel=56, .gate=kGate8th}, {.step=14, .tone=kThird, .octave=0, .vel=50, .gate=kGate8th},
};
// The faintest shaker/tambourine — only where the fuller sections can carry it.
inline constexpr StyleEvent kPercSoft[] = {
    {.step=4, .tone=kTambourine, .octave=0, .vel=54, .gate=kGateHat}, {.step=12, .tone=kTambourine, .octave=0, .vel=56, .gate=kGateHat},
};
inline constexpr StyleEvent kPercShake[] = {
    {.step=2, .tone=kMaracas, .octave=0, .vel=46, .gate=kGateHat}, {.step=6, .tone=kMaracas, .octave=0, .vel=46, .gate=kGateHat}, {.step=10, .tone=kMaracas, .octave=0, .vel=46, .gate=kGateHat}, {.step=14, .tone=kMaracas, .octave=0, .vel=46, .gate=kGateHat},
};

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
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice},
    {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kChord2Beat1), .gm_program=kChord2Voice},
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
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice},
    {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kChord2Beat13), .gm_program=kChord2Voice},
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
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice},
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
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice},
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
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice},
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
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice},
};

// varC: flowing 6/8-lilt broken arpeggio — a rolling feel vs A/B's long sustains.
inline constexpr StyleEvent kVarCDrums[] = {{.step=0, .tone=kKick, .octave=0, .vel=72, .gate=kGateHat}, {.step=8, .tone=kKick, .octave=0, .vel=66, .gate=kGateHat}, {.step=4, .tone=kRimshot, .octave=0, .vel=60, .gate=kGateHat}, {.step=12, .tone=kRimshot, .octave=0, .vel=62, .gate=kGateHat}, {.step=0, .tone=kRide, .octave=0, .vel=58, .gate=kGate8th}, {.step=2, .tone=kRide, .octave=0, .vel=50, .gate=kGate8th}, {.step=4, .tone=kRide, .octave=0, .vel=56, .gate=kGate8th}, {.step=6, .tone=kRide, .octave=0, .vel=50, .gate=kGate8th}, {.step=8, .tone=kRide, .octave=0, .vel=58, .gate=kGate8th}, {.step=10, .tone=kRide, .octave=0, .vel=50, .gate=kGate8th}, {.step=12, .tone=kRide, .octave=0, .vel=56, .gate=kGate8th}, {.step=14, .tone=kRide, .octave=0, .vel=50, .gate=kGate8th}};
inline constexpr StyleEvent kVarCBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=74, .gate=kGateHalfBar}, {.step=8, .tone=kFifth, .octave=-1, .vel=70, .gate=kGateHalfBar}};
inline constexpr StyleEvent kVarCChord[] = {{.step=0, .tone=kRoot, .octave=0, .vel=66, .gate=kGate8th}, {.step=2, .tone=kThird, .octave=0, .vel=62, .gate=kGate8th}, {.step=4, .tone=kFifth, .octave=0, .vel=66, .gate=kGate8th}, {.step=6, .tone=kSeventh, .octave=0, .vel=62, .gate=kGate8th}, {.step=8, .tone=kRoot, .octave=1, .vel=66, .gate=kGate8th}, {.step=10, .tone=kThird, .octave=1, .vel=62, .gate=kGate8th}, {.step=12, .tone=kFifth, .octave=0, .vel=64, .gate=kGate8th}, {.step=14, .tone=kSeventh, .octave=0, .vel=60, .gate=kGate8th}};
inline constexpr StylePattern kVarCPatterns[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarCDrums)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarCBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarCChord)},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice},
    {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kChord2Beat13), .gm_program=kChord2Voice},
    {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArpHarp), .gm_program=kArpVoice},
    {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercShake)}};
// varD: fullest — snare backbeat, ride eighths, gentle walking bass, full spread 7th chord.
inline constexpr StyleEvent kVarDDrums[] = {{.step=0, .tone=kKick, .octave=0, .vel=80, .gate=kGateHat}, {.step=8, .tone=kKick, .octave=0, .vel=76, .gate=kGateHat}, {.step=4, .tone=kSnare, .octave=0, .vel=84, .gate=kGateHat}, {.step=12, .tone=kSnare, .octave=0, .vel=86, .gate=kGateHat}, {.step=0, .tone=kRide, .octave=0, .vel=64, .gate=kGate8th}, {.step=2, .tone=kRide, .octave=0, .vel=56, .gate=kGate8th}, {.step=4, .tone=kRide, .octave=0, .vel=64, .gate=kGate8th}, {.step=6, .tone=kRide, .octave=0, .vel=56, .gate=kGate8th}, {.step=8, .tone=kRide, .octave=0, .vel=64, .gate=kGate8th}, {.step=10, .tone=kRide, .octave=0, .vel=56, .gate=kGate8th}, {.step=12, .tone=kRide, .octave=0, .vel=64, .gate=kGate8th}, {.step=14, .tone=kRide, .octave=0, .vel=56, .gate=kGate8th}};
inline constexpr StyleEvent kVarDBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=80, .gate=kGateBeat}, {.step=4, .tone=kThird, .octave=0, .vel=74, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=78, .gate=kGateBeat}, {.step=12, .tone=kSeventh, .octave=0, .vel=74, .gate=kGateBeat}};
inline constexpr StyleEvent kVarDChord[] = {{.step=0, .tone=kRoot, .octave=0, .vel=74, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=72, .gate=kGateHeld}, {.step=4, .tone=kFifth, .octave=0, .vel=72, .gate=kGateHalfBar}, {.step=6, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateHalfBar}, {.step=8, .tone=kThird, .octave=1, .vel=70, .gate=kGateHalfBar}, {.step=8, .tone=kFifth, .octave=0, .vel=68, .gate=kGateHalfBar}, {.step=12, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateHalfBar}, {.step=12, .tone=kRoot, .octave=1, .vel=64, .gate=kGateHalfBar}};
inline constexpr StylePattern kVarDPatterns[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarDDrums)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarDBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarDChord)},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice},
    {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kChord2Beat13), .gm_program=kChord2Voice},
    {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArpHarp), .gm_program=kArpVoice},
    {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercSoft)}};

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

}  // namespace styles
}  // namespace arrangrr
