#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Pure-data model for the Browser zone (ux-workstation.md §4.3): a
// searchable tree of the three draggable material kinds — Styles, Clips,
// MIDI seqs. No ImGui, no I/O; browser_panel.cpp is the only file that
// touches ImGui or drag-drop payloads (the *_panel/*_model split,
// docs/design/gui-fase2-mechanical-plan.md's Invariants).

namespace sonotron {

// The 16 builtin styles, in the SAME order as `style load <name>`
// (components/platform/hostrt/shell_music_commands.cpp::cmd_style's "load" verb
// resolves by name via `find_builtin_style`, case-insensitively) and as
// declared in components/core/arrangrr/include/arrangrr/arranger/style.hpp's
// `styles::kBuiltins` — read (read-only) to source this list; gui-sonotron
// never includes the core (D38, pure client), so this is a hand-copied
// literal of what was read, not an invented name list.
inline constexpr std::array<std::string_view, 16> kBuiltinStyleNames = {
    "basic", "pop",   "rock",   "ballad",  "funk",  "disco",   "house", "swing",
    "bossa", "samba", "reggae", "country", "blues", "shuffle", "latin", "motown",
};

// ImGui drag-drop payload id carrying a style index (std::size_t into
// kBuiltinStyleNames) from browser_panel.cpp's drag source to
// grid_panel.cpp's cell drop target. Declared here (a plain string literal,
// no ImGui dependency) so both panels share the exact same id without
// either including the other's header.
inline constexpr const char* kStyleDragPayloadId = "SONOTRON_STYLE_INDEX";

// ImGui drag-drop payload id carrying a SectionType byte (repeat-zone-real-
// contract.md SLICE 4a) from browser_panel.cpp's "variations" list drag
// source to grid_panel.cpp's scene-header drop target. Distinct from
// kStyleDragPayloadId above (a style INDEX) so ImGui::AcceptDragDropPayload
// never confuses the two payload shapes at a shared drop site.
inline constexpr const char* kVariationDragPayloadId = "SONOTRON_VARIATION_SECTION";

// Primary taxonomy axis for the style browser at scale (task #30, docs/
// proposals/style-browser-corpus-scale.md §2.1's 7+1-family tree, derived
// from the corpus's own measured rhythmic-family clustering, NOT from
// filename vibes). kOther is not decorative: a real fraction of any future
// imported corpus will have no confident family (the doc measured 54% of
// the real 1010-style corpus carrying no genre token at all) -- showing
// that state honestly, as its own filterable bucket, is the difference
// between a taxonomy and a silent mis-bucketing a player will notice.
enum class StyleFamily : std::uint8_t {
  kPopRockBallad,
  kDanceFourOnFloor,
  kFunkGroove,
  kSwingShuffleJazz,
  kLatinClave,
  kBallroomTraditional,
  kWorldRegional,
  kOther,
};

// Display label for a family bucket (used by the browser panel's section
// headers and its filter combo). Never returns an empty view -- every
// enumerator has a real label.
//
// NAMING SUBTLETY (found while building this taxonomy, do not re-derive it):
// style_matches_filter's text-search haystack is `name + " " + style_family_
// label(family)` -- so a label that spells out a DIFFERENT sibling style's
// own name inside the SAME family silently breaks "family filter + text
// narrows correctly", because every style in that family shares the same
// label text. kPopRockBallad's siblings are pop/rock/ballad/country/motown;
// a label of "Pop / Rock / Ballad" would make searching "roc" match every
// one of them (their haystacks all carry the substring "Rock" via the
// label), not just "rock" itself -- hence "Pop / Ballad" below (rock is
// still found trivially by its own name). The same scrutiny was applied to
// every other family with more than one sibling among today's 16 built-ins:
// kSwingShuffleJazz's siblings are swing/blues/shuffle, so its label avoids
// spelling either "swing" or "shuffle" ("Big Band / Jazz" instead); kLatinClave's
// siblings are bossa/samba/reggae/latin, so its label avoids all four
// ("Clave / Tropical" instead). kFunkGroove/kOther have exactly one sibling
// each among the 16 (funk, basic) so there is nothing to cross-contaminate;
// kBallroomTraditional/kWorldRegional have none of the 16 assigned to them
// yet, so their labels use the doc's own family names verbatim.
std::string_view style_family_label(StyleFamily family);

// Hand-classified per docs/proposals/style-browser-corpus-scale.md §2.1's
// family table, applied to TODAY's 16 built-in styles (kBuiltinStyleNames,
// same order/index). "basic" is a generic default style authoring no
// distinctive rhythmic signature of its own -- it deliberately lands in
// kOther rather than being force-fit into a family it doesn't really
// belong to, exercising the fallback bucket with REAL data, not only a
// synthetic test case.
inline constexpr std::array<StyleFamily, 16> kBuiltinStyleFamilies = {{
    StyleFamily::kOther,             // basic
    StyleFamily::kPopRockBallad,     // pop
    StyleFamily::kPopRockBallad,     // rock
    StyleFamily::kPopRockBallad,     // ballad
    StyleFamily::kFunkGroove,        // funk
    StyleFamily::kDanceFourOnFloor,  // disco
    StyleFamily::kDanceFourOnFloor,  // house
    StyleFamily::kSwingShuffleJazz,  // swing
    StyleFamily::kLatinClave,        // bossa
    StyleFamily::kLatinClave,        // samba
    StyleFamily::kLatinClave,        // reggae
    StyleFamily::kPopRockBallad,     // country
    StyleFamily::kSwingShuffleJazz,  // blues
    StyleFamily::kSwingShuffleJazz,  // shuffle
    StyleFamily::kLatinClave,        // latin
    StyleFamily::kPopRockBallad,     // motown
}};

class BrowserModel {
 public:
  std::size_t style_count() const { return kBuiltinStyleNames.size(); }
  std::string_view style_name(std::size_t index) const;
  StyleFamily style_family(std::size_t index) const;

  // Clips / MIDI seqs: user-authored material (§4.3's "Clips" and
  // "MIDI seqs" branches). Nothing is authored yet in this slice — there is
  // no recorder/authoring UI and no clip primitive on the wire — so both
  // stay empty lists, an honest placeholder browser_panel.cpp renders as
  // "(none authored yet)" rather than inventing sample entries.
  const std::vector<std::string>& clip_names() const { return m_clips; }
  const std::vector<std::string>& midi_seq_names() const { return m_midi_seqs; }

  // The "[search…]" field (§3 wireframe): case-insensitive substring match
  // against `name + " " + style_family_label(family)` (task #30 broadens the
  // haystack beyond the name alone, per docs/proposals/style-browser-corpus-
  // scale.md §3.2). An empty filter matches everything.
  void set_search_filter(std::string filter) { m_filter = std::move(filter); }
  const std::string& search_filter() const { return m_filter; }

  // Family filter (task #30): nullopt means "no family filter, every family
  // matches" -- ANDed with the text search above in style_matches_filter.
  void set_family_filter(std::optional<StyleFamily> family) { m_family_filter = family; }
  std::optional<StyleFamily> family_filter() const { return m_family_filter; }

  bool style_matches_filter(std::size_t index) const;

 private:
  std::vector<std::string> m_clips;
  std::vector<std::string> m_midi_seqs;
  std::string m_filter;
  std::optional<StyleFamily> m_family_filter;
};

}  // namespace sonotron
