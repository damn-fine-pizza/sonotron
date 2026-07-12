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
// (see in_process_brain_session.cpp's command_line_to_command -- it needs
// Shell-internal name resolution) must still surface visibly rather than
// vanish silently.
void test_untranslated_command_surfaces_as_error_note() {
  InProcessBrainSession session;
  CHECK(session.start());

  std::vector<BrainEvent> collected;
  session.send("style load basic");
  CHECK(poll_until(
      session, collected, [](const BrainEvent& ev) { return ev.kind == BrainEvent::Kind::kError; },
      50));

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
  test_stop_is_idempotent_and_safe_before_start();
  return sonotron::test::failures();
}
