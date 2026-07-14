// Functional tests for the graceful-degradation path in
// load_or_create_default(): a persisted layout.json that is schema-
// incompatible (absent/legacy "schema_version", or one naming a zone id
// this binary cannot render) must not be loaded verbatim -- that is exactly
// how a stale file from an older app version used to silently break the
// whole UI (missing "browser" zone, unknown ids drawing as empty panels).
// See docs/DESIGN.md §2 architectural principle #8 (versioned persisted
// formats) and this milestone's task grounding for the full story.

#include "src/layout_json.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "test.hpp"

namespace {

std::filesystem::path unique_temp_path() {
  const auto dir = std::filesystem::temp_directory_path() / "sonotron-layout-schema-test";
  return dir / "layout.json";
}

void write_file(const std::filesystem::path& path, const std::string& contents) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << contents;
}

std::string read_file(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

// The exact stale shape described in the task grounding: no "schema_version"
// key at all, and zone ids from the pre-Fase-2-restart layout (arrangement/
// harmony/structure survive, "browser" is missing entirely -- exactly what
// silently broke style loading).
void test_legacy_file_resets_to_default_and_self_upgrades_on_disk() {
  const std::filesystem::path path = unique_temp_path();
  std::error_code remove_error;
  std::filesystem::remove_all(path.parent_path(), remove_error);

  write_file(path, R"({
    "window": "sonotron",
    "zones": [
      { "id": "transport", "title": "Transport", "row": 0, "span": "full" },
      { "id": "arrangement", "title": "Arrangement", "row": 1, "col": 0 },
      { "id": "harmony", "title": "Harmony", "row": 1, "col": 1 },
      { "id": "structure", "title": "Structure", "row": 1, "col": 2 },
      { "id": "intention", "title": "Intention", "row": 2, "col": 0 }
    ]
  })");

  sonotron::Layout loaded;
  std::string error;
  const bool ok = sonotron::load_or_create_default(path.string(), loaded, error);
  CHECK(ok);  // a reset is NOT a load failure
  CHECK(loaded == sonotron::default_layout());

  // The on-disk file must have been self-upgraded: re-reading and
  // re-parsing it now yields a current, fully-renderable layout.
  sonotron::Layout reparsed;
  const bool reparse_ok = sonotron::parse_layout(read_file(path), reparsed, error);
  CHECK(reparse_ok);
  CHECK(reparsed.schema_version == sonotron::kLayoutSchemaVersion);
  CHECK(reparsed == sonotron::default_layout());
  for (const sonotron::Zone& zone : reparsed.zones) {
    CHECK(sonotron::is_renderable_zone_id(zone.id));
  }

  // A second load of the now-upgraded file must NOT reset again.
  sonotron::Layout second_load;
  const bool second_ok = sonotron::load_or_create_default(path.string(), second_load, error);
  CHECK(second_ok);
  CHECK(second_load == sonotron::default_layout());

  std::filesystem::remove_all(path.parent_path(), remove_error);
}

// A current, valid v1 file with a customization (a non-default window
// title, so an erroneous reset is distinguishable from a correct load)
// round-trips unchanged -- no spurious reset.
void test_current_v1_file_round_trips_unchanged() {
  const std::filesystem::path path = unique_temp_path();
  std::error_code remove_error;
  std::filesystem::remove_all(path.parent_path(), remove_error);

  sonotron::Layout customized = sonotron::default_layout();
  customized.window_title = "sonotron-customized";
  std::string error;
  const bool save_ok = sonotron::save_layout(path.string(), customized, error);
  CHECK(save_ok);

  sonotron::Layout loaded;
  const bool load_ok = sonotron::load_or_create_default(path.string(), loaded, error);
  CHECK(load_ok);
  CHECK(loaded == customized);
  CHECK(loaded.window_title == "sonotron-customized");  // not silently reset to default

  std::filesystem::remove_all(path.parent_path(), remove_error);
}

// A v1 file (correct schema_version) naming one unknown zone id resets to
// default -- the id inventory check applies independently of the schema
// version check.
void test_v1_file_with_unknown_zone_id_resets_to_default() {
  const std::filesystem::path path = unique_temp_path();
  std::error_code remove_error;
  std::filesystem::remove_all(path.parent_path(), remove_error);

  write_file(path, R"({
    "schema_version": )" +
                       std::to_string(sonotron::kLayoutSchemaVersion) + R"(,
    "window": "sonotron",
    "zones": [
      { "id": "transport", "title": "Transport", "row": 0, "span": "full" },
      { "id": "browser", "title": "Browser", "row": 1, "col": 0 },
      { "id": "future-panel", "title": "Future", "row": 1, "col": 1 }
    ]
  })");

  sonotron::Layout loaded;
  std::string error;
  const bool ok = sonotron::load_or_create_default(path.string(), loaded, error);
  CHECK(ok);
  CHECK(loaded == sonotron::default_layout());

  std::filesystem::remove_all(path.parent_path(), remove_error);
}

// A file whose schema_version is GREATER than what this binary knows (a
// downgrade scenario, e.g. after rolling back to an older binary) is
// treated as incompatible too, symmetric with a too-old file -- this
// binary cannot know whether a newer schema kept the same zone-id
// semantics, so it resets rather than guess.
void test_newer_schema_version_resets_to_default() {
  const std::filesystem::path path = unique_temp_path();
  std::error_code remove_error;
  std::filesystem::remove_all(path.parent_path(), remove_error);

  write_file(path, R"({
    "schema_version": )" +
                       std::to_string(sonotron::kLayoutSchemaVersion + 1) + R"(,
    "window": "sonotron",
    "zones": [
      { "id": "transport", "title": "Transport", "row": 0, "span": "full" },
      { "id": "browser", "title": "Browser", "row": 1, "col": 0 }
    ]
  })");

  sonotron::Layout loaded;
  std::string error;
  const bool ok = sonotron::load_or_create_default(path.string(), loaded, error);
  CHECK(ok);
  CHECK(loaded == sonotron::default_layout());

  std::filesystem::remove_all(path.parent_path(), remove_error);
}

}  // namespace

int main() {
  test_legacy_file_resets_to_default_and_self_upgrades_on_disk();
  test_current_v1_file_round_trips_unchanged();
  test_v1_file_with_unknown_zone_id_resets_to_default();
  test_newer_schema_version_resets_to_default();
  return sonotron::test::failures();
}
