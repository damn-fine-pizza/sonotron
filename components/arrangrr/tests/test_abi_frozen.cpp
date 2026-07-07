// Frozen-ABI guard (node 11720). This test is the ENFORCEMENT mechanism for the
// additive-only v1 ABI freeze declared in arrangrr/abi.hpp: it pins the exact
// numeric value of every command/event id and the exact size of the wire
// structs. It is almost entirely compile-time (static_assert), so it breaks the
// BUILD the instant someone renumbers, removes, reorders, re-semanticizes, or
// resizes an existing part of protocol v1.
//
// A FAILURE HERE MEANS THE FREEZE WAS VIOLATED. The fix is never to edit this
// test to match — the fix is to APPEND (a new enumerator at the end, a reserved
// bit, or an appended field guarded by the size asserts) and, only for a
// genuine BREAKING change, bump kProtocolVersion to 2 in version.hpp.

#include <cstddef>
#include <cstdint>

#include "arrangrr/abi.hpp"
#include "arrangrr/version.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;

// --- Op: every value pinned -------------------------------------------------
static_assert(static_cast<std::uint8_t>(Op::kSet) == 0);
static_assert(static_cast<std::uint8_t>(Op::kDo) == 1);
static_assert(static_cast<std::uint8_t>(Op::kGet) == 2);

// --- Param: every current enumerator pinned to its exact value --------------
// kNone(0) .. kInputZone(42). Next free id is 43 (additive-only).
static_assert(static_cast<std::uint16_t>(Param::kNone) == 0);
static_assert(static_cast<std::uint16_t>(Param::kTransportTempo) == 1);
static_assert(static_cast<std::uint16_t>(Param::kTransportStart) == 2);
static_assert(static_cast<std::uint16_t>(Param::kTransportStop) == 3);
static_assert(static_cast<std::uint16_t>(Param::kTransportContinue) == 4);
static_assert(static_cast<std::uint16_t>(Param::kPanic) == 5);
static_assert(static_cast<std::uint16_t>(Param::kRouteAdd) == 6);
static_assert(static_cast<std::uint16_t>(Param::kRouteClear) == 7);
static_assert(static_cast<std::uint16_t>(Param::kClockOutMask) == 8);
static_assert(static_cast<std::uint16_t>(Param::kTrackNew) == 9);
static_assert(static_cast<std::uint16_t>(Param::kTrackStep) == 10);
static_assert(static_cast<std::uint16_t>(Param::kTrackLength) == 11);
static_assert(static_cast<std::uint16_t>(Param::kTrackMute) == 12);
static_assert(static_cast<std::uint16_t>(Param::kTrackSolo) == 13);
static_assert(static_cast<std::uint16_t>(Param::kKeySet) == 14);
static_assert(static_cast<std::uint16_t>(Param::kChordPlay) == 15);
static_assert(static_cast<std::uint16_t>(Param::kChordStop) == 16);
static_assert(static_cast<std::uint16_t>(Param::kChordHold) == 17);
static_assert(static_cast<std::uint16_t>(Param::kChordOut) == 18);
static_assert(static_cast<std::uint16_t>(Param::kSeqNew) == 19);
static_assert(static_cast<std::uint16_t>(Param::kSeqUse) == 20);
static_assert(static_cast<std::uint16_t>(Param::kSeqRec) == 21);
static_assert(static_cast<std::uint16_t>(Param::kSeqAdd) == 22);
static_assert(static_cast<std::uint16_t>(Param::kSeqLoop) == 23);
static_assert(static_cast<std::uint16_t>(Param::kSeqPlay) == 24);
static_assert(static_cast<std::uint16_t>(Param::kSeqStop) == 25);
static_assert(static_cast<std::uint16_t>(Param::kSeqTranspose) == 26);
static_assert(static_cast<std::uint16_t>(Param::kSeqDel) == 27);
static_assert(static_cast<std::uint16_t>(Param::kSeqClear) == 28);
static_assert(static_cast<std::uint16_t>(Param::kChordMode) == 29);
static_assert(static_cast<std::uint16_t>(Param::kStyleLoad) == 30);
static_assert(static_cast<std::uint16_t>(Param::kStyleSection) == 31);
static_assert(static_cast<std::uint16_t>(Param::kStyleRoute) == 32);
static_assert(static_cast<std::uint16_t>(Param::kStyleSwitch) == 33);
static_assert(static_cast<std::uint16_t>(Param::kChordDetect) == 34);
static_assert(static_cast<std::uint16_t>(Param::kProgram) == 35);
static_assert(static_cast<std::uint16_t>(Param::kPartMute) == 36);
static_assert(static_cast<std::uint16_t>(Param::kPartSolo) == 37);
static_assert(static_cast<std::uint16_t>(Param::kGroove) == 38);
static_assert(static_cast<std::uint16_t>(Param::kArp) == 39);
static_assert(static_cast<std::uint16_t>(Param::kArpOut) == 40);
static_assert(static_cast<std::uint16_t>(Param::kChordFollow) == 41);
static_assert(static_cast<std::uint16_t>(Param::kInputZone) == 42);

// --- OutEvent::Kind: every value pinned -------------------------------------
// kMidi(0) .. kSection(4). Next free id is 5 (additive-only).
static_assert(static_cast<std::uint8_t>(OutEvent::Kind::kMidi) == 0);
static_assert(static_cast<std::uint8_t>(OutEvent::Kind::kTransport) == 1);
static_assert(static_cast<std::uint8_t>(OutEvent::Kind::kWarn) == 2);
static_assert(static_cast<std::uint8_t>(OutEvent::Kind::kChord) == 3);
static_assert(static_cast<std::uint8_t>(OutEvent::Kind::kSection) == 4);

// --- WarnCode: every value pinned, plus the count ---------------------------
// kNone(0) .. kUnsupported(9), kWarnCodeCount == 10 (next free id).
static_assert(static_cast<std::uint16_t>(WarnCode::kNone) == 0);
static_assert(static_cast<std::uint16_t>(WarnCode::kSchedulerFull) == 1);
static_assert(static_cast<std::uint16_t>(WarnCode::kRouteTableFull) == 2);
static_assert(static_cast<std::uint16_t>(WarnCode::kUnknownCommand) == 3);
static_assert(static_cast<std::uint16_t>(WarnCode::kBadArgument) == 4);
static_assert(static_cast<std::uint16_t>(WarnCode::kTrackTableFull) == 5);
static_assert(static_cast<std::uint16_t>(WarnCode::kNotInKey) == 6);
static_assert(static_cast<std::uint16_t>(WarnCode::kSeqTableFull) == 7);
static_assert(static_cast<std::uint16_t>(WarnCode::kSeqEmpty) == 8);
static_assert(static_cast<std::uint16_t>(WarnCode::kUnsupported) == 9);
static_assert(kWarnCodeCount == 10);

// --- Wire struct sizes: pinned to the exact measured values -----------------
// Concrete numbers (fixed-width fields => identical on host and arm-none-eabi).
// The existing `<=` design ceilings are kept alongside as the looser guard.
static_assert(sizeof(Command) == 20);
static_assert(sizeof(Command) <= 20);
static_assert(sizeof(OutEvent) == 16);
static_assert(sizeof(OutEvent) <= 16);

// --- Protocol version: still v1 (freezing v1 keeps it v1) -------------------
static_assert(kProtocolVersion == 1);

// --- Reserved MIDI-FX shape (node 5100): the one committed symbol -----------
static_assert(kMaxInserts == 8);

}  // namespace

int main() {
  // All pins above are compile-time; reaching here means the build honored the
  // frozen v1 ABI. A runtime CHECK keeps this a real, registered ctest target.
  CHECK(kProtocolVersion == 1);
  if (arrangrr::test::failures() == 0) {
    std::printf("test_abi_frozen: all OK (frozen v1 ABI intact)\n");
  }
  return arrangrr::test::failures();
}
