#include "midi_import.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "diagnostics.hpp"
#include "model.hpp"
#include "serialize.hpp"
#include "test.hpp"

namespace {

using namespace arrstyle;

std::vector<std::uint8_t> read_fixture(const std::string& name) {
  const std::string path = std::string(ARRSTYLE_FIXTURES) + name;
  std::ifstream file(path, std::ios::binary);
  std::ostringstream ss;
  ss << file.rdbuf();
  const std::string data = ss.str();
  return std::vector<std::uint8_t>(data.begin(), data.end());
}

bool has_warning_containing(const Diagnostics& diag, const std::string& needle) {
  for (const Diagnostic& d : diag.items()) {
    if (d.severity == Severity::kWarning && d.message.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

void test_import_tiny_midi_subset() {
  const std::vector<std::uint8_t> bytes = read_fixture("tiny.mid");
  CHECK(!bytes.empty());

  Diagnostics diag;
  StyleModel style;
  const bool ok = import_midi(bytes, "tiny.mid", style, diag);
  CHECK(ok);
  CHECK(!diag.has_errors());

  CHECK(style.source_format == SourceFormat::kStandardMidiFile);
  CHECK(style.source_ppqn == 96);
  CHECK(style.tempo_milli_bpm == 120000);  // 500000 us/quarter
  CHECK(style.time_sig_num == 4);
  CHECK(style.time_sig_den == 4);
  CHECK(style.sections.size() == 1);

  const StyleSection& section = style.sections[0];
  CHECK(section.kind == SectionKind::kMain);
  CHECK(section.variation == SectionVariation::kA);
  CHECK(section.bars == 1);
  // Channel 0 (melodic) and channel 9 (drums) => two lanes.
  CHECK(section.lanes.size() == 2);

  bool saw_drums = false;
  bool saw_phrase = false;
  for (const PhraseLane& lane : section.lanes) {
    if (lane.role == Role::kDrums) {
      saw_drums = true;
      CHECK(lane.source_channel == 9);
      CHECK(lane.transposition == TranspositionPolicy::kFixed);
      CHECK(lane.events.size() == 1);
    }
    if (lane.role == Role::kPhrase) {
      saw_phrase = true;
      CHECK(lane.source_channel == 0);
      CHECK(lane.transposition == TranspositionPolicy::kChordTone);
      CHECK(lane.events.size() == 2);  // notes 60 and 62
      // First note: C4 (60), velocity 100, gate 96.
      CHECK(lane.events[0].note == 60);
      CHECK(lane.events[0].velocity == 100);
      CHECK(lane.events[0].gate_ticks == 96);
    }
  }
  CHECK(saw_drums);
  CHECK(saw_phrase);

  // The control-change message must be reported, never silently dropped.
  CHECK(has_warning_containing(diag, "control change"));
}

void test_import_is_deterministic() {
  const std::vector<std::uint8_t> bytes = read_fixture("tiny.mid");
  Diagnostics d1;
  Diagnostics d2;
  StyleModel s1;
  StyleModel s2;
  CHECK(import_midi(bytes, "tiny.mid", s1, d1));
  CHECK(import_midi(bytes, "tiny.mid", s2, d2));
  CHECK(to_json(s1).dump() == to_json(s2).dump());
}

void test_rejects_garbage() {
  const std::vector<std::uint8_t> junk = {'n', 'o', 't', 'm', 'i', 'd', 'i'};
  Diagnostics diag;
  StyleModel style;
  CHECK(!import_midi(junk, "junk", style, diag));
  CHECK(diag.has_errors());
}

}  // namespace

int main() {
  test_import_tiny_midi_subset();
  test_import_is_deterministic();
  test_rejects_garbage();
  return arrstyle::test::failures();
}
