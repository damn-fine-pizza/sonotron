#include "melodd/soundfont_discovery.hpp"

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <vector>

namespace melodd {

namespace {
constexpr const char* kSoundfontDir = "/usr/share/soundfonts";
}  // namespace

std::string find_system_soundfont(const std::string& override_path) {
  if (!override_path.empty()) {
    return override_path;
  }

  namespace fs = std::filesystem;
  std::error_code ec;

  const fs::path preferred[] = {
      fs::path(kSoundfontDir) / "FluidR3_GM.sf2",
      fs::path(kSoundfontDir) / "default.sf2",
  };
  for (const fs::path& candidate : preferred) {
    if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec)) {
      return candidate.string();
    }
  }

  std::vector<std::string> matches;
  fs::directory_iterator it(kSoundfontDir, fs::directory_options::skip_permission_denied, ec);
  if (!ec) {
    for (const auto& entry : it) {
      if (entry.path().extension() == ".sf2" && entry.is_regular_file(ec)) {
        matches.push_back(entry.path().string());
      }
    }
  }
  if (matches.empty()) {
    return {};
  }
  std::sort(matches.begin(), matches.end());
  return matches.front();
}

}  // namespace melodd
