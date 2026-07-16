#pragma once

#include "arrangrr/arranger/style_model.hpp"

namespace arrangrr {
namespace styles {


// ---------------------------------------------------------------------------
// "disco": four-on-the-floor kick, off-beat open hats, clap backbeat, the
// classic octave-jumping eighth bass, off-beat string-stab triads.
namespace disco {
inline constexpr StyleEvent kHeldBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=100, .gate=kGateHeld}};
inline constexpr StyleEvent kFillBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=100, .gate=kGate8th}, {.step=8, .tone=kRoot, .octave=1, .vel=94, .gate=kGate8th}};
// Motif engine (9210, Ottorino RANK 8): the flat FILL bass (2 onsets, reused
// identically across FillA-D) — low expressiveness either way, but real,
// measured redundancy. kRetrograde; the Var bass (kAB/kBB) and Arp
// (kArp8/kArp16) already carry real hand-authored variety, so they are not
// touched here.
inline constexpr MotifSpec kFillBassMotif{.transform = MotifTransform::kRetrograde, .seed = 802};

// Fuller-band roles (ADDITIVE): a sustained string bed, a chicken-scratch guitar
// chank, a saw-lead arp and Latin percussion. kPad->GM50 (SynthStrings1),
// kChord2->GM27 (Clean Guitar), kArp->GM81 (Saw Lead); kPerc rides the drum
// channel. Pad holds register 48 under the whole bar.
inline constexpr std::int16_t kPadVoice = 50;
inline constexpr std::int16_t kChord2Voice = 27;
inline constexpr std::int16_t kArpVoice = 81;
inline constexpr StyleEvent kPadT[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=56, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=54, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=56, .gate=kGateHeld},
};
inline constexpr StyleEvent kPad7[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=58, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=56, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=58, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=52, .gate=kGateHeld},
};
inline constexpr StyleEvent kPadRe[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=58, .gate=kGateHalfBar}, {.step=0, .tone=kThird, .octave=0, .vel=56, .gate=kGateHalfBar}, {.step=0, .tone=kFifth, .octave=0, .vel=58, .gate=kGateHalfBar}, {.step=0, .tone=kSeventh, .octave=0, .vel=52, .gate=kGateHalfBar},
    {.step=8, .tone=kRoot, .octave=0, .vel=56, .gate=kGateHalfBar}, {.step=8, .tone=kThird, .octave=0, .vel=54, .gate=kGateHalfBar}, {.step=8, .tone=kFifth, .octave=0, .vel=56, .gate=kGateHalfBar}, {.step=8, .tone=kSeventh, .octave=0, .vel=50, .gate=kGateHalfBar},
};
// Guitar chank: tight on-beat scratches (varA) opening to a 16th chank (varD),
// complementary to chord1's off-beat string stabs.
inline constexpr StyleEvent kCh2A[] = {
    {.step=0, .tone=kThird, .octave=0, .vel=64, .gate=kGateStaccato}, {.step=0, .tone=kFifth, .octave=0, .vel=64, .gate=kGateStaccato}, {.step=8, .tone=kThird, .octave=0, .vel=62, .gate=kGateStaccato}, {.step=8, .tone=kFifth, .octave=0, .vel=62, .gate=kGateStaccato},
};
inline constexpr StyleEvent kCh2Chank[] = {
    {.step=0, .tone=kThird, .octave=0, .vel=64, .gate=kGateStaccato}, {.step=0, .tone=kFifth, .octave=0, .vel=64, .gate=kGateStaccato}, {.step=3, .tone=kThird, .octave=0, .vel=56, .gate=kGateStaccato}, {.step=3, .tone=kFifth, .octave=0, .vel=56, .gate=kGateStaccato},
    {.step=4, .tone=kThird, .octave=0, .vel=62, .gate=kGateStaccato}, {.step=4, .tone=kFifth, .octave=0, .vel=62, .gate=kGateStaccato}, {.step=7, .tone=kThird, .octave=0, .vel=56, .gate=kGateStaccato}, {.step=7, .tone=kFifth, .octave=0, .vel=56, .gate=kGateStaccato},
    {.step=8, .tone=kThird, .octave=0, .vel=64, .gate=kGateStaccato}, {.step=8, .tone=kFifth, .octave=0, .vel=64, .gate=kGateStaccato}, {.step=11, .tone=kThird, .octave=0, .vel=56, .gate=kGateStaccato}, {.step=11, .tone=kFifth, .octave=0, .vel=56, .gate=kGateStaccato},
    {.step=12, .tone=kThird, .octave=0, .vel=62, .gate=kGateStaccato}, {.step=12, .tone=kFifth, .octave=0, .vel=62, .gate=kGateStaccato}, {.step=15, .tone=kThird, .octave=0, .vel=56, .gate=kGateStaccato}, {.step=15, .tone=kFifth, .octave=0, .vel=56, .gate=kGateStaccato},
};
// Saw-lead arp: octave-jumping disco run up the chord.
inline constexpr StyleEvent kArp8[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=70, .gate=kGateHat}, {.step=2, .tone=kThird, .octave=0, .vel=64, .gate=kGateHat}, {.step=4, .tone=kFifth, .octave=0, .vel=68, .gate=kGateHat}, {.step=6, .tone=kRoot, .octave=1, .vel=66, .gate=kGateHat},
    {.step=8, .tone=kFifth, .octave=0, .vel=68, .gate=kGateHat}, {.step=10, .tone=kSeventh, .octave=0, .vel=64, .gate=kGateHat}, {.step=12, .tone=kThird, .octave=1, .vel=68, .gate=kGateHat}, {.step=14, .tone=kRoot, .octave=1, .vel=66, .gate=kGateHat},
};
inline constexpr StyleEvent kArp16[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=72, .gate=kGateStaccato}, {.step=1, .tone=kThird, .octave=0, .vel=60, .gate=kGateStaccato}, {.step=2, .tone=kFifth, .octave=0, .vel=66, .gate=kGateStaccato}, {.step=3, .tone=kRoot, .octave=1, .vel=62, .gate=kGateStaccato},
    {.step=4, .tone=kThird, .octave=1, .vel=70, .gate=kGateStaccato}, {.step=5, .tone=kRoot, .octave=1, .vel=60, .gate=kGateStaccato}, {.step=6, .tone=kFifth, .octave=0, .vel=66, .gate=kGateStaccato}, {.step=7, .tone=kThird, .octave=0, .vel=60, .gate=kGateStaccato},
    {.step=8, .tone=kRoot, .octave=0, .vel=72, .gate=kGateStaccato}, {.step=9, .tone=kThird, .octave=0, .vel=60, .gate=kGateStaccato}, {.step=10, .tone=kFifth, .octave=0, .vel=66, .gate=kGateStaccato}, {.step=11, .tone=kSeventh, .octave=0, .vel=62, .gate=kGateStaccato},
    {.step=12, .tone=kRoot, .octave=1, .vel=70, .gate=kGateStaccato}, {.step=13, .tone=kSeventh, .octave=0, .vel=60, .gate=kGateStaccato}, {.step=14, .tone=kFifth, .octave=0, .vel=66, .gate=kGateStaccato}, {.step=15, .tone=kThird, .octave=0, .vel=60, .gate=kGateStaccato},
};
// Percussion: off-beat tambourine (varB) opening to a conga/tambourine bed (varD).
inline constexpr StyleEvent kPercA[] = {
    {.step=2, .tone=kTambourine, .octave=0, .vel=66, .gate=kGateHat}, {.step=6, .tone=kTambourine, .octave=0, .vel=62, .gate=kGateHat}, {.step=10, .tone=kTambourine, .octave=0, .vel=66, .gate=kGateHat}, {.step=14, .tone=kTambourine, .octave=0, .vel=62, .gate=kGateHat},
};
inline constexpr StyleEvent kPercD[] = {
    {.step=0, .tone=kOpenHiConga, .octave=0, .vel=64, .gate=kGateHat}, {.step=2, .tone=kTambourine, .octave=0, .vel=68, .gate=kGateHat}, {.step=4, .tone=kLoConga, .octave=0, .vel=60, .gate=kGateHat}, {.step=6, .tone=kTambourine, .octave=0, .vel=66, .gate=kGateHat},
    {.step=8, .tone=kOpenHiConga, .octave=0, .vel=64, .gate=kGateHat}, {.step=10, .tone=kTambourine, .octave=0, .vel=68, .gate=kGateHat}, {.step=12, .tone=kLoConga, .octave=0, .vel=60, .gate=kGateHat}, {.step=14, .tone=kTambourine, .octave=0, .vel=66, .gate=kGateHat},
};
// The classic disco STRING SWELL: collapse the pad block into ONE kRollUp event
// so the string tones spread (ascending) across the held gate — a rising open at
// the top of the bar instead of a flat block. A rolled role stays kAsWritten
// (never kLead on a gesture's output).
inline constexpr StyleEvent kPadSwell[] = {{.step=0, .tone=kRoot, .octave=0, .vel=58, .gate=kGateHeld, .gesture=ChordGesture::kRollUp}};
// A descending disco synth line for the peak (varD): octave-7th-5th-3rd, the
// falling-string cliché. kScaleDegree keeps it diatonic to the live KEY over any
// chord change. kLead role -> anchor 72.
inline constexpr std::int16_t kLeadVoice = 82;  // Lead 3 (calliope) — disco synth line
inline constexpr StyleEvent kLeadLine[] = {
    {.step=8,  .tone=7, .octave=0, .vel=90, .gate=kGate8th,  .src=NoteSource::kScaleDegree},   // octave
    {.step=10, .tone=6, .octave=0, .vel=84, .gate=kGate8th,  .src=NoteSource::kScaleDegree},   // 7th
    {.step=12, .tone=4, .octave=0, .vel=88, .gate=kGate8th,  .src=NoteSource::kScaleDegree},   // 5th
    {.step=14, .tone=2, .octave=0, .vel=86, .gate=kGateBeat, .src=NoteSource::kScaleDegree},   // 3rd
};
// The falling disco synth line, wired only into varD.
inline constexpr MotifSpec kLeadLineMotif{.transform = MotifTransform::kDiatonicTranspose, .seed = 801};
inline constexpr StyleEvent kIn1D[] = {{.step=8, .tone=kOpenHat, .octave=0, .vel=64, .gate=120}, {.step=10, .tone=kOpenHat, .octave=0, .vel=70, .gate=120}, {.step=12, .tone=kOpenHat, .octave=0, .vel=78, .gate=120}, {.step=14, .tone=kOpenHat, .octave=0, .vel=88, .gate=120}};
inline constexpr StylePattern kIn1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)}};
inline constexpr StyleEvent kIn2D[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=4, .tone=kKick, .octave=0, .vel=106, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=12, .tone=kKick, .octave=0, .vel=106, .gate=120},
    {.step=2, .tone=kOpenHat, .octave=0, .vel=72, .gate=120}, {.step=6, .tone=kOpenHat, .octave=0, .vel=72, .gate=120}, {.step=10, .tone=kOpenHat, .octave=0, .vel=72, .gate=120}, {.step=14, .tone=kOpenHat, .octave=0, .vel=72, .gate=120}, {.step=4, .tone=kClap, .octave=0, .vel=96, .gate=120}, {.step=12, .tone=kClap, .octave=0, .vel=96, .gate=120},
};
inline constexpr StyleEvent kIn2C[] = {{.step=2, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=76, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=76, .gate=kGateStab}};
inline constexpr StylePattern kIn2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIn2C)}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadT), .gm_program=kPadVoice}};
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
inline constexpr StylePattern kAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAB)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAC), .voicing=VoicingPolicy::kLead},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadT), .gm_program=kPadVoice}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCh2A), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead}};
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
inline constexpr StylePattern kBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBB)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBC), .voicing=VoicingPolicy::kLead},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadT), .gm_program=kPadVoice}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCh2A), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercA)}};
inline constexpr StyleEvent kFAD[] = {{.step=0, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=4, .tone=kClap, .octave=0, .vel=100, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=96, .gate=120}, {.step=10, .tone=kSnare, .octave=0, .vel=100, .gate=120}, {.step=12, .tone=kOpenHat, .octave=0, .vel=90, .gate=120}, {.step=14, .tone=kOpenHat, .octave=0, .vel=98, .gate=120}};
inline constexpr StyleEvent kFBD[] = {{.step=0, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=98, .gate=100}, {.step=6, .tone=kSnare, .octave=0, .vel=92, .gate=100}, {.step=8, .tone=kTomHi, .octave=0, .vel=100, .gate=100}, {.step=10, .tone=kTomMid, .octave=0, .vel=104, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=108, .gate=100}, {.step=14, .tone=kClap, .octave=0, .vel=110, .gate=100}};
inline constexpr StyleEvent kFCD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=94, .gate=100}, {.step=2, .tone=kSnare, .octave=0, .vel=88, .gate=100}, {.step=4, .tone=kTomHi, .octave=0, .vel=100, .gate=100}, {.step=6, .tone=kTomHi, .octave=0, .vel=96, .gate=100}, {.step=8, .tone=kTomMid, .octave=0, .vel=104, .gate=100}, {.step=10, .tone=kTomMid, .octave=0, .vel=100, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=110, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=114, .gate=100}};
inline constexpr StyleEvent kFDD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=92, .gate=50}, {.step=1, .tone=kSnare, .octave=0, .vel=86, .gate=50}, {.step=2, .tone=kSnare, .octave=0, .vel=96, .gate=50}, {.step=3, .tone=kSnare, .octave=0, .vel=90, .gate=50}, {.step=4, .tone=kSnare, .octave=0, .vel=100, .gate=50}, {.step=5, .tone=kSnare, .octave=0, .vel=94, .gate=50}, {.step=6, .tone=kTomHi, .octave=0, .vel=104, .gate=50}, {.step=7, .tone=kTomHi, .octave=0, .vel=98, .gate=50}, {.step=8, .tone=kTomMid, .octave=0, .vel=106, .gate=50}, {.step=9, .tone=kTomMid, .octave=0, .vel=100, .gate=50}, {.step=10, .tone=kTomLow, .octave=0, .vel=110, .gate=50}, {.step=11, .tone=kTomLow, .octave=0, .vel=104, .gate=50}, {.step=12, .tone=kTomFloor, .octave=0, .vel=114, .gate=50}, {.step=14, .tone=kClap, .octave=0, .vel=118, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=120, .gate=kGate8th}};
inline constexpr StylePattern kFAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass), .motif=&kFillBassMotif}};
inline constexpr StylePattern kFBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass), .motif=&kFillBassMotif}};
inline constexpr StylePattern kFCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass), .motif=&kFillBassMotif}};
inline constexpr StylePattern kFDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass), .motif=&kFillBassMotif}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercD)}};
inline constexpr StyleEvent kE1D[] = {{.step=0, .tone=kKick, .octave=0, .vel=114, .gate=120}, {.step=0, .tone=kCrash, .octave=0, .vel=106, .gate=kGateHalfBar}};
inline constexpr StyleEvent kE1B[] = {{.step=0, .tone=kRoot, .octave=0, .vel=106, .gate=kGateHeld}};
inline constexpr StyleEvent kE1C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=86, .gate=kGateHeld}};
inline constexpr StylePattern kE1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1C)}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice}};
inline constexpr StyleEvent kE2D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=112, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=116, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=8, .tone=kClap, .octave=0, .vel=100, .gate=120}};
inline constexpr StyleEvent kE2B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=110, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=98, .gate=kGateHeld}};
inline constexpr StyleEvent kE2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=88, .gate=kGateHeld}};
inline constexpr StylePattern kE2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2C)}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice}};
// varC: breakdown — bare four-on-the-floor, clap backbeat, off-beat open hats, space.
inline constexpr StyleEvent kCD[] = {{.step=0, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=4, .tone=kKick, .octave=0, .vel=108, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=12, .tone=kKick, .octave=0, .vel=108, .gate=120}, {.step=4, .tone=kClap, .octave=0, .vel=98, .gate=120}, {.step=12, .tone=kClap, .octave=0, .vel=98, .gate=120}, {.step=2, .tone=kOpenHat, .octave=0, .vel=80, .gate=120}, {.step=6, .tone=kOpenHat, .octave=0, .vel=80, .gate=120}, {.step=10, .tone=kOpenHat, .octave=0, .vel=80, .gate=120}, {.step=14, .tone=kOpenHat, .octave=0, .vel=80, .gate=120}};
inline constexpr StyleEvent kCC[] = {{.step=2, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStab}};
inline constexpr StylePattern kCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAB)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCC), .voicing=VoicingPolicy::kLead},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadRe), .gm_program=kPadVoice}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCh2A), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArp8), .gm_program=kArpVoice}};
// varD: peak — snare-doubled claps, 16th hat carpet with opens, tambourine, full 7th stabs.
inline constexpr StyleEvent kDD[] = {{.step=0, .tone=kKick, .octave=0, .vel=116, .gate=120}, {.step=4, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=116, .gate=120}, {.step=12, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=4, .tone=kClap, .octave=0, .vel=102, .gate=120}, {.step=12, .tone=kClap, .octave=0, .vel=102, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=90, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=90, .gate=120}, {.step=2, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}, {.step=6, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}, {.step=10, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}, {.step=14, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}, {.step=0, .tone=kClosedHat, .octave=0, .vel=68, .gate=50}, {.step=1, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=3, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=68, .gate=50}, {.step=5, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=7, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=68, .gate=50}, {.step=9, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=11, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=68, .gate=50}, {.step=13, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=15, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=0, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=8, .tone=kTambourine, .octave=0, .vel=60, .gate=50}};
inline constexpr StyleEvent kDC[] = {{.step=2, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}, {.step=6, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}, {.step=6, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}, {.step=14, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}, {.step=14, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateStab}};
inline constexpr StylePattern kDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBB)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kDC), .voicing=VoicingPolicy::kLead},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadSwell), .gm_program=kPadVoice}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCh2Chank), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArp16), .gm_program=kArpVoice}, {.role=TrackRole::kLead, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kLeadLine), .gm_program=kLeadVoice, .motif=&kLeadLineMotif}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercD)}};
// varBreak: the disco break — the band drops to one accented downbeat (kick+crash,
// an octave bass pop, a full 7th stab held as a literal synchronized block), wide
// silence, then a tom/clap/open-hat pickup on beat 4 that throws the
// four-on-the-floor back in.
inline constexpr StyleEvent kBrkD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=116, .gate=120}, {.step=0, .tone=kCrash, .octave=0, .vel=106, .gate=kGateStab},
    {.step=12, .tone=kTomMid, .octave=0, .vel=100, .gate=100}, {.step=13, .tone=kTomLow, .octave=0, .vel=104, .gate=100}, {.step=14, .tone=kClap, .octave=0, .vel=108, .gate=100}, {.step=15, .tone=kOpenHat, .octave=0, .vel=96, .gate=120},
};
inline constexpr StyleEvent kBrkB[] = {{.step=0, .tone=kRoot, .octave=0, .vel=110, .gate=kGateStab}};
inline constexpr StyleEvent kBrkC[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=90, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=90, .gate=kGateStab}, {.step=0, .tone=kFifth, .octave=0, .vel=90, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=90, .gate=kGateStab},
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
inline constexpr Style kStyle{.name="disco", .sections=Span<const StyleSection>(kSections), .tempo=12200};
}  // namespace disco

}  // namespace styles
}  // namespace arrangrr
