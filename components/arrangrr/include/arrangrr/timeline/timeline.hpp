#pragma once

#include <cstdint>

#include "arrangrr/common/function_ref.hpp"
#include "arrangrr/common/seeded_hash.hpp"  // D16 shared position hash (arrangrr::seeded_hash)
#include "arrangrr/common/static_vector.hpp"
#include "common/time.hpp"
#include "arrangrr/config.hpp"
#include "common/midi/message.hpp"

// The Living Timeline primitive (D10): Track = role + MIDI destination +
// per-track length (polymeter) + mute/solo, filled by gestures. M1 ships the
// minimal "write" gesture: one note per grid step on a fixed 16th-note grid.
// Chord-follow transforms (D24) plug in between step data and scheduling in
// later milestones.

namespace arrangrr {

// M1 grid: one step = one sixteenth = 240 scheduler ticks (960 PPQN / 4).
inline constexpr std::uint32_t kTicksPerStep = kPpqn / 4;

// Upper bound on per-step retriggers (the `ratchet` param-lock).
inline constexpr std::uint8_t kMaxRatchet = 8;

enum class TrackRole : std::uint8_t {
  kDrums = 0,
  kPerc = 1,
  kBass = 2,
  kChord1 = 3,
  kChord2 = 4,
  kPad = 5,
  kArp = 6,
  kPhrase = 7,
  kLead = 8,
  kCc = 9,
};

// One grid slot. vel == 0 means the slot is empty (a NoteOn with velocity 0
// would be a NoteOff anyway, so zero is unambiguous as "no event").
//
// The trailing four fields are Elektron-style per-step parameter locks (D16
// determinism, D27 integer micro-timing). They are POD and neutral by default:
// a Step{} with probability=100, ratchet=1, micro=0, tie=false reproduces the
// original one-note-per-step behaviour byte-for-byte. Live RAM cost is the
// whole pool: kMaxTracks x kMaxStepsPerTrack slots.
//
// `micro` is FORWARD-ONLY (0..127): a step is evaluated only at its own
// boundary tick and delays are relative to `now`, so a negative offset could
// never anticipate the beat — it would just clamp to 0. Bidirectional micro
// (anticipation / ahead-of-the-beat) needs one-step look-ahead in on_tick and
// is deliberately deferred; today `micro` is an honest lay-back (behind-the-
// beat push) only.
struct Step {
  std::uint8_t note = 0;
  std::uint8_t vel = 0;
  std::uint16_t gate = 0;          // sounding length in scheduler ticks
  std::uint8_t probability = 100;  // 0..100 % chance the step fires (100 = always)
  std::uint8_t ratchet = 1;        // evenly-spaced retriggers within the step (1..8)
  std::uint8_t micro = 0;          // forward micro-timing push in ticks (0..127, on+off together)
  bool tie = false;                // hold into the next step instead of retriggering
};
static_assert(sizeof(Step) == 8,
              "Step must stay 8 bytes: it is live RAM x kMaxTracks x kMaxStepsPerTrack");

struct Track {
  TrackRole role = TrackRole::kLead;
  std::uint8_t port = 0;
  std::uint8_t channel = 0;  // 0-based
  std::uint8_t length = 16;  // active steps; per-track => polymeter
  bool mute = false;
  bool solo = false;
  Step steps[kMaxStepsPerTrack]{};
};

class Timeline {
 public:
  // Schedules msg on `port` at `delay` ticks after the current stream tick.
  using ScheduleFn = FunctionRef<void(std::uint8_t port, TickOffset delay, const MidiMessage& msg)>;

  // Returns the new track index, or -1 when the pool is full.
  int add_track(TrackRole role, std::uint8_t port, std::uint8_t channel) noexcept {
    Track t;
    t.role = role;
    t.port = port;
    t.channel = static_cast<std::uint8_t>(channel & 0x0F);
    if (!m_tracks.push_back(t)) {
      return -1;
    }
    return static_cast<int>(m_tracks.size() - 1);
  }

  Track* track(std::size_t idx) noexcept {
    return idx < m_tracks.size() ? &m_tracks[idx] : nullptr;
  }
  const Track* track(std::size_t idx) const noexcept {
    return idx < m_tracks.size() ? &m_tracks[idx] : nullptr;
  }
  std::size_t track_count() const noexcept { return m_tracks.size(); }

  // The four param-lock arguments default to their neutral values, so every
  // existing 5-argument caller keeps the original behaviour unchanged.
  bool set_step(std::size_t idx, std::size_t step, std::uint8_t note, std::uint8_t vel,
                std::uint16_t gate, std::uint8_t probability = 100, std::uint8_t ratchet = 1,
                std::uint8_t micro = 0, bool tie = false) noexcept {
    Track* t = track(idx);
    if (t == nullptr || step >= kMaxStepsPerTrack || note > 127 || vel > 127) {
      return false;
    }
    // gate 0 on an audible step would land the NoteOff on the NoteOn's own
    // tick, and the D29 class order (off before on) turns that into a
    // guaranteed stuck note. The binary ABI is the product boundary (D26):
    // the invariant lives here, not in a host-side check.
    if (vel > 0 && gate == 0) {
      return false;
    }
    Step s;
    s.note = note;
    s.vel = vel;
    s.gate = gate;
    s.probability = probability > 100 ? 100 : probability;
    s.ratchet = ratchet < 1 ? 1 : (ratchet > kMaxRatchet ? kMaxRatchet : ratchet);
    s.micro = micro;
    s.tie = tie;
    t->steps[step] = s;
    return true;
  }

  bool set_length(std::size_t idx, std::size_t steps) noexcept {
    Track* t = track(idx);
    if (t == nullptr || steps == 0 || steps > kMaxStepsPerTrack) {
      return false;
    }
    t->length = static_cast<std::uint8_t>(steps);
    return true;
  }

  bool any_solo() const noexcept {
    for (const Track& t : m_tracks) {
      if (t.solo) {
        return true;
      }
    }
    return false;
  }

  // Fires the grid slots that fall on `transport_tick` (call once per tick
  // while the transport plays; also with tick 0 right after start).
  void on_tick(Tick transport_tick, ScheduleFn schedule) {
    if (transport_tick % kTicksPerStep != 0) {
      return;
    }
    const std::uint32_t global_step = transport_tick / kTicksPerStep;
    const bool solo_active = any_solo();
    for (std::size_t ti = 0; ti < m_tracks.size(); ++ti) {
      const Track& t = m_tracks[ti];
      if (t.mute || (solo_active && !t.solo)) {
        continue;
      }
      const Step& s = t.steps[global_step % t.length];
      if (s.vel == 0) {
        continue;
      }
      // Tie suppression (sustain-chain model, D42-adjacent). A maximal run of
      // tied steps on the SAME note is ONE sustained note: the run's START step
      // articulates once and owns a single note-off spanning the whole run;
      // every step absorbed into that run emits nothing. This step is absorbed
      // (and skipped) when an earlier same-note tied step reaches it — see
      // suppressed_by_tie. A run start, a different note, or a rest are never
      // absorbed and fall through to the normal firing path below.
      if (suppressed_by_tie(t, global_step)) {
        continue;
      }
      // Probability gate (D16): a seeded position hash keyed on track index and
      // global step position. 100 % never consults the hash, so a neutral step
      // is byte-identical to the original path; same position => same verdict.
      // NOTE: the seed is the track INDEX, so verdicts are stable only while the
      // track table is add-only. Removing a track would renumber the survivors
      // and shift every step's probability verdict — acceptable because tracks
      // are add-only in this milestone; revisit if track deletion lands.
      if (s.probability < 100 &&
          seeded_hash(static_cast<std::uint32_t>(ti), global_step) % 100u >= s.probability) {
        continue;
      }
      emit_step(t, static_cast<std::uint32_t>(global_step % t.length), schedule);
    }
  }

 private:
  // Forward micro-timing push (0..127): a lay-back added to the step-relative
  // delay. Non-negative by construction, so the absolute tick can never go
  // negative and the D29 off-before-on order is preserved (off = on + gate).
  static constexpr TickOffset shift_delay(TickOffset base, std::uint8_t micro) noexcept {
    return base + static_cast<TickOffset>(micro);
  }

  // Sustain-chain suppression: returns true when the step at `global_step` is
  // ABSORBED into a same-note tied run that started at an earlier step, so it
  // must emit nothing (the run start already owns the held note and its off).
  //
  // It walks backwards over consecutive same-note tied predecessors WITHIN the
  // current pattern iteration to find the run start. If no tied predecessor
  // reaches this step (walk stays put) the step is a run start or a plain step
  // and is not suppressed. Otherwise the step is ABSORBED into the run and is
  // always suppressed: the run's single probability verdict (D16) lives at the
  // start step's own gate in on_tick, so if the start fails it emits nothing and
  // every absorbed step stays silent too — the whole run is held-or-silent as
  // one. (Re-verdicting the absorbed step here, as an earlier version did, let
  // it re-fire when the start failed its roll, fragmenting the run.)
  //
  // Loop seam (deferred, deliberate): the walk stops at step index 0 and never
  // wraps past t.length, so a tie on the last pattern step does NOT carry across
  // the loop restart — the pattern re-articulates on step 0.
  static constexpr bool suppressed_by_tie(const Track& t, std::uint32_t global_step) noexcept {
    const std::uint32_t cur_idx = global_step % t.length;
    if (cur_idx == 0) {
      return false;  // loop seam: step 0 always re-articulates
    }
    const Step& cur = t.steps[cur_idx];
    std::uint32_t rs = cur_idx;
    while (rs > 0) {
      const Step& prev = t.steps[rs - 1];
      if (!prev.tie || prev.vel == 0 || prev.note != cur.note) {
        break;
      }
      --rs;
    }
    if (rs == cur_idx) {
      return false;  // no tied predecessor: run start or plain step
    }
    return true;  // absorbed into a same-note tied run: held or silent as one
  }

  // Emits one firing step at pattern index `cur_idx`. A tied run start sustains
  // one held note across the whole same-note run; otherwise `ratchet` evenly-
  // spaced micro-shifted note-on/off pairs (ratchet 1 = the original single
  // hit). The caller has already resolved suppression, so this is always the
  // articulating step.
  static void emit_step(const Track& t, std::uint32_t cur_idx, ScheduleFn& schedule) {
    const Step& s = t.steps[cur_idx];
    const MidiMessage on = MidiMessage::note_on(t.channel, s.note, s.vel);
    const MidiMessage off = MidiMessage::note_off(t.channel, s.note);

    if (s.tie) {
      // Forward-scan the maximal same-note tied run starting here. Absorb the
      // next step while the current step ties AND the next step repeats the same
      // audible note. The scan is bounded by t.length and STOPS at the loop seam
      // (never wraps past t.length): a tie on the last step does not carry over.
      std::uint32_t last = cur_idx;
      while (last + 1 < t.length && t.steps[last].tie && t.steps[last + 1].vel != 0 &&
             t.steps[last + 1].note == s.note) {
        ++last;
      }
      // One note-on at the run start (micro-shifted, as before); one note-off at
      // (start tick of the LAST step) + (that last step's gate), carried by the
      // same run-start micro so on and off move together and off > on always
      // (D29). A lone tie (no same-note successor: last == cur_idx) degrades to
      // exactly the normal single-hit shape — a tie only means something with a
      // same-note step to hold into.
      const TickOffset on_delay = shift_delay(0, s.micro);
      const std::uint32_t span =
          (last - cur_idx) * kTicksPerStep + static_cast<std::uint32_t>(t.steps[last].gate);
      const auto off_delay = static_cast<TickOffset>(on_delay + static_cast<TickOffset>(span));
      schedule(t.port, on_delay, on);
      schedule(t.port, off_delay, off);
      return;
    }

    // `ratchet` is already bounded to [1, kMaxRatchet] by set_step and the ABI
    // decode mask, so no re-clamp is needed here.
    const std::uint8_t ratchet = s.ratchet;
    if (ratchet <= 1) {
      const TickOffset on_delay = shift_delay(0, s.micro);
      schedule(t.port, on_delay, on);
      schedule(t.port, static_cast<TickOffset>(on_delay + static_cast<TickOffset>(s.gate)), off);
      return;
    }

    // Subdivide the step into `ratchet` slices; bound each sub-hit inside its
    // slice so consecutive retriggers never overlap (D29 stays clean). Integer
    // division front-aligns the hits: a ratchet that does not divide 240 (e.g.
    // 7 -> slice 34, last hit at 204) leaves a small tail gap before the next
    // step boundary. This is acceptable — the hits stay evenly spaced and the
    // step remains musically front-aligned.
    const std::uint32_t slice = kTicksPerStep / ratchet;
    std::uint16_t sub_gate = static_cast<std::uint16_t>(slice > s.gate ? s.gate : slice);
    if (sub_gate == 0) {
      sub_gate = 1;
    }
    for (std::uint8_t i = 0; i < ratchet; ++i) {
      const auto base = static_cast<TickOffset>(static_cast<std::uint32_t>(i) * slice);
      const TickOffset on_delay = shift_delay(base, s.micro);
      schedule(t.port, on_delay, on);
      schedule(t.port, static_cast<TickOffset>(on_delay + static_cast<TickOffset>(sub_gate)), off);
    }
  }

  StaticVector<Track, kMaxTracks> m_tracks;
};

}  // namespace arrangrr
