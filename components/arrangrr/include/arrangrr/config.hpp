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

}  // namespace arrangrr
