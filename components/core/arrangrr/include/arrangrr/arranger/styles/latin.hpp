#pragma once

#include "arrangrr/arranger/style_model.hpp"

namespace arrangrr {
namespace styles {

// ---------------------------------------------------------------------------
// "latin": salsa/mambo — cowbell on the beats, tumbao congas, timbale accents,
// an anticipated tumbao bass and a syncopated montuno guajeo comping figure.
namespace latin {
// Fuller-band roles (ADDITIVE): a string pad, a nylon-guitar guajeo, a harp arp
// and a rich extra percussion section (son clave, bongo martillo, agogo,
// maracas, guiro). kPad -> 48 (Strings), kChord2 -> 24 (Nylon), kArp -> 46
// (Harp); kPerc rides the drum channel.
inline constexpr std::int16_t kPadVoice = 48;
inline constexpr std::int16_t kChord2Voice = 24;
inline constexpr std::int16_t kArpVoice = 46;
inline constexpr std::int16_t kLeadVoice = 61;  // Brass Section — mambo horn line
// Mambo horn stab-answer in the back half of the bar. kScaleDegree keeps the
// "section" diatonic to the live KEY over the changes; the step-13 note is a
// kInterval leading tone (maj7, 11 semitones off the current chord root) that
// pushes up into the landing octave. kLead role -> anchor 72.
inline constexpr StyleEvent kMamboHorn[] = {
    {.step = 8,
     .tone = 4,
     .octave = 0,
     .vel = 92,
     .gate = kGateStaccato,
     .src = NoteSource::kScaleDegree},  // 5th
    {.step = 10,
     .tone = 4,
     .octave = 0,
     .vel = 82,
     .gate = kGateStaccato,
     .src = NoteSource::kScaleDegree},  // 5th
    {.step = 11,
     .tone = 5,
     .octave = 0,
     .vel = 86,
     .gate = kGateStaccato,
     .src = NoteSource::kScaleDegree},  // 6th
    {.step = 13,
     .tone = 11,
     .octave = 0,
     .vel = 84,
     .gate = kGateStaccato,
     .src = NoteSource::kInterval},  // maj7 leading tone off the chord root
    {.step = 14,
     .tone = 7,
     .octave = 0,
     .vel = 96,
     .gate = kGateBeat,
     .src = NoteSource::kScaleDegree},  // land on the octave
};
// Motif engine (9210, Ottorino RANK 6): the mambo horn call, wired only into
// varD. kDiatonicTranspose is the real payoff here — horn-call variation is
// idiomatic and carries no clave risk (unlike the tumbao bass below, which
// this batch leaves un-wired: Finding B, flagged, HOLD for a listening pass).
inline constexpr MotifSpec kMamboHornMotif{.transform = MotifTransform::kDiatonicTranspose,
                                           .seed = 601};
// Motif engine (9210 Wave 1, style-depth program task A): extend variation
// onto chord1/chord2/pad, wired onto the peak variation (varD) plus the
// shared reused Pad/nylon-guajeo arrays. kDisplacement for pad/chord2 (a soft
// evolving push); kRetrograde for the chord1 montuno comp (a legible
// mirrored answer). kDrums stays UNWIRED here (Finding B): the son
// clave/cowbell-conga groove's exact placement defines the tumbao's anchor,
// and motif variation risks decoherence -- same reasoning that already
// excludes the tumbao bass above.
inline constexpr MotifSpec kPeakChordMotif{.transform = MotifTransform::kRetrograde, .seed = 611};
inline constexpr MotifSpec kSharedPadMotif{.transform = MotifTransform::kDisplacement, .seed = 612};
inline constexpr MotifSpec kSharedChord2Motif{.transform = MotifTransform::kDisplacement,
                                              .seed = 613};
inline constexpr StyleEvent kPadTriad[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 54, .gate = kGateHeld},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 52, .gate = kGateHeld},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 54, .gate = kGateHeld},
    // bar 2 (style-depth Wave-2 C): re-hit so the string pad keeps sustaining
    // once a section using it (VarA) runs 2 bars -- a background hold, not
    // the section's own bar-to-bar variation (that lives in drums/chord1/perc).
    {.step = 16, .tone = kRoot, .octave = 0, .vel = 54, .gate = kGateHeld},
    {.step = 16, .tone = kThird, .octave = 0, .vel = 52, .gate = kGateHeld},
    {.step = 16, .tone = kFifth, .octave = 0, .vel = 54, .gate = kGateHeld},
};
inline constexpr StyleEvent kPad7[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 56, .gate = kGateHeld},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 54, .gate = kGateHeld},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 56, .gate = kGateHeld},
    {.step = 0, .tone = kSeventh, .octave = 0, .vel = 50, .gate = kGateHeld},
};
// Nylon guajeo: a light upper counter-line to chord1's montuno.
inline constexpr StyleEvent kNylon[] = {
    {.step = 4, .tone = kRoot, .octave = 1, .vel = 60, .gate = kGateStaccato},
    {.step = 4, .tone = kThird, .octave = 1, .vel = 60, .gate = kGateStaccato},
    {.step = 10, .tone = kFifth, .octave = 0, .vel = 58, .gate = kGateStaccato},
    {.step = 10, .tone = kRoot, .octave = 1, .vel = 58, .gate = kGateStaccato},
    {.step = 14, .tone = kThird, .octave = 1, .vel = 56, .gate = kGateStaccato},
    {.step = 14, .tone = kFifth, .octave = 1, .vel = 56, .gate = kGateStaccato},
};
// Harp arp: flowing arpeggio, high register (~72).
inline constexpr StyleEvent kHarp[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 62, .gate = kGateHat},
    {.step = 2, .tone = kThird, .octave = 0, .vel = 56, .gate = kGateHat},
    {.step = 4, .tone = kFifth, .octave = 0, .vel = 60, .gate = kGateHat},
    {.step = 6, .tone = kRoot, .octave = 1, .vel = 58, .gate = kGateHat},
    {.step = 8, .tone = kThird, .octave = 1, .vel = 62, .gate = kGateHat},
    {.step = 10, .tone = kRoot, .octave = 1, .vel = 56, .gate = kGateHat},
    {.step = 12, .tone = kFifth, .octave = 0, .vel = 60, .gate = kGateHat},
    {.step = 14, .tone = kThird, .octave = 0, .vel = 56, .gate = kGateHat},
};
// Son clave (3-2) — the timeline the whole groove leans on.
inline constexpr StyleEvent kClave[] = {
    {.step = 0, .tone = kClaves, .octave = 0, .vel = 82, .gate = kGateHat},
    {.step = 3, .tone = kClaves, .octave = 0, .vel = 78, .gate = kGateHat},
    {.step = 6, .tone = kClaves, .octave = 0, .vel = 78, .gate = kGateHat},
    {.step = 10, .tone = kClaves, .octave = 0, .vel = 80, .gate = kGateHat},
    {.step = 12, .tone = kClaves, .octave = 0, .vel = 80, .gate = kGateHat},
};
// varA perc: clave over a maracas sixteenth bed.
inline constexpr StyleEvent kPercA[] = {
    {.step = 0, .tone = kClaves, .octave = 0, .vel = 84, .gate = kGateHat},
    {.step = 3, .tone = kClaves, .octave = 0, .vel = 80, .gate = kGateHat},
    {.step = 6, .tone = kClaves, .octave = 0, .vel = 80, .gate = kGateHat},
    {.step = 10, .tone = kClaves, .octave = 0, .vel = 82, .gate = kGateHat},
    {.step = 12, .tone = kClaves, .octave = 0, .vel = 82, .gate = kGateHat},
    {.step = 0, .tone = kMaracas, .octave = 0, .vel = 54, .gate = kGateHat},
    {.step = 2, .tone = kMaracas, .octave = 0, .vel = 46, .gate = kGateHat},
    {.step = 4, .tone = kMaracas, .octave = 0, .vel = 54, .gate = kGateHat},
    {.step = 6, .tone = kMaracas, .octave = 0, .vel = 46, .gate = kGateHat},
    {.step = 8, .tone = kMaracas, .octave = 0, .vel = 54, .gate = kGateHat},
    {.step = 10, .tone = kMaracas, .octave = 0, .vel = 46, .gate = kGateHat},
    {.step = 12, .tone = kMaracas, .octave = 0, .vel = 54, .gate = kGateHat},
    {.step = 14, .tone = kMaracas, .octave = 0, .vel = 46, .gate = kGateHat},
    // bar 2 (style-depth Wave-2 C): the son clave repeats verbatim (it is a
    // fixed anchor, not varied bar to bar); the maracas bed gets one extra
    // accent on the last 16th as the turnaround into the repeat.
    {.step = 16, .tone = kClaves, .octave = 0, .vel = 84, .gate = kGateHat},
    {.step = 19, .tone = kClaves, .octave = 0, .vel = 80, .gate = kGateHat},
    {.step = 22, .tone = kClaves, .octave = 0, .vel = 80, .gate = kGateHat},
    {.step = 26, .tone = kClaves, .octave = 0, .vel = 82, .gate = kGateHat},
    {.step = 28, .tone = kClaves, .octave = 0, .vel = 82, .gate = kGateHat},
    {.step = 16, .tone = kMaracas, .octave = 0, .vel = 54, .gate = kGateHat},
    {.step = 18, .tone = kMaracas, .octave = 0, .vel = 46, .gate = kGateHat},
    {.step = 20, .tone = kMaracas, .octave = 0, .vel = 54, .gate = kGateHat},
    {.step = 22, .tone = kMaracas, .octave = 0, .vel = 46, .gate = kGateHat},
    {.step = 24, .tone = kMaracas, .octave = 0, .vel = 54, .gate = kGateHat},
    {.step = 26, .tone = kMaracas, .octave = 0, .vel = 46, .gate = kGateHat},
    {.step = 28, .tone = kMaracas, .octave = 0, .vel = 54, .gate = kGateHat},
    {.step = 30, .tone = kMaracas, .octave = 0, .vel = 46, .gate = kGateHat},
    {.step = 31, .tone = kMaracas, .octave = 0, .vel = 62, .gate = kGateHat},
};
// varB perc: clave, bongo martillo and agogo bell.
inline constexpr StyleEvent kPercB[] = {
    {.step = 0, .tone = kClaves, .octave = 0, .vel = 86, .gate = kGateHat},
    {.step = 3, .tone = kClaves, .octave = 0, .vel = 82, .gate = kGateHat},
    {.step = 6, .tone = kClaves, .octave = 0, .vel = 82, .gate = kGateHat},
    {.step = 10, .tone = kClaves, .octave = 0, .vel = 84, .gate = kGateHat},
    {.step = 12, .tone = kClaves, .octave = 0, .vel = 84, .gate = kGateHat},
    {.step = 0, .tone = kLoBongo, .octave = 0, .vel = 60, .gate = kGateHat},
    {.step = 2, .tone = kHiBongo, .octave = 0, .vel = 66, .gate = kGateHat},
    {.step = 4, .tone = kLoBongo, .octave = 0, .vel = 58, .gate = kGateHat},
    {.step = 6, .tone = kHiBongo, .octave = 0, .vel = 66, .gate = kGateHat},
    {.step = 8, .tone = kLoBongo, .octave = 0, .vel = 60, .gate = kGateHat},
    {.step = 10, .tone = kHiBongo, .octave = 0, .vel = 66, .gate = kGateHat},
    {.step = 12, .tone = kLoBongo, .octave = 0, .vel = 58, .gate = kGateHat},
    {.step = 14, .tone = kHiBongo, .octave = 0, .vel = 66, .gate = kGateHat},
    {.step = 0, .tone = kHiAgogo, .octave = 0, .vel = 64, .gate = kGateHat},
    {.step = 6, .tone = kLoAgogo, .octave = 0, .vel = 60, .gate = kGateHat},
    {.step = 8, .tone = kHiAgogo, .octave = 0, .vel = 64, .gate = kGateHat},
    {.step = 14, .tone = kLoAgogo, .octave = 0, .vel = 60, .gate = kGateHat},
};
// varC perc (cha-cha): clave with a guiro rasp.
inline constexpr StyleEvent kPercC[] = {
    {.step = 0, .tone = kClaves, .octave = 0, .vel = 82, .gate = kGateHat},
    {.step = 3, .tone = kClaves, .octave = 0, .vel = 78, .gate = kGateHat},
    {.step = 6, .tone = kClaves, .octave = 0, .vel = 78, .gate = kGateHat},
    {.step = 10, .tone = kClaves, .octave = 0, .vel = 80, .gate = kGateHat},
    {.step = 12, .tone = kClaves, .octave = 0, .vel = 80, .gate = kGateHat},
    {.step = 0, .tone = kShortGuiro, .octave = 0, .vel = 62, .gate = kGateHat},
    {.step = 4, .tone = kShortGuiro, .octave = 0, .vel = 56, .gate = kGateHat},
    {.step = 6, .tone = kShortGuiro, .octave = 0, .vel = 58, .gate = kGateHat},
    {.step = 8, .tone = kShortGuiro, .octave = 0, .vel = 62, .gate = kGateHat},
    {.step = 12, .tone = kShortGuiro, .octave = 0, .vel = 56, .gate = kGateHat},
    {.step = 14, .tone = kShortGuiro, .octave = 0, .vel = 58, .gate = kGateHat},
};
// varD perc (mambo peak): clave, bongos, maracas and agogo all in.
inline constexpr StyleEvent kPercD[] = {
    {.step = 0, .tone = kClaves, .octave = 0, .vel = 88, .gate = kGateHat},
    {.step = 3, .tone = kClaves, .octave = 0, .vel = 84, .gate = kGateHat},
    {.step = 6, .tone = kClaves, .octave = 0, .vel = 84, .gate = kGateHat},
    {.step = 10, .tone = kClaves, .octave = 0, .vel = 86, .gate = kGateHat},
    {.step = 12, .tone = kClaves, .octave = 0, .vel = 86, .gate = kGateHat},
    {.step = 0, .tone = kLoBongo, .octave = 0, .vel = 64, .gate = kGateHat},
    {.step = 2, .tone = kHiBongo, .octave = 0, .vel = 70, .gate = kGateHat},
    {.step = 4, .tone = kLoBongo, .octave = 0, .vel = 62, .gate = kGateHat},
    {.step = 6, .tone = kHiBongo, .octave = 0, .vel = 70, .gate = kGateHat},
    {.step = 8, .tone = kLoBongo, .octave = 0, .vel = 64, .gate = kGateHat},
    {.step = 10, .tone = kHiBongo, .octave = 0, .vel = 70, .gate = kGateHat},
    {.step = 12, .tone = kLoBongo, .octave = 0, .vel = 62, .gate = kGateHat},
    {.step = 14, .tone = kHiBongo, .octave = 0, .vel = 70, .gate = kGateHat},
    {.step = 0, .tone = kMaracas, .octave = 0, .vel = 52, .gate = kGateHat},
    {.step = 2, .tone = kMaracas, .octave = 0, .vel = 44, .gate = kGateHat},
    {.step = 4, .tone = kMaracas, .octave = 0, .vel = 52, .gate = kGateHat},
    {.step = 6, .tone = kMaracas, .octave = 0, .vel = 44, .gate = kGateHat},
    {.step = 8, .tone = kMaracas, .octave = 0, .vel = 52, .gate = kGateHat},
    {.step = 10, .tone = kMaracas, .octave = 0, .vel = 44, .gate = kGateHat},
    {.step = 12, .tone = kMaracas, .octave = 0, .vel = 52, .gate = kGateHat},
    {.step = 14, .tone = kMaracas, .octave = 0, .vel = 44, .gate = kGateHat},
    {.step = 0, .tone = kHiAgogo, .octave = 0, .vel = 66, .gate = kGateHat},
    {.step = 6, .tone = kLoAgogo, .octave = 0, .vel = 62, .gate = kGateHat},
    {.step = 10, .tone = kHiAgogo, .octave = 0, .vel = 66, .gate = kGateHat},
};
// Fill perc: agogo/bongo accents that ride the timbale rolls.
inline constexpr StyleEvent kPercFill[] = {
    {.step = 0, .tone = kHiAgogo, .octave = 0, .vel = 80, .gate = kGateHat},
    {.step = 4, .tone = kLoAgogo, .octave = 0, .vel = 84, .gate = kGateHat},
    {.step = 8, .tone = kHiBongo, .octave = 0, .vel = 88, .gate = kGateHat},
    {.step = 12, .tone = kLoBongo, .octave = 0, .vel = 92, .gate = kGateHat},
};
inline constexpr StyleEvent kHeldBass[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 94, .gate = kGateHeld},
    // bar 2 (style-depth Wave-2 C): re-hit so the held bass keeps sustaining
    // now that Intro1 (its only user) runs 2 bars -- a background hold.
    {.step = 16, .tone = kRoot, .octave = 0, .vel = 94, .gate = kGateHeld},
};
// Tumbao bass (Finding B, above): deliberately left UN-VARIED across bar 2 --
// the exact syncopated placement IS the tumbao's identity, and the guardrail
// singles it out as the one lane not to touch for bar-to-bar variation. The
// tail below is a LITERAL repeat of bar 1 (not a new idea), so the section
// keeps its anchor playing in bar 2 instead of going silent every other bar.
inline constexpr StyleEvent kTumbao[] = {
    {.step = 6, .tone = kFifth, .octave = -1, .vel = 98, .gate = kGate8th},
    {.step = 8, .tone = kSeventh, .octave = -1, .vel = 88, .gate = kGateStab},
    {.step = 12, .tone = kRoot, .octave = 0, .vel = 100, .gate = kGate8th},
    {.step = 14, .tone = kFifth, .octave = -1, .vel = 90, .gate = kGateStab},
    {.step = 0, .tone = kRoot, .octave = -1, .vel = 86, .gate = kGateStab},
    {.step = 22, .tone = kFifth, .octave = -1, .vel = 98, .gate = kGate8th},
    {.step = 24, .tone = kSeventh, .octave = -1, .vel = 88, .gate = kGateStab},
    {.step = 28, .tone = kRoot, .octave = 0, .vel = 100, .gate = kGate8th},
    {.step = 30, .tone = kFifth, .octave = -1, .vel = 90, .gate = kGateStab},
    {.step = 16, .tone = kRoot, .octave = -1, .vel = 86, .gate = kGateStab},
};
// varD peak bass (item 4): kTumbao verbatim, plus one added octave-doubled
// anticipation note on the syncopated "and of 2" (step 6) -- a real
// octave-doubled push on the tumbao's own accent, not a new placement.
// Wired ONLY into varD's Bass role; every other section keeps the plain
// shared kTumbao above (Finding B guardrail untouched everywhere else).
inline constexpr StyleEvent kTumbaoPeak[] = {
    {.step = 6, .tone = kFifth, .octave = -1, .vel = 98, .gate = kGate8th},
    {.step = 6, .tone = kFifth, .octave = 0, .vel = 70, .gate = kGateStaccato},
    {.step = 8, .tone = kSeventh, .octave = -1, .vel = 88, .gate = kGateStab},
    {.step = 12, .tone = kRoot, .octave = 0, .vel = 100, .gate = kGate8th},
    {.step = 14, .tone = kFifth, .octave = -1, .vel = 90, .gate = kGateStab},
    {.step = 0, .tone = kRoot, .octave = -1, .vel = 86, .gate = kGateStab},
    {.step = 22, .tone = kFifth, .octave = -1, .vel = 98, .gate = kGate8th},
    {.step = 24, .tone = kSeventh, .octave = -1, .vel = 88, .gate = kGateStab},
    {.step = 28, .tone = kRoot, .octave = 0, .vel = 100, .gate = kGate8th},
    {.step = 30, .tone = kFifth, .octave = -1, .vel = 90, .gate = kGateStab},
    {.step = 16, .tone = kRoot, .octave = -1, .vel = 86, .gate = kGateStab},
};
inline constexpr StyleEvent kIn1D[] = {
    {.step = 8, .tone = kOpenHiConga, .octave = 0, .vel = 66, .gate = 50},
    {.step = 10, .tone = kLoConga, .octave = 0, .vel = 62, .gate = 50},
    {.step = 12, .tone = kOpenHiConga, .octave = 0, .vel = 72, .gate = 50},
    {.step = 14, .tone = kOpenHiConga, .octave = 0, .vel = 80, .gate = 50},
    // bar 2 (style-depth Wave-2 C): arrival -- the cowbell enters alongside a
    // fuller conga pattern, landing right where Intro2/VarA pick up.
    {.step = 16, .tone = kCowbell, .octave = 0, .vel = 78, .gate = 50},
    {.step = 20, .tone = kCowbell, .octave = 0, .vel = 70, .gate = 50},
    {.step = 24, .tone = kCowbell, .octave = 0, .vel = 78, .gate = 50},
    {.step = 28, .tone = kCowbell, .octave = 0, .vel = 70, .gate = 50},
    {.step = 18, .tone = kOpenHiConga, .octave = 0, .vel = 66, .gate = 50},
    {.step = 22, .tone = kLoConga, .octave = 0, .vel = 70, .gate = 50},
    {.step = 26, .tone = kOpenHiConga, .octave = 0, .vel = 66, .gate = 50},
    {.step = 30, .tone = kLoConga, .octave = 0, .vel = 72, .gate = 50},
};
inline constexpr StylePattern kIn1P[] = {{.role = TrackRole::kDrums,
                                          .policy = RolePolicy::kFixed,
                                          .events = Span<const StyleEvent>(kIn1D)},
                                         {.role = TrackRole::kBass,
                                          .policy = RolePolicy::kChordTone,
                                          .events = Span<const StyleEvent>(kHeldBass)}};
inline constexpr StyleEvent kIn2D[] = {
    {.step = 0, .tone = kCowbell, .octave = 0, .vel = 78, .gate = 50},
    {.step = 4, .tone = kCowbell, .octave = 0, .vel = 70, .gate = 50},
    {.step = 8, .tone = kCowbell, .octave = 0, .vel = 78, .gate = 50},
    {.step = 12, .tone = kCowbell, .octave = 0, .vel = 70, .gate = 50},
    {.step = 2, .tone = kOpenHiConga, .octave = 0, .vel = 66, .gate = 50},
    {.step = 6, .tone = kLoConga, .octave = 0, .vel = 70, .gate = 50},
    {.step = 10, .tone = kOpenHiConga, .octave = 0, .vel = 66, .gate = 50},
    {.step = 14, .tone = kLoConga, .octave = 0, .vel = 72, .gate = 50}};
inline constexpr StyleEvent kIn2C[] = {
    {.step = 6, .tone = kRoot, .octave = 0, .vel = 76, .gate = kGateStaccato},
    {.step = 6, .tone = kThird, .octave = 0, .vel = 76, .gate = kGateStaccato},
    {.step = 6, .tone = kFifth, .octave = 0, .vel = 76, .gate = kGateStaccato},
    {.step = 12, .tone = kRoot, .octave = 0, .vel = 78, .gate = kGateStaccato},
    {.step = 12, .tone = kThird, .octave = 0, .vel = 78, .gate = kGateStaccato},
    {.step = 12, .tone = kFifth, .octave = 0, .vel = 78, .gate = kGateStaccato}};
inline constexpr StylePattern kIn2P[] = {{.role = TrackRole::kDrums,
                                          .policy = RolePolicy::kFixed,
                                          .events = Span<const StyleEvent>(kIn2D)},
                                         {.role = TrackRole::kBass,
                                          .policy = RolePolicy::kChordTone,
                                          .events = Span<const StyleEvent>(kTumbao)},
                                         {.role = TrackRole::kChord1,
                                          .policy = RolePolicy::kChordTone,
                                          .events = Span<const StyleEvent>(kIn2C)},
                                         {.role = TrackRole::kPad,
                                          .policy = RolePolicy::kChordTone,
                                          .events = Span<const StyleEvent>(kPad7),
                                          .gm_program = kPadVoice,
                                          .voicing = VoicingPolicy::kLead,
                                          .motif = &kSharedPadMotif},
                                         {.role = TrackRole::kPerc,
                                          .policy = RolePolicy::kFixed,
                                          .events = Span<const StyleEvent>(kClave)}};
inline constexpr StyleEvent kAD[] = {
    {.step = 0, .tone = kCowbell, .octave = 0, .vel = 84, .gate = 50},
    {.step = 4, .tone = kCowbell, .octave = 0, .vel = 74, .gate = 50},
    {.step = 8, .tone = kCowbell, .octave = 0, .vel = 84, .gate = 50},
    {.step = 12, .tone = kCowbell, .octave = 0, .vel = 74, .gate = 50},
    {.step = 2, .tone = kOpenHiConga, .octave = 0, .vel = 72, .gate = 50},
    {.step = 3, .tone = kMuteHiConga, .octave = 0, .vel = 58, .gate = 50},
    {.step = 6, .tone = kLoConga, .octave = 0, .vel = 76, .gate = 50},
    {.step = 8, .tone = kOpenHiConga, .octave = 0, .vel = 70, .gate = 50},
    {.step = 10, .tone = kOpenHiConga, .octave = 0, .vel = 72, .gate = 50},
    {.step = 11, .tone = kMuteHiConga, .octave = 0, .vel = 58, .gate = 50},
    {.step = 14, .tone = kLoConga, .octave = 0, .vel = 78, .gate = 50},
    {.step = 4, .tone = kHiTimbale, .octave = 0, .vel = 64, .gate = 50},
    {.step = 12, .tone = kLoTimbale, .octave = 0, .vel = 66, .gate = 50},
    // bar 2 (style-depth Wave-2 C): the cowbell/conga bed repeats, but the
    // timbale answers with a paired accent instead of the single hit -- a
    // real turnaround into the repeat.
    {.step = 16, .tone = kCowbell, .octave = 0, .vel = 84, .gate = 50},
    {.step = 20, .tone = kCowbell, .octave = 0, .vel = 74, .gate = 50},
    {.step = 24, .tone = kCowbell, .octave = 0, .vel = 84, .gate = 50},
    {.step = 28, .tone = kCowbell, .octave = 0, .vel = 74, .gate = 50},
    {.step = 18, .tone = kOpenHiConga, .octave = 0, .vel = 72, .gate = 50},
    {.step = 19, .tone = kMuteHiConga, .octave = 0, .vel = 58, .gate = 50},
    {.step = 22, .tone = kLoConga, .octave = 0, .vel = 76, .gate = 50},
    {.step = 24, .tone = kOpenHiConga, .octave = 0, .vel = 70, .gate = 50},
    {.step = 26, .tone = kOpenHiConga, .octave = 0, .vel = 72, .gate = 50},
    {.step = 27, .tone = kMuteHiConga, .octave = 0, .vel = 58, .gate = 50},
    {.step = 30, .tone = kLoConga, .octave = 0, .vel = 78, .gate = 50},
    {.step = 20, .tone = kHiTimbale, .octave = 0, .vel = 64, .gate = 50},
    {.step = 27, .tone = kLoTimbale, .octave = 0, .vel = 62, .gate = 50},
    {.step = 28, .tone = kHiTimbale, .octave = 0, .vel = 68, .gate = 50},
    {.step = 30, .tone = kLoTimbale, .octave = 0, .vel = 72, .gate = 50},
};
inline constexpr StyleEvent kAC[] = {
    {.step = 2, .tone = kRoot, .octave = 0, .vel = 80, .gate = kGateStaccato},
    {.step = 2, .tone = kThird, .octave = 0, .vel = 80, .gate = kGateStaccato},
    {.step = 2, .tone = kFifth, .octave = 0, .vel = 80, .gate = kGateStaccato},
    {.step = 6, .tone = kThird, .octave = 0, .vel = 76, .gate = kGateStaccato},
    {.step = 6, .tone = kFifth, .octave = 0, .vel = 76, .gate = kGateStaccato},
    {.step = 6, .tone = kSeventh, .octave = 0, .vel = 76, .gate = kGateStaccato},
    {.step = 8, .tone = kRoot, .octave = 0, .vel = 80, .gate = kGateStaccato},
    {.step = 8, .tone = kThird, .octave = 0, .vel = 80, .gate = kGateStaccato},
    {.step = 8, .tone = kFifth, .octave = 0, .vel = 80, .gate = kGateStaccato},
    {.step = 11, .tone = kThird, .octave = 0, .vel = 74, .gate = kGateStaccato},
    {.step = 11, .tone = kFifth, .octave = 0, .vel = 74, .gate = kGateStaccato},
    {.step = 14, .tone = kRoot, .octave = 0, .vel = 78, .gate = kGateStaccato},
    {.step = 14, .tone = kThird, .octave = 0, .vel = 78, .gate = kGateStaccato},
    {.step = 14, .tone = kFifth, .octave = 0, .vel = 78, .gate = kGateStaccato},
    // bar 2 (style-depth Wave-2 C): the montuno answers syncopated -- every
    // hit lands a 16th earlier than its bar-1 counterpart, a classic guajeo
    // call-and-response rather than a repeat.
    {.step = 17, .tone = kRoot, .octave = 0, .vel = 78, .gate = kGateStaccato},
    {.step = 17, .tone = kThird, .octave = 0, .vel = 78, .gate = kGateStaccato},
    {.step = 17, .tone = kFifth, .octave = 0, .vel = 78, .gate = kGateStaccato},
    {.step = 21, .tone = kThird, .octave = 0, .vel = 74, .gate = kGateStaccato},
    {.step = 21, .tone = kFifth, .octave = 0, .vel = 74, .gate = kGateStaccato},
    {.step = 21, .tone = kSeventh, .octave = 0, .vel = 74, .gate = kGateStaccato},
    {.step = 24, .tone = kRoot, .octave = 0, .vel = 78, .gate = kGateStaccato},
    {.step = 24, .tone = kThird, .octave = 0, .vel = 78, .gate = kGateStaccato},
    {.step = 24, .tone = kFifth, .octave = 0, .vel = 78, .gate = kGateStaccato},
    {.step = 27, .tone = kThird, .octave = 0, .vel = 72, .gate = kGateStaccato},
    {.step = 27, .tone = kFifth, .octave = 0, .vel = 72, .gate = kGateStaccato},
    {.step = 30, .tone = kRoot, .octave = 0, .vel = 76, .gate = kGateStaccato},
    {.step = 30, .tone = kThird, .octave = 0, .vel = 76, .gate = kGateStaccato},
    {.step = 30, .tone = kFifth, .octave = 0, .vel = 76, .gate = kGateStaccato},
};
inline constexpr StylePattern kAP[] = {{.role = TrackRole::kDrums,
                                        .policy = RolePolicy::kFixed,
                                        .events = Span<const StyleEvent>(kAD)},
                                       {.role = TrackRole::kBass,
                                        .policy = RolePolicy::kChordTone,
                                        .events = Span<const StyleEvent>(kTumbao)},
                                       {.role = TrackRole::kChord1,
                                        .policy = RolePolicy::kChordTone,
                                        .events = Span<const StyleEvent>(kAC)},
                                       {.role = TrackRole::kPad,
                                        .policy = RolePolicy::kChordTone,
                                        .events = Span<const StyleEvent>(kPadTriad),
                                        .gm_program = kPadVoice,
                                        .voicing = VoicingPolicy::kLead},
                                       {.role = TrackRole::kChord2,
                                        .policy = RolePolicy::kChordTone,
                                        .events = Span<const StyleEvent>(kNylon),
                                        .gm_program = kChord2Voice,
                                        .motif = &kSharedChord2Motif},
                                       {.role = TrackRole::kPerc,
                                        .policy = RolePolicy::kFixed,
                                        .events = Span<const StyleEvent>(kPercA)}};
inline constexpr StyleEvent kBD[] = {
    {.step = 0, .tone = kCowbell, .octave = 0, .vel = 88, .gate = 50},
    {.step = 2, .tone = kCowbell, .octave = 0, .vel = 68, .gate = 50},
    {.step = 4, .tone = kCowbell, .octave = 0, .vel = 78, .gate = 50},
    {.step = 6, .tone = kCowbell, .octave = 0, .vel = 68, .gate = 50},
    {.step = 8, .tone = kCowbell, .octave = 0, .vel = 88, .gate = 50},
    {.step = 10, .tone = kCowbell, .octave = 0, .vel = 68, .gate = 50},
    {.step = 12, .tone = kCowbell, .octave = 0, .vel = 78, .gate = 50},
    {.step = 14, .tone = kCowbell, .octave = 0, .vel = 68, .gate = 50},
    {.step = 0, .tone = kLoConga, .octave = 0, .vel = 72, .gate = 50},
    {.step = 2, .tone = kOpenHiConga, .octave = 0, .vel = 76, .gate = 50},
    {.step = 3, .tone = kOpenHiConga, .octave = 0, .vel = 70, .gate = 50},
    {.step = 6, .tone = kLoConga, .octave = 0, .vel = 78, .gate = 50},
    {.step = 8, .tone = kOpenHiConga, .octave = 0, .vel = 74, .gate = 50},
    {.step = 10, .tone = kOpenHiConga, .octave = 0, .vel = 76, .gate = 50},
    {.step = 11, .tone = kOpenHiConga, .octave = 0, .vel = 70, .gate = 50},
    {.step = 14, .tone = kLoConga, .octave = 0, .vel = 80, .gate = 50},
    {.step = 4, .tone = kHiTimbale, .octave = 0, .vel = 70, .gate = 50},
    {.step = 7, .tone = kLoTimbale, .octave = 0, .vel = 64, .gate = 50},
    {.step = 12, .tone = kHiTimbale, .octave = 0, .vel = 70, .gate = 50},
    {.step = 15, .tone = kLoTimbale, .octave = 0, .vel = 66, .gate = 50},
};
inline constexpr StyleEvent kBC[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 82, .gate = kGateStaccato},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 82, .gate = kGateStaccato},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 82, .gate = kGateStaccato},
    {.step = 2, .tone = kThird, .octave = 0, .vel = 74, .gate = kGateStaccato},
    {.step = 2, .tone = kFifth, .octave = 0, .vel = 74, .gate = kGateStaccato},
    {.step = 4, .tone = kRoot, .octave = 1, .vel = 76, .gate = kGateStaccato},
    {.step = 6, .tone = kThird, .octave = 0, .vel = 78, .gate = kGateStaccato},
    {.step = 6, .tone = kFifth, .octave = 0, .vel = 78, .gate = kGateStaccato},
    {.step = 6, .tone = kSeventh, .octave = 0, .vel = 78, .gate = kGateStaccato},
    {.step = 8, .tone = kRoot, .octave = 0, .vel = 82, .gate = kGateStaccato},
    {.step = 8, .tone = kThird, .octave = 0, .vel = 82, .gate = kGateStaccato},
    {.step = 8, .tone = kFifth, .octave = 0, .vel = 82, .gate = kGateStaccato},
    {.step = 10, .tone = kThird, .octave = 0, .vel = 74, .gate = kGateStaccato},
    {.step = 10, .tone = kFifth, .octave = 0, .vel = 74, .gate = kGateStaccato},
    {.step = 12, .tone = kRoot, .octave = 1, .vel = 76, .gate = kGateStaccato},
    {.step = 14, .tone = kThird, .octave = 0, .vel = 78, .gate = kGateStaccato},
    {.step = 14, .tone = kFifth, .octave = 0, .vel = 78, .gate = kGateStaccato},
    {.step = 14, .tone = kSeventh, .octave = 0, .vel = 78, .gate = kGateStaccato}};
inline constexpr StylePattern kBP[] = {{.role = TrackRole::kDrums,
                                        .policy = RolePolicy::kFixed,
                                        .events = Span<const StyleEvent>(kBD)},
                                       {.role = TrackRole::kBass,
                                        .policy = RolePolicy::kChordTone,
                                        .events = Span<const StyleEvent>(kTumbao)},
                                       {.role = TrackRole::kChord1,
                                        .policy = RolePolicy::kChordTone,
                                        .events = Span<const StyleEvent>(kBC)},
                                       {.role = TrackRole::kPad,
                                        .policy = RolePolicy::kChordTone,
                                        .events = Span<const StyleEvent>(kPadTriad),
                                        .gm_program = kPadVoice,
                                        .voicing = VoicingPolicy::kLead},
                                       {.role = TrackRole::kChord2,
                                        .policy = RolePolicy::kChordTone,
                                        .events = Span<const StyleEvent>(kNylon),
                                        .gm_program = kChord2Voice,
                                        .motif = &kSharedChord2Motif},
                                       {.role = TrackRole::kPerc,
                                        .policy = RolePolicy::kFixed,
                                        .events = Span<const StyleEvent>(kPercB)}};
inline constexpr StyleEvent kFAD[] = {
    {.step = 0, .tone = kHiTimbale, .octave = 0, .vel = 88, .gate = 50},
    {.step = 4, .tone = kHiTimbale, .octave = 0, .vel = 84, .gate = 50},
    {.step = 8, .tone = kLoTimbale, .octave = 0, .vel = 90, .gate = 50},
    {.step = 10, .tone = kLoTimbale, .octave = 0, .vel = 94, .gate = 50},
    {.step = 12, .tone = kLoConga, .octave = 0, .vel = 98, .gate = 50},
    {.step = 14, .tone = kLoConga, .octave = 0, .vel = 104, .gate = 50}};
inline constexpr StyleEvent kFBD[] = {
    {.step = 0, .tone = kHiTimbale, .octave = 0, .vel = 88, .gate = 50},
    {.step = 2, .tone = kHiTimbale, .octave = 0, .vel = 84, .gate = 50},
    {.step = 4, .tone = kLoTimbale, .octave = 0, .vel = 92, .gate = 50},
    {.step = 6, .tone = kLoTimbale, .octave = 0, .vel = 88, .gate = 50},
    {.step = 8, .tone = kTomHi, .octave = 0, .vel = 96, .gate = 50},
    {.step = 10, .tone = kTomMid, .octave = 0, .vel = 100, .gate = 50},
    {.step = 12, .tone = kTomLow, .octave = 0, .vel = 104, .gate = 50},
    {.step = 14, .tone = kTomFloor, .octave = 0, .vel = 108, .gate = 50}};
inline constexpr StyleEvent kFCD[] = {
    {.step = 0, .tone = kLoConga, .octave = 0, .vel = 86, .gate = 50},
    {.step = 2, .tone = kOpenHiConga, .octave = 0, .vel = 90, .gate = 50},
    {.step = 4, .tone = kHiTimbale, .octave = 0, .vel = 92, .gate = 50},
    {.step = 6, .tone = kHiTimbale, .octave = 0, .vel = 88, .gate = 50},
    {.step = 8, .tone = kLoTimbale, .octave = 0, .vel = 96, .gate = 50},
    {.step = 10, .tone = kLoTimbale, .octave = 0, .vel = 92, .gate = 50},
    {.step = 12, .tone = kTomLow, .octave = 0, .vel = 104, .gate = 50},
    {.step = 14, .tone = kTomFloor, .octave = 0, .vel = 110, .gate = 50}};
inline constexpr StyleEvent kFDD[] = {
    {.step = 0, .tone = kHiTimbale, .octave = 0, .vel = 90, .gate = 50},
    {.step = 1, .tone = kHiTimbale, .octave = 0, .vel = 86, .gate = 50},
    {.step = 2, .tone = kLoTimbale, .octave = 0, .vel = 94, .gate = 50},
    {.step = 3, .tone = kLoTimbale, .octave = 0, .vel = 90, .gate = 50},
    {.step = 4, .tone = kTomHi, .octave = 0, .vel = 98, .gate = 50},
    {.step = 5, .tone = kTomHi, .octave = 0, .vel = 94, .gate = 50},
    {.step = 6, .tone = kTomMid, .octave = 0, .vel = 100, .gate = 50},
    {.step = 7, .tone = kTomMid, .octave = 0, .vel = 96, .gate = 50},
    {.step = 8, .tone = kTomLow, .octave = 0, .vel = 104, .gate = 50},
    {.step = 9, .tone = kTomLow, .octave = 0, .vel = 100, .gate = 50},
    {.step = 10, .tone = kTomFloor, .octave = 0, .vel = 108, .gate = 50},
    {.step = 11, .tone = kTomFloor, .octave = 0, .vel = 104, .gate = 50},
    {.step = 12, .tone = kCowbell, .octave = 0, .vel = 110, .gate = 50},
    {.step = 13, .tone = kCowbell, .octave = 0, .vel = 106, .gate = 50},
    {.step = 14, .tone = kCowbell, .octave = 0, .vel = 114, .gate = 50},
    {.step = 15, .tone = kCrash, .octave = 0, .vel = 120, .gate = kGate8th}};
inline constexpr StylePattern kFAP[] = {{.role = TrackRole::kDrums,
                                         .policy = RolePolicy::kFixed,
                                         .events = Span<const StyleEvent>(kFAD)},
                                        {.role = TrackRole::kBass,
                                         .policy = RolePolicy::kChordTone,
                                         .events = Span<const StyleEvent>(kTumbao)},
                                        {.role = TrackRole::kPerc,
                                         .policy = RolePolicy::kFixed,
                                         .events = Span<const StyleEvent>(kPercFill)}};
inline constexpr StylePattern kFBP[] = {{.role = TrackRole::kDrums,
                                         .policy = RolePolicy::kFixed,
                                         .events = Span<const StyleEvent>(kFBD)},
                                        {.role = TrackRole::kBass,
                                         .policy = RolePolicy::kChordTone,
                                         .events = Span<const StyleEvent>(kTumbao)},
                                        {.role = TrackRole::kPerc,
                                         .policy = RolePolicy::kFixed,
                                         .events = Span<const StyleEvent>(kPercFill)}};
inline constexpr StylePattern kFCP[] = {{.role = TrackRole::kDrums,
                                         .policy = RolePolicy::kFixed,
                                         .events = Span<const StyleEvent>(kFCD)},
                                        {.role = TrackRole::kBass,
                                         .policy = RolePolicy::kChordTone,
                                         .events = Span<const StyleEvent>(kTumbao)},
                                        {.role = TrackRole::kPerc,
                                         .policy = RolePolicy::kFixed,
                                         .events = Span<const StyleEvent>(kPercFill)}};
inline constexpr StylePattern kFDP[] = {{.role = TrackRole::kDrums,
                                         .policy = RolePolicy::kFixed,
                                         .events = Span<const StyleEvent>(kFDD)},
                                        {.role = TrackRole::kBass,
                                         .policy = RolePolicy::kChordTone,
                                         .events = Span<const StyleEvent>(kTumbao)},
                                        {.role = TrackRole::kPerc,
                                         .policy = RolePolicy::kFixed,
                                         .events = Span<const StyleEvent>(kPercFill)}};
inline constexpr StyleEvent kE1D[] = {
    {.step = 0, .tone = kCrash, .octave = 0, .vel = 104, .gate = kGateHalfBar},
    {.step = 0, .tone = kCowbell, .octave = 0, .vel = 88, .gate = 50},
    {.step = 8, .tone = kLoConga, .octave = 0, .vel = 84, .gate = 50},
    // bar 2 (A.5, ending): the cierre's downbeat tag hit -- crash ring plus cowbell.
    {.step = 16, .tone = kCrash, .octave = 0, .vel = 100, .gate = kGateHeld},
    {.step = 16, .tone = kCowbell, .octave = 0, .vel = 86, .gate = kGateHat},
};
inline constexpr StyleEvent kE1B[] = {
    {.step = 0, .tone = kRoot, .octave = -1, .vel = 100, .gate = kGateHeld},
    // bar 2: the unison band stab lands on the downbeat.
    {.step = 16, .tone = kRoot, .octave = -1, .vel = 92, .gate = kGateStab},
};
inline constexpr StyleEvent kE1C[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 84, .gate = kGateHeld},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 84, .gate = kGateHeld},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 84, .gate = kGateHeld},
    // bar 2: the unison stab's triad, doubling the bass hit above.
    {.step = 16, .tone = kRoot, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 16, .tone = kThird, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 16, .tone = kFifth, .octave = 0, .vel = 90, .gate = kGateStab},
};
// A 3-hit clave tag phrase (A.5 device): the son clave keeps the timeline
// alive through the cierre's downbeat stab, syncopated off it.
inline constexpr StyleEvent kE1Perc[] = {
    {.step = 18, .tone = kClaves, .octave = 0, .vel = 86, .gate = kGateHat},
    {.step = 21, .tone = kClaves, .octave = 0, .vel = 82, .gate = kGateHat},
    {.step = 24, .tone = kClaves, .octave = 0, .vel = 88, .gate = kGateHat},
};
inline constexpr StylePattern kE1P[] = {{.role = TrackRole::kDrums,
                                         .policy = RolePolicy::kFixed,
                                         .events = Span<const StyleEvent>(kE1D)},
                                        {.role = TrackRole::kBass,
                                         .policy = RolePolicy::kChordTone,
                                         .events = Span<const StyleEvent>(kE1B)},
                                        {.role = TrackRole::kChord1,
                                         .policy = RolePolicy::kChordTone,
                                         .events = Span<const StyleEvent>(kE1C)},
                                        {.role = TrackRole::kPad,
                                         .policy = RolePolicy::kChordTone,
                                         .events = Span<const StyleEvent>(kPad7),
                                         .gm_program = kPadVoice,
                                         .voicing = VoicingPolicy::kLead,
                                         .motif = &kSharedPadMotif},
                                        {.role = TrackRole::kPerc,
                                         .policy = RolePolicy::kFixed,
                                         .events = Span<const StyleEvent>(kE1Perc)}};
inline constexpr StyleEvent kE2D[] = {
    {.step = 0, .tone = kCrash, .octave = 0, .vel = 112, .gate = kGateHeld},
    {.step = 0, .tone = kCowbell, .octave = 0, .vel = 90, .gate = 50},
    {.step = 8, .tone = kLoConga, .octave = 0, .vel = 86, .gate = 50},
    {.step = 12, .tone = kHiTimbale, .octave = 0, .vel = 94, .gate = 50},
    // bar 2 (A.5, mambo "corte"): TWO full-band unison tag hits, not ending1's
    // single clave phrase -- a repeated cut before the final hold.
    {.step = 16, .tone = kCrash, .octave = 0, .vel = 108, .gate = kGateStab},
    {.step = 16, .tone = kCowbell, .octave = 0, .vel = 94, .gate = kGateStab},
    {.step = 16, .tone = kHiTimbale, .octave = 0, .vel = 98, .gate = kGateStab},
    {.step = 24, .tone = kCrash, .octave = 0, .vel = 112, .gate = kGateHeld},
    {.step = 24, .tone = kCowbell, .octave = 0, .vel = 96, .gate = kGateStab},
    {.step = 24, .tone = kHiTimbale, .octave = 0, .vel = 100, .gate = kGateStab},
};
inline constexpr StyleEvent kE2B[] = {
    {.step = 0, .tone = kRoot, .octave = -1, .vel = 104, .gate = kGateHeld},
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 94, .gate = kGateHeld},
    // bar 2: the corte's two stabs, root only, mirroring the drums above.
    {.step = 16, .tone = kRoot, .octave = -1, .vel = 100, .gate = kGateStab},
    {.step = 24, .tone = kRoot, .octave = -1, .vel = 104, .gate = kGateHeld},
};
inline constexpr StyleEvent kE2C[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 86, .gate = kGateHeld},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 86, .gate = kGateHeld},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 86, .gate = kGateHeld},
    {.step = 0, .tone = kSeventh, .octave = 0, .vel = 86, .gate = kGateHeld},
    // bar 2: the corte's two triad stabs -- the second holds as the true
    // final chord, a full 7th voicing.
    {.step = 16, .tone = kRoot, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 16, .tone = kThird, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 16, .tone = kFifth, .octave = 0, .vel = 90, .gate = kGateStab},
    {.step = 24, .tone = kRoot, .octave = 0, .vel = 94, .gate = kGateHeld},
    {.step = 24, .tone = kThird, .octave = 0, .vel = 94, .gate = kGateHeld},
    {.step = 24, .tone = kFifth, .octave = 0, .vel = 94, .gate = kGateHeld},
    {.step = 24, .tone = kSeventh, .octave = 0, .vel = 94, .gate = kGateHeld},
};
// The clave keeps the timeline alive through both corte stabs (optional per
// spec; strengthens the "corte" without changing its defining drum shape).
inline constexpr StyleEvent kE2Perc[] = {
    {.step = 16, .tone = kClaves, .octave = 0, .vel = 88, .gate = kGateHat},
    {.step = 24, .tone = kClaves, .octave = 0, .vel = 90, .gate = kGateHat},
};
inline constexpr StylePattern kE2P[] = {{.role = TrackRole::kDrums,
                                         .policy = RolePolicy::kFixed,
                                         .events = Span<const StyleEvent>(kE2D)},
                                        {.role = TrackRole::kBass,
                                         .policy = RolePolicy::kChordTone,
                                         .events = Span<const StyleEvent>(kE2B)},
                                        {.role = TrackRole::kChord1,
                                         .policy = RolePolicy::kChordTone,
                                         .events = Span<const StyleEvent>(kE2C)},
                                        {.role = TrackRole::kPad,
                                         .policy = RolePolicy::kChordTone,
                                         .events = Span<const StyleEvent>(kPad7),
                                         .gm_program = kPadVoice,
                                         .voicing = VoicingPolicy::kLead,
                                         .motif = &kSharedPadMotif},
                                        {.role = TrackRole::kPerc,
                                         .policy = RolePolicy::kFixed,
                                         .events = Span<const StyleEvent>(kE2Perc)}};
// varC: cha-cha — lighter cowbell on the beats, simple conga slaps, sparser montuno.
inline constexpr StyleEvent kCD[] = {
    {.step = 0, .tone = kCowbell, .octave = 0, .vel = 78, .gate = 50},
    {.step = 4, .tone = kCowbell, .octave = 0, .vel = 70, .gate = 50},
    {.step = 8, .tone = kCowbell, .octave = 0, .vel = 78, .gate = 50},
    {.step = 12, .tone = kCowbell, .octave = 0, .vel = 70, .gate = 50},
    {.step = 2, .tone = kOpenHiConga, .octave = 0, .vel = 66, .gate = 50},
    {.step = 6, .tone = kLoConga, .octave = 0, .vel = 70, .gate = 50},
    {.step = 10, .tone = kOpenHiConga, .octave = 0, .vel = 66, .gate = 50},
    {.step = 14, .tone = kLoConga, .octave = 0, .vel = 72, .gate = 50},
    {.step = 6, .tone = kHiTimbale, .octave = 0, .vel = 62, .gate = 50},
    {.step = 14, .tone = kLoTimbale, .octave = 0, .vel = 64, .gate = 50}};
inline constexpr StyleEvent kCC[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 80, .gate = kGateStaccato},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 80, .gate = kGateStaccato},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 80, .gate = kGateStaccato},
    {.step = 6, .tone = kThird, .octave = 0, .vel = 76, .gate = kGateStaccato},
    {.step = 6, .tone = kFifth, .octave = 0, .vel = 76, .gate = kGateStaccato},
    {.step = 6, .tone = kSeventh, .octave = 0, .vel = 76, .gate = kGateStaccato},
    {.step = 10, .tone = kRoot, .octave = 0, .vel = 80, .gate = kGateStaccato},
    {.step = 10, .tone = kThird, .octave = 0, .vel = 80, .gate = kGateStaccato},
    {.step = 10, .tone = kFifth, .octave = 0, .vel = 80, .gate = kGateStaccato}};
inline constexpr StylePattern kCP[] = {{.role = TrackRole::kDrums,
                                        .policy = RolePolicy::kFixed,
                                        .events = Span<const StyleEvent>(kCD)},
                                       {.role = TrackRole::kBass,
                                        .policy = RolePolicy::kChordTone,
                                        .events = Span<const StyleEvent>(kTumbao)},
                                       {.role = TrackRole::kChord1,
                                        .policy = RolePolicy::kChordTone,
                                        .events = Span<const StyleEvent>(kCC)},
                                       {.role = TrackRole::kPad,
                                        .policy = RolePolicy::kChordTone,
                                        .events = Span<const StyleEvent>(kPad7),
                                        .gm_program = kPadVoice,
                                        .voicing = VoicingPolicy::kLead,
                                        .motif = &kSharedPadMotif},
                                       {.role = TrackRole::kChord2,
                                        .policy = RolePolicy::kChordTone,
                                        .events = Span<const StyleEvent>(kNylon),
                                        .gm_program = kChord2Voice,
                                        .motif = &kSharedChord2Motif},
                                       {.role = TrackRole::kArp,
                                        .policy = RolePolicy::kChordTone,
                                        .events = Span<const StyleEvent>(kHarp),
                                        .gm_program = kArpVoice},
                                       {.role = TrackRole::kPerc,
                                        .policy = RolePolicy::kFixed,
                                        .events = Span<const StyleEvent>(kPercC)}};
// varD: mambo peak — cowbell eighths, busy congas and timbales, full 7th montuno.
inline constexpr StyleEvent kDD[] = {
    {.step = 0, .tone = kCowbell, .octave = 0, .vel = 88, .gate = 50},
    {.step = 2, .tone = kCowbell, .octave = 0, .vel = 68, .gate = 50},
    {.step = 4, .tone = kCowbell, .octave = 0, .vel = 78, .gate = 50},
    {.step = 6, .tone = kCowbell, .octave = 0, .vel = 68, .gate = 50},
    {.step = 8, .tone = kCowbell, .octave = 0, .vel = 88, .gate = 50},
    {.step = 10, .tone = kCowbell, .octave = 0, .vel = 68, .gate = 50},
    {.step = 12, .tone = kCowbell, .octave = 0, .vel = 78, .gate = 50},
    {.step = 14, .tone = kCowbell, .octave = 0, .vel = 68, .gate = 50},
    {.step = 0, .tone = kLoConga, .octave = 0, .vel = 72, .gate = 50},
    {.step = 2, .tone = kOpenHiConga, .octave = 0, .vel = 76, .gate = 50},
    {.step = 3, .tone = kOpenHiConga, .octave = 0, .vel = 70, .gate = 50},
    {.step = 6, .tone = kLoConga, .octave = 0, .vel = 78, .gate = 50},
    {.step = 8, .tone = kOpenHiConga, .octave = 0, .vel = 74, .gate = 50},
    {.step = 10, .tone = kOpenHiConga, .octave = 0, .vel = 76, .gate = 50},
    {.step = 11, .tone = kOpenHiConga, .octave = 0, .vel = 70, .gate = 50},
    {.step = 14, .tone = kLoConga, .octave = 0, .vel = 80, .gate = 50},
    {.step = 4, .tone = kHiTimbale, .octave = 0, .vel = 70, .gate = 50},
    {.step = 7, .tone = kLoTimbale, .octave = 0, .vel = 64, .gate = 50},
    {.step = 12, .tone = kHiTimbale, .octave = 0, .vel = 70, .gate = 50},
    {.step = 15, .tone = kLoTimbale, .octave = 0, .vel = 66, .gate = 50}};
// varD peak drums (item 4): the cowbell voice goes from son-montuno 8th
// spacing (kBD's kDD above) to a genuine 16th-note "mambo bell" -- every
// step in the bar, accents alternating strong/weak -- with the same
// conga/timbale bed carried over verbatim.
inline constexpr StyleEvent kDDPeak[] = {
    {.step = 0, .tone = kCowbell, .octave = 0, .vel = 90, .gate = 50},
    {.step = 1, .tone = kCowbell, .octave = 0, .vel = 64, .gate = 50},
    {.step = 2, .tone = kCowbell, .octave = 0, .vel = 88, .gate = 50},
    {.step = 3, .tone = kCowbell, .octave = 0, .vel = 62, .gate = 50},
    {.step = 4, .tone = kCowbell, .octave = 0, .vel = 90, .gate = 50},
    {.step = 5, .tone = kCowbell, .octave = 0, .vel = 64, .gate = 50},
    {.step = 6, .tone = kCowbell, .octave = 0, .vel = 88, .gate = 50},
    {.step = 7, .tone = kCowbell, .octave = 0, .vel = 62, .gate = 50},
    {.step = 8, .tone = kCowbell, .octave = 0, .vel = 90, .gate = 50},
    {.step = 9, .tone = kCowbell, .octave = 0, .vel = 64, .gate = 50},
    {.step = 10, .tone = kCowbell, .octave = 0, .vel = 88, .gate = 50},
    {.step = 11, .tone = kCowbell, .octave = 0, .vel = 62, .gate = 50},
    {.step = 12, .tone = kCowbell, .octave = 0, .vel = 90, .gate = 50},
    {.step = 13, .tone = kCowbell, .octave = 0, .vel = 64, .gate = 50},
    {.step = 14, .tone = kCowbell, .octave = 0, .vel = 88, .gate = 50},
    {.step = 15, .tone = kCowbell, .octave = 0, .vel = 62, .gate = 50},
    {.step = 0, .tone = kLoConga, .octave = 0, .vel = 72, .gate = 50},
    {.step = 2, .tone = kOpenHiConga, .octave = 0, .vel = 76, .gate = 50},
    {.step = 3, .tone = kOpenHiConga, .octave = 0, .vel = 70, .gate = 50},
    {.step = 6, .tone = kLoConga, .octave = 0, .vel = 78, .gate = 50},
    {.step = 8, .tone = kOpenHiConga, .octave = 0, .vel = 74, .gate = 50},
    {.step = 10, .tone = kOpenHiConga, .octave = 0, .vel = 76, .gate = 50},
    {.step = 11, .tone = kOpenHiConga, .octave = 0, .vel = 70, .gate = 50},
    {.step = 14, .tone = kLoConga, .octave = 0, .vel = 80, .gate = 50},
    {.step = 4, .tone = kHiTimbale, .octave = 0, .vel = 70, .gate = 50},
    {.step = 7, .tone = kLoTimbale, .octave = 0, .vel = 64, .gate = 50},
    {.step = 12, .tone = kHiTimbale, .octave = 0, .vel = 70, .gate = 50},
    {.step = 15, .tone = kLoTimbale, .octave = 0, .vel = 66, .gate = 50},
};
inline constexpr StyleEvent kDC[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 84, .gate = kGateStaccato},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 84, .gate = kGateStaccato},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 84, .gate = kGateStaccato},
    {.step = 0, .tone = kSeventh, .octave = 0, .vel = 84, .gate = kGateStaccato},
    {.step = 2, .tone = kThird, .octave = 0, .vel = 74, .gate = kGateStaccato},
    {.step = 2, .tone = kFifth, .octave = 0, .vel = 74, .gate = kGateStaccato},
    {.step = 6, .tone = kThird, .octave = 0, .vel = 78, .gate = kGateStaccato},
    {.step = 6, .tone = kFifth, .octave = 0, .vel = 78, .gate = kGateStaccato},
    {.step = 6, .tone = kSeventh, .octave = 0, .vel = 78, .gate = kGateStaccato},
    {.step = 8, .tone = kRoot, .octave = 0, .vel = 84, .gate = kGateStaccato},
    {.step = 8, .tone = kThird, .octave = 0, .vel = 84, .gate = kGateStaccato},
    {.step = 8, .tone = kFifth, .octave = 0, .vel = 84, .gate = kGateStaccato},
    {.step = 8, .tone = kSeventh, .octave = 0, .vel = 84, .gate = kGateStaccato},
    {.step = 10, .tone = kThird, .octave = 0, .vel = 74, .gate = kGateStaccato},
    {.step = 10, .tone = kFifth, .octave = 0, .vel = 74, .gate = kGateStaccato},
    {.step = 14, .tone = kThird, .octave = 0, .vel = 78, .gate = kGateStaccato},
    {.step = 14, .tone = kFifth, .octave = 0, .vel = 78, .gate = kGateStaccato},
    {.step = 14, .tone = kSeventh, .octave = 0, .vel = 78, .gate = kGateStaccato}};
inline constexpr StylePattern kDP[] = {{.role = TrackRole::kDrums,
                                        .policy = RolePolicy::kFixed,
                                        .events = Span<const StyleEvent>(kDDPeak)},
                                       {.role = TrackRole::kBass,
                                        .policy = RolePolicy::kChordTone,
                                        .events = Span<const StyleEvent>(kTumbaoPeak)},
                                       {.role = TrackRole::kChord1,
                                        .policy = RolePolicy::kChordTone,
                                        .events = Span<const StyleEvent>(kDC),
                                        .motif = &kPeakChordMotif},
                                       {.role = TrackRole::kPad,
                                        .policy = RolePolicy::kChordTone,
                                        .events = Span<const StyleEvent>(kPad7),
                                        .gm_program = kPadVoice,
                                        .voicing = VoicingPolicy::kLead,
                                        .motif = &kSharedPadMotif},
                                       {.role = TrackRole::kChord2,
                                        .policy = RolePolicy::kChordTone,
                                        .events = Span<const StyleEvent>(kNylon),
                                        .gm_program = kChord2Voice,
                                        .motif = &kSharedChord2Motif},
                                       {.role = TrackRole::kArp,
                                        .policy = RolePolicy::kChordTone,
                                        .events = Span<const StyleEvent>(kHarp),
                                        .gm_program = kArpVoice},
                                       {.role = TrackRole::kLead,
                                        .policy = RolePolicy::kChordTone,
                                        .events = Span<const StyleEvent>(kMamboHorn),
                                        .gm_program = kLeadVoice,
                                        .motif = &kMamboHornMotif},
                                       {.role = TrackRole::kPerc,
                                        .policy = RolePolicy::kFixed,
                                        .events = Span<const StyleEvent>(kPercD)}};
// varBreak (cierre): the band cuts on a downbeat accent — crash+cowbell, a low
// bass pop, a full 7th stab — while the son clave keeps the timeline through the
// silence, then a timbale abanico pickup on beat 4 throws the mambo back in.
inline constexpr StyleEvent kBrkD[] = {
    {.step = 0, .tone = kCrash, .octave = 0, .vel = 110, .gate = kGateStab},
    {.step = 0, .tone = kCowbell, .octave = 0, .vel = 92, .gate = 50},
    {.step = 12, .tone = kHiTimbale, .octave = 0, .vel = 96, .gate = 50},
    {.step = 13, .tone = kHiTimbale, .octave = 0, .vel = 100, .gate = 50},
    {.step = 14, .tone = kLoTimbale, .octave = 0, .vel = 104, .gate = 50},
    {.step = 15, .tone = kLoTimbale, .octave = 0, .vel = 110, .gate = 50},
};
inline constexpr StyleEvent kBrkB[] = {
    {.step = 0, .tone = kRoot, .octave = -1, .vel = 104, .gate = kGateStab}};
inline constexpr StyleEvent kBrkC[] = {
    {.step = 0, .tone = kRoot, .octave = 0, .vel = 94, .gate = kGateStab},
    {.step = 0, .tone = kThird, .octave = 0, .vel = 94, .gate = kGateStab},
    {.step = 0, .tone = kFifth, .octave = 0, .vel = 94, .gate = kGateStab},
    {.step = 0, .tone = kSeventh, .octave = 0, .vel = 94, .gate = kGateStab},
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
    {.role = TrackRole::kPerc,
     .policy = RolePolicy::kFixed,
     .events = Span<const StyleEvent>(kClave)},
};
// Style-depth Wave-2 C: VarB stays 1 bar -- no natural bar-2 idea beyond what
// VarA above already carries, and its own bass/chord2 are the same
// tumbao/motif-locked lanes flagged above. VarC and VarD stay 1 bar too:
// their chord2 (kNylon, kSharedChord2Motif) is motif-driven in every use,
// VarD's chord1 and lead are motif-driven as well (kPeakChordMotif,
// kMamboHornMotif), and the tumbao bass (kTumbao) is deliberately left
// un-varied everywhere per the clave/tumbao anchor guardrail above -- little
// motif-safe headroom left for a genuine bar 2 on the peak section.
inline constexpr StyleSection kSections[] = {
    {.type = SectionType::kIntro1, .bars = 2, .patterns = Span<const StylePattern>(kIn1P)},
    {.type = SectionType::kIntro2, .bars = 1, .patterns = Span<const StylePattern>(kIn2P)},
    {.type = SectionType::kVarA, .bars = 2, .patterns = Span<const StylePattern>(kAP)},
    {.type = SectionType::kVarB, .bars = 1, .patterns = Span<const StylePattern>(kBP)},
    {.type = SectionType::kVarC, .bars = 1, .patterns = Span<const StylePattern>(kCP)},
    {.type = SectionType::kVarD, .bars = 1, .patterns = Span<const StylePattern>(kDP)},
    {.type = SectionType::kFillA, .bars = 1, .patterns = Span<const StylePattern>(kFAP)},
    {.type = SectionType::kFillB, .bars = 1, .patterns = Span<const StylePattern>(kFBP)},
    {.type = SectionType::kFillC, .bars = 1, .patterns = Span<const StylePattern>(kFCP)},
    {.type = SectionType::kFillD, .bars = 1, .patterns = Span<const StylePattern>(kFDP)},
    {.type = SectionType::kBreak, .bars = 1, .patterns = Span<const StylePattern>(kBrkP)},
    {.type = SectionType::kEnding1, .bars = 2, .patterns = Span<const StylePattern>(kE1P)},
    {.type = SectionType::kEnding2, .bars = 2, .patterns = Span<const StylePattern>(kE2P)},
};
// TODO(owner): Ottorino proposes 18000 (180 BPM cut-time salsa) — pending owner confirm.
// Left at kDefaultBpm (9120) until the owner confirms the aggressive cut-time tempo.
// Groove (9100 Wave 1, task B): latin is named alongside swing/blues as one of
// the loosest genres -- heavy deterministic humanize.
inline constexpr Style kStyle{.name = "latin",
                              .sections = Span<const StyleSection>(kSections),
                              .groove = {.humanize_timing = 16, .humanize_velocity = 24}};
}  // namespace latin

}  // namespace styles
}  // namespace arrangrr
