#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "brain_event.hpp"

// The abstract command/event/snapshot boundary hiding whether the brain runs
// in-process (a future static link to hostrt) or across the UDS control
// socket (ux-workstation.md §9). Per the architectural-quality requirement,
// the physical transport is a swappable detail behind this interface -- not
// a decision baked into the panels. The panels depend only on BrainSession
// (never on UdsBrainSession directly), so a future InProcessBrainSession
// slots in without a panel rewrite.

namespace sonotron {

// Best-effort connect-time state (style/section/key/bpm, per-part mute/
// solo/program, groove/arp, follow mode). No shipped event currently
// populates this: `Op::kGet` is unwired in v1 and there is no
// snapshot-on-connect (ux-workstation.md §11.4, gap #4) -- every field stays
// "unknown" until that follow-on additive work lands. Present now, minimal
// on purpose, so callers can code against the final shape without a
// rewrite; extend it when §11.4 ships a real wire source.
struct BrainSnapshot {
  bool style_known = false;
  std::string style;
  bool section_known = false;
  std::string section;
  bool key_known = false;
  std::string key;
  bool bpm_known = false;
  int bpm = 0;
};

class BrainSession {
 public:
  enum class Status { kDisconnected, kConnecting, kConnected, kError };

  virtual ~BrainSession() = default;

  // Sends one L1 command line (no trailing '\n' -- the transport appends
  // it). Implementations MUST silently refuse `quit`/`exit`: those verbs
  // tear down the whole shared host for every connected client
  // (gui-contract-map.md §0), so a window-close or a stray Enter must never
  // forward them.
  virtual void send(std::string_view command_line) = 0;

  // Non-blocking: appends every event decoded since the last call and
  // returns immediately (drains whatever the transport currently has
  // ready). Call once per render frame -- see gui-contract-map.md §1's
  // poll-in-frame integration shape (no background reader thread).
  virtual void poll(std::vector<BrainEvent>& out) = 0;

  // Best-effort connect-time state; see BrainSnapshot.
  virtual const BrainSnapshot& snapshot() const = 0;

  virtual Status status() const = 0;
};

}  // namespace sonotron
