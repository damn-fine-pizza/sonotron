#include "validate.hpp"

#include <string>

#include "diagnostics.hpp"
#include "model.hpp"
#include "serialize.hpp"
#include "test.hpp"

namespace {

using namespace arrstyle;

StyleModel make_valid_style() {
  StyleModel s;
  s.name = "unit";
  s.source_format = SourceFormat::kStandardMidiFile;
  s.source_ppqn = 96;
  s.tempo_milli_bpm = 120000;
  s.time_sig_num = 4;
  s.time_sig_den = 4;
  StyleSection sec;
  sec.kind = SectionKind::kMain;
  sec.variation = SectionVariation::kA;
  sec.bars = 1;
  PhraseLane lane;
  lane.role = Role::kBass;
  lane.transposition = TranspositionPolicy::kChordTone;
  lane.retrigger = RetriggerPolicy::kSustain;
  lane.events.push_back(PhraseEvent{.tick = 0, .note = 40, .velocity = 100, .gate_ticks = 96});
  lane.events.push_back(PhraseEvent{.tick = 96, .note = 43, .velocity = 90, .gate_ticks = 96});
  sec.lanes.push_back(std::move(lane));
  s.sections.push_back(std::move(sec));
  return s;
}

SongModel make_valid_song() {
  SongModel s;
  s.name = "unit-song";
  s.key_root_pc = 0;
  s.key_mode = 0;
  s.chords.push_back(ChordEvent{.position = 0,
                                .bar = 0,
                                .root_pc = 0,
                                .quality = ChordQuality::kMaj7,
                                .bass_pc = -1,
                                .source_text = "Cmaj7"});
  return s;
}

void test_valid_style_and_song_pass() {
  {
    Diagnostics diag;
    const std::string text = to_json(make_valid_style()).dump();
    CHECK(validate_text(text, "style", diag));
    CHECK(!diag.has_errors());
  }
  {
    Diagnostics diag;
    const std::string text = to_json(make_valid_song()).dump();
    CHECK(validate_text(text, "song", diag));
    CHECK(!diag.has_errors());
  }
}

void test_unknown_role_fails() {
  StyleModel s = make_valid_style();
  std::string text = to_json(s).dump();
  const std::string from = "\"role\": \"bass\"";
  const std::string to = "\"role\": \"tuba\"";
  const std::size_t pos = text.find(from);
  CHECK(pos != std::string::npos);
  text.replace(pos, from.size(), to);
  Diagnostics diag;
  CHECK(!validate_text(text, "style", diag));
  CHECK(diag.has_errors());
}

void test_negative_tempo_fails() {
  StyleModel s = make_valid_style();
  std::string text = to_json(s).dump();
  const std::string from = "\"tempo_milli_bpm\": 120000";
  const std::string to = "\"tempo_milli_bpm\": -1";
  const std::size_t pos = text.find(from);
  CHECK(pos != std::string::npos);
  text.replace(pos, from.size(), to);
  Diagnostics diag;
  CHECK(!validate_text(text, "style", diag));
}

void test_out_of_range_note_fails() {
  StyleModel s = make_valid_style();
  std::string text = to_json(s).dump();
  const std::string from = "\"note\": 40";
  const std::string to = "\"note\": 200";
  const std::size_t pos = text.find(from);
  CHECK(pos != std::string::npos);
  text.replace(pos, from.size(), to);
  Diagnostics diag;
  CHECK(!validate_text(text, "style", diag));
}

void test_malformed_json_fails() {
  Diagnostics diag;
  CHECK(!validate_text("{ not json", "bad", diag));
  CHECK(diag.has_errors());
}

void test_unknown_format_fails() {
  Diagnostics diag;
  CHECK(!validate_text("{\n  \"format\": \"mystery\",\n  \"version\": 1\n}", "bad", diag));
  CHECK(diag.has_errors());
}

}  // namespace

int main() {
  test_valid_style_and_song_pass();
  test_unknown_role_fails();
  test_negative_tempo_fails();
  test_out_of_range_note_fails();
  test_malformed_json_fails();
  test_unknown_format_fails();
  return arrstyle::test::failures();
}
