#pragma once

#include "arrangrr/arranger/style_model.hpp"

namespace arrangrr {
namespace styles {

// ---------------------------------------------------------------------------
// Built-in demo style "basic": 4/4, one-bar sections, three roles.
// GM drums on the fixed role: kick 36, snare 38, closed hat 42, crash 49.
namespace basic {

// ---------------------------------------------------------------------------
// Fuller-band roles (ADDITIVE): a sustained pad bed, a complementary second
// comp voice, a high arpeggio and extra percussion. Shared event arrays keep
// the table compact; each section binds the ones that fit its energy.
// kPad -> GM 89 (Pad2 warm), kChord2 -> GM 4 (E.Piano1), kArp -> GM 10 (Music
// Box); kPerc rides the drum channel (gm_program stays -1).
inline constexpr std::int16_t kPadVoice = 89;
inline constexpr std::int16_t kChord2Voice = 4;
inline constexpr std::int16_t kArpVoice = 10;
// Pad: hold the triad (register 48) under the whole bar. Re-articulated at
// step 16 too (style-depth Wave-2 C): VarA is now 2 bars, and the sustain
// re-triggers every bar exactly as it always has when the section looped --
// a background hold, not the section's own bar-to-bar variation.
inline constexpr StyleEvent kPadTriad[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=58, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=56, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=58, .gate=kGateHeld},
    {.step=16, .tone=kRoot, .octave=0, .vel=58, .gate=kGateHeld}, {.step=16, .tone=kThird, .octave=0, .vel=56, .gate=kGateHeld}, {.step=16, .tone=kFifth, .octave=0, .vel=58, .gate=kGateHeld},
};
// Pad with a 7th for the bigger sections; re-hit at the half bar for movement.
inline constexpr StyleEvent kPad7[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=60, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=58, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=60, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=54, .gate=kGateHeld},
};
inline constexpr StyleEvent kPadRehit[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=60, .gate=kGateHalfBar}, {.step=0, .tone=kThird, .octave=0, .vel=58, .gate=kGateHalfBar}, {.step=0, .tone=kFifth, .octave=0, .vel=60, .gate=kGateHalfBar}, {.step=0, .tone=kSeventh, .octave=0, .vel=54, .gate=kGateHalfBar},
    {.step=8, .tone=kRoot, .octave=0, .vel=58, .gate=kGateHalfBar}, {.step=8, .tone=kThird, .octave=0, .vel=56, .gate=kGateHalfBar}, {.step=8, .tone=kFifth, .octave=0, .vel=58, .gate=kGateHalfBar}, {.step=8, .tone=kSeventh, .octave=0, .vel=52, .gate=kGateHalfBar},
};
// Chord2: complementary comp. Beats 2 & 4 when kChord1 hits 1 & 3 (varA);
// on the beat when kChord1 is off-beat (varB); light upper triad on the &'s.
// Bar 2 (style-depth Wave-2 C, VarA only -- VarB/VarC still read bar 1):
// the same comp answers syncopated onto the & of 2/4 instead of square on the
// beat, a genuine call-and-response rather than a repeat of bar 1.
inline constexpr StyleEvent kChord2Beat24[] = {
    {.step=4, .tone=kRoot, .octave=0, .vel=62, .gate=kGateStab}, {.step=4, .tone=kThird, .octave=0, .vel=62, .gate=kGateStab}, {.step=4, .tone=kFifth, .octave=0, .vel=62, .gate=kGateStab},
    {.step=12, .tone=kRoot, .octave=0, .vel=60, .gate=kGateStab}, {.step=12, .tone=kThird, .octave=0, .vel=60, .gate=kGateStab}, {.step=12, .tone=kFifth, .octave=0, .vel=60, .gate=kGateStab},
    {.step=22, .tone=kRoot, .octave=0, .vel=62, .gate=kGateStab}, {.step=22, .tone=kThird, .octave=0, .vel=62, .gate=kGateStab}, {.step=22, .tone=kFifth, .octave=0, .vel=62, .gate=kGateStab},
    {.step=30, .tone=kRoot, .octave=0, .vel=60, .gate=kGateStab}, {.step=30, .tone=kThird, .octave=0, .vel=60, .gate=kGateStab}, {.step=30, .tone=kFifth, .octave=0, .vel=60, .gate=kGateStab},
};
inline constexpr StyleEvent kChord2Beat13[] = {
    {.step=0, .tone=kThird, .octave=0, .vel=60, .gate=kGateBeat}, {.step=0, .tone=kFifth, .octave=0, .vel=60, .gate=kGateBeat},
    {.step=8, .tone=kThird, .octave=0, .vel=58, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=58, .gate=kGateBeat},
};
inline constexpr StyleEvent kChord2Off16[] = {
    {.step=2, .tone=kThird, .octave=0, .vel=58, .gate=kGateStab}, {.step=2, .tone=kFifth, .octave=0, .vel=58, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=56, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=56, .gate=kGateStab},
    {.step=10, .tone=kThird, .octave=0, .vel=58, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=58, .gate=kGateStab}, {.step=14, .tone=kThird, .octave=0, .vel=56, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=56, .gate=kGateStab},
};
// Arp: high 8th-note figure (register 72), up then back down the chord.
inline constexpr StyleEvent kArp8[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=66, .gate=kGateHat}, {.step=2, .tone=kThird, .octave=0, .vel=60, .gate=kGateHat}, {.step=4, .tone=kFifth, .octave=0, .vel=64, .gate=kGateHat}, {.step=6, .tone=kSeventh, .octave=0, .vel=60, .gate=kGateHat},
    {.step=8, .tone=kRoot, .octave=1, .vel=66, .gate=kGateHat}, {.step=10, .tone=kSeventh, .octave=0, .vel=60, .gate=kGateHat}, {.step=12, .tone=kFifth, .octave=0, .vel=64, .gate=kGateHat}, {.step=14, .tone=kThird, .octave=0, .vel=60, .gate=kGateHat},
};
// Motif engine (9210, Ottorino RANK 16): the arp, reused identically across
// its two instances. kDisplacement. `basic` is the neutral scaffold style,
// not a real genre with a signature to protect — wiring it is for
// consistency (every built-in style demonstrates the engine), not measured
// musical urgency.
inline constexpr MotifSpec kArp8Motif{.transform = MotifTransform::kDisplacement, .seed = 1602};
// Perc: tambourine on the backbeat; a cabasa 8th bed for the busy sections.
inline constexpr StyleEvent kPercTamb[] = {
    {.step=4, .tone=kTambourine, .octave=0, .vel=80, .gate=kGateHat}, {.step=12, .tone=kTambourine, .octave=0, .vel=82, .gate=kGateHat},
};
inline constexpr StyleEvent kPercShake[] = {
    {.step=0, .tone=kCabasa, .octave=0, .vel=64, .gate=kGateHat}, {.step=2, .tone=kCabasa, .octave=0, .vel=54, .gate=kGateHat}, {.step=4, .tone=kTambourine, .octave=0, .vel=80, .gate=kGateHat}, {.step=6, .tone=kCabasa, .octave=0, .vel=54, .gate=kGateHat},
    {.step=8, .tone=kCabasa, .octave=0, .vel=64, .gate=kGateHat}, {.step=10, .tone=kCabasa, .octave=0, .vel=54, .gate=kGateHat}, {.step=12, .tone=kTambourine, .octave=0, .vel=82, .gate=kGateHat}, {.step=14, .tone=kCabasa, .octave=0, .vel=54, .gate=kGateHat},
};

// Motif engine (9210 Wave 1, style-depth program task A): extend per-repetition
// variation from bass/arp/lead onto drums/chord1/chord2/pad, wired onto the
// peak variation (varD) plus the shared reused Pad array -- same "wire the
// most-heard instance" granularity the original bass/arp/lead pass used.
// kDisplacement for drums/pad (a soft evolving push); kRetrograde for the
// chord1 comp stabs (a legible mirrored answer, same idiom already proven on
// the bass riffs). `basic` has no genre anchor to protect (no clave/one-drop),
// so every lane is safe to wire.
inline constexpr MotifSpec kPeakDrumsMotif{.transform = MotifTransform::kDisplacement, .seed = 1610};
inline constexpr MotifSpec kPeakChordMotif{.transform = MotifTransform::kRetrograde, .seed = 1611};
inline constexpr MotifSpec kSharedPadMotif{.transform = MotifTransform::kDisplacement, .seed = 1612};
inline constexpr MotifSpec kSharedChord2Motif{.transform = MotifTransform::kDisplacement, .seed = 1613};

// VarA (style-depth Wave-2 C): genuinely 2 bars now. Bar 1 is untouched (the
// pinned zero-jitter probes in test_fx.cpp/test_dual_arp_collision.cpp/
// test_engine_fire_order.cpp only ever read bar 1); bar 2 is a real
// turnaround, not a copy -- an open-hat lift on the last 8th (drums), an
// octave-up push then a leading 7th (bass; the bar-2 DOWNBEAT stays the root
// so test_engine_fire_order's chord-seq-before-arranger G-downbeat pin still
// resolves the same way), and a syncopated comp answer (chord1, off the beat
// instead of square on it).
inline constexpr StyleEvent kVarADrums[] = {
    {.step=0, .tone=36, .octave=0, .vel=110, .gate=120}, {.step=4, .tone=38, .octave=0, .vel=100, .gate=120}, {.step=8, .tone=36, .octave=0, .vel=105, .gate=120}, {.step=12, .tone=38, .octave=0, .vel=100, .gate=120},
    {.step=0, .tone=42, .octave=0, .vel=70, .gate=60},   {.step=2, .tone=42, .octave=0, .vel=60, .gate=60},   {.step=4, .tone=42, .octave=0, .vel=70, .gate=60},   {.step=6, .tone=42, .octave=0, .vel=60, .gate=60},
    {.step=8, .tone=42, .octave=0, .vel=70, .gate=60},   {.step=10, .tone=42, .octave=0, .vel=60, .gate=60},  {.step=12, .tone=42, .octave=0, .vel=70, .gate=60},  {.step=14, .tone=42, .octave=0, .vel=60, .gate=60},
    // bar 2: same backbeat, but the hat opens on the last 8th (turnaround).
    {.step=16, .tone=36, .octave=0, .vel=110, .gate=120}, {.step=20, .tone=38, .octave=0, .vel=100, .gate=120}, {.step=24, .tone=36, .octave=0, .vel=105, .gate=120}, {.step=28, .tone=38, .octave=0, .vel=100, .gate=120},
    {.step=16, .tone=42, .octave=0, .vel=70, .gate=60},  {.step=18, .tone=42, .octave=0, .vel=60, .gate=60},  {.step=20, .tone=42, .octave=0, .vel=70, .gate=60},  {.step=22, .tone=42, .octave=0, .vel=60, .gate=60},
    {.step=24, .tone=42, .octave=0, .vel=70, .gate=60},  {.step=26, .tone=42, .octave=0, .vel=60, .gate=60},  {.step=28, .tone=42, .octave=0, .vel=70, .gate=60},  {.step=30, .tone=46, .octave=0, .vel=88, .gate=90},
};
inline constexpr StyleEvent kVarABass[] = {
    {.step=0, .tone=0, .octave=0, .vel=100, .gate=220},  // root
    {.step=4, .tone=2, .octave=0, .vel=85, .gate=220},   // fifth
    {.step=8, .tone=0, .octave=0, .vel=95, .gate=220},
    {.step=12, .tone=2, .octave=0, .vel=85, .gate=220},
    // bar 2: root (downbeat -- kept for the fire-order chord-seq pin), fifth,
    // an octave-up push, then the 7th leading back into the repeat.
    {.step=16, .tone=0, .octave=0, .vel=100, .gate=220},
    {.step=20, .tone=2, .octave=0, .vel=85, .gate=220},
    {.step=24, .tone=0, .octave=1, .vel=95, .gate=220},
    {.step=28, .tone=3, .octave=0, .vel=88, .gate=220},
};
inline constexpr StyleEvent kVarAChord[] = {
    // Comp stabs on beats 1 and 3: full stack (tones 0..3).
    {.step=0, .tone=0, .octave=0, .vel=80, .gate=360}, {.step=0, .tone=1, .octave=0, .vel=80, .gate=360}, {.step=0, .tone=2, .octave=0, .vel=80, .gate=360}, {.step=0, .tone=3, .octave=0, .vel=80, .gate=360},
    {.step=8, .tone=0, .octave=0, .vel=75, .gate=360}, {.step=8, .tone=1, .octave=0, .vel=75, .gate=360}, {.step=8, .tone=2, .octave=0, .vel=75, .gate=360}, {.step=8, .tone=3, .octave=0, .vel=75, .gate=360},
    // bar 2: the comp answers syncopated -- off the beat instead of on it.
    {.step=18, .tone=0, .octave=0, .vel=78, .gate=360}, {.step=18, .tone=1, .octave=0, .vel=78, .gate=360}, {.step=18, .tone=2, .octave=0, .vel=78, .gate=360}, {.step=18, .tone=3, .octave=0, .vel=78, .gate=360},
    {.step=26, .tone=0, .octave=0, .vel=74, .gate=360}, {.step=26, .tone=1, .octave=0, .vel=74, .gate=360}, {.step=26, .tone=2, .octave=0, .vel=74, .gate=360}, {.step=26, .tone=3, .octave=0, .vel=74, .gate=360},
};
inline constexpr StylePattern kVarAPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarADrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarABass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarAChord)},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice},
    {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kChord2Beat24), .gm_program=kChord2Voice},
};

inline constexpr StyleEvent kVarBDrums[] = {
    {.step=0, .tone=36, .octave=0, .vel=115, .gate=120},  {.step=4, .tone=38, .octave=0, .vel=105, .gate=120}, {.step=7, .tone=36, .octave=0, .vel=90, .gate=120}, {.step=8, .tone=36, .octave=0, .vel=110, .gate=120},
    {.step=12, .tone=38, .octave=0, .vel=105, .gate=120}, {.step=0, .tone=42, .octave=0, .vel=75, .gate=50},   {.step=1, .tone=42, .octave=0, .vel=55, .gate=50},  {.step=2, .tone=42, .octave=0, .vel=65, .gate=50},
    {.step=3, .tone=42, .octave=0, .vel=55, .gate=50},    {.step=4, .tone=42, .octave=0, .vel=75, .gate=50},   {.step=5, .tone=42, .octave=0, .vel=55, .gate=50},  {.step=6, .tone=42, .octave=0, .vel=65, .gate=50},
    {.step=7, .tone=42, .octave=0, .vel=55, .gate=50},    {.step=8, .tone=42, .octave=0, .vel=75, .gate=50},   {.step=9, .tone=42, .octave=0, .vel=55, .gate=50},  {.step=10, .tone=42, .octave=0, .vel=65, .gate=50},
    {.step=11, .tone=42, .octave=0, .vel=55, .gate=50},   {.step=12, .tone=42, .octave=0, .vel=75, .gate=50},  {.step=13, .tone=42, .octave=0, .vel=55, .gate=50}, {.step=14, .tone=42, .octave=0, .vel=65, .gate=50},
    {.step=15, .tone=42, .octave=0, .vel=55, .gate=50},
};
inline constexpr StyleEvent kVarBBass[] = {
    {.step=0, .tone=0, .octave=0, .vel=105, .gate=200}, {.step=2, .tone=0, .octave=0, .vel=70, .gate=100},  {.step=4, .tone=1, .octave=0, .vel=90, .gate=200},
    {.step=8, .tone=2, .octave=0, .vel=95, .gate=200},  {.step=10, .tone=0, .octave=1, .vel=75, .gate=100}, {.step=12, .tone=3, .octave=0, .vel=90, .gate=200},
};
inline constexpr StyleEvent kVarBChord[] = {
    {.step=2, .tone=0, .octave=0, .vel=78, .gate=200},  {.step=2, .tone=1, .octave=0, .vel=78, .gate=200},  {.step=2, .tone=2, .octave=0, .vel=78, .gate=200},  {.step=6, .tone=0, .octave=0, .vel=72, .gate=200},
    {.step=6, .tone=1, .octave=0, .vel=72, .gate=200},  {.step=6, .tone=2, .octave=0, .vel=72, .gate=200},  {.step=10, .tone=0, .octave=0, .vel=78, .gate=200}, {.step=10, .tone=1, .octave=0, .vel=78, .gate=200},
    {.step=10, .tone=2, .octave=0, .vel=78, .gate=200}, {.step=14, .tone=0, .octave=0, .vel=72, .gate=200}, {.step=14, .tone=1, .octave=0, .vel=72, .gate=200}, {.step=14, .tone=2, .octave=0, .vel=72, .gate=200},
};
inline constexpr StylePattern kVarBPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarBDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarBBass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarBChord)},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice},
    {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kChord2Beat13), .gm_program=kChord2Voice},
    {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercTamb)},
};

// Intro1 (style-depth Wave-2 C): a genuine 2-bar build. Bar 1 (unchanged) is
// the sparse hat pickup; bar 2 is the arrival -- crash + backbeat kick/snare
// enter under a full 8th-note hat, landing the band right where VarA starts.
inline constexpr StyleEvent kIntroDrums[] = {
    {.step=8, .tone=42, .octave=0, .vel=60, .gate=60},
    {.step=10, .tone=42, .octave=0, .vel=65, .gate=60},
    {.step=12, .tone=42, .octave=0, .vel=70, .gate=60},
    {.step=14, .tone=42, .octave=0, .vel=80, .gate=60},
    // bar 2: arrival.
    {.step=16, .tone=49, .octave=0, .vel=90, .gate=1800},  // crash announces the downbeat
    {.step=16, .tone=36, .octave=0, .vel=100, .gate=120},
    {.step=20, .tone=38, .octave=0, .vel=92, .gate=120},
    {.step=24, .tone=36, .octave=0, .vel=104, .gate=120},
    {.step=28, .tone=38, .octave=0, .vel=98, .gate=120},
    {.step=16, .tone=42, .octave=0, .vel=66, .gate=60}, {.step=18, .tone=42, .octave=0, .vel=54, .gate=60},
    {.step=20, .tone=42, .octave=0, .vel=66, .gate=60}, {.step=22, .tone=42, .octave=0, .vel=54, .gate=60},
    {.step=24, .tone=42, .octave=0, .vel=66, .gate=60}, {.step=26, .tone=42, .octave=0, .vel=54, .gate=60},
    {.step=28, .tone=42, .octave=0, .vel=66, .gate=60}, {.step=30, .tone=42, .octave=0, .vel=60, .gate=60},
};
inline constexpr StyleEvent kIntroBass[] = {
    {.step=0, .tone=0, .octave=0, .vel=90, .gate=3600},  // held root pickup
    // bar 2: hold the root again, then a short 5th anticipation into VarA.
    {.step=16, .tone=0, .octave=0, .vel=94, .gate=3200},
    {.step=30, .tone=2, .octave=0, .vel=80, .gate=kGate8th},
};
inline constexpr StylePattern kIntroPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIntroDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntroBass)},
};

inline constexpr StyleEvent kFillDrums[] = {
    {.step=0, .tone=36, .octave=0, .vel=110, .gate=120}, {.step=4, .tone=38, .octave=0, .vel=100, .gate=120},  {.step=8, .tone=38, .octave=0, .vel=90, .gate=100},
    {.step=10, .tone=38, .octave=0, .vel=95, .gate=100}, {.step=12, .tone=38, .octave=0, .vel=105, .gate=100}, {.step=14, .tone=38, .octave=0, .vel=115, .gate=100},
};
inline constexpr StyleEvent kFillBass[] = {
    {.step=0, .tone=0, .octave=0, .vel=100, .gate=220},
    {.step=8, .tone=2, .octave=0, .vel=90, .gate=220},
};
// The flat FILL bass, reused identically across all 4 fills; the Var bass
// (kVarABass..kVarDBass) is already 4 distinct hand-authored ideas.
inline constexpr MotifSpec kFillBassMotif{.transform = MotifTransform::kRetrograde, .seed = 1601};
inline constexpr StylePattern kFillPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFillDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kFillBass), .motif=&kFillBassMotif},
};

inline constexpr StyleEvent kEndDrums[] = {
    {.step=0, .tone=36, .octave=0, .vel=115, .gate=120},
    {.step=0, .tone=49, .octave=0, .vel=110, .gate=1800},
};
inline constexpr StyleEvent kEndBass[] = {
    {.step=0, .tone=0, .octave=0, .vel=100, .gate=3600},
};
inline constexpr StyleEvent kEndChord[] = {
    {.step=0, .tone=0, .octave=0, .vel=85, .gate=3600},
    {.step=0, .tone=1, .octave=0, .vel=85, .gate=3600},
    {.step=0, .tone=2, .octave=0, .vel=85, .gate=3600},
    {.step=0, .tone=3, .octave=0, .vel=85, .gate=3600},
};
inline constexpr StylePattern kEndPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kEndDrums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEndBass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEndChord)},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice},
};

// intro2: a fuller one-bar intro — full backbeat over a steady hat bed.
inline constexpr StyleEvent kIntro2Drums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=104, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=100, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=92, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=98, .gate=120},
    {.step=0, .tone=kClosedHat, .octave=0, .vel=70, .gate=60}, {.step=2, .tone=kClosedHat, .octave=0, .vel=58, .gate=60}, {.step=4, .tone=kClosedHat, .octave=0, .vel=70, .gate=60}, {.step=6, .tone=kClosedHat, .octave=0, .vel=58, .gate=60},
    {.step=8, .tone=kClosedHat, .octave=0, .vel=70, .gate=60}, {.step=10, .tone=kClosedHat, .octave=0, .vel=58, .gate=60}, {.step=12, .tone=kClosedHat, .octave=0, .vel=70, .gate=60}, {.step=14, .tone=kClosedHat, .octave=0, .vel=64, .gate=60},
};
inline constexpr StyleEvent kIntro2Bass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=92, .gate=kGateHeld}};
inline constexpr StyleEvent kIntro2Chord[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=76, .gate=kGateBeat}, {.step=0, .tone=kThird, .octave=0, .vel=76, .gate=kGateBeat}, {.step=0, .tone=kFifth, .octave=0, .vel=76, .gate=kGateBeat},
    {.step=8, .tone=kRoot, .octave=0, .vel=74, .gate=kGateBeat}, {.step=8, .tone=kThird, .octave=0, .vel=74, .gate=kGateBeat}, {.step=8, .tone=kFifth, .octave=0, .vel=74, .gate=kGateBeat},
};
inline constexpr StylePattern kIntro2Patterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIntro2Drums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntro2Bass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntro2Chord)},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice},
};

// Fills B..D: progressively busier one-bar tom/snare fills, all shared bass.
inline constexpr StyleEvent kFillBDrums[] = {
    {.step=0, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=100, .gate=120}, {.step=6, .tone=kSnare, .octave=0, .vel=96, .gate=120},
    {.step=8, .tone=kTomHi, .octave=0, .vel=100, .gate=120}, {.step=10, .tone=kTomMid, .octave=0, .vel=104, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=108, .gate=120}, {.step=14, .tone=kTomLow, .octave=0, .vel=112, .gate=120},
};
inline constexpr StyleEvent kFillCDrums[] = {
    {.step=0, .tone=kSnare, .octave=0, .vel=96, .gate=100}, {.step=2, .tone=kSnare, .octave=0, .vel=90, .gate=100}, {.step=4, .tone=kTomHi, .octave=0, .vel=100, .gate=100}, {.step=6, .tone=kTomHi, .octave=0, .vel=98, .gate=100},
    {.step=8, .tone=kTomMid, .octave=0, .vel=104, .gate=100}, {.step=10, .tone=kTomMid, .octave=0, .vel=102, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=110, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=114, .gate=100},
};
inline constexpr StyleEvent kFillDDrums[] = {
    {.step=0, .tone=kSnare, .octave=0, .vel=92, .gate=50}, {.step=1, .tone=kSnare, .octave=0, .vel=86, .gate=50}, {.step=2, .tone=kSnare, .octave=0, .vel=96, .gate=50}, {.step=3, .tone=kSnare, .octave=0, .vel=90, .gate=50},
    {.step=4, .tone=kSnare, .octave=0, .vel=100, .gate=50}, {.step=5, .tone=kSnare, .octave=0, .vel=94, .gate=50}, {.step=6, .tone=kTomHi, .octave=0, .vel=104, .gate=50}, {.step=7, .tone=kTomHi, .octave=0, .vel=98, .gate=50},
    {.step=8, .tone=kTomMid, .octave=0, .vel=106, .gate=50}, {.step=9, .tone=kTomMid, .octave=0, .vel=100, .gate=50}, {.step=10, .tone=kTomLow, .octave=0, .vel=110, .gate=50}, {.step=11, .tone=kTomLow, .octave=0, .vel=104, .gate=50},
    {.step=12, .tone=kTomFloor, .octave=0, .vel=114, .gate=50}, {.step=13, .tone=kTomFloor, .octave=0, .vel=108, .gate=50}, {.step=14, .tone=kSnare, .octave=0, .vel=118, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=120, .gate=240},
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

// ending2: fuller resolving cadence — crash, held chord and bass, mid snare hit.
inline constexpr StyleEvent kEnd2Drums[] = {
    {.step=0, .tone=kCrash, .octave=0, .vel=112, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=116, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=100, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=108, .gate=120},
};
inline constexpr StyleEvent kEnd2Bass[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=104, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=96, .gate=kGateHeld}};
inline constexpr StyleEvent kEnd2Chord[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=90, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=90, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=90, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=90, .gate=kGateHeld},
};
inline constexpr StylePattern kEnd2Patterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kEnd2Drums)},
    {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEnd2Bass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kEnd2Chord)},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice},
};

// varC: half-time feel — backbeat pulled to beat 3, wide space, quarter hats.
inline constexpr StyleEvent kVarCDrums[] = {{.step=0, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=8, .tone=kSnare, .octave=0, .vel=104, .gate=120}, {.step=0, .tone=kClosedHat, .octave=0, .vel=68, .gate=60}, {.step=4, .tone=kClosedHat, .octave=0, .vel=58, .gate=60}, {.step=8, .tone=kClosedHat, .octave=0, .vel=68, .gate=60}, {.step=12, .tone=kClosedHat, .octave=0, .vel=58, .gate=60}};
inline constexpr StyleEvent kVarCBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=96, .gate=kGateHalfBar}, {.step=8, .tone=kFifth, .octave=0, .vel=88, .gate=kGateHalfBar}};
inline constexpr StyleEvent kVarCChord[] = {{.step=0, .tone=kRoot, .octave=0, .vel=74, .gate=kGateHalfBar}, {.step=0, .tone=kThird, .octave=0, .vel=74, .gate=kGateHalfBar}, {.step=0, .tone=kFifth, .octave=0, .vel=74, .gate=kGateHalfBar}, {.step=8, .tone=kThird, .octave=0, .vel=70, .gate=kGateHalfBar}, {.step=8, .tone=kFifth, .octave=0, .vel=70, .gate=kGateHalfBar}, {.step=8, .tone=kSeventh, .octave=0, .vel=70, .gate=kGateHalfBar}};
inline constexpr StylePattern kVarCPatterns[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarCDrums)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarCBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarCChord)},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadRehit), .gm_program=kPadVoice, .motif=&kSharedPadMotif},
    {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kChord2Beat24), .gm_program=kChord2Voice},
    {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArp8), .gm_program=kArpVoice, .motif=&kArp8Motif}};
// varD: peak — driving eighth bass, kick pushes, 16th hats, full four-note stabs.
inline constexpr StyleEvent kVarDDrums[] = {{.step=0, .tone=kCrash, .octave=0, .vel=104, .gate=kGateBeat}, {.step=0, .tone=kKick, .octave=0, .vel=115, .gate=120}, {.step=3, .tone=kKick, .octave=0, .vel=92, .gate=120}, {.step=8, .tone=kKick, .octave=0, .vel=110, .gate=120}, {.step=11, .tone=kKick, .octave=0, .vel=92, .gate=120}, {.step=4, .tone=kSnare, .octave=0, .vel=110, .gate=120}, {.step=12, .tone=kSnare, .octave=0, .vel=110, .gate=120}, {.step=0, .tone=kClosedHat, .octave=0, .vel=78, .gate=50}, {.step=1, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=2, .tone=kClosedHat, .octave=0, .vel=68, .gate=50}, {.step=3, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=78, .gate=50}, {.step=5, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=6, .tone=kClosedHat, .octave=0, .vel=68, .gate=50}, {.step=7, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=78, .gate=50}, {.step=9, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=68, .gate=50}, {.step=11, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=78, .gate=50}, {.step=13, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}, {.step=14, .tone=kClosedHat, .octave=0, .vel=68, .gate=50}, {.step=15, .tone=kClosedHat, .octave=0, .vel=58, .gate=50}};
inline constexpr StyleEvent kVarDBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=108, .gate=kGate8th}, {.step=2, .tone=kRoot, .octave=1, .vel=82, .gate=kGateHat}, {.step=4, .tone=kFifth, .octave=0, .vel=98, .gate=kGate8th}, {.step=6, .tone=kFifth, .octave=1, .vel=82, .gate=kGateHat}, {.step=8, .tone=kRoot, .octave=0, .vel=104, .gate=kGate8th}, {.step=10, .tone=kRoot, .octave=1, .vel=82, .gate=kGateHat}, {.step=12, .tone=kFifth, .octave=0, .vel=98, .gate=kGate8th}, {.step=14, .tone=kSeventh, .octave=0, .vel=90, .gate=kGateHat}};
inline constexpr StyleEvent kVarDChord[] = {{.step=0, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=0, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}, {.step=4, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=4, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=4, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}, {.step=4, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateStab}, {.step=8, .tone=kRoot, .octave=0, .vel=84, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=84, .gate=kGateStab}, {.step=8, .tone=kFifth, .octave=0, .vel=84, .gate=kGateStab}, {.step=8, .tone=kSeventh, .octave=0, .vel=84, .gate=kGateStab}, {.step=12, .tone=kRoot, .octave=0, .vel=80, .gate=kGateStab}, {.step=12, .tone=kThird, .octave=0, .vel=80, .gate=kGateStab}, {.step=12, .tone=kFifth, .octave=0, .vel=80, .gate=kGateStab}, {.step=12, .tone=kSeventh, .octave=0, .vel=80, .gate=kGateStab}};
inline constexpr StylePattern kVarDPatterns[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kVarDDrums), .motif=&kPeakDrumsMotif}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarDBass)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kVarDChord), .motif=&kPeakChordMotif},
    {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadRehit), .gm_program=kPadVoice, .motif=&kSharedPadMotif},
    {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kChord2Off16), .gm_program=kChord2Voice, .motif=&kSharedChord2Motif},
    {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArp8), .gm_program=kArpVoice, .motif=&kArp8Motif},
    {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercShake)}};

inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=2, .patterns=Span<const StylePattern>(kIntroPatterns)},
    {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIntro2Patterns)},
    {.type=SectionType::kVarA, .bars=2, .patterns=Span<const StylePattern>(kVarAPatterns)},
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

// Groove (9100 Wave 1, task B): basic is the neutral scaffold -- a light-to-
// medium, generic-default amount of deterministic humanize so repeats
// breathe without leaning into any one genre's extreme (pop/rock territory).
// NOTE (owner-approved handoff): this deliberately shifts the pinned exact-
// tick/velocity assertions in test_fx.cpp, test_dual_arp_collision.cpp and
// test_engine_fire_order.cpp, which used style index 0 as a zero-jitter
// probe for engine ordering/timing invariants unrelated to musical feel.
// That breakage is expected and intentionally NOT fixed here; Torquato will
// make those probes humanize-independent and regenerate goldens downstream.
inline constexpr Style kStyle{.name="basic", .sections=Span<const StyleSection>(kSections),
                              .groove={.humanize_timing=8, .humanize_velocity=16}, .tempo=12000};

}  // namespace basic

}  // namespace styles
}  // namespace arrangrr
