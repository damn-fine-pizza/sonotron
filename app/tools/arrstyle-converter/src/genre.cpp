#include "genre.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

// The classifier is a small, explainable rule cascade over a handful of features
// derived from the SMF. It deliberately avoids machine learning: the corpus is
// heterogeneous and a transparent rule set is far easier to audit and to correct
// than an opaque model. Every feature is measured on the drum lane (GM channel
// 10, 0-based 9) plus the file-level tempo/time-signature, because the groove is
// the single most genre-discriminating element of an arranger style.

namespace arrstyle {

namespace {

// GM percussion key numbers we key features off (channel 9, 0-based).
constexpr std::uint8_t kAcousticBassDrum = 35;
constexpr std::uint8_t kBassDrum1 = 36;
constexpr std::uint8_t kSideStick = 37;
constexpr std::uint8_t kAcousticSnare = 38;
constexpr std::uint8_t kElectricSnare = 40;
constexpr std::uint8_t kClosedHiHat = 42;
constexpr std::uint8_t kPedalHiHat = 44;
constexpr std::uint8_t kOpenHiHat = 46;

constexpr std::uint8_t kDrumChannel = 9;

constexpr int kStepsPerBar = 16;  // 16th-note grid, 4/4 reference

// A bar-relative onset histogram on the 16th grid, split by drum voice.
struct DrumFeatures {
  std::array<int, kStepsPerBar> kick{};
  std::array<int, kStepsPerBar> snare{};
  std::array<int, kStepsPerBar> hat{};
  std::array<int, kStepsPerBar> side_stick{};
  int drum_note_count = 0;
  int total_note_count = 0;
  int melodic_note_count = 0;  // non-drum
  // Swing estimate: mean position (0..1) of clear off-beat onsets inside a beat.
  double offbeat_mean = 0.5;
  int offbeat_samples = 0;
};

bool is_kick(std::uint8_t note) { return note == kAcousticBassDrum || note == kBassDrum1; }
bool is_snare(std::uint8_t note) { return note == kAcousticSnare || note == kElectricSnare; }
bool is_hat(std::uint8_t note) {
  return note == kClosedHiHat || note == kPedalHiHat || note == kOpenHiHat;
}

// Maps an absolute tick to its bar-relative 16th step (0..15) for 4/4 timing.
int bar_step(std::uint32_t tick, std::uint16_t division) {
  if (division == 0) {
    return 0;
  }
  const std::uint32_t step = (tick * 4U) / division;  // 4 sixteenths per quarter
  return static_cast<int>(step % static_cast<std::uint32_t>(kStepsPerBar));
}

DrumFeatures extract_features(const SmfFile& smf) {
  DrumFeatures f;
  const std::uint16_t div = smf.division;
  double offbeat_accum = 0.0;
  for (const SmfTrack& track : smf.tracks) {
    for (const SmfNote& n : track.notes) {
      ++f.total_note_count;
      if (n.channel == kDrumChannel) {
        ++f.drum_note_count;
        const int step = bar_step(n.tick, div);
        if (is_kick(n.note)) {
          ++f.kick[static_cast<std::size_t>(step)];
        } else if (is_snare(n.note)) {
          ++f.snare[static_cast<std::size_t>(step)];
        } else if (is_hat(n.note)) {
          ++f.hat[static_cast<std::size_t>(step)];
        } else if (n.note == kSideStick) {
          ++f.side_stick[static_cast<std::size_t>(step)];
        }
      } else {
        ++f.melodic_note_count;
      }
      // Swing sampling: where inside a beat does an off-beat onset land?
      if (div != 0) {
        const double frac = static_cast<double>(n.tick % div) / static_cast<double>(div);
        if (frac > 0.30 && frac < 0.72) {
          offbeat_accum += frac;
          ++f.offbeat_samples;
        }
      }
    }
  }
  if (f.offbeat_samples > 0) {
    f.offbeat_mean = offbeat_accum / static_cast<double>(f.offbeat_samples);
  }
  return f;
}

bool present(const std::array<int, kStepsPerBar>& v, int step) {
  return v[static_cast<std::size_t>(step)] > 0;
}

int distinct_steps(const std::array<int, kStepsPerBar>& v) {
  int n = 0;
  for (int c : v) {
    if (c > 0) {
      ++n;
    }
  }
  return n;
}

float clamp01(float v) { return std::max(0.0F, std::min(1.0F, v)); }

}  // namespace

GenreGuess infer_genre(const SmfFile& smf) {
  const DrumFeatures f = extract_features(smf);
  const std::uint32_t bpm = smf.tempo_milli_bpm / 1000U;

  // Triple metre is a strong, unambiguous signal.
  if (smf.time_sig_num == 3) {
    return {.genre = "waltz", .confidence = 0.80F};
  }

  if (f.total_note_count == 0) {
    return {.genre = "unknown", .confidence = 0.0F};
  }

  // Groove predicates on the 4/4 16th grid.
  const bool four_on_floor =
      present(f.kick, 0) && present(f.kick, 4) && present(f.kick, 8) && present(f.kick, 12);
  const bool backbeat = present(f.snare, 4) && present(f.snare, 12);
  const bool offbeat_hat =
      present(f.hat, 2) && present(f.hat, 6) && present(f.hat, 10) && present(f.hat, 14);
  const bool no_backbeat = !backbeat;
  const bool has_side_stick = distinct_steps(f.side_stick) > 0;
  const int hat_steps = distinct_steps(f.hat);
  const bool swing_feel = f.offbeat_mean > 0.60 && f.offbeat_samples >= 3;
  // Syncopated kick: kick lands on weak 16ths (the "e"/"a") away from downbeats.
  const bool syncopated_kick = present(f.kick, 3) || present(f.kick, 6) || present(f.kick, 10) ||
                               present(f.kick, 14) || present(f.kick, 7);

  // Cascade, most specific first.
  if (swing_feel) {
    const float conf = clamp01(0.55F + (backbeat ? 0.0F : 0.10F) + (bpm >= 140U ? 0.10F : 0.0F));
    return {.genre = bpm >= 150U ? "swing" : "jazz", .confidence = conf};
  }

  if (four_on_floor && offbeat_hat && bpm >= 118U && bpm <= 145U) {
    return {.genre = "disco", .confidence = clamp01(0.70F + (backbeat ? 0.10F : 0.0F))};
  }

  if (no_backbeat && has_side_stick && bpm >= 100U && bpm <= 140U) {
    // Dense 16th percussion and a faster tempo lean samba; otherwise bossa.
    const bool dense = f.drum_note_count >= 12 && hat_steps >= 8;
    if (dense && bpm >= 120U) {
      return {.genre = "samba", .confidence = 0.65F};
    }
    return {.genre = "bossa", .confidence = 0.68F};
  }

  if (syncopated_kick && backbeat && bpm >= 90U && bpm <= 120U && hat_steps >= 8) {
    return {.genre = "funk", .confidence = 0.62F};
  }

  if (backbeat) {
    if (bpm <= 82U && f.drum_note_count <= 10) {
      return {.genre = "ballad", .confidence = clamp01(0.55F + (bpm <= 72U ? 0.10F : 0.0F))};
    }
    if (bpm >= 118U) {
      return {.genre = "rock", .confidence = clamp01(0.55F + (bpm >= 130U ? 0.10F : 0.0F))};
    }
    return {.genre = "pop", .confidence = 0.58F};
  }

  // Weak fallback: use tempo alone with low confidence.
  if (f.drum_note_count == 0 && f.melodic_note_count > 0) {
    if (bpm <= 82U) {
      return {.genre = "ballad", .confidence = 0.30F};
    }
    return {.genre = "pop", .confidence = 0.25F};
  }

  return {.genre = "unknown", .confidence = 0.10F};
}

}  // namespace arrstyle
