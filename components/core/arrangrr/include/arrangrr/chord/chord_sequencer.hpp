#pragma once

#include <cstdint>

#include "arrangrr/chord/chord_sequence.hpp"
#include "arrangrr/common/function_ref.hpp"

// ChordSequencer: bounded pool of sequences, one current. Records chord
// plays with stream timing (quantize-after on stop), and during playback
// fires resolved chords on transport ticks — degree -> concrete chord is
// re-derived in the sequence's CURRENT reference key (D28), which is what
// makes transpose musical instead of chromatic.

namespace arrangrr {

class ChordSequencer {
 public:
  // Fired chord during playback: resolved against the sequence key.
  // root_note anchors the voicing in octave 4 (C4=60 .. B4=71).
  using FireFn = FunctionRef<void(std::uint8_t root_note, ChordQuality quality, std::uint8_t degree,
                                  std::uint8_t velocity)>;
  using ReleaseFn = FunctionRef<void()>;

  // ---- pool ----------------------------------------------------------------
  int add_sequence(const Key& reference) noexcept {
    ChordSequence seq;
    seq.key = reference;
    if (!m_pool.push_back(seq)) {
      return -1;
    }
    m_current = m_pool.size() - 1;
    return static_cast<int>(m_current);
  }
  bool use(std::size_t idx) noexcept {
    if (idx >= m_pool.size()) {
      return false;
    }
    m_current = idx;
    return true;
  }
  ChordSequence* current() noexcept { return m_pool.empty() ? nullptr : &m_pool[m_current]; }
  const ChordSequence* current() const noexcept {
    return m_pool.empty() ? nullptr : &m_pool[m_current];
  }
  std::size_t count() const noexcept { return m_pool.size(); }
  // Which pool slot is current (Phase-5 Item #2: lets a caller -- Engine's
  // clip-stop path -- tell whether ITS sequence is the one actually playing
  // before stopping it, since this is a single-active-sequence machine, not
  // per-clip parallel playback).
  std::size_t current_index() const noexcept { return m_current; }

  // ---- recording (D13: the recorded half) ----------------------------------
  bool start_record(Tick now) noexcept {
    if (m_pool.empty() || m_playing) {
      return false;
    }
    m_recording = true;
    m_record_base = now;
    current()->clear();
    return true;
  }
  bool recording() const noexcept { return m_recording; }

  // Called for every live chord play while recording: closes the previous
  // step and opens a new one at the current stream tick.
  void capture(Tick now, std::int8_t degree, std::int8_t quality_ovr,
               std::uint8_t velocity) noexcept {
    if (!m_recording) {
      return;
    }
    ChordSequence* seq = current();
    const Tick rel = now - m_record_base;
    if (ChordStep* prev = seq->last(); prev != nullptr && prev->duration == 0) {
      prev->duration = rel - prev->start;
    }
    (void)seq->record(ChordStep{.start = rel,
                                .duration = 0,
                                .degree = degree,
                                .quality_ovr = quality_ovr,
                                .velocity = velocity});
  }

  // Stops recording; the open step is closed at `now`, then quantize-after.
  bool stop_record(Tick now, Tick grid = kTicksPerBar) noexcept {
    if (!m_recording) {
      return false;
    }
    m_recording = false;
    ChordSequence* seq = current();
    if (ChordStep* prev = seq->last(); prev != nullptr && prev->duration == 0) {
      prev->duration = now - m_record_base - prev->start;
      if (prev->duration == 0) {
        prev->duration = grid;
      }
    }
    seq->quantize(grid);
    return true;
  }

  // ---- playback -------------------------------------------------------------
  // Arming an EMPTY sequence is legal: it plays silence until steps are added
  // (the demo -i workflow pre-arms, the user only types `seq add ...`).
  bool play(Tick transport_tick) noexcept {
    if (m_pool.empty() || m_recording) {
      return false;  // record and playback are mutually exclusive states
    }
    m_playing = true;
    m_base = transport_tick;
    return true;
  }
  void stop_playback(ReleaseFn release) noexcept {
    if (m_playing) {
      release();
    }
    m_playing = false;
  }
  bool playing() const noexcept { return m_playing; }

  // Call once per transport tick while the transport runs (and once with the
  // start tick right after transport start).
  //
  // Regression (spike follow-up, docs/proposals/gui-live-harmony-musical-
  // design.md): play() rebases m_base to whatever tick the transport is AT
  // when called -- including while the transport is STOPPED (Transport::
  // stop() intentionally preserves position, MIDI Stop semantics). A
  // subsequent Transport::start() rewinds the transport itself back to tick
  // 0 (MIDI Start semantics, also intentional) but does nothing to this
  // class on its own -- the old unconditional `transport_tick < m_base`
  // early-return would then silently starve the sequence until the tick
  // counter numerically caught back up to the stale m_base (which, for any
  // reasonably long previous run, is effectively "never" within a session).
  // `transport_tick < m_base` can only mean the transport itself was
  // rewound since m_base was last set (ticks only ever increase while
  // playing otherwise), so treat it as this run's fresh start and re-anchor
  // here, rather than assuming transport_tick is monotonic against a stale
  // anchor.
  void on_tick(Tick transport_tick, FireFn fire, ReleaseFn release) {
    if (!m_playing) {
      return;
    }
    if (transport_tick < m_base) {
      m_base = transport_tick;
    }
    ChordSequence* seq = current();
    const Tick len = seq->length();
    if (len == 0) {
      return;  // armed but still empty: silence (and no modulo by zero)
    }
    Tick pos = transport_tick - m_base;
    if (pos >= len) {
      if (!seq->loop) {
        // Stop on reaching OR overshooting the end: a stale m_base (e.g. a
        // bare kSeqUse switch to a shorter sequence, never paired with a
        // rebasing kSeqPlay) can leave pos > len, not just == len. Treat
        // both as "sequence over" -- the old pos == len check silently
        // stalled forever instead of stopping when pos overshot (spike:
        // docs/proposals/gui-live-harmony-musical-design.md S3.4 Option B).
        stop_playback(release);
        return;
      }
      pos %= len;
    }
    // Fire the step that starts exactly on this position.
    for (std::size_t i = 0; i < seq->count(); ++i) {
      const ChordStep& s = seq->step(i);
      if (s.start != pos) {
        continue;
      }
      const theory::Scale scale = theory::scale_of(seq->key.mode);
      const std::uint8_t root_pc = static_cast<std::uint8_t>(
          (seq->key.root_pc + scale.steps[static_cast<std::uint8_t>(s.degree)]) % 12);
      const ChordQuality q = s.quality_ovr >= 0 ? static_cast<ChordQuality>(s.quality_ovr)
                                                : theory::smart_quality(seq->key.mode, s.degree);
      fire(static_cast<std::uint8_t>(60 + root_pc), q, static_cast<std::uint8_t>(s.degree),
           s.velocity);
      break;
    }
  }

 private:
  StaticVector<ChordSequence, kMaxChordSequences> m_pool;
  std::size_t m_current = 0;
  bool m_recording = false;
  bool m_playing = false;
  Tick m_record_base = 0;
  Tick m_base = 0;
};

}  // namespace arrangrr
