// Compile-only proof (docs/proposals/isoundengine-contract.md §4, Corelli):
// a trivial ISoundEngine implementation, compiled on the arm-none-eabi
// toolchain, turns "components/core/audio_engine is dual-target-ready" into
// a CI-verified fact rather than an aspirational claim about a header
// nobody has tried to compile freestanding. This TU deliberately does NOT
// need to link into a running firmware image -- it is built as its own
// OBJECT library (see this directory's CMakeLists.txt) so it costs nothing
// at runtime and adds exactly one compiled TU to the cross build.

#include "audio_engine/i_sound_engine.hpp"

namespace sonotron::audio_engine {

class NullSoundEngine : public ISoundEngine {
 public:
  void dispatch(const arrangrr::MidiMessage& /*msg*/) noexcept override {}
  void all_notes_off() noexcept override {}
  void render(float* out, int frame_count) noexcept override {
    for (int i = 0; i < frame_count * 2; ++i) {
      out[i] = 0.0F;
    }
  }
  const char* name() const noexcept override { return "null"; }
};

// Referenced nowhere else on purpose: forces the compiler to fully
// instantiate NullSoundEngine (vtable + all four overrides), not just parse
// the class body, exercising every ISoundEngine signature (const
// arrangrr::MidiMessage&, float*, int, const char*) through the real
// arm-none-eabi toolchain.
bool null_sound_engine_compile_gate() {
  static NullSoundEngine engine;
  float buffer[4] = {0.0F, 0.0F, 0.0F, 0.0F};
  engine.dispatch(arrangrr::MidiMessage::note_on(0, 60, 100));
  engine.render(buffer, 2);
  engine.all_notes_off();
  return engine.name() != nullptr;
}

}  // namespace sonotron::audio_engine
