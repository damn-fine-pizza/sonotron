// Functional test: the full load -> render-ready -> save -> reload path
// against a real file on disk, driven through the public API exactly as
// main.cpp uses it (load_or_create_default / save_layout).

#include "src/layout_json.hpp"

#include <filesystem>
#include <fstream>

#include "test.hpp"

namespace {

std::filesystem::path unique_temp_path() {
  const auto dir = std::filesystem::temp_directory_path() / "sonotron-layout-test";
  return dir / "layout.json";
}

void test_missing_file_creates_default_and_round_trips() {
  const std::filesystem::path path = unique_temp_path();
  std::error_code remove_error;
  std::filesystem::remove_all(path.parent_path(), remove_error);
  CHECK(!std::filesystem::exists(path));

  sonotron::Layout first_load;
  std::string error;
  const bool created_ok = sonotron::load_or_create_default(path.string(), first_load, error);
  CHECK(created_ok);
  CHECK(error.empty());
  CHECK(std::filesystem::exists(path));
  CHECK(first_load == sonotron::default_layout());

  // Reload the file that was just written and confirm it is identical.
  sonotron::Layout second_load;
  const bool reload_ok = sonotron::load_or_create_default(path.string(), second_load, error);
  CHECK(reload_ok);
  CHECK(first_load == second_load);

  std::filesystem::remove_all(path.parent_path(), remove_error);
}

void test_modified_layout_round_trips_after_save() {
  const std::filesystem::path path = unique_temp_path();
  std::error_code remove_error;
  std::filesystem::remove_all(path.parent_path(), remove_error);

  sonotron::Layout layout = sonotron::default_layout();
  layout.window_title = "sonotron-modified";
  layout.font_size_px = 18.0F;
  layout.zones[1].title = "Intention (renamed)";
  layout.zones[3].width_weight = 0.35F;

  std::string error;
  const bool save_ok = sonotron::save_layout(path.string(), layout, error);
  CHECK(save_ok);
  CHECK(error.empty());

  sonotron::Layout reloaded;
  const bool load_ok = sonotron::load_or_create_default(path.string(), reloaded, error);
  CHECK(load_ok);
  CHECK(layout == reloaded);
  CHECK(reloaded.font_size_px > 17.9F && reloaded.font_size_px < 18.1F);

  std::filesystem::remove_all(path.parent_path(), remove_error);
}

// Owner-facing knob: editing "font_size" in ~/.config/sonotron/layout.json
// by hand (no rebuild) must take effect on the next load, and must survive
// the app's on-exit save unchanged — this is the exact path main.cpp drives
// (load_or_create_default at startup, save_layout at shutdown). The
// hand-edit carries a current "schema_version" so the graceful-degradation
// reset (test_layout_schema_upgrade.cpp) does not fire here — this test is
// about the font_size knob, not about schema versioning.
void test_hand_edited_font_size_survives_load_and_save() {
  const std::filesystem::path path = unique_temp_path();
  std::error_code remove_error;
  std::filesystem::remove_all(path.parent_path(), remove_error);
  std::filesystem::create_directories(path.parent_path());

  {
    std::ofstream hand_edited(path);
    hand_edited << R"({ "schema_version": )" << sonotron::kLayoutSchemaVersion
                << R"(, "window": "sonotron", "font_size": 22, "zones": [] })";
  }

  sonotron::Layout loaded;
  std::string error;
  const bool load_ok = sonotron::load_or_create_default(path.string(), loaded, error);
  CHECK(load_ok);
  CHECK(loaded.font_size_px > 21.9F && loaded.font_size_px < 22.1F);

  const bool save_ok = sonotron::save_layout(path.string(), loaded, error);
  CHECK(save_ok);

  sonotron::Layout reloaded;
  const bool reload_ok = sonotron::load_or_create_default(path.string(), reloaded, error);
  CHECK(reload_ok);
  CHECK(reloaded.font_size_px > 21.9F && reloaded.font_size_px < 22.1F);

  std::filesystem::remove_all(path.parent_path(), remove_error);
}

void test_load_reports_error_on_corrupt_file() {
  const std::filesystem::path path = unique_temp_path();
  std::error_code remove_error;
  std::filesystem::remove_all(path.parent_path(), remove_error);
  std::filesystem::create_directories(path.parent_path());

  {
    std::ofstream corrupt(path);
    corrupt << "{ not valid json";
  }

  sonotron::Layout layout;
  std::string error;
  const bool ok = sonotron::load_or_create_default(path.string(), layout, error);
  CHECK(!ok);
  CHECK(!error.empty());

  std::filesystem::remove_all(path.parent_path(), remove_error);
}

}  // namespace

int main() {
  test_missing_file_creates_default_and_round_trips();
  test_modified_layout_round_trips_after_save();
  test_hand_edited_font_size_survives_load_and_save();
  test_load_reports_error_on_corrupt_file();
  return sonotron::test::failures();
}
