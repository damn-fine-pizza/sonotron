#pragma once

#include "arrangrr/arranger/style_model.hpp"

namespace arrangrr {
namespace styles {


// ---------------------------------------------------------------------------
// "samba": driving surdo kick on the backbeat, busy tamborim 16ths, agogo
// bells, a syncopated root-fifth bass and short percussive stabs.
namespace samba {
// Held root pickup for Intro1. Re-articulated at step 16 too (style-depth
// Wave-2 C): Intro1 is now 2 bars, and the sustain re-triggers every bar
// exactly as it always has when the section looped -- a background hold, not
// the section's own bar-to-bar variation (same idiom as basic's kPadTriad).
inline constexpr StyleEvent kHeldBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=94, .gate=kGateHeld},
    {.step=16, .tone=kRoot, .octave=0, .vel=92, .gate=kGateHeld},
};
inline constexpr StyleEvent kSyncBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=96, .gate=kGate8th}, {.step=4, .tone=kFifth, .octave=-1, .vel=100, .gate=kGate8th}, {.step=6, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStaccato}, {.step=8, .tone=kRoot, .octave=0, .vel=92, .gate=kGate8th}, {.step=12, .tone=kFifth, .octave=-1, .vel=100, .gate=kGate8th}, {.step=14, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStaccato}};
// Motif engine (9210, Ottorino RANK 13): samba's only bass idiom, reused
// across VarA/B/C/D and FillA-D. Finding B flagged this lane at moderate risk
// (the surdo/tamborim pattern in the same section stays fixed while the bass
// alone would move) and recommended a listening pass before shipping; this
// batch unholds it per the owner's Phase 7 node 9210 continuation directive.
// kRetrograde, seed 1301. Wired into VarA-D/FillA-D only (8 of the 9
// kSyncBass references) — the intro (kIn2P) states the pattern once, plainly,
// before any call-and-response begins (same idiom as every other style).
inline constexpr MotifSpec kSyncBassMotif{.transform = MotifTransform::kRetrograde, .seed = 1301};
// ---------------------------------------------------------------------------
// Fuller-band roles (ADDITIVE): a string bed, a nylon-guitar cavaquinho comp, a
// bright arpeggio and very rich shaker/guiro/cowbell percussion. Pad at register
// 48, arp at 72. Voices: pad -> Strings, chord2 -> Nylon Guitar, arp -> Harp.
inline constexpr std::int16_t kPadVoice = 48;
inline constexpr std::int16_t kChord2Voice = 24;
inline constexpr std::int16_t kArpVoice = 46;
// Motif engine (9210, Option 2 / generated seed, node 9210 owner sign-off):
// samba's kLead model gap closed by generation. Sits a bright, cutting
// piccolo an octave above the anchor (a stable degree), matching real
// samba's high flute/clarinet melody over the surdo/tamborim bed; draws its
// onset candidates from kPercRich's own guiro/cowbell even-16th mask.
inline constexpr std::int16_t kLeadVoice = 72;  // Piccolo
inline constexpr MotifSpec kLeadMotif{
    .transform = MotifTransform::kDiatonicTranspose,
    .seed = 1302, .length = 6, .center_degree = 7, .vel = 78, .gate = kGateStaccato,
    .idiom_role = TrackRole::kPerc,
};
// Pad: hold the triad under the whole bar. Re-articulated at step 16 too
// (style-depth Wave-2 C): VarA is now 2 bars, and the sustain re-triggers
// every bar exactly as it always has when the section looped -- a
// background hold, not the section's own bar-to-bar variation (kPadTriad is
// unmotifed in every use here, so this is safe -- basic.hpp's own kPadTriad).
inline constexpr StyleEvent kPadTriad[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=56, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=54, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=56, .gate=kGateHeld},
    {.step=16, .tone=kRoot, .octave=0, .vel=56, .gate=kGateHeld}, {.step=16, .tone=kThird, .octave=0, .vel=54, .gate=kGateHeld}, {.step=16, .tone=kFifth, .octave=0, .vel=56, .gate=kGateHeld},
};
inline constexpr StyleEvent kPad7[] = {{.step=0, .tone=kRoot, .octave=0, .vel=58, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=56, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=58, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=52, .gate=kGateHeld}};
inline constexpr StyleEvent kPadRehit[] = {{.step=0, .tone=kRoot, .octave=0, .vel=58, .gate=kGateHalfBar}, {.step=0, .tone=kThird, .octave=0, .vel=56, .gate=kGateHalfBar}, {.step=0, .tone=kFifth, .octave=0, .vel=58, .gate=kGateHalfBar}, {.step=0, .tone=kSeventh, .octave=0, .vel=52, .gate=kGateHalfBar}, {.step=8, .tone=kRoot, .octave=0, .vel=56, .gate=kGateHalfBar}, {.step=8, .tone=kThird, .octave=0, .vel=54, .gate=kGateHalfBar}, {.step=8, .tone=kFifth, .octave=0, .vel=56, .gate=kGateHalfBar}, {.step=8, .tone=kSeventh, .octave=0, .vel=50, .gate=kGateHalfBar}};
inline constexpr StyleEvent kChord2Sync[] = {{.step=3, .tone=kThird, .octave=0, .vel=62, .gate=kGateStaccato}, {.step=3, .tone=kSeventh, .octave=0, .vel=62, .gate=kGateStaccato}, {.step=7, .tone=kThird, .octave=0, .vel=60, .gate=kGateStaccato}, {.step=7, .tone=kFifth, .octave=0, .vel=60, .gate=kGateStaccato}, {.step=11, .tone=kThird, .octave=0, .vel=62, .gate=kGateStaccato}, {.step=11, .tone=kSeventh, .octave=0, .vel=62, .gate=kGateStaccato}, {.step=15, .tone=kFifth, .octave=0, .vel=58, .gate=kGateStaccato}, {.step=15, .tone=kSeventh, .octave=0, .vel=58, .gate=kGateStaccato}};
inline constexpr StyleEvent kChord2Off[] = {{.step=1, .tone=kThird, .octave=0, .vel=60, .gate=kGateStaccato}, {.step=1, .tone=kSeventh, .octave=0, .vel=60, .gate=kGateStaccato}, {.step=3, .tone=kFifth, .octave=0, .vel=58, .gate=kGateStaccato}, {.step=3, .tone=kSeventh, .octave=0, .vel=58, .gate=kGateStaccato}, {.step=7, .tone=kThird, .octave=0, .vel=60, .gate=kGateStaccato}, {.step=7, .tone=kSeventh, .octave=0, .vel=60, .gate=kGateStaccato}, {.step=9, .tone=kFifth, .octave=0, .vel=58, .gate=kGateStaccato}, {.step=9, .tone=kSeventh, .octave=0, .vel=58, .gate=kGateStaccato}, {.step=11, .tone=kThird, .octave=0, .vel=60, .gate=kGateStaccato}, {.step=11, .tone=kSeventh, .octave=0, .vel=60, .gate=kGateStaccato}, {.step=15, .tone=kFifth, .octave=0, .vel=58, .gate=kGateStaccato}, {.step=15, .tone=kSeventh, .octave=0, .vel=58, .gate=kGateStaccato}};
inline constexpr StyleEvent kArp8[] = {{.step=0, .tone=kRoot, .octave=0, .vel=62, .gate=kGateHat}, {.step=2, .tone=kFifth, .octave=0, .vel=56, .gate=kGateHat}, {.step=4, .tone=kThird, .octave=0, .vel=60, .gate=kGateHat}, {.step=6, .tone=kSeventh, .octave=0, .vel=56, .gate=kGateHat}, {.step=8, .tone=kRoot, .octave=1, .vel=62, .gate=kGateHat}, {.step=10, .tone=kFifth, .octave=0, .vel=56, .gate=kGateHat}, {.step=12, .tone=kThird, .octave=1, .vel=60, .gate=kGateHat}, {.step=14, .tone=kSeventh, .octave=0, .vel=56, .gate=kGateHat}};
inline constexpr StyleEvent kPercShake[] = {{.step=0, .tone=kMaracas, .octave=0, .vel=56, .gate=kGateHat}, {.step=2, .tone=kMaracas, .octave=0, .vel=46, .gate=kGateHat}, {.step=4, .tone=kMaracas, .octave=0, .vel=56, .gate=kGateHat}, {.step=6, .tone=kMaracas, .octave=0, .vel=46, .gate=kGateHat}, {.step=8, .tone=kMaracas, .octave=0, .vel=56, .gate=kGateHat}, {.step=10, .tone=kMaracas, .octave=0, .vel=46, .gate=kGateHat}, {.step=12, .tone=kMaracas, .octave=0, .vel=56, .gate=kGateHat}, {.step=14, .tone=kMaracas, .octave=0, .vel=46, .gate=kGateHat}};
inline constexpr StyleEvent kPercRich[] = {{.step=0, .tone=kShortGuiro, .octave=0, .vel=62, .gate=kGateHat}, {.step=4, .tone=kShortGuiro, .octave=0, .vel=58, .gate=kGateHat}, {.step=8, .tone=kShortGuiro, .octave=0, .vel=62, .gate=kGateHat}, {.step=12, .tone=kShortGuiro, .octave=0, .vel=58, .gate=kGateHat}, {.step=2, .tone=kCowbell, .octave=0, .vel=54, .gate=kGateHat}, {.step=6, .tone=kCowbell, .octave=0, .vel=50, .gate=kGateHat}, {.step=10, .tone=kCowbell, .octave=0, .vel=54, .gate=kGateHat}, {.step=14, .tone=kCowbell, .octave=0, .vel=50, .gate=kGateHat}};
// Motif engine (9210 Wave 1, style-depth program task A): extend variation
// onto drums/chord1/chord2/pad, wired onto the peak variation (varD) plus the
// shared reused Pad/nylon-comp arrays. kDisplacement for drums/pad/chord2 (a
// soft evolving push); kRetrograde for the chord1 comp (a legible mirrored
// answer, same idiom already proven on the syncopated bass above).
inline constexpr MotifSpec kPeakDrumsMotif{.transform = MotifTransform::kDisplacement, .seed = 1310};
inline constexpr MotifSpec kPeakChordMotif{.transform = MotifTransform::kRetrograde, .seed = 1311};
inline constexpr MotifSpec kSharedPadMotif{.transform = MotifTransform::kDisplacement, .seed = 1312};
inline constexpr MotifSpec kSharedChord2Motif{.transform = MotifTransform::kDisplacement, .seed = 1313};
// Intro1 (style-depth Wave-2 C): a genuine 2-bar build. Bar 1 (unchanged) is
// the sparse agogo pickup on the last beat; bar 2 is the arrival -- the
// surdo kick enters on the backbeat under a fuller agogo bell line, landing
// the band right where Intro2/VarA starts.
inline constexpr StyleEvent kIn1D[] = {
    {.step=8, .tone=kHiAgogo, .octave=0, .vel=70, .gate=50}, {.step=10, .tone=kLoAgogo, .octave=0, .vel=64, .gate=50}, {.step=12, .tone=kHiAgogo, .octave=0, .vel=76, .gate=50}, {.step=14, .tone=kLoAgogo, .octave=0, .vel=80, .gate=50},
    // bar 2: arrival -- surdo kick lands on the backbeat, agogo fills in.
    {.step=16, .tone=kHiAgogo, .octave=0, .vel=72, .gate=50}, {.step=18, .tone=kLoAgogo, .octave=0, .vel=64, .gate=50},
    {.step=20, .tone=kKick, .octave=0, .vel=100, .gate=50}, {.step=22, .tone=kHiAgogo, .octave=0, .vel=76, .gate=50},
    {.step=24, .tone=kLoAgogo, .octave=0, .vel=68, .gate=50}, {.step=26, .tone=kHiAgogo, .octave=0, .vel=80, .gate=50},
    {.step=28, .tone=kKick, .octave=0, .vel=104, .gate=50}, {.step=30, .tone=kLoAgogo, .octave=0, .vel=84, .gate=50},
};
inline constexpr StylePattern kIn1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)}};
inline constexpr StyleEvent kIn2D[] = {{.step=4, .tone=kKick, .octave=0, .vel=100, .gate=50}, {.step=12, .tone=kKick, .octave=0, .vel=104, .gate=50}, {.step=0, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=2, .tone=kTambourine, .octave=0, .vel=54, .gate=50}, {.step=4, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=6, .tone=kTambourine, .octave=0, .vel=54, .gate=50}, {.step=8, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=10, .tone=kTambourine, .octave=0, .vel=54, .gate=50}, {.step=12, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=14, .tone=kTambourine, .octave=0, .vel=54, .gate=50}};
// Cavaquinho/guitar chord stabs on the offbeats become ONE gesture event each
// (D42): gesture::expand ignores the authored tone and brushes the whole live
// chord low->high (kStrumUp) / high->low (kStrumDown), adapting to triad vs 7th.
inline constexpr StyleEvent kIn2C[] = {{.step=2, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStaccato, .gesture=ChordGesture::kStrumUp}, {.step=10, .tone=kRoot, .octave=0, .vel=74, .gate=kGateStaccato, .gesture=ChordGesture::kStrumDown}};
inline constexpr StylePattern kIn2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSyncBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIn2C)}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}};
// VarA (style-depth Wave-2 C): genuinely 2 bars now. Bar 1 is unchanged. Bar 2
// keeps the kick+tamborim bed shifted verbatim (the steady surdo pulse, same
// idiom as basic's re-triggered pad -- not this bar's own variation); the
// REAL bar-2 idea is the agogo bell answering with a denser call-and-response
// line. The syncopated bass (kSyncBass) is motif-locked (kSyncBassMotif) and
// is samba's only bass idiom, so it is deliberately left untouched here
// (guardrail: vary drums/perc, not the bass).
inline constexpr StyleEvent kAD[] = {
    {.step=4, .tone=kKick, .octave=0, .vel=108, .gate=50}, {.step=12, .tone=kKick, .octave=0, .vel=112, .gate=50}, {.step=0, .tone=kKick, .octave=0, .vel=76, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=76, .gate=50},
    {.step=0, .tone=kTambourine, .octave=0, .vel=64, .gate=50}, {.step=1, .tone=kTambourine, .octave=0, .vel=48, .gate=50}, {.step=2, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=3, .tone=kTambourine, .octave=0, .vel=48, .gate=50}, {.step=4, .tone=kTambourine, .octave=0, .vel=64, .gate=50}, {.step=5, .tone=kTambourine, .octave=0, .vel=48, .gate=50}, {.step=6, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=7, .tone=kTambourine, .octave=0, .vel=48, .gate=50}, {.step=8, .tone=kTambourine, .octave=0, .vel=64, .gate=50}, {.step=9, .tone=kTambourine, .octave=0, .vel=48, .gate=50}, {.step=10, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=11, .tone=kTambourine, .octave=0, .vel=48, .gate=50}, {.step=12, .tone=kTambourine, .octave=0, .vel=64, .gate=50}, {.step=13, .tone=kTambourine, .octave=0, .vel=48, .gate=50}, {.step=14, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=15, .tone=kTambourine, .octave=0, .vel=48, .gate=50},
    {.step=0, .tone=kHiAgogo, .octave=0, .vel=72, .gate=50}, {.step=3, .tone=kLoAgogo, .octave=0, .vel=64, .gate=50}, {.step=6, .tone=kHiAgogo, .octave=0, .vel=68, .gate=50}, {.step=8, .tone=kHiAgogo, .octave=0, .vel=72, .gate=50}, {.step=11, .tone=kLoAgogo, .octave=0, .vel=64, .gate=50}, {.step=14, .tone=kHiAgogo, .octave=0, .vel=68, .gate=50},
    // bar 2: kick+tamborim bed repeats verbatim (shifted +16); the agogo
    // bell answers with a denser, differently-ordered call-and-response.
    {.step=20, .tone=kKick, .octave=0, .vel=112, .gate=50}, {.step=28, .tone=kKick, .octave=0, .vel=116, .gate=50}, {.step=16, .tone=kKick, .octave=0, .vel=84, .gate=50}, {.step=24, .tone=kKick, .octave=0, .vel=84, .gate=50},
    {.step=16, .tone=kTambourine, .octave=0, .vel=64, .gate=50}, {.step=17, .tone=kTambourine, .octave=0, .vel=48, .gate=50}, {.step=18, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=19, .tone=kTambourine, .octave=0, .vel=48, .gate=50}, {.step=20, .tone=kTambourine, .octave=0, .vel=64, .gate=50}, {.step=21, .tone=kTambourine, .octave=0, .vel=48, .gate=50}, {.step=22, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=23, .tone=kTambourine, .octave=0, .vel=48, .gate=50}, {.step=24, .tone=kTambourine, .octave=0, .vel=64, .gate=50}, {.step=25, .tone=kTambourine, .octave=0, .vel=48, .gate=50}, {.step=26, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=27, .tone=kTambourine, .octave=0, .vel=48, .gate=50}, {.step=28, .tone=kTambourine, .octave=0, .vel=64, .gate=50}, {.step=29, .tone=kTambourine, .octave=0, .vel=48, .gate=50}, {.step=30, .tone=kTambourine, .octave=0, .vel=58, .gate=50}, {.step=31, .tone=kTambourine, .octave=0, .vel=48, .gate=50},
    {.step=16, .tone=kLoAgogo, .octave=0, .vel=68, .gate=50}, {.step=19, .tone=kHiAgogo, .octave=0, .vel=72, .gate=50}, {.step=22, .tone=kLoAgogo, .octave=0, .vel=70, .gate=50}, {.step=24, .tone=kLoAgogo, .octave=0, .vel=68, .gate=50}, {.step=27, .tone=kHiAgogo, .octave=0, .vel=76, .gate=50}, {.step=30, .tone=kHiAgogo, .octave=0, .vel=82, .gate=50}, {.step=31, .tone=kLoAgogo, .octave=0, .vel=78, .gate=50},
};
inline constexpr StyleEvent kAC[] = {
    {.step=2, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato, .gesture=ChordGesture::kStrumUp}, {.step=6, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStaccato, .gesture=ChordGesture::kStrumDown}, {.step=10, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato, .gesture=ChordGesture::kStrumUp}, {.step=14, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStaccato, .gesture=ChordGesture::kStrumDown},
    // bar 2: the cavaquinho comp answers syncopated, off the same offbeat
    // slots, landing a final strum on the last 16th (turnaround).
    {.step=18, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato, .gesture=ChordGesture::kStrumUp}, {.step=22, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStaccato, .gesture=ChordGesture::kStrumDown}, {.step=26, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato, .gesture=ChordGesture::kStrumUp}, {.step=31, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStaccato, .gesture=ChordGesture::kStrumDown},
};
inline constexpr StylePattern kAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSyncBass), .motif=&kSyncBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAC)}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kChord2Sync), .gm_program=kChord2Voice}};
// VarB left at 1 bar (style-depth Wave-2 C): no bar-2 idea distinct from
// VarA's own turnaround without touching the motif-locked syncopated bass.
inline constexpr StyleEvent kBD[] = {
    {.step=4, .tone=kKick, .octave=0, .vel=112, .gate=50}, {.step=12, .tone=kKick, .octave=0, .vel=116, .gate=50}, {.step=0, .tone=kKick, .octave=0, .vel=84, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=84, .gate=50}, {.step=7, .tone=kKick, .octave=0, .vel=70, .gate=50}, {.step=15, .tone=kKick, .octave=0, .vel=70, .gate=50},
    {.step=0, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=1, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=2, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=3, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=4, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=5, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=6, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=7, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=8, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=9, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=10, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=11, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=12, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=13, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=14, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=15, .tone=kTambourine, .octave=0, .vel=50, .gate=50},
    {.step=2, .tone=kOpenHiConga, .octave=0, .vel=70, .gate=50}, {.step=6, .tone=kLoConga, .octave=0, .vel=72, .gate=50}, {.step=10, .tone=kOpenHiConga, .octave=0, .vel=70, .gate=50}, {.step=14, .tone=kLoConga, .octave=0, .vel=74, .gate=50},
};
inline constexpr StyleEvent kBC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStaccato, .gesture=ChordGesture::kStrumUp}, {.step=3, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStaccato, .gesture=ChordGesture::kStrumDown}, {.step=6, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato, .gesture=ChordGesture::kStrumUp}, {.step=10, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato, .gesture=ChordGesture::kStrumDown}, {.step=13, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStaccato, .gesture=ChordGesture::kStrumUp}, {.step=14, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStaccato, .gesture=ChordGesture::kStrumDown}};
inline constexpr StylePattern kBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSyncBass), .motif=&kSyncBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBC)}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kChord2Sync), .gm_program=kChord2Voice}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercShake)}};
inline constexpr StyleEvent kFAD[] = {{.step=0, .tone=kKick, .octave=0, .vel=100, .gate=50}, {.step=4, .tone=kTomHi, .octave=0, .vel=90, .gate=100}, {.step=6, .tone=kTomHi, .octave=0, .vel=86, .gate=100}, {.step=8, .tone=kTomMid, .octave=0, .vel=94, .gate=100}, {.step=10, .tone=kTomMid, .octave=0, .vel=90, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=100, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=106, .gate=100}};
inline constexpr StyleEvent kFBD[] = {{.step=0, .tone=kTomHi, .octave=0, .vel=90, .gate=100}, {.step=2, .tone=kTomHi, .octave=0, .vel=86, .gate=100}, {.step=4, .tone=kTomMid, .octave=0, .vel=94, .gate=100}, {.step=6, .tone=kTomMid, .octave=0, .vel=90, .gate=100}, {.step=8, .tone=kTomLow, .octave=0, .vel=98, .gate=100}, {.step=10, .tone=kTomLow, .octave=0, .vel=94, .gate=100}, {.step=12, .tone=kTomFloor, .octave=0, .vel=104, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=108, .gate=100}};
inline constexpr StyleEvent kFCD[] = {{.step=0, .tone=kOpenHiConga, .octave=0, .vel=88, .gate=50}, {.step=2, .tone=kLoConga, .octave=0, .vel=84, .gate=50}, {.step=4, .tone=kOpenHiConga, .octave=0, .vel=90, .gate=50}, {.step=6, .tone=kLoConga, .octave=0, .vel=86, .gate=50}, {.step=8, .tone=kTomHi, .octave=0, .vel=96, .gate=50}, {.step=10, .tone=kTomMid, .octave=0, .vel=100, .gate=50}, {.step=12, .tone=kTomLow, .octave=0, .vel=104, .gate=50}, {.step=14, .tone=kTomFloor, .octave=0, .vel=110, .gate=50}};
inline constexpr StyleEvent kFDD[] = {{.step=0, .tone=kTomHi, .octave=0, .vel=90, .gate=50}, {.step=1, .tone=kTomHi, .octave=0, .vel=86, .gate=50}, {.step=2, .tone=kTomMid, .octave=0, .vel=92, .gate=50}, {.step=3, .tone=kTomMid, .octave=0, .vel=88, .gate=50}, {.step=4, .tone=kTomLow, .octave=0, .vel=96, .gate=50}, {.step=5, .tone=kTomLow, .octave=0, .vel=92, .gate=50}, {.step=6, .tone=kTomFloor, .octave=0, .vel=100, .gate=50}, {.step=7, .tone=kTomFloor, .octave=0, .vel=96, .gate=50}, {.step=8, .tone=kSnare, .octave=0, .vel=102, .gate=50}, {.step=9, .tone=kSnare, .octave=0, .vel=98, .gate=50}, {.step=10, .tone=kSnare, .octave=0, .vel=104, .gate=50}, {.step=11, .tone=kSnare, .octave=0, .vel=100, .gate=50}, {.step=12, .tone=kSnare, .octave=0, .vel=108, .gate=50}, {.step=13, .tone=kSnare, .octave=0, .vel=112, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=116, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=120, .gate=kGate8th}};
inline constexpr StylePattern kFAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSyncBass), .motif=&kSyncBassMotif}};
inline constexpr StylePattern kFBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSyncBass), .motif=&kSyncBassMotif}};
inline constexpr StylePattern kFCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSyncBass), .motif=&kSyncBassMotif}};
inline constexpr StylePattern kFDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSyncBass), .motif=&kSyncBassMotif}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercRich)}};
inline constexpr StyleEvent kE1D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=100, .gate=kGateHalfBar}, {.step=0, .tone=kKick, .octave=0, .vel=104, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=96, .gate=50}};
inline constexpr StyleEvent kE1B[] = {{.step=0, .tone=kRoot, .octave=0, .vel=100, .gate=kGateHeld}};
inline constexpr StyleEvent kE1C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=84, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=84, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=84, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateHeld}};
inline constexpr StylePattern kE1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1C)}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}};
inline constexpr StyleEvent kE2D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=110, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=108, .gate=50}, {.step=4, .tone=kKick, .octave=0, .vel=100, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=104, .gate=50}, {.step=12, .tone=kKick, .octave=0, .vel=100, .gate=50}};
inline constexpr StyleEvent kE2B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=104, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=94, .gate=kGateHeld}};
inline constexpr StyleEvent kE2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=86, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=86, .gate=kGateHeld}};
inline constexpr StylePattern kE2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2C)}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}};
// varC: batucada breakdown — surdo backbeat and agogo clave only, tamborim dropped.
// VarC left at 1 bar (style-depth Wave-2 C): the batucada breakdown is a
// single deliberate held gesture -- a second bar would just repeat it.
inline constexpr StyleEvent kCD[] = {{.step=4, .tone=kKick, .octave=0, .vel=108, .gate=50}, {.step=12, .tone=kKick, .octave=0, .vel=112, .gate=50}, {.step=0, .tone=kHiAgogo, .octave=0, .vel=72, .gate=50}, {.step=3, .tone=kLoAgogo, .octave=0, .vel=64, .gate=50}, {.step=6, .tone=kHiAgogo, .octave=0, .vel=68, .gate=50}, {.step=8, .tone=kHiAgogo, .octave=0, .vel=72, .gate=50}, {.step=11, .tone=kLoAgogo, .octave=0, .vel=64, .gate=50}, {.step=14, .tone=kHiAgogo, .octave=0, .vel=68, .gate=50}};
inline constexpr StyleEvent kCC[] = {{.step=2, .tone=kRoot, .octave=0, .vel=78, .gate=kGateStaccato, .gesture=ChordGesture::kStrumUp}, {.step=10, .tone=kRoot, .octave=0, .vel=76, .gate=kGateStaccato, .gesture=ChordGesture::kStrumDown}};
inline constexpr StylePattern kCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSyncBass), .motif=&kSyncBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCC)}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadRehit), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead, .motif=&kSharedPadMotif}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kChord2Off), .gm_program=kChord2Voice, .motif=&kSharedChord2Motif}, {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArp8), .gm_program=kArpVoice}};
// varD: peak escola — full surdo, 16th tamborim, congas and agogo, full 7th stabs.
// VarD left at 1 bar (style-depth Wave-2 C): already the style's fullest,
// busiest bar (motif-driven drums/chord1/lead); no clean additional idea.
inline constexpr StyleEvent kDD[] = {{.step=4, .tone=kKick, .octave=0, .vel=114, .gate=50}, {.step=12, .tone=kKick, .octave=0, .vel=118, .gate=50}, {.step=0, .tone=kKick, .octave=0, .vel=84, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=84, .gate=50}, {.step=7, .tone=kKick, .octave=0, .vel=70, .gate=50}, {.step=15, .tone=kKick, .octave=0, .vel=70, .gate=50}, {.step=0, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=1, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=2, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=3, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=4, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=5, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=6, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=7, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=8, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=9, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=10, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=11, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=12, .tone=kTambourine, .octave=0, .vel=66, .gate=50}, {.step=13, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=14, .tone=kTambourine, .octave=0, .vel=60, .gate=50}, {.step=15, .tone=kTambourine, .octave=0, .vel=50, .gate=50}, {.step=2, .tone=kOpenHiConga, .octave=0, .vel=72, .gate=50}, {.step=6, .tone=kLoConga, .octave=0, .vel=74, .gate=50}, {.step=10, .tone=kOpenHiConga, .octave=0, .vel=72, .gate=50}, {.step=14, .tone=kLoConga, .octave=0, .vel=76, .gate=50}, {.step=0, .tone=kHiAgogo, .octave=0, .vel=74, .gate=50}, {.step=8, .tone=kHiAgogo, .octave=0, .vel=74, .gate=50}};
inline constexpr StyleEvent kDC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStaccato, .gesture=ChordGesture::kStrumUp}, {.step=6, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato, .gesture=ChordGesture::kStrumDown}, {.step=10, .tone=kRoot, .octave=0, .vel=82, .gate=kGateStaccato, .gesture=ChordGesture::kStrumUp}, {.step=14, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStaccato, .gesture=ChordGesture::kStrumDown}};
inline constexpr StylePattern kDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kDD), .motif=&kPeakDrumsMotif}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kSyncBass), .motif=&kSyncBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kDC), .motif=&kPeakChordMotif}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadRehit), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead, .motif=&kSharedPadMotif}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kChord2Off), .gm_program=kChord2Voice, .motif=&kSharedChord2Motif}, {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArp8), .gm_program=kArpVoice}, {.role=TrackRole::kLead, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(), .gm_program=kLeadVoice, .motif=&kLeadMotif}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercRich)}};
inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=2, .patterns=Span<const StylePattern>(kIn1P)}, {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIn2P)},
    {.type=SectionType::kVarA, .bars=2, .patterns=Span<const StylePattern>(kAP)}, {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kBP)},
    {.type=SectionType::kVarC, .bars=1, .patterns=Span<const StylePattern>(kCP)}, {.type=SectionType::kVarD, .bars=1, .patterns=Span<const StylePattern>(kDP)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFAP)}, {.type=SectionType::kFillB, .bars=1, .patterns=Span<const StylePattern>(kFBP)}, {.type=SectionType::kFillC, .bars=1, .patterns=Span<const StylePattern>(kFCP)}, {.type=SectionType::kFillD, .bars=1, .patterns=Span<const StylePattern>(kFDP)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kE1P)}, {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kE2P)},
};
// Groove (9100 Wave 1, task B): percussive, syncopated groove -- a mid-weight
// deterministic humanize (a touch more than bossa's smooth 2-feel).
inline constexpr Style kStyle{.name="samba", .sections=Span<const StyleSection>(kSections),
                              .groove={.humanize_timing=10, .humanize_velocity=20}, .tempo=10400};
}  // namespace samba

}  // namespace styles
}  // namespace arrangrr
