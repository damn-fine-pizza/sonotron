#pragma once

#include "arrangrr/arranger/style_model.hpp"

namespace arrangrr {
namespace styles {


// ---------------------------------------------------------------------------
// "motown": soul backbeat — kick 1 & 3, cracking snare 2 & 4, tambourine on the
// eighths, a melodic eighth-note soul bass line and bright off-beat triad stabs.
namespace motown {
// Fuller-band roles (ADDITIVE): the Motown string section, an electric-piano
// comp, a vibraphone arp and extra hand percussion (claps on the backbeat,
// tambourine under the fills). kPad -> 48 (Strings), kChord2 -> 4 (E.Piano1),
// kArp -> 11 (Vibraphone); kPerc rides the drum channel.
inline constexpr std::int16_t kPadVoice = 48;
inline constexpr std::int16_t kChord2Voice = 4;
inline constexpr std::int16_t kArpVoice = 11;
inline constexpr std::int16_t kLeadVoice = 61;  // Brass Section — the Motown horn answer
inline constexpr StyleEvent kPadTriad[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=54, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=52, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=54, .gate=kGateHeld},
};
inline constexpr StyleEvent kPad7[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=56, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=54, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=56, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=50, .gate=kGateHeld},
};
// E.Piano comp: a warm quarter-note bed on beats 1 & 3, interlocking with
// chord1's off-beat stabs.
inline constexpr StyleEvent kEP[] = {
    {.step=0, .tone=kThird, .octave=0, .vel=58, .gate=kGateBeat}, {.step=0, .tone=kFifth, .octave=0, .vel=58, .gate=kGateBeat},
    {.step=8, .tone=kThird, .octave=0, .vel=56, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=56, .gate=kGateBeat},
};
// Vibraphone arp: high melodic eighths up and back down the chord (~72).
inline constexpr StyleEvent kVibes[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=64, .gate=kGateHat}, {.step=2, .tone=kThird, .octave=0, .vel=56, .gate=kGateHat}, {.step=4, .tone=kFifth, .octave=0, .vel=60, .gate=kGateHat}, {.step=6, .tone=kSeventh, .octave=0, .vel=56, .gate=kGateHat},
    {.step=8, .tone=kRoot, .octave=1, .vel=64, .gate=kGateHat}, {.step=10, .tone=kSeventh, .octave=0, .vel=56, .gate=kGateHat}, {.step=12, .tone=kFifth, .octave=0, .vel=60, .gate=kGateHat}, {.step=14, .tone=kThird, .octave=0, .vel=56, .gate=kGateHat},
};
// Perc: the famous Motown handclaps on the backbeat; a tambourine for fills.
inline constexpr StyleEvent kPercClap[] = {
    {.step=4, .tone=kClap, .octave=0, .vel=84, .gate=kGateHat}, {.step=12, .tone=kClap, .octave=0, .vel=88, .gate=kGateHat},
};
inline constexpr StyleEvent kPercTamb[] = {
    {.step=4, .tone=kTambourine, .octave=0, .vel=78, .gate=kGateHat}, {.step=12, .tone=kTambourine, .octave=0, .vel=82, .gate=kGateHat},
};
// The Motown horn answer: punchy stabs in the back half of the bar that reply
// to the vocal phrase. Every note is kScaleDegree, so the section stays diatonic
// to the live KEY and floats over the chord changes (it does NOT chase the chord
// root the way a kInterval line would) — the signature "section" sound. kLead -> 72.
inline constexpr StyleEvent kHornAnswer[] = {
    {.step=8,  .tone=4, .octave=0, .vel=84, .gate=kGateStab, .src=NoteSource::kScaleDegree},   // 5th of key
    {.step=10, .tone=6, .octave=0, .vel=80, .gate=kGateStab, .src=NoteSource::kScaleDegree},   // 7th
    {.step=12, .tone=7, .octave=0, .vel=88, .gate=kGateStab, .src=NoteSource::kScaleDegree},   // up to the octave
    {.step=14, .tone=4, .octave=0, .vel=82, .gate=kGateBeat, .src=NoteSource::kScaleDegree},   // back down to the 5th
};
inline constexpr StyleEvent kHeldBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=94, .gate=kGateHeld}};
inline constexpr StyleEvent kSoulBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=100, .gate=kGate8th}, {.step=4, .tone=kThird, .octave=0, .vel=88, .gate=kGate8th}, {.step=6, .tone=kFifth, .octave=0, .vel=84, .gate=kGate8th}, {.step=8, .tone=kFifth, .octave=0, .vel=94, .gate=kGate8th}, {.step=10, .tone=kRoot, .octave=1, .vel=86, .gate=kGate8th}, {.step=12, .tone=kThird, .octave=0, .vel=90, .gate=kGate8th}, {.step=14, .tone=kSeventh, .octave=0, .vel=84, .gate=kGate8th}};
// Motif engine (9210, Ottorino RANK 2): the soul bass is reused 9x (the
// sharpest single-idiom bass redundancy after blues) — root-third-fifth with
// a big skip to the octave root at step 10, a signature riff whose
// kRetrograde reflection stays legible as "the same line, answered".
inline constexpr MotifSpec kSoulBassMotif{.transform = MotifTransform::kRetrograde, .seed = 202};
// The Motown horn answer, wired only into the peak variation (varD):
// kDiatonicTranspose is literally how a horn section varies its own riff
// bar to bar.
inline constexpr MotifSpec kHornAnswerMotif{.transform = MotifTransform::kDiatonicTranspose, .seed = 201};
inline constexpr StyleEvent kIn1D[] = {{.step=8, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=10, .tone=kTambourine, .octave=0, .vel=64, .gate=50}, {.step=12, .tone=kSnare, .octave=0, .vel=80, .gate=100}, {.step=14, .tone=kSnare, .octave=0, .vel=92, .gate=100}};
inline constexpr StylePattern kIn1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)}};
inline constexpr StyleEvent kIn2D[] = {{.step=0, .tone=kKick, .octave=0, .vel=100, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=96, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=98, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=98, .gate=120}, {.step=0, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=2, .tone=kTambourine, .octave=0, .vel=54, .gate=50}, {.step=4, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=6, .tone=kTambourine, .octave=0, .vel=54, .gate=50}, {.step=8, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=10, .tone=kTambourine, .octave=0, .vel=54, .gate=50}, {.step=12, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=14, .tone=kTambourine, .octave=0, .vel=54, .gate=50}};
inline constexpr StyleEvent kIn2C[] = {{.step=2, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=76, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=76, .gate=kGateStab}};
inline constexpr StylePattern kIn2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSoulBass), .motif=&kSoulBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIn2C), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}};
inline constexpr StyleEvent kAD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=106, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=102, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=108, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=108, .gate=120},
    {.step=0, .tone=kTambourine, .octave=0, .vel=64, .gate=50}, {.step=2, .tone=kTambourine, .octave=0, .vel=56, .gate=50}, {.step=4, .tone=kTambourine, .octave=0, .vel=64, .gate=50}, {.step=6, .tone=kTambourine, .octave=0, .vel=56, .gate=50}, {.step=8, .tone=kTambourine, .octave=0, .vel=64, .gate=50}, {.step=10, .tone=kTambourine, .octave=0, .vel=56, .gate=50}, {.step=12, .tone=kTambourine, .octave=0, .vel=64, .gate=50}, {.step=14, .tone=kTambourine, .octave=0, .vel=56, .gate=50},
};
inline constexpr StyleEvent kAC[] = {{.step=2, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=82, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStab}, {.step=6, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=82, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStab}, {.step=14, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStab}};
inline constexpr StylePattern kAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSoulBass), .motif=&kSoulBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAC), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEP), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead}};
inline constexpr StyleEvent kBD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=108, .gate=120}, {.step=6, .tone=kKick, .octave=0, .vel=90, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=110, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=110, .gate=120}, {.step=10, .tone=kSnare, .octave=0, .vel=58, .gate=50},
    {.step=0, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=1, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=2, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=3, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=4, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=5, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=6, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=7, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=8, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=9, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=10, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=11, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=12, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=13, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=14, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=15, .tone=kTambourine, .octave=0, .vel=52, .gate=50},
};
inline constexpr StyleEvent kBC[] = {{.step=2, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}, {.step=6, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=8, .tone=kFifth, .octave=0, .vel=76, .gate=kGateStaccato}, {.step=10, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}, {.step=14, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}};
inline constexpr StylePattern kBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSoulBass), .motif=&kSoulBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBC), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEP), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercClap)}};
inline constexpr StyleEvent kFAD[] = {{.step=0, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=100, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=92, .gate=120}, {.step=10, .tone=kSnare, .octave=0, .vel=96, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=104, .gate=120}, {.step=14, .tone=kSnare, .octave=0, .vel=110, .gate=120}};
inline constexpr StyleEvent kFBD[] = {{.step=0, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=98, .gate=100}, {.step=6, .tone=kSnare, .octave=0, .vel=92, .gate=100}, {.step=8, .tone=kTomHi, .octave=0, .vel=96, .gate=100}, {.step=10, .tone=kTomMid, .octave=0, .vel=100, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=104, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=110, .gate=100}};
inline constexpr StyleEvent kFCD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=94, .gate=100}, {.step=2, .tone=kSnare, .octave=0, .vel=88, .gate=100}, {.step=4, .tone=kTomHi, .octave=0, .vel=98, .gate=100}, {.step=6, .tone=kTomHi, .octave=0, .vel=94, .gate=100}, {.step=8, .tone=kTomMid, .octave=0, .vel=102, .gate=100}, {.step=10, .tone=kTomMid, .octave=0, .vel=98, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=108, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=114, .gate=100}};
inline constexpr StyleEvent kFDD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=92, .gate=50}, {.step=1, .tone=kSnare, .octave=0, .vel=86, .gate=50}, {.step=2, .tone=kSnare, .octave=0, .vel=96, .gate=50}, {.step=3, .tone=kSnare, .octave=0, .vel=90, .gate=50}, {.step=4, .tone=kTomHi, .octave=0, .vel=100, .gate=50}, {.step=6, .tone=kTomHi, .octave=0, .vel=96, .gate=50}, {.step=8, .tone=kTomMid, .octave=0, .vel=104, .gate=50}, {.step=10, .tone=kTomLow, .octave=0, .vel=108, .gate=50}, {.step=12, .tone=kTomFloor, .octave=0, .vel=112, .gate=50}, {.step=13, .tone=kSnare, .octave=0, .vel=114, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=118, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=120, .gate=kGate8th}};
inline constexpr StylePattern kFAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSoulBass), .motif=&kSoulBassMotif}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercTamb)}};
inline constexpr StylePattern kFBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSoulBass), .motif=&kSoulBassMotif}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercTamb)}};
inline constexpr StylePattern kFCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSoulBass), .motif=&kSoulBassMotif}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercTamb)}};
inline constexpr StylePattern kFDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSoulBass), .motif=&kSoulBassMotif}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercTamb)}};
inline constexpr StyleEvent kE1D[] = {{.step=0, .tone=kKick, .octave=0, .vel=106, .gate=120}, {.step=0, .tone=kCrash, .octave=0, .vel=100, .gate=kGateHalfBar}};
inline constexpr StyleEvent kE1B[] = {{.step=0, .tone=kRoot, .octave=0, .vel=98, .gate=kGateHeld}};
inline constexpr StyleEvent kE1C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=84, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=84, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=84, .gate=kGateHeld}};
inline constexpr StylePattern kE1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1C), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}};
inline constexpr StyleEvent kE2D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=110, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=108, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=100, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=104, .gate=120}};
inline constexpr StyleEvent kE2B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=104, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=94, .gate=kGateHeld}};
inline constexpr StyleEvent kE2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=86, .gate=kGateHeld}};
inline constexpr StylePattern kE2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2C), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}};
// varC: half-time soul — backbeat on beat 3, tambourine eighths, sustained triad.
inline constexpr StyleEvent kCD[] = {{.step=0, .tone=kKick, .octave=0, .vel=106, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=108, .gate=120}, {.step=0, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=2, .tone=kTambourine, .octave=0, .vel=54, .gate=50}, {.step=4, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=6, .tone=kTambourine, .octave=0, .vel=54, .gate=50}, {.step=8, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=10, .tone=kTambourine, .octave=0, .vel=54, .gate=50}, {.step=12, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=14, .tone=kTambourine, .octave=0, .vel=54, .gate=50}};
inline constexpr StyleEvent kCC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=78, .gate=kGateBeat}, {.step=0, .tone=kThird, .octave=0, .vel=78, .gate=kGateBeat}, {.step=0, .tone=kFifth, .octave=0, .vel=78, .gate=kGateBeat}, {.step=8, .tone=kRoot, .octave=0, .vel=76, .gate=kGateBeat}, {.step=8, .tone=kThird, .octave=0, .vel=76, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=76, .gate=kGateBeat}};
inline constexpr StylePattern kCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSoulBass), .motif=&kSoulBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCC), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEP), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVibes), .gm_program=kArpVoice}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercClap)}};
// varD: peak soul — kick pushes, cracking snare with ghost, 16th tambourine, full 7th stabs.
inline constexpr StyleEvent kDD[] = {{.step=0, .tone=kKick, .octave=0, .vel=108, .gate=120}, {.step=6, .tone=kKick, .octave=0, .vel=90, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=10, .tone=kKick, .octave=0, .vel=88, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=112, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=112, .gate=120}, {.step=10, .tone=kSnare, .octave=0, .vel=58, .gate=50}, {.step=0, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=1, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=2, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=3, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=4, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=5, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=6, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=7, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=8, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=9, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=10, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=11, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=12, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=13, .tone=kTambourine, .octave=0, .vel=52, .gate=50}, {.step=14, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=15, .tone=kTambourine, .octave=0, .vel=52, .gate=50}};
inline constexpr StyleEvent kDC[] = {{.step=2, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}, {.step=6, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}, {.step=14, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}};
inline constexpr StylePattern kDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSoulBass), .motif=&kSoulBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kDC), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEP), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVibes), .gm_program=kArpVoice}, {.role=TrackRole::kLead, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHornAnswer), .gm_program=kLeadVoice, .motif=&kHornAnswerMotif}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercClap)}};
// varBreak: a Motown stop. The band cuts out on one accented downbeat hit — kick
// + crash, a handclap, a bass pop and a full 7th chord stab — then wide silence,
// then a snare/tom pickup on beat 4 throws the soul groove back in. The famous
// "everybody stop" that leaves the singer alone for a bar.
inline constexpr StyleEvent kBrkD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=106, .gate=120}, {.step=0, .tone=kCrash, .octave=0, .vel=100, .gate=kGateStab}, {.step=0, .tone=kClap, .octave=0, .vel=92, .gate=kGateHat},
    {.step=12, .tone=kSnare, .octave=0, .vel=90, .gate=100}, {.step=13, .tone=kSnare, .octave=0, .vel=96, .gate=100}, {.step=14, .tone=kTomMid, .octave=0, .vel=102, .gate=100}, {.step=15, .tone=kTomLow, .octave=0, .vel=108, .gate=100},
};
inline constexpr StyleEvent kBrkB[] = {{.step=0, .tone=kRoot, .octave=0, .vel=104, .gate=kGateStab}};
inline constexpr StyleEvent kBrkC[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=88, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=88, .gate=kGateStab}, {.step=0, .tone=kFifth, .octave=0, .vel=88, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=88, .gate=kGateStab},
};
inline constexpr StylePattern kBrkP[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kBrkD)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBrkB)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBrkC), .voicing=VoicingPolicy::kLead},
};
inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=1, .patterns=Span<const StylePattern>(kIn1P)}, {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIn2P)},
    {.type=SectionType::kVarA, .bars=1, .patterns=Span<const StylePattern>(kAP)}, {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kBP)},
    {.type=SectionType::kVarC, .bars=1, .patterns=Span<const StylePattern>(kCP)}, {.type=SectionType::kVarD, .bars=1, .patterns=Span<const StylePattern>(kDP)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFAP)}, {.type=SectionType::kFillB, .bars=1, .patterns=Span<const StylePattern>(kFBP)}, {.type=SectionType::kFillC, .bars=1, .patterns=Span<const StylePattern>(kFCP)}, {.type=SectionType::kFillD, .bars=1, .patterns=Span<const StylePattern>(kFDP)},
    {.type=SectionType::kBreak, .bars=1, .patterns=Span<const StylePattern>(kBrkP)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kE1P)}, {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kE2P)},
};
inline constexpr Style kStyle{.name="motown", .sections=Span<const StyleSection>(kSections), .tempo=12400};
}  // namespace motown

}  // namespace styles
}  // namespace arrangrr
