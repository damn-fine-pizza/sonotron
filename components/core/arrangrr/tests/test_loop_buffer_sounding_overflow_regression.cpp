// Torquato QA regression guard (Phase 7, node 6000 hardening pass, SLICE 1):
// LoopBuffer::on_tick's PLAYBACK-side polyphony bookkeeping
// (LoopBufferPlayState::sounding[kMaxLoopHeldNotes]) is bounded at
// kMaxLoopHeldNotes (16) concurrently-sounding voices per slot -- the SAME
// budget the RECORDING-side table (m_held) already documents. Giotto's
// fix gates the note-ON itself on store_sounding() actually claiming a
// tracking slot (on_tick's own header comment: "Only sound the note-ON if
// the sounding table can actually track its release"): past the 16th
// concurrently-sounding voice from ONE slot's own content, the excess voice
// is silently CAPPED -- it never sounds at all -- rather than sounding and
// then having no release path (the orphaned-note-on defect this file
// originally pinned, now fixed in loop_buffer.hpp).
//
// Locked contract this test guards:
//   - per-slot playback polyphony is bounded at kMaxLoopHeldNotes;
//   - an excess simultaneous voice beyond that bound is DROPPED (never
//     fires its note-on) -- capacity is enforced at the SOUND boundary, not
//     merely at the bookkeeping boundary;
//   - every voice that DID sound gets its own note-off -- ZERO orphans,
//     ever (this is the real invariant the earlier RED pinned a violation
//     of; it is the one this GREEN test now protects against regressing).
//
// Minimized reproducer (unchanged shape from the original RED pin): one
// NON-LOOPING slot (clip->loop = false, so the natural-end path drives
// release_all_sounding() -- the cleanest, most direct consumer of the
// sounding table), kMaxLoopHeldNotes + 1 (17) events, all starting at tick 0
// with the SAME duration so they would all reach their natural end on the
// SAME tick -- LoopBuffer alone, no Engine needed (the contract lives
// entirely inside LoopBuffer's own per-slot sounding-table bookkeeping) --
// unit-level regression pin.
//
// GREEN today (Giotto's fix is in the tree): exactly kMaxLoopHeldNotes
// (16) note-ons fire at tick 0 -- the 17th voice is capped, never sounds --
// and exactly that many note-offs fire at the clip's natural end: every
// voice that sounded is released, none orphaned. Kept in its own file/CTest
// entry (mirrors test_performance_style_id_regression.cpp's own isolation
// precedent) so a future re-break of this specific guard (e.g. reverting to
// firing the note-on unconditionally) is caught in isolation, without
// masking the surrounding GREEN test_loop.cpp/test_loop_event.cpp coverage.

#include "arrangrr/loop/loop_buffer.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;

struct FiredEvent {
  std::uint8_t note = 0;
  bool on = false;
};

void test_playback_sounding_table_caps_excess_voices_no_orphaned_note_on() {
  LoopBuffer buf;
  const int idx = buf.add_slot();
  CHECK(idx == 0);
  LoopClip* clip = buf.get(static_cast<std::size_t>(idx));
  CHECK(clip != nullptr);
  clip->loop = false;  // one-shot: the natural-end path drives
                       // release_all_sounding() directly (see file header).

  // kMaxLoopHeldNotes + 1 (17) events, all starting at tick 0, all lasting
  // exactly 100 ticks -- distinct kInterval tones (0..16) so each resolves to
  // a distinct, unambiguous absolute MIDI note (no chord/key needed: anchor
  // is key.root_pc == 0 with no chord sounding, so resolve_note(ev) == ev.tone
  // directly). Still generates MORE than kMaxLoopHeldNotes concurrent voices
  // from this ONE slot, so the cap is genuinely exercised.
  constexpr int kEventCount = static_cast<int>(kMaxLoopHeldNotes) + 1;
  for (int i = 0; i < kEventCount; ++i) {
    const LoopEvent ev{.start = 0,
                       .duration = 100,
                       .tone = static_cast<std::int8_t>(i),
                       .octave = 0,
                       .velocity = 100,
                       .source = LoopNoteSource::kInterval};
    CHECK(clip->record(ev));
  }
  CHECK(clip->count() == static_cast<std::size_t>(kEventCount));
  CHECK(clip->length() == 100);  // content_length: furthest end (0 + 100)

  CHECK(buf.start_playback(0, /*transport_tick=*/0));

  const ChordState no_chord{};  // valid == false: kInterval anchors on key root
  const Key key{.root_pc = 0, .mode = Mode::kMajor};

  StaticVector<FiredEvent, 64> fired;
  auto fire = [&](std::uint8_t note, std::uint8_t /*velocity*/, bool on) {
    CHECK(fired.push_back(FiredEvent{note, on}));
  };

  // Tick 0: all 17 events start here, but the sounding table only has room
  // for kMaxLoopHeldNotes (16) -- the 17th voice's store_sounding() call
  // fails to claim a slot, so on_tick's own gate (loop_buffer.hpp) never
  // fires its note-on at all. Exactly 16 note-ons sound, not 17.
  (void)buf.on_tick(0, /*transport_tick=*/0, no_chord, key, fire);
  int ons = 0;
  for (const FiredEvent& f : fired) {
    if (f.on) {
      ++ons;
    }
  }
  CHECK(ons == static_cast<int>(kMaxLoopHeldNotes));  // the excess voice is capped, never sounds

  // Tick 100: the clip's own natural end (raw_pos == length, non-looping) --
  // on_tick's early-return branch calls release_all_sounding(), which
  // releases every voice that IS tracked -- exactly the ones that sounded.
  fired.clear();
  const LoopBuffer::TickResult r = buf.on_tick(0, /*transport_tick=*/100, no_chord, key, fire);
  CHECK(r.ended);
  int offs = 0;
  for (const FiredEvent& f : fired) {
    CHECK(!f.on);  // release_all_sounding only ever fires note-offs
    ++offs;
  }
  // THE INVARIANT: every voice that sounded (ons) gets released (offs) --
  // zero orphans, by construction of the fix (a note-on that never fired
  // has nothing left to leak; a note-on that DID fire always claimed a
  // sounding-table slot first, so release_all_sounding will always find it).
  CHECK(offs == ons);
}

}  // namespace

int main() {
  test_playback_sounding_table_caps_excess_voices_no_orphaned_note_on();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_loop_buffer_sounding_overflow_regression: all OK\n");
  }
  return arrangrr::test::failures();
}
