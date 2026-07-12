#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "midi_monitor.hpp"
#include "ui_style.hpp"

// Host-only renderer for the `parts` panel: the arranger-band mixer. One row
// per style part (Drums..Phrase) showing its route channel, GM voice, live
// mute/solo state and a real-time activity meter read from the MIDI monitor.
// Pure: no terminal access, no I/O — styling goes through UiStyle roles, so
// with colours disabled the output is plain text.
//
// Seam D (docs/design/orchestrator-pipeline-extraction.md §17.2): decoupled
// from the live core `Arranger&`/`TrackRole` -- this file takes plain,
// caller-owned row data instead. Today (Phase 3a) the caller (`Shell`) fills
// each `PartRowState` straight off the live in-process `Arranger&`; a future
// pure client (Phase 3b/3c) fills the SAME shape from parsed
// `kParamState`/`kSection` events. No behavior change either way -- only the
// data's origin differs.

namespace arrangrr::host {

// The eight automatic style parts shown in the mixer (roles 0..7); kLead/kCc
// are excluded — they are not arranger band parts.
inline constexpr std::size_t kMixerPartCount = 8;

// One mixer row's live state -- pure data, no core type. `is_percussion`
// selects the "(kit)" voice label instead of a GM program name (GM
// percussion lives on channel 10 regardless of program).
struct PartRowState {
  std::string name;  // display label, e.g. "Drums"
  bool is_percussion = false;
  bool routed = false;
  std::uint8_t port = 0;
  std::uint8_t channel = 0;  // 0-based
  int gm_program = -1;       // -1 = no voice assigned
  bool muted = false;
  bool soloed = false;
};

// Renders the mixer to lines no wider than `cols`. `selected` is the 0-based
// highlighted row (clamped). `any_solo` mirrors `Arranger::any_solo()` (dims
// unsoloed rows when some other row is soloed). `rows` is expected to have
// `kMixerPartCount` entries, in display order. Activity counts the monitor's
// active notes on each row's routed port+channel.
std::vector<std::string> render_parts_panel(const std::vector<PartRowState>& rows, bool any_solo,
                                            const MidiMonitor& monitor, int selected, int cols,
                                            const UiStyle& style);

}  // namespace arrangrr::host
