#pragma once

#include "arrangrr/arranger/style_model.hpp"

namespace arrangrr {
namespace styles {


// ---------------------------------------------------------------------------
// "bossa": gentle side-stick bossa-nova cross-stick, soft surdo kick, brushed
// hats, a two-feel root-fifth bass and lush syncopated 7th comping.
namespace bossa {
// Intro1 (style-depth Wave-2 C): 2 bars now. Bar 1 (unchanged) is the held
// root under the sparse cross-stick pickup; bar 2 continues the hold, then a
// soft 5th anticipation leads into the two-feel bass proper.
inline constexpr StyleEvent kHeldBass[] = {
    {.step=0, .tone=kRoot, .octave=0, .vel=80, .gate=kGateHeld},
    {.step=16, .tone=kRoot, .octave=0, .vel=80, .gate=kGateHalfBar},
    {.step=30, .tone=kFifth, .octave=-1, .vel=74, .gate=kGate8th},
};
inline constexpr StyleEvent kTwoBass[] = {{.step=0, .tone=kRoot, .octave=0, .vel=84, .gate=kGate8th}, {.step=6, .tone=kFifth, .octave=-1, .vel=78, .gate=kGate8th}, {.step=8, .tone=kRoot, .octave=0, .vel=82, .gate=kGate8th}, {.step=14, .tone=kFifth, .octave=-1, .vel=76, .gate=kGate8th}};
// Motif engine (9210, Ottorino RANK 12): bossa's 2-feel bass, wired into
// every reference of BOTH bass tables (kTwoBass 7x, kBB 2x — the same bass
// lane split across two literal arrays, same as country). kRetrograde:
// bossa's smooth, unaccented 2-feel makes a stable mirrored answer (not an
// evolving displacement) the safer, more idiomatic choice. bossa has zero
// kLead content (a model gap, not just underuse) — closing it needs Option 2
// (motif::generate), a separate later batch, not mechanical wiring.
inline constexpr MotifSpec kTwoBassMotif{.transform = MotifTransform::kRetrograde, .seed = 1201};
// ---------------------------------------------------------------------------
// Fuller-band roles (ADDITIVE): soft string bed, nylon-guitar bossa comp, harp
// arpeggio and shaker/agogo/claves percussion. Pad sits at register 48, arp up
// at 72. Voices: pad -> Strings, chord2 -> Nylon Guitar, arp -> Harp.
inline constexpr std::int16_t kPadVoice = 48;
inline constexpr std::int16_t kChord2Voice = 24;
inline constexpr std::int16_t kArpVoice = 46;
// Motif engine (9210, Option 2 / generated seed, node 9210 owner sign-off):
// bossa's kLead model gap closed by generation, not authoring. Floats a
// flute line one register above the mid comping, centered on the 5th (a
// stable degree, so the contour naturally settles there), drawing its onset
// candidates from kDD's own side-stick/kick/hat mask.
inline constexpr std::int16_t kLeadVoice = 73;  // Flute
inline constexpr MotifSpec kLeadMotif{
    .transform = MotifTransform::kDiatonicTranspose,
    .seed = 1202, .length = 6, .center_degree = 4, .vel = 64, .gate = kGate8th,
    .idiom_role = TrackRole::kDrums,
};
// Re-articulated at step 16 too (style-depth Wave-2 C): VarA is now 2 bars;
// background hold, not the section's own variation. Shared with VarB (also
// motif-free), which stays 1 bar and never reads past step 15.
inline constexpr StyleEvent kPadTriad[] = {{.step=0, .tone=kRoot, .octave=0, .vel=50, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=48, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=50, .gate=kGateHeld},
    {.step=16, .tone=kRoot, .octave=0, .vel=50, .gate=kGateHeld}, {.step=16, .tone=kThird, .octave=0, .vel=48, .gate=kGateHeld}, {.step=16, .tone=kFifth, .octave=0, .vel=50, .gate=kGateHeld}};
inline constexpr StyleEvent kPad7[] = {{.step=0, .tone=kRoot, .octave=0, .vel=52, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=50, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=52, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=46, .gate=kGateHeld}};
inline constexpr StyleEvent kPadRehit[] = {{.step=0, .tone=kRoot, .octave=0, .vel=52, .gate=kGateHalfBar}, {.step=0, .tone=kThird, .octave=0, .vel=50, .gate=kGateHalfBar}, {.step=0, .tone=kFifth, .octave=0, .vel=52, .gate=kGateHalfBar}, {.step=0, .tone=kSeventh, .octave=0, .vel=46, .gate=kGateHalfBar}, {.step=8, .tone=kRoot, .octave=0, .vel=50, .gate=kGateHalfBar}, {.step=8, .tone=kThird, .octave=0, .vel=48, .gate=kGateHalfBar}, {.step=8, .tone=kFifth, .octave=0, .vel=50, .gate=kGateHalfBar}, {.step=8, .tone=kSeventh, .octave=0, .vel=44, .gate=kGateHalfBar}};
// A single rolled-open pad chord (D42): gesture::expand ignores the authored
// tone and spreads the whole live chord low->high across the gate — a gentle
// harp-like bloom that opens the tune. A rolled role stays kAsWritten.
inline constexpr StyleEvent kPadRollOpen[] = {{.step=0, .tone=kRoot, .octave=0, .vel=50, .gate=kGateHeld, .gesture=ChordGesture::kRollUp}};
// Re-syncopated at bar 2 (style-depth Wave-2 C, VarA now 2 bars): the
// accents land at a genuinely different placement, not a repeat. Shared
// with VarB (also motif-free), which stays 1 bar and never reads past step
// 15, so this tail is safe there too.
inline constexpr StyleEvent kChord2Sync[] = {{.step=2, .tone=kThird, .octave=0, .vel=58, .gate=kGateStab}, {.step=2, .tone=kSeventh, .octave=0, .vel=58, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=56, .gate=kGateStab}, {.step=8, .tone=kFifth, .octave=0, .vel=56, .gate=kGateStab}, {.step=8, .tone=kSeventh, .octave=0, .vel=56, .gate=kGateStab}, {.step=13, .tone=kThird, .octave=0, .vel=54, .gate=kGateStab}, {.step=13, .tone=kSeventh, .octave=0, .vel=54, .gate=kGateStab},
    {.step=20, .tone=kThird, .octave=0, .vel=58, .gate=kGateStab}, {.step=20, .tone=kSeventh, .octave=0, .vel=58, .gate=kGateStab}, {.step=25, .tone=kFifth, .octave=0, .vel=56, .gate=kGateStab}, {.step=25, .tone=kSeventh, .octave=0, .vel=56, .gate=kGateStab}, {.step=29, .tone=kThird, .octave=0, .vel=54, .gate=kGateStab}, {.step=29, .tone=kSeventh, .octave=0, .vel=54, .gate=kGateStab}};
inline constexpr StyleEvent kChord2Off[] = {{.step=3, .tone=kThird, .octave=0, .vel=56, .gate=kGateStab}, {.step=3, .tone=kSeventh, .octave=0, .vel=56, .gate=kGateStab}, {.step=7, .tone=kFifth, .octave=0, .vel=54, .gate=kGateStab}, {.step=7, .tone=kSeventh, .octave=0, .vel=54, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=56, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=56, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=54, .gate=kGateStab}, {.step=14, .tone=kSeventh, .octave=0, .vel=54, .gate=kGateStab}};
inline constexpr StyleEvent kArp8[] = {{.step=0, .tone=kRoot, .octave=0, .vel=56, .gate=kGateHat}, {.step=2, .tone=kThird, .octave=0, .vel=50, .gate=kGateHat}, {.step=4, .tone=kFifth, .octave=0, .vel=54, .gate=kGateHat}, {.step=6, .tone=kSeventh, .octave=0, .vel=50, .gate=kGateHat}, {.step=8, .tone=kRoot, .octave=1, .vel=56, .gate=kGateHat}, {.step=10, .tone=kSeventh, .octave=0, .vel=50, .gate=kGateHat}, {.step=12, .tone=kFifth, .octave=0, .vel=54, .gate=kGateHat}, {.step=14, .tone=kThird, .octave=0, .vel=50, .gate=kGateHat}};
inline constexpr StyleEvent kPercShake[] = {{.step=0, .tone=kCabasa, .octave=0, .vel=52, .gate=kGateHat}, {.step=2, .tone=kCabasa, .octave=0, .vel=44, .gate=kGateHat}, {.step=4, .tone=kCabasa, .octave=0, .vel=52, .gate=kGateHat}, {.step=6, .tone=kCabasa, .octave=0, .vel=44, .gate=kGateHat}, {.step=8, .tone=kCabasa, .octave=0, .vel=52, .gate=kGateHat}, {.step=10, .tone=kCabasa, .octave=0, .vel=44, .gate=kGateHat}, {.step=12, .tone=kCabasa, .octave=0, .vel=52, .gate=kGateHat}, {.step=14, .tone=kCabasa, .octave=0, .vel=44, .gate=kGateHat}};
inline constexpr StyleEvent kPercClave[] = {{.step=0, .tone=kClaves, .octave=0, .vel=64, .gate=kGateHat}, {.step=3, .tone=kClaves, .octave=0, .vel=58, .gate=kGateHat}, {.step=6, .tone=kClaves, .octave=0, .vel=60, .gate=kGateHat}, {.step=10, .tone=kClaves, .octave=0, .vel=58, .gate=kGateHat}, {.step=12, .tone=kClaves, .octave=0, .vel=60, .gate=kGateHat}, {.step=8, .tone=kHiAgogo, .octave=0, .vel=56, .gate=kGateHat}, {.step=14, .tone=kLoAgogo, .octave=0, .vel=54, .gate=kGateHat}};
// Motif engine (9210 Wave 1, style-depth program task A): extend variation
// onto drums/chord1/chord2/pad, wired onto the peak variation (varD) plus the
// shared reused Pad/nylon-comp arrays. kDisplacement for drums/pad/chord2 (a
// soft evolving push); kRetrograde for the chord1 comp (a legible mirrored
// answer, same idiom already proven on the two-feel bass above).
inline constexpr MotifSpec kPeakDrumsMotif{.transform = MotifTransform::kDisplacement, .seed = 1210};
inline constexpr MotifSpec kPeakChordMotif{.transform = MotifTransform::kRetrograde, .seed = 1211};
inline constexpr MotifSpec kSharedPadMotif{.transform = MotifTransform::kDisplacement, .seed = 1212};
inline constexpr MotifSpec kSharedChord2Motif{.transform = MotifTransform::kDisplacement, .seed = 1213};
inline constexpr StyleEvent kIn1D[] = {{.step=8, .tone=kSideStick, .octave=0, .vel=54, .gate=50}, {.step=11, .tone=kSideStick, .octave=0, .vel=58, .gate=50}, {.step=14, .tone=kSideStick, .octave=0, .vel=64, .gate=50},
    // bar 2 (style-depth Wave-2 C): arrival -- the cross-stick fills out to
    // the full two-feel pattern and the surdo kick previews, softly, before
    // VarA.
    {.step=16, .tone=kSideStick, .octave=0, .vel=58, .gate=50}, {.step=19, .tone=kSideStick, .octave=0, .vel=56, .gate=50}, {.step=22, .tone=kSideStick, .octave=0, .vel=60, .gate=50}, {.step=24, .tone=kSideStick, .octave=0, .vel=64, .gate=50}, {.step=27, .tone=kSideStick, .octave=0, .vel=56, .gate=50}, {.step=30, .tone=kSideStick, .octave=0, .vel=62, .gate=50},
    {.step=16, .tone=kKick, .octave=0, .vel=58, .gate=50}, {.step=24, .tone=kKick, .octave=0, .vel=56, .gate=50},
};
inline constexpr StylePattern kIn1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kHeldBass)}};
inline constexpr StyleEvent kIn2D[] = {{.step=0, .tone=kSideStick, .octave=0, .vel=64, .gate=50}, {.step=3, .tone=kSideStick, .octave=0, .vel=56, .gate=50}, {.step=6, .tone=kSideStick, .octave=0, .vel=58, .gate=50}, {.step=8, .tone=kSideStick, .octave=0, .vel=62, .gate=50}, {.step=11, .tone=kSideStick, .octave=0, .vel=56, .gate=50}, {.step=14, .tone=kSideStick, .octave=0, .vel=58, .gate=50}, {.step=0, .tone=kKick, .octave=0, .vel=60, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=58, .gate=50}};
inline constexpr StyleEvent kIn2C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=66, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=66, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=64, .gate=kGateStab}, {.step=10, .tone=kFifth, .octave=0, .vel=64, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=64, .gate=kGateStab}};
inline constexpr StylePattern kIn2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kIn2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kTwoBass), .motif=&kTwoBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIn2C)}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadRollOpen), .gm_program=kPadVoice}};
inline constexpr StyleEvent kAD[] = {
    {.step=0, .tone=kSideStick, .octave=0, .vel=70, .gate=50}, {.step=3, .tone=kSideStick, .octave=0, .vel=60, .gate=50}, {.step=6, .tone=kSideStick, .octave=0, .vel=62, .gate=50}, {.step=8, .tone=kSideStick, .octave=0, .vel=68, .gate=50}, {.step=11, .tone=kSideStick, .octave=0, .vel=60, .gate=50}, {.step=14, .tone=kSideStick, .octave=0, .vel=62, .gate=50},
    {.step=0, .tone=kKick, .octave=0, .vel=64, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=60, .gate=50},
    {.step=2, .tone=kClosedHat, .octave=0, .vel=48, .gate=50}, {.step=6, .tone=kClosedHat, .octave=0, .vel=44, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=48, .gate=50}, {.step=14, .tone=kClosedHat, .octave=0, .vel=44, .gate=50},
    // bar 2 (style-depth Wave-2 C): rhythm-section continuation (not the
    // bar's own idea -- that lives in chord1/chord2 below) so the two-feel
    // groove doesn't drop out for a full bar.
    {.step=16, .tone=kSideStick, .octave=0, .vel=70, .gate=50}, {.step=19, .tone=kSideStick, .octave=0, .vel=60, .gate=50}, {.step=22, .tone=kSideStick, .octave=0, .vel=62, .gate=50}, {.step=24, .tone=kSideStick, .octave=0, .vel=68, .gate=50}, {.step=27, .tone=kSideStick, .octave=0, .vel=60, .gate=50}, {.step=30, .tone=kSideStick, .octave=0, .vel=62, .gate=50},
    {.step=16, .tone=kKick, .octave=0, .vel=64, .gate=50}, {.step=24, .tone=kKick, .octave=0, .vel=60, .gate=50},
    {.step=18, .tone=kClosedHat, .octave=0, .vel=48, .gate=50}, {.step=22, .tone=kClosedHat, .octave=0, .vel=44, .gate=50}, {.step=26, .tone=kClosedHat, .octave=0, .vel=48, .gate=50}, {.step=30, .tone=kClosedHat, .octave=0, .vel=44, .gate=50},
};
// VarA (style-depth Wave-2 C): bar 2 answers with a fifth-centered comp,
// syncopated differently than bar 1 -- a genuine call-and-response, not a
// repeat.
inline constexpr StyleEvent kAC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=70, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=70, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=70, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=66, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=66, .gate=kGateStab}, {.step=6, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateStab}, {.step=11, .tone=kRoot, .octave=0, .vel=68, .gate=kGateStab}, {.step=11, .tone=kThird, .octave=0, .vel=68, .gate=kGateStab}, {.step=11, .tone=kSeventh, .octave=0, .vel=68, .gate=kGateStab},
    {.step=19, .tone=kFifth, .octave=0, .vel=70, .gate=kGateStab}, {.step=19, .tone=kSeventh, .octave=0, .vel=70, .gate=kGateStab},
    {.step=25, .tone=kRoot, .octave=0, .vel=68, .gate=kGateStab}, {.step=25, .tone=kThird, .octave=0, .vel=68, .gate=kGateStab}, {.step=25, .tone=kFifth, .octave=0, .vel=68, .gate=kGateStab},
    {.step=29, .tone=kRoot, .octave=0, .vel=70, .gate=kGateStab}, {.step=29, .tone=kThird, .octave=0, .vel=70, .gate=kGateStab}, {.step=29, .tone=kSeventh, .octave=0, .vel=70, .gate=kGateStab}};
inline constexpr StylePattern kAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kTwoBass), .motif=&kTwoBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kAC)}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kChord2Sync), .gm_program=kChord2Voice}};
// VarB stays 1 bar (style-depth Wave-2 C): the two-feel bass is motif-locked
// here (kTwoBassMotif on kBB), and drums/chord1 already differ from VarA --
// no genuine bar-2 idea beyond what VarA's call-and-response already carries.
inline constexpr StyleEvent kBD[] = {
    {.step=0, .tone=kSideStick, .octave=0, .vel=72, .gate=50}, {.step=3, .tone=kSideStick, .octave=0, .vel=62, .gate=50}, {.step=6, .tone=kSideStick, .octave=0, .vel=64, .gate=50}, {.step=8, .tone=kSideStick, .octave=0, .vel=70, .gate=50}, {.step=10, .tone=kSideStick, .octave=0, .vel=58, .gate=50}, {.step=11, .tone=kSideStick, .octave=0, .vel=62, .gate=50}, {.step=14, .tone=kSideStick, .octave=0, .vel=64, .gate=50},
    {.step=0, .tone=kKick, .octave=0, .vel=66, .gate=50}, {.step=4, .tone=kKick, .octave=0, .vel=54, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=62, .gate=50}, {.step=12, .tone=kKick, .octave=0, .vel=54, .gate=50},
    {.step=0, .tone=kClosedHat, .octave=0, .vel=50, .gate=50}, {.step=2, .tone=kClosedHat, .octave=0, .vel=46, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=50, .gate=50}, {.step=6, .tone=kClosedHat, .octave=0, .vel=46, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=50, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=46, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=50, .gate=50}, {.step=14, .tone=kClosedHat, .octave=0, .vel=46, .gate=50},
};
inline constexpr StyleEvent kBB[] = {{.step=0, .tone=kRoot, .octave=0, .vel=86, .gate=kGate8th}, {.step=4, .tone=kFifth, .octave=-1, .vel=78, .gate=kGate8th}, {.step=6, .tone=kFifth, .octave=-1, .vel=76, .gate=kGate8th}, {.step=8, .tone=kRoot, .octave=0, .vel=84, .gate=kGate8th}, {.step=12, .tone=kThird, .octave=0, .vel=78, .gate=kGate8th}, {.step=14, .tone=kFifth, .octave=-1, .vel=76, .gate=kGate8th}};
inline constexpr StyleEvent kBC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=72, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=0, .tone=kFifth, .octave=0, .vel=72, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}, {.step=3, .tone=kThird, .octave=0, .vel=66, .gate=kGateStab}, {.step=3, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=68, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=68, .gate=kGateStab}, {.step=6, .tone=kSeventh, .octave=0, .vel=68, .gate=kGateStab}, {.step=11, .tone=kRoot, .octave=0, .vel=70, .gate=kGateStab}, {.step=11, .tone=kThird, .octave=0, .vel=70, .gate=kGateStab}, {.step=11, .tone=kSeventh, .octave=0, .vel=70, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=64, .gate=kGateStab}, {.step=14, .tone=kSeventh, .octave=0, .vel=64, .gate=kGateStab}};
inline constexpr StylePattern kBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBB), .motif=&kTwoBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBC)}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kChord2Sync), .gm_program=kChord2Voice}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercShake)}};
inline constexpr StyleEvent kFAD[] = {{.step=0, .tone=kSideStick, .octave=0, .vel=70, .gate=50}, {.step=4, .tone=kSideStick, .octave=0, .vel=66, .gate=50}, {.step=8, .tone=kTomHi, .octave=0, .vel=72, .gate=100}, {.step=10, .tone=kTomMid, .octave=0, .vel=76, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=80, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=84, .gate=100}};
inline constexpr StyleEvent kFBD[] = {{.step=0, .tone=kTomHi, .octave=0, .vel=70, .gate=100}, {.step=3, .tone=kTomHi, .octave=0, .vel=66, .gate=100}, {.step=6, .tone=kTomMid, .octave=0, .vel=74, .gate=100}, {.step=8, .tone=kTomMid, .octave=0, .vel=76, .gate=100}, {.step=11, .tone=kTomLow, .octave=0, .vel=80, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=86, .gate=100}};
inline constexpr StyleEvent kFCD[] = {{.step=0, .tone=kSideStick, .octave=0, .vel=68, .gate=50}, {.step=2, .tone=kTomHi, .octave=0, .vel=74, .gate=100}, {.step=4, .tone=kTomHi, .octave=0, .vel=76, .gate=100}, {.step=6, .tone=kTomMid, .octave=0, .vel=78, .gate=100}, {.step=8, .tone=kTomMid, .octave=0, .vel=80, .gate=100}, {.step=10, .tone=kTomLow, .octave=0, .vel=84, .gate=100}, {.step=12, .tone=kTomLow, .octave=0, .vel=86, .gate=100}, {.step=14, .tone=kTomFloor, .octave=0, .vel=90, .gate=100}};
inline constexpr StyleEvent kFDD[] = {{.step=0, .tone=kTomHi, .octave=0, .vel=72, .gate=50}, {.step=2, .tone=kTomHi, .octave=0, .vel=68, .gate=50}, {.step=4, .tone=kTomMid, .octave=0, .vel=76, .gate=50}, {.step=6, .tone=kTomMid, .octave=0, .vel=72, .gate=50}, {.step=8, .tone=kTomLow, .octave=0, .vel=80, .gate=50}, {.step=9, .tone=kTomLow, .octave=0, .vel=76, .gate=50}, {.step=10, .tone=kTomFloor, .octave=0, .vel=84, .gate=50}, {.step=11, .tone=kTomFloor, .octave=0, .vel=80, .gate=50}, {.step=12, .tone=kSideStick, .octave=0, .vel=88, .gate=50}, {.step=13, .tone=kSideStick, .octave=0, .vel=92, .gate=50}, {.step=14, .tone=kSideStick, .octave=0, .vel=98, .gate=50}, {.step=15, .tone=kCrash, .octave=0, .vel=100, .gate=kGate8th}};
inline constexpr StylePattern kFAP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFAD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kTwoBass), .motif=&kTwoBassMotif}};
inline constexpr StylePattern kFBP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFBD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kTwoBass), .motif=&kTwoBassMotif}};
inline constexpr StylePattern kFCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kTwoBass), .motif=&kTwoBassMotif}};
inline constexpr StylePattern kFDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kFDD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kTwoBass), .motif=&kTwoBassMotif}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercClave)}};
inline constexpr StyleEvent kE1D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=76, .gate=kGateHalfBar}, {.step=0, .tone=kKick, .octave=0, .vel=64, .gate=50}};
inline constexpr StyleEvent kE1B[] = {{.step=0, .tone=kRoot, .octave=0, .vel=80, .gate=kGateHeld}};
inline constexpr StyleEvent kE1C[] = {{.step=0, .tone=kRoot, .octave=0, .vel=70, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=0, .vel=70, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=70, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=70, .gate=kGateHeld}};
inline constexpr StylePattern kE1P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE1D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE1C)}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}};
inline constexpr StyleEvent kE2D[] = {{.step=0, .tone=kCrash, .octave=0, .vel=84, .gate=kGateHeld}, {.step=0, .tone=kKick, .octave=0, .vel=68, .gate=50}, {.step=8, .tone=kSideStick, .octave=0, .vel=60, .gate=50}};
inline constexpr StyleEvent kE2B[] = {{.step=0, .tone=kRoot, .octave=-1, .vel=82, .gate=kGateHeld}, {.step=0, .tone=kRoot, .octave=0, .vel=74, .gate=kGateHeld}};
inline constexpr StyleEvent kE2C[] = {{.step=0, .tone=kThird, .octave=0, .vel=72, .gate=kGateHeld}, {.step=0, .tone=kFifth, .octave=0, .vel=72, .gate=kGateHeld}, {.step=0, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateHeld}, {.step=0, .tone=kThird, .octave=1, .vel=68, .gate=kGateHeld}};
inline constexpr StylePattern kE2P[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kE2D)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2B)}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kE2C)}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPad7), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead}};
// varC: partido-alto stripped — bare cross-stick clave and soft surdo, wide space.
// VarC stays 1 bar (style-depth Wave-2 C): the stripped-down partido-alto IS
// the variation; a bar-2 repeat would just double the space, not add an idea.
inline constexpr StyleEvent kCD[] = {{.step=0, .tone=kSideStick, .octave=0, .vel=70, .gate=50}, {.step=3, .tone=kSideStick, .octave=0, .vel=60, .gate=50}, {.step=6, .tone=kSideStick, .octave=0, .vel=62, .gate=50}, {.step=10, .tone=kSideStick, .octave=0, .vel=58, .gate=50}, {.step=11, .tone=kSideStick, .octave=0, .vel=60, .gate=50}, {.step=14, .tone=kSideStick, .octave=0, .vel=62, .gate=50}, {.step=0, .tone=kKick, .octave=0, .vel=64, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=60, .gate=50}};
inline constexpr StyleEvent kCC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=68, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=68, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=68, .gate=kGateStab}, {.step=8, .tone=kThird, .octave=0, .vel=66, .gate=kGateStab}, {.step=8, .tone=kFifth, .octave=0, .vel=66, .gate=kGateStab}, {.step=8, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateStab}};
inline constexpr StylePattern kCP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kCD)}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kTwoBass), .motif=&kTwoBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kCC)}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadRehit), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead, .motif=&kSharedPadMotif}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kChord2Off), .gm_program=kChord2Voice, .motif=&kSharedChord2Motif}, {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArp8), .gm_program=kArpVoice}};
// varD: fullest — clave + walking surdo + brushed hats, lush syncopated 7th comping.
// VarD stays 1 bar (style-depth Wave-2 C): already the fullest section; a
// bar-2 arc would blur into Ending2's own cadence rather than add a genuine
// idea.
inline constexpr StyleEvent kDD[] = {{.step=0, .tone=kSideStick, .octave=0, .vel=72, .gate=50}, {.step=3, .tone=kSideStick, .octave=0, .vel=62, .gate=50}, {.step=6, .tone=kSideStick, .octave=0, .vel=64, .gate=50}, {.step=8, .tone=kSideStick, .octave=0, .vel=70, .gate=50}, {.step=10, .tone=kSideStick, .octave=0, .vel=58, .gate=50}, {.step=11, .tone=kSideStick, .octave=0, .vel=62, .gate=50}, {.step=14, .tone=kSideStick, .octave=0, .vel=64, .gate=50}, {.step=0, .tone=kKick, .octave=0, .vel=66, .gate=50}, {.step=4, .tone=kKick, .octave=0, .vel=54, .gate=50}, {.step=8, .tone=kKick, .octave=0, .vel=62, .gate=50}, {.step=12, .tone=kKick, .octave=0, .vel=54, .gate=50}, {.step=0, .tone=kClosedHat, .octave=0, .vel=50, .gate=50}, {.step=2, .tone=kClosedHat, .octave=0, .vel=46, .gate=50}, {.step=4, .tone=kClosedHat, .octave=0, .vel=50, .gate=50}, {.step=6, .tone=kClosedHat, .octave=0, .vel=46, .gate=50}, {.step=8, .tone=kClosedHat, .octave=0, .vel=50, .gate=50}, {.step=10, .tone=kClosedHat, .octave=0, .vel=46, .gate=50}, {.step=12, .tone=kClosedHat, .octave=0, .vel=50, .gate=50}, {.step=14, .tone=kClosedHat, .octave=0, .vel=46, .gate=50}};
inline constexpr StyleEvent kDC[] = {{.step=0, .tone=kRoot, .octave=0, .vel=72, .gate=kGateStab}, {.step=0, .tone=kThird, .octave=0, .vel=72, .gate=kGateStab}, {.step=0, .tone=kFifth, .octave=0, .vel=72, .gate=kGateStab}, {.step=0, .tone=kSeventh, .octave=0, .vel=72, .gate=kGateStab}, {.step=3, .tone=kThird, .octave=0, .vel=66, .gate=kGateStab}, {.step=3, .tone=kSeventh, .octave=0, .vel=66, .gate=kGateStab}, {.step=6, .tone=kThird, .octave=0, .vel=68, .gate=kGateStab}, {.step=6, .tone=kFifth, .octave=0, .vel=68, .gate=kGateStab}, {.step=6, .tone=kSeventh, .octave=0, .vel=68, .gate=kGateStab}, {.step=10, .tone=kRoot, .octave=0, .vel=70, .gate=kGateStab}, {.step=10, .tone=kThird, .octave=0, .vel=70, .gate=kGateStab}, {.step=10, .tone=kSeventh, .octave=0, .vel=70, .gate=kGateStab}, {.step=14, .tone=kFifth, .octave=0, .vel=64, .gate=kGateStab}, {.step=14, .tone=kSeventh, .octave=0, .vel=64, .gate=kGateStab}};
inline constexpr StylePattern kDP[] = {{.role=TrackRole::kDrums, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kDD), .motif=&kPeakDrumsMotif}, {.role=TrackRole::kBass, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kBB), .motif=&kTwoBassMotif}, {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kDC), .motif=&kPeakChordMotif}, {.role=TrackRole::kPad, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadRehit), .gm_program=kPadVoice, .voicing=VoicingPolicy::kLead, .motif=&kSharedPadMotif}, {.role=TrackRole::kChord2, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kChord2Off), .gm_program=kChord2Voice, .motif=&kSharedChord2Motif}, {.role=TrackRole::kArp, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kArp8), .gm_program=kArpVoice}, {.role=TrackRole::kLead, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(), .gm_program=kLeadVoice, .motif=&kLeadMotif}, {.role=TrackRole::kPerc, .policy=RolePolicy::kFixed, .events=Span<const StyleEvent>(kPercClave)}};
inline constexpr StyleSection kSections[] = {
    {.type=SectionType::kIntro1, .bars=2, .patterns=Span<const StylePattern>(kIn1P)}, {.type=SectionType::kIntro2, .bars=1, .patterns=Span<const StylePattern>(kIn2P)},
    {.type=SectionType::kVarA, .bars=2, .patterns=Span<const StylePattern>(kAP)}, {.type=SectionType::kVarB, .bars=1, .patterns=Span<const StylePattern>(kBP)},
    {.type=SectionType::kVarC, .bars=1, .patterns=Span<const StylePattern>(kCP)}, {.type=SectionType::kVarD, .bars=1, .patterns=Span<const StylePattern>(kDP)},
    {.type=SectionType::kFillA, .bars=1, .patterns=Span<const StylePattern>(kFAP)}, {.type=SectionType::kFillB, .bars=1, .patterns=Span<const StylePattern>(kFBP)}, {.type=SectionType::kFillC, .bars=1, .patterns=Span<const StylePattern>(kFCP)}, {.type=SectionType::kFillD, .bars=1, .patterns=Span<const StylePattern>(kFDP)},
    {.type=SectionType::kEnding1, .bars=1, .patterns=Span<const StylePattern>(kE1P)}, {.type=SectionType::kEnding2, .bars=1, .patterns=Span<const StylePattern>(kE2P)},
};
// Groove (9100 Wave 1, task B): smooth, gentle 2-feel -- a moderate,
// unaccented deterministic humanize.
inline constexpr Style kStyle{.name="bossa", .sections=Span<const StyleSection>(kSections),
                              .groove={.humanize_timing=10, .humanize_velocity=18}, .tempo=13000};
}  // namespace bossa

}  // namespace styles
}  // namespace arrangrr
