#pragma once

#include <cstdint>

// Diatonic chord theory, fully constexpr (D32): mode scales, degree lookup,
// and the "smart per degree" chord quality of D19 — qualities are DERIVED by
// stacking thirds on the mode's scale (so every mode is automatically
// correct), with one deliberate musical exception: in the minor (aeolian)
// mode the V degree is raised to a dominant 7th (harmonic-minor practice,
// resolving the DESIGN §24.6 open point). Overrides are always possible
// (D19/D20) via explicit qualities.
//
// Phase-4b promotion (docs/design/orchestrator-pipeline-extraction.md
// §16.1/§16.8): moved byte-for-byte out of components/core/arrangrr alongside
// ChordDetector/FollowedContext, which both need Key/ChordQuality/ChordState
// and the theory:: functions -- chorddet must not depend on arrangrr (D43),
// so this pure-theory vocabulary travels with them. arrangrr's own chord/
// arranger modules now depend on chorddet for these types (one direction
// only: arrangrr -> chorddet, never the reverse). Namespace stays `arrangrr`
// (minimal churn, same precedent as the Phase-1 runtime extraction).

namespace arrangrr {

enum class Mode : std::uint8_t {
  kMajor = 0,  // ionian
  kMinor = 1,  // aeolian
  kDorian = 2,
  kPhrygian = 3,
  kLydian = 4,
  kMixolydian = 5,
  kLocrian = 6,
};
inline constexpr std::uint8_t kModeCount = 7;

struct Key {
  std::uint8_t root_pc = 0;  // 0 = C
  Mode mode = Mode::kMajor;
};

enum class ChordQuality : std::uint8_t {
  kMaj = 0,
  kMin = 1,
  kDim = 2,
  kAug = 3,
  kMaj7 = 4,
  kMin7 = 5,
  kDom7 = 6,
  kHalfDim7 = 7,
  kDim7 = 8,
  kSus2 = 9,
  kSus4 = 10,
};
inline constexpr std::uint8_t kQualityCount = 11;

// Up to 4 chord tones as semitone offsets from the chord root.
struct ChordShape {
  std::uint8_t count = 0;
  std::uint8_t offsets[4] = {0, 0, 0, 0};
};

// The live harmonic context the arranger's NTT resolution (D24) resolves
// against: a pitch class + quality, `valid` until the first chord sounds. Set
// by the ChordEngine (chord play / recorded ChordSequencer) and by live
// piano->chord detection. Lives here as a pure-theory type so both the chord
// engine and the freestanding ChordDetector can name it without pulling in
// MIDI/host headers.
struct ChordState {
  std::uint8_t root_pc = 0;
  ChordQuality quality = ChordQuality::kMaj;
  bool valid = false;  // false until the first chord sounds
};

namespace theory {

// Scale intervals per mode (rotations of the major scale).
struct Scale {
  std::uint8_t steps[7];
};

constexpr Scale scale_of(Mode mode) noexcept {
  constexpr std::uint8_t kMajorScale[7] = {0, 2, 4, 5, 7, 9, 11};
  constexpr std::uint8_t kRotation[kModeCount] = {
      0,  // major   = ionian
      5,  // minor   = aeolian (6th rotation)
      1,  // dorian
      2,  // phrygian
      3,  // lydian
      4,  // mixolydian
      6,  // locrian
  };
  const std::uint8_t r = kRotation[static_cast<std::uint8_t>(mode)];
  Scale s{};
  const std::uint8_t base = kMajorScale[r];
  for (int i = 0; i < 7; ++i) {
    s.steps[i] = static_cast<std::uint8_t>((kMajorScale[(r + i) % 7] + 12 - base) % 12);
  }
  return s;
}

// Signed scale degree -> semitone offset from the key tonic. The degree is
// diatonic and unbounded: it wraps every 7 steps with a full-octave (12
// semitone) shift, so degree 7 is the tonic one octave up and degree -1 is the
// scale's leading tone one octave down. Uses floor division/modulo so negative
// degrees wrap correctly (e.g. C major degree -1 = B below tonic = -1).
constexpr int degree_to_semitones(Mode mode, int degree) noexcept {
  const Scale s = scale_of(mode);
  constexpr int kScaleSteps = 7;
  constexpr int kSemitonesPerOctave = 12;
  // Floor division/modulo (C++ truncates toward zero, wrong for negatives).
  int octave = degree / kScaleSteps;
  int index = degree % kScaleSteps;
  if (index < 0) {
    index += kScaleSteps;
    --octave;
  }
  return static_cast<int>(s.steps[index]) + kSemitonesPerOctave * octave;
}

// Degree (0..6) of a pitch class within the key's scale; -1 if chromatic.
constexpr int degree_of(const Key& key, std::uint8_t pc) noexcept {
  const Scale s = scale_of(key.mode);
  const std::uint8_t rel = static_cast<std::uint8_t>((pc + 12 - key.root_pc) % 12);
  for (int i = 0; i < 7; ++i) {
    if (s.steps[i] == rel) {
      return i;
    }
  }
  return -1;
}

// Chord tones for an explicit quality.
constexpr ChordShape shape_of(ChordQuality q) noexcept {
  switch (q) {
    case ChordQuality::kMaj:
      return {.count=3, .offsets={0, 4, 7, 0}};
    case ChordQuality::kMin:
      return {.count=3, .offsets={0, 3, 7, 0}};
    case ChordQuality::kDim:
      return {.count=3, .offsets={0, 3, 6, 0}};
    case ChordQuality::kAug:
      return {.count=3, .offsets={0, 4, 8, 0}};
    case ChordQuality::kMaj7:
      return {.count=4, .offsets={0, 4, 7, 11}};
    case ChordQuality::kMin7:
      return {.count=4, .offsets={0, 3, 7, 10}};
    case ChordQuality::kDom7:
      return {.count=4, .offsets={0, 4, 7, 10}};
    case ChordQuality::kHalfDim7:
      return {.count=4, .offsets={0, 3, 6, 10}};
    case ChordQuality::kDim7:
      return {.count=4, .offsets={0, 3, 6, 9}};
    case ChordQuality::kSus2:
      return {.count=3, .offsets={0, 2, 7, 0}};
    case ChordQuality::kSus4:
      return {.count=3, .offsets={0, 5, 7, 0}};
  }
  return {};
}

// Smart quality (D19): stack thirds on the scale at `degree`, then map the
// resulting third/fifth/seventh to a seventh-chord quality.
constexpr ChordQuality smart_quality(Mode mode, int degree) noexcept {
  const Scale s = scale_of(mode);
  const auto tone = [&](int d) { return s.steps[d % 7]; };
  const std::uint8_t root = s.steps[degree];
  const std::uint8_t third = static_cast<std::uint8_t>((tone(degree + 2) + 12 - root) % 12);
  const std::uint8_t fifth = static_cast<std::uint8_t>((tone(degree + 4) + 12 - root) % 12);
  const std::uint8_t seventh = static_cast<std::uint8_t>((tone(degree + 6) + 12 - root) % 12);

  // Harmonic-minor exception: V of the minor mode becomes dominant.
  if (mode == Mode::kMinor && degree == 4) {
    return ChordQuality::kDom7;
  }

  if (third == 4 && fifth == 7 && seventh == 11) {
    return ChordQuality::kMaj7;
  }
  if (third == 3 && fifth == 7 && seventh == 10) {
    return ChordQuality::kMin7;
  }
  if (third == 4 && fifth == 7 && seventh == 10) {
    return ChordQuality::kDom7;
  }
  if (third == 3 && fifth == 6 && seventh == 10) {
    return ChordQuality::kHalfDim7;
  }
  if (third == 3 && fifth == 6 && seventh == 9) {
    return ChordQuality::kDim7;
  }
  if (third == 4 && fifth == 8) {
    return ChordQuality::kAug;
  }
  return third == 3 ? ChordQuality::kMin7 : ChordQuality::kMaj7;
}

// arrangrr scale-aware single-finger (Dxx, refines D45) — NOT Yamaha Single
// Finger, which is key-independent (one key = major). This is the Casio-Chord /
// "smart" lineage: one key -> the diatonic MAJOR-or-MINOR TRIAD rooted on the
// pressed pitch class, so a non-keyboardist stays in key with one finger.
//   - diatonic root: the triad quality is the third of D19 `smart_quality` at
//     that degree (a minor third -> minor, a major third -> major);
//   - dim degree (vii in major, ii in natural minor) -> MINOR: it already has
//     the minor third, so we keep it and restore the perfect fifth;
//   - aug degree -> MAJOR by the same major-third rule. (Musically stated for
//     completeness; smart_quality only ever yields maj7/dom7/min7/half-dim/dim7
//     over the diatonic degrees, so kAug never actually reaches here — the
//     major-third branch is defensive, not a live path.)
//   - chromatic root (out of key) -> MAJOR: it functions as a secondary
//     dominant and a one-finger shortcut must NEVER stall mid-phrase.
// Triads only, no D19 richness — this is what keeps single-finger DISTINCT from
// diatonic mode (which is D20-strict and shows the true dim/aug quality).
constexpr ChordQuality single_finger_quality(const Key& key, std::uint8_t pc) noexcept {
  const int degree = degree_of(key, pc);
  if (degree < 0) {
    return ChordQuality::kMaj;  // chromatic root -> major (secondary dominant)
  }
  // The third of the diatonic seventh-chord (offsets[1]) is the quality bit: a
  // minor third (3) collapses dim/min7/half-dim to a MINOR triad, a major third
  // (4) collapses aug/maj7/dom7 to a MAJOR triad. Fifth is always restored.
  const ChordShape sh = shape_of(smart_quality(key.mode, degree));
  return sh.offsets[1] == 3 ? ChordQuality::kMin : ChordQuality::kMaj;
}

// Mode C (D12): complete a shell/partial voicing. `intervals` are the pitch
// classes above the root (mod 12, root excluded, zero-terminated array of up
// to 3). Decision tree over third/fifth/seventh/sus flags; unmatched combos
// fall back to the closest family.
constexpr ChordQuality complete_shell(const std::uint8_t (&iv)[3], std::uint8_t n) noexcept {
  bool maj3 = false, min3 = false, dim5 = false, aug5 = false;
  bool min7 = false, maj7 = false, sus4 = false, sus2 = false;
  for (std::uint8_t i = 0; i < n; ++i) {
    switch (iv[i]) {
      case 4:
        maj3 = true;
        break;
      case 3:
        min3 = true;
        break;
      case 6:
        dim5 = true;
        break;
      case 8:
        aug5 = true;
        break;
      case 10:
        min7 = true;
        break;
      case 11:
        maj7 = true;
        break;
      case 5:
        sus4 = true;
        break;
      case 2:
        sus2 = true;
        break;
      default:
        break;  // perfect fifth (7) adds no color
    }
  }
  if (min3 && dim5) {
    return min7 ? ChordQuality::kHalfDim7 : ChordQuality::kDim;
  }
  if (maj3 && aug5) {
    return ChordQuality::kAug;
  }
  if (min7) {
    return min3 ? ChordQuality::kMin7 : ChordQuality::kDom7;
  }
  if (maj7) {
    return ChordQuality::kMaj7;  // maj or min third: closest is maj7
  }
  if (min3) {
    return ChordQuality::kMin;
  }
  if (maj3) {
    return ChordQuality::kMaj;
  }
  if (sus4) {
    return ChordQuality::kSus4;
  }
  if (sus2) {
    return ChordQuality::kSus2;
  }
  return ChordQuality::kMaj;  // bare root / bare fifth: major
}

// dim7 needs the diminished seventh (9), folded in here to keep the tree flat.
constexpr ChordQuality complete_shell_full(const std::uint8_t (&iv)[3], std::uint8_t n) noexcept {
  bool min3 = false, dim5 = false, bb7 = false;
  for (std::uint8_t i = 0; i < n; ++i) {
    if (iv[i] == 3) {
      min3 = true;
    }
    if (iv[i] == 6) {
      dim5 = true;
    }
    if (iv[i] == 9) {
      bb7 = true;
    }
  }
  if (min3 && dim5 && bb7) {
    return ChordQuality::kDim7;
  }
  return complete_shell(iv, n);
}

// --- compile-time self-tests (D32) ----------------------------------------
namespace selftest {
constexpr Key kCMajor{.root_pc=0, .mode=Mode::kMajor};
constexpr Key kAMinor{.root_pc=9, .mode=Mode::kMinor};
// C major degrees: D is the ii, B the vii.
static_assert(degree_of(kCMajor, 2) == 1);
static_assert(degree_of(kCMajor, 11) == 6);
static_assert(degree_of(kCMajor, 1) == -1);  // C# is chromatic in C major
// D19 smart qualities in major: I maj7, ii min7, V dom7, vii ø7.
static_assert(smart_quality(Mode::kMajor, 0) == ChordQuality::kMaj7);
static_assert(smart_quality(Mode::kMajor, 1) == ChordQuality::kMin7);
static_assert(smart_quality(Mode::kMajor, 3) == ChordQuality::kMaj7);
static_assert(smart_quality(Mode::kMajor, 4) == ChordQuality::kDom7);
static_assert(smart_quality(Mode::kMajor, 6) == ChordQuality::kHalfDim7);
// Minor: i min7, III maj7, V dominant by the harmonic exception, VII dom7.
static_assert(smart_quality(Mode::kMinor, 0) == ChordQuality::kMin7);
static_assert(smart_quality(Mode::kMinor, 2) == ChordQuality::kMaj7);
static_assert(smart_quality(Mode::kMinor, 4) == ChordQuality::kDom7);
static_assert(smart_quality(Mode::kMinor, 6) == ChordQuality::kDom7);
// A natural minor scale root check: E (pc 4) is the V of A minor.
static_assert(degree_of(kAMinor, 4) == 4);
// degree_to_semitones: C major maps degrees to major-scale semitones, wrapping
// an octave every 7 steps with correct floor behavior for negatives.
static_assert(degree_to_semitones(Mode::kMajor, 0) == 0);
static_assert(degree_to_semitones(Mode::kMajor, 1) == 2);
static_assert(degree_to_semitones(Mode::kMajor, 4) == 7);
static_assert(degree_to_semitones(Mode::kMajor, 7) == 12);
static_assert(degree_to_semitones(Mode::kMajor, 8) == 14);
static_assert(degree_to_semitones(Mode::kMajor, -1) == -1);  // leading tone below
static_assert(degree_to_semitones(Mode::kMajor, -7) == -12);
// Aeolian (natural minor) follows its own steps {0,2,3,5,7,8,10}.
static_assert(degree_to_semitones(Mode::kMinor, 2) == 3);   // minor third
static_assert(degree_to_semitones(Mode::kMinor, 5) == 8);   // minor sixth
static_assert(degree_to_semitones(Mode::kMinor, -1) == -2); // whole tone below tonic
// Shell completion (mode C) spot checks.
constexpr std::uint8_t kIv1[3] = {4, 0, 0};
static_assert(complete_shell_full(kIv1, 1) == ChordQuality::kMaj);
constexpr std::uint8_t kIv2[3] = {3, 10, 0};
static_assert(complete_shell_full(kIv2, 2) == ChordQuality::kMin7);
constexpr std::uint8_t kIv3[3] = {10, 0, 0};
static_assert(complete_shell_full(kIv3, 1) == ChordQuality::kDom7);
constexpr std::uint8_t kIv4[3] = {3, 6, 9};
static_assert(complete_shell_full(kIv4, 3) == ChordQuality::kDim7);
// Scale-aware single-finger (Dxx): triads only, dim->minor, aug->major,
// chromatic->major. C major: I major, ii minor, vii(dim)->minor, C# chromatic
// ->major. A minor: i minor, III major, ii(dim)->minor.
static_assert(single_finger_quality(kCMajor, 0) == ChordQuality::kMaj);   // I
static_assert(single_finger_quality(kCMajor, 2) == ChordQuality::kMin);   // ii
static_assert(single_finger_quality(kCMajor, 11) == ChordQuality::kMin);  // vii dim->min
static_assert(single_finger_quality(kCMajor, 1) == ChordQuality::kMaj);   // C# chromatic->maj
static_assert(single_finger_quality(kAMinor, 9) == ChordQuality::kMin);   // i
static_assert(single_finger_quality(kAMinor, 0) == ChordQuality::kMaj);   // III (C)
static_assert(single_finger_quality(kAMinor, 11) == ChordQuality::kMin);  // ii dim->min (B)
}  // namespace selftest

}  // namespace theory
}  // namespace arrangrr
