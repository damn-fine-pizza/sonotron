#pragma once

#include <string>
#include <vector>

#include "arrangrr/arranger/arranger.hpp"
#include "midi_monitor.hpp"
#include "ui_style.hpp"

// Host-only renderer for the `parts` panel: the arranger-band mixer. One row
// per style part (Drums..Phrase) showing its route channel, GM voice, live
// mute/solo state and a real-time activity meter read from the MIDI monitor.
// Pure: no terminal access, no I/O — styling goes through UiStyle roles, so
// with colours disabled the output is plain text.

namespace arrangrr::host {

// The eight automatic style parts shown in the mixer (roles 0..7); kLead/kCc
// are excluded — they are not arranger band parts.
inline constexpr std::size_t kMixerPartCount = 8;

// Renders the mixer to lines no wider than `cols`. `selected` is the 0-based
// highlighted row (clamped). Activity counts the monitor's active notes on each
// part's routed port+channel.
std::vector<std::string> render_parts_panel(const Arranger& arranger, const MidiMonitor& monitor,
                                            int selected, int cols, const UiStyle& style);

// The TrackRole shown at mixer row `index` (0..kMixerPartCount-1).
TrackRole mixer_role(std::size_t index);

}  // namespace arrangrr::host
