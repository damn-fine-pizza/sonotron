#include "piano_roll_edit_ops.hpp"

#include <algorithm>
#include <string>

#include "brain_session.hpp"

namespace sonotron {

namespace {

// Composes the full-restate `track step <idx> <n> <note> <vel> <gate>
// prob=<p> ratchet=<r> micro=<m> tie=<on|off>` wire line -- ALWAYS emits
// every lock token explicitly, never conditionally: a conditional-emit
// discipline ("only when non-neutral") is one refactor away from an
// omission bug; always-emit removes that whole class of mistake, at the
// cost of a slightly longer wire line every time (irrelevant on a UDS/
// in-process command channel).
std::string compose_track_step_command(int track_idx, std::size_t step_index,
                                       const StepPatternStep& s) {
  return "track step " + std::to_string(track_idx) + " " + std::to_string(step_index + 1) + " " +
         std::to_string(static_cast<int>(s.note)) + " " + std::to_string(static_cast<int>(s.vel)) +
         " " + std::to_string(s.gate) + " prob=" + std::to_string(static_cast<int>(s.probability)) +
         " ratchet=" + std::to_string(static_cast<int>(s.ratchet)) +
         " micro=" + std::to_string(static_cast<int>(s.micro)) + " tie=" + (s.tie ? "on" : "off");
}

}  // namespace

bool write_step_full_restate(StepPatternModel& track, BrainSession* session, int track_idx,
                             std::size_t step_index, const StepPatternStep& step) {
  if (!track.set_step(step_index, step.note, step.vel, step.gate, step.probability, step.ratchet,
                      step.micro, step.tie)) {
    return false;
  }
  if (session != nullptr && track_idx >= 0) {
    session->send(compose_track_step_command(track_idx, step_index, step));
  }
  return true;
}

void clear_step_full_restate(StepPatternModel& track, BrainSession* session, int track_idx,
                             std::size_t step_index) {
  track.clear_step(step_index);
  if (session != nullptr && track_idx >= 0) {
    session->send("track step " + std::to_string(track_idx) + " " + std::to_string(step_index + 1) +
                  " clear");
  }
}

bool add_note(StepPatternModel& track, BrainSession* session, int track_idx, std::size_t step_index,
              std::uint8_t note, std::uint8_t vel, std::uint16_t gate) {
  StepPatternStep s;
  s.note = note;
  s.vel = vel;
  s.gate = gate;
  return write_step_full_restate(track, session, track_idx, step_index, s);
}

void delete_note(StepPatternModel& track, BrainSession* session, int track_idx,
                 std::size_t step_index) {
  clear_step_full_restate(track, session, track_idx, step_index);
}

bool repitch_note(StepPatternModel& track, BrainSession* session, int track_idx,
                  std::size_t step_index, std::uint8_t new_note) {
  StepPatternStep s = track.step(step_index);
  s.note = new_note;
  return write_step_full_restate(track, session, track_idx, step_index, s);
}

bool move_note(StepPatternModel& track, BrainSession* session, int track_idx,
               std::size_t from_index, std::size_t to_index) {
  if (from_index == to_index) {
    return false;
  }
  const StepPatternStep s = track.step(from_index);
  if (s.vel == 0) {
    return false;
  }
  clear_step_full_restate(track, session, track_idx, from_index);
  return write_step_full_restate(track, session, track_idx, to_index, s);
}

std::size_t tied_run_start(const StepPatternModel& track, std::size_t step_index) {
  std::size_t rs = step_index;
  while (rs > 0) {
    const StepPatternStep& prev = track.step(rs - 1);
    const StepPatternStep& cur = track.step(rs);
    if (!prev.tie || prev.vel == 0 || prev.note != cur.note) {
      break;
    }
    --rs;
  }
  return rs;
}

std::size_t tied_run_last(const StepPatternModel& track, std::size_t run_start,
                          std::size_t length) {
  if (length == 0) {
    return run_start;
  }
  std::size_t last = run_start;
  const StepPatternStep start_step = track.step(run_start);
  while (last + 1 < length) {
    const StepPatternStep& cur = track.step(last);
    const StepPatternStep& next = track.step(last + 1);
    if (!cur.tie || next.vel == 0 || next.note != start_step.note) {
      break;
    }
    ++last;
  }
  return last;
}

void resize_note_run(StepPatternModel& track, BrainSession* session, int track_idx,
                     std::size_t anchor, std::size_t old_last, std::size_t new_last,
                     std::uint8_t note, std::uint8_t vel, std::uint16_t release_gate) {
  const StepPatternStep anchor_step = track.step(anchor);
  const std::size_t run_end = std::max(old_last, new_last);
  for (std::size_t i = anchor; i <= run_end && i < kStepPatternMaxSteps; ++i) {
    if (i > new_last) {
      // Part of the OLD run being shrunk away -- clear it back to empty
      // rather than leaving an orphaned tie=true pointing past the new end.
      clear_step_full_restate(track, session, track_idx, i);
      continue;
    }
    StepPatternStep s;
    s.note = note;
    s.vel = vel;
    const bool is_release = (i == new_last);
    s.tie = !is_release;
    // Interior/link steps still need vel>0 => gate!=0 to satisfy
    // StepPatternModel::set_step's own validation, but their own gate is
    // never read by the scheduler once tied (arrangrr::Timeline::emit_step
    // reads only the run's LAST step's gate for the note-off delay) --
    // kStepPatternTicksPerStep is an inert placeholder, not a meaningful
    // duration.
    s.gate = is_release ? release_gate : static_cast<std::uint16_t>(kStepPatternTicksPerStep);
    if (i == anchor) {
      // arrangrr::Timeline::on_tick consults ONLY the run-start step's own
      // probability/ratchet/micro for the whole run -- must survive a
      // resize unchanged, regardless of whether the anchor ends up the
      // run's sole step or its start link.
      s.probability = anchor_step.probability;
      s.ratchet = anchor_step.ratchet;
      s.micro = anchor_step.micro;
    } else {
      s.probability = 100;
      s.ratchet = 1;
      s.micro = 0;
    }
    write_step_full_restate(track, session, track_idx, i, s);
  }
}

}  // namespace sonotron
