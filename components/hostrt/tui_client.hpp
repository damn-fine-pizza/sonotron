#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "midi_monitor.hpp"
#include "param_state_mirror.hpp"

// The client-side presentation half of the client/server seam (docs/design/
// orchestrator-pipeline-extraction.md §17.1/§17.5 Phase 3b): everything a
// PURE socket client needs to translate a user gesture into the ALREADY-
// EXISTING L1 grammar and to fold the inbound JSONL wire back into the
// core-free mirror structs Seam D (§17.2) already established
// (param_state_mirror.hpp, groove_view.hpp/arp_view.hpp's *ViewParams,
// midi_monitor.hpp's MidiOutEvent). Deliberately does NOT #include
// arrangrr/abi.hpp or any other core header -- verified structurally by
// grep, not just by convention (D38's "pure client" rule, extended from
// gui-sonotron to cli-arrangrr's own client half per this milestone).
//
// Scope of this increment: the socket-session plumbing + the gesture
// translators the client/server contract gap actually named (§17.3a/b) --
// note-on/off, groove/arp adjust, part mute/solo. Full TUI panel-rendering
// parity (piano/style/chords key dispatch, panel focus state) is NOT
// reproduced here yet; that is the next mechanical slice once this seam is
// proven (mirrors §17.5's own "prove the seam before the big cutover"
// sequencing advice).

namespace arrangrr::host {

class TuiClient {
 public:
  // Forwards one L1 text line to the server. The ONLY channel a gesture
  // translator uses to reach the engine -- never a Command, never raw MIDI
  // bytes (D38, extended to cli-arrangrr by this milestone).
  using SendLine = std::function<void(const std::string&)>;

  explicit TuiClient(SendLine send) : m_send(std::move(send)) {}

  // Decodes one inbound wire line and folds it into the local mirror/
  // monitor. Recognizes kParamState's "ev":"param" shape (param_state_wire
  // .hpp) and the other 7 shapes (client_event.hpp); an unrecognized or
  // malformed line is ignored -- never throws, never crashes on a stray or
  // future-additive line (additive-only ABI growth, abi.hpp's own
  // invariant).
  void on_wire_line(const std::string& line);

  const ParamStateMirror& params() const { return m_mirror; }
  const MidiMonitor& monitor() const { return m_monitor; }

  // A short "<field>=<value>" description of the most recent kParamState
  // fold (e.g. "groove.swing=42"), or empty when the last inbound line
  // was not a param-state echo. Observability seam for callers (and tests)
  // that want to prove a specific gesture's round trip landed, without
  // reaching into the mirror's internals field by field.
  const std::string& last_param_change() const { return m_last_param_change; }

  // Gesture translators -- each builds the SAME L1 verb text the existing
  // TUI's cmd_* grammar already accepts and forwards it via SendLine. None
  // of these touches a core type or an ABI Command; deltas are computed
  // against the locally-shadowed ParamStateMirror (kept in sync by
  // on_wire_line's kParamState folding), exactly as §17.3a's resolution
  // describes.

  // note <port>[:ch] on|off <midinote> [velocity] -- the wire-safe
  // equivalent of a piano/chords key gesture (surface_send_note(), §17.3a).
  void send_note(const std::string& port_name, int channel_1based, std::uint8_t midi_note,
                 std::uint8_t velocity, bool on);

  // Groove/arp panel row indices, matching the existing TUI's own row
  // order (shell_input.cpp's kGrooveRowField / ArpRow) -- the caller (a
  // future panel-key dispatcher) supplies the SAME selected-row index the
  // live panel already tracks.
  static constexpr int kGrooveRowCount = 6;
  static constexpr int kArpRowCount = 6;

  // Reads the CURRENT value for `row` from the mirror, applies the SAME
  // clamped delta rule Shell::groove_adjust()/arp_adjust() already use, and
  // sends the resulting `groove <field> <value>` / `arp ...` L1 line.
  void groove_adjust(int row, int delta);
  void arp_adjust(int row, int delta);

  // Toggles mute/solo for the mixer row `role_index` (0..7, the SAME
  // Drums..Phrase order as shell_internal.hpp's kMixerParts), sending
  // `part <role> mute|solo on|off` with the flipped current value.
  void part_toggle_mute(int role_index);
  void part_toggle_solo(int role_index);

 private:
  SendLine m_send;
  ParamStateMirror m_mirror;
  MidiMonitor m_monitor;
  std::string m_last_param_change;
};

}  // namespace arrangrr::host
