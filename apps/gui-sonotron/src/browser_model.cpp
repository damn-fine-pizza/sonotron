#include "browser_model.hpp"

#include <algorithm>
#include <cctype>

namespace sonotron {

namespace {

std::string to_lower(std::string_view s) {
  std::string out(s);
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

}  // namespace

std::string_view BrowserModel::style_name(std::size_t index) const {
  return kBuiltinStyleNames[index];
}

bool BrowserModel::style_matches_filter(std::size_t index) const {
  if (m_filter.empty()) {
    return true;
  }
  const std::string name = to_lower(kBuiltinStyleNames[index]);
  const std::string needle = to_lower(m_filter);
  return name.find(needle) != std::string::npos;
}

}  // namespace sonotron
