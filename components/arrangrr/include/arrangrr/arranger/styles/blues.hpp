#pragma once

#include "arrangrr/arranger/style_model.hpp"

namespace arrangrr {
namespace styles {


// ---------------------------------------------------------------------------
// "blues": slow 12/8 shuffle — triplet ride on the long-short eighths, soft
// kick/snare, a boogie root-fifth-seventh bass and dominant-7 shuffle stabs.
namespace blues {
// Fuller-band roles (ADDITIVE): a sustained string bed, a rock-organ comp, a
// clean-guitar triplet arp and a light tambourine. kPad -> 48 (Strings),
// kChord2 -> 18 (Rock Organ), kArp -> 27 (Clean Guitar); kPerc rides drums.
inline constexpr std::int16_t kPadVoice = 48;
inline constexpr std::int16_t kChord2Voice = 18;
inline constexpr std::int16_t kArpVoice = 27;
inline constexpr std::int16_t kLeadVoice = 22;  // Harmonica — the blues harp line
inline constexpr StyleEvent kPadTriad[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=54, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=52, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=54, .gate=kGateHeld},
};
inline constexpr StyleEvent kPad7[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=56, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=54, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=56, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=50, .gate=kGateHeld},
};
// Organ comp: soft dominant swell on the downbeats, under chord1's off stabs.
inline constexpr StyleEvent kOrganComp[] = {
    {.step=0, .tone=kThird, .octave=0, .vel=58, .gate=kGateHalfBar}, {.step=0, .tone=kFifth, .octave=0, .vel=58, .gate=kGateHalfBar}, {.step=0, .tone=kSeventh, .octave=0, .vel=54, .gate=kGateHalfBar},
    {.step=8, .tone=kThird, .octave=0, .vel=56, .gate=kGateHalfBar}, {.step=8, .tone=kFifth, .octave=0, .vel=56, .gate=kGateHalfBar}, {.step=8, .tone=kSeventh, .octave=0, .vel=52, .gate=kGateHalfBar},
};
// Arp: bluesy triplet climb up the dominant 7, high register (~72).
inline constexpr StyleEvent kArpTrip[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=62, .gate=kGateHat}, {.step=2, .tone=kFifth, .octave=0, .vel=50, .gate=kGateHat}, {.step=4, .tone=kThird, .octave=0, .vel=56, .gate=kGateHat}, {.step=6, .tone=kSeventh, .octave=0, .vel=50, .gate=kGateHat},
    {.step=8, .tone=kFifth, .octave=0, .vel=60, .gate=kGateHat}, {.step=10, .tone=kRoot, .octave=1, .vel=54, .gate=kGateHat}, {.step=12, .tone=kSeventh, .octave=0, .vel=56, .gate=kGateHat}, {.step=14, .tone=kThird, .octave=1, .vel=50, .gate=kGateHat},
};
// Perc: light tambourine on the backbeat.
inline constexpr StyleEvent kPercTamb[] = {
    {.step=4, .tone=kTambourine, .octave=0, .vel=66, .gate=kGateHat}, {.step=12, .tone=kTambourine, .octave=0, .vel=70, .gate=kGateHat},
};
// Blues harp line: the signature sound is all BLUE NOTES. Every note is
// kInterval — a signed semitone offset from the CURRENT chord ROOT — so the b3
// (3), b5 (6) and b7 (10) bend against whichever of the I/IV/V chords is live,
// exactly as a harp player leans on them through the 12-bar. kLead -> 72.
inline constexpr StyleEvent kLeadLick[] = {
    {.step=8,  .tone=0,  .octave=0, .vel=76, .gate=kGate8th,      .src=NoteSource::kInterval},   // chord root
    {.step=10, .tone=3,  .octave=0, .vel=70, .gate=kGate8th,      .src=NoteSource::kInterval},   // blue b3
    {.step=11, .tone=6,  .octave=0, .vel=66, .gate=kGateStaccato, .src=NoteSource::kInterval},   // blue b5 passing tone
    {.step=12, .tone=7,  .octave=0, .vel=74, .gate=kGate8th,      .src=NoteSource::kInterval},   // up to the 5th
    {.step=14, .tone=10, .octave=0, .vel=72, .gate=kGateBeat,     .src=NoteSource::kInterval},   // blue b7 hangs over
};
inline constexpr StyleEvent kHeldBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=82, .gate=kGateHeld}};
inline constexpr StyleEvent kBoogieBass[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=90, .gate=kGate8th}, {.step=4, .tone=kFifth, .octave=-1, .vel=82, .gate=kGate8th}, {.step=6, .tone=kSeventh, .octave=-1, .vel=78, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=-1, .vel=88, .gate=kGate8th}, {.step=12, .tone=kFifth, .octave=-1, .vel=82, .gate=kGate8th}, {.step=14, .tone=kSeventh, .octave=-1, .vel=78, .gate=kGateStab}};
inline constexpr StyleEvent kIn1D[] = {{.step=8, .tone=kRide, .octave=0, .vel=56, .gate=120}, {.step=10, .tone=kRide, .octave=0, .vel=48, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=60, .gate=120}, {.step=14, .tone=kRide, .octave=0, .vel=52, .gate=120}};
inline constexpr StylePattern kIn1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)}};
inline constexpr StyleEvent kIn2D[] = {{.step=0, .tone=kRide, .octave=0, .vel=64, .gate=120}, {.step=2, .tone=kRide, .octave=0, .vel=50, .gate=120}, {.step=4, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=6, .tone=kRide, .octave=0, .vel=50, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=62, .gate=120}, {.step=10, .tone=kRide, .octave=0, .vel=50, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=14, .tone=kRide, .octave=0, .vel=50, .gate=120}, {.step=0, .tone=kKick, .octave=0, .vel=80, .gate=100}, {.step=8, .tone=kKick, .octave=0, .vel=78, .gate=100}, {.step=4, .tone=kSnare, .octave=0, .vel=76, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=78, .gate=100}};
inline constexpr StyleEvent kIn2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=72, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=70, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=70, .gate=kGateStab}, {.step=8, .tone=kSeventh, .octave=0, .vel=70, .gate=kGateStab}};
inline constexpr StylePattern kIn2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIn2C), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}};
inline constexpr StyleEvent kAD[] = {
    {.step=0, .tone=kRide, .octave=0, .vel=74, .gate=120}, {.step=2, .tone=kRide, .octave=0, .vel=52, .gate=120}, {.step=4, .tone=kRide, .octave=0, .vel=66, .gate=120}, {.step=6, .tone=kRide, .octave=0, .vel=52, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=72, .gate=120}, {.step=10, .tone=kRide, .octave=0, .vel=52, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=66, .gate=120}, {.step=14, .tone=kRide, .octave=0, .vel=52, .gate=120},
    {.step=0, .tone=kKick, .octave=0, .vel=88, .gate=100}, {.step=8, .tone=kKick, .octave=0, .vel=84, .gate=100}, {.step=4, .tone=kSnare, .octave=0, .vel=94, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=96, .gate=100},
};
inline constexpr StyleEvent kAC[] = {{.step=2, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=76, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=76, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=74, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=74, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=74, .gate=kGateStab}};
inline constexpr StylePattern kAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAC), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kOrganComp), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead}};
inline constexpr StyleEvent kBD[] = {
    {.step=0, .tone=kRide, .octave=0, .vel=78, .gate=120}, {.step=2, .tone=kRide, .octave=0, .vel=56, .gate=120}, {.step=4, .tone=kRide, .octave=0, .vel=70, .gate=120}, {.step=6, .tone=kRide, .octave=0, .vel=56, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=76, .gate=120}, {.step=10, .tone=kRide, .octave=0, .vel=56, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=70, .gate=120}, {.step=14, .tone=kRide, .octave=0, .vel=56, .gate=120},
    {.step=0, .tone=kKick, .octave=0, .vel=92, .gate=100}, {.step=6, .tone=kKick, .octave=0, .vel=76, .gate=100}, {.step=8, .tone=kKick, .octave=0, .vel=90, .gate=100}, {.step=4, .tone=kSnare, .octave=0, .vel=98, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=100, .gate=100}, {.step=14, .tone=kSnare, .octave=0, .vel=60, .gate=50},
};
inline constexpr StyleEvent kBC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=8, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}};
inline constexpr StylePattern kBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBC), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kOrganComp), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercTamb)}};
inline constexpr StyleEvent kFAD[] = {{.step=0, .tone=kKick, .octave=0, .vel=90, .gate=100}, {.step=4, .tone=kSnare, .octave=0, .vel=90, .gate=100}, {.step=8, .tone=kSnare, .octave=0, .vel=84, .gate=100}, {.step=10, .tone=kSnare, .octave=0, .vel=88, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=94, .gate=100}, {.step=14, .tone=kSnare, .octave=0, .vel=100, .gate=100}};
inline constexpr StyleEvent kFBD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=84, .gate=100}, {.step=2, .tone=kSnare, .octave=0, .vel=78, .gate=100}, {.step=4, .tone=kTomHi, .octave=0, .vel=88, .gate=100}, {.step=6, .tone=kTomMid, .octave=0, .vel=86, .gate=100}, {.step=8, .tone=kSnare, .octave=0, .vel=90, .gate=100}, {.step=10, .tone=kTomLow, .octave=0, .vel=92, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=96, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=100, .gate=100}};
inline constexpr StyleEvent kFCD[] = {{.step=0, .tone=kTomHi, .octave=0, .vel=84, .gate=100}, {.step=2, .tone=kTomHi, .octave=0, .vel=80, .gate=100}, {.step=4, .tone=kTomMid, .octave=0, .vel=88, .gate=100}, {.step=6, .tone=kTomMid, .octave=0, .vel=84, .gate=100}, {.step=8, .tone=kTomLow, .octave=0, .vel=92, .gate=100}, {.step=10, .tone=kTomLow, .octave=0, .vel=88, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=96, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=102, .gate=100}};
inline constexpr StyleEvent kFDD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=84, .gate=50}, {.step=2, .tone=kSnare, .octave=0, .vel=78, .gate=50}, {.step=3, .tone=kTomHi, .octave=0, .vel=88, .gate=50}, {.step=4, .tone=kTomHi, .octave=0, .vel=92, .gate=50}, {.step=6, .tone=kTomMid, .octave=0, .vel=90, .gate=50}, {.step=7, .tone=kTomMid, .octave=0, .vel=94, .gate=50}, {.step=8, .tone=kTomLow, .octave=0, .vel=96, .gate=50}, {.step=10, .tone=kTomLow, .octave=0, .vel=92, .gate=50}, {.step=11, .tone=kTomFloor, .octave=0, .vel=98, .gate=50}, {.step=12, .tone=kSnare, .octave=0, .vel=102, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=108, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=112, .gate=kGate8th}};
inline constexpr StylePattern kFAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass)}};
inline constexpr StylePattern kFBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass)}};
inline constexpr StylePattern kFCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass)}};
inline constexpr StylePattern kFDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass)}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercTamb)}};
inline constexpr StyleEvent kE1D[] = {{.step=0, .tone=kKick, .octave=0, .vel=94, .gate=100}, {.step=0, .tone=kCrash, .octave=0, .vel=90, .gate=kGateHalfBar}};
inline constexpr StyleEvent kE1B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=90, .gate=kGateHeld}};
inline constexpr StyleEvent kE1C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=80, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=80, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateHeld}};
inline constexpr StylePattern kE1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1C), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}};
inline constexpr StyleEvent kE2D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=98, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=94, .gate=100}, {.step=8, .tone=kKick, .octave=0, .vel=88, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=92, .gate=100}};
inline constexpr StyleEvent kE2B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=94, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=-1, .vel=82, .gate=kGateHeld}};
inline constexpr StyleEvent kE2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=82, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=82, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=82, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=82, .gate=kGateHeld}};
inline constexpr StylePattern kE2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2C), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}};
// varC: slow-drag stop-time — bare ride and kick/snare on the beats, wide space.
inline constexpr StyleEvent kCD[] = {{.step=0, .tone=kRide, .octave=0, .vel=70, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=66, .gate=120}, {.step=0, .tone=kKick, .octave=0, .vel=86, .gate=100}, {.step=8, .tone=kSnare, .octave=0, .vel=90, .gate=100}};
inline constexpr StyleEvent kCC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=76, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=76, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=74, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=74, .gate=kGateStab}, {.step=8, .tone=kSeventh, .octave=0, .vel=74, .gate=kGateStab}};
inline constexpr StylePattern kCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCC), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kOrganComp), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArpTrip), .gm_program=kArpVoice}};
// varD: peak shuffle — full triplet ride, driving kick/snare, full dominant-7 stabs.
inline constexpr StyleEvent kDD[] = {{.step=0, .tone=kRide, .octave=0, .vel=80, .gate=120}, {.step=2, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=4, .tone=kRide, .octave=0, .vel=72, .gate=120}, {.step=6, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=78, .gate=120}, {.step=10, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=72, .gate=120}, {.step=14, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=0, .tone=kKick, .octave=0, .vel=94, .gate=100}, {.step=6, .tone=kKick, .octave=0, .vel=78, .gate=100}, {.step=8, .tone=kKick, .octave=0, .vel=92, .gate=100}, {.step=4, .tone=kSnare, .octave=0, .vel=100, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=102, .gate=100}, {.step=14, .tone=kSnare, .octave=0, .vel=62, .gate=50}};
inline constexpr StyleEvent kDC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=0, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=8, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}, {.step=8, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}};
inline constexpr StylePattern kDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kDC), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kOrganComp), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArpTrip), .gm_program=kArpVoice}, {.role=TrackRole::kLead, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kLeadLick), .gm_program=kLeadVoice}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercTamb)}};
// varBreak: the classic blues stop-time break. The whole band drops out on an
// accented downbeat stab (kick+crash, a bass pop, a dominant-7 chord stab),
// then wide silence, then a snare/tom pickup on beat 4 that throws the 12-bar
// back in — the "stop" that leaves a bar wide open for a vocal or solo answer.
inline constexpr StyleEvent kBrkD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=98, .gate=100}, {.step=0, .tone=kCrash, .octave=0, .vel=94, .gate=kGateStab},
    {.step=12, .tone=kSnare, .octave=0, .vel=84, .gate=100}, {.step=13, .tone=kSnare, .octave=0, .vel=90, .gate=100}, {.step=14, .tone=kTomMid, .octave=0, .vel=96, .gate=100}, {.step=15, .tone=kTomLow, .octave=0, .vel=104, .gate=100},
};
inline constexpr StyleEvent kBrkB[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=96, .gate=kGateStab}};
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
inline constexpr Style kStyle{.name="blues", .sections=Span<const StyleSection>(kSections), .groove={.swing=75, .accent=10, .swing_grid=8}, .tempo=6600};
}  // namespace blues

}  // namespace styles
}  // namespace arrangrr
