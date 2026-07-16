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

bool write_binary_file(const std::string& path, const std::vector<std::uint8_t>& bytes,
                       std::string& error) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file) {
    error = "cannot open file for writing: " + path;
    return false;
  }
  if (!bytes.empty()) {
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  }
  return static_cast<bool>(file);
}

}  // namespace midisrc
