// ABI pin -- RE-BASELINED for Phase 5 (docs/design/phase5-execution-plan.md,
// docs/design/clip-primitive-design.md). This test used to enforce an
// additive-only FREEZE (node 11720); the owner lifted that freeze for Phase 5
// (fork F3 RESOLVED), and Item #2 (the clip primitive) is the first reshape
// spent from that budget: it adds `Command::boundary`/`Command::n_bars` and
// consolidates `kChordPlay`'s old `idx != 0` overload and `kStyleSwitch`'s old
// `c != 0` overload onto the new shared `Boundary` field (both still ride the
// SAME struct sizes -- sizeof(Command) == 20, sizeof(OutEvent) == 16,
// unchanged, because the reshape reuses what used to be alignment padding).
//
// This file is now a DELIBERATE re-baseline of the current shape, not an
// append-only enforcement mechanism: it pins every id/size AS THEY STAND
// TODAY so a future accidental renumber/resize still fails loudly, but a
// deliberate Phase-5 reshape is expected to EDIT this file (update the pins),
// not just append to it -- see phase5-execution-plan.md's gate discipline
// ("no test_abi_frozen constraint on Phase-5 items; if the ABI is rewritten,
// update/retire that test deliberately"). A BREAKING change to a value that
// has already shipped to a released build still bumps kProtocolVersion to 2
// (version.hpp).

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

// --- Boundary (Phase-5 Item #2 reshape): every value pinned -----------------
static_assert(static_cast<std::uint8_t>(Boundary::kImmediate) == 0);
static_assert(static_cast<std::uint8_t>(Boundary::kNextBar) == 1);
static_assert(static_cast<std::uint8_t>(Boundary::kNextNBars) == 2);

// --- Param: every current enumerator pinned to its exact value --------------
// kNone(0) .. kPerformanceRecall(52). Next free id is 53.
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
static_assert(static_cast<std::uint16_t>(Param::kNoteRaw) == 43);
static_assert(static_cast<std::uint16_t>(Param::kClipAdd) == 44);
static_assert(static_cast<std::uint16_t>(Param::kClipLaunch) == 45);
static_assert(static_cast<std::uint16_t>(Param::kClipStop) == 46);
static_assert(static_cast<std::uint16_t>(Param::kSceneQuantize) == 47);
// Phase-5 Item #9 (docs/phase5-design-reviews.md "Pad/Scene live ->
// Performance"): pad banks + the Performance one-button recall.
static_assert(static_cast<std::uint16_t>(Param::kPadAssign) == 48);
static_assert(static_cast<std::uint16_t>(Param::kPadTrigger) == 49);
static_assert(static_cast<std::uint16_t>(Param::kPadRelease) == 50);
static_assert(static_cast<std::uint16_t>(Param::kPerformanceStore) == 51);
static_assert(static_cast<std::uint16_t>(Param::kPerformanceRecall) == 52);

// --- OutEvent::Kind: every value pinned -------------------------------------
// kMidi(0) .. kClip(8). Next free id is 9.
static_assert(static_cast<std::uint8_t>(OutEvent::Kind::kMidi) == 0);
static_assert(static_cast<std::uint8_t>(OutEvent::Kind::kTransport) == 1);
static_assert(static_cast<std::uint8_t>(OutEvent::Kind::kWarn) == 2);
static_assert(static_cast<std::uint8_t>(OutEvent::Kind::kChord) == 3);
static_assert(static_cast<std::uint8_t>(OutEvent::Kind::kSection) == 4);
static_assert(static_cast<std::uint8_t>(OutEvent::Kind::kChordFollowed) == 5);
static_assert(static_cast<std::uint8_t>(OutEvent::Kind::kBeat) == 6);
// Phase 3a (§17.3b): the ABI waiver stays unspent -- kParamState is a NEW
// appended enumerator, not a reshape. It rides the SAME 16-byte OutEvent
// layout (sizeof(OutEvent) == 16, pinned below, is untouched).
static_assert(static_cast<std::uint8_t>(OutEvent::Kind::kParamState) == 7);
// Phase-5 Item #2: kClip is the clip-primitive launch-state echo -- rides the
// SAME 16-byte OutEvent layout unchanged (no new field, no resize).
static_assert(static_cast<std::uint8_t>(OutEvent::Kind::kClip) == 8);

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
  // All pins above are compile-time; reaching here means the build matches
  // this re-baselined shape. A runtime CHECK keeps this a real, registered
  // ctest target.
  CHECK(kProtocolVersion == 1);
  if (arrangrr::test::failures() == 0) {
    std::printf("test_abi_frozen: all OK (v1 ABI matches the re-baselined shape)\n");
  }
  return arrangrr::test::failures();
}
