// Host-only panel tests: note-name formatting (note_names) and the pure ASCII
// piano renderer (piano_view). Deterministic and terminal-free.

#include <string>
#include <vector>

#include "arrangrr/abi.hpp"
#include "midi_monitor.hpp"
#include "note_names.hpp"
#include "piano_view.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;
using namespace arrangrr::host;

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
  const NoteNameOptions sharp{NoteNaming::kCde, false, true};
  CHECK(note_name(60, sharp) == "C4");
  CHECK(note_name(61, sharp) == "C#4");
  CHECK(note_name(59, sharp) == "B3");
  CHECK(note_name(0, sharp) == "C-1");
  CHECK(note_name(127, sharp) == "G9");

  const NoteNameOptions flat{NoteNaming::kCde, true, true};
  CHECK(note_name(61, flat) == "Db4");

  const NoteNameOptions no_octave{NoteNaming::kCde, false, false};
  CHECK(note_name(60, no_octave) == "C");
}

void test_note_name_doremi() {
  const NoteNameOptions sharp{NoteNaming::kDoReMi, false, true};
  CHECK(note_name(60, sharp) == "Do4");
  CHECK(note_name(61, sharp) == "Do#4");

  const NoteNameOptions flat{NoteNaming::kDoReMi, true, true};
  CHECK(note_name(61, flat) == "Reb4");
}

void test_pitch_class_ignores_octave() {
  const NoteNameOptions opts{NoteNaming::kCde, false, true};
  CHECK(pitch_class_name(60, opts) == "C");
  CHECK(pitch_class_name(72, opts) == "C");
  CHECK(pitch_class_name(61, opts) == "C#");
}

void test_keymap() {
  const auto& white = default_keymap_white();
  const auto& black = default_keymap_black();
  CHECK(white.size() == 11);
  CHECK(black.size() == 6);

  // No binding, white or black, may use the reserved 'P'/'p' key.
  for (const PianoKeyBinding& b : white) {
    CHECK(b.key != 'P' && b.key != 'p');
  }
  for (const PianoKeyBinding& b : black) {
    CHECK(b.key != 'P' && b.key != 'p');
  }

  CHECK(white[0].key == 'A' && white[0].semitone_from_base == 0);
  CHECK(black[0].key == 'W' && black[0].semitone_from_base == 1);
  CHECK(white[1].key == 'S' && white[1].semitone_from_base == 2);
  CHECK(white[7].key == 'K' && white[7].semitone_from_base == 12);
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
  monitor.observe(OutEvent::midi(0, MidiMessage::note_on(0, 60, 100), 10));
  CHECK(monitor.active_notes().size() == 1);
  CHECK(monitor.log_events(MidiEventFilter{}).size() == 1);
  CHECK(monitor.log_events(MidiEventFilter{})[0].same_tick_index == 0);

  monitor.observe(OutEvent::midi(0, MidiMessage::note_off(0, 60), 20));
  CHECK(monitor.active_notes().size() == 0);
  CHECK(monitor.log_events(MidiEventFilter{}).size() == 2);

  // same_tick_index increments among equal ticks and resets on a new tick.
  MidiMonitor ticks;
  ticks.observe(OutEvent::midi(0, MidiMessage::note_on(0, 60, 100), 100));
  ticks.observe(OutEvent::midi(0, MidiMessage::note_on(0, 62, 100), 100));
  ticks.observe(OutEvent::midi(0, MidiMessage::note_on(0, 64, 100), 100));
  ticks.observe(OutEvent::midi(0, MidiMessage::note_on(0, 65, 100), 200));
  const std::vector<MidiLogEvent> log = ticks.log_events(MidiEventFilter{});
  CHECK(log.size() == 4);
  CHECK(log[0].same_tick_index == 0);
  CHECK(log[1].same_tick_index == 1);
  CHECK(log[2].same_tick_index == 2);
  CHECK(log[3].same_tick_index == 0);

  // Log is bounded and keeps the newest entries.
  MidiMonitor capped;
  for (int i = 0; i < 300; ++i) {
    capped.observe(OutEvent::midi(0, MidiMessage::note_on(0, 60, 100), static_cast<Tick>(i)));
  }
  CHECK(capped.log_events(MidiEventFilter{}).size() == monitor_limits::kLogCapacity);
  CHECK(capped.log_events(MidiEventFilter{}).front().tick == 44);

  // log_events applies the filter.
  MidiMonitor filtered;
  filtered.observe(OutEvent::midi(0, MidiMessage::note_on(0, 60, 100), 10));
  filtered.observe(OutEvent::midi(0, MidiMessage::note_on(1, 62, 100), 10));
  MidiEventFilter only_ch1{};
  only_ch1.channel = std::uint8_t{1};
  CHECK(filtered.log_events(only_ch1).size() == 1);

  // Non-MIDI events are ignored entirely.
  MidiMonitor non_midi;
  non_midi.observe(OutEvent::warn(WarnCode::kSchedulerFull, 5));
  CHECK(non_midi.log_events(MidiEventFilter{}).empty());
  CHECK(non_midi.active_notes().size() == 0);

  // Overflow flag trips once the 129th distinct held note is rejected.
  MidiMonitor overflow;
  for (int i = 0; i < 128; ++i) {
    overflow.observe(
        OutEvent::midi(0, MidiMessage::note_on(0, static_cast<std::uint8_t>(i), 100), 0));
  }
  CHECK(!overflow.tracker_overflowed());
  overflow.observe(OutEvent::midi(1, MidiMessage::note_on(0, 0, 100), 0));  // distinct via port
  CHECK(overflow.tracker_overflowed());

  overflow.clear();
  CHECK(!overflow.tracker_overflowed());
  CHECK(overflow.active_notes().size() == 0);
}

void test_monitor_renderers() {
  MidiMonitor monitor;
  monitor.observe(OutEvent::midi(0, MidiMessage::note_on(0, 60, 96), 100), 'A');

  const MidiEventFilter filter{};
  const MidiViewOptions options{};

  // Keyboard view: the 'A' key (note 60 at base octave 4) is marked and its
  // event appears in the strip.
  PianoViewState keyboard;
  const std::vector<std::string> kb = render_piano_panel(keyboard, 100, monitor, filter, options);
  CHECK(any_line_contains(kb, "*A*"));
  CHECK(any_line_contains(kb, "A:C4"));

  // Active-notes view: grouped by channel (1-based), notes named.
  PianoViewState active = keyboard;
  active.view = PianoView::kActiveNotes;
  const std::vector<std::string> an = render_piano_panel(active, 100, monitor, filter, options);
  CHECK(any_line_contains(an, "ch1:"));
  CHECK(any_line_contains(an, "C4"));

  PianoViewState active_doremi = active;
  active_doremi.note_naming = NoteNaming::kDoReMi;
  const std::vector<std::string> an_doremi =
      render_piano_panel(active_doremi, 100, monitor, filter, options);
  CHECK(any_line_contains(an_doremi, "Do4"));

  // Event-log view: "@<tick>" lines, velocity honours the option.
  PianoViewState log = keyboard;
  log.view = PianoView::kEventLog;
  const std::vector<std::string> el = render_piano_panel(log, 100, monitor, filter, options);
  CHECK(any_line_contains(el, "@100"));
  CHECK(any_line_contains(el, "vel"));

  MidiViewOptions no_velocity = options;
  no_velocity.show_velocity = false;
  const std::vector<std::string> el_no_vel =
      render_piano_panel(log, 100, monitor, filter, no_velocity);
  CHECK(!any_line_contains(el_no_vel, "vel"));

  // Every view keeps lines within the width budget.
  for (const std::vector<std::string>* view : {&kb, &an, &el}) {
    for (const std::string& line : *view) {
      CHECK(static_cast<int>(line.size()) <= 100);
    }
  }
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
  test_active_note_tracker();
  test_visual_event_buffer();
  test_midi_monitor_observe();
  test_monitor_renderers();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_panels: all OK\n");
  }
  return arrangrr::test::failures();
}
