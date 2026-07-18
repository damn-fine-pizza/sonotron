// Deterministic, no system-SoundFont dependency: proves find_system_soundfont()
// discovers .sf3 alongside .sf2 (Phase-1 sound task #7, deliverable B), and
// that the FluidR3_GM.sf2/default.sf2 special-cased names still take
// precedence over the general scan. Uses the search_dir override parameter
// (added by this same task) to point find_system_soundfont() at a scratch
// temp directory it fully controls, instead of the real system path.

#include <filesystem>
#include <fstream>
#include <string>

#include "melodd/soundfont_discovery.hpp"
#include "test.hpp"

namespace {

namespace fs = std::filesystem;

void touch(const fs::path& path) {
  std::ofstream file(path);
  file << "not a real soundfont, existence-only fixture\n";
}

}  // namespace

int main() {
  const fs::path scratch_dir = fs::temp_directory_path() / "sonotron_soundfont_discovery_test";
  std::error_code ec;
  fs::remove_all(scratch_dir, ec);
  fs::create_directories(scratch_dir, ec);
  CHECK(!ec);

  // --- override_path always wins, search_dir untouched -------------------
  CHECK(melodd::find_system_soundfont("/explicit/path.sf2", scratch_dir.string()) ==
        "/explicit/path.sf2");

  // --- empty directory: no match ------------------------------------------
  CHECK(melodd::find_system_soundfont("", scratch_dir.string()).empty());

  // --- a lone .sf3 IS discovered (the actual behavior this task adds) ----
  touch(scratch_dir / "custom_bank.sf3");
  CHECK(melodd::find_system_soundfont("", scratch_dir.string()) ==
        (scratch_dir / "custom_bank.sf3").string());

  // --- .sf2 and .sf3 both present: lexicographic order across both -------
  touch(scratch_dir / "aaa_first.sf2");
  CHECK(melodd::find_system_soundfont("", scratch_dir.string()) ==
        (scratch_dir / "aaa_first.sf2").string());

  // --- FluidR3_GM.sf2 always wins over the scan, .sf3 present or not -----
  touch(scratch_dir / "FluidR3_GM.sf2");
  CHECK(melodd::find_system_soundfont("", scratch_dir.string()) ==
        (scratch_dir / "FluidR3_GM.sf2").string());

  // --- a non-soundfont extension is ignored -------------------------------
  fs::remove(scratch_dir / "FluidR3_GM.sf2", ec);
  fs::remove(scratch_dir / "aaa_first.sf2", ec);
  touch(scratch_dir / "readme.txt");
  CHECK(melodd::find_system_soundfont("", scratch_dir.string()) ==
        (scratch_dir / "custom_bank.sf3").string());

  fs::remove_all(scratch_dir, ec);

  if (melodd::test::failures() == 0) {
    std::printf("OK: find_system_soundfont() discovers .sf3 alongside .sf2, precedence intact\n");
  }
  return melodd::test::failures();
}
