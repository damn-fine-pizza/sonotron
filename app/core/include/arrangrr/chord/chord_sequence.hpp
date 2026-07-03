#pragma once

#include <cstdint>

#include "arrangrr/chord/theory.hpp"
#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/common/time.hpp"
#include "arrangrr/config.hpp"
#include "arrangrr/transport/transport.hpp"

// ChordSequence: the recorded half of D13 and the heart of the first WOW.
// Steps have FREE durations (D14) and are stored FUNCTIONALLY (D28): a
// degree + optional quality override relative to a per-sequence reference
// key. Transposition moves the reference key and playback re-derives — so
// "transpose to G" turns the ii into Am7 instead of blindly shifting pitches.
// (Chromatic absolute overrides — mod sec/borrow — arrive with M4/M5.)

namespace arrangrr {

struct ChordStep {
  Tick start = 0;                // relative to sequence start
  Tick duration = 0;             // free length in ticks (D14)
  std::int8_t degree = -1;       // 0..6 in the reference key
  std::int8_t quality_ovr = -1;  // -1 = smart (D19) at playback
  std::uint8_t velocity = 100;
};
static_assert(sizeof(ChordStep) == 12);  // D33 budget: 16 seqs x 128 x 12 B = 24 KB

class ChordSequence {
 public:
  Key key{};  // per-sequence reference key (D28)
  bool loop = false;

  constexpr std::size_t count() const noexcept { return m_steps.size(); }
  constexpr const ChordStep& step(std::size_t i) const noexcept { return m_steps[i]; }
  constexpr Span<const ChordStep> steps() const noexcept { return m_steps.span(); }

  // Total length: end of the last step (free durations stack sequentially).
  constexpr Tick length() const noexcept {
    if (m_steps.empty()) {
      return 0;
    }
    const ChordStep& last = m_steps[m_steps.size() - 1];
    return last.start + last.duration;
  }

  // Appends a step after the current end.
  [[nodiscard]] constexpr bool append(std::int8_t degree, std::int8_t quality_ovr,
                                      std::uint8_t velocity, Tick duration) noexcept {
    if (duration == 0) {
      return false;
    }
    return m_steps.push_back(ChordStep{length(), duration, degree, quality_ovr, velocity});
  }

  // Removes step i and closes the gap (later steps shift earlier).
  constexpr bool remove(std::size_t i) noexcept {
    if (i >= m_steps.size()) {
      return false;
    }
    const Tick removed = m_steps[i].duration;
    m_steps.erase(i);
    for (std::size_t k = i; k < m_steps.size(); ++k) {
      m_steps[k].start -= removed;
    }
    return true;
  }

  constexpr void clear() noexcept { m_steps.clear(); }

  // Raw append during recording (start given by the recorder).
  [[nodiscard]] constexpr bool record(const ChordStep& step) noexcept {
    return m_steps.push_back(step);
  }
  constexpr ChordStep* last() noexcept {
    return m_steps.empty() ? nullptr : &m_steps[m_steps.size() - 1];
  }

  // Quantize-after (D14 live entry): snap starts to the grid, rebuild
  // durations from consecutive starts; every step keeps at least one grid
  // unit. Default grid = one bar.
  constexpr void quantize(Tick grid = kTicksPerBar) noexcept {
    if (grid == 0 || m_steps.empty()) {
      return;
    }
    Tick previous_end = 0;
    for (std::size_t i = 0; i < m_steps.size(); ++i) {
      Tick snapped = ((m_steps[i].start + grid / 2) / grid) * grid;
      if (snapped < previous_end) {
        snapped = previous_end;  // keep order
      }
      m_steps[i].start = snapped;
      previous_end = snapped + grid;  // provisional; fixed below
    }
    for (std::size_t i = 0; i + 1 < m_steps.size(); ++i) {
      const Tick gap = m_steps[i + 1].start - m_steps[i].start;
      m_steps[i].duration = gap > 0 ? gap : grid;
    }
    ChordStep& tail = m_steps[m_steps.size() - 1];
    tail.duration = ((tail.duration + grid / 2) / grid) * grid;
    if (tail.duration == 0) {
      tail.duration = grid;
    }
  }

  // D28 transposition: move the reference key; degrees are untouched and
  // playback re-derives the concrete chords.
  constexpr void transpose_to(std::uint8_t root_pc, std::int8_t mode) noexcept {
    key.root_pc = static_cast<std::uint8_t>(root_pc % 12);
    if (mode >= 0 && mode < kModeCount) {
      key.mode = static_cast<Mode>(mode);
    }
  }
  constexpr void transpose_by(std::int8_t semitones) noexcept {
    key.root_pc = static_cast<std::uint8_t>((key.root_pc + (semitones % 12) + 12) % 12);
  }

 private:
  StaticVector<ChordStep, kMaxChordSteps> m_steps;
};

}  // namespace arrangrr
