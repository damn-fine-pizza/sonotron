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

std::string_view style_family_label(StyleFamily family) {
  switch (family) {
    case StyleFamily::kPopRockBallad:
      return "Pop / Ballad";
    case StyleFamily::kDanceFourOnFloor:
      return "Dance / Four-on-floor";
    case StyleFamily::kFunkGroove:
      return "Funk / Groove";
    case StyleFamily::kSwingShuffleJazz:
      return "Big Band / Jazz";
    case StyleFamily::kLatinClave:
      return "Clave / Tropical";
    case StyleFamily::kBallroomTraditional:
      return "Ballroom / Traditional";
    case StyleFamily::kWorldRegional:
      return "World / Regional";
    case StyleFamily::kOther:
      return "Other";
  }
  return "Other";
}

std::string_view BrowserModel::style_name(std::size_t index) const {
  return kBuiltinStyleNames[index];
}

StyleFamily BrowserModel::style_family(std::size_t index) const {
  return kBuiltinStyleFamilies[index];
}

bool BrowserModel::style_matches_filter(std::size_t index) const {
  if (m_family_filter.has_value() && *m_family_filter != kBuiltinStyleFamilies[index]) {
    return false;
  }
  if (m_filter.empty()) {
    return true;
  }
  const std::string haystack = to_lower(kBuiltinStyleNames[index]) + " " +
                               to_lower(style_family_label(kBuiltinStyleFamilies[index]));
  const std::string needle = to_lower(m_filter);
  return haystack.find(needle) != std::string::npos;
}

}  // namespace sonotron
