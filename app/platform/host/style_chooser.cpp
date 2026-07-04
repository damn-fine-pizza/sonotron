#include "style_chooser.hpp"

#include <utility>

namespace arrangrr::host {

namespace {

constexpr char kSelectedMarker = '>';
constexpr char kUnselectedMarker = ' ';

constexpr char kDigitLow = '0';
constexpr char kDigitHigh = '9';

const char* section_short_name(SectionType t) {
  switch (t) {
    case SectionType::kIntro1:
      return "Intro1";
    case SectionType::kIntro2:
      return "Intro2";
    case SectionType::kVarA:
      return "VarA";
    case SectionType::kVarB:
      return "VarB";
    case SectionType::kVarC:
      return "VarC";
    case SectionType::kVarD:
      return "VarD";
    case SectionType::kFillA:
      return "FillA";
    case SectionType::kFillB:
      return "FillB";
    case SectionType::kFillC:
      return "FillC";
    case SectionType::kFillD:
      return "FillD";
    case SectionType::kBreak:
      return "Break";
    case SectionType::kEnding1:
      return "Ending1";
    case SectionType::kEnding2:
      return "Ending2";
  }
  return "?";
}

int clamp_index(int value, int count) {
  if (count <= 0) {
    return 0;
  }
  if (value < 0) {
    return 0;
  }
  if (value >= count) {
    return count - 1;
  }
  return value;
}

}  // namespace

StyleChooser::StyleChooser(std::vector<StyleInfo> styles) : m_styles(std::move(styles)) {}

std::vector<std::size_t> StyleChooser::filtered_indices() const {
  std::vector<std::size_t> out;
  for (std::size_t i = 0; i < m_styles.size(); ++i) {
    const std::string key = std::to_string(m_styles[i].index);
    if (m_filter.empty() || key.find(m_filter) != std::string::npos) {
      out.push_back(i);
    }
  }
  return out;
}

void StyleChooser::feed_digit(char d) {
  if (d < kDigitLow || d > kDigitHigh) {
    return;
  }
  m_filter.push_back(d);
  // Changing the filter resets the style highlight to the first match, and the
  // section highlight to that style's first section.
  m_style_pos = 0;
  m_section_pos = 0;
}

void StyleChooser::backspace() {
  if (m_filter.empty()) {
    return;
  }
  m_filter.pop_back();
  m_style_pos = 0;
  m_section_pos = 0;
}

void StyleChooser::nav_style(int delta) {
  const auto count = static_cast<int>(filtered_indices().size());
  const int next = clamp_index(m_style_pos + delta, count);
  if (next != m_style_pos) {
    m_style_pos = next;
    // Changing style resets the section highlight to that style's first.
    m_section_pos = 0;
  }
}

void StyleChooser::nav_section(int delta) {
  const StyleInfo* style = selected_style();
  const int count = style == nullptr ? 0 : static_cast<int>(style->sections.size());
  m_section_pos = clamp_index(m_section_pos + delta, count);
}

std::vector<StyleInfo> StyleChooser::filtered() const {
  std::vector<StyleInfo> out;
  for (const std::size_t i : filtered_indices()) {
    out.push_back(m_styles[i]);
  }
  return out;
}

const StyleInfo* StyleChooser::selected_style() const {
  const std::vector<std::size_t> indices = filtered_indices();
  if (indices.empty()) {
    return nullptr;
  }
  const int pos = clamp_index(m_style_pos, static_cast<int>(indices.size()));
  return &m_styles[indices[static_cast<std::size_t>(pos)]];
}

SectionType StyleChooser::selected_section() const {
  const StyleInfo* style = selected_style();
  if (style == nullptr || style->sections.empty()) {
    return SectionType::kVarA;
  }
  const int pos = clamp_index(m_section_pos, static_cast<int>(style->sections.size()));
  return style->sections[static_cast<std::size_t>(pos)];
}

std::vector<std::string> StyleChooser::render(NoteNaming /*naming*/) const {
  std::vector<std::string> lines;

  const std::vector<std::size_t> indices = filtered_indices();
  const int style_pos = clamp_index(m_style_pos, static_cast<int>(indices.size()));

  std::string style_line = "style:[";
  style_line += m_filter;
  style_line += "] ";
  if (indices.empty()) {
    style_line += "(no match)";
  } else {
    for (std::size_t i = 0; i < indices.size(); ++i) {
      const StyleInfo& info = m_styles[indices[i]];
      style_line += (static_cast<int>(i) == style_pos ? kSelectedMarker : kUnselectedMarker);
      style_line += std::to_string(info.index);
      style_line += ' ';
      style_line += info.name;
      style_line += ' ';
    }
  }
  lines.push_back(style_line);

  std::string section_line = "section: ";
  const StyleInfo* style = selected_style();
  if (style == nullptr || style->sections.empty()) {
    section_line += "(none)";
  } else {
    const int section_pos = clamp_index(m_section_pos, static_cast<int>(style->sections.size()));
    for (std::size_t i = 0; i < style->sections.size(); ++i) {
      section_line += (static_cast<int>(i) == section_pos ? kSelectedMarker : kUnselectedMarker);
      section_line += section_short_name(style->sections[i]);
      section_line += ' ';
    }
  }
  lines.push_back(section_line);

  lines.emplace_back(
      "ENTER next-bar | CTRL+\\ now | ESC cancel | digits filter | up/down style | "
      "left/right section");

  return lines;
}

}  // namespace arrangrr::host
