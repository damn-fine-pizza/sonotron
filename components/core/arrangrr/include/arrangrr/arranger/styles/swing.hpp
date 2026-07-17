#pragma once

#include "arrangrr/arranger/style_model.hpp"

namespace arrangrr {
namespace styles {


// ---------------------------------------------------------------------------
// "swing": jazz ride pattern with a swung feel, pedal-hat on 2 & 4, feathered
// kick, walking quarter-note bass and sparse rootless off-beat comping.
namespace swing {
// Held root pickup for Intro1. Re-articulated at step 16 too (style-depth
// Wave-2 C): Intro1 is now 2 bars, and the sustain re-triggers every bar
// exactly as it always has when the section looped -- a background hold, not
// the section's own bar-to-bar variation (same idiom as basic's kPadTriad).
inline constexpr StyleEvent kHeldBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=84, .gate=kGateHeld},
    {.step=16, .tone=kRoot, .octave=0, .vel=82, .gate=kGateHeld},
};
inline constexpr StyleEvent kWalkBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=86, .gate=kGateBeat}, {.step=4, .tone=kThird, .octave=0, .vel=80, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=84, .gate=kGateBeat}, {.step=12, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateBeat}};
// Wave-2.5 (item 2, VarD only): the same 4 quarter-note walk, extended with a
// chromatic half-step approach note (kInterval, one semitone below the chord
// root) landing late in the bar to walk into the repeat's downbeat -- the
// standard "walk into the turnaround" bebop idiom. Every other section keeps
// the plain kWalkBass above, untouched.
inline constexpr StyleEvent kWalkBassPeak[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=86, .gate=kGateBeat},
    {.step=4, .tone=kThird, .octave=0, .vel=80, .gate=kGateBeat},
    {.step=8, .tone=kFifth, .octave=0, .vel=84, .gate=kGateBeat},
    {.step=12, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateBeat},
    {.step=14, .tone=-1, .octave=0, .vel=70, .gate=kGateStaccato, .src=NoteSource::kInterval},  // chromatic half-step approach into the repeat's root
};
// Motif engine (9210, Ottorino RANK 3): the walking quarter-note bass,
// reused 9x. kRetrograde maps the four on-the-beat quarters to the four
// "and" 16ths just before the next beat — a real jazz-bass device
// (alternating ascending/mirrored-descending walk), and needs no amount so
// the alternation stays stable.
inline constexpr MotifSpec kWalkBassMotif{.transform = MotifTransform::kRetrograde, .seed = 301};
// The bebop shout-chorus sax answer, wired only into varD.
inline constexpr MotifSpec kLeadLickMotif{.transform = MotifTransform::kDiatonicTranspose, .seed = 302};
// Motif engine (9210 Wave 1, style-depth program task A): extend variation
// onto drums/chord1/chord2/pad, wired onto the peak variation (varD) plus the
// shared reused Pad (kPadRe, varC+varD) and Chord2 (kCh2B, varB+varD)
// arrays. kDisplacement for drums/pad/chord2 (a soft evolving push);
// kRetrograde for the chord1 rootless comp (a legible mirrored answer, the
// same jazz-comp displacement idiom already proven on the walking bass
// above).
inline constexpr MotifSpec kPeakDrumsMotif{.transform = MotifTransform::kDisplacement, .seed = 310};
inline constexpr MotifSpec kPeakChordMotif{.transform = MotifTransform::kRetrograde, .seed = 311};
inline constexpr MotifSpec kSharedPadMotif{.transform = MotifTransform::kDisplacement, .seed = 312};
inline constexpr MotifSpec kSharedChord2Motif{.transform = MotifTransform::kDisplacement, .seed = 313};

// Fuller-band roles (ADDITIVE): a soft string bed, a warm E.piano comp, a
// vibraphone arp and a whisper of tambourine. Pad and comp are ROOTLESS
// (third/fifth/seventh) so they sit above the walking bass. kPad->GM48
// (Strings), kChord2->GM4 (E.Piano1), kArp->GM11 (Vibraphone); kPerc rides the
// drum channel. Pad holds register 48 under the whole bar.
inline constexpr std::int16_t kPadVoice = 48;
inline constexpr std::int16_t kChord2Voice = 4;
inline constexpr std::int16_t kArpVoice = 11;
inline constexpr std::int16_t kLeadVoice = 66;  // Tenor Sax — the bebop horn answer
// Rootless pad: hold the upper structure under the whole bar. Re-articulated
// at step 16 too (style-depth Wave-2 C): VarA is now 2 bars, and the sustain
// re-triggers every bar exactly as it always has when the section looped --
// a background hold, not the section's own bar-to-bar variation (kPadT is
// unmotifed in every use here, so this is safe -- basic.hpp's own kPadTriad).
inline constexpr StyleEvent kPadT[] = {
    {.step=0, .tone=kThird, .octave=0, .vel=46, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=44, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=46, .gate=kGateHeld},
    {.step=16, .tone=kThird, .octave=0, .vel=46, .gate=kGateHeld}, {.step=16, .tone=kFifth, .octave=0, .vel=44, .gate=kGateHeld}, {.step=16, .tone=kSeventh, .octave=0, .vel=46, .gate=kGateHeld},
};
inline constexpr StyleEvent kPad7[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=48, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=46, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=48, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=44, .gate=kGateHeld},
};
inline constexpr StyleEvent kPadRe[] = {
    {.step=0, .tone=kThird, .octave=0, .vel=48, .gate=kGateHalfBar}, {.step=0, .tone=kFifth, .octave=0, .vel=46, .gate=kGateHalfBar}, {.step=0, .tone=kSeventh, .octave=0, .vel=48, .gate=kGateHalfBar},
    {.step=8, .tone=kThird, .octave=0, .vel=46, .gate=kGateHalfBar}, {.step=8, .tone=kFifth, .octave=0, .vel=44, .gate=kGateHalfBar}, {.step=8, .tone=kSeventh, .octave=0, .vel=46, .gate=kGateHalfBar},
};
// E.piano comp: rootless off-beat cushions between chord1's stabs.
inline constexpr StyleEvent kCh2A[] = {
    {.step=2, .tone=kThird, .octave=0, .vel=58, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=58, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=56, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=56, .gate=kGateStab},
};
inline constexpr StyleEvent kCh2B[] = {
    {.step=0, .tone=kThird, .octave=0, .vel=58, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=58, .gate=kGateStab}, {.step=4, .tone=kFifth, .octave=0, .vel=54, .gate=kGateStab}, {.step=4, .tone=kSeventh, .octave=0, .vel=54, .gate=kGateStab},
    {.step=8, .tone=kThird, .octave=0, .vel=58, .gate=kGateStab}, {.step=8, .tone=kSeventh, .octave=0, .vel=58, .gate=kGateStab}, {.step=12, .tone=kFifth, .octave=0, .vel=54, .gate=kGateStab}, {.step=12, .tone=kSeventh, .octave=0, .vel=54, .gate=kGateStab},
};
// Vibraphone arp: gentle swung eighths climbing the chord.
inline constexpr StyleEvent kArp8[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=58, .gate=kGate8th}, {.step=2, .tone=kThird, .octave=0, .vel=52, .gate=kGate8th}, {.step=4, .tone=kFifth, .octave=0, .vel=56, .gate=kGate8th}, {.step=6, .tone=kSeventh, .octave=0, .vel=52, .gate=kGate8th},
    {.step=8, .tone=kRoot, .octave=1, .vel=58, .gate=kGate8th}, {.step=10, .tone=kSeventh, .octave=0, .vel=52, .gate=kGate8th}, {.step=12, .tone=kFifth, .octave=0, .vel=56, .gate=kGate8th}, {.step=14, .tone=kThird, .octave=0, .vel=52, .gate=kGate8th},
};
// Percussion: soft tambourine on 2 & 4 (varB) and light off-beats (varD).
inline constexpr StyleEvent kPercA[] = {
    {.step=4, .tone=kTambourine, .octave=0, .vel=50, .gate=kGateHat}, {.step=12, .tone=kTambourine, .octave=0, .vel=52, .gate=kGateHat},
};
inline constexpr StyleEvent kPercD[] = {
    {.step=2, .tone=kTambourine, .octave=0, .vel=48, .gate=kGateHat}, {.step=6, .tone=kTambourine, .octave=0, .vel=46, .gate=kGateHat}, {.step=10, .tone=kTambourine, .octave=0, .vel=48, .gate=kGateHat}, {.step=14, .tone=kTambourine, .octave=0, .vel=46, .gate=kGateHat},
};
// A sax answer in the back half of the bar (shout chorus). kScaleDegree keeps
// the line diatonic to the live KEY; the step-11 grace is a kInterval blue b3
// (3 semitones off the chord ROOT) that resolves chromatically UP into the
// chord's major 3rd on step 12 — the bebop half-step approach. kLead -> 72.
inline constexpr StyleEvent kLeadLick[] = {
    {.step=8,  .tone=4, .octave=0, .vel=74, .gate=kGate8th,      .src=NoteSource::kScaleDegree},   // 5th of key
    {.step=10, .tone=6, .octave=0, .vel=68, .gate=kGate8th,      .src=NoteSource::kScaleDegree},   // 7th of key
    {.step=11, .tone=3, .octave=0, .vel=64, .gate=kGateStaccato, .src=NoteSource::kInterval},      // chromatic b3 off the chord root
    {.step=12, .tone=4, .octave=0, .vel=76, .gate=kGate8th,      .src=NoteSource::kInterval},      // resolves up to the chord's major 3rd
    {.step=14, .tone=0, .octave=0, .vel=72, .gate=kGateBeat,     .src=NoteSource::kScaleDegree},   // land on the tonic
};
// Intro1 (style-depth Wave-2 C): a genuine 2-bar build. Bar 1 (unchanged) is
// the sparse ride pickup on the last beat; bar 2 is the arrival -- the ride
// fills out and the pedal-hat (2 & 4) enters, landing the band right where
// Intro2/VarA starts.
inline constexpr StyleEvent kIn1D[] = {
    {.step=8, .tone=kRide, .octave=0, .vel=56, .gate=120}, {.step=10, .tone=kRide, .octave=0, .vel=50, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=60, .gate=120}, {.step=14, .tone=kRide, .octave=0, .vel=54, .gate=120},
    // bar 2: arrival -- fuller ride, pedal-hat enters on 2 & 4.
    {.step=16, .tone=kRide, .octave=0, .vel=62, .gate=120}, {.step=20, .tone=kRide, .octave=0, .vel=56, .gate=120}, {.step=22, .tone=kRide, .octave=0, .vel=50, .gate=120},
    {.step=24, .tone=kRide, .octave=0, .vel=64, .gate=120}, {.step=28, .tone=kRide, .octave=0, .vel=56, .gate=120}, {.step=30, .tone=kRide, .octave=0, .vel=50, .gate=120},
    {.step=20, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=28, .tone=kClosedHat, .octave=0, .vel=56, .gate=50},
};
inline constexpr StylePattern kIn1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)}};
inline constexpr StyleEvent kIn2D[] = {{.step=0, .tone=kRide, .octave=0, .vel=64, .gate=120}, {.step=4, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=6, .tone=kRide, .octave=0, .vel=52, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=62, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=14, .tone=kRide, .octave=0, .vel=52, .gate=120}, {.step=4, .tone=kClosedHat, .octave=0, .vel=54, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=54, .gate=50}};
inline constexpr StyleEvent kIn2C[] = {{.step=6, .tone=kThird, .octave=0, .vel=68, .gate=kGateStab}, {.step=6, .tone=kSeventh, .octave=0, .vel=68, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=66, .gate=kGateStab}, {.step=14, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateStab}};
inline constexpr StylePattern kIn2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kWalkBass), .motif=&kWalkBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIn2C), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadT), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}};
// VarA (style-depth Wave-2 C): genuinely 2 bars now. Bar 1 is unchanged. Bar 2
// keeps the ride/hat time-keeping steady (a real jazz ride rarely changes bar
// to bar -- same idiom as basic's re-triggered pad, not this bar's own
// variation); the REAL bar-2 idea is a soft kick "bomb" and a snare ghost
// note marking the turnaround, plus the rootless comp answering on a
// different offbeat pair. The walking bass (kWalkBass) is motif-locked
// (kWalkBassMotif) and is this style's only bass idiom, so it is
// deliberately left untouched here (guardrail: vary drums/comp, not the bass).
inline constexpr StyleEvent kAD[] = {
    {.step=0, .tone=kRide, .octave=0, .vel=72, .gate=120}, {.step=4, .tone=kRide, .octave=0, .vel=64, .gate=120}, {.step=6, .tone=kRide, .octave=0, .vel=56, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=70, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=64, .gate=120}, {.step=14, .tone=kRide, .octave=0, .vel=56, .gate=120},
    {.step=4, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=0, .tone=kKick, .octave=0, .vel=40, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=38, .gate=50},
    // bar 2: the ride/hat pattern repeats verbatim (shifted +16, steady time-
    // keeping); a soft kick "bomb" and a snare ghost note mark the turnaround.
    {.step=16, .tone=kRide, .octave=0, .vel=72, .gate=120}, {.step=20, .tone=kRide, .octave=0, .vel=64, .gate=120}, {.step=22, .tone=kRide, .octave=0, .vel=56, .gate=120}, {.step=24, .tone=kRide, .octave=0, .vel=70, .gate=120}, {.step=28, .tone=kRide, .octave=0, .vel=64, .gate=120}, {.step=30, .tone=kRide, .octave=0, .vel=56, .gate=120},
    {.step=20, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=28, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=16, .tone=kKick, .octave=0, .vel=40, .gate=50}, {.step=30, .tone=kKick, .octave=0, .vel=44, .gate=50}, {.step=31, .tone=kSnare, .octave=0, .vel=46, .gate=50},
};
inline constexpr StyleEvent kAC[] = {
    {.step=6, .tone=kThird, .octave=0, .vel=70, .gate=kGateStab}, {.step=6, .tone=kSeventh, .octave=0, .vel=70, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=68, .gate=kGateStab}, {.step=14, .tone=kSeventh, .octave=0, .vel=68, .gate=kGateStab},
    // bar 2: the rootless comp answers on a different offbeat pair -- a
    // turnaround comping idea, not a repeat of bar 1's stabs.
    {.step=18, .tone=kThird, .octave=0, .vel=68, .gate=kGateStab}, {.step=18, .tone=kSeventh, .octave=0, .vel=68, .gate=kGateStab}, {.step=27, .tone=kThird, .octave=0, .vel=66, .gate=kGateStab}, {.step=27, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateStab},
};
inline constexpr StylePattern kAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kWalkBass), .motif=&kWalkBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAC), .voicing=VoicingPolicy::kLead},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadT), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCh2A), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead}};
// VarB left at 1 bar (style-depth Wave-2 C): no genuine bar-2 idea beyond
// VarA's own turnaround without touching the motif-locked walking bass.
inline constexpr StyleEvent kBD[] = {
    {.step=0, .tone=kRide, .octave=0, .vel=76, .gate=120}, {.step=2, .tone=kRide, .octave=0, .vel=54, .gate=120}, {.step=4, .tone=kRide, .octave=0, .vel=68, .gate=120}, {.step=6, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=74, .gate=120}, {.step=10, .tone=kRide, .octave=0, .vel=54, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=68, .gate=120}, {.step=14, .tone=kRide, .octave=0, .vel=58, .gate=120},
    {.step=4, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=0, .tone=kKick, .octave=0, .vel=44, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=42, .gate=50}, {.step=6, .tone=kSnare, .octave=0, .vel=52, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=56, .gate=50},
};
inline constexpr StyleEvent kBC[] = {{.step=2, .tone=kThird, .octave=0, .vel=68, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=68, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=72, .gate=kGateStab}, {.step=6, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=66, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=70, .gate=kGateStab}, {.step=14, .tone=kSeventh, .octave=0, .vel=70, .gate=kGateStab}};
inline constexpr StylePattern kBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kWalkBass), .motif=&kWalkBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBC), .voicing=VoicingPolicy::kLead},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadT), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCh2B), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead, .motif=&kSharedChord2Motif}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercA)}};
inline constexpr StyleEvent kFAD[] = {{.step=0, .tone=kRide, .octave=0, .vel=70, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=76, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=70, .gate=120}, {.step=10, .tone=kSnare, .octave=0, .vel=74, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=80, .gate=120}, {.step=14, .tone=kSnare, .octave=0, .vel=86, .gate=120}};
inline constexpr StyleEvent kFBD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=76, .gate=100}, {.step=2, .tone=kSnare, .octave=0, .vel=70, .gate=100}, {.step=4, .tone=kTomHi, .octave=0, .vel=82, .gate=100}, {.step=6, .tone=kTomMid, .octave=0, .vel=80, .gate=100}, {.step=8, .tone=kSnare, .octave=0, .vel=84, .gate=100}, {.step=10, .tone=kTomLow, .octave=0, .vel=86, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=90, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=94, .gate=100}};
inline constexpr StyleEvent kFCD[] = {{.step=0, .tone=kTomHi, .octave=0, .vel=80, .gate=100}, {.step=2, .tone=kTomHi, .octave=0, .vel=76, .gate=100}, {.step=4, .tone=kSnare, .octave=0, .vel=84, .gate=100}, {.step=6, .tone=kSnare, .octave=0, .vel=80, .gate=100}, {.step=8, .tone=kTomMid, .octave=0, .vel=88, .gate=100}, {.step=10, .tone=kTomLow, .octave=0, .vel=90, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=94, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=98, .gate=100}};
inline constexpr StyleEvent kFDD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=80, .gate=50}, {.step=2, .tone=kSnare, .octave=0, .vel=74, .gate=50}, {.step=3, .tone=kTomHi, .octave=0, .vel=84, .gate=50}, {.step=4, .tone=kTomHi, .octave=0, .vel=88, .gate=50}, {.step=6, .tone=kTomMid, .octave=0, .vel=86, .gate=50}, {.step=7, .tone=kTomMid, .octave=0, .vel=90, .gate=50}, {.step=8, .tone=kTomLow, .octave=0, .vel=92, .gate=50}, {.step=10, .tone=kTomLow, .octave=0, .vel=88, .gate=50}, {.step=11, .tone=kTomFloor, .octave=0, .vel=94, .gate=50}, {.step=12, .tone=kSnare, .octave=0, .vel=98, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=104, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=110, .gate=kGate8th}};
inline constexpr StylePattern kFAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kWalkBass), .motif=&kWalkBassMotif}};
inline constexpr StylePattern kFBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kWalkBass), .motif=&kWalkBassMotif}};
inline constexpr StylePattern kFCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kWalkBass), .motif=&kWalkBassMotif}};
inline constexpr StylePattern kFDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kWalkBass), .motif=&kWalkBassMotif}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercD)}};
inline constexpr StyleEvent kE1D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=80, .gate=kGateHalfBar}, {.step=0, .tone=kKick, .octave=0, .vel=70, .gate=50}};
inline constexpr StyleEvent kE1B[] = {{.step=0, .tone=kRoot, .octave=0, .vel=84, .gate=kGateHeld}};
inline constexpr StyleEvent kE1C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=74, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=74, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=74, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=74, .gate=kGateHeld}};
inline constexpr StylePattern kE1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1C), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}};
inline constexpr StyleEvent kE2D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=74, .gate=50}, {.step=8, .tone=kRide, .octave=0, .vel=62, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=58, .gate=120}};
inline constexpr StyleEvent kE2B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=78, .gate=kGateHeld}};
inline constexpr StyleEvent kE2C[] = {{.step=0, .tone=kThird, .octave=0, .vel=76, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=76, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=76, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=1, .vel=72, .gate=kGateHeld}};
inline constexpr StylePattern kE2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2C), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}};
// varC: two-feel — laid-back quarter ride, feathered kick, pedal hat, sparse comp.
// VarC left at 1 bar (style-depth Wave-2 C): the two-feel/laid-back groove
// is a single sustained idea -- a second bar would just repeat it.
inline constexpr StyleEvent kCD[] = {{.step=0, .tone=kRide, .octave=0, .vel=68, .gate=120}, {.step=4, .tone=kRide, .octave=0, .vel=60, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=66, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=60, .gate=120}, {.step=0, .tone=kKick, .octave=0, .vel=42, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=40, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=54, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=54, .gate=50}};
inline constexpr StyleEvent kCC[] = {{.step=2, .tone=kThird, .octave=0, .vel=68, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=68, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=66, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateStab}};
inline constexpr StylePattern kCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kWalkBass), .motif=&kWalkBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCC), .voicing=VoicingPolicy::kLead},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadRe), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead, .motif=&kSharedPadMotif}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCh2A), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArp8), .gm_program=kArpVoice}};
// varD: shout chorus — busy swung ride, hats on 2 & 4, kick bombs, snare comps, full rootless 7ths.
// VarD left at 1 bar (style-depth Wave-2 C): already the shout-chorus peak
// (motif-driven drums/chord1/lead-lick); no clean additional idea for a
// second bar.
inline constexpr StyleEvent kDD[] = {{.step=0, .tone=kRide, .octave=0, .vel=78, .gate=120}, {.step=2, .tone=kRide, .octave=0, .vel=56, .gate=120}, {.step=4, .tone=kRide, .octave=0, .vel=72, .gate=120}, {.step=6, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=76, .gate=120}, {.step=10, .tone=kRide, .octave=0, .vel=56, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=72, .gate=120}, {.step=14, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=4, .tone=kClosedHat, .octave=0, .vel=62, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=62, .gate=50}, {.step=0, .tone=kKick, .octave=0, .vel=50, .gate=50}, {.step=4, .tone=kKick, .octave=0, .vel=44, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=48, .gate=50}, {.step=12, .tone=kKick, .octave=0, .vel=44, .gate=50}, {.step=2, .tone=kSnare, .octave=0, .vel=54, .gate=50}, {.step=6, .tone=kSnare, .octave=0, .vel=58, .gate=50}, {.step=10, .tone=kSnare, .octave=0, .vel=54, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=60, .gate=50}};
inline constexpr StyleEvent kDC[] = {{.step=2, .tone=kThird, .octave=0, .vel=74, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=74, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=74, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=72, .gate=kGateStab}, {.step=6, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=74, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=74, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=74, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=72, .gate=kGateStab}, {.step=14, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}};
inline constexpr StylePattern kDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kDD), .motif=&kPeakDrumsMotif}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kWalkBassPeak), .motif=&kWalkBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kDC), .voicing=VoicingPolicy::kLead, .motif=&kPeakChordMotif},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadRe), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead, .motif=&kSharedPadMotif}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCh2B), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead, .motif=&kSharedChord2Motif}, {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArp8), .gm_program=kArpVoice}, {.role=TrackRole::kLead, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kLeadLick), .gm_program=kLeadVoice, .motif=&kLeadLickMotif}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercD)}};
inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=2, .patterns=Span<const StylePattern>(kIn1P)}, {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIn2P)},
    {.type=SectionType::kVarA, .bars=2, .patterns=Span<const StylePattern>(kAP)}, {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kBP)},
    {.type=SectionType::kVarC, .bars=1, .patterns=Span<const StylePattern>(kCP)}, {.type=SectionType::kVarD, .bars=1, .patterns=Span<const StylePattern>(kDP)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFAP)}, {.type=SectionType::kFillB, .bars=1, .patterns=Span<const StylePattern>(kFBP)}, {.type=SectionType::kFillC, .bars=1, .patterns=Span<const StylePattern>(kFCP)}, {.type=SectionType::kFillD, .bars=1, .patterns=Span<const StylePattern>(kFDP)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kE1P)}, {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kE2P)},
};
// Groove (9100 Wave 1, task B): jazz swing feel -- heavy deterministic
// humanize on top of the existing swing/accent feel, one of the loosest
// genres named alongside blues/latin.
inline constexpr Style kStyle{.name="swing", .sections=Span<const StyleSection>(kSections), .groove={.swing=62, .humanize_timing=18, .humanize_velocity=26, .accent=12, .swing_grid=8}, .tempo=14000};
}  // namespace swing

}  // namespace styles
}  // namespace arrangrr
