#include "app_state.hpp"

namespace sonotron {

namespace {

// Formats one decoded event into a short, human-readable log line. Panels
// (and the log) never see the raw wire text -- see brain_event.hpp's
// "panels never see raw text" design note -- so this is derived from the
// decoded POD, not the JSONL source.
std::string format_log_line(const BrainEvent& ev) {
  switch (ev.kind) {
    case BrainEvent::Kind::kMidiOut:
      return "midi-out port=" + std::to_string(ev.port) + " msg=" + ev.msg;
    case BrainEvent::Kind::kChord:
      return "chord " + ev.chord_in + " -> " + ev.chord_out + " (" + ev.chord_deg + ")";
    case BrainEvent::Kind::kSection:
      return "section " + ev.section_name;
    case BrainEvent::Kind::kTransport:
      return "transport " + ev.transport_state;
    case BrainEvent::Kind::kWarn:
      return "warn " + ev.warn_code;
    case BrainEvent::Kind::kError:
      return "error: " + ev.error + " (cmd: " + ev.cmd + ")";
    case BrainEvent::Kind::kChordFollowed:
      return "chord-followed " + ev.followed_current + " / next " + ev.followed_next + " (" +
             ev.followed_source + ")";
    case BrainEvent::Kind::kBeat:
      return "beat " + std::to_string(ev.beat_bar) + "." + std::to_string(ev.beat_index) + "." +
             std::to_string(ev.beat_pulse);
    case BrainEvent::Kind::kClip:
      return "clip " + std::to_string(ev.clip_id) + " " + ev.clip_state;
    case BrainEvent::Kind::kLoop:
      return "loop " + std::to_string(ev.loop_slot_id) + " " + ev.loop_event_kind;
    case BrainEvent::Kind::kTimeSig:
      return "time-sig " + std::to_string(ev.time_sig_beats_per_bar) + "/4";
    case BrainEvent::Kind::kUnknown:
    default:
      return "unknown/malformed event";
  }
}

// Decodes the "clip" OutEvent's `clip_state` string label (event_labels.hpp's
// clip_state_name on the host side) into AppState::ClipLaunchState. An
// unrecognized label reads as kStopped -- never a crash, never a stale
// leftover state.
AppState::ClipLaunchState parse_clip_launch_state(const std::string& s) {
  if (s == "armed") {
    return AppState::ClipLaunchState::kArmed;
  }
  if (s == "playing") {
    return AppState::ClipLaunchState::kPlaying;
  }
  if (s == "queued_stop") {
    return AppState::ClipLaunchState::kQueuedStop;
  }
  return AppState::ClipLaunchState::kStopped;
}

}  // namespace

void AppState::apply(const BrainEvent& ev) {
  m_log.push_back(format_log_line(ev));
  while (m_log.size() > kMaxLog) {
    m_log.pop_front();
  }

  if (!ev.valid) {
    return;
  }
  switch (ev.kind) {
    case BrainEvent::Kind::kChord:
      m_chord_in = ev.chord_in;
      m_chord_out = ev.chord_out;
      m_chord_deg = ev.chord_deg;
      break;
    case BrainEvent::Kind::kSection:
      m_section = ev.section_name;
      break;
    case BrainEvent::Kind::kTransport:
      if (ev.transport_state == "playing") {
        m_transport = Transport::kPlaying;
      } else if (ev.transport_state == "paused") {
        m_transport = Transport::kPaused;
      } else {
        m_transport = Transport::kStopped;
        m_manual_steer = false;  // back to rest: let the activity gate close
        // Stop parks the playhead honestly: no position lingers on screen
        // once the transport is no longer moving.
        m_bar = 0;
        m_beat = 0;
        m_pulse = 0;
      }
      break;
    case BrainEvent::Kind::kChordFollowed:
      m_chord_followed_current = ev.followed_current;
      m_chord_followed_current_valid = ev.followed_current != "-";
      m_chord_followed_next = ev.followed_next;
      m_chord_followed_next_valid = ev.followed_next != "-";
      m_chord_followed_source = ev.followed_source;
      if (m_chord_followed_current_valid) {
        // Authoritative confirmation that a chord was steered -- opens the
        // same activity gate note_manual_steer()'s optimistic hint does.
        m_manual_steer = true;
      }
      break;
    case BrainEvent::Kind::kBeat:
      m_bar = ev.beat_bar;
      m_beat = ev.beat_index;
      m_pulse = ev.beat_pulse;
      // A beat only fires while the core transport is running, so receiving one
      // is authoritative confirmation of playback -- adopt kPlaying even if the
      // GUI connected mid-play and never saw the transport "playing" event
      // (there is no replay on connect). Mirrors kChordFollowed opening the
      // activity gate above, and keeps the panel from showing a moving playhead
      // beside a "stopped" label. A later "stopped" event resets it (and parks
      // the playhead).
      m_transport = Transport::kPlaying;
      break;
    case BrainEvent::Kind::kClip:
      // Real per-cell readback (repeat-zone-real-contract.md §3): the ONLY
      // consumer of this event was a log line before this fix -- grid_panel.cpp
      // now reads playing/armed/queued-stop state off this map instead of a
      // local click-time echo (V02State's former row_playing).
      m_clip_states[ev.clip_id] = parse_clip_launch_state(ev.clip_state);
      break;
    case BrainEvent::Kind::kTimeSig:
      // The Repeat-Zone beat-synchronized playhead (grid_model.hpp's
      // section_playhead_phase) needs the REAL beats_per_bar, not the
      // hand-copied 4/4 default -- guarded > 0 so a malformed/zero announce
      // can never leave the playhead dividing by zero downstream.
      if (ev.time_sig_beats_per_bar > 0) {
        m_beats_per_bar = ev.time_sig_beats_per_bar;
      }
      break;
    case BrainEvent::Kind::kMidiOut:
    case BrainEvent::Kind::kWarn:
    case BrainEvent::Kind::kError:
    case BrainEvent::Kind::kUnknown:
    // Phase 7 (node 6000, the Looper -- item 9): decoded honestly above
    // (no longer misread as a warning), but AppState has no per-slot loop
    // recording-state view yet -- that is item 10/11's GUI panel work, a
    // later slice. Logged above; no view-state change here yet.
    case BrainEvent::Kind::kLoop:
      break;  // logged above, no other view-state change
  }
}

void AppState::apply_line(const std::string& raw) { apply(parse_brain_event(raw)); }

}  // namespace sonotron
