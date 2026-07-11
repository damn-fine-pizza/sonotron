#pragma once

#include <array>
#include <cstddef>
#include <string_view>

// The 9 Live-Loops grid part rows (ux-workstation.md §4.4, §4.6): the exact,
// ordered TrackRole vocabulary read (read-only) from components/arrangrr/
// include/arrangrr/timeline/timeline.hpp (`kDrums..kLead`, in that enum
// order) and components/hostrt/shell_parse.cpp's `parse_role()` (the lower-
// case wire tokens `part <role> mute|solo on|off` expects). gui-sonotron
// never includes the core (D38, pure client) — these two arrays are a
// hand-copied literal of what was read, not an invented list, shared by
// GridModel, PartsModel and SeqEditModel so the 9 rows never drift between
// panels. `TrackRole::kCc` is deliberately excluded: it is a control-change
// utility role, not a performable grid/mixer row (ux-workstation.md §4.4
// only lists drums/perc/bass/chord1/chord2/pad/arp/phrase/lead).

namespace sonotron {

inline constexpr std::size_t kTrackRoleCount = 9;

// Display labels, Title Case, for panel text.
inline constexpr std::array<std::string_view, kTrackRoleCount> kTrackRoleLabels = {
    "Drums", "Perc", "Bass", "Chord1", "Chord2", "Pad", "Arp", "Phrase", "Lead",
};

// Wire tokens, exactly as `components/hostrt/shell_parse.cpp::parse_role()`
// matches them (lower-case, no spaces) — what a panel puts into a
// `part <role> mute|solo on|off` L1 command line.
inline constexpr std::array<std::string_view, kTrackRoleCount> kTrackRoleWireTokens = {
    "drums", "perc", "bass", "chord1", "chord2", "pad", "arp", "phrase", "lead",
};

}  // namespace sonotron
