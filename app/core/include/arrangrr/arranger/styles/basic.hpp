#pragma once

#include "arrangrr/arranger/style_model.hpp"

namespace arrangrr {
namespace styles {

// ---------------------------------------------------------------------------
// Built-in demo style "basic": 4/4, one-bar sections, three roles.
// GM drums on the fixed role: kick 36, snare 38, closed hat 42, crash 49.
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

}  // namespace styles
}  // namespace arrangrr
