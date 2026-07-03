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
  using FireFn = FunctionRef<void(std::uint8_t root_note, ChordQuality quality,
                                  std::uint8_t degree, std::uint8_t velocity)>;
  using ReleaseFn = FunctionRef<void()>;

  // ---- pool ----------------------------------------------------------------
  int add_sequence(const Key& reference) noexcept {
    ChordSequence seq;
    seq.key = reference;
    if (!pool_.push_back(seq)) return -1;
    current_ = pool_.size() - 1;
    return static_cast<int>(current_);
  }
  bool use(std::size_t idx) noexcept {
    if (idx >= pool_.size()) return false;
    current_ = idx;
    return true;
  }
  ChordSequence* current() noexcept {
    return pool_.empty() ? nullptr : &pool_[current_];
  }
  const ChordSequence* current() const noexcept {
    return pool_.empty() ? nullptr : &pool_[current_];
  }
  std::size_t count() const noexcept { return pool_.size(); }

  // ---- recording (D13: the recorded half) ----------------------------------
  bool start_record(Tick now) noexcept {
    if (pool_.empty() || playing_) return false;
    recording_ = true;
    record_base_ = now;
    current()->clear();
    return true;
  }
  bool recording() const noexcept { return recording_; }

  // Called for every live chord play while recording: closes the previous
  // step and opens a new one at the current stream tick.
  void capture(Tick now, std::int8_t degree, std::int8_t quality_ovr,
               std::uint8_t velocity) noexcept {
    if (!recording_) return;
    ChordSequence* seq = current();
    const Tick rel = now - record_base_;
    if (ChordStep* prev = seq->last(); prev != nullptr && prev->duration == 0) {
      prev->duration = rel - prev->start;
    }
    (void)seq->record(ChordStep{rel, 0, degree, quality_ovr, velocity});
  }

  // Stops recording; the open step is closed at `now`, then quantize-after.
  bool stop_record(Tick now, Tick grid = kTicksPerBar) noexcept {
    if (!recording_) return false;
    recording_ = false;
    ChordSequence* seq = current();
    if (ChordStep* prev = seq->last(); prev != nullptr && prev->duration == 0) {
      prev->duration = now - record_base_ - prev->start;
      if (prev->duration == 0) prev->duration = grid;
    }
    seq->quantize(grid);
    return true;
  }

  // ---- playback -------------------------------------------------------------
  bool play(Tick transport_tick) noexcept {
    if (pool_.empty() || current()->count() == 0) return false;
    playing_ = true;
    base_ = transport_tick;
    next_step_ = 0;
    return true;
  }
  void stop_playback(ReleaseFn release) noexcept {
    if (playing_) release();
    playing_ = false;
  }
  bool playing() const noexcept { return playing_; }

  // Call once per transport tick while the transport runs (and once with the
  // start tick right after transport start).
  void on_tick(Tick transport_tick, FireFn fire, ReleaseFn release) {
    if (!playing_ || transport_tick < base_) return;
    ChordSequence* seq = current();
    const Tick len = seq->length();
    Tick pos = transport_tick - base_;
    if (pos >= len) {
      if (!seq->loop) {
        if (pos == len) stop_playback(release);  // sequence over: silence
        return;
      }
      pos %= len;
    }
    if (next_step_ >= seq->count() || pos == 0) next_step_ = 0;
    // Fire the step that starts exactly on this position.
    for (std::size_t i = 0; i < seq->count(); ++i) {
      const ChordStep& s = seq->step(i);
      if (s.start != pos) continue;
      const theory::Scale scale = theory::scale_of(seq->key.mode);
      const std::uint8_t root_pc = static_cast<std::uint8_t>(
          (seq->key.root_pc + scale.steps[static_cast<std::uint8_t>(s.degree)]) % 12);
      const ChordQuality q =
          s.quality_ovr >= 0
              ? static_cast<ChordQuality>(s.quality_ovr)
              : theory::smart_quality(seq->key.mode, s.degree);
      fire(static_cast<std::uint8_t>(60 + root_pc), q,
           static_cast<std::uint8_t>(s.degree), s.velocity);
      break;
    }
  }

 private:
  StaticVector<ChordSequence, kMaxChordSequences> pool_;
  std::size_t current_ = 0;
  bool recording_ = false;
  bool playing_ = false;
  Tick record_base_ = 0;
  Tick base_ = 0;
  std::size_t next_step_ = 0;
};

}  // namespace arrangrr
