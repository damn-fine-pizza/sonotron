#pragma once

#include <cstddef>
#include <cstdint>

// Bounded-capacity configuration, anchored to the STM32H743 512 KB pool
// envelope (D33). Every MAX here is a compile-time constant; static_asserts
// keep the totals honest.

namespace arrangrr {

inline constexpr std::size_t kMaxPorts = 4;              // DIN in/out + USB in/out (D6)
inline constexpr std::size_t kMaxRoutes = 32;            // routing matrix entries
inline constexpr std::size_t kSchedulerCapacity = 4096;  // out-queue entries (D33)
inline constexpr std::size_t kMaxTracks = 16;            // timeline tracks
inline constexpr std::size_t kMaxStepsPerTrack = 64;     // write-gesture grid slots
inline constexpr std::size_t kMaxChordSequences = 16;    // D33: 16 x 128 x 12 B = 24 KB
inline constexpr std::size_t kMaxChordSteps = 128;       // free-duration steps per sequence

// Repeat-Zone grid pool (Phase-5 Item #2, docs/design/clip-primitive-design.md):
// one ClipMatrix slot per launchable grid cell. Headroom over the GUI's own
// kRoleCount(10) x GridModel::kMaxSceneCount(8) = 80 cells
// (apps/gui-sonotron/src/grid_model.hpp).
inline constexpr std::size_t kMaxClips = 96;
// Budget (D33): Clip is a tiny 12-byte POD (TrackRole + scene_index +
// ContentKind + content_index + LaunchState + n_bars + the Phase 7 node T0
// frozen quantize window) -- pinned by static_assert(sizeof(Clip) == 12) in
// arrangrr/clip/clip_matrix.hpp, so even generous headroom stays a trivial
// slice of the STM32H743 512 KB envelope (kMaxClips x 12 B ~= 1.1 KB well
// below kSchedulerCapacity's own 64 KB pool).
// Capped here so a future caller cannot silently balloon the grid past a sane
// size.
static_assert(kMaxClips <= 256, "ClipMatrix pool: keep the Repeat-Zone grid bounded (D33)");

// Pad banks (Phase-5 Item #9, docs/phase5-design-reviews.md "Pad/Scene live ->
// Performance"): 8 banks x 4 pads (Yamaha/Korg's own "4 pads per bank"
// convention) = 32 flat pad slots, addressed by a flat 0..kMaxPads-1 id on
// the ABI (kPadAssign/kPadTrigger/kPadRelease). Budget (D33): Pad is a
// 12-byte POD (pinned by static_assert(sizeof(Pad) == 12) in
// arrangrr/pad/pad_bank.hpp), so kMaxPads x 12 B stays a trivial slice of the
// STM32H743 512 KB envelope.
inline constexpr std::size_t kMaxPadsPerBank = 4;
inline constexpr std::size_t kMaxPadBanks = 8;
inline constexpr std::size_t kMaxPads = kMaxPadsPerBank * kMaxPadBanks;
static_assert(kMaxPads == kMaxPadsPerBank * kMaxPadBanks);
static_assert(kMaxPads <= 256, "Pad grid: keep the pad-bank pool bounded (D33)");

// Performance store (Phase-5 Item #9): one-button full-state recall slots
// (DESIGN.md section 17, node 8200). Budget (D33): Performance is a 576-byte
// POD as of format_version 2's FX-chain snapshot (Phase-6 Theme 3 Item #3,
// pinned by static_assert(sizeof(Performance) == 576) in
// arrangrr/perf/performance.hpp), so kMaxPerformances x 576 B (~9 KB) stays a
// trivial slice of the STM32H743 512 KB envelope.
inline constexpr std::size_t kMaxPerformances = 16;
static_assert(kMaxPerformances <= 256,
              "Performance store: keep the recall-slot pool bounded (D33)");

// MIDI-FX insert chain (Phase-5 Item #10, node 5100): the bounded fan-out
// buffer one note produces as it flows through a role's InsertChain
// (arrangrr/fx/insert_chain.hpp) -- an Echo/NoteRepeat insert can multiply
// one input note into several output notes; kMaxChainFan caps how many a
// SINGLE insert stage (and the chain's own scratch buffers) may ever hold at
// once. Budget note: a dense chain multiplies one note's 2 scheduler entries
// (note-on + note-off) up to kMaxChainFan x 2 against kSchedulerCapacity =
// 4096 -- 8 is generous headroom (no authored Echo/NoteRepeat preset needs
// more than a handful of repeats) while keeping that worst case a trivial
// slice of the scheduler pool.
inline constexpr int kMaxChainFan = 8;
static_assert(kMaxChainFan >= 1 && kMaxChainFan <= 32,
              "InsertChain fan-out: keep one note's chain multiplication bounded (D33)");

// Retrigger-care PRODUCER tags (Torquato QA heavy pass, Phase-6 Theme 4
// dual-arp collision fix): runtime::OutScheduler::cancel_note_off is scoped
// to (port, channel, note, source) so a pending note-off is only ever
// cancelled by a NEW note-on from the SAME producer -- two INDEPENDENT
// producers sharing a (port, channel, note) triple at the same tick (e.g.
// the Engine-global live-keyboard arpeggiator and a role's own kArp insert)
// must never tombstone each other's legitimate note-off. `source` is a plain
// runtime scheduler-bookkeeping byte -- it never rides the wire (OutEvent
// carries the resulting MidiMessage only, never this tag) and never touches
// ScheduledEvent's own persistence (it has none: OutScheduler is an
// in-process priority queue, not an ABI/wire shape).
//   kScheduleSourceCore     -- Timeline + ordinary (non-arp-insert) Arranger
//                              scheduling: the historical, UNDIFFERENTIATED
//                              shared pool every existing golden already
//                              exercises. No producer distinction existed at
//                              all before this fix, so lumping these
//                              together reproduces the OLD single-pool
//                              behavior bit-for-bit -- zero behavior change
//                              for any style/track that never configures an
//                              arp-insert (i.e. every current golden).
//   kScheduleSourceLiveArp  -- Engine::fire_arp, the global live-keyboard
//                              arpeggiator (Engine::m_arp).
//   kScheduleSourceRoleArpBase -- BASE id for a role's own arp-insert
//                              (Arranger's P5 on_tick pass): the actual tag
//                              is kScheduleSourceRoleArpBase + the TrackRole
//                              index (0..9), so two DIFFERENT roles' own
//                              arp-inserts never cancel each other's
//                              note-offs either.
inline constexpr std::uint8_t kScheduleSourceCore = 0;
inline constexpr std::uint8_t kScheduleSourceLiveArp = 1;
inline constexpr std::uint8_t kScheduleSourceRoleArpBase = 2;

// LoopBuffer (Phase 7, node 6000, "the capture gesture" -- docs/reflections/
// phase7-scope-6000-8100-clip-timeline-seam.md, Fork A/E/F resolved by the
// owner for this slice): a bounded, static (no-heap on EITHER target, D32)
// pool of captured, chord-tone-relative note loops, the note-level peer of
// ChordSequencer (arrangrr/loop/loop_buffer.hpp).
//
// TARGET-CONDITIONAL budget (owner directive): the arm-none-eabi firmware
// target keeps the tight DESIGN.md `6000` envelope verbatim -- 8 slots x 3072
// events -- even though LoopEvent (12 B, matching ChordStep's own 12 B
// precedent for a chord-relative payload) is larger than the ORIGINAL 8-B/
// event planning estimate that produced "192 KB" (docs/reflections/
// phase7-scope-6000-8100-clip-timeline-seam.md §1/§4 flagged this explicitly:
// the DESIGN.md byte figure was never `static_assert`-pinned, only the SLOT
// and EVENT counts were quoted as the reusable envelope). __arm__ is
// predefined by arm-none-eabi-gcc/g++ for the Cortex-M7 target (cmake/
// toolchains/arm-cortex-m7.cmake) and is NOT defined by the host's native
// (x86_64/aarch64) compiler, so this is a clean, existing-toolchain-native
// target switch, not a new build knob.
//
// The HOST build has no SRAM pressure, so it gets MORE slots (double arm's
// 8) for scripting/testing flexibility -- but NOT an unboundedly large
// per-slot event capacity: LoopBuffer is an Engine-owned VALUE member (m_loop
// in engine.hpp, mirroring m_seq/m_timeline's own placement), and Engine
// itself is routinely STACK-allocated, often SEVERAL AT ONCE in one test
// function (e.g. test_step_locks.cpp declares three `Player`-wrapped Engines
// in a single scope). A naive "generous" host figure (e.g. 32 slots x 8192
// events, an earlier draft of this constant) balloons a single Engine by
// several MB and reliably blows the default 8 MB thread stack the moment two
// or three are alive at once -- reproduced as a genuine SIGSEGV in
// test_step_locks/test_master_transpose before this figure was corrected.
// 16 slots x 512 events keeps LoopBuffer's own footprint at ~100 KB per
// Engine on host (safe for many simultaneous stack instances) while still
// giving host scripting/tests twice arm's slot count to work with.
#if defined(__arm__)
inline constexpr std::size_t kMaxLoopSlots = 8;      // DESIGN.md 6000: 8 loop slots
inline constexpr std::size_t kMaxLoopEvents = 3072;  // DESIGN.md 6000: 3072 events/slot
#else
inline constexpr std::size_t kMaxLoopSlots = 16;
inline constexpr std::size_t kMaxLoopEvents = 512;
#endif
static_assert(kMaxLoopSlots >= 1 && kMaxLoopSlots <= 256,
              "LoopBuffer pool: keep the loop-slot pool bounded (D33)");
static_assert(kMaxLoopEvents >= 1, "LoopBuffer pool: every slot needs room for at least one event");
// Budget note (Fork E, single-generation undo): the pool itself is
// kMaxLoopSlots x kMaxLoopEvents x 12 B (LoopEvent, pinned by
// static_assert(sizeof(LoopEvent) == 12) in arrangrr/loop/loop_event.hpp);
// undo adds exactly ONE extra slot's worth of shadow storage (a single
// shared LoopClip, not one shadow PER slot -- a live performer can only be
// actively recording/overdub/erasing ONE slot at a time, so shadowing every
// slot would waste (kMaxLoopSlots - 1)x the RAM for no reachable benefit).
// On arm-none-eabi: (8 + 1) x 3072 x 12 B ~= 324 KB, alongside the existing
// ~40 KB the rest of arrangrr's own bounded pools already commit (chord
// sequences 24 KB, timeline 8 KB, clips/pads/performances/scheduler the
// remainder) -- within the STM32H743 512 KB planning envelope (D33). On
// host: (16 + 1) x 512 x 12 B ~= 100 KB per Engine -- small enough that
// several stack-allocated Engines in one test scope stay well inside the
// default thread stack. The static_assert gate below is arm-only: the host
// figure above is a deliberate STACK-safety choice, not an SRAM-envelope
// constraint, so there is nothing meaningful to gate on host.
#if defined(__arm__)
static_assert((kMaxLoopSlots + 1) * kMaxLoopEvents * 12ull <= 512ull * 1024ull,
              "LoopBuffer pool (content + one undo shadow): keep it inside the D33 SRAM envelope");
#endif

}  // namespace arrangrr
