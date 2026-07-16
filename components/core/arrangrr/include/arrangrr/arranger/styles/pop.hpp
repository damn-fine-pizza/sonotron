#pragma once

#include "arrangrr/arranger/style_model.hpp"

namespace arrangrr {
namespace styles {


// ---------------------------------------------------------------------------
// "pop": straight 8ths, clean backbeat. Kick on 1 & 3, snare on 2 & 4, steady
// closed-hat 8ths, simple root-fifth bass, triad stabs on the off-beats.
// Bright and mid-velocity.
namespace pop {

// ---------------------------------------------------------------------------
// Fuller-band roles (ADDITIVE): sustained pad, a second comp that fills the
// on-beats under pop's off-beat upstroke, a bright arp and extra percussion.
// kPad -> GM 89 (Pad2 warm), kChord2 -> GM 4 (E.Piano1), kArp -> GM 10 (Music
// Box); kPerc rides the drum channel.
inline constexpr std::int16_t kPadVoice = 89;
inline constexpr std::int16_t kChord2Voice = 4;
inline constexpr std::int16_t kArpVoice = 10;
inline constexpr StyleEvent kPadTriad[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=58, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=56, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=58, .gate=kGateHeld},
};
inline constexpr StyleEvent kPad7[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=60, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=58, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=60, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=54, .gate=kGateHeld},
};
inline constexpr StyleEvent kPadRehit[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=60, .gate=kGateHalfBar}, {.step=0, .tone=kThird, .octave=0, .vel=58, .gate=kGateHalfBar}, {.step=0, .tone=kFifth, .octave=0, .vel=60, .gate=kGateHalfBar}, {.step=0, .tone=kSeventh, .octave=0, .vel=54, .gate=kGateHalfBar},
    {.step=8, .tone=kRoot, .octave=0, .vel=58, .gate=kGateHalfBar}, {.step=8, .tone=kThird, .octave=0, .vel=56, .gate=kGateHalfBar}, {.step=8, .tone=kFifth, .octave=0, .vel=58, .gate=kGateHalfBar}, {.step=8, .tone=kSeventh, .octave=0, .vel=52, .gate=kGateHalfBar},
};
// Chord2 on the down-beats (kChord1 stabs the off-beats) — the two interlock.
inline constexpr StyleEvent kChord2On[] = {
    {.step=0, .tone=kThird, .octave=0, .vel=60, .gate=kGateBeat}, {.step=0, .tone=kFifth, .octave=0, .vel=60, .gate=kGateBeat},
    {.step=8, .tone=kThird, .octave=0, .vel=58, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=58, .gate=kGateBeat},
};
// Chord2 filling the off-beats kChord1 leaves open in the dance variations.
inline constexpr StyleEvent kChord2Fill[] = {
    {.step=6, .tone=kRoot, .octave=0, .vel=60, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=60, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=60, .gate=kGateStab},
    {.step=14, .tone=kRoot, .octave=0, .vel=58, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=58, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=58, .gate=kGateStab},
};
inline constexpr StyleEvent kArp8[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=68, .gate=kGateHat}, {.step=2, .tone=kThird, .octave=0, .vel=62, .gate=kGateHat}, {.step=4, .tone=kFifth, .octave=0, .vel=66, .gate=kGateHat}, {.step=6, .tone=kSeventh, .octave=0, .vel=62, .gate=kGateHat},
    {.step=8, .tone=kRoot, .octave=1, .vel=68, .gate=kGateHat}, {.step=10, .tone=kSeventh, .octave=0, .vel=62, .gate=kGateHat}, {.step=12, .tone=kFifth, .octave=0, .vel=66, .gate=kGateHat}, {.step=14, .tone=kThird, .octave=0, .vel=62, .gate=kGateHat},
};
inline constexpr StyleEvent kPercTamb[] = {
    {.step=4, .tone=kTambourine, .octave=0, .vel=82, .gate=kGateHat}, {.step=12, .tone=kTambourine, .octave=0, .vel=84, .gate=kGateHat},
};
inline constexpr StyleEvent kPercShake[] = {
    {.step=0, .tone=kCabasa, .octave=0, .vel=66, .gate=kGateHat}, {.step=2, .tone=kCabasa, .octave=0, .vel=56, .gate=kGateHat}, {.step=4, .tone=kTambourine, .octave=0, .vel=82, .gate=kGateHat}, {.step=6, .tone=kCabasa, .octave=0, .vel=56, .gate=kGateHat},
    {.step=8, .tone=kCabasa, .octave=0, .vel=66, .gate=kGateHat}, {.step=10, .tone=kCabasa, .octave=0, .vel=56, .gate=kGateHat}, {.step=12, .tone=kTambourine, .octave=0, .vel=84, .gate=kGateHat}, {.step=14, .tone=kCabasa, .octave=0, .vel=56, .gate=kGateHat},
};
// A tasteful synth hook for the peak (varD): a major-pentatonic answer in the
// back half of the bar. kScaleDegree keeps the line locked to the live KEY over
// any chord change; kLead role -> anchor 72, singing above the comp.
inline constexpr std::int16_t kLeadVoice = 81;  // Lead 2 (sawtooth) — pop synth hook
inline constexpr StyleEvent kLeadHook[] = {
    {.step=8,  .tone=4, .octave=0, .vel=92, .gate=kGate8th,  .src=NoteSource::kScaleDegree},   // 5th
    {.step=10, .tone=5, .octave=0, .vel=86, .gate=kGate8th,  .src=NoteSource::kScaleDegree},   // 6th
    {.step=12, .tone=7, .octave=0, .vel=96, .gate=kGate8th,  .src=NoteSource::kScaleDegree},   // up to the octave
    {.step=14, .tone=4, .octave=0, .vel=88, .gate=kGateBeat, .src=NoteSource::kScaleDegree},   // settle on the 5th
};
// Motif engine (9210, Ottorino RANK 11): the pop synth hook, wired only into
// varD — pop is the most genre-generic style in the corpus, so this is the
// smallest per-effort payoff among the safe Option-1 styles, but real.
inline constexpr MotifSpec kLeadHookMotif{.transform = MotifTransform::kDiatonicTranspose, .seed = 1101};

inline constexpr StyleEvent kVarADrums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=102, .gate=kGateHat},  {.step=8, .tone=kKick, .octave=0, .vel=98, .gate=kGateHat},
    {.step=4, .tone=kSnare, .octave=0, .vel=100, .gate=kGateHat}, {.step=12, .tone=kSnare, .octave=0, .vel=100, .gate=kGateHat},
    {.step=0, .tone=kClosedHat, .octave=0, .vel=78, .gate=kGateHat},  {.step=2, .tone=kClosedHat, .octave=0, .vel=64, .gate=kGateHat},
    {.step=4, .tone=kClosedHat, .octave=0, .vel=78, .gate=kGateHat},  {.step=6, .tone=kClosedHat, .octave=0, .vel=64, .gate=kGateHat},
    {.step=8, .tone=kClosedHat, .octave=0, .vel=78, .gate=kGateHat},  {.step=10, .tone=kClosedHat, .octave=0, .vel=64, .gate=kGateHat},
    {.step=12, .tone=kClosedHat, .octave=0, .vel=78, .gate=kGateHat}, {.step=14, .tone=kClosedHat, .octave=0, .vel=64, .gate=kGateHat},
};
inline constexpr StyleEvent kVarABass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=98, .gate=kGate8th},  {.step=4, .tone=kFifth, .octave=0, .vel=88, .gate=kGate8th},
    {.step=8, .tone=kRoot, .octave=0, .vel=94, .gate=kGate8th},  {.step=12, .tone=kFifth, .octave=0, .vel=88, .gate=kGate8th},
};
inline constexpr StyleEvent kVarAChord[] = {
    // Off-beat triad stabs (root/3rd/5th) — the classic pop upstroke.
    {.step=2, .tone=kRoot, .octave=0, .vel=88, .gate=kGateStab},  {.step=2, .tone=kThird, .octave=0, .vel=88, .gate=kGateStab},  {.step=2, .tone=kFifth, .octave=0, .vel=88, .gate=kGateStab},
    {.step=6, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab},  {.step=6, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab},  {.step=6, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab},
    {.step=10, .tone=kRoot, .octave=0, .vel=88, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=88, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=88, .gate=kGateStab},
    {.step=14, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab},
};
inline constexpr StylePattern kVarAPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarADrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarABass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarAChord)},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead},
    {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kChord2On), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead},
};

inline constexpr StyleEvent kVarBDrums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=104, .gate=kGateHat},  {.step=6, .tone=kKick, .octave=0, .vel=92, .gate=kGateHat},
    {.step=8, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat},  {.step=10, .tone=kKick, .octave=0, .vel=90, .gate=kGateHat},
    {.step=4, .tone=kSnare, .octave=0, .vel=102, .gate=kGateHat}, {.step=12, .tone=kSnare, .octave=0, .vel=102, .gate=kGateHat},
    {.step=4, .tone=kClap, .octave=0, .vel=90, .gate=kGateHat},   {.step=12, .tone=kClap, .octave=0, .vel=90, .gate=kGateHat},
    {.step=0, .tone=kClosedHat, .octave=0, .vel=80, .gate=kGateHat},  {.step=2, .tone=kClosedHat, .octave=0, .vel=66, .gate=kGateHat},
    {.step=4, .tone=kClosedHat, .octave=0, .vel=80, .gate=kGateHat},  {.step=6, .tone=kClosedHat, .octave=0, .vel=66, .gate=kGateHat},
    {.step=8, .tone=kClosedHat, .octave=0, .vel=80, .gate=kGateHat},  {.step=10, .tone=kClosedHat, .octave=0, .vel=66, .gate=kGateHat},
    {.step=12, .tone=kClosedHat, .octave=0, .vel=80, .gate=kGateHat}, {.step=14, .tone=kOpenHat, .octave=0, .vel=78, .gate=kGate8th},
};
inline constexpr StyleEvent kVarBBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=100, .gate=kGate8th}, {.step=6, .tone=kRoot, .octave=0, .vel=78, .gate=kGateHat},
    {.step=8, .tone=kFifth, .octave=0, .vel=92, .gate=kGate8th}, {.step=10, .tone=kRoot, .octave=1, .vel=80, .gate=kGateHat},
    {.step=12, .tone=kFifth, .octave=0, .vel=88, .gate=kGate8th},
};
inline constexpr StyleEvent kVarBChord[] = {
    // Fuller four-note stabs on every off-beat.
    {.step=2, .tone=kRoot, .octave=0, .vel=90, .gate=kGateStab},  {.step=2, .tone=kThird, .octave=0, .vel=90, .gate=kGateStab},  {.step=2, .tone=kFifth, .octave=0, .vel=90, .gate=kGateStab},  {.step=2, .tone=kSeventh, .octave=0, .vel=90, .gate=kGateStab},
    {.step=6, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab},  {.step=6, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab},  {.step=6, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab},
    {.step=10, .tone=kRoot, .octave=0, .vel=90, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=90, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=90, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=90, .gate=kGateStab},
    {.step=14, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab},
};
inline constexpr StylePattern kVarBPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarBDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarBBass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarBChord)},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead},
    {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kChord2On), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead},
    {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercTamb)},
};

inline constexpr StyleEvent kIntroDrums[] = {
    // Hat pickup crescendo into the downbeat.
    {.step=8, .tone=kClosedHat, .octave=0, .vel=62, .gate=kGateHat},  {.step=10, .tone=kClosedHat, .octave=0, .vel=70, .gate=kGateHat},
    {.step=12, .tone=kClosedHat, .octave=0, .vel=78, .gate=kGateHat}, {.step=14, .tone=kClosedHat, .octave=0, .vel=88, .gate=kGateHat},
};
inline constexpr StyleEvent kIntroBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=90, .gate=kGateHeld},
};
inline constexpr StylePattern kIntroPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIntroDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntroBass)},
};

inline constexpr StyleEvent kFillDrums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat},  {.step=4, .tone=kSnare, .octave=0, .vel=96, .gate=kGateHat},
    {.step=8, .tone=kSnare, .octave=0, .vel=88, .gate=100},       {.step=10, .tone=kSnare, .octave=0, .vel=94, .gate=100},
    {.step=12, .tone=kSnare, .octave=0, .vel=102, .gate=100},     {.step=14, .tone=kSnare, .octave=0, .vel=110, .gate=100},
};
inline constexpr StyleEvent kFillBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=98, .gate=kGate8th}, {.step=8, .tone=kFifth, .octave=0, .vel=88, .gate=kGate8th},
};
// The flat FILL bass, reused identically across all 4 fills; the Var bass
// (kVarABass..kVarDBass) is already 4 distinct hand-authored ideas.
inline constexpr MotifSpec kFillBassMotif{.transform = MotifTransform::kRetrograde, .seed = 1102};
inline constexpr StylePattern kFillPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFillDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass), .motif=&kFillBassMotif},
};

inline constexpr StyleEvent kEndDrums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=110, .gate=kGateHat},
    {.step=0, .tone=kCrash, .octave=0, .vel=104, .gate=kGateHalfBar},
};
inline constexpr StyleEvent kEndBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=98, .gate=kGateHeld},
};
inline constexpr StyleEvent kEndChord[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=88, .gate=kGateHeld},  {.step=0, .tone=kThird, .octave=0, .vel=88, .gate=kGateHeld},
    {.step=0, .tone=kFifth, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=88, .gate=kGateHeld},
};
inline constexpr StylePattern kEndPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kEndDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEndBass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEndChord)},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead},
};

inline constexpr StyleEvent kIntro2Drums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat}, {.step=8, .tone=kKick, .octave=0, .vel=96, .gate=kGateHat}, {.step=4, .tone=kClap, .octave=0, .vel=92, .gate=kGateHat}, {.step=12, .tone=kClap, .octave=0, .vel=92, .gate=kGateHat},
    {.step=0, .tone=kClosedHat, .octave=0, .vel=74, .gate=kGateHat}, {.step=2, .tone=kClosedHat, .octave=0, .vel=60, .gate=kGateHat}, {.step=4, .tone=kClosedHat, .octave=0, .vel=74, .gate=kGateHat}, {.step=6, .tone=kClosedHat, .octave=0, .vel=60, .gate=kGateHat},
    {.step=8, .tone=kClosedHat, .octave=0, .vel=74, .gate=kGateHat}, {.step=10, .tone=kClosedHat, .octave=0, .vel=60, .gate=kGateHat}, {.step=12, .tone=kClosedHat, .octave=0, .vel=74, .gate=kGateHat}, {.step=14, .tone=kOpenHat, .octave=0, .vel=76, .gate=kGate8th},
};
inline constexpr StyleEvent kIntro2Chord[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=78, .gate=kGateBeat}, {.step=0, .tone=kThird, .octave=0, .vel=78, .gate=kGateBeat}, {.step=0, .tone=kFifth, .octave=0, .vel=78, .gate=kGateBeat},
    {.step=8, .tone=kRoot, .octave=0, .vel=76, .gate=kGateBeat}, {.step=8, .tone=kThird, .octave=0, .vel=76, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=76, .gate=kGateBeat},
};
inline constexpr StylePattern kIntro2Patterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIntro2Drums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntroBass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntro2Chord)},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead},
};

inline constexpr StyleEvent kFillBDrums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=100, .gate=kGateHat}, {.step=4, .tone=kSnare, .octave=0, .vel=98, .gate=120}, {.step=6, .tone=kClap, .octave=0, .vel=90, .gate=120},
    {.step=8, .tone=kSnare, .octave=0, .vel=92, .gate=120}, {.step=10, .tone=kTomMid, .octave=0, .vel=98, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=104, .gate=120}, {.step=14, .tone=kTomLow, .octave=0, .vel=110, .gate=120},
};
inline constexpr StyleEvent kFillCDrums[] = {
    {.step=0, .tone=kSnare, .octave=0, .vel=94, .gate=100}, {.step=2, .tone=kSnare, .octave=0, .vel=88, .gate=100}, {.step=4, .tone=kClap, .octave=0, .vel=98, .gate=100}, {.step=6, .tone=kTomHi, .octave=0, .vel=96, .gate=100},
    {.step=8, .tone=kTomMid, .octave=0, .vel=102, .gate=100}, {.step=10, .tone=kTomMid, .octave=0, .vel=100, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=108, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=114, .gate=100},
};
inline constexpr StyleEvent kFillDDrums[] = {
    {.step=0, .tone=kSnare, .octave=0, .vel=92, .gate=50}, {.step=1, .tone=kSnare, .octave=0, .vel=86, .gate=50}, {.step=2, .tone=kSnare, .octave=0, .vel=98, .gate=50}, {.step=3, .tone=kSnare, .octave=0, .vel=90, .gate=50},
    {.step=4, .tone=kClap, .octave=0, .vel=102, .gate=50}, {.step=5, .tone=kSnare, .octave=0, .vel=94, .gate=50}, {.step=6, .tone=kTomHi, .octave=0, .vel=104, .gate=50}, {.step=7, .tone=kTomHi, .octave=0, .vel=98, .gate=50},
    {.step=8, .tone=kTomMid, .octave=0, .vel=106, .gate=50}, {.step=9, .tone=kTomMid, .octave=0, .vel=100, .gate=50}, {.step=10, .tone=kTomLow, .octave=0, .vel=110, .gate=50}, {.step=11, .tone=kTomLow, .octave=0, .vel=104, .gate=50},
    {.step=12, .tone=kTomFloor, .octave=0, .vel=114, .gate=50}, {.step=13, .tone=kTomFloor, .octave=0, .vel=108, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=118, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=120, .gate=kGate8th},
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
    {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercShake)},
};

inline constexpr StyleEvent kEnd2Drums[] = {
    {.step=0, .tone=kCrash, .octave=0, .vel=108, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=112, .gate=kGateHat}, {.step=8, .tone=kClap, .octave=0, .vel=98, .gate=kGateHat}, {.step=8, .tone=kKick, .octave=0, .vel=104, .gate=kGateHat},
};
inline constexpr StyleEvent kEnd2Bass[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=100, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=92, .gate=kGateHeld}};
inline constexpr StyleEvent kEnd2Chord[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=88, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=88, .gate=kGateHeld},
};
inline constexpr StylePattern kEnd2Patterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kEnd2Drums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEnd2Bass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEnd2Chord)},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead},
};

// varC: four-on-the-floor dance-pop pump — a different lift from the backbeat A/B.
inline constexpr StyleEvent kVarCDrums[] = {{.step=0, .tone=kKick, .octave=0, .vel=104, .gate=kGateHat}, {.step=4, .tone=kKick, .octave=0, .vel=98, .gate=kGateHat}, {.step=8, .tone=kKick, .octave=0, .vel=102, .gate=kGateHat}, {.step=12, .tone=kKick, .octave=0, .vel=98, .gate=kGateHat}, {.step=4, .tone=kClap, .octave=0, .vel=90, .gate=kGateHat}, {.step=12, .tone=kClap, .octave=0, .vel=90, .gate=kGateHat}, {.step=0, .tone=kClosedHat, .octave=0, .vel=74, .gate=kGateHat}, {.step=2, .tone=kClosedHat, .octave=0, .vel=62, .gate=kGateHat}, {.step=4, .tone=kClosedHat, .octave=0, .vel=74, .gate=kGateHat}, {.step=6, .tone=kClosedHat, .octave=0, .vel=62, .gate=kGateHat}, {.step=8, .tone=kClosedHat, .octave=0, .vel=74, .gate=kGateHat}, {.step=10, .tone=kClosedHat, .octave=0, .vel=62, .gate=kGateHat}, {.step=12, .tone=kClosedHat, .octave=0, .vel=74, .gate=kGateHat}, {.step=14, .tone=kClosedHat, .octave=0, .vel=62, .gate=kGateHat}};
inline constexpr StyleEvent kVarCBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=96, .gate=kGate8th}, {.step=4, .tone=kFifth, .octave=0, .vel=86, .gate=kGate8th}, {.step=8, .tone=kRoot, .octave=0, .vel=94, .gate=kGate8th}, {.step=12, .tone=kFifth, .octave=0, .vel=86, .gate=kGate8th}};
inline constexpr StyleEvent kVarCChord[] = {{.step=2, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=82, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=82, .gate=kGateStab}};
inline constexpr StylePattern kVarCPatterns[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarCDrums)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarCBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarCChord)},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadRehit), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead},
    {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kChord2Fill), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead},
    {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArp8), .gm_program=kArpVoice},
    {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercShake)}};
// varD: peak — kick pushes, clap-doubled backbeat, 16th hats, driving bass, full 7th stabs.
inline constexpr StyleEvent kVarDDrums[] = {{.step=0, .tone=kKick, .octave=0, .vel=106, .gate=kGateHat}, {.step=6, .tone=kKick, .octave=0, .vel=92, .gate=kGateHat}, {.step=8, .tone=kKick, .octave=0, .vel=102, .gate=kGateHat}, {.step=10, .tone=kKick, .octave=0, .vel=92, .gate=kGateHat}, {.step=4, .tone=kSnare, .octave=0, .vel=104, .gate=kGateHat}, {.step=12, .tone=kSnare, .octave=0, .vel=104, .gate=kGateHat}, {.step=4, .tone=kClap, .octave=0, .vel=94, .gate=kGateHat}, {.step=12, .tone=kClap, .octave=0, .vel=94, .gate=kGateHat}, {.step=0, .tone=kClosedHat, .octave=0, .vel=80, .gate=50}, {.step=1, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=2, .tone=kClosedHat, .octave=0, .vel=70, .gate=50}, {.step=3, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=80, .gate=50}, {.step=5, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=6, .tone=kClosedHat, .octave=0, .vel=70, .gate=50}, {.step=7, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=80, .gate=50}, {.step=9, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=70, .gate=50}, {.step=11, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=80, .gate=50}, {.step=13, .tone=kClosedHat, .octave=0, .vel=60, .gate=50}, {.step=14, .tone=kOpenHat, .octave=0, .vel=80, .gate=kGate8th}};
inline constexpr StyleEvent kVarDBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=104, .gate=kGate8th}, {.step=2, .tone=kRoot, .octave=1, .vel=82, .gate=kGateHat}, {.step=4, .tone=kFifth, .octave=0, .vel=94, .gate=kGate8th}, {.step=6, .tone=kRoot, .octave=0, .vel=84, .gate=kGateHat}, {.step=8, .tone=kRoot, .octave=0, .vel=100, .gate=kGate8th}, {.step=10, .tone=kRoot, .octave=1, .vel=82, .gate=kGateHat}, {.step=12, .tone=kFifth, .octave=0, .vel=94, .gate=kGate8th}, {.step=14, .tone=kSeventh, .octave=0, .vel=88, .gate=kGateHat}};
inline constexpr StyleEvent kVarDChord[] = {{.step=2, .tone=kRoot, .octave=0, .vel=90, .gate=kGateStab}, {.step=2, .tone=kThird, .octave=0, .vel=90, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=90, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=90, .gate=kGateStab}, {.step=6, .tone=kRoot, .octave=0, .vel=86, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=86, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=86, .gate=kGateStab}, {.step=6, .tone=kSeventh, .octave=0, .vel=86, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=90, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=90, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=90, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=90, .gate=kGateStab}, {.step=14, .tone=kRoot, .octave=0, .vel=86, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=86, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=86, .gate=kGateStab}, {.step=14, .tone=kSeventh, .octave=0, .vel=86, .gate=kGateStab}};
inline constexpr StylePattern kVarDPatterns[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarDDrums)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarDBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarDChord)},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadRehit), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead},
    {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kChord2On), .gm_program=kChord2Voice, .voicing=VoicingPolicy::kLead},
    {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArp8), .gm_program=kArpVoice},
    {.role=TrackRole::kLead, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kLeadHook), .gm_program=kLeadVoice, .motif=&kLeadHookMotif},
    {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercShake)}};

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
inline constexpr Style kStyle{.name="pop", .sections=Span<const StyleSection>(kSections), .tempo=12000};

}  // namespace pop

}  // namespace styles
}  // namespace arrangrr
