#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Canonical, tool-only model for the arrstyle-converter (DESIGN.md).
//
// These are host-side value types with arrangrr-native names. They are the
// import target for every alien format and the future input to the runtime
// STYLE COMPILER that lowers them onto the constexpr device format
// (arrangrr/arranger/style.hpp: Style / StyleSection / StylePattern /
// StyleEvent). Nothing here ever ships to the STM32 core: it uses std::string
// / std::vector and lives entirely in the host toolchain.

namespace arrstyle {

// -------------------------------------------------------------------------
// Enumerations (native vocabulary — never Yamaha/SFF terminology).
// -------------------------------------------------------------------------

enum class SourceFormat : std::uint8_t {
  kUnknown = 0,
  kStandardMidiFile = 1,
  kChordPro = 2,
  kYamahaSff = 3,
};

// SectionKind + SectionVariation together address a runtime SectionType
// (Intro1/2, VarA..D, FillA..D, Break, Ending1/2).
enum class SectionKind : std::uint8_t {
  kIntro = 0,
  kMain = 1,
  kFill = 2,
  kBreak = 3,
  kEnding = 4,
};

enum class SectionVariation : std::uint8_t {
  kA = 0,
  kB = 1,
  kC = 2,
  kD = 3,
  kNone = 4,
};

enum class Role : std::uint8_t {
  kDrums = 0,
  kPercussion = 1,
  kBass = 2,
  kChord1 = 3,
  kChord2 = 4,
  kPad = 5,
  kArp = 6,
  kPhrase = 7,
  kLead = 8,
  kUnassigned = 9,
};

// Does the lane follow the live chord at playback? (RolePolicy in the core:
// kFixed = literal MIDI notes, kChordTone = resolved through the NTT.)
enum class TranspositionPolicy : std::uint8_t {
  kFixed = 0,
  kChordTone = 1,
};

// How a still-sounding phrase reacts to a chord change (future NTT input).
enum class RetriggerPolicy : std::uint8_t {
  kSustain = 0,
  kRetrigger = 1,
};

// Where the arranger's harmony comes from when a SongModel plays.
enum class PhraseSourceHarmony : std::uint8_t {
  kChordSequence = 0,
  kLiveChord = 1,
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
  kUnknown = 11,
};

// -------------------------------------------------------------------------
// StyleModel — imported accompaniment (target: runtime Style).
// -------------------------------------------------------------------------

struct PhraseEvent {
  std::uint32_t tick = 0;        // section-relative, in source_ppqn units
  std::uint8_t note = 0;         // kFixed: MIDI note; kChordTone: source MIDI note
  std::uint8_t velocity = 0;     // 1..127 (0 is never stored: it means "no event")
  std::uint32_t gate_ticks = 0;  // sounding length in source_ppqn units
};

struct PhraseLane {
  Role role = Role::kUnassigned;
  std::uint8_t source_channel = 0;  // MIDI channel of origin, 0-based (provenance)
  TranspositionPolicy transposition = TranspositionPolicy::kChordTone;
  RetriggerPolicy retrigger = RetriggerPolicy::kSustain;
  // SFF/CASM provenance. The recorded notes of a chord-tone lane were written
  // over a fixed SOURCE chord (Yamaha: almost always C Maj7); together with the
  // transposition policy this is what makes the lane transposable to any live
  // chord (D24 NTT). `source_root_pc < 0` means "not applicable" (e.g. an SMF
  // import), and those lanes serialize exactly as before (no extra JSON keys).
  std::int8_t source_root_pc = -1;                       // 0..11; -1 = not set
  ChordQuality source_quality = ChordQuality::kUnknown;  // quality of the source chord
  std::uint8_t note_low = 0;                             // CASM register clamp, low
  std::uint8_t note_high = 127;                          // CASM register clamp, high
  std::vector<PhraseEvent> events;
};

struct StyleSection {
  SectionKind kind = SectionKind::kMain;
  SectionVariation variation = SectionVariation::kA;
  std::uint16_t bars = 1;
  std::vector<PhraseLane> lanes;
};

struct StyleModel {
  std::string name;
  SourceFormat source_format = SourceFormat::kUnknown;
  std::uint16_t source_ppqn = 960;
  std::uint32_t tempo_milli_bpm = 120000;  // 120.000 BPM
  std::uint8_t time_sig_num = 4;
  std::uint8_t time_sig_den = 4;
  std::vector<StyleSection> sections;
};

// -------------------------------------------------------------------------
// SongModel — imported progression (target: runtime ChordSequence + Song).
// -------------------------------------------------------------------------

struct ChordEvent {
  std::uint16_t position = 0;  // running slot index (order of appearance)
  std::uint16_t bar = 0;       // best-effort bar index (weak for ChordPro)
  std::int8_t root_pc = -1;    // 0..11; -1 = unparseable
  ChordQuality quality = ChordQuality::kUnknown;
  std::int8_t bass_pc = -1;  // slash-chord bass pitch class; -1 = none
  std::string source_text;   // original token, kept for diagnostics
};

struct SongSection {
  std::string label;           // marker / directive text
  std::uint16_t position = 0;  // slot index where the marker starts
};

struct SongModel {
  std::string name;
  std::int8_t key_root_pc = -1;       // 0..11; -1 = unspecified
  std::uint8_t key_mode = 0;          // 0 = major, 1 = minor
  std::uint32_t tempo_milli_bpm = 0;  // 0 = unspecified
  std::uint8_t time_sig_num = 0;      // 0 = unspecified
  std::uint8_t time_sig_den = 0;
  PhraseSourceHarmony harmony_source = PhraseSourceHarmony::kChordSequence;
  std::vector<SongSection> sections;
  std::vector<ChordEvent> chords;
};

// -------------------------------------------------------------------------
// Enum <-> stable string names (used by the JSON writer and validator).
// -------------------------------------------------------------------------

const char* to_string(SourceFormat v) noexcept;
const char* to_string(SectionKind v) noexcept;
const char* to_string(SectionVariation v) noexcept;
const char* to_string(Role v) noexcept;
const char* to_string(TranspositionPolicy v) noexcept;
const char* to_string(RetriggerPolicy v) noexcept;
const char* to_string(PhraseSourceHarmony v) noexcept;
const char* to_string(ChordQuality v) noexcept;

// Parsers return false and leave the out-param untouched on an unknown name.
bool parse_section_kind(const std::string& s, SectionKind& out) noexcept;
bool parse_section_variation(const std::string& s, SectionVariation& out) noexcept;
bool parse_role(const std::string& s, Role& out) noexcept;
bool parse_transposition_policy(const std::string& s, TranspositionPolicy& out) noexcept;
bool parse_retrigger_policy(const std::string& s, RetriggerPolicy& out) noexcept;
bool parse_chord_quality(const std::string& s, ChordQuality& out) noexcept;

}  // namespace arrstyle
