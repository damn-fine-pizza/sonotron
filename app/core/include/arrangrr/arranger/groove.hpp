#pragma once

#include <cstdint>

#include "arrangrr/common/time.hpp"       // Tick, TickOffset
#include "arrangrr/timeline/timeline.hpp"  // kTicksPerStep

// Groove engine (host UI: the `groove` panel): turns FEEL into parameters
// instead of hand-authored notes. It post-processes each arranger event —
// pushing off-beats (swing), emphasizing downbeats (accent) and adding a
// deterministic, seeded wobble (humanize) to timing and velocity. Pure and
// freestanding (no heap, no I/O); every result is a deterministic function of
// the parameters + the event's grid position, so "same seed ⇒ same groove"
// (D16). This is the first parameter layer the future Generative Director (D37)
// will drive.

namespace arrangrr {

// One global groove feel applied to every arranger part. Per-part grooves are a
// later refinement; the field-addressed setter keeps the ABI stable for it.
struct GrooveParams {
  std::uint8_t swing = 0;              // 0..100 %: push the off-beat toward triplet feel
  std::uint8_t humanize_timing = 0;    // 0..100 %: deterministic late-push wobble
  std::uint8_t humanize_velocity = 0;  // 0..100 %: deterministic velocity wobble
  std::uint8_t accent = 0;             // 0..100 %: downbeat velocity emphasis
  std::uint8_t swing_grid = 8;         // 8 or 16: swing the off-8th or off-16th
  std::uint8_t quantize = 0;           // 0..100 %: pull the timing offset back to the grid
  std::uint32_t seed = 1;              // humanize seed (determinism)
};

// Field selector for the ABI (kGroove) and the host command/panel.
enum class GrooveField : std::uint8_t {
  kSwing = 0,
  kHumanizeTiming = 1,
  kHumanizeVelocity = 2,
  kAccent = 3,
  kSwingGrid = 4,
  kSeed = 5,
  kQuantize = 6,
};
inline constexpr std::uint8_t kGrooveFieldCount = 7;

struct GrooveOut {
  TickOffset timing_offset = 0;  // added to the note-on AND note-off delay (gate preserved)
  std::uint8_t velocity = 0;     // adjusted velocity, always 1..127
};

namespace groove {

// Deterministic 32-bit position hash — same inputs always yield the same value,
// so humanize is reproducible with no PRNG state to carry or reset.
constexpr std::uint32_t hash(std::uint32_t seed, std::uint32_t tick, std::uint8_t role,
                             std::uint16_t step) noexcept {
  std::uint32_t h = seed * 2654435761u + tick + 0x9E3779B9u;
  h *= 2246822519u;
  h ^= (static_cast<std::uint32_t>(role) << 16) ^ step;
  h *= 3266489917u;
  h ^= h >> 15;
  return h;
}

// Applies the groove to one event. `step` is the 0..15 sixteenth-grid slot in
// the bar, `tick` the absolute transport tick (decorrelates humanize), `role`
// the part index, `base_vel` the pattern's velocity.
constexpr GrooveOut apply(const GrooveParams& params, std::uint8_t role, std::uint16_t step,
                          Tick tick, std::uint8_t base_vel) noexcept {
  GrooveOut out{.timing_offset = 0, .velocity = base_vel};

  // Swing: delay the off-beats of the chosen grid. Off-8th = steps 2,6,10,14;
  // off-16th = every odd step. Max push ~2/3 of a 16th at 100% (triplet feel).
  const std::uint16_t period = (params.swing_grid == 16) ? 1 : 2;
  if (step % static_cast<std::uint16_t>(period * 2) == period && params.swing > 0) {
    out.timing_offset +=
        static_cast<TickOffset>((kTicksPerStep * 2u * params.swing) / 300u);
  }

  int vel = base_vel;

  // Accent: strongest on beat 1, half on beat 3, a touch softer on 2 & 4.
  if (params.accent > 0) {
    const int amount = params.accent;
    if (step % 16 == 0) {
      vel += (30 * amount) / 100;
    } else if (step % 8 == 0) {
      vel += (15 * amount) / 100;
    } else if (step % 4 == 0) {
      vel -= (8 * amount) / 100;
    }
  }

  // Humanize: one position hash feeds both the velocity and timing wobble.
  const std::uint32_t h = hash(params.seed, tick, role, step);
  if (params.humanize_velocity > 0) {
    const int jitter = static_cast<int>(h % 21u) - 10;  // -10..+10
    vel += (jitter * params.humanize_velocity) / 100;
  }
  if (params.humanize_timing > 0) {
    // Push-late only (0..half a 16th) so the note never schedules in the past.
    const int push = static_cast<int>((h >> 8) % (kTicksPerStep / 2u));
    out.timing_offset += static_cast<TickOffset>((push * params.humanize_timing) / 100);
  }

  // Quantize: scale the accumulated swing+humanize offset toward the grid as a
  // final step. 0% leaves the groove untouched; 100% snaps the event exactly
  // onto the grid (offset zeroed). The same scaled offset still rides both the
  // note-on and note-off, so the gate stays preserved.
  if (params.quantize > 0) {
    out.timing_offset = static_cast<TickOffset>(
        (out.timing_offset * (100 - static_cast<TickOffset>(params.quantize))) / 100);
  }

  out.velocity = static_cast<std::uint8_t>(vel < 1 ? 1 : (vel > 127 ? 127 : vel));
  return out;
}

// Sets one field from an ABI/command value, clamping to its valid range.
constexpr void set_field(GrooveParams& params, GrooveField field, std::int32_t value) noexcept {
  const auto pct = static_cast<std::uint8_t>(value < 0 ? 0 : (value > 100 ? 100 : value));
  switch (field) {
    case GrooveField::kSwing:
      params.swing = pct;
      break;
    case GrooveField::kHumanizeTiming:
      params.humanize_timing = pct;
      break;
    case GrooveField::kHumanizeVelocity:
      params.humanize_velocity = pct;
      break;
    case GrooveField::kAccent:
      params.accent = pct;
      break;
    case GrooveField::kSwingGrid:
      params.swing_grid = (value == 16) ? 16 : 8;
      break;
    case GrooveField::kSeed:
      params.seed = static_cast<std::uint32_t>(value < 0 ? 0 : value);
      break;
    case GrooveField::kQuantize:
      params.quantize = pct;
      break;
  }
}

}  // namespace groove

}  // namespace arrangrr
