// Torquato QA regression pin (Phase-5 Item #9, docs/phase5-design-reviews.md
// "Pad/Scene live -> Performance"): recalling a Performance that captured a
// CONCRETE, already-validated builtin style_id silently degrades the LIVE
// Arranger::style_id() to 0xFFFF ("unknown/compiled") instead of restoring
// it. Currently RED -- pins an open, unfixed defect in
// Engine::apply_performance (src/engine.cpp); do not silence it. See
// test_performance.cpp for the surrounding GREEN capture/recall coverage
// (every OTHER field of a Performance genuinely round-trips; only this one
// metadata field is broken).
//
// Root cause: Arranger::style_id() (arranger/arranger.hpp) is set to a
// concrete builtin table index ONLY by Arranger::load(builtin_index) --
// Arranger::request_style(const Style*, ...), used by BOTH the live
// `style switch` verb and Engine::apply_performance's own recall, only ever
// receives a raw Style* (never the table index) and therefore ALWAYS resets
// m_style_id to 0xFFFF on its immediate branch (see request_style's own
// comment: "a pointer-based switch has no known builtin index" -- Corelli
// fix #1). That is correct and by design for `style switch`, which never had
// an index to preserve in the first place. But Engine::apply_performance
// DOES already hold a concrete, validated index right there
// (perf.style_id, checked < styles::kBuiltinCount by validate_performance)
// and still routes it through the SAME index-losing request_style() call
// instead of a path that preserves the known index (e.g. Arranger::load()
// followed by a section request, since apply_performance always applies
// immediately). The bug is in apply_performance's OWN choice of Arranger
// entry point, not in Arranger itself.
//
// Impact beyond the metadata itself: capture_performance() reads the LIVE
// style_id, so a `perf store` issued right after a `perf recall` silently
// captures style_id == 0xFFFF ("keep current style") even when a concrete
// style IS actually loaded -- a second-order defect where one recall poisons
// every later capture until the next explicit `style load`.

#include "arrangrr/engine.hpp"
#include "arrangrr/perf/performance.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "test.hpp"
#include "test_harness.hpp"

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 512>;

struct Band {
  test::TestEngine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0,
           std::uint16_t idx = 0, Boundary boundary = Boundary::kImmediate,
           std::uint8_t n_bars = 1) {
    Command command;
    command.op = Op::kDo;
    command.boundary = boundary;
    command.param = p;
    command.idx = idx;
    command.n_bars = n_bars;
    command.a = a;
    command.b = b;
    command.c = c;
    e.push_command(command, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
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
};

void test_perf_recall_loses_concrete_style_id_metadata() {
  Band b;
  b.cmd(Param::kStyleLoad, 0);  // "basic" -> arranger().style_id() == 0, concrete
  CHECK(b.e.arranger().style_id() == 0);
  b.cmd(Param::kPerformanceStore, 0, 0, 0, /*idx=*/0);  // captures style_id == 0 (concrete)

  b.cmd(Param::kStyleLoad, 1);  // mutate away: style_id == 1 ("pop"), concrete
  CHECK(b.e.arranger().style_id() == 1);

  b.ev.clear();
  b.cmd(Param::kPerformanceRecall, 0, 0, 0, /*idx=*/0);
  CHECK(b.warns() == 0);
  // EXPECTED: the recalled Performance's captured style_id (0, "basic") is
  // restored exactly, matching the pre-store value asserted above.
  // ACTUAL (as shipped): Engine::apply_performance -> Arranger::request_style
  // unconditionally zeroes it to 0xFFFF, so this fails.
  //   FAIL test_performance_style_id_regression.cpp: b.e.arranger().style_id() == 0
  CHECK(b.e.arranger().style_id() == 0);
}

// Second-order consequence: a `perf store` issued right after a `perf
// recall` of a CONCRETE style captures 0xFFFF ("keep current") instead of
// the style that is ACTUALLY loaded -- one recall silently poisons every
// later capture, until the next explicit `style load`.
void test_perf_store_after_recall_captures_poisoned_style_id() {
  Band b;
  b.cmd(Param::kStyleLoad, 0);                          // "basic", concrete
  b.cmd(Param::kPerformanceStore, 0, 0, 0, /*idx=*/0);  // slot 0: captures style_id == 0

  b.cmd(Param::kStyleLoad, 1);                           // "pop", concrete
  b.cmd(Param::kPerformanceRecall, 0, 0, 0, /*idx=*/0);  // recall slot 0: live style becomes
                                                         // "basic" (index 0) AGAIN, but the
                                                         // request_style path zeroes style_id
                                                         // metadata to 0xFFFF (the defect above)

  b.cmd(Param::kPerformanceStore, 0, 0, 0, /*idx=*/1);  // slot 1: capture the CURRENT rig now

  // EXPECTED: the live style genuinely IS "basic" (index 0) at this point (the
  // recall above landed it), so slot 1 should capture style_id == 0.
  // ACTUAL (as shipped): the live style_id metadata was poisoned to 0xFFFF by
  // the recall, so slot 1 wrongly captures "keep current" instead of the
  // concrete index that is actually loaded.
  const Performance* p1 = b.e.performances().get(1);
  CHECK(p1 != nullptr);
  CHECK(p1->style_id == 0);
}

}  // namespace

int main() {
  test_perf_recall_loses_concrete_style_id_metadata();
  test_perf_store_after_recall_captures_poisoned_style_id();
  return arrangrr::test::failures();
}
