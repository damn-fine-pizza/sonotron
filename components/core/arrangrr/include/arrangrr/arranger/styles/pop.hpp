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
// Re-articulated at step 16 too (style-depth Wave-2 C): VarA is now 2 bars,
// and the sustain re-triggers every bar exactly as it always has when the
// section looped -- a background hold, not the section's own bar-to-bar
// variation. VarB/Intro2 stay 1 bar and never read step>=16, so this is safe.
inline constexpr StyleEvent kPadTriad[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 58, .gate = kGateHeld},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 56, .gate = kGateHeld},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 58, .gate = kGateHeld},
    {.step = 16, .tone = kRoot, .octave = 0, .vel = 58, .gate = kGateHeld},
    {.step = 16, .tone = kThird, .octave = 0, .vel = 56, .gate = kGateHeld},
    {.step = 16, .tone = kFifth, .octave = 0, .vel = 58, .gate = kGateHeld},
};
inline constexpr StyleEvent kPad7[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 60, .gate = kGateHeld},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 58, .gate = kGateHeld},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 60, .gate = kGateHeld},
    {.step = 0, .tone = kSeventh, .octave = 0, .vel = 54, .gate = kGateHeld},
    // bar 2 (ending1 only, Part A): restate the held 7th chord into bar 2.
    {.step = 16, .tone = kRoot, .octave = 0, .vel = 60, .gate = kGateHeld},
    {.step = 16, .tone = kThird, .octave = 0, .vel = 58, .gate = kGateHeld},
    {.step = 16, .tone = kFifth, .octave = 0, .vel = 60, .gate = kGateHeld},
    {.step = 16, .tone = kSeventh, .octave = 0, .vel = 54, .gate = kGateHeld},
};
// Ending2-only fork of kPad7 (Part A): ending1 restates the held chord at
// step 16, ending2 stays silent there and only sounds under the big final
// hit at step 24 -- the two endings diverge, so this duplicates kPad7's
// original bar-1 content verbatim and adds its own distinct bar-2 tail.
inline constexpr StyleEvent kPad7Ending2[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 60, .gate = kGateHeld},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 58, .gate = kGateHeld},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 60, .gate = kGateHeld},
    {.step = 0, .tone = kSeventh, .octave = 0, .vel = 54, .gate = kGateHeld},
    {.step = 24, .tone = kRoot, .octave = 0, .vel = 60, .gate = kGateHeld},
    {.step = 24, .tone = kThird, .octave = 0, .vel = 58, .gate = kGateHeld},
    {.step = 24, .tone = kFifth, .octave = 0, .vel = 60, .gate = kGateHeld},
    {.step = 24, .tone = kSeventh, .octave = 0, .vel = 54, .gate = kGateHeld},
};
inline constexpr StyleEvent kPadRehit[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 60, .gate = kGateHalfBar},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 58, .gate = kGateHalfBar},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 60, .gate = kGateHalfBar},
    {.step = 0, .tone = kSeventh, .octave = 0, .vel = 54, .gate = kGateHalfBar},
    {.step = 8, .tone = kRoot, .octave = 0, .vel = 58, .gate = kGateHalfBar},
    {.step = 8, .tone = kThird, .octave = 0, .vel = 56, .gate = kGateHalfBar},
    {.step = 8, .tone = kFifth, .octave = 0, .vel = 58, .gate = kGateHalfBar},
    {.step = 8, .tone = kSeventh, .octave = 0, .vel = 52, .gate = kGateHalfBar},
};
// Chord2 on the down-beats (kChord1 stabs the off-beats) — the two interlock.
inline constexpr StyleEvent kChord2On[] = {
    {.step = 0, .tone = kThird, .octave = 0, .vel = 60, .gate = kGateBeat},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 60, .gate = kGateBeat},
    {.step = 8, .tone = kThird, .octave = 0, .vel = 58, .gate = kGateBeat},
    {.step = 8, .tone = kFifth, .octave = 0, .vel = 58, .gate = kGateBeat},
};
// Chord2 filling the off-beats kChord1 leaves open in the dance variations.
inline constexpr StyleEvent kChord2Fill[] = {
    {.step = 6, .tone = kRoot, .octave = 0, .vel = 60, .gate = kGateStab},
    {.step = 6, .tone = kThird, .octave = 0, .vel = 60, .gate = kGateStab},
    {.step = 6, .tone = kFifth, .octave = 0, .vel = 60, .gate = kGateStab},
    {.step = 14, .tone = kRoot, .octave = 0, .vel = 58, .gate = kGateStab},
    {.step = 14, .tone = kThird, .octave = 0, .vel = 58, .gate = kGateStab},
    {.step = 14, .tone = kFifth, .octave = 0, .vel = 58, .gate = kGateStab},
};
inline constexpr StyleEvent kArp8[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 68, .gate = kGateHat},
    {.step = 2, .tone = kThird, .octave = 0, .vel = 62, .gate = kGateHat},
    {.step = 4, .tone = kFifth, .octave = 0, .vel = 66, .gate = kGateHat},
    {.step = 6, .tone = kSeventh, .octave = 0, .vel = 62, .gate = kGateHat},
    {.step = 8, .tone = kRoot, .octave = 1, .vel = 68, .gate = kGateHat},
    {.step = 10, .tone = kSeventh, .octave = 0, .vel = 62, .gate = kGateHat},
    {.step = 12, .tone = kFifth, .octave = 0, .vel = 66, .gate = kGateHat},
    {.step = 14, .tone = kThird, .octave = 0, .vel = 62, .gate = kGateHat},
};
inline constexpr StyleEvent kPercTamb[] = {
    {.step = 4, .tone = kTambourine, .octave = 0, .vel = 82, .gate = kGateHat},
    {.step = 12, .tone = kTambourine, .octave = 0, .vel = 84, .gate = kGateHat},
};
inline constexpr StyleEvent kPercShake[] = {
    {.step = 0, .tone = kCabasa, .octave = 0, .vel = 66, .gate = kGateHat},
    {.step = 2, .tone = kCabasa, .octave = 0, .vel = 56, .gate = kGateHat},
    {.step = 4, .tone = kTambourine, .octave = 0, .vel = 82, .gate = kGateHat},
    {.step = 6, .tone = kCabasa, .octave = 0, .vel = 56, .gate = kGateHat},
    {.step = 8, .tone = kCabasa, .octave = 0, .vel = 66, .gate = kGateHat},
    {.step = 10, .tone = kCabasa, .octave = 0, .vel = 56, .gate = kGateHat},
    {.step = 12, .tone = kTambourine, .octave = 0, .vel = 84, .gate = kGateHat},
    {.step = 14, .tone = kCabasa, .octave = 0, .vel = 56, .gate = kGateHat},
};
// A tasteful synth hook for the peak (varD): a major-pentatonic answer in the
// back half of the bar. kScaleDegree keeps the line locked to the live KEY over
// any chord change; kLead role -> anchor 72, singing above the comp.
inline constexpr std::int16_t kLeadVoice = 81;  // Lead 2 (sawtooth) — pop synth hook
inline constexpr StyleEvent kLeadHook[] = {
    {.step = 8,
     .tone = 4,
     .octave = 0,
     .vel = 92,
     .gate = kGate8th,
     .src = NoteSource::kScaleDegree},  // 5th
    {.step = 10,
     .tone = 5,
     .octave = 0,
     .vel = 86,
     .gate = kGate8th,
     .src = NoteSource::kScaleDegree},  // 6th
    {.step = 12,
     .tone = 7,
     .octave = 0,
     .vel = 96,
     .gate = kGate8th,
     .src = NoteSource::kScaleDegree},  // up to the octave
    {.step = 14,
     .tone = 4,
     .octave = 0,
     .vel = 88,
     .gate = kGateBeat,
     .src = NoteSource::kScaleDegree},  // settle on the 5th
};
// Motif engine (9210, Ottorino RANK 11): the pop synth hook, wired only into
// varD — pop is the most genre-generic style in the corpus, so this is the
// smallest per-effort payoff among the safe Option-1 styles, but real.
inline constexpr MotifSpec kLeadHookMotif{.transform = MotifTransform::kDiatonicTranspose,
                                          .seed = 1101};
// Motif engine (9210 Wave 1, style-depth program task A): extend variation
// onto drums/chord1/chord2/pad, wired onto the peak variation (varD) plus the
// shared reused Pad (kPadRehit, varC+varD) and Chord2 (kChord2On, varA/B/D)
// arrays. kDisplacement for drums/pad/chord2 (a soft evolving push);
// kRetrograde for the chord1 off-beat triad stabs (a legible mirrored
// answer). pop is the most genre-generic style, so this is real but modest
// payoff, matching the existing lead-hook wiring's own rationale.
inline constexpr MotifSpec kPeakDrumsMotif{.transform = MotifTransform::kDisplacement,
                                           .seed = 1110};
inline constexpr MotifSpec kPeakChordMotif{.transform = MotifTransform::kRetrograde, .seed = 1111};
inline constexpr MotifSpec kSharedPadMotif{.transform = MotifTransform::kDisplacement,
                                           .seed = 1112};
inline constexpr MotifSpec kSharedChord2Motif{.transform = MotifTransform::kDisplacement,
                                              .seed = 1113};

inline constexpr StyleEvent kVarADrums[] = {
    // bar 1 (unchanged)
    {.step = 0, .tone = kKick, .octave = 0, .vel = 102, .gate = kGateHat},
    {.step = 8, .tone = kKick, .octave = 0, .vel = 98, .gate = kGateHat},
    {.step = 4, .tone = kSnare, .octave = 0, .vel = 100, .gate = kGateHat},
    {.step = 12, .tone = kSnare, .octave = 0, .vel = 100, .gate = kGateHat},
    {.step = 0, .tone = kClosedHat, .octave = 0, .vel = 78, .gate = kGateHat},
    {.step = 2, .tone = kClosedHat, .octave = 0, .vel = 64, .gate = kGateHat},
    {.step = 4, .tone = kClosedHat, .octave = 0, .vel = 78, .gate = kGateHat},
    {.step = 6, .tone = kClosedHat, .octave = 0, .vel = 64, .gate = kGateHat},
    {.step = 8, .tone = kClosedHat, .octave = 0, .vel = 78, .gate = kGateHat},
    {.step = 10, .tone = kClosedHat, .octave = 0, .vel = 64, .gate = kGateHat},
    {.step = 12, .tone = kClosedHat, .octave = 0, .vel = 78, .gate = kGateHat},
    {.step = 14, .tone = kClosedHat, .octave = 0, .vel = 64, .gate = kGateHat},
    // bar 2 (style-depth Wave-2 C): a small turnaround -- the kick pushes
    // ahead of beat 3, and the hat opens on the last 8th to lift into the repeat.
    {.step = 16, .tone = kKick, .octave = 0, .vel = 102, .gate = kGateHat},
    {.step = 22, .tone = kKick, .octave = 0, .vel = 90, .gate = kGateHat},
    {.step = 24, .tone = kKick, .octave = 0, .vel = 98, .gate = kGateHat},
    {.step = 20, .tone = kSnare, .octave = 0, .vel = 100, .gate = kGateHat},
    {.step = 28, .tone = kSnare, .octave = 0, .vel = 100, .gate = kGateHat},
    {.step = 16, .tone = kClosedHat, .octave = 0, .vel = 78, .gate = kGateHat},
    {.step = 18, .tone = kClosedHat, .octave = 0, .vel = 64, .gate = kGateHat},
    {.step = 20, .tone = kClosedHat, .octave = 0, .vel = 78, .gate = kGateHat},
    {.step = 22, .tone = kClosedHat, .octave = 0, .vel = 64, .gate = kGateHat},
    {.step = 24, .tone = kClosedHat, .octave = 0, .vel = 78, .gate = kGateHat},
    {.step = 26, .tone = kClosedHat, .octave = 0, .vel = 64, .gate = kGateHat},
    {.step = 28, .tone = kClosedHat, .octave = 0, .vel = 78, .gate = kGateHat},
    {.step = 30, .tone = kOpenHat, .octave = 0, .vel = 88, .gate = kGate8th},
};
inline constexpr StyleEvent kVarABass[] = {
    // bar 1 (unchanged)
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 98, .gate = kGate8th},
    {.step = 4, .tone = kFifth, .octave = 0, .vel = 88, .gate = kGate8th},
    {.step = 8, .tone = kRoot, .octave = 0, .vel = 94, .gate = kGate8th},
    {.step = 12, .tone = kFifth, .octave = 0, .vel = 88, .gate = kGate8th},
    // bar 2 (style-depth Wave-2 C): a walk-up -- root, fifth, an octave push,
    // then the leading 7th back into the repeat.
    {.step = 16, .tone = kRoot, .octave = 0, .vel = 98, .gate = kGate8th},
    {.step = 20, .tone = kFifth, .octave = 0, .vel = 88, .gate = kGate8th},
    {.step = 24, .tone = kRoot, .octave = 1, .vel = 94, .gate = kGate8th},
    {.step = 28, .tone = kSeventh, .octave = 0, .vel = 90, .gate = kGate8th},
};
inline constexpr StyleEvent kVarAChord[] = {
    // bar 1 (unchanged): off-beat triad stabs (root/3rd/5th) — the classic pop upstroke.
    {.step = 2, .tone = kRoot, .octave = 0, .vel = 88, .gate = kGateStab},
    {.step = 2, .tone = kThird, .octave = 0, .vel = 88, .gate = kGateStab},
    {.step = 2, .tone = kFifth, .octave = 0, .vel = 88, .gate = kGateStab},
    {.step = 6, .tone = kRoot, .octave = 0, .vel = 84, .gate = kGateStab},
    {.step = 6, .tone = kThird, .octave = 0, .vel = 84, .gate = kGateStab},
    {.step = 6, .tone = kFifth, .octave = 0, .vel = 84, .gate = kGateStab},
    {.step = 10, .tone = kRoot, .octave = 0, .vel = 88, .gate = kGateStab},
    {.step = 10, .tone = kThird, .octave = 0, .vel = 88, .gate = kGateStab},
    {.step = 10, .tone = kFifth, .octave = 0, .vel = 88, .gate = kGateStab},
    {.step = 14, .tone = kRoot, .octave = 0, .vel = 84, .gate = kGateStab},
    {.step = 14, .tone = kThird, .octave = 0, .vel = 84, .gate = kGateStab},
    {.step = 14, .tone = kFifth, .octave = 0, .vel = 84, .gate = kGateStab},
    // bar 2 (style-depth Wave-2 C): the comp answers a 16th early with the 7th
    // added to the stack -- a genuine syncopated push, not a repeat of bar 1.
    {.step = 3, .tone = kRoot, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 3, .tone = kThird, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 3, .tone = kFifth, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 3, .tone = kSeventh, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 7, .tone = kRoot, .octave = 0, .vel = 86, .gate = kGateStab},
    {.step = 7, .tone = kThird, .octave = 0, .vel = 86, .gate = kGateStab},
    {.step = 7, .tone = kFifth, .octave = 0, .vel = 86, .gate = kGateStab},
    {.step = 11, .tone = kRoot, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 11, .tone = kThird, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 11, .tone = kFifth, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 11, .tone = kSeventh, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 15, .tone = kRoot, .octave = 0, .vel = 86, .gate = kGateStab},
    {.step = 15, .tone = kThird, .octave = 0, .vel = 86, .gate = kGateStab},
    {.step = 15, .tone = kFifth, .octave = 0, .vel = 86, .gate = kGateStab},
};
inline constexpr StylePattern kVarAPatterns[] = {
    {.role = TrackRole::kDrums,
     .policy = RolePolicy::kFixed,
     .events = Span<const StyleEvent>(kVarADrums)},
    {.role = TrackRole::kBass,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kVarABass)},
    {.role = TrackRole::kChord1,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kVarAChord)},
    {.role = TrackRole::kPad,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kPadTriad),
     .gm_program = kPadVoice,
     .voicing = VoicingPolicy::kLead},
    // Chord2 stays motif-driven and UNCHANGED here (style-depth Wave-2 C,
    // deliberate): kChord2On is also motif-referenced from VarB/VarD, so it is
    // off-limits to step>=16 additions (motif engine hazard guardrail —
    // from_span/retrograde/displacement assume a 16-slot bar). A side effect:
    // motif-generated events never carry step>=16 (motif.hpp), so Chord2 will
    // sound in bar 1 of this now-2-bar VarA and go quiet in bar 2 — flagged,
    // not fixed; an engine change, not a data change, would be needed to close it.
    {.role = TrackRole::kChord2,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kChord2On),
     .gm_program = kChord2Voice,
     .voicing = VoicingPolicy::kLead,
     .motif = &kSharedChord2Motif},
};

// VarB stays 1 bar (style-depth Wave-2 C): its clap-doubled, busier hat
// pattern already reads as a distinct energy step up from VarA; a 2nd bar
// here would just restate that same contrast rather than add a new idea.
inline constexpr StyleEvent kVarBDrums[] = {
    {.step = 0, .tone = kKick, .octave = 0, .vel = 104, .gate = kGateHat},
    {.step = 6, .tone = kKick, .octave = 0, .vel = 92, .gate = kGateHat},
    {.step = 8, .tone = kKick, .octave = 0, .vel = 100, .gate = kGateHat},
    {.step = 10, .tone = kKick, .octave = 0, .vel = 90, .gate = kGateHat},
    {.step = 4, .tone = kSnare, .octave = 0, .vel = 102, .gate = kGateHat},
    {.step = 12, .tone = kSnare, .octave = 0, .vel = 102, .gate = kGateHat},
    {.step = 4, .tone = kClap, .octave = 0, .vel = 90, .gate = kGateHat},
    {.step = 12, .tone = kClap, .octave = 0, .vel = 90, .gate = kGateHat},
    {.step = 0, .tone = kClosedHat, .octave = 0, .vel = 80, .gate = kGateHat},
    {.step = 2, .tone = kClosedHat, .octave = 0, .vel = 66, .gate = kGateHat},
    {.step = 4, .tone = kClosedHat, .octave = 0, .vel = 80, .gate = kGateHat},
    {.step = 6, .tone = kClosedHat, .octave = 0, .vel = 66, .gate = kGateHat},
    {.step = 8, .tone = kClosedHat, .octave = 0, .vel = 80, .gate = kGateHat},
    {.step = 10, .tone = kClosedHat, .octave = 0, .vel = 66, .gate = kGateHat},
    {.step = 12, .tone = kClosedHat, .octave = 0, .vel = 80, .gate = kGateHat},
    {.step = 14, .tone = kOpenHat, .octave = 0, .vel = 78, .gate = kGate8th},
};
inline constexpr StyleEvent kVarBBass[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 100, .gate = kGate8th},
    {.step = 6, .tone = kRoot, .octave = 0, .vel = 78, .gate = kGateHat},
    {.step = 8, .tone = kFifth, .octave = 0, .vel = 92, .gate = kGate8th},
    {.step = 10, .tone = kRoot, .octave = 1, .vel = 80, .gate = kGateHat},
    {.step = 12, .tone = kFifth, .octave = 0, .vel = 88, .gate = kGate8th},
};
inline constexpr StyleEvent kVarBChord[] = {
    // Fuller four-note stabs on every off-beat.
    {.step = 2, .tone = kRoot, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 2, .tone = kThird, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 2, .tone = kFifth, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 2, .tone = kSeventh, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 6, .tone = kRoot, .octave = 0, .vel = 84, .gate = kGateStab},
    {.step = 6, .tone = kThird, .octave = 0, .vel = 84, .gate = kGateStab},
    {.step = 6, .tone = kFifth, .octave = 0, .vel = 84, .gate = kGateStab},
    {.step = 10, .tone = kRoot, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 10, .tone = kThird, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 10, .tone = kFifth, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 10, .tone = kSeventh, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 14, .tone = kRoot, .octave = 0, .vel = 84, .gate = kGateStab},
    {.step = 14, .tone = kThird, .octave = 0, .vel = 84, .gate = kGateStab},
    {.step = 14, .tone = kFifth, .octave = 0, .vel = 84, .gate = kGateStab},
};
inline constexpr StylePattern kVarBPatterns[] = {
    {.role = TrackRole::kDrums,
     .policy = RolePolicy::kFixed,
     .events = Span<const StyleEvent>(kVarBDrums)},
    {.role = TrackRole::kBass,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kVarBBass)},
    {.role = TrackRole::kChord1,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kVarBChord)},
    {.role = TrackRole::kPad,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kPadTriad),
     .gm_program = kPadVoice,
     .voicing = VoicingPolicy::kLead},
    {.role = TrackRole::kChord2,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kChord2On),
     .gm_program = kChord2Voice,
     .voicing = VoicingPolicy::kLead,
     .motif = &kSharedChord2Motif},
    {.role = TrackRole::kPerc,
     .policy = RolePolicy::kFixed,
     .events = Span<const StyleEvent>(kPercTamb)},
};

inline constexpr StyleEvent kIntroDrums[] = {
    // bar 1 (unchanged): hat pickup crescendo into the downbeat.
    {.step = 8, .tone = kClosedHat, .octave = 0, .vel = 62, .gate = kGateHat},
    {.step = 10, .tone = kClosedHat, .octave = 0, .vel = 70, .gate = kGateHat},
    {.step = 12, .tone = kClosedHat, .octave = 0, .vel = 78, .gate = kGateHat},
    {.step = 14, .tone = kClosedHat, .octave = 0, .vel = 88, .gate = kGateHat},
    // bar 2 (style-depth Wave-2 C): the arrival -- crash announces the
    // downbeat, backbeat kick/snare enter under a full 8th-note hat, landing
    // the band right where VarA starts.
    {.step = 16, .tone = kCrash, .octave = 0, .vel = 92, .gate = kGateHalfBar},
    {.step = 16, .tone = kKick, .octave = 0, .vel = 100, .gate = kGateHat},
    {.step = 24, .tone = kKick, .octave = 0, .vel = 96, .gate = kGateHat},
    {.step = 20, .tone = kSnare, .octave = 0, .vel = 96, .gate = kGateHat},
    {.step = 28, .tone = kSnare, .octave = 0, .vel = 98, .gate = kGateHat},
    {.step = 16, .tone = kClosedHat, .octave = 0, .vel = 76, .gate = kGateHat},
    {.step = 18, .tone = kClosedHat, .octave = 0, .vel = 64, .gate = kGateHat},
    {.step = 20, .tone = kClosedHat, .octave = 0, .vel = 76, .gate = kGateHat},
    {.step = 22, .tone = kClosedHat, .octave = 0, .vel = 64, .gate = kGateHat},
    {.step = 24, .tone = kClosedHat, .octave = 0, .vel = 76, .gate = kGateHat},
    {.step = 26, .tone = kClosedHat, .octave = 0, .vel = 64, .gate = kGateHat},
    {.step = 28, .tone = kClosedHat, .octave = 0, .vel = 76, .gate = kGateHat},
    {.step = 30, .tone = kOpenHat, .octave = 0, .vel = 86, .gate = kGate8th},
};
// Held root pickup. Shared with Intro2 (kIntro2Patterns below), which stays 1
// bar and never reads step>=16, so the Intro1-only bar-2 tail is safe here.
inline constexpr StyleEvent kIntroBass[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 90, .gate = kGateHeld},
    // bar 2 (style-depth Wave-2 C, Intro1 only): hold the root again, then a
    // short 5th anticipation leading into VarA.
    {.step = 16, .tone = kRoot, .octave = 0, .vel = 94, .gate = 3200},
    {.step = 30, .tone = kFifth, .octave = 0, .vel = 82, .gate = kGate8th},
};
inline constexpr StylePattern kIntroPatterns[] = {
    {.role = TrackRole::kDrums,
     .policy = RolePolicy::kFixed,
     .events = Span<const StyleEvent>(kIntroDrums)},
    {.role = TrackRole::kBass,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kIntroBass)},
};

inline constexpr StyleEvent kFillDrums[] = {
    {.step = 0, .tone = kKick, .octave = 0, .vel = 100, .gate = kGateHat},
    {.step = 4, .tone = kSnare, .octave = 0, .vel = 96, .gate = kGateHat},
    {.step = 8, .tone = kSnare, .octave = 0, .vel = 88, .gate = 100},
    {.step = 10, .tone = kSnare, .octave = 0, .vel = 94, .gate = 100},
    {.step = 12, .tone = kSnare, .octave = 0, .vel = 102, .gate = 100},
    {.step = 14, .tone = kSnare, .octave = 0, .vel = 110, .gate = 100},
};
inline constexpr StyleEvent kFillBass[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 98, .gate = kGate8th},
    {.step = 8, .tone = kFifth, .octave = 0, .vel = 88, .gate = kGate8th},
};
// The flat FILL bass, reused identically across all 4 fills; the Var bass
// (kVarABass..kVarDBass) is already 4 distinct hand-authored ideas.
inline constexpr MotifSpec kFillBassMotif{.transform = MotifTransform::kRetrograde, .seed = 1102};
inline constexpr StylePattern kFillPatterns[] = {
    {.role = TrackRole::kDrums,
     .policy = RolePolicy::kFixed,
     .events = Span<const StyleEvent>(kFillDrums)},
    {.role = TrackRole::kBass,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kFillBass),
     .motif = &kFillBassMotif},
};

inline constexpr StyleEvent kEndDrums[] = {
    {.step = 0, .tone = kKick, .octave = 0, .vel = 110, .gate = kGateHat},
    {.step = 0, .tone = kCrash, .octave = 0, .vel = 104, .gate = kGateHalfBar},
    // bar 2 (Part A, 2-bar endings): the arrival hit -- kick under a held crash.
    {.step = 16, .tone = kKick, .octave = 0, .vel = 110, .gate = kGateHat},
    {.step = 16, .tone = kCrash, .octave = 0, .vel = 104, .gate = kGateHeld},
};
inline constexpr StyleEvent kEndBass[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 98, .gate = kGateHeld},
    {.step = 16, .tone = kRoot, .octave = 0, .vel = 98, .gate = kGateHeld},
};
inline constexpr StyleEvent kEndChord[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 88, .gate = kGateHeld},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 88, .gate = kGateHeld},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 88, .gate = kGateHeld},
    {.step = 0, .tone = kSeventh, .octave = 0, .vel = 88, .gate = kGateHeld},
    // bar 2 (Part A, pop delta): a sus4 pickup, then the resolved 7th chord
    // -- pop keeps its own full triad+7th voicing, softer than the arrival hit.
    {.step = 14, .tone = 5, .octave = 0, .vel = 84, .gate = kGate8th, .src = NoteSource::kInterval},
    {.step = 16, .tone = kThird, .octave = 0, .vel = 90, .gate = kGateHeld},
    {.step = 16, .tone = kRoot, .octave = 0, .vel = 90, .gate = kGateHeld},
    {.step = 16, .tone = kFifth, .octave = 0, .vel = 90, .gate = kGateHeld},
    {.step = 16, .tone = kSeventh, .octave = 0, .vel = 88, .gate = kGateHeld},
};
inline constexpr StyleEvent kEndPerc[] = {
    // The Part-A ending arrival shaker tap (pop delta): a single bright
    // cabasa hit marking the bar-2 downbeat, echoing kPercShake's own texture
    // without reusing its bar-1 content (which never reaches step 16).
    {.step = 16, .tone = kCabasa, .octave = 0, .vel = 70, .gate = kGateHat},
};
inline constexpr StylePattern kEndPatterns[] = {
    {.role = TrackRole::kDrums,
     .policy = RolePolicy::kFixed,
     .events = Span<const StyleEvent>(kEndDrums)},
    {.role = TrackRole::kBass,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kEndBass)},
    {.role = TrackRole::kChord1,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kEndChord)},
    {.role = TrackRole::kPad,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kPad7),
     .gm_program = kPadVoice,
     .voicing = VoicingPolicy::kLead},
    {.role = TrackRole::kPerc,
     .policy = RolePolicy::kFixed,
     .events = Span<const StyleEvent>(kEndPerc)},
};

inline constexpr StyleEvent kIntro2Drums[] = {
    {.step = 0, .tone = kKick, .octave = 0, .vel = 100, .gate = kGateHat},
    {.step = 8, .tone = kKick, .octave = 0, .vel = 96, .gate = kGateHat},
    {.step = 4, .tone = kClap, .octave = 0, .vel = 92, .gate = kGateHat},
    {.step = 12, .tone = kClap, .octave = 0, .vel = 92, .gate = kGateHat},
    {.step = 0, .tone = kClosedHat, .octave = 0, .vel = 74, .gate = kGateHat},
    {.step = 2, .tone = kClosedHat, .octave = 0, .vel = 60, .gate = kGateHat},
    {.step = 4, .tone = kClosedHat, .octave = 0, .vel = 74, .gate = kGateHat},
    {.step = 6, .tone = kClosedHat, .octave = 0, .vel = 60, .gate = kGateHat},
    {.step = 8, .tone = kClosedHat, .octave = 0, .vel = 74, .gate = kGateHat},
    {.step = 10, .tone = kClosedHat, .octave = 0, .vel = 60, .gate = kGateHat},
    {.step = 12, .tone = kClosedHat, .octave = 0, .vel = 74, .gate = kGateHat},
    {.step = 14, .tone = kOpenHat, .octave = 0, .vel = 76, .gate = kGate8th},
};
inline constexpr StyleEvent kIntro2Chord[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 78, .gate = kGateBeat},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 78, .gate = kGateBeat},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 78, .gate = kGateBeat},
    {.step = 8, .tone = kRoot, .octave = 0, .vel = 76, .gate = kGateBeat},
    {.step = 8, .tone = kThird, .octave = 0, .vel = 76, .gate = kGateBeat},
    {.step = 8, .tone = kFifth, .octave = 0, .vel = 76, .gate = kGateBeat},
};
inline constexpr StylePattern kIntro2Patterns[] = {
    {.role = TrackRole::kDrums,
     .policy = RolePolicy::kFixed,
     .events = Span<const StyleEvent>(kIntro2Drums)},
    {.role = TrackRole::kBass,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kIntroBass)},
    {.role = TrackRole::kChord1,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kIntro2Chord)},
    {.role = TrackRole::kPad,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kPadTriad),
     .gm_program = kPadVoice,
     .voicing = VoicingPolicy::kLead},
};

inline constexpr StyleEvent kFillBDrums[] = {
    {.step = 0, .tone = kKick, .octave = 0, .vel = 100, .gate = kGateHat},
    {.step = 4, .tone = kSnare, .octave = 0, .vel = 98, .gate = 120},
    {.step = 6, .tone = kClap, .octave = 0, .vel = 90, .gate = 120},
    {.step = 8, .tone = kSnare, .octave = 0, .vel = 92, .gate = 120},
    {.step = 10, .tone = kTomMid, .octave = 0, .vel = 98, .gate = 120},
    {.step = 12, .tone = kSnare, .octave = 0, .vel = 104, .gate = 120},
    {.step = 14, .tone = kTomLow, .octave = 0, .vel = 110, .gate = 120},
};
inline constexpr StyleEvent kFillCDrums[] = {
    {.step = 0, .tone = kSnare, .octave = 0, .vel = 94, .gate = 100},
    {.step = 2, .tone = kSnare, .octave = 0, .vel = 88, .gate = 100},
    {.step = 4, .tone = kClap, .octave = 0, .vel = 98, .gate = 100},
    {.step = 6, .tone = kTomHi, .octave = 0, .vel = 96, .gate = 100},
    {.step = 8, .tone = kTomMid, .octave = 0, .vel = 102, .gate = 100},
    {.step = 10, .tone = kTomMid, .octave = 0, .vel = 100, .gate = 100},
    {.step = 12, .tone = kTomLow, .octave = 0, .vel = 108, .gate = 100},
    {.step = 14, .tone = kTomFloor, .octave = 0, .vel = 114, .gate = 100},
};
inline constexpr StyleEvent kFillDDrums[] = {
    {.step = 0, .tone = kSnare, .octave = 0, .vel = 92, .gate = 50},
    {.step = 1, .tone = kSnare, .octave = 0, .vel = 86, .gate = 50},
    {.step = 2, .tone = kSnare, .octave = 0, .vel = 98, .gate = 50},
    {.step = 3, .tone = kSnare, .octave = 0, .vel = 90, .gate = 50},
    {.step = 4, .tone = kClap, .octave = 0, .vel = 102, .gate = 50},
    {.step = 5, .tone = kSnare, .octave = 0, .vel = 94, .gate = 50},
    {.step = 6, .tone = kTomHi, .octave = 0, .vel = 104, .gate = 50},
    {.step = 7, .tone = kTomHi, .octave = 0, .vel = 98, .gate = 50},
    {.step = 8, .tone = kTomMid, .octave = 0, .vel = 106, .gate = 50},
    {.step = 9, .tone = kTomMid, .octave = 0, .vel = 100, .gate = 50},
    {.step = 10, .tone = kTomLow, .octave = 0, .vel = 110, .gate = 50},
    {.step = 11, .tone = kTomLow, .octave = 0, .vel = 104, .gate = 50},
    {.step = 12, .tone = kTomFloor, .octave = 0, .vel = 114, .gate = 50},
    {.step = 13, .tone = kTomFloor, .octave = 0, .vel = 108, .gate = 50},
    {.step = 14, .tone = kSnare, .octave = 0, .vel = 118, .gate = 50},
    {.step = 15, .tone = kCrash, .octave = 0, .vel = 120, .gate = kGate8th},
};
inline constexpr StylePattern kFillBPatterns[] = {
    {.role = TrackRole::kDrums,
     .policy = RolePolicy::kFixed,
     .events = Span<const StyleEvent>(kFillBDrums)},
    {.role = TrackRole::kBass,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kFillBass),
     .motif = &kFillBassMotif},
};
inline constexpr StylePattern kFillCPatterns[] = {
    {.role = TrackRole::kDrums,
     .policy = RolePolicy::kFixed,
     .events = Span<const StyleEvent>(kFillCDrums)},
    {.role = TrackRole::kBass,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kFillBass),
     .motif = &kFillBassMotif},
};
inline constexpr StylePattern kFillDPatterns[] = {
    {.role = TrackRole::kDrums,
     .policy = RolePolicy::kFixed,
     .events = Span<const StyleEvent>(kFillDDrums)},
    {.role = TrackRole::kBass,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kFillBass),
     .motif = &kFillBassMotif},
    {.role = TrackRole::kPerc,
     .policy = RolePolicy::kFixed,
     .events = Span<const StyleEvent>(kPercShake)},
};

inline constexpr StyleEvent kEnd2Drums[] = {
    {.step = 0, .tone = kCrash, .octave = 0, .vel = 108, .gate = kGateHeld},
    {.step = 0, .tone = kKick, .octave = 0, .vel = 112, .gate = kGateHat},
    {.step = 8, .tone = kClap, .octave = 0, .vel = 98, .gate = kGateHat},
    {.step = 8, .tone = kKick, .octave = 0, .vel = 104, .gate = kGateHat},
    // bar 2 (Part A, "Amen tag"): a short punctuation kick on 1, the big
    // final hit (kick+held crash) on beat 3.
    {.step = 16, .tone = kKick, .octave = 0, .vel = 100, .gate = kGateHat},
    {.step = 24, .tone = kKick, .octave = 0, .vel = 110, .gate = kGateHat},
    {.step = 24, .tone = kCrash, .octave = 0, .vel = 108, .gate = kGateHeld},
};
inline constexpr StyleEvent kEnd2Bass[] = {
    {.step = 0, .tone = kRoot, .octave = -1, .vel = 100, .gate = kGateHeld},
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 92, .gate = kGateHeld},
    {.step = 16, .tone = kRoot, .octave = 0, .vel = 92, .gate = kGateBeat},
    {.step = 24, .tone = kRoot, .octave = -1, .vel = 100, .gate = kGateHeld},
    {.step = 24, .tone = kRoot, .octave = 0, .vel = 92, .gate = kGateHeld},
};
inline constexpr StyleEvent kEnd2Chord[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 88, .gate = kGateHeld},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 88, .gate = kGateHeld},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 88, .gate = kGateHeld},
    {.step = 0, .tone = kSeventh, .octave = 0, .vel = 88, .gate = kGateHeld},
    // bar 2 (Part A, "Amen tag"): a short 6th/4th color tag on 1, then the
    // big final chord on beat 3 of bar 2 -- NOT the downbeat -- root doubled
    // an octave up.
    {.step = 16,
     .tone = 9,
     .octave = 0,
     .vel = 80,
     .gate = kGateBeat,
     .src = NoteSource::kInterval},
    {.step = 16,
     .tone = 5,
     .octave = 0,
     .vel = 78,
     .gate = kGateBeat,
     .src = NoteSource::kInterval},
    {.step = 24, .tone = kRoot, .octave = 0, .vel = 100, .gate = kGateHeld},
    {.step = 24, .tone = kThird, .octave = 0, .vel = 100, .gate = kGateHeld},
    {.step = 24, .tone = kFifth, .octave = 0, .vel = 100, .gate = kGateHeld},
    {.step = 24, .tone = kRoot, .octave = 1, .vel = 100, .gate = kGateHeld},
};
inline constexpr StyleEvent kEnd2Perc[] = {
    {.step = 16, .tone = kCabasa, .octave = 0, .vel = 64, .gate = kGateHat},
};
inline constexpr StylePattern kEnd2Patterns[] = {
    {.role = TrackRole::kDrums,
     .policy = RolePolicy::kFixed,
     .events = Span<const StyleEvent>(kEnd2Drums)},
    {.role = TrackRole::kBass,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kEnd2Bass)},
    {.role = TrackRole::kChord1,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kEnd2Chord)},
    {.role = TrackRole::kPad,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kPad7Ending2),
     .gm_program = kPadVoice,
     .voicing = VoicingPolicy::kLead},
    {.role = TrackRole::kPerc,
     .policy = RolePolicy::kFixed,
     .events = Span<const StyleEvent>(kEnd2Perc)},
};

// varC stays 1 bar (style-depth Wave-2 C): the four-on-the-floor dance-pump
// pulse IS the idiom -- an identical, hypnotic repeat is the point, so a
// distinct 2nd bar would work against the groove rather than deepen it.
// varC: four-on-the-floor dance-pop pump — a different lift from the backbeat A/B.
inline constexpr StyleEvent kVarCDrums[] = {
    {.step = 0, .tone = kKick, .octave = 0, .vel = 104, .gate = kGateHat},
    {.step = 4, .tone = kKick, .octave = 0, .vel = 98, .gate = kGateHat},
    {.step = 8, .tone = kKick, .octave = 0, .vel = 102, .gate = kGateHat},
    {.step = 12, .tone = kKick, .octave = 0, .vel = 98, .gate = kGateHat},
    {.step = 4, .tone = kClap, .octave = 0, .vel = 90, .gate = kGateHat},
    {.step = 12, .tone = kClap, .octave = 0, .vel = 90, .gate = kGateHat},
    {.step = 0, .tone = kClosedHat, .octave = 0, .vel = 74, .gate = kGateHat},
    {.step = 2, .tone = kClosedHat, .octave = 0, .vel = 62, .gate = kGateHat},
    {.step = 4, .tone = kClosedHat, .octave = 0, .vel = 74, .gate = kGateHat},
    {.step = 6, .tone = kClosedHat, .octave = 0, .vel = 62, .gate = kGateHat},
    {.step = 8, .tone = kClosedHat, .octave = 0, .vel = 74, .gate = kGateHat},
    {.step = 10, .tone = kClosedHat, .octave = 0, .vel = 62, .gate = kGateHat},
    {.step = 12, .tone = kClosedHat, .octave = 0, .vel = 74, .gate = kGateHat},
    {.step = 14, .tone = kClosedHat, .octave = 0, .vel = 62, .gate = kGateHat}};
inline constexpr StyleEvent kVarCBass[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 96, .gate = kGate8th},
    {.step = 4, .tone = kFifth, .octave = 0, .vel = 86, .gate = kGate8th},
    {.step = 8, .tone = kRoot, .octave = 0, .vel = 94, .gate = kGate8th},
    {.step = 12, .tone = kFifth, .octave = 0, .vel = 86, .gate = kGate8th}};
inline constexpr StyleEvent kVarCChord[] = {
    {.step = 2, .tone = kRoot, .octave = 0, .vel = 84, .gate = kGateStab},
    {.step = 2, .tone = kThird, .octave = 0, .vel = 84, .gate = kGateStab},
    {.step = 2, .tone = kFifth, .octave = 0, .vel = 84, .gate = kGateStab},
    {.step = 10, .tone = kRoot, .octave = 0, .vel = 82, .gate = kGateStab},
    {.step = 10, .tone = kThird, .octave = 0, .vel = 82, .gate = kGateStab},
    {.step = 10, .tone = kFifth, .octave = 0, .vel = 82, .gate = kGateStab}};
inline constexpr StylePattern kVarCPatterns[] = {{.role = TrackRole::kDrums,
                                                  .policy = RolePolicy::kFixed,
                                                  .events = Span<const StyleEvent>(kVarCDrums)},
                                                 {.role = TrackRole::kBass,
                                                  .policy = RolePolicy::kChordTone,
                                                  .events = Span<const StyleEvent>(kVarCBass)},
                                                 {.role = TrackRole::kChord1,
                                                  .policy = RolePolicy::kChordTone,
                                                  .events = Span<const StyleEvent>(kVarCChord)},
                                                 {.role = TrackRole::kPad,
                                                  .policy = RolePolicy::kChordTone,
                                                  .events = Span<const StyleEvent>(kPadRehit),
                                                  .gm_program = kPadVoice,
                                                  .voicing = VoicingPolicy::kLead,
                                                  .motif = &kSharedPadMotif},
                                                 {.role = TrackRole::kChord2,
                                                  .policy = RolePolicy::kChordTone,
                                                  .events = Span<const StyleEvent>(kChord2Fill),
                                                  .gm_program = kChord2Voice,
                                                  .voicing = VoicingPolicy::kLead},
                                                 {.role = TrackRole::kArp,
                                                  .policy = RolePolicy::kChordTone,
                                                  .events = Span<const StyleEvent>(kArp8),
                                                  .gm_program = kArpVoice},
                                                 {.role = TrackRole::kPerc,
                                                  .policy = RolePolicy::kFixed,
                                                  .events = Span<const StyleEvent>(kPercShake)}};
// varD stays 1 bar (style-depth Wave-2 C): the peak already carries the most
// per-repeat motion of any section (drums/chord1/pad/chord2/lead all
// motif-wired); a literal 2nd bar would need touching those same
// motif-locked arrays this pass deliberately leaves alone (see the VarA
// Chord2 note above), so the arc stays call-and-response, not a longer loop.
// varD: peak — kick pushes, clap-doubled backbeat, 16th hats, driving bass, full 7th stabs.
inline constexpr StyleEvent kVarDDrums[] = {
    {.step = 0, .tone = kKick, .octave = 0, .vel = 106, .gate = kGateHat},
    {.step = 6, .tone = kKick, .octave = 0, .vel = 92, .gate = kGateHat},
    {.step = 8, .tone = kKick, .octave = 0, .vel = 102, .gate = kGateHat},
    {.step = 10, .tone = kKick, .octave = 0, .vel = 92, .gate = kGateHat},
    {.step = 4, .tone = kSnare, .octave = 0, .vel = 104, .gate = kGateHat},
    {.step = 12, .tone = kSnare, .octave = 0, .vel = 104, .gate = kGateHat},
    {.step = 4, .tone = kClap, .octave = 0, .vel = 94, .gate = kGateHat},
    {.step = 12, .tone = kClap, .octave = 0, .vel = 94, .gate = kGateHat},
    {.step = 0, .tone = kClosedHat, .octave = 0, .vel = 80, .gate = 50},
    {.step = 1, .tone = kClosedHat, .octave = 0, .vel = 60, .gate = 50},
    {.step = 2, .tone = kClosedHat, .octave = 0, .vel = 70, .gate = 50},
    {.step = 3, .tone = kClosedHat, .octave = 0, .vel = 60, .gate = 50},
    {.step = 4, .tone = kClosedHat, .octave = 0, .vel = 80, .gate = 50},
    {.step = 5, .tone = kClosedHat, .octave = 0, .vel = 60, .gate = 50},
    {.step = 6, .tone = kClosedHat, .octave = 0, .vel = 70, .gate = 50},
    {.step = 7, .tone = kClosedHat, .octave = 0, .vel = 60, .gate = 50},
    {.step = 8, .tone = kClosedHat, .octave = 0, .vel = 80, .gate = 50},
    {.step = 9, .tone = kClosedHat, .octave = 0, .vel = 60, .gate = 50},
    {.step = 10, .tone = kClosedHat, .octave = 0, .vel = 70, .gate = 50},
    {.step = 11, .tone = kClosedHat, .octave = 0, .vel = 60, .gate = 50},
    {.step = 12, .tone = kClosedHat, .octave = 0, .vel = 80, .gate = 50},
    {.step = 13, .tone = kClosedHat, .octave = 0, .vel = 60, .gate = 50},
    {.step = 14, .tone = kOpenHat, .octave = 0, .vel = 80, .gate = kGate8th}};
inline constexpr StyleEvent kVarDBass[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 104, .gate = kGate8th},
    {.step = 2, .tone = kRoot, .octave = 1, .vel = 82, .gate = kGateHat},
    {.step = 4, .tone = kFifth, .octave = 0, .vel = 94, .gate = kGate8th},
    {.step = 6, .tone = kRoot, .octave = 0, .vel = 84, .gate = kGateHat},
    {.step = 8, .tone = kRoot, .octave = 0, .vel = 100, .gate = kGate8th},
    {.step = 10, .tone = kRoot, .octave = 1, .vel = 82, .gate = kGateHat},
    {.step = 12, .tone = kFifth, .octave = 0, .vel = 94, .gate = kGate8th},
    {.step = 14, .tone = kSeventh, .octave = 0, .vel = 88, .gate = kGateHat}};
inline constexpr StyleEvent kVarDChord[] = {
    {.step = 2, .tone = kRoot, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 2, .tone = kThird, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 2, .tone = kFifth, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 2, .tone = kSeventh, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 6, .tone = kRoot, .octave = 0, .vel = 86, .gate = kGateStab},
    {.step = 6, .tone = kThird, .octave = 0, .vel = 86, .gate = kGateStab},
    {.step = 6, .tone = kFifth, .octave = 0, .vel = 86, .gate = kGateStab},
    {.step = 6, .tone = kSeventh, .octave = 0, .vel = 86, .gate = kGateStab},
    {.step = 10, .tone = kRoot, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 10, .tone = kThird, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 10, .tone = kFifth, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 10, .tone = kSeventh, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 14, .tone = kRoot, .octave = 0, .vel = 86, .gate = kGateStab},
    {.step = 14, .tone = kThird, .octave = 0, .vel = 86, .gate = kGateStab},
    {.step = 14, .tone = kFifth, .octave = 0, .vel = 86, .gate = kGateStab},
    {.step = 14, .tone = kSeventh, .octave = 0, .vel = 86, .gate = kGateStab}};
inline constexpr StylePattern kVarDPatterns[] = {{.role = TrackRole::kDrums,
                                                  .policy = RolePolicy::kFixed,
                                                  .events = Span<const StyleEvent>(kVarDDrums),
                                                  .motif = &kPeakDrumsMotif},
                                                 {.role = TrackRole::kBass,
                                                  .policy = RolePolicy::kChordTone,
                                                  .events = Span<const StyleEvent>(kVarDBass)},
                                                 {.role = TrackRole::kChord1,
                                                  .policy = RolePolicy::kChordTone,
                                                  .events = Span<const StyleEvent>(kVarDChord),
                                                  .motif = &kPeakChordMotif},
                                                 {.role = TrackRole::kPad,
                                                  .policy = RolePolicy::kChordTone,
                                                  .events = Span<const StyleEvent>(kPadRehit),
                                                  .gm_program = kPadVoice,
                                                  .voicing = VoicingPolicy::kLead,
                                                  .motif = &kSharedPadMotif},
                                                 {.role = TrackRole::kChord2,
                                                  .policy = RolePolicy::kChordTone,
                                                  .events = Span<const StyleEvent>(kChord2On),
                                                  .gm_program = kChord2Voice,
                                                  .voicing = VoicingPolicy::kLead,
                                                  .motif = &kSharedChord2Motif},
                                                 {.role = TrackRole::kArp,
                                                  .policy = RolePolicy::kChordTone,
                                                  .events = Span<const StyleEvent>(kArp8),
                                                  .gm_program = kArpVoice},
                                                 {.role = TrackRole::kLead,
                                                  .policy = RolePolicy::kChordTone,
                                                  .events = Span<const StyleEvent>(kLeadHook),
                                                  .gm_program = kLeadVoice,
                                                  .motif = &kLeadHookMotif},
                                                 {.role = TrackRole::kPerc,
                                                  .policy = RolePolicy::kFixed,
                                                  .events = Span<const StyleEvent>(kPercShake)}};

// varBreak (Part B, pop delta): the bright pop stop — everybody drops out on
// an accented downbeat (kick+crash, a bass pop, a triad stab), then wide
// silence, then a hand-clap pickup (not rock's snare/tom fill) throws the
// groove back in.
inline constexpr StyleEvent kBrkDrums[] = {
    {.step = 0, .tone = kKick, .octave = 0, .vel = 110, .gate = kGateHat},
    {.step = 0, .tone = kCrash, .octave = 0, .vel = 100, .gate = kGateStab},
    {.step = 12, .tone = kClap, .octave = 0, .vel = 100, .gate = kGateHat},
    {.step = 14, .tone = kClap, .octave = 0, .vel = 104, .gate = kGateHat},
};
inline constexpr StyleEvent kBrkBass[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 100, .gate = kGateStab},
};
inline constexpr StyleEvent kBrkChord1[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 96, .gate = kGateStab},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 96, .gate = kGateStab},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 96, .gate = kGateStab},
};
inline constexpr StylePattern kBrkPatterns[] = {
    {.role = TrackRole::kDrums,
     .policy = RolePolicy::kFixed,
     .events = Span<const StyleEvent>(kBrkDrums)},
    {.role = TrackRole::kBass,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kBrkBass)},
    {.role = TrackRole::kChord1,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kBrkChord1)},
};

inline constexpr StyleSection kSections[] = {
    {.type = SectionType::kIntro1, .bars = 2, .patterns = Span<const StylePattern>(kIntroPatterns)},
    {.type = SectionType::kIntro2,
     .bars = 1,
     .patterns = Span<const StylePattern>(kIntro2Patterns)},
    {.type = SectionType::kVarA, .bars = 2, .patterns = Span<const StylePattern>(kVarAPatterns)},
    {.type = SectionType::kVarB, .bars = 1, .patterns = Span<const StylePattern>(kVarBPatterns)},
    {.type = SectionType::kVarC, .bars = 1, .patterns = Span<const StylePattern>(kVarCPatterns)},
    {.type = SectionType::kVarD, .bars = 1, .patterns = Span<const StylePattern>(kVarDPatterns)},
    {.type = SectionType::kFillA, .bars = 1, .patterns = Span<const StylePattern>(kFillPatterns)},
    {.type = SectionType::kFillB, .bars = 1, .patterns = Span<const StylePattern>(kFillBPatterns)},
    {.type = SectionType::kFillC, .bars = 1, .patterns = Span<const StylePattern>(kFillCPatterns)},
    {.type = SectionType::kFillD, .bars = 1, .patterns = Span<const StylePattern>(kFillDPatterns)},
    {.type = SectionType::kBreak, .bars = 1, .patterns = Span<const StylePattern>(kBrkPatterns)},
    {.type = SectionType::kEnding1, .bars = 2, .patterns = Span<const StylePattern>(kEndPatterns)},
    {.type = SectionType::kEnding2, .bars = 2, .patterns = Span<const StylePattern>(kEnd2Patterns)},
};
// Groove (9100 Wave 1, task B): mainstream, bright genre -- a moderate
// deterministic humanize.
inline constexpr Style kStyle{.name = "pop",
                              .sections = Span<const StyleSection>(kSections),
                              .groove = {.humanize_timing = 8, .humanize_velocity = 16},
                              .tempo = 12000};

}  // namespace pop

}  // namespace styles
}  // namespace arrangrr
