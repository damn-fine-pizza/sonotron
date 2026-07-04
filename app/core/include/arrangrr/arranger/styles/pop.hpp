#pragma once

#include "arrangrr/arranger/style_model.hpp"

namespace arrangrr {
namespace styles {


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

}  // namespace styles
}  // namespace arrangrr
