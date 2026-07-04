#pragma once

#include <string>
#include <vector>

#include "arrangrr/arranger/style.hpp"
#include "note_names.hpp"
#include "ui_style.hpp"

// Pure state machine for the interactive style/section chooser, host side only:
// NO terminal, NO engine, NO I/O. The caller drives it from key events and
// renders render() into the contextual panel, then, on ENTER/CTRL+\, issues the
// resolved (style index, section, immediate) as a kStyleSwitch command. Keeping
// this a pure value object makes it trivially unit-testable and portable.

namespace arrangrr::host {

// One selectable built-in style, built by the caller from styles::kBuiltins.
struct StyleInfo {
  int index = 0;                      // builtin index (matched by the core)
  std::string name;                   // display only, e.g. "basic"
  std::vector<SectionType> sections;  // the sections this style actually defines
};

// How ENTER vs CTRL+\ apply the switch. The chooser only reports intent; the
// caller maps it to the kStyleSwitch `immediate` flag.
enum class ChooserApply { kNextBar, kImmediate };

class StyleChooser {
 public:
  explicit StyleChooser(std::vector<StyleInfo> styles);

  // Filter is digits only; a non-digit is ignored.
  void feed_digit(char d);
  void backspace();

  // Navigate within the FILTERED style list / the selected style's sections.
  // Both CLAMP at the ends (no wrap) so repeated keypresses rest on the edge.
  void nav_style(int delta);
  void nav_section(int delta);

  const std::string& filter() const { return m_filter; }

  // Styles whose std::to_string(index) CONTAINS the filter substring: "13"
  // matches 13, 113, 213, 130; "130" matches only 130. Empty filter = all.
  std::vector<StyleInfo> filtered() const;

  // The highlighted style, or nullptr when the filter matches nothing. The
  // pointer is stable (into the chooser's own storage) until the next mutation.
  const StyleInfo* selected_style() const;

  // The highlighted section of the selected style (kVarA when none).
  SectionType selected_section() const;

  // Compact block for the contextual panel: a "style:" line, a "section:" line,
  // and a hint line. The currently highlighted style/section are styled bold+
  // colour via `style` (kSuccess role); with colours off they stay plain.
  // `naming` is accepted for API symmetry; section labels are structural.
  std::vector<std::string> render(NoteNaming naming, const UiStyle& style) const;

 private:
  std::vector<std::size_t> filtered_indices() const;

  std::vector<StyleInfo> m_styles;
  std::string m_filter;
  int m_style_pos = 0;    // index into the FILTERED list
  int m_section_pos = 0;  // index into the selected style's sections
};

}  // namespace arrangrr::host
