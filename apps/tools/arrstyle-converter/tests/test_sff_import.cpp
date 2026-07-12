#include "sff_import.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "casm.hpp"
#include "cli.hpp"
#include "midisrc/diagnostics.hpp"
#include "model.hpp"
#include "serialize.hpp"
#include "test.hpp"
#include "validate.hpp"

namespace {

using namespace arrstyle;

std::vector<std::uint8_t> read_bytes(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream ss;
  ss << file.rdbuf();
  const std::string data = ss.str();
  return std::vector<std::uint8_t>(data.begin(), data.end());
}

std::string fixture(const std::string& name) { return std::string(ARRSTYLE_FIXTURES) + name; }

const PhraseLane* find_lane(const StyleSection& section, Role role) {
  for (const PhraseLane& lane : section.lanes) {
    if (lane.role == role) {
      return &lane;
    }
  }
  return nullptr;
}

// ---------------------------------------------------------------------------

void test_decode_synthetic_sff() {
  const std::vector<std::uint8_t> bytes = read_bytes(fixture("synth_sff.sty"));
  CHECK(!bytes.empty());
  CHECK(looks_like_sff(bytes));
  CHECK(sff_is_importable(bytes));

  Diagnostics diag;
  StyleModel style;
  const bool ok = import_sff(bytes, "synth_sff.sty", style, diag);
  CHECK(ok);
  CHECK(!diag.has_errors());

  CHECK(style.source_format == SourceFormat::kYamahaSff);
  CHECK(style.source_ppqn == 96);
  CHECK(style.sections.size() == 1);

  const StyleSection& section = style.sections[0];
  CHECK(section.kind == SectionKind::kMain);
  CHECK(section.variation == SectionVariation::kA);
  CHECK(section.bars == 1);
  CHECK(section.lanes.size() == 3);

  // Drums: source channel 8 -> destination 9, fixed (NTT Bypass).
  const PhraseLane* drums = find_lane(section, Role::kDrums);
  CHECK(drums != nullptr);
  if (drums != nullptr) {
    CHECK(drums->source_channel == 8);
    CHECK(drums->transposition == TranspositionPolicy::kFixed);
    CHECK(drums->events.size() == 1);
    CHECK(drums->events[0].note == 36);
  }

  // Bass: chord-relative, over the source chord C Maj7, register-clamped, with
  // the out-of-range artefact note (Bb5 = 82) dropped by the bass filter.
  const PhraseLane* bass = find_lane(section, Role::kBass);
  CHECK(bass != nullptr);
  if (bass != nullptr) {
    CHECK(bass->source_channel == 10);
    CHECK(bass->transposition == TranspositionPolicy::kChordTone);
    CHECK(bass->source_root_pc == 0);  // C
    CHECK(bass->source_quality == ChordQuality::kMaj7);
    CHECK(bass->note_low == 28);
    CHECK(bass->note_high == 55);
    CHECK(bass->events.size() == 1);  // 82 dropped, 40 kept
    CHECK(bass->events[0].note == 40);
  }

  // Chord: chord-tone.
  const PhraseLane* chord = find_lane(section, Role::kChord1);
  CHECK(chord != nullptr);
  if (chord != nullptr) {
    CHECK(chord->transposition == TranspositionPolicy::kChordTone);
    CHECK(chord->events.size() == 1);
    CHECK(chord->events[0].note == 60);
  }
}

void test_synthetic_roundtrip_validates() {
  const std::vector<std::uint8_t> bytes = read_bytes(fixture("synth_sff.sty"));
  Diagnostics diag;
  StyleModel style;
  CHECK(import_sff(bytes, "synth_sff.sty", style, diag));

  const std::string json = to_json(style).dump() + "\n";
  Diagnostics vdiag;
  CHECK(validate_text(json, "synth", vdiag));
  CHECK(!vdiag.has_errors());

  // Import is deterministic: identical bytes twice.
  Diagnostics d2;
  StyleModel s2;
  CHECK(import_sff(bytes, "synth_sff.sty", s2, d2));
  CHECK(to_json(s2).dump() == to_json(style).dump());
}

void test_bass_register_filter() {
  // In-register notes are kept unchanged.
  CHECK(normalize_bass_note(40) == std::optional<std::uint8_t>(40));
  CHECK(normalize_bass_note(28) == std::optional<std::uint8_t>(28));
  CHECK(normalize_bass_note(55) == std::optional<std::uint8_t>(55));
  // Mildly out-of-register notes are octave-folded into the bass register.
  CHECK(normalize_bass_note(67) == std::optional<std::uint8_t>(55));  // 67 - 12
  CHECK(normalize_bass_note(20) == std::optional<std::uint8_t>(32));  // 20 + 12
  // Percussion artefacts far outside any bass octave are dropped.
  CHECK(normalize_bass_note(82) == std::nullopt);  // Bb5
  CHECK(normalize_bass_note(100) == std::nullopt);
}

void test_section_marker_mapping() {
  SectionKind kind{};
  SectionVariation var{};
  CHECK(parse_style_section("Main A", kind, var));
  CHECK(kind == SectionKind::kMain && var == SectionVariation::kA);
  CHECK(parse_style_section("Fill In BA", kind, var));
  CHECK(kind == SectionKind::kFill && var == SectionVariation::kB);
  CHECK(parse_style_section("Ending C", kind, var));
  CHECK(kind == SectionKind::kEnding && var == SectionVariation::kC);
  CHECK(parse_style_section("Intro A", kind, var));
  CHECK(kind == SectionKind::kIntro && var == SectionVariation::kA);
  CHECK(parse_style_section("Break", kind, var));
  CHECK(kind == SectionKind::kBreak && var == SectionVariation::kNone);
  // Non-section markers are rejected.
  CHECK(!parse_style_section("SFF1", kind, var));
  CHECK(!parse_style_section("SInt", kind, var));
}

void test_role_mapping() {
  CHECK(role_from_destination_channel(8) == Role::kPercussion);
  CHECK(role_from_destination_channel(9) == Role::kDrums);
  CHECK(role_from_destination_channel(10) == Role::kBass);
  CHECK(role_from_destination_channel(11) == Role::kChord1);
  CHECK(role_from_destination_channel(12) == Role::kChord2);
  CHECK(role_from_destination_channel(13) == Role::kPad);
  CHECK(role_from_destination_channel(14) == Role::kPhrase);
  CHECK(role_from_destination_channel(15) == Role::kLead);
}

void test_cli_import_and_validate() {
  const std::string out_path = "sff_test_out.arrstyle.json";
  std::ostringstream out;
  std::ostringstream err;
  const int code = run({"import-sff", fixture("synth_sff.sty"), "--out", out_path}, out, err);
  CHECK(code == kExitOk);
  CHECK(out.str().find("wrote") != std::string::npos);

  std::ostringstream vout;
  std::ostringstream verr;
  CHECK(run({"validate", out_path}, vout, verr) == kExitOk);
}

void test_cli_refuses_plain_smf() {
  // A plain SMF (the MIDI fixture) is not a style: refused, non-zero.
  std::ostringstream out;
  std::ostringstream err;
  const int code = run({"import-sff", fixture("tiny.mid")}, out, err);
  CHECK(code == kExitFailure);
  CHECK(out.str().find("inspect-only") != std::string::npos);
}

// A couple of real corpus files, if present on this machine (user-provided
// reference material). Content is not hard-coded: we only assert import exits 0
// and the output validates. Missing files are skipped, not failed.
void test_real_corpus_samples() {
  const std::vector<std::string> samples = {
      "/var/home/crsn/Condos/fedora-strudel/projects/resources/styles/extra/PSR-S950-06/"
      "PSR-S950-06/50's Pop.sty",
      "/var/home/crsn/Condos/fedora-strudel/projects/resources/styles/extra/PERF_for_S950/"
      "PERF_for_S950/BluesOrganTrio.S930.STY",
  };
  int tested = 0;
  for (const std::string& path : samples) {
    const std::vector<std::uint8_t> bytes = read_bytes(path);
    if (bytes.empty()) {
      continue;  // not available on this machine
    }
    ++tested;
    Diagnostics diag;
    StyleModel style;
    CHECK(import_sff(bytes, path, style, diag));
    CHECK(!diag.has_errors());
    CHECK(!style.sections.empty());
    const std::string json = to_json(style).dump() + "\n";
    Diagnostics vdiag;
    CHECK(validate_text(json, path, vdiag));
    CHECK(!vdiag.has_errors());
  }
  std::printf("[test_sff_import] real corpus samples exercised: %d\n", tested);
}

}  // namespace

int main() {
  test_decode_synthetic_sff();
  test_synthetic_roundtrip_validates();
  test_bass_register_filter();
  test_section_marker_mapping();
  test_role_mapping();
  test_cli_import_and_validate();
  test_cli_refuses_plain_smf();
  test_real_corpus_samples();
  return arrstyle::test::failures();
}
