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
  test_load_reports_error_on_corrupt_file();
  return sonotron::test::failures();
}
