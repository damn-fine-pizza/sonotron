#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "midi_monitor.hpp"
#include "note_names.hpp"
#include "ui_style.hpp"

// Pure ASCII piano renderer, host side only: NO ANSI, NO terminal access, NO
// I/O. render_piano_panel() turns a view state and a column budget into a set
// of plain text lines; the caller owns cursor placement and colouring. Keeping
// this a pure function makes it trivially unit-testable and portable.

namespace arrangrr::host {

enum class PianoView { kKeyboard, kActiveNotes, kEventLog };

enum class OctaveDisplayMode { kBoundary, kAll, kNone };

struct PianoViewState {
  PianoView view = PianoView::kKeyboard;
  NoteNaming note_naming = NoteNaming::kCde;
  int base_octave = 4;       // piano_keys::kDefaultOctave
  std::uint8_t channel = 0;  // 0-based internal; displayed 1-based
  std::uint8_t velocity = 96;
  int transpose = 0;  // semitones added to played notes; always shown in the header
  OctaveDisplayMode octave_display = OctaveDisplayMode::kAll;
};

struct PianoKeyBinding {
  char key;
  int semitone_from_base;
};

// Centralized computer-keyboard -> semitone map (single source of truth so the
// renderer and any future input handler never diverge). 'P' is the D#5 black
// key (it sits above L/D5-;/E5); octave shift lives on '.'/'/' so no musical
// key doubles as a shortcut.
inline constexpr std::size_t kWhiteKeyCount = 11;
inline constexpr std::size_t kBlackKeyCount = 7;

const std::array<PianoKeyBinding, kWhiteKeyCount>& default_keymap_white();
const std::array<PianoKeyBinding, kBlackKeyCount>& default_keymap_black();

enum class KeyboardNoteLabelMode { kWhiteKey, kBlackKey };

// White-key labels are SPACED ("C 4", "Do 4"); black-key labels are unspaced
// ("C#4", "Do#4"). Always includes the octave; octave suppression is a
// render-level concern, not this primitive's.
std::string format_keyboard_note_label(std::uint8_t midi_note, NoteNaming naming,
                                       KeyboardNoteLabelMode mode);

// Renders the panel for the given terminal width. Never emits a line longer
// than terminal_columns (defensive truncation at the end). This overload
// renders the keyboard against an empty static monitor (no active notes, no
// events) — kept for callers that do not observe the output stream.
std::vector<std::string> render_piano_panel(const PianoViewState& state, int terminal_columns);

// Monitor-aware rendering (H2/H3): the keyboard marks active keys and shows a
// recent-events strip, and the active-notes / event-log views draw from the
// monitor's models and the caller's filter/display options. `style` applies the
// semantic UiRoles (active keys, note-on/off, drums); with colours disabled the
// output is byte-identical to plain rendering.
std::vector<std::string> render_piano_panel(const PianoViewState& state, int terminal_columns,
                                            const MidiMonitor& monitor,
                                            const MidiEventFilter& filter,
                                            const MidiViewOptions& options, const UiStyle& style);

}  // namespace arrangrr::host
