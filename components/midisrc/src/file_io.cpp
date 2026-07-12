#include "midisrc/file_io.hpp"

#include <fstream>
#include <sstream>

namespace midisrc {

bool read_binary_file(const std::string& path, std::vector<std::uint8_t>& out, std::string& error) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    error = "cannot open file: " + path;
    return false;
  }
  std::ostringstream ss;
  ss << file.rdbuf();
  const std::string data = ss.str();
  out.assign(data.begin(), data.end());
  return true;
}

}  // namespace midisrc
