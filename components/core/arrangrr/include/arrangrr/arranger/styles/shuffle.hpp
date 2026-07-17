#pragma once

#include "arrangrr/arranger/style_model.hpp"

namespace arrangrr {
namespace styles {


// ---------------------------------------------------------------------------
// "shuffle": driving Texas shuffle — bouncy closed-hat shuffle eighths, hard
// backbeat, a bouncing root-fifth shuffle bass and bright triad shuffle stabs.
namespace shuffle {
// Fuller-band roles (ADDITIVE): a warm pad bed, a rock-organ shuffle comp, a
// clean-guitar arp and a tambourine backbeat. kPad -> 89 (Pad2 warm),
// kChord2 -> 18 (Rock Organ), kArp -> 27 (Clean Guitar); kPerc rides drums.
inline constexpr std::int16_t kPadVoice = 89;
inline constexpr std::int16_t kChord2Voice = 18;
inline constexpr std::int16_t kArpVoice = 27;
inline constexpr std::int16_t kLeadVoice = 29;  // Overdriven Guitar — the Texas shuffle lead
inline constexpr StyleEvent kPadTriad[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=56, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=54, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=56, .gate=kGateHeld},
};
inline constexpr StyleEvent kPad7[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=58, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=56, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=58, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=52, .gate=kGateHeld},
};
// Organ comp: bright triad swell on the downbeats, filling chord1's off stabs.
inline constexpr StyleEvent kOrganComp[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=60, .gate=kGateHalfBar}, {.step=0, .tone=kThird, .octave=0, .vel=60, .gate=kGateHalfBar}, {.step=0, .tone=kFifth, .octave=0, .vel=60, .gate=kGateHalfBar},
    {.step=8, .tone=kRoot, .octave=0, .vel=58, .gate=kGateHalfBar}, {.step=8, .tone=kThird, .octave=0, .vel=58, .gate=kGateHalfBar}, {.step=8, .tone=kFifth, .octave=0, .vel=58, .gate=kGateHalfBar},
};
// Arp: clean-guitar shuffle eighths up the chord, high register (~72).
inline constexpr StyleEvent kArpShuf[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=64, .gate=kGateHat}, {.step=2, .tone=kThird, .octave=0, .vel=52, .gate=kGateHat}, {.step=4, .tone=kFifth, .octave=0, .vel=60, .gate=kGateHat}, {.step=6, .tone=kSeventh, .octave=0, .vel=52, .gate=kGateHat},
    {.step=8, .tone=kRoot, .octave=1, .vel=64, .gate=kGateHat}, {.step=10, .tone=kSeventh, .octave=0, .vel=52, .gate=kGateHat}, {.step=12, .tone=kFifth, .octave=0, .vel=60, .gate=kGateHat}, {.step=14, .tone=kThird, .octave=0, .vel=52, .gate=kGateHat},
};
// Perc: tambourine on the backbeat.
inline constexpr StyleEvent kPercTamb[] = {
    {.step=4, .tone=kTambourine, .octave=0, .vel=72, .gate=kGateHat}, {.step=12, .tone=kTambourine, .octave=0, .vel=76, .gate=kGateHat},
};
// A bluesy shuffle lead in the back half of the bar. The step-8 anchor is
// kScaleDegree (a diatonic boogie 5th that floats over the KEY); the rest are
// kInterval BLUE NOTES — b3 (3), b5 (6), b7 (10) — that bend against the live
// chord ROOT, so the two sources pull apart across the I/IV/V. kLead -> 72.
inline constexpr StyleEvent kLeadLick[] = {
    {.step=8,  .tone=4,  .octave=0, .vel=80, .gate=kGate8th,      .src=NoteSource::kScaleDegree},   // 5th of key (boogie anchor)
    {.step=10, .tone=3,  .octave=0, .vel=72, .gate=kGate8th,      .src=NoteSource::kInterval},      // blue b3 off the chord root
    {.step=11, .tone=6,  .octave=0, .vel=68, .gate=kGateStaccato, .src=NoteSource::kInterval},      // blue b5 grace
    {.step=12, .tone=7,  .octave=0, .vel=78, .gate=kGate8th,      .src=NoteSource::kInterval},      // to the 5th
    {.step=14, .tone=10, .octave=0, .vel=76, .gate=kGateBeat,     .src=NoteSource::kInterval},      // blue b7 hangs over
};
inline constexpr StyleEvent kHeldBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=96, .gate=kGateHeld}};
inline constexpr StyleEvent kShufBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=104, .gate=kGate8th}, {.step=2, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=4, .tone=kFifth, .octave=0, .vel=96, .gate=kGate8th}, {.step=6, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=102, .gate=kGate8th}, {.step=10, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=12, .tone=kFifth, .octave=0, .vel=96, .gate=kGate8th}, {.step=14, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}};
// Motif engine (9210, Ottorino RANK 4): the shuffle bass, reused 9x — the
// densest single-idiom bass in the corpus (8 onsets, not a sparse hook), so
// kDisplacement's evolving push (a different phase each odd repeat) suits
// it better than a fixed mirrored answer.
inline constexpr MotifSpec kShufBassMotif{.transform = MotifTransform::kDisplacement, .seed = 401};
// The densest blue-note lick in the corpus, wired only into varD.
inline constexpr MotifSpec kLeadLickMotif{.transform = MotifTransform::kDiatonicTranspose, .seed = 402};
// Motif engine (9210 Wave 1, style-depth program task A): extend variation
// onto drums/chord1/chord2/pad, wired onto the peak variation (varD) plus the
// shared reused Pad (kPad7) and Chord2 (kOrganComp) arrays. kDisplacement for
// drums/pad/chord2 (a soft evolving push); kRetrograde for the chord1 triad
// stabs (a legible mirrored answer).
inline constexpr MotifSpec kPeakDrumsMotif{.transform = MotifTransform::kDisplacement, .seed = 410};
inline constexpr MotifSpec kPeakChordMotif{.transform = MotifTransform::kRetrograde, .seed = 411};
inline constexpr MotifSpec kSharedPadMotif{.transform = MotifTransform::kDisplacement, .seed = 412};
inline constexpr MotifSpec kSharedChord2Motif{.transform = MotifTransform::kDisplacement, .seed = 413};
inline constexpr StyleEvent kIn1D[] = {{.step=8, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=12, .tone=kSnare, .octave=0, .vel=80, .gate=100}, {.step=14, .tone=kSnare, .octave=0, .vel=92, .gate=100}};
inline constexpr StylePattern kIn1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)}};
inline constexpr StyleEvent kIn2D[] = {{.step=0, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=100, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=98, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=98, .gate=120}, {.step=0, .tone=kClosedHat, .octave=0, .vel=66, .gate=50}, {.step=2, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=62, .gate=50}, {.step=6, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=66, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=62, .gate=50}, {.step=14, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}};
inline constexpr StyleEvent kIn2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=0, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=8, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStab}};
inline constexpr StylePattern kIn2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kShufBass), .motif=&kShufBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIn2C), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead, .motif=&kSharedPadMotif}};
inline constexpr StyleEvent kAD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=106, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=108, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=108, .gate=120},
    {.step=0, .tone=kClosedHat, .octave=0, .vel=72, .gate=50}, {.step=2, .tone=kClosedHat, .octave=0, .vel=54, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=68, .gate=50}, {.step=6, .tone=kClosedHat, .octave=0, .vel=54, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=72, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=54, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=68, .gate=50}, {.step=14, .tone=kClosedHat, .octave=0, .vel=54, .gate=50},
};
inline constexpr StyleEvent kAC[] = {{.step=2, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=82, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStab}};
inline constexpr StylePattern kAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kShufBass), .motif=&kShufBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAC), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kOrganComp), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead, .motif=&kSharedChord2Motif}};
inline constexpr StyleEvent kBD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=114, .gate=120}, {.step=6, .tone=kKick, .octave=0, .vel=92, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=110, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=110, .gate=120}, {.step=14, .tone=kSnare, .octave=0, .vel=64, .gate=50},
    {.step=0, .tone=kOpenHat, .octave=0, .vel=78, .gate=120}, {.step=2, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=4, .tone=kOpenHat, .octave=0, .vel=74, .gate=120}, {.step=6, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=8, .tone=kOpenHat, .octave=0, .vel=78, .gate=120}, {.step=10, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=12, .tone=kOpenHat, .octave=0, .vel=74, .gate=120}, {.step=14, .tone=kClosedHat, .octave=0, .vel=56, .gate=50},
};
inline constexpr StyleEvent kBC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=86, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=86, .gate=kGateStab}, {.step=0, .tone=kFifth, .octave=0, .vel=86, .gate=kGateStab}, {.step=2, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=76, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=76, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=86, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=86, .gate=kGateStab}, {.step=8, .tone=kFifth, .octave=0, .vel=86, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=76, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=76, .gate=kGateStab}};
inline constexpr StylePattern kBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kShufBass), .motif=&kShufBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBC), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kOrganComp), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead, .motif=&kSharedChord2Motif}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercTamb)}};
inline constexpr StyleEvent kFAD[] = {{.step=0, .tone=kKick, .octave=0, .vel=108, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=100, .gate=100}, {.step=8, .tone=kSnare, .octave=0, .vel=92, .gate=100}, {.step=10, .tone=kSnare, .octave=0, .vel=96, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=104, .gate=100}, {.step=14, .tone=kSnare, .octave=0, .vel=110, .gate=100}};
inline constexpr StyleEvent kFBD[] = {{.step=0, .tone=kKick, .octave=0, .vel=108, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=100, .gate=100}, {.step=6, .tone=kSnare, .octave=0, .vel=94, .gate=100}, {.step=8, .tone=kTomHi, .octave=0, .vel=98, .gate=100}, {.step=10, .tone=kTomMid, .octave=0, .vel=102, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=106, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=112, .gate=100}};
inline constexpr StyleEvent kFCD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=96, .gate=100}, {.step=2, .tone=kSnare, .octave=0, .vel=90, .gate=100}, {.step=4, .tone=kTomHi, .octave=0, .vel=100, .gate=100}, {.step=6, .tone=kTomHi, .octave=0, .vel=96, .gate=100}, {.step=8, .tone=kTomMid, .octave=0, .vel=104, .gate=100}, {.step=10, .tone=kTomLow, .octave=0, .vel=108, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=104, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=114, .gate=100}};
inline constexpr StyleEvent kFDD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=94, .gate=50}, {.step=2, .tone=kSnare, .octave=0, .vel=88, .gate=50}, {.step=3, .tone=kSnare, .octave=0, .vel=92, .gate=50}, {.step=4, .tone=kTomHi, .octave=0, .vel=102, .gate=50}, {.step=6, .tone=kTomHi, .octave=0, .vel=98, .gate=50}, {.step=7, .tone=kTomMid, .octave=0, .vel=100, .gate=50}, {.step=8, .tone=kTomMid, .octave=0, .vel=106, .gate=50}, {.step=10, .tone=kTomLow, .octave=0, .vel=108, .gate=50}, {.step=11, .tone=kTomLow, .octave=0, .vel=104, .gate=50}, {.step=12, .tone=kTomFloor, .octave=0, .vel=114, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=118, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=120, .gate=kGate8th}};
inline constexpr StylePattern kFAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kShufBass), .motif=&kShufBassMotif}};
inline constexpr StylePattern kFBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kShufBass), .motif=&kShufBassMotif}};
inline constexpr StylePattern kFCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kShufBass), .motif=&kShufBassMotif}};
inline constexpr StylePattern kFDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kShufBass), .motif=&kShufBassMotif}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercTamb)}};
inline constexpr StyleEvent kE1D[] = {{.step=0, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=0, .tone=kCrash, .octave=0, .vel=104, .gate=kGateHalfBar}};
inline constexpr StyleEvent kE1B[] = {{.step=0, .tone=kRoot, .octave=0, .vel=104, .gate=kGateHeld}};
inline constexpr StyleEvent kE1C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=86, .gate=kGateHeld}};
inline constexpr StylePattern kE1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1C), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}};
inline constexpr StyleEvent kE2D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=112, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=104, .gate=120}};
inline constexpr StyleEvent kE2B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=108, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=98, .gate=kGateHeld}};
inline constexpr StyleEvent kE2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=88, .gate=kGateHeld}};
inline constexpr StylePattern kE2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2C), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}};
// varC: half-time shuffle — backbeat on beat 3 with ghost notes, bouncy shuffle hats.
inline constexpr StyleEvent kCD[] = {{.step=0, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=108, .gate=120}, {.step=2, .tone=kSnare, .octave=0, .vel=50, .gate=50}, {.step=6, .tone=kSnare, .octave=0, .vel=48, .gate=50}, {.step=10, .tone=kSnare, .octave=0, .vel=52, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=50, .gate=50}, {.step=0, .tone=kClosedHat, .octave=0, .vel=66, .gate=50}, {.step=2, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=62, .gate=50}, {.step=6, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=66, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=62, .gate=50}, {.step=14, .tone=kClosedHat, .octave=0, .vel=52, .gate=50}};
inline constexpr StyleEvent kCC[] = {{.step=2, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=82, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}};
inline constexpr StylePattern kCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kShufBass), .motif=&kShufBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCC), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead, .motif=&kSharedPadMotif}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kOrganComp), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead, .motif=&kSharedChord2Motif}, {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArpShuf), .gm_program=kArpVoice}};
// varD: peak Texas shuffle — open-hat shuffle, hard backbeat, kick pushes, full 7th stabs.
inline constexpr StyleEvent kDD[] = {{.step=0, .tone=kKick, .octave=0, .vel=116, .gate=120}, {.step=6, .tone=kKick, .octave=0, .vel=94, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=112, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=112, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=112, .gate=120}, {.step=14, .tone=kSnare, .octave=0, .vel=66, .gate=50}, {.step=0, .tone=kOpenHat, .octave=0, .vel=80, .gate=120}, {.step=2, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=4, .tone=kOpenHat, .octave=0, .vel=76, .gate=120}, {.step=6, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=8, .tone=kOpenHat, .octave=0, .vel=80, .gate=120}, {.step=10, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}, {.step=12, .tone=kOpenHat, .octave=0, .vel=76, .gate=120}, {.step=14, .tone=kClosedHat, .octave=0, .vel=56, .gate=50}};
inline constexpr StyleEvent kDC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=88, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=88, .gate=kGateStab}, {.step=0, .tone=kFifth, .octave=0, .vel=88, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=88, .gate=kGateStab}, {.step=2, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=88, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=88, .gate=kGateStab}, {.step=8, .tone=kFifth, .octave=0, .vel=88, .gate=kGateStab}, {.step=8, .tone=kSeventh, .octave=0, .vel=88, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStab}};
inline constexpr StylePattern kDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kDD), .motif=&kPeakDrumsMotif}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kShufBass), .motif=&kShufBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kDC), .voicing=VoicingPolicy::kLead, .motif=&kPeakChordMotif}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead, .motif=&kSharedPadMotif}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kOrganComp), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead, .motif=&kSharedChord2Motif}, {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArpShuf), .gm_program=kArpVoice}, {.role=TrackRole::kLead, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kLeadLick), .gm_program=kLeadVoice, .motif=&kLeadLickMotif}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercTamb)}};
inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=1, .patterns=Span<const StylePattern>(kIn1P)}, {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIn2P)},
    {.type=SectionType::kVarA, .bars=1, .patterns=Span<const StylePattern>(kAP)}, {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kBP)},
    {.type=SectionType::kVarC, .bars=1, .patterns=Span<const StylePattern>(kCP)}, {.type=SectionType::kVarD, .bars=1, .patterns=Span<const StylePattern>(kDP)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFAP)}, {.type=SectionType::kFillB, .bars=1, .patterns=Span<const StylePattern>(kFBP)}, {.type=SectionType::kFillC, .bars=1, .patterns=Span<const StylePattern>(kFCP)}, {.type=SectionType::kFillD, .bars=1, .patterns=Span<const StylePattern>(kFDP)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kE1P)}, {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kE2P)},
};
// Groove (9100 Wave 1, task B): driving Texas shuffle -- heavy deterministic
// humanize on top of the existing swing/accent feel.
inline constexpr Style kStyle{.name="shuffle", .sections=Span<const StyleSection>(kSections), .groove={.swing=72, .humanize_timing=16, .humanize_velocity=24, .accent=10, .swing_grid=8}, .tempo=13000};
}  // namespace shuffle

}  // namespace styles
}  // namespace arrangrr
