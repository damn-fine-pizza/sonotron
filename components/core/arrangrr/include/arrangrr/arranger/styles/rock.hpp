#pragma once

#include "arrangrr/arranger/style_model.hpp"

namespace arrangrr {
namespace styles {


// ---------------------------------------------------------------------------
// "rock": driving and heavier. Busy kick, hard snare backbeat, 8th hats with
// open-hat accents, a crash on the varB downbeat, power (root+fifth) bass with
// octave pushes, power stabs. High velocity.
namespace rock {

// ---------------------------------------------------------------------------
// Fuller-band roles (ADDITIVE): a wide strings pad under the power chords, a
// rock-organ second comp driving the off-beats, a clean-guitar arp and cowbell
// percussion. kPad -> GM 48 (Strings), kChord2 -> GM 18 (Rock Organ), kArp ->
// GM 27 (Clean Guitar); kPerc rides the drum channel.
inline constexpr std::int16_t kPadVoice = 48;
inline constexpr std::int16_t kChord2Voice = 18;
inline constexpr std::int16_t kArpVoice = 27;
// Strings sit back (lower velocity) and hold the triad the power chords omit.
inline constexpr StyleEvent kPadTriad[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=56, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=52, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=56, .gate=kGateHeld},
};
inline constexpr StyleEvent kPad7[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=60, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=56, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=60, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=54, .gate=kGateHeld},
};
inline constexpr StyleEvent kPadRehit[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=60, .gate=kGateHalfBar}, {.step=0, .tone=kThird, .octave=0, .vel=56, .gate=kGateHalfBar}, {.step=0, .tone=kFifth, .octave=0, .vel=60, .gate=kGateHalfBar}, {.step=0, .tone=kSeventh, .octave=0, .vel=54, .gate=kGateHalfBar},
    {.step=8, .tone=kRoot, .octave=0, .vel=58, .gate=kGateHalfBar}, {.step=8, .tone=kThird, .octave=0, .vel=54, .gate=kGateHalfBar}, {.step=8, .tone=kFifth, .octave=0, .vel=58, .gate=kGateHalfBar}, {.step=8, .tone=kSeventh, .octave=0, .vel=52, .gate=kGateHalfBar},
};
// Organ power stabs on the off-beats — drives against the on-beat kChord1.
inline constexpr StyleEvent kChord2Off[] = {
    {.step=2, .tone=kRoot, .octave=0, .vel=74, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=74, .gate=kGateStab}, {.step=6, .tone=kRoot, .octave=0, .vel=70, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=70, .gate=kGateStab},
    {.step=10, .tone=kRoot, .octave=0, .vel=74, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=74, .gate=kGateStab}, {.step=14, .tone=kRoot, .octave=0, .vel=70, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=70, .gate=kGateStab},
};
// Organ sustained under the half-time stomp (varC).
inline constexpr StyleEvent kChord2HalfBar[] = {
    {.step=0, .tone=kThird, .octave=0, .vel=68, .gate=kGateHalfBar}, {.step=0, .tone=kSeventh, .octave=0, .vel=64, .gate=kGateHalfBar},
    {.step=8, .tone=kThird, .octave=0, .vel=66, .gate=kGateHalfBar}, {.step=8, .tone=kSeventh, .octave=0, .vel=62, .gate=kGateHalfBar},
};
// Clean-guitar arpeggio, high register.
inline constexpr StyleEvent kArp8[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=76, .gate=kGateHat}, {.step=2, .tone=kFifth, .octave=0, .vel=68, .gate=kGateHat}, {.step=4, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateHat}, {.step=6, .tone=kRoot, .octave=1, .vel=68, .gate=kGateHat},
    {.step=8, .tone=kFifth, .octave=0, .vel=76, .gate=kGateHat}, {.step=10, .tone=kSeventh, .octave=0, .vel=68, .gate=kGateHat}, {.step=12, .tone=kRoot, .octave=1, .vel=72, .gate=kGateHat}, {.step=14, .tone=kFifth, .octave=0, .vel=68, .gate=kGateHat},
};
// Cowbell quarters + tambourine backbeat; a plain tambourine for fills.
inline constexpr StyleEvent kPercCowbell[] = {
    {.step=0, .tone=kCowbell, .octave=0, .vel=84, .gate=kGateHat}, {.step=4, .tone=kCowbell, .octave=0, .vel=78, .gate=kGateHat}, {.step=8, .tone=kCowbell, .octave=0, .vel=84, .gate=kGateHat}, {.step=12, .tone=kCowbell, .octave=0, .vel=78, .gate=kGateHat},
    {.step=4, .tone=kTambourine, .octave=0, .vel=82, .gate=kGateHat}, {.step=12, .tone=kTambourine, .octave=0, .vel=84, .gate=kGateHat},
};
inline constexpr StyleEvent kPercTamb[] = {
    {.step=4, .tone=kTambourine, .octave=0, .vel=84, .gate=kGateHat}, {.step=12, .tone=kTambourine, .octave=0, .vel=86, .gate=kGateHat},
};
// A rock lead lick for the back half of the peak bar. kScaleDegree keeps it
// diatonic to the live KEY over any chord; the step-10 grace is a kInterval blue
// b3 (3 semitones) off the current chord ROOT — it bends bluesy against the
// harmony, not the key. kLead role -> anchor 72, screaming over the wall.
inline constexpr std::int16_t kLeadVoice = 29;  // Overdriven Guitar
inline constexpr StyleEvent kLeadLick[] = {
    {.step=8,  .tone=4, .octave=0, .vel=104, .gate=kGate8th,      .src=NoteSource::kScaleDegree},   // 5th
    {.step=10, .tone=3, .octave=0, .vel=98,  .gate=kGateStaccato, .src=NoteSource::kInterval},      // blue b3 off the chord root
    {.step=12, .tone=4, .octave=0, .vel=106, .gate=kGate8th,      .src=NoteSource::kScaleDegree},   // 5th
    {.step=14, .tone=7, .octave=0, .vel=110, .gate=kGateBeat,     .src=NoteSource::kScaleDegree},   // up to the octave
};
// Motif engine (9210, Ottorino RANK 10): the blues-rock lead lick, wired only
// into varD — a genuine "call-response guitar lick" idiom (blues-rock solo
// phrasing).
inline constexpr MotifSpec kLeadLickMotif{.transform = MotifTransform::kDiatonicTranspose, .seed = 1001};
// Motif engine (9210 Wave 1, style-depth program task A): extend variation
// onto drums/chord1/chord2/pad, wired onto the peak variation (varD) plus the
// shared reused Pad (kPadRehit, varC+varD) and Chord2 (kChord2Off, varA/B/D)
// arrays. kDisplacement for drums/pad/chord2 (a soft evolving push);
// kRetrograde for the chord1 strums (a legible mirrored answer, only ever
// touches `step` so the strum gesture itself is unaffected).
inline constexpr MotifSpec kPeakDrumsMotif{.transform = MotifTransform::kDisplacement, .seed = 1010};
inline constexpr MotifSpec kPeakChordMotif{.transform = MotifTransform::kRetrograde, .seed = 1011};
inline constexpr MotifSpec kSharedPadMotif{.transform = MotifTransform::kDisplacement, .seed = 1012};
inline constexpr MotifSpec kSharedChord2Motif{.transform = MotifTransform::kDisplacement, .seed = 1013};

inline constexpr StyleEvent kVarADrums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=118, .gate=kGateHat},  {.step=3, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat},
    {.step=8, .tone=kKick, .octave=0, .vel=114, .gate=kGateHat},  {.step=11, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat},
    {.step=4, .tone=kSnare, .octave=0, .vel=116, .gate=kGateHat}, {.step=12, .tone=kSnare, .octave=0, .vel=116, .gate=kGateHat},
    {.step=0, .tone=kClosedHat, .octave=0, .vel=90, .gate=kGateHat},  {.step=2, .tone=kClosedHat, .octave=0, .vel=76, .gate=kGateHat},
    {.step=4, .tone=kClosedHat, .octave=0, .vel=90, .gate=kGateHat},  {.step=6, .tone=kOpenHat, .octave=0, .vel=88, .gate=kGate8th},
    {.step=8, .tone=kClosedHat, .octave=0, .vel=90, .gate=kGateHat},  {.step=10, .tone=kClosedHat, .octave=0, .vel=76, .gate=kGateHat},
    {.step=12, .tone=kClosedHat, .octave=0, .vel=90, .gate=kGateHat}, {.step=14, .tone=kOpenHat, .octave=0, .vel=88, .gate=kGate8th},
};
inline constexpr StyleEvent kVarABass[] = {
    // Power root with an octave push on the off-beats.
    {.step=0, .tone=kRoot, .octave=0, .vel=116, .gate=kGate8th},  {.step=2, .tone=kRoot, .octave=1, .vel=96, .gate=kGateHat},
    {.step=4, .tone=kRoot, .octave=0, .vel=110, .gate=kGate8th},  {.step=6, .tone=kRoot, .octave=1, .vel=96, .gate=kGateHat},
    {.step=8, .tone=kFifth, .octave=0, .vel=112, .gate=kGate8th}, {.step=10, .tone=kFifth, .octave=1, .vel=96, .gate=kGateHat},
    {.step=12, .tone=kRoot, .octave=0, .vel=110, .gate=kGate8th}, {.step=14, .tone=kRoot, .octave=1, .vel=96, .gate=kGateHat},
};
inline constexpr StyleEvent kVarAChord[] = {
    // Downbeat electric-guitar strums — one gesture per hit sweeps the whole live
    // chord high->low (the authored tone is ignored). Collapsed from the former
    // root+fifth block so the sweep fans the full chord, not one strum per tone.
    {.step=0, .tone=kRoot, .octave=0, .vel=110, .gate=kGateBeat, .gesture=ChordGesture::kStrumDown},
    {.step=8, .tone=kRoot, .octave=0, .vel=108, .gate=kGateBeat, .gesture=ChordGesture::kStrumDown},
};
inline constexpr StylePattern kVarAPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarADrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarABass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarAChord)},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead},
    {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kChord2Off), .gm_program=kChord2Voice, .motif=&kSharedChord2Motif},
};

inline constexpr StyleEvent kVarBDrums[] = {
    {.step=0, .tone=kCrash, .octave=0, .vel=118, .gate=kGateBeat},  // crash on the downbeat
    {.step=0, .tone=kKick, .octave=0, .vel=120, .gate=kGateHat},    {.step=2, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat},
    {.step=3, .tone=kKick, .octave=0, .vel=104, .gate=kGateHat},    {.step=8, .tone=kKick, .octave=0, .vel=116, .gate=kGateHat},
    {.step=10, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat},   {.step=11, .tone=kKick, .octave=0, .vel=104, .gate=kGateHat},
    {.step=4, .tone=kSnare, .octave=0, .vel=118, .gate=kGateHat},   {.step=12, .tone=kSnare, .octave=0, .vel=118, .gate=kGateHat},
    {.step=2, .tone=kOpenHat, .octave=0, .vel=92, .gate=kGate8th},  {.step=6, .tone=kOpenHat, .octave=0, .vel=92, .gate=kGate8th},
    {.step=10, .tone=kOpenHat, .octave=0, .vel=92, .gate=kGate8th}, {.step=14, .tone=kOpenHat, .octave=0, .vel=92, .gate=kGate8th},
};
inline constexpr StyleEvent kVarBBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=120, .gate=kGate8th},  {.step=2, .tone=kRoot, .octave=1, .vel=100, .gate=kGateHat},
    {.step=3, .tone=kRoot, .octave=0, .vel=100, .gate=kGateHat},  {.step=4, .tone=kFifth, .octave=0, .vel=112, .gate=kGate8th},
    {.step=8, .tone=kRoot, .octave=0, .vel=116, .gate=kGate8th},  {.step=10, .tone=kSeventh, .octave=0, .vel=100, .gate=kGateHat},
    {.step=12, .tone=kFifth, .octave=0, .vel=112, .gate=kGate8th},
};
inline constexpr StyleEvent kVarBChord[] = {
    // Full downbeat strums plus an upbeat accent on 4 — each a single gesture.
    {.step=0, .tone=kRoot, .octave=0, .vel=112, .gate=kGateBeat, .gesture=ChordGesture::kStrumDown},
    {.step=8, .tone=kRoot, .octave=0, .vel=110, .gate=kGateBeat, .gesture=ChordGesture::kStrumDown},
    {.step=12, .tone=kRoot, .octave=0, .vel=106, .gate=kGateStab, .gesture=ChordGesture::kStrumDown},
};
inline constexpr StylePattern kVarBPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarBDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarBBass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarBChord)},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead},
    {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kChord2Off), .gm_program=kChord2Voice, .motif=&kSharedChord2Motif},
    {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercTamb)},
};

inline constexpr StyleEvent kIntroDrums[] = {
    {.step=12, .tone=kSnare, .octave=0, .vel=96, .gate=100},  {.step=13, .tone=kSnare, .octave=0, .vel=104, .gate=100},
    {.step=14, .tone=kSnare, .octave=0, .vel=112, .gate=100}, {.step=15, .tone=kSnare, .octave=0, .vel=120, .gate=100},
};
inline constexpr StyleEvent kIntroBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=110, .gate=kGateHeld},
};
inline constexpr StylePattern kIntroPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIntroDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntroBass)},
};

inline constexpr StyleEvent kFillDrums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=116, .gate=kGateHat}, {.step=4, .tone=kSnare, .octave=0, .vel=110, .gate=100},
    {.step=6, .tone=kSnare, .octave=0, .vel=104, .gate=100},     {.step=8, .tone=kSnare, .octave=0, .vel=108, .gate=100},
    {.step=10, .tone=kSnare, .octave=0, .vel=112, .gate=100},    {.step=12, .tone=kSnare, .octave=0, .vel=116, .gate=100},
    {.step=13, .tone=kSnare, .octave=0, .vel=118, .gate=100},    {.step=14, .tone=kSnare, .octave=0, .vel=120, .gate=100},
    {.step=15, .tone=kSnare, .octave=0, .vel=120, .gate=100},
};
inline constexpr StyleEvent kFillBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=114, .gate=kGate8th}, {.step=8, .tone=kFifth, .octave=0, .vel=108, .gate=kGate8th},
};
// The flat FILL bass, reused identically across all 4 fills; the Var bass
// (kVarABass..kVarDBass) is already 4 distinct hand-authored ideas, so it is
// not a target.
inline constexpr MotifSpec kFillBassMotif{.transform = MotifTransform::kRetrograde, .seed = 1002};
inline constexpr StylePattern kFillPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFillDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass), .motif=&kFillBassMotif},
};

inline constexpr StyleEvent kEndDrums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=120, .gate=kGateHat},
    {.step=0, .tone=kCrash, .octave=0, .vel=118, .gate=kGateHalfBar},
};
inline constexpr StyleEvent kEndBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=116, .gate=kGateHeld},
};
inline constexpr StyleEvent kEndChord[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=112, .gate=kGateHeld},  {.step=0, .tone=kFifth, .octave=0, .vel=112, .gate=kGateHeld},
    {.step=0, .tone=kRoot, .octave=1, .vel=112, .gate=kGateHeld},
};
inline constexpr StylePattern kEndPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kEndDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEndBass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEndChord)},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead},
};

inline constexpr StyleEvent kIntro2Drums[] = {
    {.step=0, .tone=kCrash, .octave=0, .vel=110, .gate=kGateBeat}, {.step=0, .tone=kKick, .octave=0, .vel=118, .gate=kGateHat}, {.step=8, .tone=kKick, .octave=0, .vel=112, .gate=kGateHat}, {.step=4, .tone=kSnare, .octave=0, .vel=112, .gate=kGateHat}, {.step=12, .tone=kSnare, .octave=0, .vel=114, .gate=kGateHat},
    {.step=0, .tone=kClosedHat, .octave=0, .vel=88, .gate=kGateHat}, {.step=2, .tone=kClosedHat, .octave=0, .vel=74, .gate=kGateHat}, {.step=4, .tone=kClosedHat, .octave=0, .vel=88, .gate=kGateHat}, {.step=6, .tone=kClosedHat, .octave=0, .vel=74, .gate=kGateHat},
    {.step=8, .tone=kClosedHat, .octave=0, .vel=88, .gate=kGateHat}, {.step=10, .tone=kClosedHat, .octave=0, .vel=74, .gate=kGateHat}, {.step=12, .tone=kClosedHat, .octave=0, .vel=88, .gate=kGateHat}, {.step=14, .tone=kOpenHat, .octave=0, .vel=86, .gate=kGate8th},
};
inline constexpr StyleEvent kIntro2Chord[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=104, .gate=kGateBeat}, {.step=0, .tone=kFifth, .octave=0, .vel=104, .gate=kGateBeat}, {.step=8, .tone=kRoot, .octave=0, .vel=102, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=102, .gate=kGateBeat},
};
inline constexpr StylePattern kIntro2Patterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIntro2Drums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntroBass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntro2Chord)},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead},
};

inline constexpr StyleEvent kFillBDrums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=118, .gate=kGateHat}, {.step=4, .tone=kSnare, .octave=0, .vel=112, .gate=120}, {.step=6, .tone=kSnare, .octave=0, .vel=106, .gate=120},
    {.step=8, .tone=kTomHi, .octave=0, .vel=110, .gate=120}, {.step=10, .tone=kTomMid, .octave=0, .vel=112, .gate=120}, {.step=12, .tone=kTomLow, .octave=0, .vel=116, .gate=120}, {.step=14, .tone=kTomFloor, .octave=0, .vel=120, .gate=120},
};
inline constexpr StyleEvent kFillCDrums[] = {
    {.step=0, .tone=kTomHi, .octave=0, .vel=110, .gate=100}, {.step=2, .tone=kTomHi, .octave=0, .vel=106, .gate=100}, {.step=4, .tone=kTomMid, .octave=0, .vel=112, .gate=100}, {.step=6, .tone=kTomMid, .octave=0, .vel=108, .gate=100},
    {.step=8, .tone=kSnare, .octave=0, .vel=114, .gate=100}, {.step=10, .tone=kSnare, .octave=0, .vel=110, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=116, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=120, .gate=100},
};
inline constexpr StyleEvent kFillDDrums[] = {
    {.step=0, .tone=kSnare, .octave=0, .vel=104, .gate=50}, {.step=1, .tone=kSnare, .octave=0, .vel=98, .gate=50}, {.step=2, .tone=kTomHi, .octave=0, .vel=108, .gate=50}, {.step=3, .tone=kTomHi, .octave=0, .vel=102, .gate=50},
    {.step=4, .tone=kTomMid, .octave=0, .vel=110, .gate=50}, {.step=5, .tone=kTomMid, .octave=0, .vel=104, .gate=50}, {.step=6, .tone=kTomLow, .octave=0, .vel=112, .gate=50}, {.step=7, .tone=kTomLow, .octave=0, .vel=106, .gate=50},
    {.step=8, .tone=kTomFloor, .octave=0, .vel=114, .gate=50}, {.step=9, .tone=kTomFloor, .octave=0, .vel=108, .gate=50}, {.step=10, .tone=kSnare, .octave=0, .vel=116, .gate=50}, {.step=11, .tone=kSnare, .octave=0, .vel=110, .gate=50},
    {.step=12, .tone=kSnare, .octave=0, .vel=118, .gate=50}, {.step=13, .tone=kSnare, .octave=0, .vel=114, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=120, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=120, .gate=kGate8th},
};
inline constexpr StylePattern kFillBPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFillBDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass), .motif=&kFillBassMotif},
};
inline constexpr StylePattern kFillCPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFillCDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass), .motif=&kFillBassMotif},
};
inline constexpr StylePattern kFillDPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFillDDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass), .motif=&kFillBassMotif},
    {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercTamb)},
};

inline constexpr StyleEvent kEnd2Drums[] = {
    {.step=0, .tone=kCrash, .octave=0, .vel=120, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=120, .gate=kGateHat}, {.step=8, .tone=kSnare, .octave=0, .vel=114, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=116, .gate=kGateHat}, {.step=12, .tone=kCrash, .octave=0, .vel=112, .gate=kGateBeat},
};
inline constexpr StyleEvent kEnd2Bass[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=118, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=110, .gate=kGateHeld}};
inline constexpr StyleEvent kEnd2Chord[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=114, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=114, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=1, .vel=114, .gate=kGateHeld},
};
inline constexpr StylePattern kEnd2Patterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kEnd2Drums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEnd2Bass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEnd2Chord)},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead},
};

// varC: half-time stomp — huge kick, backbeat on beat 3, open-hat quarters, held power chord.
inline constexpr StyleEvent kVarCDrums[] = {{.step=0, .tone=kCrash, .octave=0, .vel=110, .gate=kGateBeat}, {.step=0, .tone=kKick, .octave=0, .vel=120, .gate=kGateHat}, {.step=4, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat}, {.step=8, .tone=kKick, .octave=0, .vel=116, .gate=kGateHat}, {.step=8, .tone=kSnare, .octave=0, .vel=118, .gate=kGateHat}, {.step=0, .tone=kOpenHat, .octave=0, .vel=90, .gate=kGate8th}, {.step=4, .tone=kOpenHat, .octave=0, .vel=86, .gate=kGate8th}, {.step=8, .tone=kOpenHat, .octave=0, .vel=90, .gate=kGate8th}, {.step=12, .tone=kOpenHat, .octave=0, .vel=86, .gate=kGate8th}};
inline constexpr StyleEvent kVarCBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=116, .gate=kGate8th}, {.step=4, .tone=kRoot, .octave=1, .vel=96, .gate=kGateHat}, {.step=8, .tone=kFifth, .octave=0, .vel=112, .gate=kGate8th}, {.step=12, .tone=kRoot, .octave=1, .vel=96, .gate=kGateHat}};
inline constexpr StyleEvent kVarCChord[] = {{.step=0, .tone=kRoot, .octave=0, .vel=110, .gate=kGateHalfBar}, {.step=0, .tone=kFifth, .octave=0, .vel=110, .gate=kGateHalfBar}, {.step=8, .tone=kRoot, .octave=0, .vel=108, .gate=kGateHalfBar}, {.step=8, .tone=kFifth, .octave=0, .vel=108, .gate=kGateHalfBar}};
inline constexpr StylePattern kVarCPatterns[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarCDrums)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarCBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarCChord)},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadRehit), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead, .motif=&kSharedPadMotif},
    {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kChord2HalfBar), .gm_program=kChord2Voice},
    {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArp8), .gm_program=kArpVoice}};
// varD: peak — four-on-the-floor eighth kick, crash, open-hat accents, power+7th stabs.
inline constexpr StyleEvent kVarDDrums[] = {{.step=0, .tone=kCrash, .octave=0, .vel=118, .gate=kGateBeat}, {.step=0, .tone=kKick, .octave=0, .vel=120, .gate=kGateHat}, {.step=2, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat}, {.step=4, .tone=kKick, .octave=0, .vel=110, .gate=kGateHat}, {.step=6, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat}, {.step=8, .tone=kKick, .octave=0, .vel=116, .gate=kGateHat}, {.step=10, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat}, {.step=12, .tone=kKick, .octave=0, .vel=110, .gate=kGateHat}, {.step=14, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat}, {.step=4, .tone=kSnare, .octave=0, .vel=118, .gate=kGateHat}, {.step=12, .tone=kSnare, .octave=0, .vel=118, .gate=kGateHat}, {.step=2, .tone=kOpenHat, .octave=0, .vel=92, .gate=kGate8th}, {.step=6, .tone=kOpenHat, .octave=0, .vel=92, .gate=kGate8th}, {.step=10, .tone=kOpenHat, .octave=0, .vel=92, .gate=kGate8th}, {.step=14, .tone=kOpenHat, .octave=0, .vel=92, .gate=kGate8th}};
inline constexpr StyleEvent kVarDBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=120, .gate=kGate8th}, {.step=2, .tone=kRoot, .octave=1, .vel=100, .gate=kGateHat}, {.step=4, .tone=kFifth, .octave=0, .vel=112, .gate=kGate8th}, {.step=6, .tone=kRoot, .octave=1, .vel=100, .gate=kGateHat}, {.step=8, .tone=kRoot, .octave=0, .vel=116, .gate=kGate8th}, {.step=10, .tone=kSeventh, .octave=0, .vel=100, .gate=kGateHat}, {.step=12, .tone=kFifth, .octave=0, .vel=112, .gate=kGate8th}, {.step=14, .tone=kRoot, .octave=1, .vel=100, .gate=kGateHat}};
inline constexpr StyleEvent kVarDChord[] = {{.step=0, .tone=kRoot, .octave=0, .vel=114, .gate=kGateBeat, .gesture=ChordGesture::kStrumDown}, {.step=8, .tone=kRoot, .octave=0, .vel=112, .gate=kGateBeat, .gesture=ChordGesture::kStrumDown}, {.step=12, .tone=kRoot, .octave=0, .vel=108, .gate=kGateStab, .gesture=ChordGesture::kStrumDown}};
inline constexpr StylePattern kVarDPatterns[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarDDrums), .motif=&kPeakDrumsMotif}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarDBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarDChord), .motif=&kPeakChordMotif},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadRehit), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead, .motif=&kSharedPadMotif},
    {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kChord2Off), .gm_program=kChord2Voice, .motif=&kSharedChord2Motif},
    {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArp8), .gm_program=kArpVoice},
    {.role=TrackRole::kLead, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kLeadLick), .gm_program=kLeadVoice, .motif=&kLeadLickMotif},
    {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercCowbell)}};

// varBreak: the rock stop — one accented downbeat (kick+crash, a bass pop, a
// power-chord stab held as a literal synchronized block), then wide silence,
// then a snare/tom pickup on beat 4 that throws the groove back in.
inline constexpr StyleEvent kBrkD[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=120, .gate=kGateHat}, {.step=0, .tone=kCrash, .octave=0, .vel=116, .gate=kGateStab},
    {.step=12, .tone=kSnare, .octave=0, .vel=104, .gate=100}, {.step=13, .tone=kSnare, .octave=0, .vel=110, .gate=100}, {.step=14, .tone=kTomMid, .octave=0, .vel=114, .gate=100}, {.step=15, .tone=kTomLow, .octave=0, .vel=118, .gate=100},
};
inline constexpr StyleEvent kBrkB[] = {{.step=0, .tone=kRoot, .octave=0, .vel=118, .gate=kGateStab}};
inline constexpr StyleEvent kBrkC[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=112, .gate=kGateStab}, {.step=0, .tone=kFifth, .octave=0, .vel=112, .gate=kGateStab}, {.step=0, .tone=kRoot, .octave=1, .vel=112, .gate=kGateStab},
};
inline constexpr StylePattern kBrkP[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kBrkD)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBrkB)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBrkC)},
};

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
    {.type=SectionType::kBreak, .bars=1, .patterns=Span<const StylePattern>(kBrkP)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kEndPatterns)},
    {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kEnd2Patterns)},
};
// Groove (9100 Wave 1, task B): driving, heavier live-band genre -- a
// moderate deterministic humanize, a touch more than the mainstream tier.
inline constexpr Style kStyle{.name="rock", .sections=Span<const StyleSection>(kSections),
                              .groove={.humanize_timing=8, .humanize_velocity=18}, .tempo=13000};

}  // namespace rock

}  // namespace styles
}  // namespace arrangrr
