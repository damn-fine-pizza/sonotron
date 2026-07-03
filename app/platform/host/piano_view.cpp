#include "piano_view.hpp"

#include <cstdio>

namespace arrangrr::host {

namespace piano_keys {

constexpr int kDefaultOctave = 4;
constexpr int kMinOctave = -1;
constexpr int kMaxOctave = 9;
constexpr std::uint8_t kDefaultVelocity = 96;

constexpr int kSemitonesPerOctave = 12;
constexpr int kMaxMidiNote = 127;

}  // namespace piano_keys

// The named constants are the source of truth for the struct defaults.
static_assert(PianoViewState{}.base_octave == piano_keys::kDefaultOctave);
static_assert(PianoViewState{}.velocity == piano_keys::kDefaultVelocity);
static_assert(piano_keys::kMinOctave <= piano_keys::kDefaultOctave &&
              piano_keys::kDefaultOctave <= piano_keys::kMaxOctave);

namespace piano_layout {

// Tier selection thresholds (terminal columns).
constexpr int kWideMinColumns = 76;
constexpr int kCompactMinColumns = 56;
constexpr int kMinimalMinColumns = 24;

// Left margin before the first key and inter-cell gap per tier. The cell width
// itself is derived from the widest label so DoReMi ("Sol#4") stays aligned.
constexpr std::size_t kLead = 1;
constexpr std::size_t kWideGap = 4;
constexpr std::size_t kCompactGap = 2;

}  // namespace piano_layout

namespace {

constexpr const char* kTooNarrowMessage = "piano: terminal too narrow";

// Row/'A' A S D F G H J K L ; ' -> C D E F G A B (over ~1.5 octaves).
constexpr std::array<PianoKeyBinding, kWhiteKeyCount> kWhiteKeys = {{
    {'A', 0},
    {'S', 2},
    {'D', 4},
    {'F', 5},
    {'G', 7},
    {'H', 9},
    {'J', 11},
    {'K', 12},
    {'L', 14},
    {';', 16},
    {'\'', 17},
}};

// W E T Y U O -> C# D# F# G# A# C#(+octave). 'P' is deliberately absent.
constexpr std::array<PianoKeyBinding, kBlackKeyCount> kBlackKeys = {{
    {'W', 1},
    {'E', 3},
    {'T', 6},
    {'Y', 8},
    {'U', 10},
    {'O', 13},
}};

int octave_of(std::uint8_t midi_note) { return midi_note / piano_keys::kSemitonesPerOctave - 1; }

// Base note of octave N is (N + 1) * 12; add the binding offset and clamp to a
// legal MIDI note so extreme base octaves cannot overflow the wire type.
std::uint8_t midi_for(const PianoViewState& state, int semitone_from_base) {
  const int base_note = (state.base_octave + 1) * piano_keys::kSemitonesPerOctave;
  int note = base_note + semitone_from_base;

  if (note < 0) {
    note = 0;
  }
  if (note > piano_keys::kMaxMidiNote) {
    note = piano_keys::kMaxMidiNote;
  }

  return static_cast<std::uint8_t>(note);
}

const char* naming_word(NoteNaming naming) {
  return naming == NoteNaming::kDoReMi ? "DoReMi" : "CDE";
}

// The label shown on a key cell, honouring the octave-display policy. kAll and
// kBoundary both show the octave on every key for this renderer; kNone strips
// it (the header still reports the octave).
std::string key_label(const PianoViewState& state, std::uint8_t midi_note,
                      KeyboardNoteLabelMode mode) {
  if (state.octave_display == OctaveDisplayMode::kNone) {
    return pitch_class_name(midi_note, NoteNameOptions{state.note_naming, false, false});
  }

  return format_keyboard_note_label(midi_note, state.note_naming, mode);
}

std::string header_line(const PianoViewState& state) {
  char buf[128];
  std::snprintf(buf, sizeof(buf), "Piano | keyboard | names:%s | oct:%d | ch:%u | vel:%u",
                naming_word(state.note_naming), state.base_octave,
                static_cast<unsigned>(state.channel) + 1U, static_cast<unsigned>(state.velocity));

  return std::string(buf);
}

// A single output row that grows on demand; text is stamped at absolute
// columns so the row composer never hand-concatenates spacing.
struct TextRow {
  std::string text;

  void put(std::size_t col, char ch) {
    if (text.size() <= col) {
      text.resize(col + 1, ' ');
    }
    text[col] = ch;
  }

  void put(std::size_t col, const std::string& s) {
    for (std::size_t i = 0; i < s.size(); ++i) {
      put(col + i, s[i]);
    }
  }

  void put_centered(std::size_t center, const std::string& s) {
    const std::size_t half = s.size() / 2;
    const std::size_t start = center >= half ? center - half : 0;
    put(start, s);
  }
};

std::size_t max_label_width(const std::array<std::string, kWhiteKeyCount>& whites,
                            const std::array<std::string, kBlackKeyCount>& blacks) {
  std::size_t width = 1;

  for (const std::string& label : whites) {
    if (label.size() > width) {
      width = label.size();
    }
  }
  for (const std::string& label : blacks) {
    if (label.size() > width) {
      width = label.size();
    }
  }

  return width;
}

// Builds the four keyboard rows (black keys, black labels, white keys, white
// labels) for the wide/compact tiers. `gap` is the only tier-dependent input.
std::vector<std::string> render_grid(const PianoViewState& state, std::size_t gap) {
  std::array<std::string, kWhiteKeyCount> white_labels;
  std::array<std::string, kBlackKeyCount> black_labels;
  std::array<std::uint8_t, kWhiteKeyCount> white_midi;
  std::array<std::uint8_t, kBlackKeyCount> black_midi;

  for (std::size_t i = 0; i < kWhiteKeyCount; ++i) {
    white_midi[i] = midi_for(state, kWhiteKeys[i].semitone_from_base);
    white_labels[i] = key_label(state, white_midi[i], KeyboardNoteLabelMode::kWhiteKey);
  }
  for (std::size_t i = 0; i < kBlackKeyCount; ++i) {
    black_midi[i] = midi_for(state, kBlackKeys[i].semitone_from_base);
    black_labels[i] = key_label(state, black_midi[i], KeyboardNoteLabelMode::kBlackKey);
  }

  const std::size_t cell = max_label_width(white_labels, black_labels);
  const std::size_t stride = cell + gap;
  const std::size_t first_center = piano_layout::kLead + cell / 2;

  std::array<std::size_t, kWhiteKeyCount> white_center;
  for (std::size_t i = 0; i < kWhiteKeyCount; ++i) {
    white_center[i] = first_center + i * stride;
  }

  TextRow white_keys_row;
  TextRow white_labels_row;
  for (std::size_t i = 0; i < kWhiteKeyCount; ++i) {
    white_keys_row.put_centered(white_center[i], std::string(1, kWhiteKeys[i].key));
    white_labels_row.put_centered(white_center[i], white_labels[i]);
  }

  TextRow black_keys_row;
  TextRow black_labels_row;
  for (std::size_t b = 0; b < kBlackKeyCount; ++b) {
    // A black key sits between the two white keys that bracket its semitone.
    const int semitone = kBlackKeys[b].semitone_from_base;
    std::size_t right = kWhiteKeyCount;
    for (std::size_t w = 0; w < kWhiteKeyCount; ++w) {
      if (kWhiteKeys[w].semitone_from_base > semitone) {
        right = w;
        break;
      }
    }
    if (right == 0 || right >= kWhiteKeyCount) {
      continue;  // defensive: every black in the map has both neighbours
    }

    const std::size_t center = (white_center[right - 1] + white_center[right]) / 2;
    black_keys_row.put_centered(center, std::string(1, kBlackKeys[b].key));
    black_labels_row.put_centered(center, black_labels[b]);
  }

  return {header_line(state), black_keys_row.text, black_labels_row.text, white_keys_row.text,
          white_labels_row.text};
}

// The narrow-but-usable fallback: keys grouped on labelled text lines.
std::vector<std::string> render_minimal(const PianoViewState& state, int terminal_columns) {
  // Entries are atomic ("W/C#4"): wrap onto a continuation row instead of
  // cutting a label mid-way when the terminal is narrow.
  const std::size_t width = static_cast<std::size_t>(terminal_columns);
  constexpr std::size_t kIndent = 2;
  constexpr std::size_t kEntryGap = 2;

  auto group = [&](const char* title, KeyboardNoteLabelMode mode, auto& keys, auto count) {
    std::vector<std::string> rows{title};
    std::string row(kIndent, ' ');

    for (std::size_t i = 0; i < count; ++i) {
      const std::uint8_t midi = midi_for(state, keys[i].semitone_from_base);
      std::string entry(1, keys[i].key);
      entry += '/';
      entry += key_label(state, midi, mode);

      const bool row_has_entries = row.size() > kIndent;
      const std::size_t needed = row.size() + (row_has_entries ? kEntryGap : 0) + entry.size();
      if (row_has_entries && needed > width) {
        rows.push_back(row);
        row.assign(kIndent, ' ');
      }
      if (row.size() > kIndent) {
        row += std::string(kEntryGap, ' ');
      }
      row += entry;
    }

    rows.push_back(row);
    return rows;
  };

  std::vector<std::string> lines{header_line(state)};

  const std::vector<std::string> black =
      group("black:", KeyboardNoteLabelMode::kBlackKey, kBlackKeys, kBlackKeyCount);
  const std::vector<std::string> white =
      group("white:", KeyboardNoteLabelMode::kWhiteKey, kWhiteKeys, kWhiteKeyCount);

  lines.insert(lines.end(), black.begin(), black.end());
  lines.insert(lines.end(), white.begin(), white.end());

  return lines;
}

void truncate_lines(std::vector<std::string>& lines, int terminal_columns) {
  const std::size_t width = static_cast<std::size_t>(terminal_columns);

  for (std::string& line : lines) {
    if (line.size() > width) {
      line.resize(width);
    }
  }
}

}  // namespace

const std::array<PianoKeyBinding, kWhiteKeyCount>& default_keymap_white() { return kWhiteKeys; }

const std::array<PianoKeyBinding, kBlackKeyCount>& default_keymap_black() { return kBlackKeys; }

std::string format_keyboard_note_label(std::uint8_t midi_note, NoteNaming naming,
                                       KeyboardNoteLabelMode mode) {
  const std::string pitch = pitch_class_name(midi_note, NoteNameOptions{naming, false, false});

  char octave[8];
  std::snprintf(octave, sizeof(octave), "%d", octave_of(midi_note));

  std::string label = pitch;
  if (mode == KeyboardNoteLabelMode::kWhiteKey) {
    label += ' ';
  }
  label += octave;

  return label;
}

std::vector<std::string> render_piano_panel(const PianoViewState& state, int terminal_columns) {
  if (terminal_columns < piano_layout::kMinimalMinColumns) {
    return {kTooNarrowMessage};
  }

  std::vector<std::string> lines;
  if (terminal_columns >= piano_layout::kWideMinColumns) {
    lines = render_grid(state, piano_layout::kWideGap);
  } else if (terminal_columns >= piano_layout::kCompactMinColumns) {
    lines = render_grid(state, piano_layout::kCompactGap);
  } else {
    lines = render_minimal(state, terminal_columns);
  }

  truncate_lines(lines, terminal_columns);

  return lines;
}

}  // namespace arrangrr::host
