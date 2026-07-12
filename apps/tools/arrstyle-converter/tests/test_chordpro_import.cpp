#include "chordpro_import.hpp"

#include <fstream>
#include <sstream>
#include <string>

#include "midisrc/diagnostics.hpp"
#include "model.hpp"
#include "test.hpp"

namespace {

using namespace arrstyle;

std::string read_fixture_text(const std::string& name) {
  const std::string path = std::string(ARRSTYLE_FIXTURES) + name;
  std::ifstream file(path, std::ios::binary);
  std::ostringstream ss;
  ss << file.rdbuf();
  return ss.str();
}

bool has_info_containing(const Diagnostics& diag, const std::string& needle) {
  for (const Diagnostic& d : diag.items()) {
    if (d.severity == Severity::kInfo && d.message.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

void test_chord_token_parsing() {
  ChordEvent c;
  CHECK(parse_chord_token("G", c) && c.root_pc == 7 && c.quality == ChordQuality::kMaj);
  CHECK(parse_chord_token("G7", c) && c.root_pc == 7 && c.quality == ChordQuality::kDom7);
  CHECK(parse_chord_token("Am7", c) && c.root_pc == 9 && c.quality == ChordQuality::kMin7);
  CHECK(parse_chord_token("Gmaj7", c) && c.root_pc == 7 && c.quality == ChordQuality::kMaj7);
  CHECK(parse_chord_token("D7sus4", c) && c.root_pc == 2);  // extended, folds to a base family
  CHECK(parse_chord_token("Bb", c) && c.root_pc == 10 && c.quality == ChordQuality::kMaj);
  CHECK(parse_chord_token("F#m7b5", c) && c.root_pc == 6 && c.quality == ChordQuality::kHalfDim7);
  // Slash chord: bass tracked separately.
  CHECK(parse_chord_token("E7/G#", c) && c.root_pc == 4 && c.bass_pc == 8);
  // Unparseable root.
  CHECK(!parse_chord_token("xyz", c) && c.root_pc == -1);
}

void test_import_sample_chordpro() {
  const std::string text = read_fixture_text("sample.chopro");
  Diagnostics diag;
  SongModel song;
  const bool ok = import_chordpro(text, "sample.chopro", song, diag);
  CHECK(ok);
  CHECK(!diag.has_errors());

  CHECK(song.name == "Twelve Bar Demo");
  CHECK(song.key_root_pc == 7);  // G
  CHECK(song.key_mode == 0);     // major
  CHECK(song.tempo_milli_bpm == 128000);
  CHECK(song.harmony_source == PhraseSourceHarmony::kChordSequence);

  // 4 verse chords + 4 verse chords + 4 turnaround chords = 12.
  CHECK(song.chords.size() == 12);
  CHECK(song.chords[0].root_pc == 7);  // G
  // Sections: verse marker + comment marker.
  CHECK(song.sections.size() == 2);
  CHECK(song.sections[0].label == "Verse");
  CHECK(song.sections[1].label == "Turnaround");

  // Lyrics ignored (reported), unknown directive reported.
  CHECK(has_info_containing(diag, "lyric line"));
  CHECK(has_info_containing(diag, "unsupported directive"));
}

void test_empty_chordpro_fails() {
  Diagnostics diag;
  SongModel song;
  CHECK(!import_chordpro("just lyrics, no chords\n", "empty", song, diag));
  CHECK(diag.has_errors());
}

}  // namespace

int main() {
  test_chord_token_parsing();
  test_import_sample_chordpro();
  test_empty_chordpro_fails();
  return arrstyle::test::failures();
}
