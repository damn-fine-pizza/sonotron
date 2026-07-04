#include "piano_view.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

namespace arrangrr::host {

namespace piano_keys {

constexpr int kDefaultOctave = 4;
constexpr int kMinOctave = -1;
constexpr int kMaxOctave = 9;
constexpr std::uint8_t kDefaultVelocity = 96;

constexpr int kSemitonesPerOctave = 12;
constexpr int kMaxMidiNote = 127;
constexpr std::size_t kMidiNoteCount = 128;
constexpr unsigned kMidiChannelCount = 16;

}  // namespace piano_keys

// A key is "active" when a note it maps to is currently held; the marker set is
// indexed by MIDI note number so the check is a flat lookup.
using ActiveNoteMask = std::array<bool, piano_keys::kMidiNoteCount>;

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

namespace monitor_view {

// Keyboard events strip: cosmetic, fixed-length activity bar (no animation).
constexpr std::size_t kBarDashes = 7;          // active bar: "-------*"
constexpr std::size_t kBarReleasedDashes = 3;  // released bar: "---o    "

// Event-log view: most-recent lines that pass the filter.
constexpr std::size_t kEventLogRows = 8;

// "source:note" column width in the keyboard events strip (e.g. "p0ch10:F#2").
constexpr std::size_t kEventLabelWidth = 12;

}  // namespace monitor_view

namespace {

constexpr const char* kTooNarrowMessage = "piano: terminal too narrow";

// Row/'A' A S D F G H J K L ; ' -> C D E F G A B (over ~1.5 octaves).
constexpr std::array<PianoKeyBinding, kWhiteKeyCount> kWhiteKeys = {{
    {.key = 'A', .semitone_from_base = 0},
    {.key = 'S', .semitone_from_base = 2},
    {.key = 'D', .semitone_from_base = 4},
    {.key = 'F', .semitone_from_base = 5},
    {.key = 'G', .semitone_from_base = 7},
    {.key = 'H', .semitone_from_base = 9},
    {.key = 'J', .semitone_from_base = 11},
    {.key = 'K', .semitone_from_base = 12},
    {.key = 'L', .semitone_from_base = 14},
    {.key = ';', .semitone_from_base = 16},
    {.key = '\'', .semitone_from_base = 17},
}};

// W E T Y U O -> C# D# F# G# A# C#(+octave). 'P' is deliberately absent.
constexpr std::array<PianoKeyBinding, kBlackKeyCount> kBlackKeys = {{
    {.key = 'W', .semitone_from_base = 1},
    {.key = 'E', .semitone_from_base = 3},
    {.key = 'T', .semitone_from_base = 6},
    {.key = 'Y', .semitone_from_base = 8},
    {.key = 'U', .semitone_from_base = 10},
    {.key = 'O', .semitone_from_base = 13},
    {.key = 'P', .semitone_from_base = 15},
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
    return pitch_class_name(midi_note, NoteNameOptions{.naming = state.note_naming,
                                                       .prefer_flats = false,
                                                       .include_octave = false});
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

// Wraps a computer-key glyph as "*A*" when its note is currently held.
std::string key_glyph(char key, bool active) {
  std::string glyph(1, key);
  if (active) {
    glyph = "*" + glyph + "*";
  }

  return glyph;
}

// Builds the four keyboard rows (black keys, black labels, white keys, white
// labels) for the wide/compact tiers. `gap` is the only tier-dependent input;
// `active` marks which MIDI notes are currently held.
std::vector<std::string> render_grid_keys(const PianoViewState& state, std::size_t gap,
                                          const ActiveNoteMask& active) {
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
    white_keys_row.put_centered(white_center[i],
                                key_glyph(kWhiteKeys[i].key, active[white_midi[i]]));
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
    black_keys_row.put_centered(center, key_glyph(kBlackKeys[b].key, active[black_midi[b]]));
    black_labels_row.put_centered(center, black_labels[b]);
  }

  // A blank separator between the raised (black) group and the natural (white)
  // group reads as two physical key rows rather than one dense block.
  return {black_keys_row.text, black_labels_row.text, "", white_keys_row.text,
          white_labels_row.text};
}

// The narrow-but-usable fallback: keys grouped on labelled text lines. An
// active entry is wrapped whole ("*A/C 4*") so the marker never splits a label.
std::vector<std::string> render_minimal_keys(const PianoViewState& state, int terminal_columns,
                                             const ActiveNoteMask& active) {
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
      if (active[midi]) {
        entry = "*" + entry + "*";
      }

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

  std::vector<std::string> lines;

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

// Note name with octave and no internal spacing ("C4", "Do4").
std::string compact_note_name(std::uint8_t midi_note, NoteNaming naming) {
  return note_name(
      midi_note, NoteNameOptions{.naming = naming, .prefer_flats = false, .include_octave = true});
}

// Which computer-keyboard keys are lit right now, indexed by MIDI note.
ActiveNoteMask active_note_mask(const MidiMonitor& monitor) {
  ActiveNoteMask mask{};

  const ActiveNoteTracker& tracker = monitor.active_notes();
  for (std::size_t i = 0; i < tracker.size(); ++i) {
    const std::uint8_t note = tracker.notes()[i].note;
    if (note < piano_keys::kMidiNoteCount) {
      mask[note] = true;
    }
  }

  return mask;
}

std::string simple_header(const char* view_name, const PianoViewState& state) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "Piano | %s | names:%s", view_name,
                naming_word(state.note_naming));

  return std::string(buf);
}

// One line of the keyboard-view events strip. `source_key` 0 means external, so
// we print the port/channel origin instead of a computer key.
std::string format_event_line(const PianoViewState& state, const PianoVisualEvent& event,
                              const MidiViewOptions& options) {
  std::string source;
  if (event.source_key != 0) {
    source = std::string(1, event.source_key);
  } else {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "p%uch%u", static_cast<unsigned>(event.port),
                  static_cast<unsigned>(event.channel) + 1U);
    source = buf;
  }

  std::string bar;
  if (event.active) {
    bar = std::string(monitor_view::kBarDashes, '-') + '*';
  } else {
    bar = std::string(monitor_view::kBarReleasedDashes, '-') + 'o' +
          std::string(monitor_view::kBarDashes - monitor_view::kBarReleasedDashes, ' ');
  }

  std::string tail;
  if (event.active) {
    if (options.show_velocity) {
      char buf[8];
      std::snprintf(buf, sizeof(buf), "v%u", static_cast<unsigned>(event.velocity));
      tail = buf;
    }
  } else {
    tail = format_duration_ticks(event.end_tick - event.start_tick);
  }

  // Pad the "source:note" field to a fixed width so every event row columns
  // up regardless of key vs port/channel origin or note-name length.
  std::string label = source + ":" + compact_note_name(event.note, state.note_naming);
  if (label.size() < monitor_view::kEventLabelWidth) {
    label.resize(monitor_view::kEventLabelWidth, ' ');
  }

  std::string line = "  ";
  line += label;
  line += "  ";
  line += bar;
  line += ' ';
  line += event.active ? "on " : "off";
  if (!tail.empty()) {
    line += "  ";
    line += tail;
  }

  return line;
}

std::vector<std::string> render_keyboard(const PianoViewState& state, int terminal_columns,
                                         const MidiMonitor& monitor,
                                         const MidiViewOptions& options) {
  std::vector<std::string> lines{header_line(state)};

  // Recent-events strip: the newest monitor_limits::kVisualEventRows, newest
  // last, between the header and the keys.
  const std::vector<PianoVisualEvent> recent = monitor.visual_events().recent_events();
  const std::size_t shown = std::min(recent.size(), monitor_limits::kVisualEventRows);
  for (std::size_t i = recent.size() - shown; i < recent.size(); ++i) {
    lines.push_back(format_event_line(state, recent[i], options));
  }

  const ActiveNoteMask active = active_note_mask(monitor);

  std::vector<std::string> keys;
  if (terminal_columns >= piano_layout::kWideMinColumns) {
    keys = render_grid_keys(state, piano_layout::kWideGap, active);
  } else if (terminal_columns >= piano_layout::kCompactMinColumns) {
    keys = render_grid_keys(state, piano_layout::kCompactGap, active);
  } else {
    keys = render_minimal_keys(state, terminal_columns, active);
  }

  lines.insert(lines.end(), keys.begin(), keys.end());

  return lines;
}

std::vector<std::string> render_active_notes(const PianoViewState& state,
                                             const MidiMonitor& monitor) {
  std::vector<std::string> lines{simple_header("active-notes", state)};

  const ActiveNoteTracker& tracker = monitor.active_notes();
  if (tracker.size() == 0) {
    lines.push_back("  (no active notes)");
    return lines;
  }

  // Channels ascending; notes ascending within a channel; port is not shown.
  for (unsigned channel = 0; channel < piano_keys::kMidiChannelCount; ++channel) {
    std::vector<std::uint8_t> notes;
    for (std::size_t i = 0; i < tracker.size(); ++i) {
      const ActiveNote& note = tracker.notes()[i];
      if (note.channel == channel) {
        notes.push_back(note.note);
      }
    }
    if (notes.empty()) {
      continue;
    }

    std::sort(notes.begin(), notes.end());

    std::string line = "ch" + std::to_string(channel + 1) + ":";
    for (const std::uint8_t note : notes) {
      line += ' ';
      line += compact_note_name(note, state.note_naming);
    }
    lines.push_back(line);
  }

  return lines;
}

// A note-log entry names its kind; anything that is not a note is generic.
const char* log_kind(const MidiMessage& msg) {
  if (msg.type() == midi::kNoteOn && msg.d2 > 0) {
    return "note-on ";
  }
  if (msg.type() == midi::kNoteOff || (msg.type() == midi::kNoteOn && msg.d2 == 0)) {
    return "note-off";
  }

  return "other   ";
}

std::string format_log_line(const PianoViewState& state, const MidiLogEvent& event, bool show_index,
                            const MidiViewOptions& options) {
  std::string line = "@" + std::to_string(event.tick);
  if (show_index) {
    line += "#" + std::to_string(event.same_tick_index);
  }
  line += "  ";

  if (options.show_port) {
    line += "p" + std::to_string(event.port) + " ";
  }
  if (options.show_channel) {
    line += "ch" + std::to_string(static_cast<unsigned>(event.msg.channel()) + 1U) + "  ";
  }

  line += log_kind(event.msg);

  if (options.show_note_numbers) {
    line += "  " + std::to_string(static_cast<unsigned>(event.msg.d1));
  }
  if (options.show_note_names) {
    line += ' ';
    line += compact_note_name(event.msg.d1, state.note_naming);
  }
  if (options.show_velocity) {
    line += "  vel " + std::to_string(static_cast<unsigned>(event.msg.d2));
  }

  return line;
}

std::vector<std::string> render_event_log(const PianoViewState& state, const MidiMonitor& monitor,
                                          const MidiEventFilter& filter,
                                          const MidiViewOptions& options) {
  std::vector<std::string> lines{simple_header("event-log", state)};

  const std::vector<MidiLogEvent> events = monitor.log_events(filter);
  const std::size_t shown = std::min(events.size(), monitor_view::kEventLogRows);
  const std::size_t first = events.size() - shown;

  for (std::size_t i = first; i < events.size(); ++i) {
    const MidiLogEvent& event = events[i];

    // "#i" only when this event is one of several visible on the same tick.
    bool share = event.same_tick_index > 0;
    for (std::size_t j = first; !share && j < events.size(); ++j) {
      if (j != i && events[j].tick == event.tick) {
        share = true;
      }
    }

    lines.push_back(format_log_line(state, event, share, options));
  }

  return lines;
}

}  // namespace

const std::array<PianoKeyBinding, kWhiteKeyCount>& default_keymap_white() { return kWhiteKeys; }

const std::array<PianoKeyBinding, kBlackKeyCount>& default_keymap_black() { return kBlackKeys; }

std::string format_keyboard_note_label(std::uint8_t midi_note, NoteNaming naming,
                                       KeyboardNoteLabelMode mode) {
  const std::string pitch = pitch_class_name(
      midi_note, NoteNameOptions{.naming = naming, .prefer_flats = false, .include_octave = false});

  char octave[8];
  std::snprintf(octave, sizeof(octave), "%d", octave_of(midi_note));

  std::string label = pitch;
  if (mode == KeyboardNoteLabelMode::kWhiteKey) {
    label += ' ';
  }
  label += octave;

  return label;
}

std::vector<std::string> render_piano_panel(const PianoViewState& state, int terminal_columns,
                                            const MidiMonitor& monitor,
                                            const MidiEventFilter& filter,
                                            const MidiViewOptions& options) {
  if (terminal_columns < piano_layout::kMinimalMinColumns) {
    return {kTooNarrowMessage};
  }

  std::vector<std::string> lines;
  switch (state.view) {
    case PianoView::kKeyboard:
      lines = render_keyboard(state, terminal_columns, monitor, options);
      break;
    case PianoView::kActiveNotes:
      lines = render_active_notes(state, monitor);
      break;
    case PianoView::kEventLog:
      lines = render_event_log(state, monitor, filter, options);
      break;
  }

  truncate_lines(lines, terminal_columns);

  return lines;
}

std::vector<std::string> render_piano_panel(const PianoViewState& state, int terminal_columns) {
  // Host is single-threaded (D-host): a function-local static const empty
  // monitor is a cheap, allocation-free stand-in for callers that do not
  // observe the output stream.
  static const MidiMonitor kEmpty;
  static const MidiEventFilter kNoFilter;
  static const MidiViewOptions kDefaultOptions;

  return render_piano_panel(state, terminal_columns, kEmpty, kNoFilter, kDefaultOptions);
}

}  // namespace arrangrr::host
