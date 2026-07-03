#include "note_names.hpp"

#include <array>
#include <cstdio>

namespace arrangrr::host {

namespace {

constexpr int kSemitonesPerOctave = 12;

// C4 = 60 lives in octave 4, so octave = note / 12 - 1.
constexpr int kOctaveOffset = -1;

using NameTable = std::array<const char*, kSemitonesPerOctave>;

// CDE tables MUST stay byte-identical to the ones jsonl.cpp historically owned:
// the golden wire output depends on these exact spellings.
constexpr NameTable kCdeSharp = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
constexpr NameTable kCdeFlat = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};

constexpr NameTable kDoReMiSharp = {"Do",  "Do#", "Re",   "Re#", "Mi",  "Fa",
                                    "Fa#", "Sol", "Sol#", "La",  "La#", "Si"};
constexpr NameTable kDoReMiFlat = {"Do",   "Reb", "Re",  "Mib", "Mi",  "Fa",
                                   "Solb", "Sol", "Lab", "La",  "Sib", "Si"};

const NameTable& select_table(const NoteNameOptions& options) {
  if (options.naming == NoteNaming::kDoReMi) {
    return options.prefer_flats ? kDoReMiFlat : kDoReMiSharp;
  }

  return options.prefer_flats ? kCdeFlat : kCdeSharp;
}

}  // namespace

std::string pitch_class_name(std::uint8_t midi_note, const NoteNameOptions& options) {
  const std::size_t pitch_class = static_cast<std::size_t>(midi_note % kSemitonesPerOctave);

  return select_table(options)[pitch_class];
}

std::string note_name(std::uint8_t midi_note, const NoteNameOptions& options) {
  std::string name = pitch_class_name(midi_note, options);

  if (options.include_octave) {
    const int octave = midi_note / kSemitonesPerOctave + kOctaveOffset;
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%d", octave);
    name += buf;
  }

  return name;
}

}  // namespace arrangrr::host
