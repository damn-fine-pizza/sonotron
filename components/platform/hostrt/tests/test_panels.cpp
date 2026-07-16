// Host-only panel tests: note-name formatting (note_names) and the pure ASCII
// piano renderer (piano_view). Deterministic and terminal-free.

#include <string>
#include <vector>

#include "midi_monitor.hpp"
#include "note_names.hpp"
#include "panel_manager.hpp"
#include "piano_view.hpp"
#include "test.hpp"
#include "ui_style.hpp"

namespace {

using namespace arrangrr;
using namespace arrangrr::host;

// Builds the MidiMonitor's pure MidiOutEvent mirror (Seam D, §17.2): the
// monitor no longer sees the core OutEvent / abi.hpp, so these tests build
// the shape a MIDI-kind OutEvent would carry directly, exactly as Shell's
// sink does at the boundary.
MidiOutEvent midi_event(std::uint8_t port, const MidiMessage& msg, std::uint32_t tick) {
  return MidiOutEvent{.port = port, .msg = msg, .tick = tick};
}

bool any_line_contains(const std::vector<std::string>& lines, const char* needle) {
  for (const std::string& line : lines) {
    if (line.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

int count_occurrences(const std::string& haystack, const char* needle) {
  int n = 0;
  std::size_t pos = haystack.find(needle);
  while (pos != std::string::npos) {
    ++n;
    pos = haystack.find(needle, pos + 1);
  }
  return n;
}

const std::string& white_label_line(const std::vector<std::string>& lines) {
  // Wide/compact layout: header, black keys, black labels, white keys, white
  // labels. The white label row is the last line.
  return lines.back();
}

void test_note_name_cde() {
  const NoteNameOptions sharp{
      .naming = NoteNaming::kCde, .prefer_flats = false, .include_octave = true};
  CHECK(note_name(60, sharp) == "C4");
  CHECK(note_name(61, sharp) == "C#4");
  CHECK(note_name(59, sharp) == "B3");
  CHECK(note_name(0, sharp) == "C-1");
  CHECK(note_name(127, sharp) == "G9");

  const NoteNameOptions flat{
      .naming = NoteNaming::kCde, .prefer_flats = true, .include_octave = true};
  CHECK(note_name(61, flat) == "Db4");

  const NoteNameOptions no_octave{
      .naming = NoteNaming::kCde, .prefer_flats = false, .include_octave = false};
  CHECK(note_name(60, no_octave) == "C");
}

void test_note_name_doremi() {
  const NoteNameOptions sharp{
      .naming = NoteNaming::kDoReMi, .prefer_flats = false, .include_octave = true};
  CHECK(note_name(60, sharp) == "Do4");
  CHECK(note_name(61, sharp) == "Do#4");

  const NoteNameOptions flat{
      .naming = NoteNaming::kDoReMi, .prefer_flats = true, .include_octave = true};
  CHECK(note_name(61, flat) == "Reb4");
}

void test_pitch_class_ignores_octave() {
  const NoteNameOptions opts{
      .naming = NoteNaming::kCde, .prefer_flats = false, .include_octave = true};
  CHECK(pitch_class_name(60, opts) == "C");
  CHECK(pitch_class_name(72, opts) == "C");
  CHECK(pitch_class_name(61, opts) == "C#");
}

void test_keymap() {
  const auto& white = default_keymap_white();
  const auto& black = default_keymap_black();
  CHECK(white.size() == 11);
  CHECK(black.size() == 7);

  // 'P' is a black key (D#5), never a white key.
  for (const PianoKeyBinding& b : white) {
    CHECK(b.key != 'P' && b.key != 'p');
  }

  CHECK(white[0].key == 'A' && white[0].semitone_from_base == 0);
  CHECK(black[0].key == 'W' && black[0].semitone_from_base == 1);
  CHECK(white[1].key == 'S' && white[1].semitone_from_base == 2);
  CHECK(white[7].key == 'K' && white[7].semitone_from_base == 12);
  // 'P' extends the black row to D#5 (base C4 + 15 semitones).
  CHECK(black[6].key == 'P' && black[6].semitone_from_base == 15);
}

void test_keyboard_note_labels() {
  CHECK(format_keyboard_note_label(60, NoteNaming::kCde, KeyboardNoteLabelMode::kWhiteKey) ==
        "C 4");
  CHECK(format_keyboard_note_label(60, NoteNaming::kDoReMi, KeyboardNoteLabelMode::kWhiteKey) ==
        "Do 4");
  CHECK(format_keyboard_note_label(61, NoteNaming::kCde, KeyboardNoteLabelMode::kBlackKey) ==
        "C#4");
  CHECK(format_keyboard_note_label(61, NoteNaming::kDoReMi, KeyboardNoteLabelMode::kBlackKey) ==
        "Do#4");

  // The label a computer key produces at the default octave (base C4 = 60), so
  // "white key S renders D 4" is checked end-to-end through the shared keymap.
  const auto& white = default_keymap_white();
  const auto& black = default_keymap_black();
  constexpr int kBaseNote = 60;  // (4 + 1) * 12
  auto white_label = [&](std::size_t i) {
    return format_keyboard_note_label(
        static_cast<std::uint8_t>(kBaseNote + white[i].semitone_from_base), NoteNaming::kCde,
        KeyboardNoteLabelMode::kWhiteKey);
  };
  auto black_label = [&](std::size_t i) {
    return format_keyboard_note_label(
        static_cast<std::uint8_t>(kBaseNote + black[i].semitone_from_base), NoteNaming::kCde,
        KeyboardNoteLabelMode::kBlackKey);
  };
  CHECK(white_label(0) == "C 4");  // A
  CHECK(white_label(1) == "D 4");  // S
  CHECK(white_label(7) == "C 5");  // K
  CHECK(black_label(0) == "C#4");  // W
  CHECK(black_label(1) == "D#4");  // E
}

void test_render_tier_selection() {
  const PianoViewState state;

  // Wide: four keyboard rows plus a header, with white and black label rows.
  const std::vector<std::string> wide = render_piano_panel(state, 100);
  CHECK(wide.size() >= 5);
  CHECK(any_line_contains(wide, "C 4"));
  CHECK(any_line_contains(wide, "C#4"));

  // Compact still produces the multi-row layout.
  const std::vector<std::string> compact = render_piano_panel(state, 60);
  CHECK(compact.size() >= 5);
  CHECK(any_line_contains(compact, "C 4"));
  CHECK(any_line_contains(compact, "C#4"));

  // Minimal: grouped text with slash-joined key/label tokens.
  const std::vector<std::string> minimal = render_piano_panel(state, 40);
  CHECK(any_line_contains(minimal, "W/C#4"));
  CHECK(any_line_contains(minimal, "A/C 4"));

  // Too narrow: a single honest message.
  const std::vector<std::string> narrow = render_piano_panel(state, 10);
  CHECK(narrow.size() == 1);
  CHECK(narrow[0] == "piano: terminal too narrow");
}

void test_octave_display_modes() {
  PianoViewState show_all;
  show_all.octave_display = OctaveDisplayMode::kAll;
  const std::vector<std::string> all_lines = render_piano_panel(show_all, 100);
  // Every white key in the base octave carries " 4"; at least six are shown.
  CHECK(count_occurrences(white_label_line(all_lines), " 4") >= 6);

  PianoViewState no_octave;
  no_octave.octave_display = OctaveDisplayMode::kNone;
  const std::vector<std::string> none_lines = render_piano_panel(no_octave, 100);
  CHECK(count_occurrences(white_label_line(none_lines), " 4") == 0);
  // The header still advertises the octave even when key rows hide it.
  CHECK(any_line_contains(none_lines, "oct:4"));
}

void test_header_naming() {
  PianoViewState cde;
  cde.note_naming = NoteNaming::kCde;
  CHECK(any_line_contains(render_piano_panel(cde, 100), "names:CDE"));

  PianoViewState doremi;
  doremi.note_naming = NoteNaming::kDoReMi;
  CHECK(any_line_contains(render_piano_panel(doremi, 100), "names:DoReMi"));
}

void test_lines_never_exceed_width() {
  const PianoViewState state;
  for (int width : {100, 60, 40}) {
    for (const std::string& line : render_piano_panel(state, width)) {
      CHECK(static_cast<int>(line.size()) <= width);
    }
  }
}

void test_format_duration_ticks() {
  // Exact grid values carry a plain label.
  CHECK(format_duration_ticks(120) == "dur=120t 1/32");
  CHECK(format_duration_ticks(240) == "dur=240t 1/16");
  CHECK(format_duration_ticks(480) == "dur=480t 1/8");
  CHECK(format_duration_ticks(960) == "dur=960t 1/4");

  // Off-grid values approximate to the nearest label.
  CHECK(format_duration_ticks(230) == "dur=230t~1/16");
  CHECK(format_duration_ticks(100) == "dur=100t~1/32");

  // Above a quarter note: bare ticks, no coarser label to approximate.
  CHECK(format_duration_ticks(1920) == "dur=1920t");
}

void test_filter_passes() {
  MidiLogEvent on{};
  on.port = 0;
  on.msg = MidiMessage::note_on(0, 60, 100);

  MidiLogEvent off{};
  off.port = 1;
  off.msg = MidiMessage::note_off(0, 60);

  MidiLogEvent on_vel0{};
  on_vel0.port = 0;
  on_vel0.msg = MidiMessage::note_on(0, 60, 0);

  const MidiEventFilter any{};
  CHECK(filter_passes(any, on));
  CHECK(filter_passes(any, off));

  MidiEventFilter ch0{};
  ch0.channel = std::uint8_t{0};
  CHECK(filter_passes(ch0, on));
  MidiEventFilter ch1{};
  ch1.channel = std::uint8_t{1};
  CHECK(!filter_passes(ch1, on));

  MidiEventFilter p0{};
  p0.port = std::uint8_t{0};
  CHECK(filter_passes(p0, on));
  CHECK(!filter_passes(p0, off));  // off lives on port 1
  MidiEventFilter p1{};
  p1.port = std::uint8_t{1};
  CHECK(filter_passes(p1, off));

  MidiEventFilter on_kind{};
  on_kind.event_kind = MidiEventKindFilter::kNoteOn;
  CHECK(filter_passes(on_kind, on));
  CHECK(!filter_passes(on_kind, off));
  CHECK(!filter_passes(on_kind, on_vel0));  // note-on vel 0 is a note-off

  MidiEventFilter off_kind{};
  off_kind.event_kind = MidiEventKindFilter::kNoteOff;
  CHECK(!filter_passes(off_kind, on));
  CHECK(filter_passes(off_kind, off));
  CHECK(filter_passes(off_kind, on_vel0));

  // H3: drums (GM ch10 = 0-based 9), melodic, and a velocity floor.
  MidiLogEvent drum{};
  drum.msg = MidiMessage::note_on(kGmDrumChannelZeroBased, 36, 100);
  MidiLogEvent soft{};
  soft.msg = MidiMessage::note_on(0, 60, 40);

  MidiEventFilter drums_only{};
  drums_only.instrument = InstrumentFilter::kDrums;
  CHECK(filter_passes(drums_only, drum));
  CHECK(!filter_passes(drums_only, on));  // melodic channel excluded

  MidiEventFilter melodic_only{};
  melodic_only.instrument = InstrumentFilter::kMelodic;
  CHECK(filter_passes(melodic_only, on));
  CHECK(!filter_passes(melodic_only, drum));

  MidiEventFilter vel80{};
  vel80.velocity_min = std::uint8_t{80};
  CHECK(filter_passes(vel80, on));     // vel 100 >= 80
  CHECK(!filter_passes(vel80, soft));  // vel 40 hidden
  CHECK(filter_passes(vel80, off));    // the floor never hides note-offs
}

void test_gm_drum_names() {
  CHECK(std::string(gm_drum_name(36)) == "Kick");
  CHECK(std::string(gm_drum_name(38)) == "Snare");
  CHECK(std::string(gm_drum_name(42)) == "Closed HH");
  CHECK(gm_drum_name(34) == nullptr);  // below the GM range
  CHECK(gm_drum_name(60) == nullptr);  // in range but no conventional name
  CHECK(gm_drum_name(200) == nullptr);
}

void test_ui_style() {
  // Default construction: no TTY, so colors are OFF and every role returns the
  // text verbatim (no escape sequences leak into scripts/pipes).
  UiStyle off;
  CHECK(!off.colors_enabled());
  for (std::size_t i = 0; i < kUiRoleCount; ++i) {
    CHECK(off.apply(static_cast<UiRole>(i), "x") == "x");
  }

  // Mode resolution: kAuto follows the TTY probe; kOn/kOff override it.
  UiStyle s;
  s.set_terminal_is_tty(true);
  CHECK(s.colors_enabled());  // auto + tty
  s.set_color_mode(ColorMode::kOff);
  CHECK(!s.colors_enabled());
  s.set_color_mode(ColorMode::kOn);
  UiStyle no_tty;
  no_tty.set_color_mode(ColorMode::kOn);
  CHECK(no_tty.colors_enabled());  // kOn overrides the missing TTY

  // With colors on, apply wraps in SGR and always closes with a reset.
  const std::string styled = s.apply(UiRole::kPianoActiveKey, "A");
  CHECK(styled.find(ansi::kEscape) != std::string::npos);
  CHECK(styled.find("A") != std::string::npos);
  CHECK(styled.size() > 1 && styled.substr(styled.size() - ansi::kReset.size()) == ansi::kReset);

  // Default theme titles carry a real foreground colour (not bold-only), so
  // panels look styled even where bold is imperceptible.
  CHECK(s.theme_name() == "default");
  const std::string title = s.apply(UiRole::kPanelTitle, "X");
  CHECK(title.find("36") != std::string::npos);  // 36 = cyan foreground
  const std::string title_focused = s.apply(UiRole::kPanelTitleFocused, "X");
  CHECK(title_focused.find("36") != std::string::npos);  // distinct, still coloured
  CHECK(title_focused.find("7;") != std::string::npos);  // 7 = reverse video

  // mono theme uses attributes only — never a colour code (30..47).
  CHECK(s.set_theme("mono"));
  const std::string mono_drum = s.apply(UiRole::kMidiDrum, "Kick");
  CHECK(mono_drum.find("[3") == std::string::npos);  // no 3x foreground
  CHECK(mono_drum.find("[4") == std::string::npos);  // no 4x background

  // Theme catalogue + rejection of an unknown name (state unchanged).
  const std::vector<std::string> names = UiStyle::theme_names();
  for (const char* want : {"default", "mono", "high-contrast", "dark", "light", "matrix"}) {
    bool found = false;
    for (const std::string& n : names) {
      found = found || n == want;
    }
    CHECK(found);
  }
  CHECK(!s.set_theme("nonexistent"));
  CHECK(s.theme_name() == "mono");  // unchanged after a failed set

  // Unicode resolution mirrors colour resolution.
  UiStyle u;
  u.set_terminal_utf8(true);
  CHECK(u.unicode_enabled());  // auto + utf8
  u.set_unicode_mode(UnicodeMode::kOff);
  CHECK(!u.unicode_enabled());
}

void test_ansi_visible_helpers() {
  UiStyle s;
  s.set_terminal_is_tty(true);
  s.set_color_mode(ColorMode::kOn);
  const std::string colored = s.apply(UiRole::kError, "abc");

  // Visible width ignores the SGR bytes.
  CHECK(ansi::visible_length("abc") == 3);
  CHECK(ansi::visible_length(colored) == 3);

  // Truncation counts visible columns and keeps styling closed.
  CHECK(ansi::visible_truncate("abcdef", 3) == "abc");
  const std::string cut = ansi::visible_truncate(colored, 2);
  CHECK(ansi::visible_length(cut) == 2);
  CHECK(cut.substr(cut.size() - ansi::kReset.size()) == ansi::kReset);

  // Padding measures visible columns, not raw bytes.
  CHECK(ansi::visible_pad("ab", 5) == "ab   ");
  CHECK(ansi::visible_length(ansi::visible_pad(colored, 6)) == 6);
}

void test_grid_layout() {
  PanelManager pm;
  pm.set_content(PanelId::kPiano, {"p1", "p2"});
  pm.set_content(PanelId::kConsole, {"c1", "c2"});
  pm.open(PanelId::kPiano);
  pm.open(PanelId::kConsole);

  // One per row (default): the grid tiles to EXACTLY `rows` lines, stacked.
  const int rows = 14;
  const std::vector<std::string> one = pm.combined_lines(80, rows);
  CHECK(static_cast<int>(one.size()) == rows);
  CHECK(any_line_contains(one, "-- piano"));
  CHECK(any_line_contains(one, "-- console"));

  // Two per row: the pair shares a single row joined by the " | " gutter.
  pm.set_per_row(2);
  const std::vector<std::string> two = pm.combined_lines(120, rows);
  CHECK(static_cast<int>(two.size()) == rows);
  bool has_gutter = false;
  for (const std::string& line : two) {
    has_gutter = has_gutter || line.find(" | ") != std::string::npos;
  }
  CHECK(has_gutter);

  // toggle_layout flips back to one per row.
  pm.toggle_layout();
  CHECK(pm.per_row() == 1);
}

void test_active_note_tracker() {
  ActiveNote a{};
  a.note = 60;
  a.velocity = 100;

  ActiveNoteTracker tracker;
  CHECK(tracker.note_on(a));
  CHECK(tracker.size() == 1);

  // Retrigger replaces the velocity, does not grow.
  ActiveNote retrigger = a;
  retrigger.velocity = 40;
  CHECK(tracker.note_on(retrigger));
  CHECK(tracker.size() == 1);
  CHECK(tracker.notes()[0].velocity == 40);

  tracker.note_off(0, 0, 60);
  CHECK(tracker.size() == 0);

  // Capacity bound: 128 distinct notes fill the tracker; a 129th is rejected.
  ActiveNoteTracker full;
  for (int i = 0; i < 128; ++i) {
    ActiveNote n{};
    n.note = static_cast<std::uint8_t>(i);
    CHECK(full.note_on(n));
  }
  CHECK(full.size() == 128);

  ActiveNote extra{};
  extra.channel = 1;  // distinct from every held (port, channel, note)
  extra.note = 0;
  CHECK(!full.note_on(extra));
  CHECK(full.size() == 128);

  full.clear();
  CHECK(full.size() == 0);
}

void test_visual_event_buffer() {
  PianoVisualEventBuffer buf;

  PianoVisualEvent e{};
  e.note = 60;
  e.velocity = 100;
  e.active = true;
  e.start_tick = 10;
  buf.note_on(e);
  CHECK(buf.recent_events().size() == 1);
  CHECK(buf.recent_events()[0].active);

  // note_off sets the duration on the newest match.
  buf.note_off(0, 0, 60, 30);
  CHECK(!buf.recent_events()[0].active);
  CHECK(buf.recent_events()[0].end_tick == 30);

  // Two concurrent instances of the same note: off closes the newer one.
  PianoVisualEventBuffer newest;
  PianoVisualEvent older{};
  older.note = 62;
  older.active = true;
  older.start_tick = 5;
  newest.note_on(older);
  PianoVisualEvent newer = older;
  newer.start_tick = 15;
  newest.note_on(newer);
  newest.note_off(0, 0, 62, 20);
  const std::vector<PianoVisualEvent> pair = newest.recent_events();
  CHECK(pair.size() == 2);
  CHECK(pair[0].active);   // older instance still held
  CHECK(!pair[1].active);  // newer instance closed
  CHECK(pair[1].end_tick == 20);

  // Ring rotates at capacity: the 33rd note evicts the oldest.
  PianoVisualEventBuffer ring;
  for (int i = 0; i < 33; ++i) {
    PianoVisualEvent r{};
    r.note = static_cast<std::uint8_t>(i);
    r.active = true;
    r.start_tick = static_cast<std::uint32_t>(i);
    ring.note_on(r);
  }
  const std::vector<PianoVisualEvent> rotated = ring.recent_events();
  CHECK(rotated.size() == 32);
  CHECK(rotated.front().start_tick == 1);  // start_tick 0 evicted
  CHECK(rotated.back().start_tick == 32);

  ring.clear();
  CHECK(ring.recent_events().empty());
}

void test_midi_monitor_observe() {
  MidiMonitor monitor;
  monitor.observe(midi_event(0, MidiMessage::note_on(0, 60, 100), 10));
  CHECK(monitor.active_notes().size() == 1);
  CHECK(monitor.log_events(MidiEventFilter{}).size() == 1);
  CHECK(monitor.log_events(MidiEventFilter{})[0].same_tick_index == 0);

  monitor.observe(midi_event(0, MidiMessage::note_off(0, 60), 20));
  CHECK(monitor.active_notes().size() == 0);
  CHECK(monitor.log_events(MidiEventFilter{}).size() == 2);

  // same_tick_index increments among equal ticks and resets on a new tick.
  MidiMonitor ticks;
  ticks.observe(midi_event(0, MidiMessage::note_on(0, 60, 100), 100));
  ticks.observe(midi_event(0, MidiMessage::note_on(0, 62, 100), 100));
  ticks.observe(midi_event(0, MidiMessage::note_on(0, 64, 100), 100));
  ticks.observe(midi_event(0, MidiMessage::note_on(0, 65, 100), 200));
  const std::vector<MidiLogEvent> log = ticks.log_events(MidiEventFilter{});
  CHECK(log.size() == 4);
  CHECK(log[0].same_tick_index == 0);
  CHECK(log[1].same_tick_index == 1);
  CHECK(log[2].same_tick_index == 2);
  CHECK(log[3].same_tick_index == 0);

  // Log is bounded and keeps the newest entries.
  MidiMonitor capped;
  for (int i = 0; i < 300; ++i) {
    capped.observe(midi_event(0, MidiMessage::note_on(0, 60, 100), static_cast<std::uint32_t>(i)));
  }
  CHECK(capped.log_events(MidiEventFilter{}).size() == monitor_limits::kLogCapacity);
  CHECK(capped.log_events(MidiEventFilter{}).front().tick == 44);

  // log_events applies the filter.
  MidiMonitor filtered;
  filtered.observe(midi_event(0, MidiMessage::note_on(0, 60, 100), 10));
  filtered.observe(midi_event(0, MidiMessage::note_on(1, 62, 100), 10));
  MidiEventFilter only_ch1{};
  only_ch1.channel = std::uint8_t{1};
  CHECK(filtered.log_events(only_ch1).size() == 1);

  // Non-MIDI OutEvents are now excluded structurally (Seam D, §17.2): the
  // monitor's observe() only accepts the MidiOutEvent mirror, which cannot
  // represent any other OutEvent::Kind -- the caller (Shell) selects kMidi
  // before one is ever built (see shell.cpp's sink lambda).

  // Overflow flag trips once the 129th distinct held note is rejected.
  MidiMonitor overflow;
  for (int i = 0; i < 128; ++i) {
    overflow.observe(midi_event(0, MidiMessage::note_on(0, static_cast<std::uint8_t>(i), 100), 0));
  }
  CHECK(!overflow.tracker_overflowed());
  overflow.observe(midi_event(1, MidiMessage::note_on(0, 0, 100), 0));  // distinct via port
  CHECK(overflow.tracker_overflowed());

  overflow.clear();
  CHECK(!overflow.tracker_overflowed());
  CHECK(overflow.active_notes().size() == 0);
}

void test_monitor_renderers() {
  MidiMonitor monitor;
  monitor.observe(midi_event(0, MidiMessage::note_on(0, 60, 96), 100), 'A');

  const MidiEventFilter filter{};
  const MidiViewOptions options{};
  const UiStyle plain{};  // colours off: output stays byte-identical to plain

  // Keyboard view: the 'A' key (note 60 at base octave 4) is marked. (Events no
  // longer appear here — the dedicated `events` panel owns the stream.)
  PianoViewState keyboard;
  const std::vector<std::string> kb =
      render_piano_panel(keyboard, 100, monitor, filter, options, plain);
  CHECK(any_line_contains(kb, "*A*"));

  // Active-notes view: grouped by channel (1-based), notes named.
  PianoViewState active = keyboard;
  active.view = PianoView::kActiveNotes;
  const std::vector<std::string> an =
      render_piano_panel(active, 100, monitor, filter, options, plain);
  CHECK(any_line_contains(an, "ch1:"));
  CHECK(any_line_contains(an, "C4"));

  PianoViewState active_doremi = active;
  active_doremi.note_naming = NoteNaming::kDoReMi;
  const std::vector<std::string> an_doremi =
      render_piano_panel(active_doremi, 100, monitor, filter, options, plain);
  CHECK(any_line_contains(an_doremi, "Do4"));

  // Event-log view: "@<tick>" lines, velocity honours the option.
  PianoViewState log = keyboard;
  log.view = PianoView::kEventLog;
  const std::vector<std::string> el = render_piano_panel(log, 100, monitor, filter, options, plain);
  CHECK(any_line_contains(el, "@100"));
  CHECK(any_line_contains(el, "vel"));

  MidiViewOptions no_velocity = options;
  no_velocity.show_velocity = false;
  const std::vector<std::string> el_no_vel =
      render_piano_panel(log, 100, monitor, filter, no_velocity, plain);
  CHECK(!any_line_contains(el_no_vel, "vel"));

  // Every view keeps lines within the width budget.
  for (const std::vector<std::string>* view : {&kb, &an, &el}) {
    for (const std::string& line : *view) {
      CHECK(static_cast<int>(line.size()) <= 100);
    }
  }
}

// The first line containing `needle`, or nullptr. In the colours-on renders the
// styling wraps whole lines, so the plain needle survives as a substring.
const std::string* find_line(const std::vector<std::string>& lines, const char* needle) {
  for (const std::string& line : lines) {
    if (line.find(needle) != std::string::npos) {
      return &line;
    }
  }
  return nullptr;
}

bool has_escape(const std::string& s) { return s.find(ansi::kEscape) != std::string::npos; }

void test_piano_styling() {
  MidiMonitor monitor;
  monitor.observe(midi_event(0, MidiMessage::note_on(0, 60, 96), 100), 'A');

  const MidiEventFilter filter{};
  const MidiViewOptions options{};

  UiStyle on;
  on.set_color_mode(ColorMode::kOn);  // default theme, colours forced on
  const UiStyle off{};                // colours off

  // --- Keyboard: the held glyph is SGR-wrapped, and columns still line up. ---
  PianoViewState keyboard;
  const std::vector<std::string> kb_off =
      render_piano_panel(keyboard, 100, monitor, filter, options, off);
  const std::vector<std::string> kb_on =
      render_piano_panel(keyboard, 100, monitor, filter, options, on);
  CHECK(kb_off.size() == kb_on.size());

  // Every keyboard row keeps identical VISIBLE width with colours on and off,
  // even though the on-render carries extra SGR bytes: proof the composer
  // positions by visible column, not byte size.
  for (std::size_t i = 0; i < kb_off.size(); ++i) {
    CHECK(ansi::visible_length(kb_on[i]) == kb_off[i].size());
    CHECK(ansi::visible_length(kb_on[i]) == ansi::visible_length(kb_off[i]));
  }

  // The row bearing the active "*A*" glyph gains escapes (and bytes) on.
  for (std::size_t i = 0; i < kb_off.size(); ++i) {
    if (kb_off[i].find("*A*") != std::string::npos) {
      CHECK(has_escape(kb_on[i]));
      CHECK(kb_on[i].size() > kb_off[i].size());
    }
  }
  CHECK(any_line_contains(kb_off, "*A*"));  // off-render is plain
  // The keyboard view no longer carries an event strip (events live in the
  // dedicated `events` panel now); event styling is covered by the views below.

  // --- Active-notes: the channel line is styled on, plain off. ---
  PianoViewState active = keyboard;
  active.view = PianoView::kActiveNotes;
  const std::vector<std::string> an_off =
      render_piano_panel(active, 100, monitor, filter, options, off);
  const std::vector<std::string> an_on =
      render_piano_panel(active, 100, monitor, filter, options, on);
  const std::string* an_line_off = find_line(an_off, "ch1:");
  const std::string* an_line_on = find_line(an_on, "ch1:");
  CHECK(an_line_off != nullptr && an_line_on != nullptr);
  CHECK(!has_escape(*an_line_off));
  CHECK(has_escape(*an_line_on));

  // --- Event-log: the note-on entry is styled on, plain off. ---
  PianoViewState log = keyboard;
  log.view = PianoView::kEventLog;
  const std::vector<std::string> el_off =
      render_piano_panel(log, 100, monitor, filter, options, off);
  const std::vector<std::string> el_on = render_piano_panel(log, 100, monitor, filter, options, on);
  const std::string* el_line_off = find_line(el_off, "@100");
  const std::string* el_line_on = find_line(el_on, "@100");
  CHECK(el_line_off != nullptr && el_line_on != nullptr);
  CHECK(!has_escape(*el_line_off));
  CHECK(has_escape(*el_line_on));
}

// Roadmap 11410: the keyboard harmony overlay colours keys by the FOLLOWED
// chord — green for the committed chord (this bar), amber for the staged next
// chord — distinct from the live/arranger sounding notes, and still legible
// with colours off.
//
// This exercises the pure render seam: PianoChordOverlay is constructed
// directly here, as Shell::refresh_piano_content would once it has already
// decided the followed chord is ACTIVE (transport playing or an explicit
// steer) — the gate itself is Shell-side production logic, covered end to end
// by test_piano_harmony_gate_at_rest (test_host.cpp), which also proves the
// at-rest home-tonic default paints NO overlay green.
void test_piano_harmony_overlay() {
  // chord_pitch_class_set stacks the quality's chord tones on the root.
  const ChordState c_major{.root_pc = 0, .quality = ChordQuality::kMaj, .valid = true};
  CHECK(chord_pitch_class_set(c_major) == ((1U << 0) | (1U << 4) | (1U << 7)));  // C E G
  const ChordState g_major{.root_pc = 7, .quality = ChordQuality::kMaj, .valid = true};
  CHECK(chord_pitch_class_set(g_major) == ((1U << 7) | (1U << 11) | (1U << 2)));  // G B D
  CHECK(chord_pitch_class_set(ChordState{}) == 0);  // invalid: no tones

  const MidiEventFilter filter{};
  const MidiViewOptions options{};

  UiStyle on;
  on.set_color_mode(ColorMode::kOn);  // default theme, colours forced on
  const UiStyle off{};                // colours off: glyph-marker fallback

  // Committed C major (green), pending G major (amber). They share G (pc 7): the
  // committed chord must win that key. Base octave 4, so on the white row:
  //   'A'=C4 'D'=E4 'G'=G4  committed;  'S'=D4 'J'=B4 pending;  green 'G' wins.
  const PianoChordOverlay overlay{
      .committed_pcs = chord_pitch_class_set(c_major),
      .pending_pcs = chord_pitch_class_set(g_major),
  };

  // (a)+(b): with colours on, committed keys carry the green note-on role and
  // pending keys carry the new amber role, and the two roles are visibly
  // different (green != amber).
  const MidiMonitor empty;  // no live/arranger notes: the overlay owns every key
  const PianoViewState keyboard;
  const std::vector<std::string> on_lines =
      render_piano_panel(keyboard, 100, empty, filter, options, on, overlay);

  const std::string green_a = on.apply(UiRole::kMidiNoteOn, "A");       // C4 committed
  const std::string green_d = on.apply(UiRole::kMidiNoteOn, "D");       // E4 committed
  const std::string green_g = on.apply(UiRole::kMidiNoteOn, "G");       // G4 committed (shared pc)
  const std::string amber_s = on.apply(UiRole::kMidiNotePending, "S");  // D4 pending
  const std::string amber_j = on.apply(UiRole::kMidiNotePending, "J");  // B4 pending
  const std::string amber_g = on.apply(UiRole::kMidiNotePending, "G");  // must NOT appear
  CHECK(green_a != amber_s);  // green and amber are visibly distinct roles
  CHECK(any_line_contains(on_lines, green_a.c_str()));
  CHECK(any_line_contains(on_lines, green_d.c_str()));
  CHECK(any_line_contains(on_lines, green_g.c_str()));
  CHECK(any_line_contains(on_lines, amber_s.c_str()));
  CHECK(any_line_contains(on_lines, amber_j.c_str()));
  CHECK(!any_line_contains(on_lines, amber_g.c_str()));  // committed wins the shared G

  // A key already sounding keeps its own colour: a live piano C4 stays the piano
  // key, not committed-green.
  MidiMonitor live;
  live.observe(midi_event(0, MidiMessage::note_on(0, 60, 96), 100), 'A');
  const std::vector<std::string> on_live =
      render_piano_panel(keyboard, 100, live, filter, options, on, overlay);
  CHECK(any_line_contains(on_live, on.apply(UiRole::kPianoActiveKey, "A").c_str()));
  CHECK(!any_line_contains(on_live, green_a.c_str()));  // piano beat the overlay

  // (c): colours off still distinguishes the sources by glyph marker — piano
  // "*A*", pending "(S)"/"(J)"; committed stays plain (no marker).
  const std::vector<std::string> off_lines =
      render_piano_panel(keyboard, 100, live, filter, options, off, overlay);
  CHECK(any_line_contains(off_lines, "*A*"));  // live piano key
  CHECK(any_line_contains(off_lines, "(S)"));  // pending amber, colours-off marker
  CHECK(any_line_contains(off_lines, "(J)"));
  CHECK(!any_line_contains(off_lines, "(A)"));  // A is the piano key, not pending
  CHECK(!any_line_contains(off_lines, "(D)"));  // D is committed -> plain, unmarked
  CHECK(!any_line_contains(off_lines, "*D*"));
  for (const std::string& line : off_lines) {  // colours off: no SGR anywhere
    CHECK(!has_escape(line));
  }

  // No overlay -> byte-identical to the pre-11410 render (no regression).
  const std::vector<std::string> plain_no_overlay =
      render_piano_panel(keyboard, 100, empty, filter, options, on);
  const std::vector<std::string> plain_empty_overlay =
      render_piano_panel(keyboard, 100, empty, filter, options, on, PianoChordOverlay{});
  CHECK(plain_no_overlay == plain_empty_overlay);
  CHECK(!any_line_contains(plain_empty_overlay, green_a.c_str()));  // empty overlay lights nothing
}

}  // namespace

int main() {
  test_note_name_cde();
  test_note_name_doremi();
  test_pitch_class_ignores_octave();
  test_keymap();
  test_keyboard_note_labels();
  test_render_tier_selection();
  test_octave_display_modes();
  test_header_naming();
  test_lines_never_exceed_width();
  test_format_duration_ticks();
  test_filter_passes();
  test_gm_drum_names();
  test_ui_style();
  test_ansi_visible_helpers();
  test_grid_layout();
  test_active_note_tracker();
  test_visual_event_buffer();
  test_midi_monitor_observe();
  test_monitor_renderers();
  test_piano_styling();
  test_piano_harmony_overlay();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_panels: all OK\n");
  }
  return arrangrr::test::failures();
}
