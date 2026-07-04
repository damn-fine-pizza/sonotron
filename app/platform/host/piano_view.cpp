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

// Who is sounding a given MIDI note right now: nothing, the user's piano keys,
// or the arranger/sequencer (external). Distinguished so the keyboard can show
// both but tell them apart (by colour) — see key_glyph.
enum class ActiveSource : std::uint8_t { kNone, kPiano, kOther };

// Indexed by MIDI note number so the check is a flat lookup.
using ActiveNoteMask = std::array<ActiveSource, piano_keys::kMidiNoteCount>;

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

// Event-log view: most-recent lines that pass the filter.
constexpr std::size_t kEventLogRows = 8;

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

// W E T Y U O P -> C# D# F# G# A# C#5 D#5. 'P' is the top black key (D#5).
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
  std::snprintf(buf, sizeof(buf), "Piano | keyboard | names:%s | oct:%d | ch:%u | vel:%u | tr:%+d",
                naming_word(state.note_naming), state.base_octave,
                static_cast<unsigned>(state.channel) + 1U, static_cast<unsigned>(state.velocity),
                state.transpose);

  return std::string(buf);
}

// A single output row that grows on demand; tokens are stamped at absolute
// VISIBLE columns so the composer never hand-concatenates spacing. Each cell
// holds one visible column's bytes: a plain " " by default, or a whole token
// (possibly SGR-wrapped) on the first column it occupies, with the remaining
// columns it spans left empty. Positioning therefore uses ansi::visible_length,
// never std::string::size(), so styled tokens never misalign their neighbours.
struct TextRow {
  std::vector<std::string> cells;

  void ensure(std::size_t col) {
    if (cells.size() <= col) {
      cells.resize(col + 1, " ");
    }
  }

  // Place `token` (visible width `width`) starting at visible column `col`.
  void put(std::size_t col, const std::string& token, std::size_t width) {
    if (width == 0) {
      return;
    }
    ensure(col + width - 1);
    cells[col] = token;
    for (std::size_t i = 1; i < width; ++i) {
      cells[col + i].clear();  // spanned column: contributes no bytes
    }
  }

  void put(std::size_t col, const std::string& token) {
    put(col, token, ansi::visible_length(token));
  }

  void put_centered(std::size_t center, const std::string& token) {
    const std::size_t width = ansi::visible_length(token);
    const std::size_t half = width / 2;
    const std::size_t start = center >= half ? center - half : 0;
    put(start, token, width);
  }

  std::string str() const {
    std::string out;
    for (const std::string& cell : cells) {
      out += cell;
    }
    return out;
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

// Renders a computer-key glyph, marking it when its note is sounding. With
// colours ON the single char is styled in place (piano = kPianoActiveKey,
// arranger = kMidiNoteOn) — SAME width, so the grid never shifts or truncates.
// With colours OFF there is no colour to tell sources apart, so only the user's
// own keys get the width-changing "*A*" marker; external notes stay plain.
std::string key_glyph(char key, ActiveSource source, const UiStyle& style) {
  const std::string glyph(1, key);
  if (source == ActiveSource::kNone) {
    return glyph;
  }

  const UiRole role =
      source == ActiveSource::kPiano ? UiRole::kPianoActiveKey : UiRole::kMidiNoteOn;
  if (style.colors_enabled()) {
    return style.apply(role, glyph);  // one visible column, colour-marked
  }

  return source == ActiveSource::kPiano ? "*" + glyph + "*" : glyph;
}

// Builds the four keyboard rows (black keys, black labels, white keys, white
// labels) for the wide/compact tiers. `gap` is the only tier-dependent input;
// `active` marks which MIDI notes are currently held.
std::vector<std::string> render_grid_keys(const PianoViewState& state, std::size_t gap,
                                          const ActiveNoteMask& active, const UiStyle& style) {
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
                                key_glyph(kWhiteKeys[i].key, active[white_midi[i]], style));
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
    black_keys_row.put_centered(center, key_glyph(kBlackKeys[b].key, active[black_midi[b]], style));
    black_labels_row.put_centered(center, black_labels[b]);
  }

  // A blank separator between the raised (black) group and the natural (white)
  // group reads as two physical key rows rather than one dense block.
  return {black_keys_row.str(), black_labels_row.str(), "", white_keys_row.str(),
          white_labels_row.str()};
}

// The narrow-but-usable fallback: keys grouped on labelled text lines. An
// active entry is wrapped whole ("*A/C 4*") so the marker never splits a label.
std::vector<std::string> render_minimal_keys(const PianoViewState& state, int terminal_columns,
                                             const ActiveNoteMask& active, const UiStyle& style) {
  // Entries are atomic ("W/C#4"): wrap onto a continuation row instead of
  // cutting a label mid-way when the terminal is narrow.
  const std::size_t width = static_cast<std::size_t>(terminal_columns);
  constexpr std::size_t kIndent = 2;
  constexpr std::size_t kEntryGap = 2;

  auto group = [&](const char* title, KeyboardNoteLabelMode mode, auto& keys, auto count) {
    std::vector<std::string> rows{title};
    std::string row(kIndent, ' ');
    // Track the row's visible width separately: `row` may carry SGR bytes once
    // an active entry is styled, so size() no longer measures layout columns.
    std::size_t row_visible = kIndent;

    for (std::size_t i = 0; i < count; ++i) {
      const std::uint8_t midi = midi_for(state, keys[i].semitone_from_base);
      std::string entry(1, keys[i].key);
      entry += '/';
      entry += key_label(state, midi, mode);
      std::size_t entry_visible = entry.size();
      const ActiveSource src = active[midi];
      if (src != ActiveSource::kNone) {
        const UiRole role =
            src == ActiveSource::kPiano ? UiRole::kPianoActiveKey : UiRole::kMidiNoteOn;
        if (style.colors_enabled()) {
          entry = style.apply(role, entry);  // same visible width
        } else if (src == ActiveSource::kPiano) {
          entry = "*" + entry + "*";
          entry_visible += 2;  // the surrounding "*...*" markers
        }
      }

      const bool row_has_entries = row_visible > kIndent;
      const std::size_t needed = row_visible + (row_has_entries ? kEntryGap : 0) + entry_visible;
      if (row_has_entries && needed > width) {
        rows.push_back(row);
        row.assign(kIndent, ' ');
        row_visible = kIndent;
      }
      if (row_visible > kIndent) {
        row += std::string(kEntryGap, ' ');
        row_visible += kEntryGap;
      }
      row += entry;
      row_visible += entry_visible;
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

  // Truncate by VISIBLE columns so styled lines keep their SGR runs intact and
  // never get cut mid-escape. For plain lines this is byte-for-byte identical to
  // a size()-based resize.
  for (std::string& line : lines) {
    line = ansi::visible_truncate(line, width);
  }
}

// Note name with octave and no internal spacing ("C4", "Do4").
std::string compact_note_name(std::uint8_t midi_note, NoteNaming naming) {
  return note_name(
      midi_note, NoteNameOptions{.naming = naming, .prefer_flats = false, .include_octave = true});
}

// Which MIDI notes are sounding right now and by whom, indexed by note. Piano
// keys (source_key != 0) always light; the arranger/sequencer output
// (source_key == 0) lights only when `show_external` is on, so pressing play
// does not make the whole chord look pressed unless the user asked to see it.
ActiveNoteMask active_note_mask(const MidiMonitor& monitor, bool show_external) {
  ActiveNoteMask mask{};

  const ActiveNoteTracker& tracker = monitor.active_notes();
  for (std::size_t i = 0; i < tracker.size(); ++i) {
    const ActiveNote& n = tracker.notes()[i];
    if (n.note >= piano_keys::kMidiNoteCount) {
      continue;
    }
    if (n.source_key != 0) {
      mask[n.note] = ActiveSource::kPiano;
    } else if (show_external && mask[n.note] == ActiveSource::kNone) {
      mask[n.note] = ActiveSource::kOther;
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
// A note event on the GM percussion channel reads as a drum; otherwise its
// note-on/note-off state picks the role. `is_drum` short-circuits both.
UiRole note_event_role(bool is_drum, bool active) {
  if (is_drum) {
    return UiRole::kMidiDrum;
  }
  return active ? UiRole::kMidiNoteOn : UiRole::kMidiNoteOff;
}

std::vector<std::string> render_keyboard(const PianoViewState& state, int terminal_columns,
                                         const MidiMonitor& monitor, const MidiViewOptions& options,
                                         const UiStyle& style) {
  // The keyboard view is just the header + the keys: the dedicated `events`
  // panel owns the live event stream now, so the old in-panel event strip is
  // redundant and only stole the height the keyboard needs. Active notes still
  // light up on the keys via the mask. (`piano view event-log` still shows
  // events inside the piano panel for anyone who wants them there.)
  std::vector<std::string> lines{header_line(state)};

  const ActiveNoteMask active = active_note_mask(monitor, options.show_external_keys);

  std::vector<std::string> keys;
  if (terminal_columns >= piano_layout::kWideMinColumns) {
    keys = render_grid_keys(state, piano_layout::kWideGap, active, style);
  } else if (terminal_columns >= piano_layout::kCompactMinColumns) {
    keys = render_grid_keys(state, piano_layout::kCompactGap, active, style);
  } else {
    keys = render_minimal_keys(state, terminal_columns, active, style);
  }

  lines.insert(lines.end(), keys.begin(), keys.end());

  return lines;
}

std::vector<std::string> render_active_notes(const PianoViewState& state,
                                             const MidiMonitor& monitor, const UiStyle& style) {
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
    // Held notes are, by definition, sounding: colour them note-on (or drum on
    // the GM percussion channel).
    const bool is_drum = channel == kGmDrumChannelZeroBased;
    lines.push_back(style.apply(note_event_role(is_drum, /*active=*/true), line));
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

// The styling role for a logged message: drums on ch10, then note-on/note-off;
// anything that is not a note stays neutral (kNormal is attribute-free).
UiRole log_role(const MidiMessage& msg, const MidiViewOptions& options) {
  const bool is_note_on = msg.type() == midi::kNoteOn && msg.d2 > 0;
  const bool is_note_off =
      msg.type() == midi::kNoteOff || (msg.type() == midi::kNoteOn && msg.d2 == 0);
  if (!is_note_on && !is_note_off) {
    return UiRole::kNormal;
  }
  if (options.show_drum_names && midi::is_channel_voice(msg.status) &&
      msg.channel() == kGmDrumChannelZeroBased) {
    return UiRole::kMidiDrum;
  }
  return is_note_on ? UiRole::kMidiNoteOn : UiRole::kMidiNoteOff;
}

std::string format_log_line(const PianoViewState& state, const MidiLogEvent& event, bool show_index,
                            const MidiViewOptions& options, const UiStyle& style) {
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

  return style.apply(log_role(event.msg, options), line);
}

std::vector<std::string> render_event_log(const PianoViewState& state, const MidiMonitor& monitor,
                                          const MidiEventFilter& filter,
                                          const MidiViewOptions& options, const UiStyle& style) {
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

    lines.push_back(format_log_line(state, event, share, options, style));
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
                                            const MidiViewOptions& options, const UiStyle& style) {
  if (terminal_columns < piano_layout::kMinimalMinColumns) {
    return {kTooNarrowMessage};
  }

  std::vector<std::string> lines;
  switch (state.view) {
    case PianoView::kKeyboard:
      lines = render_keyboard(state, terminal_columns, monitor, options, style);
      break;
    case PianoView::kActiveNotes:
      lines = render_active_notes(state, monitor, style);
      break;
    case PianoView::kEventLog:
      lines = render_event_log(state, monitor, filter, options, style);
      break;
  }

  truncate_lines(lines, terminal_columns);

  return lines;
}

std::vector<std::string> render_piano_panel(const PianoViewState& state, int terminal_columns) {
  // Host is single-threaded (D-host): a function-local static const empty
  // monitor is a cheap, allocation-free stand-in for callers that do not
  // observe the output stream. A default UiStyle keeps colours OFF, so this
  // overload's output is byte-identical to the pre-styling renderer.
  static const MidiMonitor kEmpty;
  static const MidiEventFilter kNoFilter;
  static const MidiViewOptions kDefaultOptions;
  static const UiStyle kNoColors;

  return render_piano_panel(state, terminal_columns, kEmpty, kNoFilter, kDefaultOptions, kNoColors);
}

}  // namespace arrangrr::host
