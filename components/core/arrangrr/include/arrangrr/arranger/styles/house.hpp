#pragma once

#include "arrangrr/arranger/style_model.hpp"

namespace arrangrr {
namespace styles {


// ---------------------------------------------------------------------------
// "house": four-on-the-floor with off-beat open hats and a shaker, clap on the
// backbeat, a driving off-beat eighth bass and sustained 7th organ stabs.
namespace house {
inline constexpr StyleEvent kHeldBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=100, .gate=kGateHeld},
    // bar 2 (style-depth Wave-2 C): re-hit so the held bass keeps sustaining
    // once a section using it (Intro1) runs 2 bars; Intro2 stays 1 bar and
    // never reads this tail -- a background hold, same idiom as basic's
    // kPadTriad re-trigger.
    {.step=16, .tone=kRoot, .octave=0, .vel=100, .gate=kGateHeld},
};
inline constexpr StyleEvent kFillBass[] = {{.step=2, .tone=kRoot, .octave=0, .vel=98, .gate=kGate8th}, {.step=10, .tone=kFifth, .octave=0, .vel=94, .gate=kGate8th}};
// Motif engine (9210, Ottorino RANK 9): same shape as disco — the flat FILL
// bass, reused identically across FillA-D; the Var bass (kAB/kBB) and Arp
// (kArp8/kArp16) already carry real hand-authored variety.
inline constexpr MotifSpec kFillBassMotif{.transform = MotifTransform::kRetrograde, .seed = 902};

// Fuller-band roles (ADDITIVE): a sustained warm pad, an off-beat E.piano stab,
// a square-lead arp and shaker/conga percussion. kPad->GM89 (Pad2 warm),
// kChord2->GM5 (E.Piano2), kArp->GM80 (Square Lead); kPerc rides the drum
// channel. Pad holds register 48 under the whole bar.
inline constexpr std::int16_t kPadVoice = 89;
inline constexpr std::int16_t kChord2Voice = 5;
inline constexpr std::int16_t kArpVoice = 80;
inline constexpr StyleEvent kPadT[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=54, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=52, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=54, .gate=kGateHeld},
    // bar 2 (style-depth Wave-2 C): re-hit so the pad keeps sustaining once a
    // section using it (VarA) runs 2 bars -- a background hold, not the
    // section's own bar-to-bar variation (that lives in drums/bass/chord1).
    {.step=16, .tone=kRoot, .octave=0, .vel=54, .gate=kGateHeld}, {.step=16, .tone=kThird, .octave=0, .vel=52, .gate=kGateHeld}, {.step=16, .tone=kFifth, .octave=0, .vel=54, .gate=kGateHeld},
};
inline constexpr StyleEvent kPad7[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=56, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=54, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=56, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=50, .gate=kGateHeld},
};
inline constexpr StyleEvent kPadRe[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=56, .gate=kGateHalfBar}, {.step=0, .tone=kThird, .octave=0, .vel=54, .gate=kGateHalfBar}, {.step=0, .tone=kFifth, .octave=0, .vel=56, .gate=kGateHalfBar}, {.step=0, .tone=kSeventh, .octave=0, .vel=50, .gate=kGateHalfBar},
    {.step=8, .tone=kRoot, .octave=0, .vel=54, .gate=kGateHalfBar}, {.step=8, .tone=kThird, .octave=0, .vel=52, .gate=kGateHalfBar}, {.step=8, .tone=kFifth, .octave=0, .vel=54, .gate=kGateHalfBar}, {.step=8, .tone=kSeventh, .octave=0, .vel=48, .gate=kGateHalfBar},
};
// E.piano: the classic house off-beat "and" stab, complementary to chord1.
inline constexpr StyleEvent kCh2A[] = {
    {.step=2, .tone=kThird, .octave=0, .vel=66, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=64, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=64, .gate=kGateStab},
    // bar 2 (style-depth Wave-2 C): the E.piano answers onto the "and" of 2
    // and 4 instead of 1 and 3 -- the same syncopated answer idiom kCh2Off
    // already uses, without touching kCh2Off itself (motif-locked on VarD).
    {.step=6, .tone=kThird, .octave=0, .vel=66, .gate=kGateStab}, {.step=6, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=64, .gate=kGateStab}, {.step=14, .tone=kSeventh, .octave=0, .vel=64, .gate=kGateStab},
};
inline constexpr StyleEvent kCh2Off[] = {
    {.step=2, .tone=kThird, .octave=0, .vel=66, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=62, .gate=kGateStab}, {.step=6, .tone=kSeventh, .octave=0, .vel=62, .gate=kGateStab},
    {.step=10, .tone=kThird, .octave=0, .vel=66, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=62, .gate=kGateStab}, {.step=14, .tone=kSeventh, .octave=0, .vel=62, .gate=kGateStab},
};
// Square-lead arp: high, up the chord in 8ths (varC) and 16ths (varD).
inline constexpr StyleEvent kArp8[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=68, .gate=kGateHat}, {.step=2, .tone=kThird, .octave=0, .vel=62, .gate=kGateHat}, {.step=4, .tone=kFifth, .octave=0, .vel=66, .gate=kGateHat}, {.step=6, .tone=kSeventh, .octave=0, .vel=62, .gate=kGateHat},
    {.step=8, .tone=kRoot, .octave=1, .vel=68, .gate=kGateHat}, {.step=10, .tone=kSeventh, .octave=0, .vel=62, .gate=kGateHat}, {.step=12, .tone=kFifth, .octave=0, .vel=66, .gate=kGateHat}, {.step=14, .tone=kThird, .octave=0, .vel=62, .gate=kGateHat},
};
inline constexpr StyleEvent kArp16[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=70, .gate=kGateStaccato}, {.step=1, .tone=kThird, .octave=0, .vel=58, .gate=kGateStaccato}, {.step=2, .tone=kFifth, .octave=0, .vel=64, .gate=kGateStaccato}, {.step=3, .tone=kSeventh, .octave=0, .vel=58, .gate=kGateStaccato},
    {.step=4, .tone=kRoot, .octave=1, .vel=70, .gate=kGateStaccato}, {.step=5, .tone=kSeventh, .octave=0, .vel=58, .gate=kGateStaccato}, {.step=6, .tone=kFifth, .octave=0, .vel=64, .gate=kGateStaccato}, {.step=7, .tone=kThird, .octave=0, .vel=58, .gate=kGateStaccato},
    {.step=8, .tone=kRoot, .octave=0, .vel=70, .gate=kGateStaccato}, {.step=9, .tone=kThird, .octave=0, .vel=58, .gate=kGateStaccato}, {.step=10, .tone=kFifth, .octave=0, .vel=64, .gate=kGateStaccato}, {.step=11, .tone=kSeventh, .octave=0, .vel=58, .gate=kGateStaccato},
    {.step=12, .tone=kRoot, .octave=1, .vel=70, .gate=kGateStaccato}, {.step=13, .tone=kSeventh, .octave=0, .vel=58, .gate=kGateStaccato}, {.step=14, .tone=kFifth, .octave=0, .vel=64, .gate=kGateStaccato}, {.step=15, .tone=kThird, .octave=0, .vel=58, .gate=kGateStaccato},
};
// Percussion: off-beat maracas (varB) opening to a maraca/conga bed (varD).
inline constexpr StyleEvent kPercA[] = {
    {.step=2, .tone=kMaracas, .octave=0, .vel=62, .gate=kGateHat}, {.step=6, .tone=kMaracas, .octave=0, .vel=58, .gate=kGateHat}, {.step=10, .tone=kMaracas, .octave=0, .vel=62, .gate=kGateHat}, {.step=14, .tone=kMaracas, .octave=0, .vel=58, .gate=kGateHat},
};
inline constexpr StyleEvent kPercD[] = {
    {.step=0, .tone=kMuteHiConga, .octave=0, .vel=58, .gate=kGateHat}, {.step=2, .tone=kMaracas, .octave=0, .vel=64, .gate=kGateHat}, {.step=4, .tone=kOpenHiConga, .octave=0, .vel=62, .gate=kGateHat}, {.step=6, .tone=kMaracas, .octave=0, .vel=60, .gate=kGateHat},
    {.step=8, .tone=kMuteHiConga, .octave=0, .vel=58, .gate=kGateHat}, {.step=10, .tone=kMaracas, .octave=0, .vel=64, .gate=kGateHat}, {.step=12, .tone=kLoConga, .octave=0, .vel=62, .gate=kGateHat}, {.step=14, .tone=kMaracas, .octave=0, .vel=60, .gate=kGateHat},
};
// A synth-lead hook for the peak (varD): a diatonic answer riding over the four-
// on-the-floor. kScaleDegree keeps the line locked to the live KEY over any chord
// change. kLead role -> anchor 72, singing above the stabs.
inline constexpr std::int16_t kLeadVoice = 81;  // Lead 2 (sawtooth) — house synth hook
inline constexpr StyleEvent kLeadHook[] = {
    {.step=8,  .tone=7, .octave=0, .vel=88, .gate=kGate8th,  .src=NoteSource::kScaleDegree},   // octave
    {.step=10, .tone=4, .octave=0, .vel=82, .gate=kGate8th,  .src=NoteSource::kScaleDegree},   // 5th
    {.step=12, .tone=5, .octave=0, .vel=86, .gate=kGate8th,  .src=NoteSource::kScaleDegree},   // 6th
    {.step=14, .tone=7, .octave=0, .vel=90, .gate=kGateBeat, .src=NoteSource::kScaleDegree},   // back to the octave
};
// The house synth hook, wired only into varD.
inline constexpr MotifSpec kLeadHookMotif{.transform = MotifTransform::kDiatonicTranspose, .seed = 901};
// Motif engine (9210 Wave 1, style-depth program task A): extend variation
// onto drums/chord1/chord2/pad, wired onto the peak variation (varD) plus the
// shared reused Pad (kPadRe, varC+varD) and Chord2 (kCh2Off, varB+varD)
// arrays. kDisplacement for drums/pad/chord2 (a soft evolving push);
// kRetrograde for the chord1 organ stabs (a legible mirrored answer).
inline constexpr MotifSpec kPeakDrumsMotif{.transform = MotifTransform::kDisplacement, .seed = 910};
inline constexpr MotifSpec kPeakChordMotif{.transform = MotifTransform::kRetrograde, .seed = 911};
inline constexpr MotifSpec kSharedPadMotif{.transform = MotifTransform::kDisplacement, .seed = 912};
inline constexpr MotifSpec kSharedChord2Motif{.transform = MotifTransform::kDisplacement, .seed = 913};
inline constexpr StyleEvent kIn1D[] = {
    {.step=8, .tone=kCabasa, .octave=0, .vel=58, .gate=50}, {.step=10, .tone=kCabasa, .octave=0, .vel=64, .gate=50}, {.step=12, .tone=kCabasa, .octave=0, .vel=72, .gate=50}, {.step=14, .tone=kOpenHat, .octave=0, .vel=84, .gate=120},
    // bar 2 (style-depth Wave-2 C): arrival -- a crash announces the downbeat
    // and the four-on-the-floor kick enters under alternating open hats,
    // landing the band right where VarA/Intro2 pick up.
    {.step=16, .tone=kCrash, .octave=0, .vel=96, .gate=kGateHalfBar},
    {.step=16, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=20, .tone=kKick, .octave=0, .vel=100, .gate=120}, {.step=24, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=28, .tone=kKick, .octave=0, .vel=100, .gate=120},
    {.step=18, .tone=kOpenHat, .octave=0, .vel=78, .gate=120}, {.step=22, .tone=kOpenHat, .octave=0, .vel=78, .gate=120}, {.step=26, .tone=kOpenHat, .octave=0, .vel=78, .gate=120}, {.step=30, .tone=kOpenHat, .octave=0, .vel=84, .gate=120},
};
inline constexpr StylePattern kIn1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)}};
inline constexpr StyleEvent kIn2D[] = {{.step=0, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=4, .tone=kKick, .octave=0, .vel=108, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=12, .tone=kKick, .octave=0, .vel=108, .gate=120}, {.step=2, .tone=kOpenHat, .octave=0, .vel=74, .gate=120}, {.step=6, .tone=kOpenHat, .octave=0, .vel=74, .gate=120}, {.step=10, .tone=kOpenHat, .octave=0, .vel=74, .gate=120}, {.step=14, .tone=kOpenHat, .octave=0, .vel=74, .gate=120}};
inline constexpr StyleEvent kIn2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=74, .gate=kGateBeat}, {.step=0, .tone=kThird, .octave=0, .vel=74, .gate=kGateBeat}, {.step=0, .tone=kFifth, .octave=0, .vel=74, .gate=kGateBeat}, {.step=0, .tone=kSeventh, .octave=0, .vel=74, .gate=kGateBeat}, {.step=8, .tone=kRoot, .octave=0, .vel=72, .gate=kGateBeat}, {.step=8, .tone=kThird, .octave=0, .vel=72, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=72, .gate=kGateBeat}};
inline constexpr StylePattern kIn2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIn2C)}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadT), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}};
inline constexpr StyleEvent kAD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=114, .gate=120}, {.step=4, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=114, .gate=120}, {.step=12, .tone=kKick, .octave=0, .vel=110, .gate=120},
    {.step=4, .tone=kClap, .octave=0, .vel=96, .gate=120}, {.step=12, .tone=kClap, .octave=0, .vel=96, .gate=120},
    {.step=2, .tone=kOpenHat, .octave=0, .vel=82, .gate=120}, {.step=6, .tone=kOpenHat, .octave=0, .vel=82, .gate=120}, {.step=10, .tone=kOpenHat, .octave=0, .vel=82, .gate=120}, {.step=14, .tone=kOpenHat, .octave=0, .vel=82, .gate=120},
    {.step=0, .tone=kCabasa, .octave=0, .vel=56, .gate=50}, {.step=2, .tone=kCabasa, .octave=0, .vel=50, .gate=50}, {.step=4, .tone=kCabasa, .octave=0, .vel=56, .gate=50}, {.step=6, .tone=kCabasa, .octave=0, .vel=50, .gate=50}, {.step=8, .tone=kCabasa, .octave=0, .vel=56, .gate=50}, {.step=10, .tone=kCabasa, .octave=0, .vel=50, .gate=50}, {.step=12, .tone=kCabasa, .octave=0, .vel=56, .gate=50}, {.step=14, .tone=kCabasa, .octave=0, .vel=50, .gate=50},
    // bar 2 (style-depth Wave-2 C): the four-on-the-floor anchor stays put --
    // the turnaround pushes late instead, with an extra clap accent and the
    // open hat lifting through the last two 16ths into the repeat.
    {.step=16, .tone=kKick, .octave=0, .vel=114, .gate=120}, {.step=20, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=24, .tone=kKick, .octave=0, .vel=114, .gate=120}, {.step=28, .tone=kKick, .octave=0, .vel=110, .gate=120},
    {.step=20, .tone=kClap, .octave=0, .vel=96, .gate=120}, {.step=28, .tone=kClap, .octave=0, .vel=96, .gate=120}, {.step=30, .tone=kClap, .octave=0, .vel=84, .gate=100},
    {.step=18, .tone=kOpenHat, .octave=0, .vel=82, .gate=120}, {.step=22, .tone=kOpenHat, .octave=0, .vel=82, .gate=120}, {.step=26, .tone=kOpenHat, .octave=0, .vel=82, .gate=120}, {.step=30, .tone=kOpenHat, .octave=0, .vel=92, .gate=120}, {.step=31, .tone=kOpenHat, .octave=0, .vel=80, .gate=90},
    {.step=16, .tone=kCabasa, .octave=0, .vel=56, .gate=50}, {.step=18, .tone=kCabasa, .octave=0, .vel=50, .gate=50}, {.step=20, .tone=kCabasa, .octave=0, .vel=56, .gate=50}, {.step=22, .tone=kCabasa, .octave=0, .vel=50, .gate=50}, {.step=24, .tone=kCabasa, .octave=0, .vel=56, .gate=50}, {.step=26, .tone=kCabasa, .octave=0, .vel=50, .gate=50}, {.step=28, .tone=kCabasa, .octave=0, .vel=56, .gate=50}, {.step=29, .tone=kCabasa, .octave=0, .vel=62, .gate=50},
};
inline constexpr StyleEvent kAB[] = {
    {.step=2, .tone=kRoot, .octave=0, .vel=100, .gate=kGate8th}, {.step=6, .tone=kRoot, .octave=0, .vel=96, .gate=kGate8th}, {.step=10, .tone=kFifth, .octave=0, .vel=98, .gate=kGate8th}, {.step=14, .tone=kRoot, .octave=0, .vel=96, .gate=kGate8th},
    // bar 2 (style-depth Wave-2 C, VarA only -- VarC still reads bar 1): an
    // octave-up push then the 7th leading back into the repeat, a real answer
    // to bar 1's flat root/fifth alternation.
    {.step=18, .tone=kRoot, .octave=0, .vel=100, .gate=kGate8th}, {.step=22, .tone=kRoot, .octave=1, .vel=92, .gate=kGate8th}, {.step=26, .tone=kFifth, .octave=0, .vel=98, .gate=kGate8th}, {.step=30, .tone=kSeventh, .octave=0, .vel=90, .gate=kGate8th},
};
inline constexpr StyleEvent kAC[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=80, .gate=kGateBeat}, {.step=0, .tone=kThird, .octave=0, .vel=80, .gate=kGateBeat}, {.step=0, .tone=kFifth, .octave=0, .vel=80, .gate=kGateBeat}, {.step=0, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateBeat}, {.step=8, .tone=kRoot, .octave=0, .vel=78, .gate=kGateBeat}, {.step=8, .tone=kThird, .octave=0, .vel=78, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=78, .gate=kGateBeat}, {.step=8, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateBeat},
    // bar 2 (style-depth Wave-2 C): the organ stabs answer syncopated -- off
    // the beat instead of square on it, a genuine call-and-response.
    {.step=18, .tone=kRoot, .octave=0, .vel=78, .gate=kGateBeat}, {.step=18, .tone=kThird, .octave=0, .vel=78, .gate=kGateBeat}, {.step=18, .tone=kFifth, .octave=0, .vel=78, .gate=kGateBeat}, {.step=18, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateBeat}, {.step=26, .tone=kRoot, .octave=0, .vel=76, .gate=kGateBeat}, {.step=26, .tone=kThird, .octave=0, .vel=76, .gate=kGateBeat}, {.step=26, .tone=kFifth, .octave=0, .vel=76, .gate=kGateBeat}, {.step=26, .tone=kSeventh, .octave=0, .vel=76, .gate=kGateBeat},
};
inline constexpr StylePattern kAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAB)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAC), .voicing=VoicingPolicy::kLead},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadT), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCh2A), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead}};
inline constexpr StyleEvent kBD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=116, .gate=120}, {.step=4, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=116, .gate=120}, {.step=12, .tone=kKick, .octave=0, .vel=112, .gate=120},
    {.step=4, .tone=kClap, .octave=0, .vel=100, .gate=120}, {.step=12, .tone=kClap, .octave=0, .vel=100, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=88, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=88, .gate=120},
    {.step=2, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}, {.step=6, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}, {.step=10, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}, {.step=14, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}, {.step=15, .tone=kOpenHat, .octave=0, .vel=70, .gate=120},
    {.step=0, .tone=kCabasa, .octave=0, .vel=58, .gate=50}, {.step=1, .tone=kCabasa, .octave=0, .vel=48, .gate=50}, {.step=2, .tone=kCabasa, .octave=0, .vel=54, .gate=50}, {.step=3, .tone=kCabasa, .octave=0, .vel=48, .gate=50}, {.step=4, .tone=kCabasa, .octave=0, .vel=58, .gate=50}, {.step=6, .tone=kCabasa, .octave=0, .vel=54, .gate=50}, {.step=8, .tone=kCabasa, .octave=0, .vel=58, .gate=50}, {.step=10, .tone=kCabasa, .octave=0, .vel=54, .gate=50}, {.step=12, .tone=kCabasa, .octave=0, .vel=58, .gate=50}, {.step=14, .tone=kCabasa, .octave=0, .vel=54, .gate=50},
};
inline constexpr StyleEvent kBB[] = {{.step=2, .tone=kRoot, .octave=0, .vel=104, .gate=kGate8th}, {.step=6, .tone=kThird, .octave=0, .vel=96, .gate=kGate8th}, {.step=8, .tone=kFifth, .octave=0, .vel=98, .gate=kGate8th}, {.step=10, .tone=kFifth, .octave=0, .vel=94, .gate=kGate8th}, {.step=14, .tone=kSeventh, .octave=0, .vel=96, .gate=kGate8th}};
inline constexpr StyleEvent kBC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=82, .gate=kGateStab}, {.step=0, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=82, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStab}, {.step=6, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=82, .gate=kGateStab}, {.step=8, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStab}, {.step=8, .tone=kSeventh, .octave=0, .vel=82, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStab}, {.step=14, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStab}};
inline constexpr StylePattern kBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBB)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBC), .voicing=VoicingPolicy::kLead},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadT), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCh2Off), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead, .motif=&kSharedChord2Motif}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercA)}};
inline constexpr StyleEvent kFAD[] = {{.step=0, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=4, .tone=kClap, .octave=0, .vel=100, .gate=120}, {.step=8, .tone=kClap, .octave=0, .vel=94, .gate=120}, {.step=10, .tone=kClap, .octave=0, .vel=98, .gate=120}, {.step=12, .tone=kOpenHat, .octave=0, .vel=92, .gate=120}, {.step=14, .tone=kOpenHat, .octave=0, .vel=100, .gate=120}};
inline constexpr StyleEvent kFBD[] = {{.step=0, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=96, .gate=100}, {.step=6, .tone=kCabasa, .octave=0, .vel=70, .gate=50}, {.step=8, .tone=kSnare, .octave=0, .vel=92, .gate=100}, {.step=10, .tone=kSnare, .octave=0, .vel=98, .gate=100}, {.step=12, .tone=kClap, .octave=0, .vel=104, .gate=100}, {.step=14, .tone=kOpenHat, .octave=0, .vel=100, .gate=120}};
inline constexpr StyleEvent kFCD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=92, .gate=100}, {.step=2, .tone=kClap, .octave=0, .vel=90, .gate=100}, {.step=4, .tone=kSnare, .octave=0, .vel=98, .gate=100}, {.step=6, .tone=kClap, .octave=0, .vel=96, .gate=100}, {.step=8, .tone=kSnare, .octave=0, .vel=102, .gate=100}, {.step=10, .tone=kClap, .octave=0, .vel=100, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=108, .gate=100}, {.step=14, .tone=kClap, .octave=0, .vel=112, .gate=100}};
inline constexpr StyleEvent kFDD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=90, .gate=50}, {.step=1, .tone=kClap, .octave=0, .vel=86, .gate=50}, {.step=2, .tone=kSnare, .octave=0, .vel=94, .gate=50}, {.step=3, .tone=kClap, .octave=0, .vel=90, .gate=50}, {.step=4, .tone=kSnare, .octave=0, .vel=98, .gate=50}, {.step=5, .tone=kClap, .octave=0, .vel=94, .gate=50}, {.step=6, .tone=kSnare, .octave=0, .vel=100, .gate=50}, {.step=7, .tone=kClap, .octave=0, .vel=96, .gate=50}, {.step=8, .tone=kSnare, .octave=0, .vel=104, .gate=50}, {.step=9, .tone=kClap, .octave=0, .vel=100, .gate=50}, {.step=10, .tone=kSnare, .octave=0, .vel=106, .gate=50}, {.step=11, .tone=kClap, .octave=0, .vel=102, .gate=50}, {.step=12, .tone=kSnare, .octave=0, .vel=110, .gate=50}, {.step=13, .tone=kSnare, .octave=0, .vel=114, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=118, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=120, .gate=kGate8th}};
inline constexpr StylePattern kFAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass), .motif=&kFillBassMotif}};
inline constexpr StylePattern kFBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass), .motif=&kFillBassMotif}};
inline constexpr StylePattern kFCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass), .motif=&kFillBassMotif}};
inline constexpr StylePattern kFDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass), .motif=&kFillBassMotif}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercD)}};
inline constexpr StyleEvent kE1D[] = {{.step=0, .tone=kKick, .octave=0, .vel=114, .gate=120}, {.step=0, .tone=kCrash, .octave=0, .vel=104, .gate=kGateHalfBar}};
inline constexpr StyleEvent kE1B[] = {{.step=0, .tone=kRoot, .octave=0, .vel=104, .gate=kGateHeld}};
inline constexpr StyleEvent kE1C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=84, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=84, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=84, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateHeld}};
inline constexpr StylePattern kE1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1C)}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}};
inline constexpr StyleEvent kE2D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=112, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=116, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=8, .tone=kClap, .octave=0, .vel=98, .gate=120}};
inline constexpr StyleEvent kE2B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=108, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=96, .gate=kGateHeld}};
inline constexpr StyleEvent kE2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=86, .gate=kGateHeld}};
inline constexpr StylePattern kE2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2C)}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}};
// varC: deep-house stripped — four-on-the-floor, off-beat open hats, sustained 7th pad.
inline constexpr StyleEvent kCD[] = {{.step=0, .tone=kKick, .octave=0, .vel=114, .gate=120}, {.step=4, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=114, .gate=120}, {.step=12, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=2, .tone=kOpenHat, .octave=0, .vel=82, .gate=120}, {.step=6, .tone=kOpenHat, .octave=0, .vel=82, .gate=120}, {.step=10, .tone=kOpenHat, .octave=0, .vel=82, .gate=120}, {.step=14, .tone=kOpenHat, .octave=0, .vel=82, .gate=120}};
inline constexpr StyleEvent kCC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=74, .gate=kGateBeat}, {.step=0, .tone=kThird, .octave=0, .vel=74, .gate=kGateBeat}, {.step=0, .tone=kFifth, .octave=0, .vel=74, .gate=kGateBeat}, {.step=0, .tone=kSeventh, .octave=0, .vel=74, .gate=kGateBeat}, {.step=8, .tone=kRoot, .octave=0, .vel=72, .gate=kGateBeat}, {.step=8, .tone=kThird, .octave=0, .vel=72, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=72, .gate=kGateBeat}, {.step=8, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateBeat}};
inline constexpr StylePattern kCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAB)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCC), .voicing=VoicingPolicy::kLead},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadRe), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead, .motif=&kSharedPadMotif}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCh2A), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArp8), .gm_program=kArpVoice}};
// varD: peak — clap+snare backbeat, off-beat opens, 16th cabasa carpet, full 7th stabs.
inline constexpr StyleEvent kDD[] = {{.step=0, .tone=kKick, .octave=0, .vel=116, .gate=120}, {.step=4, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=116, .gate=120}, {.step=12, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=4, .tone=kClap, .octave=0, .vel=100, .gate=120}, {.step=12, .tone=kClap, .octave=0, .vel=100, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=88, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=88, .gate=120}, {.step=2, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}, {.step=6, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}, {.step=10, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}, {.step=14, .tone=kOpenHat, .octave=0, .vel=84, .gate=120}, {.step=0, .tone=kCabasa, .octave=0, .vel=58, .gate=50}, {.step=1, .tone=kCabasa, .octave=0, .vel=48, .gate=50}, {.step=2, .tone=kCabasa, .octave=0, .vel=54, .gate=50}, {.step=3, .tone=kCabasa, .octave=0, .vel=48, .gate=50}, {.step=4, .tone=kCabasa, .octave=0, .vel=58, .gate=50}, {.step=5, .tone=kCabasa, .octave=0, .vel=48, .gate=50}, {.step=6, .tone=kCabasa, .octave=0, .vel=54, .gate=50}, {.step=7, .tone=kCabasa, .octave=0, .vel=48, .gate=50}, {.step=8, .tone=kCabasa, .octave=0, .vel=58, .gate=50}, {.step=9, .tone=kCabasa, .octave=0, .vel=48, .gate=50}, {.step=10, .tone=kCabasa, .octave=0, .vel=54, .gate=50}, {.step=11, .tone=kCabasa, .octave=0, .vel=48, .gate=50}, {.step=12, .tone=kCabasa, .octave=0, .vel=58, .gate=50}, {.step=13, .tone=kCabasa, .octave=0, .vel=48, .gate=50}, {.step=14, .tone=kCabasa, .octave=0, .vel=54, .gate=50}, {.step=15, .tone=kCabasa, .octave=0, .vel=48, .gate=50}};
inline constexpr StyleEvent kDC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=0, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStab}, {.step=6, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=8, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=8, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStab}, {.step=14, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStab}};
inline constexpr StylePattern kDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kDD), .motif=&kPeakDrumsMotif}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBB)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kDC), .voicing=VoicingPolicy::kLead, .motif=&kPeakChordMotif},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadRe), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead, .motif=&kSharedPadMotif}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCh2Off), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead, .motif=&kSharedChord2Motif}, {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArp16), .gm_program=kArpVoice}, {.role=TrackRole::kLead, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kLeadHook), .gm_program=kLeadVoice, .motif=&kLeadHookMotif}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercD)}};
// Style-depth Wave-2 C: VarB/VarC stay 1 bar -- deep/off-beat house repeats by
// design and neither has a natural bar-2 idea beyond what VarA above already
// carries. VarD (the peak) stays 1 bar too: every melodic/drum lane there
// already carries a motif-engine transform (kPeakDrumsMotif, kPeakChordMotif,
// kSharedPadMotif, kSharedChord2Motif, kLeadHookMotif) whose retrograde/
// displacement math assumes every event's .step is in [0,15]; extending any
// of those arrays into a real bar 2 would silently break that assumption
// (motif-engine hazard).
inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=2, .patterns=Span<const StylePattern>(kIn1P)}, {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIn2P)},
    {.type=SectionType::kVarA, .bars=2, .patterns=Span<const StylePattern>(kAP)}, {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kBP)},
    {.type=SectionType::kVarC, .bars=1, .patterns=Span<const StylePattern>(kCP)}, {.type=SectionType::kVarD, .bars=1, .patterns=Span<const StylePattern>(kDP)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFAP)}, {.type=SectionType::kFillB, .bars=1, .patterns=Span<const StylePattern>(kFBP)}, {.type=SectionType::kFillC, .bars=1, .patterns=Span<const StylePattern>(kFCP)}, {.type=SectionType::kFillD, .bars=1, .patterns=Span<const StylePattern>(kFDP)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kE1P)}, {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kE2P)},
};
// Groove (9100 Wave 1, task B): tight, quantized four-on-the-floor genre --
// the lightest deterministic humanize in the corpus, alongside disco.
inline constexpr Style kStyle{.name="house", .sections=Span<const StyleSection>(kSections),
                              .groove={.humanize_timing=4, .humanize_velocity=10}, .tempo=12800};
}  // namespace house

}  // namespace styles
}  // namespace arrangrr
