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
// Re-articulated at step 16 too (style-depth Wave-2 C): VarA is now 2 bars;
// the sustain re-triggers every bar exactly as it always has when the section
// looped -- background hold, not the section's own variation. VarB stays 1
// bar and never reads past step 15 (kPadTriad carries no MotifSpec in blues
// in either usage, so this tail is safe).
inline constexpr StyleEvent kPadTriad[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=54, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=52, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=54, .gate=kGateHeld},
    {.step=16, .tone=kRoot, .octave=0, .vel=54, .gate=kGateHeld}, {.step=16, .tone=kThird, .octave=0, .vel=52, .gate=kGateHeld}, {.step=16, .tone=kFifth, .octave=0, .vel=54, .gate=kGateHeld},
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
// Intro1 (style-depth Wave-2 C): 2 bars now. Bar 1 (unchanged) is the bare
// ride pickup over a held root; bar 2 continues the hold, then a 5th
// anticipation leads into the boogie bass proper.
inline constexpr StyleEvent kHeldBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=82, .gate=kGateHeld},
    {.step=16, .tone=kRoot, .octave=0, .vel=84, .gate=kGateHalfBar},
    {.step=30, .tone=kFifth, .octave=-1, .vel=76, .gate=kGate8th},
};
inline constexpr StyleEvent kBoogieBass[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=90, .gate=kGate8th}, {.step=4, .tone=kFifth, .octave=-1, .vel=82, .gate=kGate8th}, {.step=6, .tone=kSeventh, .octave=-1, .vel=78, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=-1, .vel=88, .gate=kGate8th}, {.step=12, .tone=kFifth, .octave=-1, .vel=82, .gate=kGate8th}, {.step=14, .tone=kSeventh, .octave=-1, .vel=78, .gate=kGateStab}};
// Motif engine (9210, Ottorino RANK 1): the boogie bass is the blues genre
// signature riff, copy-pasted across every Var/Fill section — the corpus's
// sharpest single-idiom redundancy. kRetrograde answers it with a stable,
// always-recognizable off-beat mirror (Finding A). Wired into VarA-D/FillA-D
// only (8 of the 9 kBoogieBass references) — the intro (kIn2P) states the
// riff once, plainly, before any call-and-response begins.
inline constexpr MotifSpec kBoogieBassMotif{.transform = MotifTransform::kRetrograde, .seed = 101};
// The blue-note harp lick, wired only into varD: a diatonic-step "lean on it
// differently" bend on the answer repeat is the harp idiom itself.
inline constexpr MotifSpec kLeadLickMotif{.transform = MotifTransform::kDiatonicTranspose, .seed = 102};
// Motif engine (9210 Wave 1, style-depth program task A): extend variation
// onto drums/chord1/chord2/pad, wired onto the peak variation (varD) plus the
// shared reused Pad/organ-comp arrays. kDisplacement for drums/pad/chord2 (a
// soft evolving push); kRetrograde for the chord1 dominant-7 stabs (the same
// mirrored-answer idiom already proven on the boogie bass above).
inline constexpr MotifSpec kPeakDrumsMotif{.transform = MotifTransform::kDisplacement, .seed = 110};
inline constexpr MotifSpec kPeakChordMotif{.transform = MotifTransform::kRetrograde, .seed = 111};
inline constexpr MotifSpec kSharedPadMotif{.transform = MotifTransform::kDisplacement, .seed = 112};
inline constexpr MotifSpec kSharedChord2Motif{.transform = MotifTransform::kDisplacement, .seed = 113};
inline constexpr StyleEvent kIn1D[] = {{.step=8, .tone=kRide, .octave=0, .vel=56, .gate=120}, {.step=10, .tone=kRide, .octave=0, .vel=48, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=60, .gate=120}, {.step=14, .tone=kRide, .octave=0, .vel=52, .gate=120},
    // bar 2 (style-depth Wave-2 C): arrival -- the ride fills out to a full
    // bed and the shuffle kick/snare backbeat previews, softly, before VarA.
    {.step=16, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=18, .tone=kRide, .octave=0, .vel=46, .gate=120}, {.step=20, .tone=kRide, .octave=0, .vel=60, .gate=120}, {.step=22, .tone=kRide, .octave=0, .vel=46, .gate=120},
    {.step=24, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=26, .tone=kRide, .octave=0, .vel=46, .gate=120}, {.step=28, .tone=kRide, .octave=0, .vel=60, .gate=120}, {.step=30, .tone=kRide, .octave=0, .vel=46, .gate=120},
    {.step=16, .tone=kKick, .octave=0, .vel=64, .gate=100}, {.step=20, .tone=kSnare, .octave=0, .vel=60, .gate=100}, {.step=24, .tone=kKick, .octave=0, .vel=62, .gate=100}, {.step=28, .tone=kSnare, .octave=0, .vel=64, .gate=100},
};
inline constexpr StylePattern kIn1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)}};
inline constexpr StyleEvent kIn2D[] = {{.step=0, .tone=kRide, .octave=0, .vel=64, .gate=120}, {.step=2, .tone=kRide, .octave=0, .vel=50, .gate=120}, {.step=4, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=6, .tone=kRide, .octave=0, .vel=50, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=62, .gate=120}, {.step=10, .tone=kRide, .octave=0, .vel=50, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=14, .tone=kRide, .octave=0, .vel=50, .gate=120}, {.step=0, .tone=kKick, .octave=0, .vel=80, .gate=100}, {.step=8, .tone=kKick, .octave=0, .vel=78, .gate=100}, {.step=4, .tone=kSnare, .octave=0, .vel=76, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=78, .gate=100}};
inline constexpr StyleEvent kIn2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=72, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=70, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=70, .gate=kGateStab}, {.step=8, .tone=kSeventh, .octave=0, .vel=70, .gate=kGateStab}};
inline constexpr StylePattern kIn2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIn2C), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead, .motif=&kSharedPadMotif}};
inline constexpr StyleEvent kAD[] = {
    {.step=0, .tone=kRide, .octave=0, .vel=74, .gate=120}, {.step=2, .tone=kRide, .octave=0, .vel=52, .gate=120}, {.step=4, .tone=kRide, .octave=0, .vel=66, .gate=120}, {.step=6, .tone=kRide, .octave=0, .vel=52, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=72, .gate=120}, {.step=10, .tone=kRide, .octave=0, .vel=52, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=66, .gate=120}, {.step=14, .tone=kRide, .octave=0, .vel=52, .gate=120},
    {.step=0, .tone=kKick, .octave=0, .vel=88, .gate=100}, {.step=8, .tone=kKick, .octave=0, .vel=84, .gate=100}, {.step=4, .tone=kSnare, .octave=0, .vel=94, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=96, .gate=100},
    // bar 2 (style-depth Wave-2 C): rhythm-section continuation (not the
    // bar's own idea -- that's the chord1 turnaround lick below) so the
    // shuffle groove doesn't drop out for a full bar.
    {.step=16, .tone=kRide, .octave=0, .vel=74, .gate=120}, {.step=18, .tone=kRide, .octave=0, .vel=52, .gate=120}, {.step=20, .tone=kRide, .octave=0, .vel=66, .gate=120}, {.step=22, .tone=kRide, .octave=0, .vel=52, .gate=120}, {.step=24, .tone=kRide, .octave=0, .vel=72, .gate=120}, {.step=26, .tone=kRide, .octave=0, .vel=52, .gate=120}, {.step=28, .tone=kRide, .octave=0, .vel=66, .gate=120}, {.step=30, .tone=kRide, .octave=0, .vel=52, .gate=120},
    {.step=16, .tone=kKick, .octave=0, .vel=88, .gate=100}, {.step=24, .tone=kKick, .octave=0, .vel=84, .gate=100}, {.step=20, .tone=kSnare, .octave=0, .vel=94, .gate=100}, {.step=28, .tone=kSnare, .octave=0, .vel=96, .gate=100},
};
// VarA (style-depth Wave-2 C): bar 2 adds a genuine blues turnaround lick --
// a fuller landing stab (all four chord tones) at the end of the bar,
// answering bar 1's two-hit stab pattern instead of repeating it.
inline constexpr StyleEvent kAC[] = {{.step=2, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=76, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=76, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=74, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=74, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=74, .gate=kGateStab},
    {.step=18, .tone=kFifth, .octave=0, .vel=76, .gate=kGateStab}, {.step=18, .tone=kSeventh, .octave=0, .vel=76, .gate=kGateStab},
    {.step=26, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=26, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=26, .tone=kFifth, .octave=0, .vel=78, .gate=kGateStab}, {.step=26, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStab}};
inline constexpr StylePattern kAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass), .motif=&kBoogieBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAC), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kOrganComp), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead, .motif=&kSharedChord2Motif}};
// VarB stays 1 bar (style-depth Wave-2 C): the boogie bass is motif-locked
// here (kBoogieBassMotif), and drums/chord1 already differ from VarA/C/D --
// no genuine bar-2 idea beyond what VarA's turnaround already carries.
inline constexpr StyleEvent kBD[] = {
    {.step=0, .tone=kRide, .octave=0, .vel=78, .gate=120}, {.step=2, .tone=kRide, .octave=0, .vel=56, .gate=120}, {.step=4, .tone=kRide, .octave=0, .vel=70, .gate=120}, {.step=6, .tone=kRide, .octave=0, .vel=56, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=76, .gate=120}, {.step=10, .tone=kRide, .octave=0, .vel=56, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=70, .gate=120}, {.step=14, .tone=kRide, .octave=0, .vel=56, .gate=120},
    {.step=0, .tone=kKick, .octave=0, .vel=92, .gate=100}, {.step=6, .tone=kKick, .octave=0, .vel=76, .gate=100}, {.step=8, .tone=kKick, .octave=0, .vel=90, .gate=100}, {.step=4, .tone=kSnare, .octave=0, .vel=98, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=100, .gate=100}, {.step=14, .tone=kSnare, .octave=0, .vel=60, .gate=50},
};
inline constexpr StyleEvent kBC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=78, .gate=kGateStab}, {.step=8, .tone=kSeventh, .octave=0, .vel=78, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}};
inline constexpr StylePattern kBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass), .motif=&kBoogieBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBC), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kOrganComp), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead, .motif=&kSharedChord2Motif}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercTamb)}};
inline constexpr StyleEvent kFAD[] = {{.step=0, .tone=kKick, .octave=0, .vel=90, .gate=100}, {.step=4, .tone=kSnare, .octave=0, .vel=90, .gate=100}, {.step=8, .tone=kSnare, .octave=0, .vel=84, .gate=100}, {.step=10, .tone=kSnare, .octave=0, .vel=88, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=94, .gate=100}, {.step=14, .tone=kSnare, .octave=0, .vel=100, .gate=100}};
inline constexpr StyleEvent kFBD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=84, .gate=100}, {.step=2, .tone=kSnare, .octave=0, .vel=78, .gate=100}, {.step=4, .tone=kTomHi, .octave=0, .vel=88, .gate=100}, {.step=6, .tone=kTomMid, .octave=0, .vel=86, .gate=100}, {.step=8, .tone=kSnare, .octave=0, .vel=90, .gate=100}, {.step=10, .tone=kTomLow, .octave=0, .vel=92, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=96, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=100, .gate=100}};
inline constexpr StyleEvent kFCD[] = {{.step=0, .tone=kTomHi, .octave=0, .vel=84, .gate=100}, {.step=2, .tone=kTomHi, .octave=0, .vel=80, .gate=100}, {.step=4, .tone=kTomMid, .octave=0, .vel=88, .gate=100}, {.step=6, .tone=kTomMid, .octave=0, .vel=84, .gate=100}, {.step=8, .tone=kTomLow, .octave=0, .vel=92, .gate=100}, {.step=10, .tone=kTomLow, .octave=0, .vel=88, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=96, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=102, .gate=100}};
inline constexpr StyleEvent kFDD[] = {{.step=0, .tone=kSnare, .octave=0, .vel=84, .gate=50}, {.step=2, .tone=kSnare, .octave=0, .vel=78, .gate=50}, {.step=3, .tone=kTomHi, .octave=0, .vel=88, .gate=50}, {.step=4, .tone=kTomHi, .octave=0, .vel=92, .gate=50}, {.step=6, .tone=kTomMid, .octave=0, .vel=90, .gate=50}, {.step=7, .tone=kTomMid, .octave=0, .vel=94, .gate=50}, {.step=8, .tone=kTomLow, .octave=0, .vel=96, .gate=50}, {.step=10, .tone=kTomLow, .octave=0, .vel=92, .gate=50}, {.step=11, .tone=kTomFloor, .octave=0, .vel=98, .gate=50}, {.step=12, .tone=kSnare, .octave=0, .vel=102, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=108, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=112, .gate=kGate8th}};
inline constexpr StylePattern kFAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass), .motif=&kBoogieBassMotif}};
inline constexpr StylePattern kFBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass), .motif=&kBoogieBassMotif}};
inline constexpr StylePattern kFCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass), .motif=&kBoogieBassMotif}};
inline constexpr StylePattern kFDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass), .motif=&kBoogieBassMotif}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercTamb)}};
inline constexpr StyleEvent kE1D[] = {{.step=0, .tone=kKick, .octave=0, .vel=94, .gate=100}, {.step=0, .tone=kCrash, .octave=0, .vel=90, .gate=kGateHalfBar}};
inline constexpr StyleEvent kE1B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=90, .gate=kGateHeld}};
inline constexpr StyleEvent kE1C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=80, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=80, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateHeld}};
inline constexpr StylePattern kE1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1C), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead, .motif=&kSharedPadMotif}};
inline constexpr StyleEvent kE2D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=98, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=94, .gate=100}, {.step=8, .tone=kKick, .octave=0, .vel=88, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=92, .gate=100}};
inline constexpr StyleEvent kE2B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=94, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=-1, .vel=82, .gate=kGateHeld}};
inline constexpr StyleEvent kE2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=82, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=82, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=82, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=82, .gate=kGateHeld}};
inline constexpr StylePattern kE2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2C), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead, .motif=&kSharedPadMotif}};
// varC: slow-drag stop-time — bare ride and kick/snare on the beats, wide space.
// VarC stays 1 bar (style-depth Wave-2 C): the stop-time drag IS the
// variation; a bar-2 repeat would just double the space, not add an idea.
inline constexpr StyleEvent kCD[] = {{.step=0, .tone=kRide, .octave=0, .vel=70, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=66, .gate=120}, {.step=0, .tone=kKick, .octave=0, .vel=86, .gate=100}, {.step=8, .tone=kSnare, .octave=0, .vel=90, .gate=100}};
inline constexpr StyleEvent kCC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=76, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=76, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=74, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=74, .gate=kGateStab}, {.step=8, .tone=kSeventh, .octave=0, .vel=74, .gate=kGateStab}};
inline constexpr StylePattern kCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass), .motif=&kBoogieBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCC), .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead, .motif=&kSharedPadMotif}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kOrganComp), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead, .motif=&kSharedChord2Motif}, {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArpTrip), .gm_program=kArpVoice}};
// varD: peak shuffle — full triplet ride, driving kick/snare, full dominant-7 stabs.
// VarD stays 1 bar (style-depth Wave-2 C): already the peak shuffle;
// extending it would encroach on the break/ending's own arc.
inline constexpr StyleEvent kDD[] = {{.step=0, .tone=kRide, .octave=0, .vel=80, .gate=120}, {.step=2, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=4, .tone=kRide, .octave=0, .vel=72, .gate=120}, {.step=6, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=8, .tone=kRide, .octave=0, .vel=78, .gate=120}, {.step=10, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=12, .tone=kRide, .octave=0, .vel=72, .gate=120}, {.step=14, .tone=kRide, .octave=0, .vel=58, .gate=120}, {.step=0, .tone=kKick, .octave=0, .vel=94, .gate=100}, {.step=6, .tone=kKick, .octave=0, .vel=78, .gate=100}, {.step=8, .tone=kKick, .octave=0, .vel=92, .gate=100}, {.step=4, .tone=kSnare, .octave=0, .vel=100, .gate=100}, {.step=12, .tone=kSnare, .octave=0, .vel=102, .gate=100}, {.step=14, .tone=kSnare, .octave=0, .vel=62, .gate=50}};
inline constexpr StyleEvent kDC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=0, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=8, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}, {.step=8, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}};
inline constexpr StylePattern kDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kDD), .motif=&kPeakDrumsMotif}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBoogieBass), .motif=&kBoogieBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kDC), .voicing=VoicingPolicy::kLead, .motif=&kPeakChordMotif}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead, .motif=&kSharedPadMotif}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kOrganComp), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead, .motif=&kSharedChord2Motif}, {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArpTrip), .gm_program=kArpVoice}, {.role=TrackRole::kLead, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kLeadLick), .gm_program=kLeadVoice, .motif=&kLeadLickMotif}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercTamb)}};
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
    {.type=SectionType::kIntro1, .bars=2, .patterns=Span<const StylePattern>(kIn1P)}, {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIn2P)},
    {.type=SectionType::kVarA, .bars=2, .patterns=Span<const StylePattern>(kAP)}, {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kBP)},
    {.type=SectionType::kVarC, .bars=1, .patterns=Span<const StylePattern>(kCP)}, {.type=SectionType::kVarD, .bars=1, .patterns=Span<const StylePattern>(kDP)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFAP)}, {.type=SectionType::kFillB, .bars=1, .patterns=Span<const StylePattern>(kFBP)}, {.type=SectionType::kFillC, .bars=1, .patterns=Span<const StylePattern>(kFCP)}, {.type=SectionType::kFillD, .bars=1, .patterns=Span<const StylePattern>(kFDP)},
    {.type=SectionType::kBreak, .bars=1, .patterns=Span<const StylePattern>(kBrkP)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kE1P)}, {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kE2P)},
};
// Groove (9100 Wave 1, task B): blues is the loosest genre in the corpus --
// the heaviest deterministic humanize on top of the existing swing/accent feel.
inline constexpr Style kStyle{.name="blues", .sections=Span<const StyleSection>(kSections), .groove={.swing=75, .humanize_timing=20, .humanize_velocity=28, .accent=10, .swing_grid=8}, .tempo=6600};
}  // namespace blues

}  // namespace styles
}  // namespace arrangrr
