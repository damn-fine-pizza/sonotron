#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// Pure-data model for the Browser zone (ux-workstation.md §4.3): a
// searchable tree of the three draggable material kinds — Styles, Clips,
// MIDI seqs. No ImGui, no I/O; browser_panel.cpp is the only file that
// touches ImGui or drag-drop payloads (the *_panel/*_model split,
// docs/design/gui-fase2-mechanical-plan.md's Invariants).

namespace sonotron {

// The 16 builtin styles, in the SAME order as `style load <name>`
// (components/hostrt/shell_music_commands.cpp::cmd_style's "load" verb
// resolves by name via `find_builtin_style`, case-insensitively) and as
// declared in components/arrangrr/include/arrangrr/arranger/style.hpp's
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

class BrowserModel {
 public:
  std::size_t style_count() const { return kBuiltinStyleNames.size(); }
  std::string_view style_name(std::size_t index) const;

  // Clips / MIDI seqs: user-authored material (§4.3's "Clips" and
  // "MIDI seqs" branches). Nothing is authored yet in this slice — there is
  // no recorder/authoring UI and no clip primitive on the wire — so both
  // stay empty lists, an honest placeholder browser_panel.cpp renders as
  // "(none authored yet)" rather than inventing sample entries.
  const std::vector<std::string>& clip_names() const { return m_clips; }
  const std::vector<std::string>& midi_seq_names() const { return m_midi_seqs; }

  // The "[search…]" field (§3 wireframe): case-insensitive substring match
  // against style names. An empty filter matches everything.
  void set_search_filter(std::string filter) { m_filter = std::move(filter); }
  const std::string& search_filter() const { return m_filter; }
  bool style_matches_filter(std::size_t index) const;

 private:
  std::vector<std::string> m_clips;
  std::vector<std::string> m_midi_seqs;
  std::string m_filter;
};

}  // namespace sonotron
