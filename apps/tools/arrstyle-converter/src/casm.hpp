#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "diagnostics.hpp"
#include "model.hpp"

// CASM decoder for Yamaha SFF (.sty) files. SFF is a REVERSE-ENGINEERED format:
// a .sty is a Standard MIDI File (the pattern data) followed by proprietary
// chunks. This module decodes only the STRUCTURE of the trailing CASM chunk
// from user-provided files: the per-channel chord table (Ctab / Ctb2) that maps
// each recorded source channel to a style role and tells the engine HOW to
// transpose it (the NTR / NTT rules — arrangrr's NTT idea, D24, in another
// dialect). No proprietary internals and no copyrighted style content are ship-
// ped; this only names offsets so a user's own file can be understood.

namespace arrstyle {

// Note Transposition Rule (Yamaha NTR): how the whole pattern is shifted.
enum class NoteTranspositionRule : std::uint8_t {
  kRootTranspose = 0,  // shift the pattern by the played chord root (e.g. bass)
  kRootFixed = 1,      // keep the reference; the NTT maps notes (drums/chord)
  kGuitar = 2,         // guitar-style voicing follow
  kUnknown = 255,
};

// Note Transposition Table (Yamaha NTT): how individual notes are remapped onto
// the played chord. `kBypass` means "do not transpose" (drums / percussion).
enum class NoteTranspositionTable : std::uint8_t {
  kBypass = 0,
  kMelody = 1,
  kChord = 2,
  kBass = 3,
  kMelodicMinor = 4,
  kHarmonicMinor = 6,
  kUnknown = 255,
};

// One decoded per-channel chord-table entry (a CASM Ctab / Ctb2 record).
struct CasmChannel {
  std::uint8_t source_channel = 0;       // the MIDI channel the notes live on
  std::uint8_t destination_channel = 0;  // the canonical style-part channel (role)
  std::string name;                      // voice/part name (provenance only)
  std::uint8_t source_chord_root = 0;    // 0..11, C = 0 (usually C)
  std::uint8_t source_chord_type = 0;    // Yamaha chord-type index (usually 2 = Maj7)
  NoteTranspositionRule ntr = NoteTranspositionRule::kUnknown;
  NoteTranspositionTable ntt = NoteTranspositionTable::kUnknown;
  std::uint8_t note_low = 0;     // register clamp, low  (SFF1 only; else 0)
  std::uint8_t note_high = 127;  // register clamp, high (SFF1 only; else 127)
  std::uint8_t retrigger = 0;    // Yamaha retrigger rule (SFF1 only; else 0)
};

// One CSEG: the set of section markers it applies to plus its channel tables.
struct CasmSegment {
  std::vector<std::string> section_names;  // e.g. {"Main B", "Main C", "Fill In AA"}
  std::vector<CasmChannel> channels;
};

struct CasmData {
  bool present = false;  // a CASM chunk was found at all
  bool sff2 = false;     // uses the SFF2 (Ctb2) channel table variant
  std::vector<CasmSegment> segments;
};

// One style marker meta-event from the embedded SMF (FF 06), with its tick.
struct StyleMarker {
  std::uint32_t tick = 0;
  std::string name;
};

// Decodes the CASM chunk. Returns true when at least one usable segment was
// read; false (with an info/warning) when CASM is absent or empty, so the
// caller can fall back to a raw SMF import. Never throws; every read is bounded.
bool decode_casm(const std::vector<std::uint8_t>& bytes, const std::string& source, CasmData& out,
                 Diagnostics& diag);

// Scans the embedded SMF track(s) for marker meta-events (FF 06). Bounded walk;
// returns an empty vector if none are found or the SMF is unreadable.
std::vector<StyleMarker> scan_style_markers(const std::vector<std::uint8_t>& bytes);

// Maps a Yamaha section-marker name ("Main A", "Fill In BA", ...) onto the
// native (kind, variation). Returns false for non-section markers ("SFF1",
// "SInt", ...).
bool parse_style_section(const std::string& name, SectionKind& kind, SectionVariation& variation);

// Maps a canonical style-part destination channel (8..15) onto a native Role.
Role role_from_destination_channel(std::uint8_t destination_channel);

// Turns a decoded source-chord root byte into a pitch class, clamped to 0..11.
std::uint8_t casm_root_pitch_class(std::uint8_t raw_root) noexcept;

}  // namespace arrstyle
