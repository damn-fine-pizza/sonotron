#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "note_names.hpp"

// Pure ASCII piano renderer, host side only: NO ANSI, NO terminal access, NO
// I/O. render_piano_panel() turns a view state and a column budget into a set
// of plain text lines; the caller owns cursor placement and colouring. Keeping
// this a pure function makes it trivially unit-testable and portable.

namespace arrangrr::host {

enum class PianoView { kKeyboard };

enum class OctaveDisplayMode { kBoundary, kAll, kNone };

struct PianoViewState {
  PianoView view = PianoView::kKeyboard;
  NoteNaming note_naming = NoteNaming::kCde;
  int base_octave = 4;       // piano_keys::kDefaultOctave
  std::uint8_t channel = 0;  // 0-based internal; displayed 1-based
  std::uint8_t velocity = 96;
  OctaveDisplayMode octave_display = OctaveDisplayMode::kAll;
};

struct PianoKeyBinding {
  char key;
  int semitone_from_base;
};

// Centralized computer-keyboard -> semitone map (single source of truth so the
// renderer and any future input handler never diverge). 'P'/'p' is reserved and
// intentionally never musical.
inline constexpr std::size_t kWhiteKeyCount = 11;
inline constexpr std::size_t kBlackKeyCount = 6;

const std::array<PianoKeyBinding, kWhiteKeyCount>& default_keymap_white();
const std::array<PianoKeyBinding, kBlackKeyCount>& default_keymap_black();

enum class KeyboardNoteLabelMode { kWhiteKey, kBlackKey };

// White-key labels are SPACED ("C 4", "Do 4"); black-key labels are unspaced
// ("C#4", "Do#4"). Always includes the octave; octave suppression is a
// render-level concern, not this primitive's.
std::string format_keyboard_note_label(std::uint8_t midi_note, NoteNaming naming,
                                       KeyboardNoteLabelMode mode);

// Renders the panel for the given terminal width. Never emits a line longer
// than terminal_columns (defensive truncation at the end).
std::vector<std::string> render_piano_panel(const PianoViewState& state, int terminal_columns);

}  // namespace arrangrr::host
