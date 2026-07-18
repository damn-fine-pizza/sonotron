// Functional round-trip tests for scenes.json (repeat-zone-real-contract.md
// §4/§8b decision 3): a renamed scene column must survive a save/load cycle
// through a real temp file, mirroring test_layout_schema_upgrade.cpp's own
// load/save/temp-dir shape.

#include "src/scenes_json.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "src/grid_model.hpp"
#include "test.hpp"

namespace {

std::filesystem::path unique_temp_path() {
  const auto dir = std::filesystem::temp_directory_path() / "sonotron-scenes-json-test";
  return dir / "scenes.json";
}

std::string read_file(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

// A freshly-constructed GridModel already carries the bare-number defaults
// ("1".."8") -- no separate default_scenes() constructor needed, same as
// Layout's own in-class defaults.
void test_default_names_are_bare_numbers() {
  sonotron::GridModel model;
  CHECK(model.scene_name(0) == "1");
  CHECK(model.scene_name(1) == "2");
  CHECK(model.scene_name(sonotron::GridModel::kMaxSceneCount - 1) ==
        std::to_string(sonotron::GridModel::kMaxSceneCount));
}

// Renamed scene names round-trip through save + a separate load into a
// fresh GridModel exactly -- the load path a GUI restart takes.
void test_rename_survives_save_and_reload() {
  const std::filesystem::path path = unique_temp_path();
  std::error_code remove_error;
  std::filesystem::remove_all(path.parent_path(), remove_error);

  sonotron::GridModel writer(5);
  writer.set_scene_name(0, "Intro");
  writer.set_scene_name(1, "Verse");
  writer.set_scene_name(2, "Chorus \xC2\xB7 hook");  // exercises multi-byte UTF-8 passthrough

  std::string error;
  CHECK(sonotron::save_scenes(path.string(), writer, error));

  sonotron::GridModel reader(5);
  CHECK(sonotron::load_scenes_or_create_default(path.string(), reader, error));
  CHECK(reader.scene_name(0) == "Intro");
  CHECK(reader.scene_name(1) == "Verse");
  CHECK(reader.scene_name(2) == "Chorus \xC2\xB7 hook");
  // Untouched columns keep the constructor default.
  CHECK(reader.scene_name(3) == "4");
  CHECK(reader.scene_name(7) == "8");

  std::filesystem::remove_all(path.parent_path(), remove_error);
}

// A missing file is not a failure: load_scenes_or_create_default writes the
// model's current (default) names to disk and returns true, mirroring
// load_or_create_default()'s "missing file -> default" shape.
void test_missing_file_writes_current_defaults() {
  const std::filesystem::path path = unique_temp_path();
  std::error_code remove_error;
  std::filesystem::remove_all(path.parent_path(), remove_error);
  CHECK(!std::filesystem::exists(path));

  sonotron::GridModel model;
  std::string error;
  CHECK(sonotron::load_scenes_or_create_default(path.string(), model, error));
  CHECK(std::filesystem::exists(path));

  sonotron::GridModel reloaded;
  CHECK(sonotron::load_scenes_or_create_default(path.string(), reloaded, error));
  CHECK(reloaded.scene_name(0) == "1");

  std::filesystem::remove_all(path.parent_path(), remove_error);
}

// A scenes.json array shorter than kMaxSceneCount leaves the remaining
// indices at whatever the target model already had -- a partial file (e.g.
// hand-edited, or written by an older binary with fewer columns) degrades
// gracefully instead of clobbering unmentioned columns to empty.
void test_short_array_leaves_remaining_indices_untouched() {
  const std::filesystem::path path = unique_temp_path();
  std::error_code remove_error;
  std::filesystem::remove_all(path.parent_path(), remove_error);

  std::filesystem::create_directories(path.parent_path());
  {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << R"({ "scenes": ["Only One"] })";
  }

  sonotron::GridModel model;
  std::string error;
  CHECK(sonotron::parse_scenes(read_file(path), model, error));
  CHECK(model.scene_name(0) == "Only One");
  CHECK(model.scene_name(1) == "2");  // untouched constructor default

  std::filesystem::remove_all(path.parent_path(), remove_error);
}

// A syntactically malformed file is a parse failure, with a line/column
// message, same discipline as layout_json.cpp's parser.
void test_malformed_file_fails_with_error_message() {
  sonotron::GridModel model;
  std::string error;
  const bool ok = sonotron::parse_scenes("{ \"scenes\": [\"unterminated ", model, error);
  CHECK(!ok);
  CHECK(!error.empty());
}

// write_scenes() -> parse_scenes() round-trips exactly through in-memory
// text alone, no filesystem involved.
void test_write_then_parse_round_trips() {
  sonotron::GridModel writer;
  writer.set_scene_name(4, "Outro");
  const std::string text = sonotron::write_scenes(writer);

  sonotron::GridModel reader;
  std::string error;
  CHECK(sonotron::parse_scenes(text, reader, error));
  for (std::size_t i = 0; i < sonotron::GridModel::kMaxSceneCount; ++i) {
    CHECK(reader.scene_name(i) == writer.scene_name(i));
  }
}

// A freshly-constructed GridModel already carries the kDefaultSectionType
// default on every scene column (SLICE 4a, docs/proposals/repeat-zone-real-
// contract.md).
void test_default_sections_are_var_a() {
  sonotron::GridModel model;
  for (std::size_t i = 0; i < sonotron::GridModel::kMaxSceneCount; ++i) {
    CHECK(model.scene_section(i) == sonotron::GridModel::kDefaultSectionType);
  }
}

// Per-scene SECTIONS round-trip through save + a separate load into a fresh
// GridModel exactly, alongside names -- the load path a GUI restart takes
// (SLICE 4a gate 5a).
void test_section_survives_save_and_reload() {
  const std::filesystem::path path = unique_temp_path();
  std::error_code remove_error;
  std::filesystem::remove_all(path.parent_path(), remove_error);

  sonotron::GridModel writer(5);
  writer.set_scene_name(0, "Intro");
  writer.set_scene_section(0, 0);   // kIntro1
  writer.set_scene_section(1, 3);   // kVarB
  writer.set_scene_section(4, 11);  // kEnding1

  std::string error;
  CHECK(sonotron::save_scenes(path.string(), writer, error));

  sonotron::GridModel reader(5);
  CHECK(sonotron::load_scenes_or_create_default(path.string(), reader, error));
  CHECK(reader.scene_name(0) == "Intro");
  CHECK(reader.scene_section(0) == 0);
  CHECK(reader.scene_section(1) == 3);
  CHECK(reader.scene_section(4) == 11);
  // An untouched column keeps the constructor default.
  CHECK(reader.scene_section(2) == sonotron::GridModel::kDefaultSectionType);

  std::filesystem::remove_all(path.parent_path(), remove_error);
}

// A scenes.json written by a PRE-SLICE-4a binary (no "sections" key at all)
// is not a parse failure -- every scene keeps the constructor default
// section, exactly like GridModel's own fresh-construction default (gate 5a:
// old-format-without-sections -> default fallback).
void test_missing_sections_key_falls_back_to_default() {
  sonotron::GridModel model;
  std::string error;
  const bool ok = sonotron::parse_scenes(R"({ "scenes": ["Intro", "Verse"] })", model, error);
  CHECK(ok);
  CHECK(model.scene_name(0) == "Intro");
  CHECK(model.scene_name(1) == "Verse");
  for (std::size_t i = 0; i < sonotron::GridModel::kMaxSceneCount; ++i) {
    CHECK(model.scene_section(i) == sonotron::GridModel::kDefaultSectionType);
  }
}

// A "sections" array shorter than kMaxSceneCount leaves the remaining
// indices at the constructor default, same discipline as the "scenes" array
// (test_short_array_leaves_remaining_indices_untouched above).
void test_short_sections_array_leaves_remaining_indices_untouched() {
  sonotron::GridModel model;
  std::string error;
  const bool ok = sonotron::parse_scenes(R"({ "scenes": [], "sections": [7] })", model, error);
  CHECK(ok);
  CHECK(model.scene_section(0) == 7);
  CHECK(model.scene_section(1) == sonotron::GridModel::kDefaultSectionType);
}

// write_scenes() -> parse_scenes() round-trips sections exactly, alongside
// names, through in-memory text alone.
void test_write_then_parse_round_trips_sections() {
  sonotron::GridModel writer;
  writer.set_scene_section(1, 6);
  writer.set_scene_section(3, 10);
  const std::string text = sonotron::write_scenes(writer);

  sonotron::GridModel reader;
  std::string error;
  CHECK(sonotron::parse_scenes(text, reader, error));
  for (std::size_t i = 0; i < sonotron::GridModel::kMaxSceneCount; ++i) {
    CHECK(reader.scene_section(i) == writer.scene_section(i));
  }
}

// A freshly-constructed GridModel already carries the kDefaultSceneRepeat
// default on every scene column (task #5).
void test_default_repeats_are_one() {
  sonotron::GridModel model;
  for (std::size_t i = 0; i < sonotron::GridModel::kMaxSceneCount; ++i) {
    CHECK(model.scene_repeat(i) == sonotron::GridModel::kDefaultSceneRepeat);
  }
}

// Per-scene REPEAT COUNTS round-trip through save + a separate load into a
// fresh GridModel exactly, alongside names/sections/bars -- the load path a
// GUI restart takes (task #5).
void test_repeat_survives_save_and_reload() {
  const std::filesystem::path path = unique_temp_path();
  std::error_code remove_error;
  std::filesystem::remove_all(path.parent_path(), remove_error);

  sonotron::GridModel writer(5);
  writer.set_scene_name(0, "Intro");
  writer.set_scene_repeat(0, 3);
  writer.set_scene_repeat(1, sonotron::GridModel::kSceneRepeatInfinite);
  writer.set_scene_repeat(4, 8);

  std::string error;
  CHECK(sonotron::save_scenes(path.string(), writer, error));

  sonotron::GridModel reader(5);
  CHECK(sonotron::load_scenes_or_create_default(path.string(), reader, error));
  CHECK(reader.scene_name(0) == "Intro");
  CHECK(reader.scene_repeat(0) == 3);
  CHECK(reader.scene_repeat(1) == sonotron::GridModel::kSceneRepeatInfinite);
  CHECK(reader.scene_repeat(4) == 8);
  // An untouched column keeps the constructor default.
  CHECK(reader.scene_repeat(2) == sonotron::GridModel::kDefaultSceneRepeat);

  std::filesystem::remove_all(path.parent_path(), remove_error);
}

// A scenes.json written by a PRE-task-#5 binary (no "repeats" key at all) is
// not a parse failure -- every scene keeps the constructor default repeat
// count, exactly like GridModel's own fresh-construction default.
void test_missing_repeats_key_falls_back_to_default() {
  sonotron::GridModel model;
  std::string error;
  const bool ok = sonotron::parse_scenes(R"({ "scenes": ["Intro", "Verse"] })", model, error);
  CHECK(ok);
  CHECK(model.scene_name(0) == "Intro");
  CHECK(model.scene_name(1) == "Verse");
  for (std::size_t i = 0; i < sonotron::GridModel::kMaxSceneCount; ++i) {
    CHECK(model.scene_repeat(i) == sonotron::GridModel::kDefaultSceneRepeat);
  }
}

// A "repeats" array shorter than kMaxSceneCount leaves the remaining indices
// at the constructor default, same discipline as "scenes"/"sections"/"bars"
// above.
void test_short_repeats_array_leaves_remaining_indices_untouched() {
  sonotron::GridModel model;
  std::string error;
  const bool ok = sonotron::parse_scenes(R"({ "scenes": [], "repeats": [5] })", model, error);
  CHECK(ok);
  CHECK(model.scene_repeat(0) == 5);
  CHECK(model.scene_repeat(1) == sonotron::GridModel::kDefaultSceneRepeat);
}

// write_scenes() -> parse_scenes() round-trips repeats exactly, alongside
// names/sections/bars, through in-memory text alone.
void test_write_then_parse_round_trips_repeats() {
  sonotron::GridModel writer;
  writer.set_scene_repeat(1, 6);
  writer.set_scene_repeat(3, sonotron::GridModel::kSceneRepeatInfinite);
  const std::string text = sonotron::write_scenes(writer);

  sonotron::GridModel reader;
  std::string error;
  CHECK(sonotron::parse_scenes(text, reader, error));
  for (std::size_t i = 0; i < sonotron::GridModel::kMaxSceneCount; ++i) {
    CHECK(reader.scene_repeat(i) == writer.scene_repeat(i));
  }
}

}  // namespace

int main() {
  test_default_names_are_bare_numbers();
  test_rename_survives_save_and_reload();
  test_missing_file_writes_current_defaults();
  test_short_array_leaves_remaining_indices_untouched();
  test_malformed_file_fails_with_error_message();
  test_write_then_parse_round_trips();
  test_default_sections_are_var_a();
  test_section_survives_save_and_reload();
  test_missing_sections_key_falls_back_to_default();
  test_short_sections_array_leaves_remaining_indices_untouched();
  test_write_then_parse_round_trips_sections();
  test_default_repeats_are_one();
  test_repeat_survives_save_and_reload();
  test_missing_repeats_key_falls_back_to_default();
  test_short_repeats_array_leaves_remaining_indices_untouched();
  test_write_then_parse_round_trips_repeats();
  return sonotron::test::failures();
}
