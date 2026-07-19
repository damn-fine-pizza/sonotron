#pragma once

#include "arrangrr/arranger/style_model.hpp"

namespace arrangrr {
namespace styles {

// ---------------------------------------------------------------------------
// "ballad": slow, sparse, gentle. Soft kick with a rimshot backbeat and an
// occasional ride, the chord spread as a rising arpeggio (tones 0..3 across the
// bar) instead of a block stab, a soft legato root bass. Low velocity, long
// gates.
//
// Voice-leading (D41) reference style: the block-chord comping roles — the
// held strings pad and the E.Piano second comp — carry VoicingPolicy::kLead,
// so their triad/7th re-octaves to the nearest register of the previous chord
// (common tones held, other voices move minimally) as the harmony changes. The
// convention: kLead belongs on roles that sound SEVERAL chord tones together at
// a step (pad / chord2 blocks); the rising arpeggio (kChord1), the harp arp and
// the bass are single-note-per-step lines whose intended shape kLead would
// flatten, so they stay kAsWritten.
namespace ballad {

// ---------------------------------------------------------------------------
// Fuller-band roles (ADDITIVE): a soft strings pad under the whole bar, a
// gentle E.Piano second comp, a harp arpeggio for sparkle and a whisper of
// percussion. Everything stays low-velocity, long-gated — the ballad must not
// get busy. kPad -> GM 48 (Strings), kChord2 -> GM 4 (E.Piano1), kArp -> GM 46
// (Harp); kPerc rides the drum channel.
inline constexpr std::int16_t kPadVoice = 48;
inline constexpr std::int16_t kChord2Voice = 4;
inline constexpr std::int16_t kArpVoice = 46;
// Motif engine (9210, Option 2 / generated seed, node 9210 owner sign-off):
// ballad's kLead model gap closed by generation -- "a ballad style with no
// vocal-style line at all" was the most conspicuous corpus gap. Warm,
// singing 3rd-centered contour on a wordless vocal voice; length 3 combined
// with kGateHeld overlaps each onset's tail with the next by design, a
// single continuous legato line rather than three separate stabs. Draws its
// onset candidates from kVarDDrums' own even-16th kick/snare/ride mask.
inline constexpr std::int16_t kLeadVoice = 53;  // Voice Oohs
inline constexpr MotifSpec kLeadMotif{
    .transform = MotifTransform::kDiatonicTranspose,
    .seed = 1503,
    .length = 3,
    .center_degree = 2,
    .vel = 62,
    .gate = kGateHeld,
    .idiom_role = TrackRole::kDrums,
};
// Re-articulated at step 16 too (style-depth Wave-2 C): Intro1 and VarA are
// now 2 bars; the sustain re-triggers every bar exactly as it always has when
// the section looped -- a background hold, not the section's own bar-to-bar
// variation. VarB/Intro2 stay 1 bar and never read past step 15, so this tail
// is invisible to them (kPadTriad carries no MotifSpec in ballad in any of
// its usages, so extending past step 15 is safe here).
inline constexpr StyleEvent kPadTriad[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 50, .gate = kGateHeld},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 48, .gate = kGateHeld},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 50, .gate = kGateHeld},
    {.step = 16, .tone = kRoot, .octave = 0, .vel = 50, .gate = kGateHeld},
    {.step = 16, .tone = kThird, .octave = 0, .vel = 48, .gate = kGateHeld},
    {.step = 16, .tone = kFifth, .octave = 0, .vel = 50, .gate = kGateHeld},
};
inline constexpr StyleEvent kPad7[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 52, .gate = kGateHeld},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 50, .gate = kGateHeld},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 52, .gate = kGateHeld},
    {.step = 0, .tone = kSeventh, .octave = 0, .vel = 46, .gate = kGateHeld},
    // bar 2 (ending1 only, Part A, ballad delta): a harp-like roll-up spread
    // instead of a block hit -- the family's one intentional exception to
    // "hit on the downbeat". ONE gesture event, since gesture::expand fans
    // the whole live chord regardless of authored tone.
    {.step = 16,
     .tone = kRoot,
     .octave = 0,
     .vel = 52,
     .gate = kGateHeld,
     .gesture = ChordGesture::kRollUp},
};
// Ending2-only fork of kPad7 (Part A): ending1 restates the held chord at
// step 16, ending2 stays silent there and only sounds under the big final
// hit at step 24 -- the two endings diverge, so this duplicates kPad7's
// original bar-1 content verbatim and adds its own distinct bar-2 tail.
inline constexpr StyleEvent kPad7Ending2[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 52, .gate = kGateHeld},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 50, .gate = kGateHeld},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 52, .gate = kGateHeld},
    {.step = 0, .tone = kSeventh, .octave = 0, .vel = 46, .gate = kGateHeld},
    // bar 2 (ending2 only, ballad delta): silent at step 16, a harp-like
    // roll-up spread restating the chord under the big final hit at step 24.
    {.step = 24,
     .tone = kRoot,
     .octave = 0,
     .vel = 52,
     .gate = kGateHeld,
     .gesture = ChordGesture::kRollUp},
};
// E.Piano: a soft block on beat 1, or beats 1 & 3 in the fuller variations.
inline constexpr StyleEvent kChord2Beat1[] = {
    {.step = 0, .tone = kThird, .octave = 0, .vel = 52, .gate = kGateHalfBar},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 52, .gate = kGateHalfBar},
};
inline constexpr StyleEvent kChord2Beat13[] = {
    {.step = 0, .tone = kThird, .octave = 0, .vel = 54, .gate = kGateBeat},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 54, .gate = kGateBeat},
    {.step = 8, .tone = kThird, .octave = 0, .vel = 50, .gate = kGateBeat},
    {.step = 8, .tone = kFifth, .octave = 0, .vel = 50, .gate = kGateBeat},
};
// Harp: a gentle rising arpeggio, high register.
inline constexpr StyleEvent kArpHarp[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 58, .gate = kGate8th},
    {.step = 2, .tone = kThird, .octave = 0, .vel = 52, .gate = kGate8th},
    {.step = 4, .tone = kFifth, .octave = 0, .vel = 56, .gate = kGate8th},
    {.step = 6, .tone = kSeventh, .octave = 0, .vel = 52, .gate = kGate8th},
    {.step = 8, .tone = kRoot, .octave = 1, .vel = 58, .gate = kGate8th},
    {.step = 10, .tone = kSeventh, .octave = 0, .vel = 52, .gate = kGate8th},
    {.step = 12, .tone = kFifth, .octave = 0, .vel = 56, .gate = kGate8th},
    {.step = 14, .tone = kThird, .octave = 0, .vel = 50, .gate = kGate8th},
};
// Motif engine (9210, Ottorino RANK 15): the harp/pad texture, reused
// identically across its two instances. kDisplacement — a soft, evolving
// push suits a harp/pad texture better than a hard mirrored flip. ballad's
// own melody is a MODEL GAP (the most conspicuous in the corpus: a ballad
// with no vocal-style line at all), which needs Option 2 generation, a
// separate later batch.
inline constexpr MotifSpec kArpHarpMotif{.transform = MotifTransform::kDisplacement, .seed = 1502};
// The faintest shaker/tambourine — only where the fuller sections can carry it.
inline constexpr StyleEvent kPercSoft[] = {
    {.step = 4, .tone = kTambourine, .octave = 0, .vel = 54, .gate = kGateHat},
    {.step = 12, .tone = kTambourine, .octave = 0, .vel = 56, .gate = kGateHat},
};
inline constexpr StyleEvent kPercShake[] = {
    {.step = 2, .tone = kMaracas, .octave = 0, .vel = 46, .gate = kGateHat},
    {.step = 6, .tone = kMaracas, .octave = 0, .vel = 46, .gate = kGateHat},
    {.step = 10, .tone = kMaracas, .octave = 0, .vel = 46, .gate = kGateHat},
    {.step = 14, .tone = kMaracas, .octave = 0, .vel = 46, .gate = kGateHat},
};

// Motif engine (9210 Wave 1, style-depth program task A): extend variation
// onto drums/chord1/chord2/pad, wired onto the peak variation (varD) plus the
// shared reused Pad/Chord2 arrays, same granularity as the bass/arp/lead
// wiring above. kDisplacement for drums/pad/chord2 (a soft evolving push);
// kRetrograde for the chord1 comp (a legible mirrored answer).
inline constexpr MotifSpec kPeakDrumsMotif{.transform = MotifTransform::kDisplacement,
                                           .seed = 1510};
inline constexpr MotifSpec kPeakChordMotif{.transform = MotifTransform::kRetrograde, .seed = 1511};
inline constexpr MotifSpec kSharedPadMotif{.transform = MotifTransform::kDisplacement,
                                           .seed = 1512};
inline constexpr MotifSpec kSharedChord2Motif{.transform = MotifTransform::kDisplacement,
                                              .seed = 1513};

inline constexpr StyleEvent kVarADrums[] = {
    {.step = 0, .tone = kKick, .octave = 0, .vel = 72, .gate = kGateHat},
    {.step = 4, .tone = kRimshot, .octave = 0, .vel = 60, .gate = kGateHat},
    {.step = 12, .tone = kRimshot, .octave = 0, .vel = 62, .gate = kGateHat},
    {.step = 0, .tone = kRide, .octave = 0, .vel = 58, .gate = kGate8th},
    {.step = 8, .tone = kRide, .octave = 0, .vel = 56, .gate = kGate8th},
    // bar 2 (style-depth Wave-2 C): a genuine turnaround -- the rimshot
    // answers off the beat (the & of 2 and the & of 4) instead of squarely
    // on 2 and 4.
    {.step = 16, .tone = kKick, .octave = 0, .vel = 72, .gate = kGateHat},
    {.step = 22, .tone = kRimshot, .octave = 0, .vel = 62, .gate = kGateHat},
    {.step = 30, .tone = kRimshot, .octave = 0, .vel = 64, .gate = kGateHat},
    {.step = 16, .tone = kRide, .octave = 0, .vel = 58, .gate = kGate8th},
    {.step = 24, .tone = kRide, .octave = 0, .vel = 58, .gate = kGate8th},
};
inline constexpr StyleEvent kVarABass[] = {
    // Legato root under the whole bar.
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 74, .gate = kGateHeld},
    // bar 2 (style-depth Wave-2 C): the hold continues (background sustain,
    // not a new idea -- the genuine bar-2 variation lives in drums/chord1).
    {.step = 16, .tone = kRoot, .octave = 0, .vel = 74, .gate = kGateHeld},
};
inline constexpr StyleEvent kVarAChord[] = {
    // Rising arpeggio: one chord tone per beat, each sustaining so they stack.
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 70, .gate = kGateHeld},
    {.step = 4, .tone = kThird, .octave = 0, .vel = 68, .gate = kGateHalfBar},
    {.step = 8, .tone = kFifth, .octave = 0, .vel = 72, .gate = kGateHalfBar},
    {.step = 12, .tone = kSeventh, .octave = 0, .vel = 66, .gate = kGateHalfBar},
    // bar 2 (style-depth Wave-2 C): a genuine turnaround -- the arpeggio
    // falls back down instead of repeating the rise, resolving on the held
    // root right where the repeat begins.
    {.step = 16, .tone = kSeventh, .octave = 0, .vel = 64, .gate = kGateHalfBar},
    {.step = 20, .tone = kFifth, .octave = 0, .vel = 66, .gate = kGateHalfBar},
    {.step = 24, .tone = kThird, .octave = 0, .vel = 64, .gate = kGateHalfBar},
    {.step = 28, .tone = kRoot, .octave = 0, .vel = 68, .gate = kGateHeld},
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
    {.role = TrackRole::kChord2,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kChord2Beat1),
     .gm_program = kChord2Voice,
     .voicing = VoicingPolicy::kLead},
};

// VarB stays 1 bar (style-depth Wave-2 C): no genuine bar-2 idea beyond
// what VarA's turnaround already carries; a second bar here would just
// repeat the denser comping, not vary it.
inline constexpr StyleEvent kVarBDrums[] = {
    {.step = 0, .tone = kKick, .octave = 0, .vel = 76, .gate = kGateHat},
    {.step = 8, .tone = kKick, .octave = 0, .vel = 70, .gate = kGateHat},
    {.step = 4, .tone = kRimshot, .octave = 0, .vel = 64, .gate = kGateHat},
    {.step = 12, .tone = kRimshot, .octave = 0, .vel = 66, .gate = kGateHat},
    {.step = 0, .tone = kRide, .octave = 0, .vel = 60, .gate = kGate8th},
    {.step = 4, .tone = kRide, .octave = 0, .vel = 54, .gate = kGate8th},
    {.step = 8, .tone = kRide, .octave = 0, .vel = 60, .gate = kGate8th},
    {.step = 12, .tone = kRide, .octave = 0, .vel = 54, .gate = kGate8th},
};
inline constexpr StyleEvent kVarBBass[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 76, .gate = kGateHalfBar},
    {.step = 8, .tone = kFifth, .octave = 0, .vel = 72, .gate = kGateHalfBar},
};
inline constexpr StyleEvent kVarBChord[] = {
    // Denser arpeggio on the 8ths, still sustaining into a spread chord.
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 74, .gate = kGateHeld},
    {.step = 2, .tone = kThird, .octave = 0, .vel = 66, .gate = kGateHalfBar},
    {.step = 4, .tone = kFifth, .octave = 0, .vel = 72, .gate = kGateHalfBar},
    {.step = 6, .tone = kSeventh, .octave = 0, .vel = 64, .gate = kGateHalfBar},
    {.step = 8, .tone = kThird, .octave = 1, .vel = 70, .gate = kGateHalfBar},
    {.step = 12, .tone = kFifth, .octave = 0, .vel = 66, .gate = kGateHalfBar},
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
     .events = Span<const StyleEvent>(kChord2Beat13),
     .gm_program = kChord2Voice,
     .voicing = VoicingPolicy::kLead,
     .motif = &kSharedChord2Motif},
};

// Intro1 (style-depth Wave-2 C): a genuine 2-bar build. Bar 1 (unchanged) is
// the bare ride pickup; bar 2 is the arrival -- the rimshot backbeat enters
// under the continuing ride, previewing VarA's kick/rimshot/ride texture
// gently rather than dropping it in cold.
inline constexpr StyleEvent kIntroDrums[] = {
    {.step = 0, .tone = kRide, .octave = 0, .vel = 52, .gate = kGate8th},
    {.step = 8, .tone = kRide, .octave = 0, .vel = 58, .gate = kGate8th},
    // bar 2: arrival -- rimshot backbeat enters under the continuing ride.
    {.step = 16, .tone = kRide, .octave = 0, .vel = 54, .gate = kGate8th},
    {.step = 24, .tone = kRide, .octave = 0, .vel = 60, .gate = kGate8th},
    {.step = 20, .tone = kRimshot, .octave = 0, .vel = 56, .gate = kGateHat},
    {.step = 28, .tone = kRimshot, .octave = 0, .vel = 60, .gate = kGateHat},
};
inline constexpr StyleEvent kIntroBass[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 70, .gate = kGateHeld},
    // bar 2: hold continues (background sustain, not a new idea), then a
    // soft fifth anticipation leads into VarA.
    {.step = 16, .tone = kRoot, .octave = 0, .vel = 72, .gate = kGateHalfBar},
    {.step = 30, .tone = kFifth, .octave = 0, .vel = 62, .gate = kGate8th},
};
inline constexpr StyleEvent kIntroChord[] = {
    {.step = 8, .tone = kRoot, .octave = 0, .vel = 62, .gate = kGateHalfBar},
    {.step = 12, .tone = kThird, .octave = 0, .vel = 64, .gate = kGateHalfBar},
    // bar 2: arrival -- the pickup resolves into a fuller stab.
    {.step = 16, .tone = kRoot, .octave = 0, .vel = 66, .gate = kGateHalfBar},
    {.step = 16, .tone = kThird, .octave = 0, .vel = 64, .gate = kGateHalfBar},
    {.step = 24, .tone = kFifth, .octave = 0, .vel = 64, .gate = kGateHalfBar},
    {.step = 24, .tone = kSeventh, .octave = 0, .vel = 62, .gate = kGateHalfBar},
};
inline constexpr StylePattern kIntroPatterns[] = {
    {.role = TrackRole::kDrums,
     .policy = RolePolicy::kFixed,
     .events = Span<const StyleEvent>(kIntroDrums)},
    {.role = TrackRole::kBass,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kIntroBass)},
    {.role = TrackRole::kChord1,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kIntroChord)},
    {.role = TrackRole::kPad,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kPadTriad),
     .gm_program = kPadVoice,
     .voicing = VoicingPolicy::kLead},
};

inline constexpr StyleEvent kFillDrums[] = {
    {.step = 0, .tone = kKick, .octave = 0, .vel = 74, .gate = kGateHat},
    {.step = 8, .tone = kRimshot, .octave = 0, .vel = 66, .gate = 100},
    {.step = 10, .tone = kRimshot, .octave = 0, .vel = 72, .gate = 100},
    {.step = 12, .tone = kSnare, .octave = 0, .vel = 78, .gate = 100},
    {.step = 14, .tone = kSnare, .octave = 0, .vel = 84, .gate = 100},
};
inline constexpr StyleEvent kFillBass[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 74, .gate = kGateHeld},
};
// The flat FILL bass, reused identically across all 4 fills; the Var bass
// (kVarABass..kVarDBass) is already 4 distinct hand-authored ideas — the
// mildest bass redundancy in the corpus.
inline constexpr MotifSpec kFillBassMotif{.transform = MotifTransform::kRetrograde, .seed = 1501};
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
    {.step = 0, .tone = kCrash, .octave = 0, .vel = 80, .gate = kGateHeld},
    {.step = 0, .tone = kKick, .octave = 0, .vel = 78, .gate = kGateHat},
    // bar 2 (Part A, ballad delta): a soft crash swell only -- no kick,
    // letting the chord ring rather than adding a hit for "finality".
    {.step = 16, .tone = kCrash, .octave = 0, .vel = 76, .gate = kGateHeld},
};
inline constexpr StyleEvent kEndBass[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 76, .gate = kGateHeld},
    {.step = 16, .tone = kRoot, .octave = 0, .vel = 76, .gate = kGateHeld},
};
inline constexpr StyleEvent kEndChord[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 72, .gate = kGateHeld},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 72, .gate = kGateHeld},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 72, .gate = kGateHeld},
    {.step = 0, .tone = kSeventh, .octave = 0, .vel = 72, .gate = kGateHeld},
    // bar 2 (Part A, ballad delta): no sus4 pickup (sparsest ending in the
    // family) -- the triad simply resolves into bar 2.
    {.step = 16, .tone = kThird, .octave = 0, .vel = 66, .gate = kGateHeld},
    {.step = 16, .tone = kRoot, .octave = 0, .vel = 66, .gate = kGateHeld},
    {.step = 16, .tone = kFifth, .octave = 0, .vel = 66, .gate = kGateHeld},
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
    {.step = 0, .tone = kRide, .octave = 0, .vel = 54, .gate = kGate8th},
    {.step = 4, .tone = kRide, .octave = 0, .vel = 48, .gate = kGate8th},
    {.step = 8, .tone = kRide, .octave = 0, .vel = 56, .gate = kGate8th},
    {.step = 12, .tone = kRide, .octave = 0, .vel = 50, .gate = kGate8th},
    {.step = 0, .tone = kKick, .octave = 0, .vel = 66, .gate = kGateHat},
    {.step = 12, .tone = kRimshot, .octave = 0, .vel = 58, .gate = kGateHat},
};
inline constexpr StyleEvent kIntro2Chord[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 62, .gate = kGateHeld},
    {.step = 4, .tone = kThird, .octave = 0, .vel = 60, .gate = kGateHalfBar},
    {.step = 8, .tone = kFifth, .octave = 0, .vel = 64, .gate = kGateHalfBar},
    {.step = 12, .tone = kSeventh, .octave = 0, .vel = 58, .gate = kGateHalfBar},
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
    {.step = 0, .tone = kKick, .octave = 0, .vel = 72, .gate = kGateHat},
    {.step = 8, .tone = kRimshot, .octave = 0, .vel = 64, .gate = 120},
    {.step = 10, .tone = kRimshot, .octave = 0, .vel = 70, .gate = 120},
    {.step = 12, .tone = kTomMid, .octave = 0, .vel = 74, .gate = 120},
    {.step = 14, .tone = kTomLow, .octave = 0, .vel = 80, .gate = 120},
};
inline constexpr StyleEvent kFillCDrums[] = {
    {.step = 0, .tone = kTomHi, .octave = 0, .vel = 68, .gate = 100},
    {.step = 4, .tone = kTomHi, .octave = 0, .vel = 70, .gate = 100},
    {.step = 6, .tone = kTomMid, .octave = 0, .vel = 74, .gate = 100},
    {.step = 8, .tone = kTomMid, .octave = 0, .vel = 76, .gate = 100},
    {.step = 10, .tone = kTomLow, .octave = 0, .vel = 80, .gate = 100},
    {.step = 12, .tone = kSnare, .octave = 0, .vel = 82, .gate = 100},
    {.step = 14, .tone = kSnare, .octave = 0, .vel = 88, .gate = 100},
};
inline constexpr StyleEvent kFillDDrums[] = {
    {.step = 0, .tone = kTomHi, .octave = 0, .vel = 70, .gate = 100},
    {.step = 2, .tone = kTomHi, .octave = 0, .vel = 66, .gate = 100},
    {.step = 4, .tone = kTomMid, .octave = 0, .vel = 74, .gate = 100},
    {.step = 6, .tone = kTomMid, .octave = 0, .vel = 70, .gate = 100},
    {.step = 8, .tone = kTomLow, .octave = 0, .vel = 80, .gate = 100},
    {.step = 10, .tone = kTomLow, .octave = 0, .vel = 76, .gate = 100},
    {.step = 12, .tone = kTomFloor, .octave = 0, .vel = 86, .gate = 100},
    {.step = 13, .tone = kSnare, .octave = 0, .vel = 90, .gate = 100},
    {.step = 14, .tone = kSnare, .octave = 0, .vel = 96, .gate = 100},
    {.step = 15, .tone = kSnare, .octave = 0, .vel = 100, .gate = 100},
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
};

inline constexpr StyleEvent kEnd2Drums[] = {
    {.step = 0, .tone = kCrash, .octave = 0, .vel = 82, .gate = kGateHeld},
    {.step = 0, .tone = kKick, .octave = 0, .vel = 80, .gate = kGateHat},
    {.step = 8, .tone = kRide, .octave = 0, .vel = 56, .gate = kGate8th},
    // bar 2 (Part A, ballad delta): a soft crash swell only under the big
    // final hit -- no kick, matching ending1's "no finality kick" choice.
    {.step = 24, .tone = kCrash, .octave = 0, .vel = 76, .gate = kGateHeld},
};
inline constexpr StyleEvent kEnd2Bass[] = {
    {.step = 0, .tone = kRoot, .octave = -1, .vel = 78, .gate = kGateHeld},
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 70, .gate = kGateHeld},
    {.step = 16, .tone = kRoot, .octave = 0, .vel = 70, .gate = kGateBeat},
    {.step = 24, .tone = kRoot, .octave = -1, .vel = 78, .gate = kGateHeld},
    {.step = 24, .tone = kRoot, .octave = 0, .vel = 70, .gate = kGateHeld},
};
inline constexpr StyleEvent kEnd2Chord[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 72, .gate = kGateHeld},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 72, .gate = kGateHeld},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 72, .gate = kGateHeld},
    {.step = 0, .tone = kSeventh, .octave = 0, .vel = 72, .gate = kGateHeld},
    {.step = 0, .tone = kThird, .octave = 1, .vel = 68, .gate = kGateHeld},
    // bar 2 (Part A, "Amen tag"): a short 6th/4th color tag on 1, then the
    // big final chord on beat 3 of bar 2 -- NOT the downbeat -- root doubled
    // an octave up. Chord1 stays literal (no gesture); the delta's roll-up
    // spread is reserved for the Pad's own restatement above.
    {.step = 16,
     .tone = 9,
     .octave = 0,
     .vel = 60,
     .gate = kGateBeat,
     .src = NoteSource::kInterval},
    {.step = 16,
     .tone = 5,
     .octave = 0,
     .vel = 58,
     .gate = kGateBeat,
     .src = NoteSource::kInterval},
    {.step = 24, .tone = kRoot, .octave = 0, .vel = 72, .gate = kGateHeld},
    {.step = 24, .tone = kThird, .octave = 0, .vel = 72, .gate = kGateHeld},
    {.step = 24, .tone = kFifth, .octave = 0, .vel = 72, .gate = kGateHeld},
    {.step = 24, .tone = kRoot, .octave = 1, .vel = 68, .gate = kGateHeld},
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

// varC: flowing 6/8-lilt broken arpeggio — a rolling feel vs A/B's long sustains.
// VarC stays 1 bar (style-depth Wave-2 C): the 6/8 lilt is itself the
// section's variation; a bar-2 repeat would add length, not a new idea.
inline constexpr StyleEvent kVarCDrums[] = {
    {.step = 0, .tone = kKick, .octave = 0, .vel = 72, .gate = kGateHat},
    {.step = 8, .tone = kKick, .octave = 0, .vel = 66, .gate = kGateHat},
    {.step = 4, .tone = kRimshot, .octave = 0, .vel = 60, .gate = kGateHat},
    {.step = 12, .tone = kRimshot, .octave = 0, .vel = 62, .gate = kGateHat},
    {.step = 0, .tone = kRide, .octave = 0, .vel = 58, .gate = kGate8th},
    {.step = 2, .tone = kRide, .octave = 0, .vel = 50, .gate = kGate8th},
    {.step = 4, .tone = kRide, .octave = 0, .vel = 56, .gate = kGate8th},
    {.step = 6, .tone = kRide, .octave = 0, .vel = 50, .gate = kGate8th},
    {.step = 8, .tone = kRide, .octave = 0, .vel = 58, .gate = kGate8th},
    {.step = 10, .tone = kRide, .octave = 0, .vel = 50, .gate = kGate8th},
    {.step = 12, .tone = kRide, .octave = 0, .vel = 56, .gate = kGate8th},
    {.step = 14, .tone = kRide, .octave = 0, .vel = 50, .gate = kGate8th}};
inline constexpr StyleEvent kVarCBass[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 74, .gate = kGateHalfBar},
    {.step = 8, .tone = kFifth, .octave = -1, .vel = 70, .gate = kGateHalfBar}};
inline constexpr StyleEvent kVarCChord[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 66, .gate = kGate8th},
    {.step = 2, .tone = kThird, .octave = 0, .vel = 62, .gate = kGate8th},
    {.step = 4, .tone = kFifth, .octave = 0, .vel = 66, .gate = kGate8th},
    {.step = 6, .tone = kSeventh, .octave = 0, .vel = 62, .gate = kGate8th},
    {.step = 8, .tone = kRoot, .octave = 1, .vel = 66, .gate = kGate8th},
    {.step = 10, .tone = kThird, .octave = 1, .vel = 62, .gate = kGate8th},
    {.step = 12, .tone = kFifth, .octave = 0, .vel = 64, .gate = kGate8th},
    {.step = 14, .tone = kSeventh, .octave = 0, .vel = 60, .gate = kGate8th}};
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
                                                  .events = Span<const StyleEvent>(kPad7),
                                                  .gm_program = kPadVoice,
                                                  .voicing = VoicingPolicy::kLead,
                                                  .motif = &kSharedPadMotif},
                                                 {.role = TrackRole::kChord2,
                                                  .policy = RolePolicy::kChordTone,
                                                  .events = Span<const StyleEvent>(kChord2Beat13),
                                                  .gm_program = kChord2Voice,
                                                  .voicing = VoicingPolicy::kLead,
                                                  .motif = &kSharedChord2Motif},
                                                 {.role = TrackRole::kArp,
                                                  .policy = RolePolicy::kChordTone,
                                                  .events = Span<const StyleEvent>(kArpHarp),
                                                  .gm_program = kArpVoice,
                                                  .motif = &kArpHarpMotif},
                                                 {.role = TrackRole::kPerc,
                                                  .policy = RolePolicy::kFixed,
                                                  .events = Span<const StyleEvent>(kPercShake)}};
// varD: fullest — snare backbeat, ride eighths, gentle walking bass, full spread 7th chord.
// VarD stays 1 bar (style-depth Wave-2 C): already the peak/fullest section;
// a bar-2 arc would blur into Ending2's own cadence rather than add a
// genuine idea.
inline constexpr StyleEvent kVarDDrums[] = {
    {.step = 0, .tone = kKick, .octave = 0, .vel = 80, .gate = kGateHat},
    {.step = 8, .tone = kKick, .octave = 0, .vel = 76, .gate = kGateHat},
    {.step = 4, .tone = kSnare, .octave = 0, .vel = 84, .gate = kGateHat},
    {.step = 12, .tone = kSnare, .octave = 0, .vel = 86, .gate = kGateHat},
    {.step = 0, .tone = kRide, .octave = 0, .vel = 64, .gate = kGate8th},
    {.step = 2, .tone = kRide, .octave = 0, .vel = 56, .gate = kGate8th},
    {.step = 4, .tone = kRide, .octave = 0, .vel = 64, .gate = kGate8th},
    {.step = 6, .tone = kRide, .octave = 0, .vel = 56, .gate = kGate8th},
    {.step = 8, .tone = kRide, .octave = 0, .vel = 64, .gate = kGate8th},
    {.step = 10, .tone = kRide, .octave = 0, .vel = 56, .gate = kGate8th},
    {.step = 12, .tone = kRide, .octave = 0, .vel = 64, .gate = kGate8th},
    {.step = 14, .tone = kRide, .octave = 0, .vel = 56, .gate = kGate8th}};
inline constexpr StyleEvent kVarDBass[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 80, .gate = kGateBeat},
    {.step = 4, .tone = kThird, .octave = 0, .vel = 74, .gate = kGateBeat},
    {.step = 8, .tone = kFifth, .octave = 0, .vel = 78, .gate = kGateBeat},
    {.step = 12, .tone = kSeventh, .octave = 0, .vel = 74, .gate = kGateBeat}};
inline constexpr StyleEvent kVarDChord[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 74, .gate = kGateHeld},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 72, .gate = kGateHeld},
    {.step = 4, .tone = kFifth, .octave = 0, .vel = 72, .gate = kGateHalfBar},
    {.step = 6, .tone = kSeventh, .octave = 0, .vel = 66, .gate = kGateHalfBar},
    {.step = 8, .tone = kThird, .octave = 1, .vel = 70, .gate = kGateHalfBar},
    {.step = 8, .tone = kFifth, .octave = 0, .vel = 68, .gate = kGateHalfBar},
    {.step = 12, .tone = kSeventh, .octave = 0, .vel = 66, .gate = kGateHalfBar},
    {.step = 12, .tone = kRoot, .octave = 1, .vel = 64, .gate = kGateHalfBar}};
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
                                                  .events = Span<const StyleEvent>(kPad7),
                                                  .gm_program = kPadVoice,
                                                  .voicing = VoicingPolicy::kLead,
                                                  .motif = &kSharedPadMotif},
                                                 {.role = TrackRole::kChord2,
                                                  .policy = RolePolicy::kChordTone,
                                                  .events = Span<const StyleEvent>(kChord2Beat13),
                                                  .gm_program = kChord2Voice,
                                                  .voicing = VoicingPolicy::kLead,
                                                  .motif = &kSharedChord2Motif},
                                                 {.role = TrackRole::kArp,
                                                  .policy = RolePolicy::kChordTone,
                                                  .events = Span<const StyleEvent>(kArpHarp),
                                                  .gm_program = kArpVoice,
                                                  .motif = &kArpHarpMotif},
                                                 {.role = TrackRole::kLead,
                                                  .policy = RolePolicy::kChordTone,
                                                  .events = Span<const StyleEvent>(),
                                                  .gm_program = kLeadVoice,
                                                  .motif = &kLeadMotif},
                                                 {.role = TrackRole::kPerc,
                                                  .policy = RolePolicy::kFixed,
                                                  .events = Span<const StyleEvent>(kPercSoft)}};

// varBreak (Part B, ballad delta): the softest possible break in the family
// -- a held fermata pad rings through the whole bar; nothing else plays. No
// hit-and-silence shape like the rest of the family, this IS the break.
inline constexpr StyleEvent kBrkDrums[] = {
    {.step = 0, .tone = kCrash, .octave = 0, .vel = 50, .gate = kGateHeld},
};
inline constexpr StyleEvent kBrkPad[] = {
    {.step = 0,
     .tone = kRoot,
     .octave = 0,
     .vel = 60,
     .gate = kGateHeld,
     .gesture = ChordGesture::kRollUp},
};
inline constexpr StylePattern kBrkPatterns[] = {
    {.role = TrackRole::kDrums,
     .policy = RolePolicy::kFixed,
     .events = Span<const StyleEvent>(kBrkDrums)},
    {.role = TrackRole::kPad,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kBrkPad),
     .gm_program = kPadVoice,
     .voicing = VoicingPolicy::kLead},
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
// Groove (9100 Wave 1, task B): slow, expressive genre -- moderate-heavy
// humanize so the sparse legato lines breathe like a real rubato-leaning band.
inline constexpr Style kStyle{.name = "ballad",
                              .sections = Span<const StyleSection>(kSections),
                              .groove = {.humanize_timing = 12, .humanize_velocity = 20},
                              .tempo = 7200};

}  // namespace ballad

}  // namespace styles
}  // namespace arrangrr
