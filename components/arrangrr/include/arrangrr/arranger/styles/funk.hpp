#pragma once

#include "arrangrr/arranger/style_model.hpp"

namespace arrangrr {
namespace styles {


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
// Motif engine (9210, Ottorino RANK 14): the flat FILL bass, reused
// identically across all 4 fills (kAB/kBB already alternate A/C vs B/D).
// Funk's own genre-signature stab line is a MODEL GAP (needs Option 2
// generation, a separate later batch), not mechanical wiring.
inline constexpr MotifSpec kFillBassMotif{.transform = MotifTransform::kRetrograde, .seed = 1401};

// Fuller-band roles (ADDITIVE): a sustained pad bed, a complementary organ
// skank, a square-lead arp and hand percussion. kPad->GM89 (Pad2 warm),
// kChord2->GM16 (Drawbar Organ), kArp->GM80 (Square Lead); kPerc rides the drum
// channel (gm_program stays -1). Pad holds register 48 under the whole bar.
inline constexpr std::int16_t kPadVoice = 89;
inline constexpr std::int16_t kChord2Voice = 16;
inline constexpr std::int16_t kArpVoice = 80;
inline constexpr StyleEvent kPadT[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=54, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=52, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=54, .gate=kGateHeld},
};
inline constexpr StyleEvent kPad7[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=56, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=54, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=56, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=50, .gate=kGateHeld},
};
inline constexpr StyleEvent kPadRe[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=56, .gate=kGateHalfBar}, {.step=0, .tone=kThird, .octave=0, .vel=54, .gate=kGateHalfBar}, {.step=0, .tone=kFifth, .octave=0, .vel=56, .gate=kGateHalfBar}, {.step=0, .tone=kSeventh, .octave=0, .vel=50, .gate=kGateHalfBar},
    {.step=8, .tone=kRoot, .octave=0, .vel=54, .gate=kGateHalfBar}, {.step=8, .tone=kThird, .octave=0, .vel=52, .gate=kGateHalfBar}, {.step=8, .tone=kFifth, .octave=0, .vel=54, .gate=kGateHalfBar}, {.step=8, .tone=kSeventh, .octave=0, .vel=48, .gate=kGateHalfBar},
};
// Organ skank: light upper-voice stabs off chord1, staccato.
inline constexpr StyleEvent kCh2A[] = {
    {.step=4, .tone=kThird, .octave=0, .vel=66, .gate=kGateStaccato}, {.step=4, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateStaccato}, {.step=12, .tone=kThird, .octave=0, .vel=64, .gate=kGateStaccato}, {.step=12, .tone=kSeventh, .octave=0, .vel=64, .gate=kGateStaccato},
};
inline constexpr StyleEvent kCh2Off[] = {
    {.step=2, .tone=kThird, .octave=0, .vel=64, .gate=kGateStaccato}, {.step=2, .tone=kSeventh, .octave=0, .vel=64, .gate=kGateStaccato}, {.step=6, .tone=kThird, .octave=0, .vel=60, .gate=kGateStaccato}, {.step=6, .tone=kSeventh, .octave=0, .vel=60, .gate=kGateStaccato},
    {.step=10, .tone=kThird, .octave=0, .vel=64, .gate=kGateStaccato}, {.step=10, .tone=kSeventh, .octave=0, .vel=64, .gate=kGateStaccato}, {.step=14, .tone=kThird, .octave=0, .vel=60, .gate=kGateStaccato}, {.step=14, .tone=kSeventh, .octave=0, .vel=60, .gate=kGateStaccato},
};
// Square-lead arp: high, up the chord in 8ths (varC) and 16ths (varD).
inline constexpr StyleEvent kArp8[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=70, .gate=kGateHat}, {.step=2, .tone=kThird, .octave=0, .vel=64, .gate=kGateHat}, {.step=4, .tone=kFifth, .octave=0, .vel=68, .gate=kGateHat}, {.step=6, .tone=kSeventh, .octave=0, .vel=64, .gate=kGateHat},
    {.step=8, .tone=kRoot, .octave=1, .vel=70, .gate=kGateHat}, {.step=10, .tone=kSeventh, .octave=0, .vel=64, .gate=kGateHat}, {.step=12, .tone=kFifth, .octave=0, .vel=68, .gate=kGateHat}, {.step=14, .tone=kThird, .octave=0, .vel=64, .gate=kGateHat},
};
inline constexpr StyleEvent kArp16[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=72, .gate=kGateStaccato}, {.step=1, .tone=kThird, .octave=0, .vel=60, .gate=kGateStaccato}, {.step=2, .tone=kFifth, .octave=0, .vel=66, .gate=kGateStaccato}, {.step=3, .tone=kSeventh, .octave=0, .vel=60, .gate=kGateStaccato},
    {.step=4, .tone=kRoot, .octave=1, .vel=72, .gate=kGateStaccato}, {.step=5, .tone=kSeventh, .octave=0, .vel=60, .gate=kGateStaccato}, {.step=6, .tone=kFifth, .octave=0, .vel=66, .gate=kGateStaccato}, {.step=7, .tone=kThird, .octave=0, .vel=60, .gate=kGateStaccato},
    {.step=8, .tone=kRoot, .octave=0, .vel=70, .gate=kGateStaccato}, {.step=9, .tone=kThird, .octave=0, .vel=60, .gate=kGateStaccato}, {.step=10, .tone=kFifth, .octave=0, .vel=66, .gate=kGateStaccato}, {.step=11, .tone=kSeventh, .octave=0, .vel=60, .gate=kGateStaccato},
    {.step=12, .tone=kRoot, .octave=1, .vel=72, .gate=kGateStaccato}, {.step=13, .tone=kSeventh, .octave=0, .vel=60, .gate=kGateStaccato}, {.step=14, .tone=kFifth, .octave=0, .vel=66, .gate=kGateStaccato}, {.step=15, .tone=kThird, .octave=0, .vel=60, .gate=kGateStaccato},
};
// Hand percussion: congas, with a cowbell on the peak groove.
inline constexpr StyleEvent kPercA[] = {
    {.step=2, .tone=kOpenHiConga, .octave=0, .vel=66, .gate=kGateHat}, {.step=6, .tone=kLoConga, .octave=0, .vel=60, .gate=kGateHat}, {.step=10, .tone=kOpenHiConga, .octave=0, .vel=66, .gate=kGateHat}, {.step=14, .tone=kLoConga, .octave=0, .vel=60, .gate=kGateHat},
};
inline constexpr StyleEvent kPercD[] = {
    {.step=0, .tone=kMuteHiConga, .octave=0, .vel=60, .gate=kGateHat}, {.step=2, .tone=kOpenHiConga, .octave=0, .vel=70, .gate=kGateHat}, {.step=4, .tone=kCowbell, .octave=0, .vel=72, .gate=kGateHat}, {.step=6, .tone=kLoConga, .octave=0, .vel=64, .gate=kGateHat},
    {.step=8, .tone=kMuteHiConga, .octave=0, .vel=60, .gate=kGateHat}, {.step=10, .tone=kOpenHiConga, .octave=0, .vel=70, .gate=kGateHat}, {.step=12, .tone=kCowbell, .octave=0, .vel=72, .gate=kGateHat}, {.step=14, .tone=kLoConga, .octave=0, .vel=64, .gate=kGateHat},
};

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
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadT), .gm_program=kPadVoice},
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
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadT), .gm_program=kPadVoice},
    {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCh2A), .gm_program=kChord2Voice},
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
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadT), .gm_program=kPadVoice},
    {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCh2Off), .gm_program=kChord2Voice},
    {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercA)},
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
inline constexpr StylePattern kFAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass), .motif=&kFillBassMotif}};
inline constexpr StylePattern kFBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass), .motif=&kFillBassMotif}};
inline constexpr StylePattern kFCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass), .motif=&kFillBassMotif}};
inline constexpr StylePattern kFDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass), .motif=&kFillBassMotif}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercD)}};
inline constexpr StyleEvent kE1D[] = {{.step=0, .tone=kKick, .octave=0, .vel=116, .gate=120}, {.step=0, .tone=kCrash, .octave=0, .vel=108, .gate=kGateHalfBar}};
inline constexpr StyleEvent kE1B[] = {{.step=0, .tone=kRoot, .octave=0, .vel=110, .gate=kGateHeld}};
inline constexpr StyleEvent kE1C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=88, .gate=kGateHeld}};
inline constexpr StylePattern kE1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1C)}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice}};
inline constexpr StyleEvent kE2D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=114, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=118, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=110, .gate=120}};
inline constexpr StyleEvent kE2B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=112, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=100, .gate=kGateHeld}};
inline constexpr StyleEvent kE2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=90, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=90, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=90, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=90, .gate=kGateHeld}};
inline constexpr StylePattern kE2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2C)}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice}};
// varC: "on the one" half-time funk — heavy downbeat, backbeat on 3, ghost snares.
inline constexpr StyleEvent kCD[] = {{.step=0, .tone=kKick, .octave=0, .vel=118, .gate=120}, {.step=6, .tone=kKick, .octave=0, .vel=88, .gate=120}, {.step=10, .tone=kKick, .octave=0, .vel=92, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=110, .gate=120}, {.step=3, .tone=kSnare, .octave=0, .vel=44, .gate=50}, {.step=13, .tone=kSnare, .octave=0, .vel=46, .gate=50}, {.step=0, .tone=kClosedHat, .octave=0, .vel=74, .gate=50}, {.step=2, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=70, .gate=50}, {.step=6, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=74, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=70, .gate=50}, {.step=14, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}};
inline constexpr StyleEvent kCC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=0, .tone=kThird, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=0, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateStaccato}, {.step=8, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=8, .tone=kThird, .octave=0, .vel=78, .gate=kGateStaccato}, {.step=8, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStaccato}};
inline constexpr StylePattern kCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAB)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCC)},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadRe), .gm_program=kPadVoice}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCh2A), .gm_program=kChord2Voice}, {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArp8), .gm_program=kArpVoice}};
// varD: peak double-pump — busiest kit, 16th hats with opens, driving bass, full 7th stabs.
inline constexpr StyleEvent kDD[] = {{.step=0, .tone=kKick, .octave=0, .vel=120, .gate=120}, {.step=3, .tone=kKick, .octave=0, .vel=96, .gate=120}, {.step=6, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=10, .tone=kKick, .octave=0, .vel=100, .gate=120}, {.step=13, .tone=kKick, .octave=0, .vel=94, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=116, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=116, .gate=120}, {.step=2, .tone=kSnare, .octave=0, .vel=46, .gate=50}, {.step=7, .tone=kSnare, .octave=0, .vel=50, .gate=50}, {.step=11, .tone=kSnare, .octave=0, .vel=48, .gate=50}, {.step=15, .tone=kSnare, .octave=0, .vel=52, .gate=50}, {.step=0, .tone=kClosedHat, .octave=0, .vel=82, .gate=50}, {.step=2, .tone=kOpenHat, .octave=0, .vel=72, .gate=120}, {.step=4, .tone=kClosedHat, .octave=0, .vel=74, .gate=50}, {.step=6, .tone=kOpenHat, .octave=0, .vel=72, .gate=120}, {.step=8, .tone=kClosedHat, .octave=0, .vel=82, .gate=50}, {.step=10, .tone=kOpenHat, .octave=0, .vel=72, .gate=120}, {.step=12, .tone=kClosedHat, .octave=0, .vel=74, .gate=50}, {.step=14, .tone=kOpenHat, .octave=0, .vel=74, .gate=120}};
inline constexpr StyleEvent kDC[] = {{.step=2, .tone=kRoot, .octave=0, .vel=86, .gate=kGateStaccato}, {.step=2, .tone=kThird, .octave=0, .vel=86, .gate=kGateStaccato}, {.step=2, .tone=kFifth, .octave=0, .vel=86, .gate=kGateStaccato}, {.step=2, .tone=kSeventh, .octave=0, .vel=86, .gate=kGateStaccato}, {.step=6, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=6, .tone=kThird, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=6, .tone=kSeventh, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=10, .tone=kRoot, .octave=0, .vel=86, .gate=kGateStaccato}, {.step=10, .tone=kThird, .octave=0, .vel=86, .gate=kGateStaccato}, {.step=10, .tone=kFifth, .octave=0, .vel=86, .gate=kGateStaccato}, {.step=10, .tone=kSeventh, .octave=0, .vel=86, .gate=kGateStaccato}, {.step=14, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=14, .tone=kThird, .octave=0, .vel=82, .gate=kGateStaccato}, {.step=14, .tone=kSeventh, .octave=0, .vel=82, .gate=kGateStaccato}};
inline constexpr StylePattern kDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBB)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kDC)},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadRe), .gm_program=kPadVoice}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCh2Off), .gm_program=kChord2Voice}, {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArp16), .gm_program=kArpVoice}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercD)}};
// varBreak: the band drops out — one accented downbeat stab (kick+crash, a bass
// pop, a full 7th chord), then wide silence, then a snare/tom pickup on beat 4
// that throws the groove back in. A real synchronized stop; SectionType::kBreak
// had been authored by zero styles until here (reachable via `style section break`).
inline constexpr StyleEvent kBrkD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=118, .gate=120}, {.step=0, .tone=kCrash, .octave=0, .vel=104, .gate=kGateStab},
    {.step=12, .tone=kSnare, .octave=0, .vel=92, .gate=100}, {.step=13, .tone=kSnare, .octave=0, .vel=98, .gate=100}, {.step=14, .tone=kTomMid, .octave=0, .vel=104, .gate=100}, {.step=15, .tone=kTomLow, .octave=0, .vel=110, .gate=100},
};
inline constexpr StyleEvent kBrkB[] = {{.step=0, .tone=kRoot, .octave=0, .vel=115, .gate=kGateStab}};
inline constexpr StyleEvent kBrkC[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=92, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=92, .gate=kGateStab}, {.step=0, .tone=kFifth, .octave=0, .vel=92, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=92, .gate=kGateStab},
};
inline constexpr StylePattern kBrkP[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kBrkD)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBrkB)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBrkC)},
};
inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=1, .patterns=Span<const StylePattern>(kIn1P)}, {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIn2P)},
    {.type=SectionType::kVarA, .bars=1, .patterns=Span<const StylePattern>(kAP)}, {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kBP)},
    {.type=SectionType::kVarC, .bars=1, .patterns=Span<const StylePattern>(kCP)}, {.type=SectionType::kVarD, .bars=1, .patterns=Span<const StylePattern>(kDP)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFAP)}, {.type=SectionType::kFillB, .bars=1, .patterns=Span<const StylePattern>(kFBP)}, {.type=SectionType::kFillC, .bars=1, .patterns=Span<const StylePattern>(kFCP)}, {.type=SectionType::kFillD, .bars=1, .patterns=Span<const StylePattern>(kFDP)},
    {.type=SectionType::kBreak, .bars=1, .patterns=Span<const StylePattern>(kBrkP)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kE1P)}, {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kE2P)},
};
inline constexpr Style kStyle{.name="funk", .sections=Span<const StyleSection>(kSections), .tempo=10800};
}  // namespace funk

}  // namespace styles
}  // namespace arrangrr
