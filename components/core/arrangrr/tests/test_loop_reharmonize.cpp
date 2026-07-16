// Torquato QA hardening pass (Phase 7, node 6000 SLICE 1): THE key property
// of the Looper's whole storage shape (Fork A, loop_event.hpp's own header
// comment) proven end to end THROUGH THE REAL ABI -- a loop recorded while
// one chord is followed and LAUNCHED after the followed chord has since
// changed re-harmonizes automatically: decompose_note() at capture time and
// resolve_note() at playback time are two INDEPENDENT calls against whatever
// chord/key context is CURRENT at each moment, never a stored absolute pitch.
// test_loop_event.cpp already proves this at the pure-function level
// (test_resolve_reharmonizes_on_chord_change); this file proves the SAME
// property survives the full record -> stop -> [context changes] -> launch
// round trip through Engine's cmd_loop/apply_clip_content_loop_buffer/
// fire_loop wiring, covering all three LoopNoteSource branches (kChordTone,
// kScaleDegree, kInterval) -- build on test_loop.cpp's own
// test_loop_record_stop_and_launch_round_trip fixture shape, not a
// duplicate: that test never changes the context between record and launch.

#include "arrangrr/loop/loop_buffer.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "test.hpp"
#include "test_harness.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 1024>;

struct Band {
  test::TestEngine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0,
           std::uint16_t idx = 0, Op op = Op::kDo, Boundary boundary = Boundary::kImmediate) {
    Command command;
    command.op = op;
    command.boundary = boundary;
    command.param = p;
    command.idx = idx;
    command.n_bars = 1;
    command.a = a;
    command.b = b;
    command.c = c;
    e.push_command(command, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void advance(std::uint32_t n) {
    e.advance_ticks(n, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void feed_note(std::uint8_t port, std::uint8_t note, std::uint8_t vel) {
    const std::uint8_t b[3] = {0x90, note, vel};
    e.push_midi_in(port, Span<const std::uint8_t>(b, 3),
                   [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  // idx = kNoExplicitClipId: the legacy sequential-append form (Repeat-Zone
  // binding contract Shape A, abi.hpp's kClipAdd comment) -- explicit here
  // since Command::idx now means "explicit clip id" for kClipAdd.
  void add_clip(TrackRole role, std::uint8_t scene, ContentKind kind, std::uint16_t content_index) {
    cmd(Param::kClipAdd, static_cast<std::int32_t>(role), scene,
        static_cast<std::int32_t>(kind) | (static_cast<std::int32_t>(content_index) << 8),
        kNoExplicitClipId);
  }
  int warns() const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kWarn) {
        ++n;
      }
    }
    return n;
  }
  bool note_seen(std::uint8_t port, std::uint8_t channel, std::uint8_t note, bool on) const {
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kMidi && o.port == port && o.msg.channel() == channel &&
          o.msg.d1 == note && (o.msg.type() == midi::kNoteOn) == on) {
        return true;
      }
    }
    return false;
  }
};

// kChordTone: record a chord-relative note under chord A (C major triad, an
// explicit quality so it is IDENTICAL across the context change -- only the
// root moves), change the followed chord to a DIFFERENT root before launch
// (G major, same quality), and confirm the loop sounds G's own tone, not a
// literal replay of the note captured under C.
void test_loop_reharmonizes_chord_tone_on_chord_change_before_launch() {
  Band b;
  b.cmd(Param::kKeySet, 0, 0, 0, 0, Op::kSet);  // C major key
  // Chord A: explicit kMaj (quality 0) on root C (note 60, root_pc 0) -- NOT
  // the diatonic-auto overload, so quality stays IDENTICAL for chord B below.
  b.cmd(Param::kChordPlay, 60, 0, 100);
  b.ev.clear();
  b.cmd(Param::kLoopNew);
  b.add_clip(TrackRole::kBass, 0, ContentKind::kLoopBuffer, 0);
  b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 1 | (2 << 8));

  b.cmd(Param::kLoopRecordStart, static_cast<std::int32_t>(LoopRecordMode::kRecord), 3, 0,
        /*idx=*/0);
  // E (64): the third of C major (offsets {0,4,7}, chord-tone index 1).
  b.feed_note(3, 64, 100);
  b.advance(kTicksPerBar);
  b.feed_note(3, 64, 0);
  b.cmd(Param::kLoopRecordStop, /*grid=*/0, 0, 0, /*idx=*/0);
  const LoopClip* clip = b.e.loops().get(0);
  CHECK(clip != nullptr && clip->count() == 1);
  CHECK(clip->event(0).source == LoopNoteSource::kChordTone);
  CHECK(clip->event(0).tone == 1);  // the third, tone index 1

  // Chord B: SAME quality (kMaj), DIFFERENT root -- G (note 67, root_pc 7).
  // This is the whole point: re-harmonize with no re-capture.
  b.cmd(Param::kChordPlay, 67, 0, 100);

  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.cmd(Param::kClipLaunch, 0, 0, 0, /*idx=*/0);
  b.advance(kTicksPerBar / 2);  // well before the wrap: only the tick-0 onset matters here
  // B (71): the third of G major (7 + 4). NOT E (64) -- the captured note
  // never sounds literally once the chord has moved.
  CHECK(b.note_seen(1, 2, 71, /*on=*/true));
  CHECK(!b.note_seen(1, 2, 64, /*on=*/true));
  CHECK(b.warns() == 0);
}

// kScaleDegree: captured with NO chord sounding (pure key-diatonic), so only
// a KEY change (not a chord change) can re-harmonize it -- resolve_note's
// scale-degree branch reads key.root_pc/key.mode only, ignoring chord
// entirely (loop_event.hpp).
void test_loop_reharmonizes_scale_degree_on_key_change_before_launch() {
  Band b;
  b.cmd(Param::kKeySet, 0, 0, 0, 0, Op::kSet);  // C major key, no chord ever played
  b.cmd(Param::kLoopNew);
  b.add_clip(TrackRole::kBass, 0, ContentKind::kLoopBuffer, 0);
  b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 1 | (2 << 8));

  b.cmd(Param::kLoopRecordStart, static_cast<std::int32_t>(LoopRecordMode::kRecord), 3, 0,
        /*idx=*/0);
  // D (62): diatonic degree 1 in C major, no chord sounding -> scale-degree
  // fallback (test_loop_event.cpp's own test_decompose_resolve_scale_degree_
  // fallback pins the same math at the pure-function level).
  b.feed_note(3, 62, 100);
  b.advance(kTicksPerBar);
  b.feed_note(3, 62, 0);
  b.cmd(Param::kLoopRecordStop, 0, 0, 0, /*idx=*/0);
  const LoopClip* clip = b.e.loops().get(0);
  CHECK(clip != nullptr && clip->count() == 1);
  CHECK(clip->event(0).source == LoopNoteSource::kScaleDegree);
  CHECK(clip->event(0).tone == 1);  // degree 1 (D)

  // Move the KEY up a whole step to D major (root_pc 2) before launch --
  // no chord ever sounds, so this is the ONLY context change that can move a
  // kScaleDegree event.
  b.cmd(Param::kKeySet, 2, 0, 0, 0, Op::kSet);

  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.cmd(Param::kClipLaunch, 0, 0, 0, /*idx=*/0);
  b.advance(kTicksPerBar / 2);
  // Degree 1 of D major (root_pc 2) is E (64): 2 + 2 (major scale degree-1
  // semitone offset) = 64. NOT D (62) -- the original absolute pitch.
  CHECK(b.note_seen(1, 2, 64, /*on=*/true));
  CHECK(!b.note_seen(1, 2, 62, /*on=*/true));
  CHECK(b.warns() == 0);
}

// kInterval: a chromatic passing tone captured with NO chord sounding
// anchors on the KEY root (decompose_note's total fallback); moving the key
// root shifts the resolved pitch by the SAME delta, automatically.
void test_loop_reharmonizes_interval_fallback_on_key_change_before_launch() {
  Band b;
  b.cmd(Param::kKeySet, 0, 0, 0, 0, Op::kSet);  // C major key, no chord
  b.cmd(Param::kLoopNew);
  b.add_clip(TrackRole::kBass, 0, ContentKind::kLoopBuffer, 0);
  b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 1 | (2 << 8));

  b.cmd(Param::kLoopRecordStart, static_cast<std::int32_t>(LoopRecordMode::kRecord), 3, 0,
        /*idx=*/0);
  // C# (61): chromatic to C major, no chord sounding -> interval fallback,
  // anchored on the key root (0): tone = 61.
  b.feed_note(3, 61, 100);
  b.advance(kTicksPerBar);
  b.feed_note(3, 61, 0);
  b.cmd(Param::kLoopRecordStop, 0, 0, 0, /*idx=*/0);
  const LoopClip* clip = b.e.loops().get(0);
  CHECK(clip != nullptr && clip->count() == 1);
  CHECK(clip->event(0).source == LoopNoteSource::kInterval);
  CHECK(clip->event(0).tone == 61);

  // Move the key root up a whole step (D, root_pc 2) before launch.
  b.cmd(Param::kKeySet, 2, 0, 0, 0, Op::kSet);

  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.cmd(Param::kClipLaunch, 0, 0, 0, /*idx=*/0);
  b.advance(kTicksPerBar / 2);
  // anchor(2) + tone(61) = 63 -- the SAME relative interval, now measured
  // from the NEW key root. NOT 61 (the original absolute pitch).
  CHECK(b.note_seen(1, 2, 63, /*on=*/true));
  CHECK(!b.note_seen(1, 2, 61, /*on=*/true));
  CHECK(b.warns() == 0);
}

}  // namespace

int main() {
  test_loop_reharmonizes_chord_tone_on_chord_change_before_launch();
  test_loop_reharmonizes_scale_degree_on_key_change_before_launch();
  test_loop_reharmonizes_interval_fallback_on_key_change_before_launch();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_loop_reharmonize: all OK\n");
  }
  return arrangrr::test::failures();
}
