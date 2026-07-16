#include "style_chooser.hpp"

#include <utility>

namespace arrangrr::host {

namespace {

constexpr char kSelectedMarker = '>';
constexpr char kUnselectedMarker = ' ';

constexpr char kDigitLow = '0';
constexpr char kDigitHigh = '9';

const char* section_short_name(SectionKind t) {
  switch (t) {
    case SectionKind::kIntro1:
      return "Intro1";
    case SectionKind::kIntro2:
      return "Intro2";
    case SectionKind::kVarA:
      return "VarA";
    case SectionKind::kVarB:
      return "VarB";
    case SectionKind::kVarC:
      return "VarC";
    case SectionKind::kVarD:
      return "VarD";
    case SectionKind::kFillA:
      return "FillA";
    case SectionKind::kFillB:
      return "FillB";
    case SectionKind::kFillC:
      return "FillC";
    case SectionKind::kFillD:
      return "FillD";
    case SectionKind::kBreak:
      return "Break";
    case SectionKind::kEnding1:
      return "Ending1";
    case SectionKind::kEnding2:
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
  if (next == m_style_pos) {
    return;
  }
  // Preserve the highlighted variation across the style change: remember its
  // TYPE, move, then re-find that type in the new style (all builtins share the
  // same section vocabulary, so this normally succeeds; clamp if it does not).
  const SectionKind keep = selected_section();
  m_style_pos = next;
  const StyleInfo* style = selected_style();
  if (style == nullptr || style->sections.empty()) {
    m_section_pos = 0;
    return;
  }
  for (std::size_t i = 0; i < style->sections.size(); ++i) {
    if (style->sections[i] == keep) {
      m_section_pos = static_cast<int>(i);
      return;
    }
  }
  m_section_pos = clamp_index(m_section_pos, static_cast<int>(style->sections.size()));
}

void StyleChooser::nav_section(int delta) {
  const StyleInfo* style = selected_style();
  const int count = style == nullptr ? 0 : static_cast<int>(style->sections.size());
  m_section_pos = clamp_index(m_section_pos + delta, count);
}

void StyleChooser::select(int style_index, SectionKind section) {
  const std::vector<std::size_t> indices = filtered_indices();
  for (std::size_t pos = 0; pos < indices.size(); ++pos) {
    if (m_styles[indices[pos]].index == style_index) {
      m_style_pos = static_cast<int>(pos);
      break;
    }
  }
  m_section_pos = 0;
  const StyleInfo* style = selected_style();
  if (style == nullptr) {
    return;
  }
  for (std::size_t i = 0; i < style->sections.size(); ++i) {
    if (style->sections[i] == section) {
      m_section_pos = static_cast<int>(i);
      return;
    }
  }
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

SectionKind StyleChooser::selected_section() const {
  const StyleInfo* style = selected_style();
  if (style == nullptr || style->sections.empty()) {
    return SectionKind::kVarA;
  }
  const int pos = clamp_index(m_section_pos, static_cast<int>(style->sections.size()));
  return style->sections[static_cast<std::size_t>(pos)];
}

std::vector<std::string> StyleChooser::render(NoteNaming /*naming*/, const UiStyle& style) const {
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
      const bool selected = static_cast<int>(i) == style_pos;
      std::string token(1, selected ? kSelectedMarker : kUnselectedMarker);
      token += std::to_string(info.index);
      token += ' ';
      token += info.name;
      style_line += selected ? style.apply(UiRole::kSuccess, token) : token;
      style_line += ' ';
    }
  }
  lines.push_back(style_line);

  std::string section_line = "section: ";
  const StyleInfo* selected_style_info = selected_style();
  if (selected_style_info == nullptr || selected_style_info->sections.empty()) {
    section_line += "(none)";
  } else {
    const int section_pos =
        clamp_index(m_section_pos, static_cast<int>(selected_style_info->sections.size()));
    for (std::size_t i = 0; i < selected_style_info->sections.size(); ++i) {
      const bool selected = static_cast<int>(i) == section_pos;
      std::string token(1, selected ? kSelectedMarker : kUnselectedMarker);
      token += section_short_name(selected_style_info->sections[i]);
      section_line += selected ? style.apply(UiRole::kSuccess, token) : token;
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
