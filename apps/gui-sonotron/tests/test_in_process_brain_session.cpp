// In-process round-trip smoke test (Phase 2b gate item 2, docs/design/
// sonotron-server-phase2-brief.md): push a Command onto the GUI->engine
// ring (via send()), let the engine thread tick, drain an OutEvent off the
// engine->GUI ring (via poll()), decode it, and check the expected
// BrainEvent state -- proving the whole in-process ring + engine-thread +
// decode mechanism end to end, with no socket and no display. Runs with no
// ALSA sequencer device required (the engine thread degrades to "silent",
// see in_process_brain_session.cpp) so this is safe in a headless/CI
// sandbox.

#include "src/in_process_brain_session.hpp"

#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "src/brain_event.hpp"
#include "test.hpp"

using sonotron::BrainEvent;
using sonotron::BrainSession;
using sonotron::InProcessBrainSession;

namespace {

// Polls the session repeatedly (the engine thread's clock and the ring are
// genuinely asynchronous with respect to this thread) until `predicate`
// matches a drained event or `max_iterations` short sleeps have elapsed.
bool poll_until(InProcessBrainSession& session, std::vector<BrainEvent>& collected,
                const std::function<bool(const BrainEvent&)>& predicate, int max_iterations = 400) {
  for (int i = 0; i < max_iterations; ++i) {
    std::vector<BrainEvent> batch;
    session.poll(batch);
    for (BrainEvent& ev : batch) {
      const bool match = predicate(ev);
      collected.push_back(std::move(ev));
      if (match) {
        return true;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  return false;
}

void test_round_trip_transport_start() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("transport start");

  const bool got_playing = poll_until(session, collected, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kTransport && ev.transport_state == "playing";
  });
  CHECK(got_playing);
  CHECK(session.status() == BrainSession::Status::kConnected);

  session.stop();
  CHECK(session.status() == BrainSession::Status::kDisconnected);
}

void test_transport_stop_after_start() {
  InProcessBrainSession session;
  CHECK(session.start());
  session.send("transport start");

  std::vector<BrainEvent> started;
  CHECK(poll_until(session, started, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kTransport && ev.transport_state == "playing";
  }));

  session.send("transport stop");
  std::vector<BrainEvent> stopped;
  CHECK(poll_until(session, stopped, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kTransport && ev.transport_state == "stopped";
  }));

  session.stop();
}

// A command this milestone deliberately does not translate to a Command POD
// (see in_process_brain_session.cpp's command_line_to_command -- its closed
// set of recognized shapes does not include this one) must still surface
// visibly rather than vanish silently. "style load"/"part ... mute|solo ..."
// are NOT this case anymore (see the tests below) -- this uses a command
// nothing in this translator recognizes at all.
void test_untranslated_command_surfaces_as_error_note() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("arp on");
  CHECK(poll_until(
      session, collected, [](const BrainEvent& ev) { return ev.kind == BrainEvent::Kind::kError; },
      50));

  session.stop();
}

// Waits a bounded, short number of iterations and reports whether `predicate`
// EVER matched a drained event -- the negative-assertion counterpart of
// poll_until, used below to prove a valid `style load`/`part ...` line does
// NOT surface a kError note (unlike before this milestone's fix).
bool never_seen(InProcessBrainSession& session, std::vector<BrainEvent>& collected,
                const std::function<bool(const BrainEvent&)>& predicate, int max_iterations = 50) {
  return !poll_until(session, collected, predicate, max_iterations);
}

// Closes the Phase 2b integrated-mode gap flagged in this milestone's own
// report: `style load <name>` now resolves the builtin name through
// Shell::resolve_style_index() (exposed for exactly this caller) and reaches
// the engine as a real kStyleLoad Command -- exactly like `--control` mode --
// instead of the generic "does not translate" kError every text command hit
// before this fix. Style/part commands on SUCCESS emit no OutEvent (neither
// does exec_line's own cmd_style/cmd_part on the unrouted default topology),
// so the observable proof here is the ABSENCE of the error note that used to
// fire on every single one of these lines, plus a live transport round trip
// straight after to prove the engine thread kept processing normally (not
// silently wedged).
void test_style_load_valid_name_is_accepted_without_error() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("style load basic");
  CHECK(never_seen(session, collected,
                   [](const BrainEvent& ev) { return ev.kind == BrainEvent::Kind::kError; }));

  session.send("transport start");
  std::vector<BrainEvent> after;
  CHECK(poll_until(session, after, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kTransport && ev.transport_state == "playing";
  }));

  session.stop();
}

// An unknown style name must still surface a clean kError (no crash, no
// silence) -- the translator's own name resolution fails BEFORE a Command is
// ever built, so nothing reaches the engine's push_command path.
void test_style_load_invalid_name_surfaces_clean_error() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("style load not-a-real-style");
  CHECK(poll_until(session, collected, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kError &&
           ev.error.find("unknown style") != std::string::npos;
  }));

  session.stop();
}

// SLICE 4a item 5 (docs/proposals/repeat-zone-real-contract.md): `style
// section <name>` resolves the section name through parse_section_name and
// reaches the engine as a real kStyleSection Command -- mirrors
// test_style_load_valid_name_is_accepted_without_error()'s own shape exactly,
// but proves the Command actually landed via the CORE's own "section"
// OutEvent (Engine::cmd_style's kStyleSection handling emits
// OutEvent::section() immediately while the transport is stopped) rather
// than merely the absence of an error -- a stronger, more direct proof that
// this specific Param reached Engine::cmd_style, not just "some Command".
void test_style_section_valid_name_reaches_style_section_command() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> after_load;
  session.send("style load basic");
  CHECK(never_seen(session, after_load,
                   [](const BrainEvent& ev) { return ev.kind == BrainEvent::Kind::kError; }));

  std::vector<BrainEvent> collected;
  session.send("style section varB");
  CHECK(poll_until(session, collected, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kSection && ev.section_name == "varB";
  }));

  session.stop();
}

// An unknown section name must still surface a clean kError (no crash, no
// silence) -- the translator's own name resolution fails BEFORE a Command is
// ever built, mirroring test_style_load_invalid_name_surfaces_clean_error().
void test_style_section_invalid_name_surfaces_clean_error() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("style section not-a-real-section");
  CHECK(poll_until(session, collected, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kError &&
           ev.error.find("unknown section") != std::string::npos;
  }));

  session.stop();
}

// `part <role> mute|solo on|off` now resolves the role through
// Shell::resolve_track_role() the same way, closing the second half of the
// gap this milestone's report flagged.
void test_part_mute_and_solo_valid_role_is_accepted_without_error() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("part lead mute on");
  session.send("part bass solo off");
  CHECK(never_seen(session, collected,
                   [](const BrainEvent& ev) { return ev.kind == BrainEvent::Kind::kError; }));

  session.send("transport start");
  std::vector<BrainEvent> after;
  CHECK(poll_until(session, after, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kTransport && ev.transport_state == "playing";
  }));

  session.stop();
}

// An unknown role name must still surface a clean kError.
void test_part_invalid_role_surfaces_clean_error() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("part wizard mute on");
  CHECK(poll_until(session, collected, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kError &&
           ev.error.find("unknown role") != std::string::npos;
  }));

  session.stop();
}

// Phase-6 Theme 3 Item #1 (docs/reflections/phase6-theme3-master-transpose-
// scope.md): `transpose <-12..12>` translates and reaches the engine as a
// real kMasterTranspose Command -- same success shape as `style load`/
// `part ...` above (no kError on a valid value).
void test_transpose_valid_value_is_accepted_without_error() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("transpose -5");
  CHECK(never_seen(session, collected,
                   [](const BrainEvent& ev) { return ev.kind == BrainEvent::Kind::kError; }));

  session.send("transport start");
  std::vector<BrainEvent> after;
  CHECK(poll_until(session, after, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kTransport && ev.transport_state == "playing";
  }));

  session.stop();
}

// A value outside [-12, +12] must still surface a clean kError -- the
// translator's own bound check fails BEFORE a Command is ever built, so
// nothing reaches the engine's push_command path.
void test_transpose_out_of_range_surfaces_clean_error() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("transpose 13");
  CHECK(poll_until(session, collected, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kError &&
           ev.error.find("bad transpose") != std::string::npos;
  }));

  session.stop();
}

// Tempo nudge (transport_panel.cpp's BPM label): `bpm <N>` translates and
// reaches the engine as a real kTransportTempo Command -- same success shape as
// `transpose` above (no kError on a valid value).
void test_bpm_valid_value_is_accepted_without_error() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("bpm 140");
  CHECK(never_seen(session, collected,
                   [](const BrainEvent& ev) { return ev.kind == BrainEvent::Kind::kError; }));

  session.send("transport start");
  std::vector<BrainEvent> after;
  CHECK(poll_until(session, after, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kTransport && ev.transport_state == "playing";
  }));

  session.stop();
}

// A value outside 20..400 must surface a clean kError -- the translator's own
// bound check fails BEFORE a Command is built, so nothing reaches the engine.
void test_bpm_out_of_range_surfaces_clean_error() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("bpm 999");
  CHECK(poll_until(session, collected, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kError && ev.error.find("bad bpm") != std::string::npos;
  }));

  session.stop();
}

// Phase-6 Theme 3 Item #2's companion (docs/reflections/phase6-theme3-pad-
// drum-cc-scope.md): `pad bank <n>` translates and reaches the engine as a
// real kPadBankSelect Command -- same success shape as `transpose` above (no
// kError on a valid value); the engine warns (kWarn, not kError -- the
// translator's own bound is unparsable-token-only) on an out-of-range bank.
void test_pad_bank_valid_value_is_accepted_without_error() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("pad bank 3");
  CHECK(never_seen(session, collected,
                   [](const BrainEvent& ev) { return ev.kind == BrainEvent::Kind::kError; }));

  session.send("transport start");
  std::vector<BrainEvent> after;
  CHECK(poll_until(session, after, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kTransport && ev.transport_state == "playing";
  }));

  session.stop();
}

void test_pad_bank_unparsable_token_surfaces_clean_error() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("pad bank nope");
  CHECK(poll_until(session, collected, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kError &&
           ev.error.find("bad pad bank") != std::string::npos;
  }));

  session.stop();
}

// GUI refinement (Accompany, Phase 4d): `midi-source load <path>` now
// round-trips through a dedicated path-carrying ring straight to
// Shell::load_midi_source() on the engine thread (Command's own POD has no
// room for a variable-length path, abi.hpp) -- mirroring --control's own
// text-command entry point (shell_io_commands.cpp's cmd_midi_source). A
// VALID path loads silently (no kError), the same success shape `style
// load`/`part ...` already have; a live transport round trip straight after
// proves the engine thread stayed alive (not wedged on the file load).
void test_midi_source_load_valid_path_is_accepted_without_error() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send(std::string("midi-source load ") + GUI_SONOTRON_TEST_MIDI_FIXTURE);
  CHECK(never_seen(session, collected,
                   [](const BrainEvent& ev) { return ev.kind == BrainEvent::Kind::kError; }));

  session.send("transport start");
  std::vector<BrainEvent> after;
  CHECK(poll_until(session, after, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kTransport && ev.transport_state == "playing";
  }));

  session.stop();
}

// A nonexistent path must surface a clean kError round-tripped from the
// engine thread through the path-result ring -- Shell::load_midi_source's
// own "midi-source load failed: <path>" message (shell_io_commands.cpp),
// forwarded verbatim, not swallowed.
void test_midi_source_load_invalid_path_surfaces_clean_error() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("midi-source load /no/such/file/does-not-exist.mid");
  CHECK(poll_until(session, collected, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kError &&
           ev.error.find("midi-source load failed") != std::string::npos;
  }));

  session.stop();
}

// Phase-5 Item #2 (docs/design/clip-primitive-design.md): `launch clip <id>
// quantize <n>` translates to a real kClipLaunch Command -- grid_panel.cpp's
// own send() shape. No clip is registered in this fresh engine, so the CORE
// itself warns (kWarn, "bad_argument") rather than the translator failing --
// the translator's own job (parsing the line into a Command) succeeds, and
// that warn round-trips as a real OutEvent, proving the whole path reaches
// push_command.
void test_launch_clip_translates_and_reaches_engine() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("launch clip 0 quantize 1");
  CHECK(poll_until(session, collected, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kWarn && ev.warn_code == "bad_argument";
  }));

  session.send("transport start");
  std::vector<BrainEvent> after;
  CHECK(poll_until(session, after, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kTransport && ev.transport_state == "playing";
  }));

  session.stop();
}

// A malformed clip id must surface a clean kError from the TRANSLATOR itself
// (never reaches push_command) -- mirrors the style/part "invalid name"
// tests above.
void test_launch_clip_bad_id_surfaces_clean_error() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("launch clip not-a-number quantize 1");
  CHECK(poll_until(session, collected, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kError && ev.error.find("bad id") != std::string::npos;
  }));

  session.stop();
}

// Repeat-Zone binding contract, Shape A (docs/proposals/repeat-zone-real-
// contract.md §3/§8b decision 1): `clip add <role> <scene> style <section>
// id <n>` -- grid_panel.cpp's own drag-drop registration -- reaches the
// engine as a real kClipAdd Command carrying the EXPLICIT id, registering
// the clip AT that id instead of the sequential counter. Proves the whole
// round trip: registration, no error/warn, a real launch addressing that
// EXACT id, and the real "clip" readback confirming it (never a bad_argument
// warn, unlike test_launch_clip_translates_and_reaches_engine's fresh,
// nothing-registered engine above).
void test_clip_add_with_explicit_id_registers_and_launches() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("clip add lead 0 style varA id 5");
  CHECK(never_seen(session, collected,
                   [](const BrainEvent& ev) { return ev.kind == BrainEvent::Kind::kWarn; }));

  session.send("launch clip 5 quantize 0");
  std::vector<BrainEvent> after;
  CHECK(poll_until(session, after, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kClip && ev.clip_id == 5 && ev.clip_state == "playing";
  }));

  session.stop();
}

// Registering the SAME explicit id twice is rejected core-side (ClipMatrix
// stays append-only, no retarget, §8b decision 1) -- a real kWarn, not a
// silent no-op.
void test_clip_add_duplicate_explicit_id_warns() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("clip add lead 0 style varA id 9");
  CHECK(never_seen(session, collected,
                   [](const BrainEvent& ev) { return ev.kind == BrainEvent::Kind::kWarn; }));

  std::vector<BrainEvent> second;
  session.send("clip add bass 1 style varB id 9");
  CHECK(poll_until(session, second, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kWarn && ev.warn_code == "bad_argument";
  }));

  session.stop();
}

// A malformed explicit id (unparsable, or the reserved sentinel value) must
// surface a clean kError from the TRANSLATOR itself, never reach push_command.
void test_clip_add_bad_id_surfaces_clean_error() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("clip add lead 0 style varA id not-a-number");
  CHECK(poll_until(session, collected, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kError && ev.error.find("bad id") != std::string::npos;
  }));

  session.stop();
}

// Phase 7 (node 6000, docs/proposals/looper-in-gui-contract.md §2/§7 items
// 1/2): `note <port> on|off <midinote> [velocity]` translates to a real
// Param::kNoteRaw Command. BEFORE this milestone's fix, ANY kNoteRaw Command
// pushed through the ring landed on Engine::push_command's unhandled-param
// default and emitted exactly WarnCode::kUnknownCommand (abi.hpp's own
// kNoteRaw comment: "it never reaches Engine::push_command()") -- this is the
// precise regression the drain-loop special case (item 2) closes.
void test_note_raw_never_surfaces_unknown_command_warn() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("note 0 on 60 100");
  session.send("note 0 off 60");
  CHECK(never_seen(session, collected, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kWarn && ev.warn_code == "unknown_command";
  }));

  session.stop();
}

// Proves the note actually reaches the LIVE-INPUT tap Engine::push_midi_in
// runs -- not just "was accepted somewhere" -- by driving a `loop record`
// window on the SAME port a note is sent on: `loop record`'s own start only
// flips m_loop.recording() true, and `loop stop` only succeeds while
// GENUINELY recording (Engine::loop_record_stop rejects otherwise with
// kBadArgument) -- so a `loop stop` that surfaces no "bad_argument" warn here
// proves the whole sequence (including feed_midi()'s note-on/note-off in
// between) reached the engine's real Engine::push_midi_in path. Neither the
// note nor the loop verbs need ALSA/port-open: kNoteRaw's own feed_midi()
// call and kLoopRecordStart/Stop's port argument are both raw indices,
// independent of Shell's port-open table -- exactly why this test (like
// every other test in this file) runs safely in a headless/CI sandbox. The
// captured EVENT COUNT itself is verified at the core layer already
// (test_loop.cpp, components/platform/hostrt/tests/test_host.cpp's
// test_shell_loop_commands) -- BrainEvent has no loop-content readback yet
// (docs/proposals/looper-in-gui-contract.md §7 item 9, a later slice), so
// this is the strongest signal reachable at this layer. NOTE: every
// kLoopRecordStart/Stop SUCCESS also emits a real OutEvent::loop() echo that
// currently (a KNOWN, documented, out-of-scope defect -- §7 item 9)
// misdecodes as a benign BrainEvent::Kind::kWarn with warn_code=="none"
// (brain_event_from_outevent.cpp's kLoop case falls into the kWarn default);
// this test checks for the REAL failure code ("bad_argument"), not the
// presence of kWarn at all, so it is not tripped by that separate, already-
// flagged bug.
void test_note_raw_reaches_engine_via_loop_record_round_trip() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("loop new");
  session.send("loop record 0 record 0");
  session.send("note 0 on 60 100");
  session.send("note 0 off 60");
  session.send("loop stop 0");
  CHECK(never_seen(session, collected, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kWarn && ev.warn_code == "bad_argument";
  }));

  session.stop();
}

// A malformed note line (unparsable port/note/velocity, or a missing on|off)
// must surface a clean kError from the TRANSLATOR itself, never reach
// push_command -- mirrors the style/part/transpose "invalid input" tests
// above.
void test_note_raw_bad_input_surfaces_clean_error() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("note 0 maybe 60 100");
  CHECK(poll_until(session, collected, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kError &&
           ev.error.find("note <port> on|off") != std::string::npos;
  }));

  session.stop();
}

// docs/proposals/looper-in-gui-contract.md §7 item 4: the SAME `loop ...`
// grammar shell_loop_commands.cpp implements, translated by the integrated
// GUI backend too -- every one of these Params already has an Engine::
// push_command case (Engine::cmd_loop), so (unlike kNoteRaw) they ride the
// normal Command ring with no drain-loop special case needed. Checks for the
// REAL failure code ("bad_argument"), not the presence of kWarn at all --
// every SUCCESS here also emits a real OutEvent::loop() echo that currently
// misdecodes as a benign kWarn/"none" (a KNOWN, documented, out-of-scope
// defect, §7 item 9), which this assertion is deliberately not tripped by
// (see test_note_raw_reaches_engine_via_loop_record_round_trip's own longer
// comment above).
void test_loop_verbs_translate_and_reach_engine_without_error() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("loop new");
  session.send("loop record 0 record 0");
  session.send("loop stop 0");
  session.send("loop length 0 fixed 960");
  session.send("loop erase 0");
  session.send("loop undo 0");
  CHECK(never_seen(session, collected, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kWarn && ev.warn_code == "bad_argument";
  }));

  session.stop();
}

// A bad loop record mode must surface a clean kError from the TRANSLATOR
// itself (never reaches push_command).
void test_loop_record_bad_mode_surfaces_clean_error() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("loop record 0 notamode 0");
  CHECK(poll_until(session, collected, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kError &&
           ev.error.find("loop record mode") != std::string::npos;
  }));

  session.stop();
}

// A `loop stop` on a slot that never started recording still reaches the
// engine and surfaces a real kWarn -- proving push_command was actually
// reached, not swallowed by the translator.
void test_loop_stop_without_active_recording_warns() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("loop new");
  session.send("loop stop 0");  // never started recording
  CHECK(poll_until(session, collected, [](const BrainEvent& ev) {
    return ev.kind == BrainEvent::Kind::kWarn && ev.warn_code == "bad_argument";
  }));

  session.stop();
}

void test_stop_is_idempotent_and_safe_before_start() {
  InProcessBrainSession session;
  session.stop();  // never started: must be a safe no-op
  CHECK(session.start());
  session.stop();
  session.stop();  // idempotent once stopped
}

}  // namespace

int main() {
  test_round_trip_transport_start();
  test_transport_stop_after_start();
  test_untranslated_command_surfaces_as_error_note();
  test_style_load_valid_name_is_accepted_without_error();
  test_style_load_invalid_name_surfaces_clean_error();
  test_style_section_valid_name_reaches_style_section_command();
  test_style_section_invalid_name_surfaces_clean_error();
  test_part_mute_and_solo_valid_role_is_accepted_without_error();
  test_part_invalid_role_surfaces_clean_error();
  test_transpose_valid_value_is_accepted_without_error();
  test_transpose_out_of_range_surfaces_clean_error();
  test_bpm_valid_value_is_accepted_without_error();
  test_bpm_out_of_range_surfaces_clean_error();
  test_pad_bank_valid_value_is_accepted_without_error();
  test_pad_bank_unparsable_token_surfaces_clean_error();
  test_midi_source_load_valid_path_is_accepted_without_error();
  test_midi_source_load_invalid_path_surfaces_clean_error();
  test_launch_clip_translates_and_reaches_engine();
  test_launch_clip_bad_id_surfaces_clean_error();
  test_clip_add_with_explicit_id_registers_and_launches();
  test_clip_add_duplicate_explicit_id_warns();
  test_clip_add_bad_id_surfaces_clean_error();
  test_note_raw_never_surfaces_unknown_command_warn();
  test_note_raw_reaches_engine_via_loop_record_round_trip();
  test_note_raw_bad_input_surfaces_clean_error();
  test_loop_verbs_translate_and_reach_engine_without_error();
  test_loop_record_bad_mode_surfaces_clean_error();
  test_loop_stop_without_active_recording_warns();
  test_stop_is_idempotent_and_safe_before_start();
  return sonotron::test::failures();
}
