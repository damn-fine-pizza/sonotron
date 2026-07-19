// Anti-NO-OP proof for Phase-1 sound task #7 deliverable B: proves the
// vendored stb_vorbis (third_party/stb_vorbis) actually COMPILES and LINKS
// into a real binary via the exact CMake wiring TSF's .sf3 decode path
// depends on (third_party/tinysoundfont/tsf_impl.c enables
// STB_VORBIS_INCLUDE_STB_VORBIS_H the same way, see that file). This test
// does NOT decode real Ogg Vorbis audio (no .sf3/.ogg test fixture exists
// in this sandbox -- flagged in the task report, no sf2-to-sf3 encoder
// tooling was available to build one); it proves the weaker but still
// load-bearing claim that "the symbols resolve and the decoder's own
// well-defined failure path runs cleanly", which is exactly what would be
// silently broken by a CMakeLists mistake that dropped `stb_vorbis` off the
// link line -- the actual regression class this test exists to catch.
//
// Declarations-only include: STB_VORBIS_HEADER_ONLY must be defined before
// pulling in stb_vorbis.c here, otherwise this translation unit would
// recompile the FULL implementation a second time (stb_vorbis_impl.c
// already does that once, into the `stb_vorbis` static library this test
// links against) and the linker would reject the duplicate symbol
// definitions -- see third_party/stb_vorbis/ARRGRR_VENDOR.md.
#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>
#undef STB_VORBIS_HEADER_ONLY

#include <cstddef>

#include "test.hpp"

int main() {
  // Deliberately not a valid Ogg stream: proves stb_vorbis_open_memory()'s
  // own well-defined failure path (return nullptr, no crash) runs -- the
  // real, valid-audio decode path needs a genuine .sf3/.ogg fixture this
  // sandbox does not have (see the task report).
  const unsigned char garbage[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                     0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
  int error = 0;
  stb_vorbis* decoder =
      stb_vorbis_open_memory(garbage, static_cast<int>(sizeof(garbage)), &error, nullptr);
  CHECK(decoder == nullptr);
  CHECK(error != 0);
  if (decoder != nullptr) {
    stb_vorbis_close(decoder);
  }

  if (melodd::test::failures() == 0) {
    std::printf(
        "OK: stb_vorbis symbols link and its own failure path runs cleanly "
        "(no real-audio fixture available in this sandbox -- see the task report)\n");
  }
  return melodd::test::failures();
}
