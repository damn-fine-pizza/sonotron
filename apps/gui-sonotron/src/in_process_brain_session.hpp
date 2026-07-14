#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "audio_midi_event.hpp"
#include "brain_session.hpp"

// The SECOND concrete BrainSession (Phase 2b, docs/design/
// sonotron-server-phase2-brief.md): the "integrated" mode -- no external
// `sonotron-server`, no socket. The GUI binary spawns a dedicated engine
// thread that owns the arrangrr Runtime + Stage + AlsaMidi in-process, and
// the render thread talks to it through two bounded SPSC lock-free rings
// (spsc_ring.hpp): a Command ring (GUI -> engine, this class's send()) and an
// OutEvent ring (engine -> GUI, drained by poll()). Same BrainSession
// contract UdsBrainSession implements, so the panels/AppState code never
// knows which transport is live (ux-workstation.md §9's whole point).
//
// D38 is retired for Phase 2 (owner-decided): unlike every OTHER header in
// apps/gui-sonotron/src, the .cpp behind this class links hostrt/runtime/
// arrangrr and names OutEvent/Command directly. This HEADER stays clean on
// purpose (pimpl: no core type appears here) so main.cpp itself keeps
// including zero core headers even though the gui_sonotron_engine library it
// now links transitively pulls the core in -- the D38 "never sees a core
// enum" discipline moves from an OS-process boundary to this library
// boundary (see this file's .cpp for the ring/thread ownership details).
//
// Thread-safety note (Corelli §15.6): the Runtime/Shell instance lives
// entirely inside the engine thread's own stack/closure and is NEVER
// reachable from this class or from GUI-thread code -- the GUI thread only
// ever touches the two ring endpoints (through send()/poll()) and the
// atomics below. There is no `engine()`/`stage()` accessor on this class,
// deliberately.
//
// Phase-6 Theme 2 addition (docs/phase6-design-reviews.md "Audio in the
// standalone GUI", Decision 2/3): set_audio_ring() below adds ONE narrow
// handle, mirroring the same discipline -- AudioMidiRing (audio_midi_event.
// hpp) is a small, arrangrr-free POD ring type, never arrangrr::OutEvent/
// Param/Kind, so this header still names zero arrangrr types.

namespace sonotron {

class InProcessBrainSession final : public BrainSession {
 public:
  InProcessBrainSession();
  ~InProcessBrainSession() override;

  InProcessBrainSession(const InProcessBrainSession&) = delete;
  InProcessBrainSession& operator=(const InProcessBrainSession&) = delete;
  InProcessBrainSession(InProcessBrainSession&&) = delete;
  InProcessBrainSession& operator=(InProcessBrainSession&&) = delete;

  // Spawns the engine thread (Runtime + arrangrr Stage + AlsaMidi). Returns
  // true once the thread is running. ALSA opening failure is NOT fatal here
  // (logged to stderr, the thread still runs silently) -- a dev/CI box with
  // no ALSA sequencer device can still exercise the ring/decode round trip.
  // Calling start() twice without an intervening stop() is a no-op (returns
  // true, does not spawn a second thread).
  bool start();

  // Signals the engine thread to stop and joins it. Safe to call when not
  // started (no-op) and safe to call from the destructor path (~ calls it).
  void stop();

  // Attaches gui_sonotron_audio::AudioEngine's producer-side ring handle
  // (Phase-6 Theme 2, Decision 1/2/3): once set, run_engine() pushes every
  // kMidi OutEvent on the primary integrated output port onto `ring` for
  // AudioEngine's ma_device callback to drain and realize. MUST be called
  // BEFORE start() -- the engine thread reads this pointer once at
  // thread-start time, not on every tick; the happens-before edge
  // std::thread's own constructor establishes between this call (on the
  // calling/GUI thread) and the new thread's first instruction is the only
  // synchronization this needs. `ring` may be null (the default -- never
  // set in --control mode), in which case OutEvents are simply never
  // realized locally, same as today.
  void set_audio_ring(AudioMidiRing* ring);

  // BrainSession
  void send(std::string_view command_line) override;
  void poll(std::vector<BrainEvent>& out) override;
  const BrainSnapshot& snapshot() const override;
  Status status() const override;

 private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace sonotron
