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

// General MIDI percussion map, channel 10 only. Table spans the conventional
// GM drum range; gaps within it (no conventional name) are nullptr.
constexpr std::uint8_t kGmDrumFirstNote = 35;
constexpr std::uint8_t kGmDrumLastNote = 59;

using GmDrumTable = std::array<const char*, kGmDrumLastNote - kGmDrumFirstNote + 1>;

constexpr GmDrumTable kGmDrumNames = {
    "Kick 2",      // 35
    "Kick",        // 36
    "Side Stick",  // 37
    "Snare",       // 38
    "Clap",        // 39
    "Snare 2",     // 40
    "Low Tom 2",   // 41
    "Closed HH",   // 42
    "Low Tom",     // 43
    "Pedal HH",    // 44
    "Mid Tom",     // 45
    "Open HH",     // 46
    "Mid Tom 2",   // 47
    "High Tom",    // 48
    "Crash",       // 49
    "High Tom 2",  // 50
    "Ride",        // 51
    "China",       // 52
    "Ride Bell",   // 53
    "Tambourine",  // 54
    "Splash",      // 55
    "Cowbell",     // 56
    "Crash 2",     // 57
    nullptr,       // 58 (no conventional GM name)
    "Ride 2",      // 59
};

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

const char* gm_drum_name(std::uint8_t midi_note) {
  if (midi_note < kGmDrumFirstNote || midi_note > kGmDrumLastNote) {
    return nullptr;
  }

  return kGmDrumNames[midi_note - kGmDrumFirstNote];
}

}  // namespace arrangrr::host
