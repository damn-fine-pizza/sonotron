#pragma once

#include <cstddef>
#include <deque>
#include <string>

#include "brain_event.hpp"

// Pure client-side view state (ux-workstation.md §9/§13's app_state
// salvage). The GUI holds NO authoritative state: the core is the truth, and
// this struct is only a snapshot re-derived from the decoded event stream
// (plus a couple of pragmatic hints about what the user just did). GPU-free
// and unit-testable: feed it decoded events (or raw wire lines via
// apply_line, for tests/convenience), inspect the result. Per BrainEvent's
// "panels never see raw text" design, the scrolling log holds a formatted
// summary of each event, not the verbatim JSONL.
//
// The harmony-visualiser gate (green = followed chord, amber = pending) from
// the spike is intentionally NOT ported as pitch-class masks here: that data
// comes only from the additive `chord-followed` event (gap P0-1,
// ux-workstation.md §11), which the core does not emit yet and whose wire
// field shape brain_event.hpp deliberately does not invent. What IS real
// today and ported here is the ACTIVITY gate itself -- whether the harmony
// surface should be considered "live" right now (transport playing, or the
// user just steered a chord) -- so a future harmony panel only has to plug a
// real pitch-class source into harmony_active(), not rebuild the gate.
namespace sonotron {

class AppState {
 public:
  enum class Transport { kStopped, kPlaying, kPaused };

  static constexpr std::size_t kMaxLog = 200;

  // Socket connection status, owned by the caller (the render loop sets it
  // from BrainSession::status()).
  void set_connected(bool connected) { m_connected = connected; }
  bool connected() const { return m_connected; }

  // Parses one incoming wire line and applies it (see apply()). Convenience
  // for tests and any caller that only has raw text; production code fed by
  // BrainSession::poll() should call apply() directly with the already
  // decoded event.
  void apply_line(const std::string& raw);

  // Applies one decoded event: logs a formatted summary, then reduces it
  // into the view state. An invalid event (malformed/unrecognized line) is
  // still logged but changes no other state.
  void apply(const BrainEvent& ev);

  // Optimistic hints from what the GUI just SENT (the core will confirm via
  // events, but these make the UI feel immediate and drive the activity
  // gate).
  void note_transport_sent(bool playing) {
    m_transport = playing ? Transport::kPlaying : Transport::kStopped;
    if (!playing) {
      m_manual_steer = false;
    }
  }
  void note_manual_steer() { m_manual_steer = true; }

  Transport transport() const { return m_transport; }
  const std::string& section() const { return m_section; }
  const std::string& chord_in() const { return m_chord_in; }
  const std::string& chord_out() const { return m_chord_out; }
  const std::string& chord_degree() const { return m_chord_deg; }
  const std::deque<std::string>& log() const { return m_log; }

  // Whether the harmony surface should read as "live" right now: transport
  // playing, or a chord was explicitly steered this run (mirrors the
  // spike's green-at-rest gate mechanism -- see the file comment for why no
  // pitch-class data exists to light with yet).
  bool harmony_active() const { return m_transport == Transport::kPlaying || m_manual_steer; }

 private:
  bool m_connected = false;
  Transport m_transport = Transport::kStopped;
  std::string m_section = "-";
  std::string m_chord_in = "-";
  std::string m_chord_out = "-";
  std::string m_chord_deg = "-";
  bool m_manual_steer = false;
  std::deque<std::string> m_log;
};

}  // namespace sonotron
