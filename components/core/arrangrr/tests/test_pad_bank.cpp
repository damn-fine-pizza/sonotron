// Unit tests for PadEngine (Phase-5 Item #9, docs/phase5-design-reviews.md
// "Pad/Scene live -> Performance"): pure bookkeeping -- assign()/get()/
// runtime() -- mirroring ClipMatrix's own scope tripwire (no dispatch logic
// lives here; Engine::fire_pad, exercised by test_pad.cpp, owns that).

#include "arrangrr/pad/pad_bank.hpp"

#include "arrangrr/config.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;

Pad sample_pad() {
  Pad p;
  p.type = PadType::kPhrase;
  p.mode = PadMode::kHold;
  p.sync = Boundary::kNextNBars;
  p.pitch = PadPitch::kTransposeWithChord;
  p.n_bars = 4;
  p.dest_port = 2;
  p.dest_channel = 9;
  p.source_idx = 17;
  p.source_aux = 3;
  return p;
}

void test_assign_get_round_trip() {
  PadEngine pads;
  CHECK(pads.assign(5, sample_pad()));
  const Pad* got = pads.get(5);
  CHECK(got != nullptr);
  CHECK(got->type == PadType::kPhrase);
  CHECK(got->mode == PadMode::kHold);
  CHECK(got->sync == Boundary::kNextNBars);
  CHECK(got->pitch == PadPitch::kTransposeWithChord);
  CHECK(got->n_bars == 4);
  CHECK(got->dest_port == 2);
  CHECK(got->dest_channel == 9);
  CHECK(got->source_idx == 17);
  CHECK(got->source_aux == 3);
}

void test_out_of_range_flat_id() {
  PadEngine pads;
  CHECK(!pads.assign(kMaxPads, sample_pad()));        // one past the end
  CHECK(!pads.assign(kMaxPads + 100, sample_pad()));  // far past the end
  CHECK(pads.get(kMaxPads) == nullptr);
  CHECK(pads.runtime(kMaxPads) == nullptr);
  const PadEngine& cpads = pads;
  CHECK(cpads.get(kMaxPads) == nullptr);
  CHECK(cpads.runtime(kMaxPads) == nullptr);
}

void test_boundary_ids_valid() {
  PadEngine pads;
  CHECK(pads.assign(0, sample_pad()));  // first slot
  CHECK(pads.get(0) != nullptr);
  CHECK(pads.assign(kMaxPads - 1, sample_pad()));  // last valid slot
  CHECK(pads.get(kMaxPads - 1) != nullptr);
}

void test_unassigned_pad_defaults_to_none() {
  PadEngine pads;
  const Pad* p = pads.get(3);
  CHECK(p != nullptr);
  CHECK(p->type == PadType::kNone);  // never assign()'d -- inert default
  const PadRuntime* rt = pads.runtime(3);
  CHECK(rt != nullptr && !rt->on);
}

void test_runtime_on_off_state() {
  PadEngine pads;
  CHECK(pads.assign(1, sample_pad()));
  PadRuntime* rt = pads.runtime(1);
  CHECK(rt != nullptr && !rt->on);
  rt->on = true;
  CHECK(pads.runtime(1)->on);
  const PadEngine& cpads = pads;
  CHECK(cpads.runtime(1)->on);  // const overload observes the same state
}

void test_reassign_drops_stale_runtime_state() {
  PadEngine pads;
  CHECK(pads.assign(2, sample_pad()));
  PadRuntime* rt = pads.runtime(2);
  rt->on = true;
  CHECK(pads.assign(2, sample_pad()));  // re-assign the SAME slot
  CHECK(!pads.runtime(2)->on);          // stale on/off state is dropped
}

}  // namespace

int main() {
  test_assign_get_round_trip();
  test_out_of_range_flat_id();
  test_boundary_ids_valid();
  test_unassigned_pad_defaults_to_none();
  test_runtime_on_off_state();
  test_reassign_drops_stale_runtime_state();
  return arrangrr::test::failures();
}
