#pragma once

#include <array>
#include <cstdint>

#include "arp_view.hpp"
#include "groove_view.hpp"
#include "param_state_wire.hpp"

// Client-side aggregator (docs/design/orchestrator-pipeline-extraction.md
// §17.3b/§17.5 Phase 3b): folds a stream of parsed `kParamState` wire echoes
// into the SAME core-free mirror structs (`GrooveViewParams`/`ArpViewParams`/
// per-role mute-solo/style index/chord detect-follow-mode/key) that
// hostrt::Shell already fills straight off its own live in-process
// Arranger&/ArpeggiatorEngine& (Seam D, §17.2, shell.cpp's
// refresh_groove_content/refresh_arp_content/refresh_parts_content/
// refresh_chords_content/refresh_styles_content) -- so a pure client can
// drive the EXACT SAME render_groove_panel()/render_arp_panel()/
// render_parts_panel() calls from a socket instead of a live engine, with no
// further reshape of the panel-rendering layer itself.
//
// Deliberately NOT wired into any transport yet (Phase 3b: built and
// unit-tested entirely in-process, per §17.5's sequencing, before any socket
// code exists) -- ParamStateMirror::apply() is exercised directly against
// synthetic ParamStateWire values AND against a real Shell's own emitted
// OutEvents in components/platform/hostrt/tests/test_param_state_wire.cpp, proving
// the decoded mirror matches the live Shell's own view structs field-for-
// field for the same script of actions.

namespace arrangrr::host {

// TrackRole's arranger-part roles this milestone's parts mixer shows (roles
// 0..7; kLead/kCc are excluded, mirroring parts_view.hpp's kMixerPartCount).
inline constexpr std::size_t kPartRoleCount = 8;

struct ParamStateMirror {
  GrooveViewParams groove{};
  ArpViewParams arp{};
  bool arp_enabled = false;
  std::array<bool, kPartRoleCount> part_muted{};
  std::array<bool, kPartRoleCount> part_soloed{};
  int style_index = -1;  // -1 = no `style load` echo observed yet
  bool chord_detect = false;
  std::uint8_t chord_detect_port = 0;
  std::uint8_t chord_follow = 0;  // raw ChordFollow value
  std::uint8_t chord_mode = 0;    // raw ChordMode value
  std::uint8_t key_root_pc = 0;
  std::uint8_t key_mode = 0;

  // Folds one decoded wire echo into the mirror. Unknown/out-of-range `sub`
  // values (a role index >= kPartRoleCount, an unrecognised GrooveField/
  // ArpField) are ignored rather than risking an out-of-bounds write --
  // additive-only ABI growth (abi.hpp's own invariant) means a NEWER server
  // build can legitimately send a field an OLDER client build doesn't know
  // yet.
  void apply(const ParamStateWire& w);
};

}  // namespace arrangrr::host
