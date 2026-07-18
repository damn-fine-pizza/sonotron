#include "browser_model.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace sonotron {

namespace {

std::string to_lower(std::string_view s) {
  std::string out(s);
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

}  // namespace

std::string_view browser_category_label(BrowserCategory category) {
  switch (category) {
    case BrowserCategory::kStyles:
      return "styles";
    case BrowserCategory::kVariations:
      return "variations";
    case BrowserCategory::kVoices:
      return "voices \xC2\xB7 sounds";
    case BrowserCategory::kKits:
      return "kits \xC2\xB7 GM";
    case BrowserCategory::kClips:
      return "clips";
  }
  return "styles";
}

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
  const std::string& styles_filter = m_filters[static_cast<std::size_t>(BrowserCategory::kStyles)];
  if (styles_filter.empty()) {
    return true;
  }
  const std::string haystack = to_lower(kBuiltinStyleNames[index]) + " " +
                               to_lower(style_family_label(kBuiltinStyleFamilies[index]));
  const std::string needle = to_lower(styles_filter);
  return haystack.find(needle) != std::string::npos;
}

void BrowserModel::set_search_filter(std::string filter) {
  m_filters[static_cast<std::size_t>(m_category)] = std::move(filter);
}

const std::string& BrowserModel::search_filter() const {
  return m_filters[static_cast<std::size_t>(m_category)];
}

void BrowserModel::set_voice_port(std::string port) {
  m_voice_port = port.empty() ? std::string("out0") : std::move(port);
}

void BrowserModel::set_voice_channel(int channel_one_based) {
  m_voice_channel = std::clamp(channel_one_based, 1, 16);
}

std::string BrowserModel::build_program_verb(std::string_view voice_name) const {
  return "program " + m_voice_port + ":" + std::to_string(m_voice_channel) + " " +
         std::string(voice_name);
}

void BrowserModel::set_kit_port(std::string port) {
  m_kit_port = port.empty() ? std::string("out0") : std::move(port);
}

void BrowserModel::set_kit_channel(int channel_one_based) {
  m_kit_channel = std::clamp(channel_one_based, 1, 16);
}

std::string BrowserModel::build_kit_verb(std::size_t index) const {
  return "program " + m_kit_port + ":" + std::to_string(m_kit_channel) + " " +
         std::to_string(kGmDrumKitPrograms[index]);
}

}  // namespace sonotron
