#pragma once

#include "common/midi/message.hpp"

// ISoundEngine (docs/proposals/isoundengine-contract.md §1, Corelli): the
// render/synthesis seam `0910`/D43 calls for -- "arrangrr ... and a future
// host-only audio engine are peer modules wired by an orchestrator,
// name-blind, talking only through the POD interface; audio never crosses
// the interface; the core stays audio-ignorant". Every method signature
// here is regime-neutral on purpose: no std::string, no std::function, no
// exceptions, no owned allocation -- POD in, POD out, caller-provided
// buffers only. This is what makes the INTERFACE itself core-capable NOW
// even though every concrete implementation today is host-only (soundfont,
// wrapping melodd::Synth, components/platform/audio).
//
// Deliberately EXCLUDED from this interface -- configuration/load.
// `load_soundfont(path, error)`-shaped operations do NOT belong here
// (Interface-Segregation, not an oversight): a soundfont engine loads a
// .sf2 file, a physical-models engine would load a DSP parameter table, an
// analog engine likely loads nothing at all. Concrete engines expose their
// own configuration API on their own concrete type; the composition root
// talks to the concrete type for configuration and to ISoundEngine& only
// for the render/dispatch/panic triad below.
//
// Virtual dispatch is not a freestanding blocker: -fno-rtti (inherited from
// components/core/common, same regime as arrangrr/runtime) disables
// dynamic_cast/typeid, not virtual functions or vtables -- a vtable costs
// flash bytes, not heap. This is precisely the "HAL boundary" DESIGN.md
// reserves `virtual` for.

namespace sonotron::audio_engine {

class ISoundEngine {
 public:
  virtual ~ISoundEngine() = default;

  // One MIDI channel-voice message. Same "not internally thread-safe"
  // contract melodd::Synth already documents: the caller (e.g.
  // AudioBackend) must serialize dispatch()/render()/all_notes_off()
  // itself.
  virtual void dispatch(const arrangrr::MidiMessage& msg) noexcept = 0;

  // Panic / shutdown: silence every voice on every channel immediately.
  // Kept as its own virtual (not synthesized from repeated CC-123 dispatch
  // calls) because the engine, not the caller, knows the cheapest way to
  // zero its own voice state.
  virtual void all_notes_off() noexcept = 0;

  // Renders `frame_count` interleaved stereo frames (2 floats/frame, -1..1)
  // into a CALLER-OWNED buffer. Implementations with nothing loaded/no
  // voices active must zero-fill, never leave `out` uninitialized.
  virtual void render(float* out, int frame_count) noexcept = 0;

  // Static-literal identification for diagnostics/UI only -- never on a hot
  // path, no ownership transfer, free on a core-capable implementation too.
  virtual const char* name() const noexcept = 0;
};

}  // namespace sonotron::audio_engine
