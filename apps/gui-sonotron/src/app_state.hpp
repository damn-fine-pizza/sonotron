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
// The harmony-visualiser gate (green = followed chord, amber = pending,
// ux-workstation.md §10) is now fed by the additive `chord-followed` event
// (gap P0-1, pipeline-p0-mechanical-plan.md), decoded in brain_event.*. The
// ACTIVITY gate (harmony_active(): transport playing, or a chord was just
// steered) still governs whether the GREEN read is considered "live" --
// receiving a kChordFollowed event with a valid current chord is itself
// authoritative confirmation of a steer, so it opens the gate the same way
// note_manual_steer()'s optimistic hint does.
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
  // spike's green-at-rest gate mechanism).
  bool harmony_active() const { return m_transport == Transport::kPlaying || m_manual_steer; }

  // The followed harmonic context (gap P0-1): the chord the arranger commits
  // to THIS bar ("current", lights GREEN when harmony_active()) and the
  // shift-staged chord pending for the NEXT bar ("next", lights AMBER).
  // Labels are "-" (and *_valid() false) when the core reports no chord.
  const std::string& chord_followed_current() const { return m_chord_followed_current; }
  bool chord_followed_current_valid() const { return m_chord_followed_current_valid; }
  const std::string& chord_followed_next() const { return m_chord_followed_next; }
  bool chord_followed_next_valid() const { return m_chord_followed_next_valid; }
  const std::string& chord_followed_source() const { return m_chord_followed_source; }

 private:
  bool m_connected = false;
  Transport m_transport = Transport::kStopped;
  std::string m_section = "-";
  std::string m_chord_in = "-";
  std::string m_chord_out = "-";
  std::string m_chord_deg = "-";
  bool m_manual_steer = false;
  std::string m_chord_followed_current = "-";
  bool m_chord_followed_current_valid = false;
  std::string m_chord_followed_next = "-";
  bool m_chord_followed_next_valid = false;
  std::string m_chord_followed_source = "-";
  std::deque<std::string> m_log;
};

}  // namespace sonotron
