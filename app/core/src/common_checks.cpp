// Compile-time gate: every common header must compile (with its
// static_asserts) on BOTH toolchains. This TU is part of the core library, so
// the arm cross-build exercises the full freestanding subset (D3).

#include "arrangrr/common/assert.hpp"
#include "arrangrr/common/crc.hpp"
#include "arrangrr/common/result.hpp"
#include "arrangrr/common/ring_buffer.hpp"
#include "arrangrr/common/span.hpp"
#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/common/time.hpp"

namespace arrangrr {
namespace {

// Container invariants hold at compile time (D32: compile-time-first).
consteval bool common_selftest() {
  StaticVector<int, 4> v;
  if (!v.push_back(1) || !v.push_back(2) || !v.push_back(3) || !v.push_back(4)) return false;
  if (v.push_back(5)) return false;  // full -> must refuse
  v.erase(1);                        // {1, 3, 4}
  if (v.size() != 3 || v[1] != 3) return false;

  Span<int> s = v.span();
  if (s.size() != 3 || s.subspan(1).size() != 2) return false;

  auto r = Result<int, int>::ok(7);
  if (!r || r.value() != 7) return false;

  TickAccumulator acc;
  acc.set_bpm(12000);
  // 1 second @120 BPM = 2 beats = 1920 ticks, exactly.
  if (acc.advance_us(1'000'000) != 1920) return false;
  return true;
}
static_assert(common_selftest());

// Event budget maths from D33 must stay honest once Event lands; placeholder
// asserts pin the constants the design derives budgets from.
static_assert(kPpqn == 960 && kMidiClockDivider == 40 && kTicksPerGridStep == 10);

}  // namespace
}  // namespace arrangrr
