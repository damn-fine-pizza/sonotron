// libFuzzer harness for arrstyle::parse_smf (components/midisrc/src/smf.cpp).
//
// This targets the exact entry point and trust boundary that
// `midisrc::MidiSourceStage::load()` (include/midisrc/midi_source_stage.hpp)
// uses in production: `read_binary_file()` (src/file_io.cpp) reads a file's
// bytes verbatim, with ZERO validation, and hands them straight to
// `parse_smf`. The `midi-source load` L1 verb (phase5-execution-plan.md Item
// G / node 9310) is what makes this parser reachable from an arbitrary
// user-supplied file, so this harness feeds fuzz bytes into `parse_smf`
// exactly as that call site does -- same bytes, same signature, same
// Diagnostics collaborator.
//
// Host-only, dev/CI tooling: gated behind `option(SONOTRON_FUZZ ...)` in
// components/midisrc/CMakeLists.txt, OFF by default. Uses ONLY clang's
// built-in libFuzzer + AddressSanitizer + UndefinedBehaviorSanitizer -- no new
// dependency, never linked into the arm-none-eabi cross build, never part of
// the default `host`/`ci.sh` target set.
//
// Build + run:
//   cmake --preset host -DCMAKE_CXX_COMPILER=clang++ -DSONOTRON_FUZZ=ON
//   cmake --build build/host --target midisrc_fuzz_smf
//   ./build/host/components/midisrc/midisrc_fuzz_smf \
//       -max_len=4096 -runs=100000 components/midisrc/fuzz/seeds

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "midisrc/diagnostics.hpp"
#include "midisrc/smf.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::vector<std::uint8_t> bytes(data, data + size);
  arrstyle::Diagnostics diag;
  arrstyle::SmfFile file;
  // The return value is intentionally ignored: parse_smf must never
  // crash/UB on EITHER outcome (accepted or rejected-with-diagnostic) for ANY
  // byte sequence -- that is exactly DESIGN.md Sec.9.A's "robustness:
  // corrupted bytes... discarded without crashing" claim, put under an
  // adversarial input generator instead of taken on faith.
  (void)arrstyle::parse_smf(bytes, "fuzz", file, diag);
  return 0;
}
