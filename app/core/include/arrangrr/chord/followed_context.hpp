#pragma once

#include <cstdint>

#include "arrangrr/chord/theory.hpp"

// The followed harmonic context — the single owner of the arranger-followed
// chord (`current`, read every tick by the arranger's NTT resolution, D24) and
// the staged next chord (`next`, the shift-quantized preview, D53).
//
// Before this type the two ChordState cells (`m_state`/`m_pending`) had four
// un-arbitrated writers across three layers (live detect, the ChordSequencer,
// manual `chord play`, and two lifecycle handlers), and the reset-vs-persist
// policy lived inline in unrelated command handlers. That diffuse ownership was
// a single defect surfacing as three symptoms (clobber on transport-start,
// reset on style change, self-drift). FollowedContext makes those bugs
// impossible by construction: `current` changes ONLY through commit_now /
// commit_bar from a producer-staged value, and the lifecycle default
// (establish_default) is a NO-OP once any producer has set an explicit chord.
//
// The D47 chord-follow gate lives here too (one policy field), replacing the
// three duplicated `*_may_follow` predicates that used to be threaded as bools
// through sound()/play*(). A non-selected producer's stage/commit is a no-op.
//
// Core-portable and freestanding: two ChordState + one bool latch + two enums,
// no heap, no dependency, identical on host and arm-none-eabi.

namespace arrangrr {

// D47: which producer is allowed to UPDATE the followed chord context. kAuto
// keeps every producer writing (legacy last-writer-wins); the other values gate
// the update down to a single named source. Gating touches ONLY the followed
// context update — the other side effects (sounding a chord, advancing the
// sequencer, tracking the held detect set) always run.
//
// kLivePriority (the engine DEFAULT) is the arbitrated model: every
// producer MAY publish (like kAuto at the gate), but while a live chord is
// actively HELD the engine suppresses the ChordSequencer's own publish AND
// redirects its comping onto the live chord — so a held live chord always wins
// over a running sequencer, and the sequencer resumes driving the instant the
// keys are released. That held-vs-released decision is DYNAMIC engine state
// (the detector's held set), so it lives at the fire_chord_seq call site, not
// in this static gate; here kLivePriority simply admits every producer.
//
// ABI note: values are append-only (0..4 are stable, never reused); the wire id
// is Param::kChordFollow's `a`.
enum class ChordFollow : std::uint8_t {
  kAuto = 0,          // all three producers may steer (legacy last-writer-wins)
  kDetect = 1,        // only live piano->chord detection steers
  kSequencer = 2,     // only the ChordSequencer steers
  kManual = 3,        // only manual `chord play` steers
  kLivePriority = 4,  // engine default: live held chord beats a running sequencer
};

// The three producers that can publish the followed chord. The owner's gate
// reads this against the ChordFollow selector.
enum class Producer : std::uint8_t {
  kDetect = 0,     // live piano->chord detection (observe_chord_input)
  kSequencer = 1,  // the recorded ChordSequencer (fire_chord_seq)
  kManual = 2,     // typed `chord play`
};

class FollowedContext {
 public:
  // D47 gate configuration.
  constexpr void set_follow(ChordFollow follow) noexcept { m_follow = follow; }
  constexpr ChordFollow follow() const noexcept { return m_follow; }

  // Stages a producer's chord as the pending "next" chord (the shift-quantized
  // path, D53): it lands in the followed context at the next bar boundary via
  // commit_bar(). A no-op when the producer is not the D47-selected source.
  // Last-wins: a second stage before the bar overwrites the first. Sets the
  // explicit latch so a lifecycle default can no longer re-home over it.
  constexpr void stage(Producer who, const ChordState& chord) noexcept {
    if (!may_follow(who)) {
      return;
    }
    m_pending = chord;
    m_explicit = true;
  }

  // Immediate steer: writes `current` NOW (the default input path and the
  // time-aligned ChordSequencer). A no-op when the producer is not the
  // D47-selected source. Sets the explicit latch.
  constexpr void commit_now(Producer who, const ChordState& chord) noexcept {
    if (!may_follow(who)) {
      return;
    }
    m_state = chord;
    m_explicit = true;
  }

  // The ONLY quantized writer of `current`: promotes a staged chord (if any) at
  // the bar boundary and clears the pending slot. Safe to call every bar (a
  // no-op when nothing is staged).
  constexpr void commit_bar() noexcept {
    if (m_pending.valid) {
      m_state = m_pending;
      m_pending = ChordState{};
    }
  }

  // Establishes the home-key context — but ONLY when no producer has set an
  // explicit chord since the last reset. Lifecycle handlers (transport-start,
  // style-load) call this: a fresh band starts in the home key, yet an explicit
  // chord the user steered is never clobbered.
  constexpr void establish_default(const Key& key) noexcept {
    if (m_explicit) {
      return;
    }
    m_state = home_chord(key);
  }

  // The genuine new-song reset: forget the explicit latch and the staged chord,
  // then re-home. Distinct from establish_default — this DOES overwrite an
  // explicit chord, so it is only for a real "forget everything" event.
  constexpr void reset(const Key& key) noexcept {
    m_explicit = false;
    m_pending = ChordState{};
    m_state = home_chord(key);
  }

  // Drops any staged next chord without touching `current` or the latch (the
  // D53 "drop a staged shift chord on transport-start / style-load" step).
  constexpr void reset_pending() noexcept { m_pending = ChordState{}; }

  constexpr const ChordState& state() const noexcept { return m_state; }
  constexpr const ChordState& pending() const noexcept { return m_pending; }
  constexpr bool explicit_set() const noexcept { return m_explicit; }

 private:
  constexpr bool may_follow(Producer who) const noexcept {
    switch (m_follow) {
      case ChordFollow::kAuto:
        return true;
      case ChordFollow::kDetect:
        return who == Producer::kDetect;
      case ChordFollow::kSequencer:
        return who == Producer::kSequencer;
      case ChordFollow::kManual:
        return who == Producer::kManual;
      case ChordFollow::kLivePriority:
        // Every producer may publish; the live-vs-sequencer arbitration is
        // enforced at the engine's fire_chord_seq call site (it withholds the
        // sequencer's publish while a live chord is held), not in this gate.
        return true;
    }
    return true;
  }

  static constexpr ChordState home_chord(const Key& key) noexcept {
    return ChordState{.root_pc = key.root_pc,
                      .quality = theory::single_finger_quality(key, key.root_pc),
                      .valid = true};
  }

  ChordState m_state{};     // `current`: the chord the band follows this bar
  ChordState m_pending{};   // `next`: the shift-staged chord, committed at the bar
  bool m_explicit = false;  // a real producer has set a context since the reset
  ChordFollow m_follow = ChordFollow::kLivePriority;  // D47 gate; live-priority default
};

}  // namespace arrangrr
