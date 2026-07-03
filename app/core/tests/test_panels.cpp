// Host-only panel tests: note-name formatting (note_names) and the pure ASCII
// piano renderer (piano_view). Deterministic and terminal-free.

#include <string>
#include <vector>

#include "note_names.hpp"
#include "piano_view.hpp"
#include "test.hpp"

namespace {

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
  if (arrangrr::test::failures() == 0) {
    std::printf("test_panels: all OK\n");
  }
  return arrangrr::test::failures();
}
