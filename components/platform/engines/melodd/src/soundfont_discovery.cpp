#include "melodd/soundfont_discovery.hpp"

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <vector>

namespace melodd {

std::string find_system_soundfont(const std::string& override_path, const std::string& search_dir) {
  if (!override_path.empty()) {
    return override_path;
  }

  namespace fs = std::filesystem;
  std::error_code ec;

  const fs::path preferred[] = {
      fs::path(search_dir) / "FluidR3_GM.sf2",
      fs::path(search_dir) / "default.sf2",
  };
  for (const fs::path& candidate : preferred) {
    if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec)) {
      return candidate.string();
    }
  }

  std::vector<std::string> matches;
  fs::directory_iterator it(search_dir, fs::directory_options::skip_permission_denied, ec);
  if (!ec) {
    for (const auto& entry : it) {
      const fs::path extension = entry.path().extension();
      if ((extension == ".sf2" || extension == ".sf3") && entry.is_regular_file(ec)) {
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
