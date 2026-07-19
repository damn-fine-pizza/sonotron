// Unit tests for piano_roll_edit_ops.hpp (Sequence Edit Phase-2, docs/
// proposals/seqedit-piano-roll-phase2-design.md §3/§7): the pure step-array
// algebra + full-restate `track step ...` wire composition every gesture in
// seqedit_panel.cpp's interactive piano-roll canvas ends in. No ImGui, no
// GLFW, no core -- these operate purely on StepPatternModel + a spy
// BrainSession (same SpyBrainSession pattern test_grid_cell_preview_vs_
// seqedit_ui_automation.cpp already uses).
//
// The FOCUSED test the design doc itself flags as the single trickiest
// piece is test_resize_extends_run_into_tie_chain and its sibling
// test_resize_shrinks_run_and_clears_orphaned_steps below: author a tied
// run, resize it, and assert the resulting StepPatternStep[] run shape --
// not just an end-to-end click test.

#include "src/piano_roll_edit_ops.hpp"

#include <string>
#include <vector>

#include "src/brain_session.hpp"
#include "src/step_pattern_model.hpp"
#include "test.hpp"

using sonotron::add_note;
using sonotron::apply_velocity_delta;
using sonotron::BrainEvent;
using sonotron::BrainSession;
using sonotron::BrainSnapshot;
using sonotron::clear_step_full_restate;
using sonotron::delete_note;
using sonotron::move_note;
using sonotron::repitch_note;
using sonotron::resize_note_run;
using sonotron::StepPatternModel;
using sonotron::StepPatternStep;
using sonotron::tied_run_last;
using sonotron::tied_run_start;
using sonotron::write_step_full_restate;

namespace {

class SpyBrainSession : public BrainSession {
 public:
  void send(std::string_view command_line) override { sent.emplace_back(command_line); }
  void poll(std::vector<BrainEvent>&) override {}
  const BrainSnapshot& snapshot() const override { return m_snapshot; }
  Status status() const override { return Status::kConnected; }

  std::vector<std::string> sent;

 private:
  BrainSnapshot m_snapshot;
};

// --- Full-restate gotcha: the load-bearing correctness requirement ---

void test_write_step_full_restate_sends_every_lock_field() {
  StepPatternModel track;
  SpyBrainSession session;
  StepPatternStep s;
  s.note = 64;
  s.vel = 90;
  s.gate = 200;
  s.probability = 70;
  s.ratchet = 3;
  s.micro = 12;
  s.tie = true;
  CHECK(write_step_full_restate(track, &session, /*track_idx=*/2, /*step_index=*/5, s));
  CHECK(track.step(5).note == 64);
  CHECK(track.step(5).probability == 70);
  CHECK(track.step(5).tie);
  CHECK(session.sent.size() == 1);
  const std::string& cmd = session.sent[0];
  CHECK(cmd == "track step 2 6 64 90 200 prob=70 ratchet=3 micro=12 tie=on");
}

void test_write_step_full_restate_nullptr_session_updates_local_echo_only() {
  StepPatternModel track;
  StepPatternStep s;
  s.note = 60;
  s.vel = 100;
  s.gate = 120;
  CHECK(write_step_full_restate(track, nullptr, 0, 0, s));
  CHECK(track.step(0).vel == 100);
}

void test_repitch_preserves_existing_locks() {
  // Pins the design doc's own "full-restate gotcha": a re-pitch that reads
  // the CURRENT step and re-supplies every lock field must never silently
  // reset a non-default probability/ratchet/micro/tie to neutral.
  StepPatternModel track;
  SpyBrainSession session;
  track.set_step(3, 60, 100, 120, /*probability=*/55, /*ratchet=*/4, /*micro=*/30, /*tie=*/true);
  CHECK(repitch_note(track, &session, 1, 3, /*new_note=*/67));
  const StepPatternStep& s = track.step(3);
  CHECK(s.note == 67);
  CHECK(s.vel == 100);
  CHECK(s.gate == 120);
  CHECK(s.probability == 55);
  CHECK(s.ratchet == 4);
  CHECK(s.micro == 30);
  CHECK(s.tie);
  CHECK(session.sent.back() == "track step 1 4 67 100 120 prob=55 ratchet=4 micro=30 tie=on");
}

// --- Add / delete / move ---

void test_add_and_delete_note() {
  StepPatternModel track;
  SpyBrainSession session;
  CHECK(add_note(track, &session, 0, 2, 62, 90, 100));
  CHECK(track.step(2).vel == 90);
  delete_note(track, &session, 0, 2);
  CHECK(track.step(2).vel == 0);
  CHECK(session.sent.back() == "track step 0 3 clear");
}

void test_move_note_clears_old_and_writes_new_preserving_locks() {
  StepPatternModel track;
  SpyBrainSession session;
  track.set_step(1, 50, 80, 150, /*probability=*/40, /*ratchet=*/2, /*micro=*/5, /*tie=*/false);
  CHECK(move_note(track, &session, 0, 1, 6));
  CHECK(track.step(1).vel == 0);
  const StepPatternStep& moved = track.step(6);
  CHECK(moved.note == 50);
  CHECK(moved.vel == 80);
  CHECK(moved.probability == 40);
  CHECK(moved.ratchet == 2);
  CHECK(moved.micro == 5);
  CHECK(session.sent.size() == 2);
  CHECK(session.sent[0] == "track step 0 2 clear");
}

void test_move_note_same_index_is_a_no_op() {
  StepPatternModel track;
  track.set_step(4, 60, 100, 120);
  CHECK(!move_note(track, nullptr, 0, 4, 4));
  CHECK(track.step(4).vel == 100);
}

void test_move_note_from_empty_step_is_a_no_op() {
  StepPatternModel track;
  CHECK(!move_note(track, nullptr, 0, 4, 5));
  CHECK(track.step(5).vel == 0);
}

// --- Velocity nudge ---

void test_apply_velocity_delta_clamps_and_preserves_locks() {
  StepPatternModel track;
  SpyBrainSession session;
  track.set_step(0, 60, 100, 120, /*probability=*/90, /*ratchet=*/2, /*micro=*/8, /*tie=*/false);
  CHECK(apply_velocity_delta(track, &session, 0, 0, 40));
  CHECK(track.step(0).vel == 127);  // clamped at the ceiling
  CHECK(track.step(0).probability == 90);
  CHECK(apply_velocity_delta(track, &session, 0, 0, -200));
  CHECK(track.step(0).vel == 1);  // clamped at the floor, never 0 (never silences via this gesture)
}

void test_apply_velocity_delta_no_op_on_empty_step() {
  StepPatternModel track;
  CHECK(!apply_velocity_delta(track, nullptr, 0, 3, 10));
  CHECK(track.step(3).vel == 0);
}

// --- Tied-run traversal helpers ---

void test_tied_run_start_and_last_walk_a_plain_note() {
  StepPatternModel track;
  track.set_step(2, 60, 100, 120);
  CHECK(tied_run_start(track, 2) == 2);
  CHECK(tied_run_last(track, 2, 16) == 2);
}

void test_tied_run_start_and_last_walk_an_authored_run() {
  // Author steps 2..4 as one tied run (2,3 tie=true; 4 the release).
  StepPatternModel track;
  track.set_step(2, 60, 100, 240, 100, 1, 0, /*tie=*/true);
  track.set_step(3, 60, 100, 240, 100, 1, 0, /*tie=*/true);
  track.set_step(4, 60, 100, 90, 100, 1, 0, /*tie=*/false);
  CHECK(tied_run_start(track, 2) == 2);
  CHECK(tied_run_start(track, 3) == 2);
  CHECK(tied_run_start(track, 4) == 2);
  CHECK(tied_run_last(track, 2, 16) == 4);
}

// --- Resize / tie-chain (the design doc's own "trickiest piece") ---

// Simple case (design doc §3's "just grow gate"): a single, non-tied note
// resized without crossing into a further step -- resize_note_run with
// anchor == old_last == new_last degrades to a plain gate change, no tie
// chain created.
void test_resize_single_step_just_grows_gate() {
  StepPatternModel track;
  SpyBrainSession session;
  track.set_step(0, 60, 100, 100, /*probability=*/80, /*ratchet=*/1, /*micro=*/0, /*tie=*/false);
  resize_note_run(track, &session, 0, /*anchor=*/0, /*old_last=*/0, /*new_last=*/0,
                  /*note=*/60, /*vel=*/100, /*release_gate=*/200);
  const StepPatternStep& s = track.step(0);
  CHECK(s.note == 60);
  CHECK(s.vel == 100);
  CHECK(s.gate == 200);
  CHECK(!s.tie);
  CHECK(s.probability == 80);  // the anchor's own lock survives a resize
  CHECK(session.sent.size() == 1);
}

// The focused test: author a 2-step tied run, drag-resize it to extend
// across 2 MORE (previously empty) steps -- assert the resulting
// StepPatternStep[] run shape matches exactly what arrangrr::Timeline::
// emit_step/suppressed_by_tie (timeline.hpp) expect: every absorbed step
// tie=true/vel>0/same note, the anchor's own probability/ratchet/micro
// preserved, and the new LAST step carrying the real release gate with
// tie=false.
void test_resize_extends_run_into_tie_chain() {
  StepPatternModel track;
  SpyBrainSession session;
  // Author the ORIGINAL run: steps 0-1, anchor keeps a non-default lock.
  track.set_step(0, 67, 90, 240, /*probability=*/60, /*ratchet=*/2, /*micro=*/15, /*tie=*/true);
  track.set_step(1, 67, 90, 50, /*probability=*/100, /*ratchet=*/1, /*micro=*/0, /*tie=*/false);
  const std::size_t anchor = tied_run_start(track, 0);
  const std::size_t old_last = tied_run_last(track, anchor, 8);
  CHECK(anchor == 0);
  CHECK(old_last == 1);

  // Extend the run to end at step 3 (absorbing steps 1 and 2, releasing at
  // step 3 with 100 ticks).
  resize_note_run(track, &session, /*track_idx=*/0, anchor, old_last, /*new_last=*/3,
                  /*note=*/67, /*vel=*/90, /*release_gate=*/100);

  CHECK(track.step(0).tie);
  CHECK(track.step(0).note == 67);
  CHECK(track.step(0).vel > 0);
  CHECK(track.step(0).probability == 60);  // anchor's own lock: MUST survive
  CHECK(track.step(0).ratchet == 2);
  CHECK(track.step(0).micro == 15);

  CHECK(track.step(1).tie);
  CHECK(track.step(1).note == 67);
  CHECK(track.step(1).vel > 0);  // nonzero is load-bearing: a zero vel breaks the chain

  CHECK(track.step(2).tie);
  CHECK(track.step(2).note == 67);
  CHECK(track.step(2).vel > 0);

  CHECK(!track.step(3).tie);
  CHECK(track.step(3).note == 67);
  CHECK(track.step(3).vel == 90);
  CHECK(track.step(3).gate == 100);

  // Re-derive the run from scratch via the traversal helpers: must agree.
  CHECK(tied_run_start(track, 3) == 0);
  CHECK(tied_run_last(track, 0, 8) == 3);
}

// The shrink edge case (design doc's own "no orphaned tie flags on
// discarded steps"): shrinking an existing 4-step run back down to 2 steps
// must clear the discarded steps back to empty, not leave a dangling
// tie=true past the new end.
void test_resize_shrinks_run_and_clears_orphaned_steps() {
  StepPatternModel track;
  SpyBrainSession session;
  track.set_step(0, 67, 90, 240, 100, 1, 0, /*tie=*/true);
  track.set_step(1, 67, 90, 240, 100, 1, 0, /*tie=*/true);
  track.set_step(2, 67, 90, 240, 100, 1, 0, /*tie=*/true);
  track.set_step(3, 67, 90, 60, 100, 1, 0, /*tie=*/false);
  const std::size_t anchor = tied_run_start(track, 3);
  const std::size_t old_last = tied_run_last(track, anchor, 8);
  CHECK(anchor == 0);
  CHECK(old_last == 3);

  resize_note_run(track, &session, 0, anchor, old_last, /*new_last=*/1, 67, 90,
                  /*release_gate=*/180);

  CHECK(track.step(0).tie);
  CHECK(!track.step(1).tie);
  CHECK(track.step(1).vel == 90);
  CHECK(track.step(1).gate == 180);
  // Steps 2 and 3, discarded from the old run, are cleared -- no orphaned
  // tie flag left pointing past the new end.
  CHECK(track.step(2).vel == 0);
  CHECK(!track.step(2).tie);
  CHECK(track.step(3).vel == 0);
  CHECK(!track.step(3).tie);

  CHECK(tied_run_start(track, 1) == 0);
  CHECK(tied_run_last(track, 0, 8) == 1);
}

}  // namespace

int main() {
  test_write_step_full_restate_sends_every_lock_field();
  test_write_step_full_restate_nullptr_session_updates_local_echo_only();
  test_repitch_preserves_existing_locks();
  test_add_and_delete_note();
  test_move_note_clears_old_and_writes_new_preserving_locks();
  test_move_note_same_index_is_a_no_op();
  test_move_note_from_empty_step_is_a_no_op();
  test_apply_velocity_delta_clamps_and_preserves_locks();
  test_apply_velocity_delta_no_op_on_empty_step();
  test_tied_run_start_and_last_walk_a_plain_note();
  test_tied_run_start_and_last_walk_an_authored_run();
  test_resize_single_step_just_grows_gate();
  test_resize_extends_run_into_tie_chain();
  test_resize_shrinks_run_and_clears_orphaned_steps();
  return sonotron::test::failures();
}
