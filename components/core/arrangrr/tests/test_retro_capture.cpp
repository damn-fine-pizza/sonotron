// Functional smoke tests for node 6300 ("grab last N bars" -- retroactive
// capture): RetroCaptureRing itself is a thin, header-only ring (no dedicated
// unit test file -- its whole surface is exercised end to end here, through
// the real ABI, exactly like test_scene.cpp drives SceneChain). This is the
// MINIMAL happy-path pass the owner asked for; thorough functional/regression
// QA is Torquato's follow-up.

#include "arrangrr/loop/retro_capture.hpp"

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
           std::uint16_t idx = 0, Op op = Op::kDo, Boundary boundary = Boundary::kImmediate,
           std::uint8_t n_bars = 1) {
    Command command;
    command.op = op;
    command.boundary = boundary;
    command.param = p;
    command.idx = idx;
    command.n_bars = n_bars;
    command.a = a;
    command.b = b;
    command.c = c;
    e.push_command(command, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void advance(std::uint32_t n) {
    e.advance_ticks(n, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  // Raw NoteOn(vel>0)/NoteOff(vel==0) bytes on `port`, channel 0 -- mirrors
  // test_loop.cpp's own push_midi_in convention.
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
  // idx = kNoLoopExplicitId: the legacy sequential-append form (docs/
  // proposals/looper-in-gui-contract.md §7 item 5, the IDENTICAL fix already
  // shipped for kClipAdd/kNoExplicitClipId above) -- explicit here since
  // Command::idx now means "explicit loop-slot id" for kLoopNew, and every
  // bare `loop_new()` caller below relies on the ORIGINAL sequential-id
  // assignment.
  void loop_new() { cmd(Param::kLoopNew, 0, 0, 0, kNoLoopExplicitId); }
  int warns() const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kWarn) {
        ++n;
      }
    }
    return n;
  }
  int count_midi(std::uint8_t port, std::uint8_t channel, std::uint8_t note, bool on) const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kMidi && o.port == port && o.msg.channel() == channel &&
          o.msg.d1 == note && (o.msg.type() == midi::kNoteOn) == on) {
        ++n;
      }
    }
    return n;
  }
};

// (a) arm -> play notes across a couple of bars -> grab last N bars -> the
// slot holds a playable loop whose notes re-resolve against the CURRENT
// chord at launch time -- NOT the one live when the note was originally
// captured (the whole point of the chord-tone-relative storage shape, Fork
// A, loop_event.hpp). Both chord_play calls pin the SAME quality (kMaj7) so
// only the root moves; only the shape/tone index stays identical, keeping
// the expected resolved pitch predictable.
void test_retro_arm_play_grab_reharmonizes() {
  Band b;
  b.cmd(Param::kKeySet, 0, 0, 0, 0, Op::kSet);
  b.cmd(Param::kChordPlay, 60, static_cast<std::int32_t>(ChordQuality::kMaj7),
        100);    // C major -> Cmaj7, root_pc 0
  b.loop_new();  // the target slot the grab will fill

  b.cmd(Param::kRetroCaptureArm, /*a=*/3);  // capture live notes on port 3
  CHECK(b.e.retro_capture().armed());
  CHECK(b.warns() == 0);

  b.cmd(Param::kTransportStart);
  b.feed_note(3, 64, 100);  // E: chord-tone index 1 of Cmaj7 (offset 4)
  b.advance(kTicksPerBar);
  b.feed_note(3, 64, 0);    // release after exactly one bar
  b.advance(kTicksPerBar);  // a second, silent bar -- "a couple of bars" of noodling
  CHECK(!b.e.retro_capture().empty());

  b.ev.clear();
  b.cmd(Param::kRetroCaptureGrab, /*a=n_bars*/ 2, 0, 0, /*idx=*/0);
  CHECK(b.warns() == 0);
  const LoopClip* clip = b.e.loops().get(0);
  CHECK(clip != nullptr && clip->count() == 1);
  CHECK(clip->event(0).start == 0 && clip->event(0).duration == kTicksPerBar);
  CHECK(clip->event(0).source == LoopNoteSource::kChordTone);
  CHECK(clip->loop);  // grabbed content is a normal loop, identical in kind to a recorded one

  // Re-harmonization: change the followed chord BEFORE launching -- the
  // grabbed loop must sound the NEW chord's tone, not the pitch originally
  // played.
  b.cmd(Param::kChordPlay, 67, static_cast<std::int32_t>(ChordQuality::kMaj7),
        100);  // G -> Gmaj7, root_pc 7 (SAME shape as capture time)
  b.add_clip(TrackRole::kBass, 0, ContentKind::kLoopBuffer, 0);
  b.cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 1 | (2 << 8));
  b.ev.clear();
  b.cmd(Param::kClipLaunch, 0, 0, 0, /*idx=*/0);
  b.advance(kTicksPerBar);
  CHECK(b.count_midi(1, 2, 64, /*on=*/true) == 0);  // NOT the originally-played pitch
  CHECK(b.count_midi(1, 2, 71, /*on=*/true) >= 1);  // re-resolved under the NEW chord (G+4+60)
  CHECK(b.warns() == 0);
}

// (b) Disarmed: the ring never receives anything (the tee is gated on
// armed()), so a grab is a clean no-op -- it warns kRetroCaptureEmpty and
// leaves the target slot untouched.
void test_retro_disarmed_ring_stays_empty_and_grab_is_noop() {
  Band b;
  b.cmd(Param::kKeySet, 0, 0, 0, 0, Op::kSet);
  b.cmd(Param::kChordPlay, 60, static_cast<std::int32_t>(ChordQuality::kMaj7), 100);
  b.loop_new();
  CHECK(!b.e.retro_capture().armed());

  b.cmd(Param::kTransportStart);
  b.feed_note(3, 64, 100);  // never armed -- the tee never fires
  b.advance(kTicksPerBar);
  b.feed_note(3, 64, 0);
  CHECK(b.e.retro_capture().empty());

  b.ev.clear();
  b.cmd(Param::kRetroCaptureGrab, /*a=n_bars*/ 1, 0, 0, /*idx=*/0);
  CHECK(b.warns() == 1);                    // kRetroCaptureEmpty: nothing was ever captured
  CHECK(b.e.loops().get(0)->count() == 0);  // the target slot is untouched
}

// (c) Byte-identity guard: an Engine that never touches any kRetroCapture*
// verb must behave EXACTLY as before (mirrors test_loop_default_inert/
// test_scene_default_inert's own precedent).
void test_retro_default_inert() {
  Band b;
  b.cmd(Param::kTransportStart);
  b.ev.clear();
  b.feed_note(3, 64, 100);  // a live note on some port, never armed
  b.advance(4 * kTicksPerBar);
  b.feed_note(3, 64, 0);
  CHECK(b.warns() == 0);
  CHECK(!b.e.retro_capture().armed());
  CHECK(b.e.retro_capture().empty());
  for (const OutEvent& o : b.ev) {
    CHECK(o.kind != OutEvent::Kind::kLoop);  // no kGrabbed echo either
  }
}

// Cheap dispatch-wiring sanity: a bad target slot / bad arm port warn without
// mutating state.
void test_retro_bad_args_warn() {
  Band b;
  b.cmd(Param::kRetroCaptureArm, /*a=*/99);  // out-of-range port
  CHECK(b.warns() == 1);
  CHECK(!b.e.retro_capture().armed());

  b.ev.clear();
  b.cmd(Param::kRetroCaptureGrab, 1, 0, 0, /*idx=*/0);  // no slot registered at all
  CHECK(b.warns() == 1);
}

// Boundary of the port check (kMaxPorts == 4: valid ports are 0..3) and the
// EXACT WarnCode carried on each rejection, not just "some warn happened".
void test_retro_bad_args_boundary_and_warn_codes() {
  Band b;
  b.cmd(Param::kRetroCaptureArm, /*a=*/kMaxPorts);  // exactly one past the last valid port
  CHECK(b.warns() == 1);
  CHECK(b.ev.back().code == static_cast<std::uint16_t>(WarnCode::kBadArgument));
  CHECK(!b.e.retro_capture().armed());

  b.ev.clear();
  b.cmd(Param::kRetroCaptureArm, /*a=*/kMaxPorts - 1);  // the last VALID port arms cleanly
  CHECK(b.warns() == 0);
  CHECK(b.e.retro_capture().armed());

  b.loop_new();  // register exactly one slot (idx 0)
  b.ev.clear();
  b.cmd(Param::kRetroCaptureGrab, 1, 0, 0, /*idx=*/5);  // idx 5 was never registered
  CHECK(b.warns() == 1);
  CHECK(b.ev.back().code == static_cast<std::uint16_t>(WarnCode::kBadArgument));

  b.ev.clear();
  b.cmd(Param::kRetroCaptureGrab, 1, 0, 0, /*idx=*/0);  // registered, but ring is empty
  CHECK(b.warns() == 1);
  CHECK(b.ev.back().code == static_cast<std::uint16_t>(WarnCode::kRetroCaptureEmpty));
}

// Port-gating: the tee is gated on BOTH armed() and the exact matching port
// (Engine::push_midi_in: `is_note_message(msg) && m_retro.armed() && port ==
// m_retro.port()`). Notes on a DIFFERENT port while armed must NOT be
// captured at all.
void test_retro_wrong_port_not_captured() {
  Band b;
  b.cmd(Param::kRetroCaptureArm, /*a=*/3);
  CHECK(b.e.retro_capture().armed());

  b.cmd(Param::kTransportStart);
  b.feed_note(1, 64, 100);  // WRONG port (armed on 3)
  b.advance(kTicksPerBar);
  b.feed_note(1, 64, 0);
  CHECK(b.e.retro_capture().empty());
  CHECK(b.warns() == 0);

  // The correct port still works in the same session.
  b.feed_note(3, 67, 100);
  b.advance(kTicksPerBar);
  b.feed_note(3, 67, 0);
  CHECK(!b.e.retro_capture().empty());
}

// Double-arm: re-arming (same port or a different one) discards any prior
// ring content -- arm()'s own documented "abandon any in-progress capture,
// fresh start" discipline, mirrored from LoopBuffer::start_record.
void test_retro_double_arm_clears_prior_content() {
  Band b;
  b.cmd(Param::kRetroCaptureArm, /*a=*/3);
  b.cmd(Param::kTransportStart);
  b.feed_note(3, 64, 100);
  b.advance(kTicksPerBar);
  b.feed_note(3, 64, 0);
  CHECK(!b.e.retro_capture().empty());

  b.cmd(Param::kRetroCaptureArm, /*a=*/3);  // re-arm on the SAME port mid-session
  CHECK(b.e.retro_capture().empty());       // prior content discarded
  CHECK(b.e.retro_capture().armed());

  b.feed_note(3, 64, 100);
  b.advance(kTicksPerBar);
  b.feed_note(3, 64, 0);
  CHECK(!b.e.retro_capture().empty());

  b.cmd(Param::kRetroCaptureArm, /*a=*/1);  // re-arm on a DIFFERENT port
  CHECK(b.e.retro_capture().empty());       // prior content discarded here too
  CHECK(b.e.retro_capture().port() == 1);
  b.feed_note(3, 64, 100);  // the OLD port no longer captures
  CHECK(b.e.retro_capture().empty());
  b.feed_note(1, 64, 100);  // the NEW port does
  CHECK(!b.e.retro_capture().empty());
}

// Disarm without ever having armed: a clean no-op, no warn, stays disarmed.
void test_retro_disarm_without_arm_is_noop() {
  Band b;
  CHECK(!b.e.retro_capture().armed());
  b.cmd(Param::kRetroCaptureDisarm);
  CHECK(b.warns() == 0);
  CHECK(!b.e.retro_capture().armed());
  CHECK(b.e.retro_capture().empty());
}

// The load-bearing invariant (bullet 5): disarm is truly inert GOING FORWARD,
// not just "never armed at all". "arm, play, disarm, grab" is a valid
// gesture (ring content is left ALONE across disarm) -- but any note played
// AFTER the disarm must be silently dropped by the tee, never reaching the
// ring. If the tee ever captured while disarmed, this guard fails.
void test_retro_disarm_mid_session_truly_stops_further_capture() {
  Band b;
  b.cmd(Param::kKeySet, 0, 0, 0, 0, Op::kSet);
  b.cmd(Param::kChordPlay, 60, static_cast<std::int32_t>(ChordQuality::kMaj7), 100);
  b.loop_new();
  b.cmd(Param::kRetroCaptureArm, /*a=*/3);
  b.cmd(Param::kTransportStart);

  b.feed_note(3, 64, 100);  // captured while armed
  b.advance(kTicksPerBar);
  b.feed_note(3, 64, 0);
  CHECK(b.e.loops().get(0) != nullptr);
  const std::size_t count_before_disarm = b.e.retro_capture().count();
  CHECK(count_before_disarm >= 1);

  b.cmd(Param::kRetroCaptureDisarm);
  CHECK(!b.e.retro_capture().armed());
  CHECK(b.e.retro_capture().count() == count_before_disarm);  // disarm alone leaves ring ALONE

  b.feed_note(3, 65, 100);  // played AFTER disarm, on the SAME port -- must be dropped
  b.advance(kTicksPerBar);
  b.feed_note(3, 65, 0);
  CHECK(b.e.retro_capture().count() == count_before_disarm);  // still unchanged -- truly inert

  b.ev.clear();
  b.cmd(Param::kRetroCaptureGrab, /*a=n_bars*/ 10, 0, 0,
        /*idx=*/0);  // grab everything the ring ever held
  CHECK(b.warns() == 0);
  const LoopClip* clip = b.e.loops().get(0);
  CHECK(clip != nullptr &&
        clip->count() == 1);  // ONLY the pre-disarm note, never the post-disarm one
}

// Re-arm after a successful grab: grab leaves ring content ALONE (so a
// second grab on the same content is possible), but arming again clears it,
// and a subsequent grab on the freshly-armed-but-not-yet-fed ring warns
// kRetroCaptureEmpty exactly like a never-armed ring.
void test_retro_rearm_after_grab_clears_ring() {
  Band b;
  b.loop_new();
  b.loop_new();
  b.cmd(Param::kRetroCaptureArm, /*a=*/3);
  b.cmd(Param::kTransportStart);
  b.feed_note(3, 64, 100);
  b.advance(kTicksPerBar);
  b.feed_note(3, 64, 0);
  CHECK(!b.e.retro_capture().empty());

  b.ev.clear();
  b.cmd(Param::kRetroCaptureGrab, /*a=*/1, 0, 0, /*idx=*/0);
  CHECK(b.warns() == 0);
  CHECK(!b.e.retro_capture().empty());  // grab leaves the ring content alone

  // A second grab into a DIFFERENT slot, same still-armed ring, still works.
  b.ev.clear();
  b.cmd(Param::kRetroCaptureGrab, /*a=*/1, 0, 0, /*idx=*/1);
  CHECK(b.warns() == 0);
  CHECK(b.e.loops().get(1)->count() == 1);

  b.cmd(Param::kRetroCaptureArm, /*a=*/3);  // re-arm: clears the ring
  CHECK(b.e.retro_capture().empty());

  b.ev.clear();
  b.cmd(Param::kRetroCaptureGrab, /*a=*/1, 0, 0, /*idx=*/0);
  CHECK(b.warns() == 1);  // kRetroCaptureEmpty: nothing captured since the re-arm
}

}  // namespace

int main() {
  test_retro_arm_play_grab_reharmonizes();
  test_retro_disarmed_ring_stays_empty_and_grab_is_noop();
  test_retro_default_inert();
  test_retro_bad_args_warn();
  test_retro_bad_args_boundary_and_warn_codes();
  test_retro_wrong_port_not_captured();
  test_retro_double_arm_clears_prior_content();
  test_retro_disarm_without_arm_is_noop();
  test_retro_disarm_mid_session_truly_stops_further_capture();
  test_retro_rearm_after_grab_clears_ring();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_retro_capture: all OK\n");
  }
  return arrangrr::test::failures();
}
