#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "brain_event.hpp"
#include "brain_session.hpp"
#include "debug_log.hpp"

// --debug / SONOTRON_DEBUG wire-level choke point (owner ask: root-cause the
// still-broken live auto-song). Every wire command a panel SENDS funnels
// through exactly one BrainSession::send() call on whichever concrete
// backend main() constructed (UdsBrainSession or InProcessBrainSession) --
// wrapping that ALREADY-CONSTRUCTED backend in this transparent decorator
// (rather than instrumenting every panel call site: grid_panel/transport_
// panel/parts_panel/browser_panel/main.cpp's menu all call brain_session.
// send()) is the one place that sees ALL of it, sent or received. Only
// main.cpp ever names this type -- the panels still depend on the abstract
// BrainSession, never on this decorator.
namespace sonotron {

class LoggingBrainSession final : public BrainSession {
 public:
  explicit LoggingBrainSession(BrainSession& inner) : m_inner(inner) {}

  void send(std::string_view command_line) override {
    if (debug_enabled()) {
      debug_log("[dbg send] " + std::string(command_line));
    }
    m_inner.send(command_line);
  }

  void poll(std::vector<BrainEvent>& out) override {
    const std::size_t before = out.size();
    m_inner.poll(out);
    if (debug_enabled()) {
      for (std::size_t i = before; i < out.size(); ++i) {
        log_received(out[i]);
      }
    }
  }

  const BrainSnapshot& snapshot() const override { return m_inner.snapshot(); }
  Status status() const override { return m_inner.status(); }

 private:
  // Every decoded event RECEIVED "that matters" (owner ask): section,
  // transport, clip, warn/error (verbatim), time-sig -- prefixed `[dbg
  // recv]`. kBeat is the transport heartbeat (one line per 24-PPQN pulse
  // while playing) -- printing every one would flood the log, so only a BAR
  // CHANGE is printed, prefixed `[dbg beat]` instead, tracked by
  // `m_last_beat_bar` right here (the single choke point every received
  // event already passes through).
  void log_received(const BrainEvent& ev) {
    switch (ev.kind) {
      case BrainEvent::Kind::kBeat:
        if (ev.beat_bar != m_last_beat_bar) {
          m_last_beat_bar = ev.beat_bar;
          debug_log("[dbg beat] bar=" + std::to_string(ev.beat_bar));
        }
        return;
      case BrainEvent::Kind::kSection:
        debug_log("[dbg recv] section " + ev.section_name);
        return;
      case BrainEvent::Kind::kTransport:
        debug_log("[dbg recv] transport " + ev.transport_state);
        return;
      case BrainEvent::Kind::kClip:
        debug_log("[dbg recv] clip " + std::to_string(ev.clip_id) + " " + ev.clip_state);
        return;
      case BrainEvent::Kind::kWarn:
        debug_log("[dbg recv] warn " + ev.warn_code);
        return;
      case BrainEvent::Kind::kError:
        debug_log("[dbg recv] error: " + ev.error + " (cmd: " + ev.cmd + ")");
        return;
      case BrainEvent::Kind::kTimeSig:
        debug_log("[dbg recv] time-sig " + std::to_string(ev.time_sig_beats_per_bar) + "/4");
        return;
      case BrainEvent::Kind::kMidiOut:
      case BrainEvent::Kind::kChord:
      case BrainEvent::Kind::kChordFollowed:
      case BrainEvent::Kind::kLoop:
      case BrainEvent::Kind::kParamState:
      case BrainEvent::Kind::kUnknown:
        return;  // not requested -- kept out of the trace to limit noise
    }
  }

  BrainSession& m_inner;
  int m_last_beat_bar = -1;
};

}  // namespace sonotron
