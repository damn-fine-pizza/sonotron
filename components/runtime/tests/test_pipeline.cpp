#include "runtime/pipeline.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "runtime/runtime.hpp"
#include "test.hpp"

// Phase 4a/4d (docs/design/phase4-execution-plan.md, orchestrator-pipeline-
// extraction.md §16.3/§16.7): the Pipeline composite unit tests.
//
// test_one_stage_pipeline_is_transparent: proves `Pipeline<Engine>` is a
// byte-identical, transparent wrapper against driving `Engine` directly
// through `Runtime<Engine, N>` -- the Phase-4a gate itself, at unit scope
// (the 18 golden tests prove the same thing end-to-end). Updated for 4d's
// heterogeneous, factory-based Pipeline construction contract (§16.9 point
// 5) and Engine's new `FollowedContext&`/`ChorddetStage&` ctor params
// (both sides share the SAME externally-owned instances so the comparison
// stays apples-to-apples).
//
// test_two_stage_fixed_order: a pure mechanism test with fake stages,
// independent of any real Accompany stage, proving (a) `on_tick` fires
// every declared sub-stage in declared order, same ctx/sink, and (b)
// `flush` delegates SOLELY to the last declared stage (Corelli's
// correction #4, §16.9 point 4).

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 128>;

bool same_event(const OutEvent& a, const OutEvent& b) {
  return a.kind == b.kind && a.port == b.port && a.tick == b.tick && a.code == b.code &&
         a.msg.status == b.msg.status && a.msg.d1 == b.msg.d1 && a.msg.d2 == b.msg.d2;
}

void test_one_stage_pipeline_is_transparent() {
  FollowedContext direct_followed;
  ChorddetStage<kMaxPorts> direct_chorddet(direct_followed);
  runtime::Runtime<Engine, kSchedulerCapacity> direct(direct_followed, direct_chorddet);

  FollowedContext piped_followed;
  ChorddetStage<kMaxPorts> piped_chorddet(piped_followed);
  runtime::Runtime<runtime::Pipeline<Engine>, kSchedulerCapacity> piped(
      [&](auto& sched, auto& transport) {
        return Engine(sched, transport, piped_followed, piped_chorddet);
      });

  Events direct_events;
  Events piped_events;
  auto direct_sink = [&](const OutEvent& o) { CHECK(direct_events.push_back(o)); };
  auto piped_sink = [&](const OutEvent& o) { CHECK(piped_events.push_back(o)); };

  // Mirrors production usage (hostrt::Shell calls `m_engine.push_command(...)`
  // directly, never `Runtime::push_command` -- see shell.hpp): reach the
  // stage through the SAME accessor Shell now uses, `.stage().stage<0>()`.
  Command mask;
  mask.op = Op::kSet;
  mask.param = Param::kClockOutMask;
  mask.a = 0b0001;  // port 0
  direct.stage().push_command(mask, direct_sink);
  piped.stage().stage<0>().push_command(mask, piped_sink);

  Command start;
  start.param = Param::kTransportStart;
  direct.stage().push_command(start, direct_sink);
  piped.stage().stage<0>().push_command(start, piped_sink);

  direct.advance_ticks(80, direct_sink);
  piped.advance_ticks(80, piped_sink);

  CHECK(direct_events.size() == piped_events.size());
  for (std::size_t i = 0; i < direct_events.size() && i < piped_events.size(); ++i) {
    CHECK(same_event(direct_events[i], piped_events[i]));
  }

  // The indirection every composition point now needs to reach the sole
  // declared stage: `Pipeline<Engine>::stage<0>()` returns the same `Engine`
  // surface `Runtime<Engine,N>::stage()` returns directly.
  CHECK(piped.stage().stage<0>().now() == direct.stage().now());
}

// Fake, Accompany-independent stages: each records a tag into a shared log
// on `on_tick`/`flush` so the test can assert declared order and flush
// delegation without any real chorddet/arrangrr stage existing yet.
class LoggingStageA {
 public:
  explicit LoggingStageA(std::vector<std::string>& log) : m_log(&log) {}

  template <typename SinkT>
  void on_tick(const runtime::StageContext&, SinkT) {
    m_log->push_back("A:tick");
  }
  template <typename SinkT>
  void flush(SinkT) {
    m_log->push_back("A:flush");
  }

 private:
  std::vector<std::string>* m_log;
};

class LoggingStageB {
 public:
  explicit LoggingStageB(std::vector<std::string>& log) : m_log(&log) {}

  template <typename SinkT>
  void on_tick(const runtime::StageContext&, SinkT) {
    m_log->push_back("B:tick");
  }
  template <typename SinkT>
  void flush(SinkT) {
    m_log->push_back("B:flush");
  }

 private:
  std::vector<std::string>* m_log;
};

void test_two_stage_fixed_order() {
  std::vector<std::string> log;
  int dummy_scheduler = 0;
  int dummy_transport = 0;
  runtime::Pipeline<LoggingStageA, LoggingStageB> pipeline(
      dummy_scheduler, dummy_transport, [&](auto&, auto&) { return LoggingStageA(log); },
      [&](auto&, auto&, auto&) { return LoggingStageB(log); });

  const runtime::StageContext ctx{.now = 0};
  auto sink = [](int) {};  // sink type is never named by Pipeline; any callable works
  pipeline.on_tick(ctx, sink);
  pipeline.on_tick(ctx, sink);
  pipeline.flush(sink);

  const std::vector<std::string> expected = {"A:tick", "B:tick", "A:tick", "B:tick", "B:flush"};
  CHECK(log == expected);
}

}  // namespace

int main() {
  test_one_stage_pipeline_is_transparent();
  test_two_stage_fixed_order();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_pipeline: all OK\n");
  }
  return arrangrr::test::failures();
}
