// Bare firmware stub: the M0 gate is that the core cross-compiles and links
// freestanding for the STM32 anchor (D31/D33). Real board bring-up is M13.

#include "arrangrr/common/time.hpp"
#include "arrangrr/version.hpp"

int main() {
  arrangrr::TickAccumulator acc;
  acc.set_bpm(arrangrr::kDefaultBpm);
  // Reference the library so the linker resolves core symbols for real.
  return (arrangrr::version_string() != nullptr && acc.advance_us(1'000'000) == 1920) ? 0 : 1;
}
