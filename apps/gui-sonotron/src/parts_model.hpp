#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include "track_roles.hpp"

// Pure-data per-part mixer state for the Parts / Mixer zone
// (ux-workstation.md §4.6). No ImGui, no I/O; parts_panel.cpp is the only
// file that renders this with ImGui or sends L1 commands.

namespace sonotron {

// One row of the Parts/Mixer zone, shaped like the core's
// `Arranger::PartInfo {routed, port, channel, gm_program, muted, soloed,
// present}` (read as reference only — component headers are never included,
// D38). `muted`/`soloed` are OPTIMISTIC local hints, the same pattern
// AppState::note_transport_sent uses: parts_panel.cpp sends the REAL,
// already-shipped `part <role> mute|solo on|off` L1 verb (§7 B8) on toggle,
// and this flips immediately so the UI feels responsive — but there is no
// shipped readback yet (`Op::kGet` unwired, §11.4), so a mute/solo change
// made by another client is not reflected here until that gap closes.
// `gm_program` stays -1 ("unknown") for the same reason: no per-part
// program readback exists on the wire today.
struct PartInfo {
  bool muted = false;
  bool soloed = false;
  int gm_program = -1;
};

class PartsModel {
 public:
  static constexpr std::size_t kPartCount = kTrackRoleCount;

  std::string_view part_label(std::size_t index) const { return kTrackRoleLabels[index]; }
  std::string_view part_wire_token(std::size_t index) const { return kTrackRoleWireTokens[index]; }
  const PartInfo& part(std::size_t index) const { return m_parts[index]; }

  void toggle_mute(std::size_t index) { m_parts[index].muted = !m_parts[index].muted; }
  void toggle_solo(std::size_t index) { m_parts[index].soloed = !m_parts[index].soloed; }

 private:
  std::array<PartInfo, kPartCount> m_parts{};
};

}  // namespace sonotron
