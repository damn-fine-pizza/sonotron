// Unit tests for AppState: event reduction (transport/section/chord) and the
// harmony activity gate (ux-workstation.md §9/§13). No GPU, no display, no
// core.

#include "src/app_state.hpp"

#include "test.hpp"

using sonotron::AppState;

namespace {

void test_connected_flag() {
  AppState app;
  CHECK(!app.connected());
  app.set_connected(true);
  CHECK(app.connected());
  app.set_connected(false);
  CHECK(!app.connected());
}

void test_transport_and_section_reduction() {
  AppState app;
  CHECK(app.transport() == AppState::Transport::kStopped);
  CHECK(app.section() == "-");

  app.apply_line(R"({"ev":"transport","state":"playing","@":1})");
  CHECK(app.transport() == AppState::Transport::kPlaying);

  app.apply_line(R"({"ev":"section","name":"fillB","@":3})");
  CHECK(app.section() == "fillB");

  app.apply_line(R"({"ev":"transport","state":"paused","@":4})");
  CHECK(app.transport() == AppState::Transport::kPaused);

  app.apply_line(R"({"ev":"transport","state":"stopped","@":5})");
  CHECK(app.transport() == AppState::Transport::kStopped);
}

void test_chord_reduction() {
  AppState app;
  CHECK(app.chord_out() == "-");
  CHECK(app.chord_degree() == "-");

  // The only shipped "chord" event is the recorded-sequencer path
  // (gui-contract-map.md §3) -- {in, out, deg}.
  app.apply_line(R"({"ev":"chord","in":"C4","out":"Cmaj","deg":"I","@":384})");
  CHECK(app.chord_in() == "C4");
  CHECK(app.chord_out() == "Cmaj");
  CHECK(app.chord_degree() == "I");
}

void test_chord_followed_reduction() {
  AppState app;
  CHECK(app.chord_followed_current() == "-");
  CHECK(!app.chord_followed_current_valid());
  CHECK(app.chord_followed_next() == "-");
  CHECK(!app.chord_followed_next_valid());

  // Manual immediate commit: current lands, no pending, src manual. Receiving
  // the event is itself authoritative confirmation of a steer -- it opens the
  // harmony_active() gate even with the transport stopped.
  CHECK(!app.harmony_active());
  app.apply_line(R"({"ev":"chord-followed","cur":"Cmaj7","cur_pcs":2193,"next":"-","next_pcs":0,)"
                 R"("src":"manual","@":0})");
  CHECK(app.chord_followed_current() == "Cmaj7");
  CHECK(app.chord_followed_current_valid());
  CHECK(!app.chord_followed_next_valid());
  CHECK(app.chord_followed_source() == "manual");
  CHECK(app.harmony_active());

  // A shift-staged chord: current unchanged, next becomes valid.
  app.apply_line(
      R"({"ev":"chord-followed","cur":"Cmaj7","cur_pcs":2193,"next":"G7","next_pcs":2212,)"
      R"("src":"detect","@":10})");
  CHECK(app.chord_followed_next() == "G7");
  CHECK(app.chord_followed_next_valid());
  CHECK(app.chord_followed_source() == "detect");

  // Transport stopping closes the activity gate but does not itself clear
  // the last-known followed chord (only a fresh event does).
  app.apply_line(R"({"ev":"transport","state":"stopped","@":11})");
  CHECK(!app.harmony_active());
  CHECK(app.chord_followed_current() == "Cmaj7");
}

void test_harmony_activity_gate() {
  AppState app;
  CHECK(!app.harmony_active());  // at rest: nothing lit

  app.apply_line(R"({"ev":"transport","state":"playing","@":0})");
  CHECK(app.harmony_active());

  app.apply_line(R"({"ev":"transport","state":"stopped","@":1})");
  CHECK(!app.harmony_active());  // back to rest closes the gate

  // Manual-steer hint opens the gate even while stopped.
  app.note_manual_steer();
  CHECK(app.harmony_active());

  // A stop event closes the gate again, regardless of the hint.
  app.apply_line(R"({"ev":"transport","state":"playing","@":2})");
  CHECK(app.harmony_active());
  app.apply_line(R"({"ev":"transport","state":"stopped","@":3})");
  CHECK(!app.harmony_active());
}

void test_note_transport_sent_hint() {
  AppState app;
  app.note_manual_steer();
  app.note_transport_sent(true);
  CHECK(app.transport() == AppState::Transport::kPlaying);
  CHECK(app.harmony_active());

  // Sending "stop" also closes the manual-steer hint (mirrors the spike).
  app.note_transport_sent(false);
  CHECK(app.transport() == AppState::Transport::kStopped);
  CHECK(!app.harmony_active());
}

void test_malformed_line_logged_not_applied() {
  AppState app;
  app.apply_line(R"({"ev":"section","name":"varA","@":0})");
  const std::string before_section = app.section();
  const AppState::Transport before_transport = app.transport();

  app.apply_line("this is not json");
  CHECK(app.section() == before_section);
  CHECK(app.transport() == before_transport);
  CHECK(!app.log().empty());
}

void test_log_cap() {
  AppState app;
  for (std::size_t i = 0; i < AppState::kMaxLog + 50; ++i) {
    app.apply_line("noise");
  }
  CHECK(app.log().size() == AppState::kMaxLog);
}

void test_warn_and_midi_out_do_not_change_view_state_but_are_logged() {
  AppState app;
  const std::size_t before = app.log().size();
  app.apply_line(R"({"ev":"warn","code":"scheduler_full","@":0})");
  app.apply_line(R"({"ev":"midi-out","port":0,"msg":"noteon","@":1})");
  CHECK(app.log().size() == before + 2);
  CHECK(app.transport() == AppState::Transport::kStopped);
  CHECK(app.section() == "-");
}

}  // namespace

int main() {
  test_connected_flag();
  test_transport_and_section_reduction();
  test_chord_reduction();
  test_chord_followed_reduction();
  test_harmony_activity_gate();
  test_note_transport_sent_hint();
  test_malformed_line_logged_not_applied();
  test_log_cap();
  test_warn_and_midi_out_do_not_change_view_state_but_are_logged();
  return sonotron::test::failures();
}
