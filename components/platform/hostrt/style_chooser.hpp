#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "note_names.hpp"
#include "ui_style.hpp"

// Pure state machine for the interactive style/section chooser, host side only:
// NO terminal, NO engine, NO I/O. The caller drives it from key events and
// renders render() into the contextual panel, then, on ENTER/CTRL+\, issues the
// resolved (style index, section, immediate) as a kStyleSwitch command. Keeping
// this a pure value object makes it trivially unit-testable and portable.
//
// Seam D (docs/design/orchestrator-pipeline-extraction.md §17.2), Phase 3b:
// decoupled from the core `arrangrr::SectionType` -- `SectionKind` below
// mirrors its stable, additive-only ABI-numbered vocabulary (arrangrr/
// arranger/style_model.hpp), arrangrr-free. Today (Phase 3b) the caller
// (`Shell`, already core-linking) casts to/from the core `SectionType` at the
// boundary (both are std::uint8_t-valued 1:1, same numbering); a future pure
// client (Phase 3c) would build the same values straight off the parsed
// kSection/kParamState wire codes. No behavior change either way.

namespace arrangrr::host {

// Mirrors arrangrr::SectionType's raw values one-to-one (arrangrr/arranger/
// style_model.hpp) -- pure data, no core type.
enum class SectionKind : std::uint8_t {
  kIntro1 = 0,
  kIntro2 = 1,
  kVarA = 2,
  kVarB = 3,
  kVarC = 4,
  kVarD = 5,
  kFillA = 6,
  kFillB = 7,
  kFillC = 8,
  kFillD = 9,
  kBreak = 10,
  kEnding1 = 11,
  kEnding2 = 12,
};

// One selectable built-in style, built by the caller from styles::kBuiltins.
struct StyleInfo {
  int index = 0;                      // builtin index (matched by the core)
  std::string name;                   // display only, e.g. "basic"
  std::vector<SectionKind> sections;  // the sections this style actually defines
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
  // nav_style PRESERVES the highlighted section by TYPE across the change (the
  // same variation stays selected in the new style); it clamps only when the
  // new style lacks that section type.
  void nav_style(int delta);
  void nav_section(int delta);

  // Absolute placement: highlight the (filtered) style whose StyleInfo.index is
  // `style_index` and the section of that type, clamping when either is absent.
  // Used to seed the chooser from the arranger's live style/section.
  void select(int style_index, SectionKind section);

  const std::string& filter() const { return m_filter; }

  // Styles whose std::to_string(index) CONTAINS the filter substring: "13"
  // matches 13, 113, 213, 130; "130" matches only 130. Empty filter = all.
  std::vector<StyleInfo> filtered() const;

  // The highlighted style, or nullptr when the filter matches nothing. The
  // pointer is stable (into the chooser's own storage) until the next mutation.
  const StyleInfo* selected_style() const;

  // The highlighted section of the selected style (kVarA when none).
  SectionKind selected_section() const;

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
