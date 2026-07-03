#pragma once

#include <cstdint>
#include <string>

// Pure note-name formatting, host side only. No terminal access, no ANSI, no
// I/O: given a MIDI note it produces a human-readable pitch-class or full note
// name. The CDE tables are the single source of truth once shared by jsonl.cpp
// so wire output and UI never diverge.

namespace arrangrr::host {

enum class NoteNaming { kCde, kDoReMi };

struct NoteNameOptions {
  NoteNaming naming = NoteNaming::kCde;
  bool prefer_flats = false;
  bool include_octave = true;
};

// Scientific pitch, C4 = 60 (octave = note / 12 - 1). With include_octave the
// octave is appended with no separator ("C4", "Do#4").
std::string note_name(std::uint8_t midi_note, const NoteNameOptions& options);

// Pitch class only; octave and include_octave are ignored.
std::string pitch_class_name(std::uint8_t midi_note, const NoteNameOptions& options);

}  // namespace arrangrr::host
