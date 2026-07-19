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
// Re-articulated at step 16 too (style-depth Wave-2 C): VarA is now 2 bars,
// and the sustain re-triggers every bar exactly as it always has when the
// section looped -- a background hold, not the section's own bar-to-bar
// variation. Intro2/VarB stay 1 bar and never read step>=16, so this is safe.
inline constexpr StyleEvent kPadTriad[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 56, .gate = kGateHeld},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 52, .gate = kGateHeld},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 56, .gate = kGateHeld},
    {.step = 16, .tone = kRoot, .octave = 0, .vel = 56, .gate = kGateHeld},
    {.step = 16, .tone = kThird, .octave = 0, .vel = 52, .gate = kGateHeld},
    {.step = 16, .tone = kFifth, .octave = 0, .vel = 56, .gate = kGateHeld},
};
inline constexpr StyleEvent kPad7[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 60, .gate = kGateHeld},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 56, .gate = kGateHeld},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 60, .gate = kGateHeld},
    {.step = 0, .tone = kSeventh, .octave = 0, .vel = 54, .gate = kGateHeld},
    // bar 2 (ending1 only, Part A, rock delta): restate the power chord (no
    // third, matching kEndChord's own voicing) into bar 2.
    {.step = 16, .tone = kRoot, .octave = 0, .vel = 60, .gate = kGateHeld},
    {.step = 16, .tone = kFifth, .octave = 0, .vel = 60, .gate = kGateHeld},
    {.step = 16, .tone = kRoot, .octave = 1, .vel = 56, .gate = kGateHeld},
};
// Ending2-only fork of kPad7 (Part A): ending1 restates the held chord at
// step 16, ending2 stays silent there and only sounds under the big final
// hit at step 24 -- the two endings diverge, so this duplicates kPad7's
// original bar-1 content verbatim and adds its own distinct bar-2 tail.
inline constexpr StyleEvent kPad7Ending2[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 60, .gate = kGateHeld},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 56, .gate = kGateHeld},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 60, .gate = kGateHeld},
    {.step = 0, .tone = kSeventh, .octave = 0, .vel = 54, .gate = kGateHeld},
    {.step = 24, .tone = kRoot, .octave = 0, .vel = 60, .gate = kGateHeld},
    {.step = 24, .tone = kFifth, .octave = 0, .vel = 60, .gate = kGateHeld},
    {.step = 24, .tone = kRoot, .octave = 1, .vel = 56, .gate = kGateHeld},
};
inline constexpr StyleEvent kPadRehit[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 60, .gate = kGateHalfBar},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 56, .gate = kGateHalfBar},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 60, .gate = kGateHalfBar},
    {.step = 0, .tone = kSeventh, .octave = 0, .vel = 54, .gate = kGateHalfBar},
    {.step = 8, .tone = kRoot, .octave = 0, .vel = 58, .gate = kGateHalfBar},
    {.step = 8, .tone = kThird, .octave = 0, .vel = 54, .gate = kGateHalfBar},
    {.step = 8, .tone = kFifth, .octave = 0, .vel = 58, .gate = kGateHalfBar},
    {.step = 8, .tone = kSeventh, .octave = 0, .vel = 52, .gate = kGateHalfBar},
};
// Organ power stabs on the off-beats — drives against the on-beat kChord1.
inline constexpr StyleEvent kChord2Off[] = {
    {.step = 2, .tone = kRoot, .octave = 0, .vel = 74, .gate = kGateStab},
    {.step = 2, .tone = kFifth, .octave = 0, .vel = 74, .gate = kGateStab},
    {.step = 6, .tone = kRoot, .octave = 0, .vel = 70, .gate = kGateStab},
    {.step = 6, .tone = kFifth, .octave = 0, .vel = 70, .gate = kGateStab},
    {.step = 10, .tone = kRoot, .octave = 0, .vel = 74, .gate = kGateStab},
    {.step = 10, .tone = kFifth, .octave = 0, .vel = 74, .gate = kGateStab},
    {.step = 14, .tone = kRoot, .octave = 0, .vel = 70, .gate = kGateStab},
    {.step = 14, .tone = kFifth, .octave = 0, .vel = 70, .gate = kGateStab},
};
// Wave-2.5 (rock.hpp motif-engine bug fix, owner-approved): VarA-ONLY dedicated
// 2-bar Chord2 array. kChord2Off above stays motif-locked (kSharedChord2Motif,
// shared with VarB/VarD) and its motif-generated events never carry step>=16
// (motif.hpp caps a generated span at kMaxMotifLen=16), which is exactly why
// VarA's Chord2 used to go silent in bar 2 -- flagged, not fixed, in the
// style-depth Wave-2 C pass (see the comment on kVarAPatterns' Chord2 entry
// below). The fix follows the SAME convention VarA's own drums/bass/chord1
// already use: a fully-authored, non-motif 2-bar array (bar 1 identical to
// kChord2Off, bar 2 a mirrored answer with a fuller turnaround on the last
// hit), wired ONLY into kVarAPatterns -- VarB/VarD keep using kChord2Off with
// kSharedChord2Motif, untouched.
inline constexpr StyleEvent kChord2OffVarA[] = {
    {.step = 2, .tone = kRoot, .octave = 0, .vel = 74, .gate = kGateStab},
    {.step = 2, .tone = kFifth, .octave = 0, .vel = 74, .gate = kGateStab},
    {.step = 6, .tone = kRoot, .octave = 0, .vel = 70, .gate = kGateStab},
    {.step = 6, .tone = kFifth, .octave = 0, .vel = 70, .gate = kGateStab},
    {.step = 10, .tone = kRoot, .octave = 0, .vel = 74, .gate = kGateStab},
    {.step = 10, .tone = kFifth, .octave = 0, .vel = 74, .gate = kGateStab},
    {.step = 14, .tone = kRoot, .octave = 0, .vel = 70, .gate = kGateStab},
    {.step = 14, .tone = kFifth, .octave = 0, .vel = 70, .gate = kGateStab},
    {.step = 18, .tone = kRoot, .octave = 0, .vel = 74, .gate = kGateStab},
    {.step = 18, .tone = kFifth, .octave = 0, .vel = 74, .gate = kGateStab},
    {.step = 22, .tone = kRoot, .octave = 0, .vel = 70, .gate = kGateStab},
    {.step = 22, .tone = kFifth, .octave = 0, .vel = 70, .gate = kGateStab},
    {.step = 26, .tone = kRoot, .octave = 0, .vel = 74, .gate = kGateStab},
    {.step = 26, .tone = kFifth, .octave = 0, .vel = 74, .gate = kGateStab},
    {.step = 30, .tone = kRoot, .octave = 0, .vel = 76, .gate = kGateStab},
    {.step = 30, .tone = kSeventh, .octave = 0, .vel = 76, .gate = kGateStab},
};
// Organ sustained under the half-time stomp (varC).
inline constexpr StyleEvent kChord2HalfBar[] = {
    {.step = 0, .tone = kThird, .octave = 0, .vel = 68, .gate = kGateHalfBar},
    {.step = 0, .tone = kSeventh, .octave = 0, .vel = 64, .gate = kGateHalfBar},
    {.step = 8, .tone = kThird, .octave = 0, .vel = 66, .gate = kGateHalfBar},
    {.step = 8, .tone = kSeventh, .octave = 0, .vel = 62, .gate = kGateHalfBar},
};
// Clean-guitar arpeggio, high register.
inline constexpr StyleEvent kArp8[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 76, .gate = kGateHat},
    {.step = 2, .tone = kFifth, .octave = 0, .vel = 68, .gate = kGateHat},
    {.step = 4, .tone = kSeventh, .octave = 0, .vel = 72, .gate = kGateHat},
    {.step = 6, .tone = kRoot, .octave = 1, .vel = 68, .gate = kGateHat},
    {.step = 8, .tone = kFifth, .octave = 0, .vel = 76, .gate = kGateHat},
    {.step = 10, .tone = kSeventh, .octave = 0, .vel = 68, .gate = kGateHat},
    {.step = 12, .tone = kRoot, .octave = 1, .vel = 72, .gate = kGateHat},
    {.step = 14, .tone = kFifth, .octave = 0, .vel = 68, .gate = kGateHat},
};
// Cowbell quarters + tambourine backbeat; a plain tambourine for fills.
inline constexpr StyleEvent kPercCowbell[] = {
    {.step = 0, .tone = kCowbell, .octave = 0, .vel = 84, .gate = kGateHat},
    {.step = 4, .tone = kCowbell, .octave = 0, .vel = 78, .gate = kGateHat},
    {.step = 8, .tone = kCowbell, .octave = 0, .vel = 84, .gate = kGateHat},
    {.step = 12, .tone = kCowbell, .octave = 0, .vel = 78, .gate = kGateHat},
    {.step = 4, .tone = kTambourine, .octave = 0, .vel = 82, .gate = kGateHat},
    {.step = 12, .tone = kTambourine, .octave = 0, .vel = 84, .gate = kGateHat},
};
inline constexpr StyleEvent kPercTamb[] = {
    {.step = 4, .tone = kTambourine, .octave = 0, .vel = 84, .gate = kGateHat},
    {.step = 12, .tone = kTambourine, .octave = 0, .vel = 86, .gate = kGateHat},
};
// A rock lead lick for the back half of the peak bar. kScaleDegree keeps it
// diatonic to the live KEY over any chord; the step-10 grace is a kInterval blue
// b3 (3 semitones) off the current chord ROOT — it bends bluesy against the
// harmony, not the key. kLead role -> anchor 72, screaming over the wall.
inline constexpr std::int16_t kLeadVoice = 29;  // Overdriven Guitar
inline constexpr StyleEvent kLeadLick[] = {
    {.step = 8,
     .tone = 4,
     .octave = 0,
     .vel = 104,
     .gate = kGate8th,
     .src = NoteSource::kScaleDegree},  // 5th
    {.step = 10,
     .tone = 3,
     .octave = 0,
     .vel = 98,
     .gate = kGateStaccato,
     .src = NoteSource::kInterval},  // blue b3 off the chord root
    {.step = 12,
     .tone = 4,
     .octave = 0,
     .vel = 106,
     .gate = kGate8th,
     .src = NoteSource::kScaleDegree},  // 5th
    {.step = 14,
     .tone = 7,
     .octave = 0,
     .vel = 110,
     .gate = kGateBeat,
     .src = NoteSource::kScaleDegree},  // up to the octave
};
// Motif engine (9210, Ottorino RANK 10): the blues-rock lead lick, wired only
// into varD — a genuine "call-response guitar lick" idiom (blues-rock solo
// phrasing).
inline constexpr MotifSpec kLeadLickMotif{.transform = MotifTransform::kDiatonicTranspose,
                                          .seed = 1001};
// Motif engine (9210 Wave 1, style-depth program task A): extend variation
// onto drums/chord1/chord2/pad, wired onto the peak variation (varD) plus the
// shared reused Pad (kPadRehit, varC+varD) and Chord2 (kChord2Off, varA/B/D)
// arrays. kDisplacement for drums/pad/chord2 (a soft evolving push);
// kRetrograde for the chord1 strums (a legible mirrored answer, only ever
// touches `step` so the strum gesture itself is unaffected).
inline constexpr MotifSpec kPeakDrumsMotif{.transform = MotifTransform::kDisplacement,
                                           .seed = 1010};
inline constexpr MotifSpec kPeakChordMotif{.transform = MotifTransform::kRetrograde, .seed = 1011};
inline constexpr MotifSpec kSharedPadMotif{.transform = MotifTransform::kDisplacement,
                                           .seed = 1012};
inline constexpr MotifSpec kSharedChord2Motif{.transform = MotifTransform::kDisplacement,
                                              .seed = 1013};

inline constexpr StyleEvent kVarADrums[] = {
    // bar 1 (unchanged)
    {.step = 0, .tone = kKick, .octave = 0, .vel = 118, .gate = kGateHat},
    {.step = 3, .tone = kKick, .octave = 0, .vel = 100, .gate = kGateHat},
    {.step = 8, .tone = kKick, .octave = 0, .vel = 114, .gate = kGateHat},
    {.step = 11, .tone = kKick, .octave = 0, .vel = 100, .gate = kGateHat},
    {.step = 4, .tone = kSnare, .octave = 0, .vel = 116, .gate = kGateHat},
    {.step = 12, .tone = kSnare, .octave = 0, .vel = 116, .gate = kGateHat},
    {.step = 0, .tone = kClosedHat, .octave = 0, .vel = 90, .gate = kGateHat},
    {.step = 2, .tone = kClosedHat, .octave = 0, .vel = 76, .gate = kGateHat},
    {.step = 4, .tone = kClosedHat, .octave = 0, .vel = 90, .gate = kGateHat},
    {.step = 6, .tone = kOpenHat, .octave = 0, .vel = 88, .gate = kGate8th},
    {.step = 8, .tone = kClosedHat, .octave = 0, .vel = 90, .gate = kGateHat},
    {.step = 10, .tone = kClosedHat, .octave = 0, .vel = 76, .gate = kGateHat},
    {.step = 12, .tone = kClosedHat, .octave = 0, .vel = 90, .gate = kGateHat},
    {.step = 14, .tone = kOpenHat, .octave = 0, .vel = 88, .gate = kGate8th},
    // bar 2 (style-depth Wave-2 C): a turnaround -- a ghost snare pushes into
    // beat 3, and the hat opens twice more (steps 22, 26) instead of once,
    // building toward the repeat.
    {.step = 16, .tone = kKick, .octave = 0, .vel = 118, .gate = kGateHat},
    {.step = 19, .tone = kKick, .octave = 0, .vel = 100, .gate = kGateHat},
    {.step = 24, .tone = kKick, .octave = 0, .vel = 114, .gate = kGateHat},
    {.step = 20, .tone = kSnare, .octave = 0, .vel = 116, .gate = kGateHat},
    {.step = 27, .tone = kSnare, .octave = 0, .vel = 70, .gate = kGateHat},
    {.step = 28, .tone = kSnare, .octave = 0, .vel = 116, .gate = kGateHat},
    {.step = 16, .tone = kClosedHat, .octave = 0, .vel = 90, .gate = kGateHat},
    {.step = 18, .tone = kClosedHat, .octave = 0, .vel = 76, .gate = kGateHat},
    {.step = 20, .tone = kClosedHat, .octave = 0, .vel = 90, .gate = kGateHat},
    {.step = 22, .tone = kOpenHat, .octave = 0, .vel = 88, .gate = kGate8th},
    {.step = 24, .tone = kClosedHat, .octave = 0, .vel = 90, .gate = kGateHat},
    {.step = 26, .tone = kOpenHat, .octave = 0, .vel = 88, .gate = kGate8th},
    {.step = 28, .tone = kClosedHat, .octave = 0, .vel = 90, .gate = kGateHat},
    {.step = 30, .tone = kOpenHat, .octave = 0, .vel = 92, .gate = kGate8th},
};
inline constexpr StyleEvent kVarABass[] = {
    // bar 1 (unchanged): power root with an octave push on the off-beats.
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 116, .gate = kGate8th},
    {.step = 2, .tone = kRoot, .octave = 1, .vel = 96, .gate = kGateHat},
    {.step = 4, .tone = kRoot, .octave = 0, .vel = 110, .gate = kGate8th},
    {.step = 6, .tone = kRoot, .octave = 1, .vel = 96, .gate = kGateHat},
    {.step = 8, .tone = kFifth, .octave = 0, .vel = 112, .gate = kGate8th},
    {.step = 10, .tone = kFifth, .octave = 1, .vel = 96, .gate = kGateHat},
    {.step = 12, .tone = kRoot, .octave = 0, .vel = 110, .gate = kGate8th},
    {.step = 14, .tone = kRoot, .octave = 1, .vel = 96, .gate = kGateHat},
    // bar 2 (style-depth Wave-2 C): the octave push moves earlier (steps
    // 19/27 instead of 2/10), and the bar resolves on the leading 7th into the repeat.
    {.step = 16, .tone = kRoot, .octave = 0, .vel = 116, .gate = kGate8th},
    {.step = 19, .tone = kRoot, .octave = 1, .vel = 98, .gate = kGateHat},
    {.step = 20, .tone = kRoot, .octave = 0, .vel = 110, .gate = kGate8th},
    {.step = 24, .tone = kFifth, .octave = 0, .vel = 112, .gate = kGate8th},
    {.step = 27, .tone = kFifth, .octave = 1, .vel = 98, .gate = kGateHat},
    {.step = 28, .tone = kRoot, .octave = 0, .vel = 110, .gate = kGate8th},
    {.step = 30, .tone = kSeventh, .octave = 0, .vel = 100, .gate = kGateHat},
};
inline constexpr StyleEvent kVarAChord[] = {
    // bar 1 (unchanged): downbeat electric-guitar strums — one gesture per hit
    // sweeps the whole live chord high->low (the authored tone is ignored).
    // Collapsed from the former root+fifth block so the sweep fans the full
    // chord, not one strum per tone.
    {.step = 0,
     .tone = kRoot,
     .octave = 0,
     .vel = 110,
     .gate = kGateBeat,
     .gesture = ChordGesture::kStrumDown},
    {.step = 8,
     .tone = kRoot,
     .octave = 0,
     .vel = 108,
     .gate = kGateBeat,
     .gesture = ChordGesture::kStrumDown},
    // bar 2 (style-depth Wave-2 C): the same downbeat strums, plus a short
    // stab on the & of 4 pushing into the repeat.
    {.step = 16,
     .tone = kRoot,
     .octave = 0,
     .vel = 110,
     .gate = kGateBeat,
     .gesture = ChordGesture::kStrumDown},
    {.step = 24,
     .tone = kRoot,
     .octave = 0,
     .vel = 108,
     .gate = kGateBeat,
     .gesture = ChordGesture::kStrumDown},
    {.step = 30,
     .tone = kRoot,
     .octave = 0,
     .vel = 100,
     .gate = kGateStab,
     .gesture = ChordGesture::kStrumDown},
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
    // Wave-2.5 bug fix: Chord2 used to stay motif-driven off kChord2Off here,
    // which -- being shared with VarB/VarD via kSharedChord2Motif -- can never
    // carry a motif-generated event past step 15 (motif.hpp caps a generated
    // span at kMaxMotifLen=16), so Chord2 went silent in bar 2 of this now-2-bar
    // VarA. Fixed with a dedicated, fully-authored, non-motif 2-bar array
    // (kChord2OffVarA, defined above) -- the same convention VarA's own
    // drums/bass/chord1 already use. VarB/VarD are untouched and keep reading
    // kChord2Off through kSharedChord2Motif.
    {.role = TrackRole::kChord2,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kChord2OffVarA),
     .gm_program = kChord2Voice},
};

// VarB stays 1 bar (style-depth Wave-2 C): the crash-on-downbeat entrance and
// its own kick pushes already read as a sharp contrast lift from VarA; a 2nd
// bar here would restate that same contrast rather than add a new idea.
inline constexpr StyleEvent kVarBDrums[] = {
    {.step = 0,
     .tone = kCrash,
     .octave = 0,
     .vel = 118,
     .gate = kGateBeat},  // crash on the downbeat
    {.step = 0, .tone = kKick, .octave = 0, .vel = 120, .gate = kGateHat},
    {.step = 2, .tone = kKick, .octave = 0, .vel = 100, .gate = kGateHat},
    {.step = 3, .tone = kKick, .octave = 0, .vel = 104, .gate = kGateHat},
    {.step = 8, .tone = kKick, .octave = 0, .vel = 116, .gate = kGateHat},
    {.step = 10, .tone = kKick, .octave = 0, .vel = 100, .gate = kGateHat},
    {.step = 11, .tone = kKick, .octave = 0, .vel = 104, .gate = kGateHat},
    {.step = 4, .tone = kSnare, .octave = 0, .vel = 118, .gate = kGateHat},
    {.step = 12, .tone = kSnare, .octave = 0, .vel = 118, .gate = kGateHat},
    {.step = 2, .tone = kOpenHat, .octave = 0, .vel = 92, .gate = kGate8th},
    {.step = 6, .tone = kOpenHat, .octave = 0, .vel = 92, .gate = kGate8th},
    {.step = 10, .tone = kOpenHat, .octave = 0, .vel = 92, .gate = kGate8th},
    {.step = 14, .tone = kOpenHat, .octave = 0, .vel = 92, .gate = kGate8th},
};
inline constexpr StyleEvent kVarBBass[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 120, .gate = kGate8th},
    {.step = 2, .tone = kRoot, .octave = 1, .vel = 100, .gate = kGateHat},
    {.step = 3, .tone = kRoot, .octave = 0, .vel = 100, .gate = kGateHat},
    {.step = 4, .tone = kFifth, .octave = 0, .vel = 112, .gate = kGate8th},
    {.step = 8, .tone = kRoot, .octave = 0, .vel = 116, .gate = kGate8th},
    {.step = 10, .tone = kSeventh, .octave = 0, .vel = 100, .gate = kGateHat},
    {.step = 12, .tone = kFifth, .octave = 0, .vel = 112, .gate = kGate8th},
};
inline constexpr StyleEvent kVarBChord[] = {
    // Full downbeat strums plus an upbeat accent on 4 — each a single gesture.
    {.step = 0,
     .tone = kRoot,
     .octave = 0,
     .vel = 112,
     .gate = kGateBeat,
     .gesture = ChordGesture::kStrumDown},
    {.step = 8,
     .tone = kRoot,
     .octave = 0,
     .vel = 110,
     .gate = kGateBeat,
     .gesture = ChordGesture::kStrumDown},
    {.step = 12,
     .tone = kRoot,
     .octave = 0,
     .vel = 106,
     .gate = kGateStab,
     .gesture = ChordGesture::kStrumDown},
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
     .events = Span<const StyleEvent>(kChord2Off),
     .gm_program = kChord2Voice,
     .motif = &kSharedChord2Motif},
    {.role = TrackRole::kPerc,
     .policy = RolePolicy::kFixed,
     .events = Span<const StyleEvent>(kPercTamb)},
};

inline constexpr StyleEvent kIntroDrums[] = {
    // bar 1 (unchanged): snare roll pickup crescendo into the downbeat.
    {.step = 12, .tone = kSnare, .octave = 0, .vel = 96, .gate = 100},
    {.step = 13, .tone = kSnare, .octave = 0, .vel = 104, .gate = 100},
    {.step = 14, .tone = kSnare, .octave = 0, .vel = 112, .gate = 100},
    {.step = 15, .tone = kSnare, .octave = 0, .vel = 120, .gate = 100},
    // bar 2 (style-depth Wave-2 C): the arrival -- crash on the downbeat, full
    // kick/snare backbeat under 8th hats, landing right where VarA starts.
    {.step = 16, .tone = kCrash, .octave = 0, .vel = 112, .gate = kGateBeat},
    {.step = 16, .tone = kKick, .octave = 0, .vel = 118, .gate = kGateHat},
    {.step = 24, .tone = kKick, .octave = 0, .vel = 114, .gate = kGateHat},
    {.step = 20, .tone = kSnare, .octave = 0, .vel = 114, .gate = kGateHat},
    {.step = 28, .tone = kSnare, .octave = 0, .vel = 116, .gate = kGateHat},
    {.step = 16, .tone = kClosedHat, .octave = 0, .vel = 88, .gate = kGateHat},
    {.step = 18, .tone = kClosedHat, .octave = 0, .vel = 74, .gate = kGateHat},
    {.step = 20, .tone = kClosedHat, .octave = 0, .vel = 88, .gate = kGateHat},
    {.step = 22, .tone = kClosedHat, .octave = 0, .vel = 74, .gate = kGateHat},
    {.step = 24, .tone = kClosedHat, .octave = 0, .vel = 88, .gate = kGateHat},
    {.step = 26, .tone = kClosedHat, .octave = 0, .vel = 74, .gate = kGateHat},
    {.step = 28, .tone = kClosedHat, .octave = 0, .vel = 88, .gate = kGateHat},
    {.step = 30, .tone = kOpenHat, .octave = 0, .vel = 90, .gate = kGate8th},
};
// Held root pickup. Shared with Intro2 (kIntro2Patterns below), which stays 1
// bar and never reads step>=16, so the Intro1-only bar-2 tail is safe here.
inline constexpr StyleEvent kIntroBass[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 110, .gate = kGateHeld},
    // bar 2 (style-depth Wave-2 C, Intro1 only): hold through, then a short
    // octave push into VarA.
    {.step = 16, .tone = kRoot, .octave = 0, .vel = 114, .gate = 3200},
    {.step = 30, .tone = kRoot, .octave = 1, .vel = 100, .gate = kGate8th},
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
    {.step = 0, .tone = kKick, .octave = 0, .vel = 116, .gate = kGateHat},
    {.step = 4, .tone = kSnare, .octave = 0, .vel = 110, .gate = 100},
    {.step = 6, .tone = kSnare, .octave = 0, .vel = 104, .gate = 100},
    {.step = 8, .tone = kSnare, .octave = 0, .vel = 108, .gate = 100},
    {.step = 10, .tone = kSnare, .octave = 0, .vel = 112, .gate = 100},
    {.step = 12, .tone = kSnare, .octave = 0, .vel = 116, .gate = 100},
    {.step = 13, .tone = kSnare, .octave = 0, .vel = 118, .gate = 100},
    {.step = 14, .tone = kSnare, .octave = 0, .vel = 120, .gate = 100},
    {.step = 15, .tone = kSnare, .octave = 0, .vel = 120, .gate = 100},
};
inline constexpr StyleEvent kFillBass[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 114, .gate = kGate8th},
    {.step = 8, .tone = kFifth, .octave = 0, .vel = 108, .gate = kGate8th},
};
// The flat FILL bass, reused identically across all 4 fills; the Var bass
// (kVarABass..kVarDBass) is already 4 distinct hand-authored ideas, so it is
// not a target.
inline constexpr MotifSpec kFillBassMotif{.transform = MotifTransform::kRetrograde, .seed = 1002};
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
    {.step = 0, .tone = kKick, .octave = 0, .vel = 120, .gate = kGateHat},
    {.step = 0, .tone = kCrash, .octave = 0, .vel = 118, .gate = kGateHalfBar},
    // bar 2 (Part A, 2-bar endings): the arrival hit -- kick under a held crash.
    {.step = 16, .tone = kKick, .octave = 0, .vel = 120, .gate = kGateHat},
    {.step = 16, .tone = kCrash, .octave = 0, .vel = 118, .gate = kGateHeld},
};
inline constexpr StyleEvent kEndBass[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 116, .gate = kGateHeld},
    {.step = 16, .tone = kRoot, .octave = 0, .vel = 116, .gate = kGateHeld},
};
inline constexpr StyleEvent kEndChord[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 112, .gate = kGateHeld},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 112, .gate = kGateHeld},
    {.step = 0, .tone = kRoot, .octave = 1, .vel = 112, .gate = kGateHeld},
    // bar 2 (Part A, rock delta): a sus4 pickup grace note, then the strummed
    // power-chord arrival -- ONE gesture event (gesture::expand fans the whole
    // live chord regardless of authored tone, so this mirrors kVarAChord's own
    // single-event-per-strum idiom rather than tagging 3 stacked literal tones,
    // which would triple-fan the same chord).
    {.step = 14,
     .tone = 5,
     .octave = 0,
     .vel = 100,
     .gate = kGate8th,
     .src = NoteSource::kInterval},
    {.step = 16,
     .tone = kRoot,
     .octave = 0,
     .vel = 114,
     .gate = kGateHeld,
     .gesture = ChordGesture::kStrumDown},
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
};

inline constexpr StyleEvent kIntro2Drums[] = {
    {.step = 0, .tone = kCrash, .octave = 0, .vel = 110, .gate = kGateBeat},
    {.step = 0, .tone = kKick, .octave = 0, .vel = 118, .gate = kGateHat},
    {.step = 8, .tone = kKick, .octave = 0, .vel = 112, .gate = kGateHat},
    {.step = 4, .tone = kSnare, .octave = 0, .vel = 112, .gate = kGateHat},
    {.step = 12, .tone = kSnare, .octave = 0, .vel = 114, .gate = kGateHat},
    {.step = 0, .tone = kClosedHat, .octave = 0, .vel = 88, .gate = kGateHat},
    {.step = 2, .tone = kClosedHat, .octave = 0, .vel = 74, .gate = kGateHat},
    {.step = 4, .tone = kClosedHat, .octave = 0, .vel = 88, .gate = kGateHat},
    {.step = 6, .tone = kClosedHat, .octave = 0, .vel = 74, .gate = kGateHat},
    {.step = 8, .tone = kClosedHat, .octave = 0, .vel = 88, .gate = kGateHat},
    {.step = 10, .tone = kClosedHat, .octave = 0, .vel = 74, .gate = kGateHat},
    {.step = 12, .tone = kClosedHat, .octave = 0, .vel = 88, .gate = kGateHat},
    {.step = 14, .tone = kOpenHat, .octave = 0, .vel = 86, .gate = kGate8th},
};
inline constexpr StyleEvent kIntro2Chord[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 104, .gate = kGateBeat},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 104, .gate = kGateBeat},
    {.step = 8, .tone = kRoot, .octave = 0, .vel = 102, .gate = kGateBeat},
    {.step = 8, .tone = kFifth, .octave = 0, .vel = 102, .gate = kGateBeat},
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
    {.step = 0, .tone = kKick, .octave = 0, .vel = 118, .gate = kGateHat},
    {.step = 4, .tone = kSnare, .octave = 0, .vel = 112, .gate = 120},
    {.step = 6, .tone = kSnare, .octave = 0, .vel = 106, .gate = 120},
    {.step = 8, .tone = kTomHi, .octave = 0, .vel = 110, .gate = 120},
    {.step = 10, .tone = kTomMid, .octave = 0, .vel = 112, .gate = 120},
    {.step = 12, .tone = kTomLow, .octave = 0, .vel = 116, .gate = 120},
    {.step = 14, .tone = kTomFloor, .octave = 0, .vel = 120, .gate = 120},
};
inline constexpr StyleEvent kFillCDrums[] = {
    {.step = 0, .tone = kTomHi, .octave = 0, .vel = 110, .gate = 100},
    {.step = 2, .tone = kTomHi, .octave = 0, .vel = 106, .gate = 100},
    {.step = 4, .tone = kTomMid, .octave = 0, .vel = 112, .gate = 100},
    {.step = 6, .tone = kTomMid, .octave = 0, .vel = 108, .gate = 100},
    {.step = 8, .tone = kSnare, .octave = 0, .vel = 114, .gate = 100},
    {.step = 10, .tone = kSnare, .octave = 0, .vel = 110, .gate = 100},
    {.step = 12, .tone = kTomLow, .octave = 0, .vel = 116, .gate = 100},
    {.step = 14, .tone = kTomFloor, .octave = 0, .vel = 120, .gate = 100},
};
inline constexpr StyleEvent kFillDDrums[] = {
    {.step = 0, .tone = kSnare, .octave = 0, .vel = 104, .gate = 50},
    {.step = 1, .tone = kSnare, .octave = 0, .vel = 98, .gate = 50},
    {.step = 2, .tone = kTomHi, .octave = 0, .vel = 108, .gate = 50},
    {.step = 3, .tone = kTomHi, .octave = 0, .vel = 102, .gate = 50},
    {.step = 4, .tone = kTomMid, .octave = 0, .vel = 110, .gate = 50},
    {.step = 5, .tone = kTomMid, .octave = 0, .vel = 104, .gate = 50},
    {.step = 6, .tone = kTomLow, .octave = 0, .vel = 112, .gate = 50},
    {.step = 7, .tone = kTomLow, .octave = 0, .vel = 106, .gate = 50},
    {.step = 8, .tone = kTomFloor, .octave = 0, .vel = 114, .gate = 50},
    {.step = 9, .tone = kTomFloor, .octave = 0, .vel = 108, .gate = 50},
    {.step = 10, .tone = kSnare, .octave = 0, .vel = 116, .gate = 50},
    {.step = 11, .tone = kSnare, .octave = 0, .vel = 110, .gate = 50},
    {.step = 12, .tone = kSnare, .octave = 0, .vel = 118, .gate = 50},
    {.step = 13, .tone = kSnare, .octave = 0, .vel = 114, .gate = 50},
    {.step = 14, .tone = kSnare, .octave = 0, .vel = 120, .gate = 50},
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
     .events = Span<const StyleEvent>(kPercTamb)},
};

inline constexpr StyleEvent kEnd2Drums[] = {
    {.step = 0, .tone = kCrash, .octave = 0, .vel = 120, .gate = kGateHeld},
    {.step = 0, .tone = kKick, .octave = 0, .vel = 120, .gate = kGateHat},
    {.step = 8, .tone = kSnare, .octave = 0, .vel = 114, .gate = 120},
    {.step = 8, .tone = kKick, .octave = 0, .vel = 116, .gate = kGateHat},
    {.step = 12, .tone = kCrash, .octave = 0, .vel = 112, .gate = kGateBeat},
    // bar 2 (Part A, "Amen tag"): a short punctuation kick on 1, the big
    // final hit (kick+held crash) on beat 3 -- a second, later, bigger arrival.
    {.step = 16, .tone = kKick, .octave = 0, .vel = 110, .gate = kGateHat},
    {.step = 24, .tone = kKick, .octave = 0, .vel = 118, .gate = kGateHat},
    {.step = 24, .tone = kCrash, .octave = 0, .vel = 120, .gate = kGateHeld},
};
inline constexpr StyleEvent kEnd2Bass[] = {
    {.step = 0, .tone = kRoot, .octave = -1, .vel = 118, .gate = kGateHeld},
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 110, .gate = kGateHeld},
    {.step = 16, .tone = kRoot, .octave = 0, .vel = 110, .gate = kGateBeat},
    {.step = 24, .tone = kRoot, .octave = -1, .vel = 118, .gate = kGateHeld},
    {.step = 24, .tone = kRoot, .octave = 0, .vel = 110, .gate = kGateHeld},
};
inline constexpr StyleEvent kEnd2Chord[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 114, .gate = kGateHeld},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 114, .gate = kGateHeld},
    {.step = 0, .tone = kRoot, .octave = 1, .vel = 114, .gate = kGateHeld},
    // bar 2 (Part A, "Amen tag", rock delta): a short 6th/4th color tag on 1,
    // then the big strummed power-chord hit on beat 3 of bar 2 -- NOT the
    // downbeat, reading as a second, later, bigger arrival.
    {.step = 16,
     .tone = 9,
     .octave = 0,
     .vel = 90,
     .gate = kGateBeat,
     .src = NoteSource::kInterval},
    {.step = 16,
     .tone = 5,
     .octave = 0,
     .vel = 88,
     .gate = kGateBeat,
     .src = NoteSource::kInterval},
    {.step = 24,
     .tone = kRoot,
     .octave = 0,
     .vel = 118,
     .gate = kGateHeld,
     .gesture = ChordGesture::kStrumDown},
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
};

// varC stays 1 bar (style-depth Wave-2 C): the half-time stomp is already a
// wide, sparse idiom (a single held power chord over a slow-moving kick) that
// reads as long even at 1 bar; a 2nd bar would need genuinely new material to
// avoid feeling like padding on top of an already-spacious groove.
// varC: half-time stomp — huge kick, backbeat on beat 3, open-hat quarters, held power chord.
inline constexpr StyleEvent kVarCDrums[] = {
    {.step = 0, .tone = kCrash, .octave = 0, .vel = 110, .gate = kGateBeat},
    {.step = 0, .tone = kKick, .octave = 0, .vel = 120, .gate = kGateHat},
    {.step = 4, .tone = kKick, .octave = 0, .vel = 100, .gate = kGateHat},
    {.step = 8, .tone = kKick, .octave = 0, .vel = 116, .gate = kGateHat},
    {.step = 8, .tone = kSnare, .octave = 0, .vel = 118, .gate = kGateHat},
    {.step = 0, .tone = kOpenHat, .octave = 0, .vel = 90, .gate = kGate8th},
    {.step = 4, .tone = kOpenHat, .octave = 0, .vel = 86, .gate = kGate8th},
    {.step = 8, .tone = kOpenHat, .octave = 0, .vel = 90, .gate = kGate8th},
    {.step = 12, .tone = kOpenHat, .octave = 0, .vel = 86, .gate = kGate8th}};
inline constexpr StyleEvent kVarCBass[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 116, .gate = kGate8th},
    {.step = 4, .tone = kRoot, .octave = 1, .vel = 96, .gate = kGateHat},
    {.step = 8, .tone = kFifth, .octave = 0, .vel = 112, .gate = kGate8th},
    {.step = 12, .tone = kRoot, .octave = 1, .vel = 96, .gate = kGateHat}};
inline constexpr StyleEvent kVarCChord[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 110, .gate = kGateHalfBar},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 110, .gate = kGateHalfBar},
    {.step = 8, .tone = kRoot, .octave = 0, .vel = 108, .gate = kGateHalfBar},
    {.step = 8, .tone = kFifth, .octave = 0, .vel = 108, .gate = kGateHalfBar}};
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
                                                  .events = Span<const StyleEvent>(kChord2HalfBar),
                                                  .gm_program = kChord2Voice},
                                                 {.role = TrackRole::kArp,
                                                  .policy = RolePolicy::kChordTone,
                                                  .events = Span<const StyleEvent>(kArp8),
                                                  .gm_program = kArpVoice}};
// varD stays 1 bar (style-depth Wave-2 C): the peak already carries the most
// per-repeat motion of any section (drums/chord1/pad/chord2/lead all
// motif-wired); a literal 2nd bar would need touching those same
// motif-locked arrays this pass deliberately leaves alone (see the VarA
// Chord2 note above), so the arc stays call-and-response, not a longer loop.
// varD: peak — four-on-the-floor eighth kick, crash, open-hat accents, power+7th stabs.
inline constexpr StyleEvent kVarDDrums[] = {
    {.step = 0, .tone = kCrash, .octave = 0, .vel = 118, .gate = kGateBeat},
    {.step = 0, .tone = kKick, .octave = 0, .vel = 120, .gate = kGateHat},
    {.step = 2, .tone = kKick, .octave = 0, .vel = 100, .gate = kGateHat},
    {.step = 4, .tone = kKick, .octave = 0, .vel = 110, .gate = kGateHat},
    {.step = 6, .tone = kKick, .octave = 0, .vel = 100, .gate = kGateHat},
    {.step = 8, .tone = kKick, .octave = 0, .vel = 116, .gate = kGateHat},
    {.step = 10, .tone = kKick, .octave = 0, .vel = 100, .gate = kGateHat},
    {.step = 12, .tone = kKick, .octave = 0, .vel = 110, .gate = kGateHat},
    {.step = 14, .tone = kKick, .octave = 0, .vel = 100, .gate = kGateHat},
    {.step = 4, .tone = kSnare, .octave = 0, .vel = 118, .gate = kGateHat},
    {.step = 12, .tone = kSnare, .octave = 0, .vel = 118, .gate = kGateHat},
    {.step = 2, .tone = kOpenHat, .octave = 0, .vel = 92, .gate = kGate8th},
    {.step = 6, .tone = kOpenHat, .octave = 0, .vel = 92, .gate = kGate8th},
    {.step = 10, .tone = kOpenHat, .octave = 0, .vel = 92, .gate = kGate8th},
    {.step = 14, .tone = kOpenHat, .octave = 0, .vel = 92, .gate = kGate8th}};
inline constexpr StyleEvent kVarDBass[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 120, .gate = kGate8th},
    {.step = 2, .tone = kRoot, .octave = 1, .vel = 100, .gate = kGateHat},
    {.step = 4, .tone = kFifth, .octave = 0, .vel = 112, .gate = kGate8th},
    {.step = 6, .tone = kRoot, .octave = 1, .vel = 100, .gate = kGateHat},
    {.step = 8, .tone = kRoot, .octave = 0, .vel = 116, .gate = kGate8th},
    {.step = 10, .tone = kSeventh, .octave = 0, .vel = 100, .gate = kGateHat},
    {.step = 12, .tone = kFifth, .octave = 0, .vel = 112, .gate = kGate8th},
    {.step = 14, .tone = kRoot, .octave = 1, .vel = 100, .gate = kGateHat}};
inline constexpr StyleEvent kVarDChord[] = {{.step = 0,
                                             .tone = kRoot,
                                             .octave = 0,
                                             .vel = 114,
                                             .gate = kGateBeat,
                                             .gesture = ChordGesture::kStrumDown},
                                            {.step = 8,
                                             .tone = kRoot,
                                             .octave = 0,
                                             .vel = 112,
                                             .gate = kGateBeat,
                                             .gesture = ChordGesture::kStrumDown},
                                            {.step = 12,
                                             .tone = kRoot,
                                             .octave = 0,
                                             .vel = 108,
                                             .gate = kGateStab,
                                             .gesture = ChordGesture::kStrumDown}};
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
                                                  .events = Span<const StyleEvent>(kChord2Off),
                                                  .gm_program = kChord2Voice,
                                                  .motif = &kSharedChord2Motif},
                                                 {.role = TrackRole::kArp,
                                                  .policy = RolePolicy::kChordTone,
                                                  .events = Span<const StyleEvent>(kArp8),
                                                  .gm_program = kArpVoice},
                                                 {.role = TrackRole::kLead,
                                                  .policy = RolePolicy::kChordTone,
                                                  .events = Span<const StyleEvent>(kLeadLick),
                                                  .gm_program = kLeadVoice,
                                                  .motif = &kLeadLickMotif},
                                                 {.role = TrackRole::kPerc,
                                                  .policy = RolePolicy::kFixed,
                                                  .events = Span<const StyleEvent>(kPercCowbell)}};

// varBreak: the rock stop — one accented downbeat (kick+crash, a bass pop, a
// power-chord stab held as a literal synchronized block), then wide silence,
// then a snare/tom pickup on beat 4 that throws the groove back in.
inline constexpr StyleEvent kBrkD[] = {
    {.step = 0, .tone = kKick, .octave = 0, .vel = 120, .gate = kGateHat},
    {.step = 0, .tone = kCrash, .octave = 0, .vel = 116, .gate = kGateStab},
    {.step = 12, .tone = kSnare, .octave = 0, .vel = 104, .gate = 100},
    {.step = 13, .tone = kSnare, .octave = 0, .vel = 110, .gate = 100},
    {.step = 14, .tone = kTomMid, .octave = 0, .vel = 114, .gate = 100},
    {.step = 15, .tone = kTomLow, .octave = 0, .vel = 118, .gate = 100},
};
inline constexpr StyleEvent kBrkB[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 118, .gate = kGateStab}};
inline constexpr StyleEvent kBrkC[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 112, .gate = kGateStab},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 112, .gate = kGateStab},
    {.step = 0, .tone = kRoot, .octave = 1, .vel = 112, .gate = kGateStab},
};
inline constexpr StylePattern kBrkP[] = {
    {.role = TrackRole::kDrums,
     .policy = RolePolicy::kFixed,
     .events = Span<const StyleEvent>(kBrkD)},
    {.role = TrackRole::kBass,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kBrkB)},
    {.role = TrackRole::kChord1,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kBrkC)},
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
    {.type = SectionType::kBreak, .bars = 1, .patterns = Span<const StylePattern>(kBrkP)},
    {.type = SectionType::kEnding1, .bars = 2, .patterns = Span<const StylePattern>(kEndPatterns)},
    {.type = SectionType::kEnding2, .bars = 2, .patterns = Span<const StylePattern>(kEnd2Patterns)},
};
// Groove (9100 Wave 1, task B): driving, heavier live-band genre -- a
// moderate deterministic humanize, a touch more than the mainstream tier.
inline constexpr Style kStyle{.name = "rock",
                              .sections = Span<const StyleSection>(kSections),
                              .groove = {.humanize_timing = 8, .humanize_velocity = 18},
                              .tempo = 13000};

}  // namespace rock

}  // namespace styles
}  // namespace arrangrr
