#include "midisrc/midi_source_stage.hpp"

#include <cstddef>
#include <string>
#include <vector>

#include "common/midi/message.hpp"
#include "midisrc/diagnostics.hpp"
#include "runtime/out_scheduler.hpp"
#include "runtime/stage.hpp"
#include "test.hpp"

namespace {

using namespace midisrc;

constexpr std::size_t kCapacity = 64;

// A generic no-op sink: `MidiSourceStage` never calls its sink parameter
// (every note goes through the shared `OutScheduler`, §16.2a) — this stands
// in for whatever concrete sink a real pipeline would inject, without
// pulling in `arrangrr::OutEvent` (midisrc must not depend on arrangrr, D43).
struct NullSink {
  template <typename T>
  void operator()(const T&) const noexcept {}
};

// Same shape as NullSink but counts invocations, for the flush() no-op check.
struct CountingSink {
  int* calls = nullptr;
  template <typename T>
  void operator()(const T&) const noexcept {
    ++(*calls);
  }
};

std::string fixture_path(const std::string& name) { return std::string(MIDISRC_FIXTURES) + name; }

// tiny.mid (shared fixture with apps/tools/arrstyle-converter/tests/
// test_midi_import.cpp): 96 PPQN, two melodic notes on channel 0 (C4/60 vel
// 100 gate 96, then D4/62) and one drum note on channel 9 — three notes,
// six events (on+off each).
void test_load_ok_and_event_count() {
  arrangrr::OutScheduler<kCapacity> scheduler;
  arrstyle::Diagnostics diag;
  MidiSourceStage<kCapacity> stage(scheduler, /*port=*/0);
  CHECK(!stage.ok());  // constructed inert -- nothing loaded yet (4d)
  CHECK(stage.load(fixture_path("tiny.mid"), diag));

  CHECK(stage.ok());
  CHECK(!diag.has_errors());
  CHECK(stage.event_count() == 6);
}

void test_events_emitted_in_tick_order_thru_scheduler() {
  arrangrr::OutScheduler<kCapacity> scheduler;
  arrstyle::Diagnostics diag;
  MidiSourceStage<kCapacity> stage(scheduler, /*port=*/0);
  CHECK(stage.load(fixture_path("tiny.mid"), diag));
  CHECK(stage.ok());

  // Drive on_tick far enough forward to schedule every event in one shot
  // (tiny.mid's whole span, scaled from 96 to the internal 960 PPQN, is well
  // under this).
  const runtime::StageContext ctx{.now = 20'000};
  stage.on_tick(ctx, NullSink{});

  std::vector<arrangrr::ScheduledEvent> popped;
  scheduler.pop_due(20'000,
                    [&popped](const arrangrr::ScheduledEvent& ev) { popped.push_back(ev); });

  CHECK(popped.size() == 6);
  for (std::size_t i = 1; i < popped.size(); ++i) {
    CHECK(popped[i - 1].tick <= popped[i].tick);
  }

  // The first melodic Note On is C4 (60), velocity 100 — the same fixture
  // fact test_midi_import.cpp already asserts.
  bool saw_note_on_60 = false;
  for (const arrangrr::ScheduledEvent& ev : popped) {
    if (ev.msg.type() == arrangrr::midi::kNoteOn && ev.msg.d1 == 60) {
      CHECK(ev.msg.d2 == 100);
      saw_note_on_60 = true;
    }
  }
  CHECK(saw_note_on_60);
}

void test_flush_is_a_no_op() {
  arrangrr::OutScheduler<kCapacity> scheduler;
  arrstyle::Diagnostics diag;
  MidiSourceStage<kCapacity> stage(scheduler, /*port=*/0);
  CHECK(stage.load(fixture_path("tiny.mid"), diag));
  CHECK(stage.ok());

  int sink_calls = 0;
  stage.flush(CountingSink{&sink_calls});
  CHECK(sink_calls == 0);
}

void test_missing_file_reports_error() {
  arrangrr::OutScheduler<kCapacity> scheduler;
  arrstyle::Diagnostics diag;
  MidiSourceStage<kCapacity> stage(scheduler, /*port=*/0);
  CHECK(!stage.load(fixture_path("does_not_exist.mid"), diag));

  CHECK(!stage.ok());
  CHECK(diag.has_errors());
  CHECK(stage.event_count() == 0);
}

void test_uninitialized_stage_on_tick_is_a_no_op() {
  // 4d: a pipeline that always declares a MIDI-source stage (e.g.
  // hostrt::Shell's unified pipeline) must be byte-identical to one without
  // it when no file is ever loaded -- on_tick/flush stay no-ops.
  arrangrr::OutScheduler<kCapacity> scheduler;
  MidiSourceStage<kCapacity> stage(scheduler, /*port=*/0);
  CHECK(!stage.ok());

  const runtime::StageContext ctx{.now = 20'000};
  stage.on_tick(ctx, NullSink{});
  int sink_calls = 0;
  stage.flush(CountingSink{&sink_calls});
  CHECK(sink_calls == 0);

  std::vector<arrangrr::ScheduledEvent> popped;
  scheduler.pop_due(20'000,
                    [&popped](const arrangrr::ScheduledEvent& ev) { popped.push_back(ev); });
  CHECK(popped.empty());
}

}  // namespace

int main() {
  test_load_ok_and_event_count();
  test_events_emitted_in_tick_order_thru_scheduler();
  test_flush_is_a_no_op();
  test_missing_file_reports_error();
  test_uninitialized_stage_on_tick_is_a_no_op();
  return midisrc::test::failures();
}
