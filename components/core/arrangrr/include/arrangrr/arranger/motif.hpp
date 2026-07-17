#pragma once

#include <cstdint>

#include "arrangrr/arranger/style_model.hpp"  // StyleEvent, StylePattern, MotifSpec, MotifTransform
#include "arrangrr/common/seeded_hash.hpp"    // D16 shared position hash
#include "arrangrr/common/span.hpp"

// Generative motif engine, first slice (roadmap 9210). See
// docs/design/motif-engine-scope.md (Ottorino, musical scope) and
// docs/design/motif-engine-placement.md (Corelli, structural placement) for
// the full design rationale; this header implements their consensus.
//
// PLACEMENT (Corelli): a motif is a gather-phase PRODUCER of StyleEvent specs,
// occupying the SAME slot `gesture::expand` (arranger/gesture.hpp) already
// does inside `Arranger::on_tick`'s per-step resolution loop (D40) -- upstream
// of resolve()/VoicingState::voice()/groove::apply(), all of which stay
// unchanged. It is NOT a runtime::Pipeline stage: unlike Restyle (9320), it
// has no external producer to compose with -- only style data already
// resident in the Arranger (the compiled Style/StylePattern tables, plus the
// live Key/ChordState `Arranger::on_tick` already receives).
//
// DATA FORM (a NEEDS-DECISION per Corelli's placement review §5, resolved
// here as this first slice's default): a motif is a bounded fixed-length
// array of the EXISTING `StyleEvent` (style_model.hpp) -- NOT a new parallel
// type. One bar (kStepsPerBar = 16 sixteenth-grid steps) of authored-or-
// generated content, reusing the same 10-byte pinned struct every style table
// already uses, so it flows UNCHANGED through resolve()/voice()/groove()
// exactly like authored content does today (motif-engine-scope.md §0.1).
//
// SEEDED DETERMINISM (D16): every generative decision here (onset choice,
// contour delta, cadence pick, anti-triviality retry, repeat-keyed transform
// amount) is a pure function of (seed, position) via the shared
// `arrangrr::seeded_hash` (arrangrr/common/seeded_hash.hpp) -- no PRNG
// object, no carried state beyond the bounded per-repeat counter the
// `Arranger` itself owns (reset like `VoicingState::reset()`).

namespace arrangrr {

// TWO DISTINCT QUANTITIES (task #31), previously conflated under one name:
//
// kStepsPerBar is the bar's 16th-grid WIDTH -- the musical quantity every
// style table already speaks in via `StyleEvent::step` (style_model.hpp).
// This is what bounds retrograde reflection, displacement wrap, generate()'s
// candidate step range, and the onset bitmask below: all of them reason
// about "where in the bar", never about how many events a Motif can hold.
inline constexpr int kStepsPerBar = 16;

// kMaxMotifLen is the `Motif::events[]` array CAPACITY only -- how many
// StyleEvent slots a motif can carry, independent of the bar's step width.
// 32, not 16: the densest single-bar kFixed drum pattern in the corpus packs
// up to ~28 simultaneous onsets, and 32 is the clean power-of-two with
// headroom above that. Used ONLY for the struct's own array size and for
// from_span's capacity clamp below -- never where the meaning is "bar
// width" (that is always kStepsPerBar above).
inline constexpr int kMaxMotifLen = 32;

// A motif: a short, ordered StyleEvent sequence. `count` is the number of
// active slots in `events` ([0, count)); unused tail slots are default.
struct Motif {
  StyleEvent events[kMaxMotifLen]{};
  std::uint8_t count = 0;
};

namespace motif {

// ---- Musicality constants (motif-engine-scope.md §2) -----------------------

// Contour bound (§2.1): consecutive generated events may not leap by more
// than this many diatonic scale-steps. Keeps a generated line stepwise-
// dominant instead of a directionless random walk (the plan's own named
// risk: "easy to make correct, hard to make interesting").
inline constexpr int kMaxLeap = 4;

// Bounded retries for the anti-triviality guard (§2.5): re-hash to another
// amount this many times before giving up and returning the result honestly
// -- same cost class as a single groove::hash evaluation, no unbounded loop
// on the realtime path.
inline constexpr int kMaxRetries = 4;

// A "stable" scale degree for the cadence rule (§2.4): root (0) or fifth (4),
// 0-indexed within the 7-degree diatonic scale -- either resolves a phrase;
// every other degree reads as unresolved.
constexpr bool is_stable_degree(int degree) noexcept {
  const int m = ((degree % 7) + 7) % 7;
  return m == 0 || m == 4;
}

// Nearest scale degree congruent to `target_mod` (0 or 4) mod 7, used by the
// cadence rule to snap a contour's last degree to the closest stable one
// without a large jump. A match always exists within +/-7 (residues repeat
// every 7 steps), so this always terminates.
constexpr int nearest_stable_degree(int degree, bool prefer_fifth) noexcept {
  const int target_mod = prefer_fifth ? 4 : 0;
  int best = degree;
  int best_dist = 1000;
  for (int cand = degree - 7; cand <= degree + 7; ++cand) {
    const int m = ((cand % 7) + 7) % 7;
    if (m != target_mod) {
      continue;
    }
    const int dist = cand >= degree ? cand - degree : degree - cand;
    if (dist < best_dist) {
      best_dist = dist;
      best = cand;
    }
  }
  return best;
}

// ---- Onset step-set helpers -------------------------------------------------

// The onset bitmask (bit i = step i is an onset) of a StyleEvent span --
// used both to read a style's own idiomatic drum-step subset (§2.3) and to
// compare an input/output motif's onset set for the anti-triviality guard
// (§2.5). Steps at or past kStepsPerBar are outside a one-bar motif and are
// ignored (defensive; no authored/generated event should ever set one).
constexpr std::uint32_t onset_mask(Span<const StyleEvent> evs) noexcept {
  std::uint32_t mask = 0;
  for (const StyleEvent& ev : evs) {
    if (ev.step < static_cast<std::uint16_t>(kStepsPerBar)) {
      mask |= (1u << ev.step);
    }
  }
  return mask;
}
constexpr std::uint32_t onset_mask(const Motif& m) noexcept {
  std::uint32_t mask = 0;
  for (std::uint8_t i = 0; i < m.count; ++i) {
    if (m.events[i].step < static_cast<std::uint16_t>(kStepsPerBar)) {
      mask |= (1u << m.events[i].step);
    }
  }
  return mask;
}

// Reads the onset mask of `role`'s own kFixed pattern within `patterns` (a
// section's pattern list), or 0 if absent. No new data (§2.3): this simply
// re-reads a pattern already resident in the style table -- the GENERATOR's
// fallback (every 16th of the bar) applies when the result is 0.
constexpr std::uint32_t idiom_onset_mask(Span<const StylePattern> patterns,
                                         TrackRole role) noexcept {
  for (const StylePattern& p : patterns) {
    if (p.role == role && p.policy == RolePolicy::kFixed) {
      return onset_mask(p.events);
    }
  }
  return 0;
}

// ---- Seed motif: authored or generated --------------------------------------

// Copies an authored seed motif out of a StylePattern's own `events` span
// (Ottorino's "Option 1" -- motif-engine-scope.md §3), capped to kMaxMotifLen.
constexpr Motif from_span(Span<const StyleEvent> evs) noexcept {
  Motif m;
  const std::uint8_t n = static_cast<std::uint8_t>(
      evs.size() > static_cast<std::size_t>(kMaxMotifLen) ? static_cast<std::size_t>(kMaxMotifLen)
                                                          : evs.size());
  for (std::uint8_t i = 0; i < n; ++i) {
    m.events[i] = evs[i];
  }
  m.count = n;
  return m;
}

// Generates a fresh seed motif (Ottorino's "Option 2", constrained-random --
// motif-engine-scope.md §3) for a kScaleDegree comping/lead role: `length`
// onsets (bounded to kStepsPerBar), each landing on a step drawn from
// `allowed_steps` (the style's own idiomatic onset mask, §2.3 -- 0 falls back
// to every 16th), a bounded-leap contour (§2.1) around `center_degree`, and a
// stable-degree cadence on the LAST onset (§2.4). Generated motifs are always
// NoteSource::kScaleDegree -- key-diatonic, so they resolve even with no live
// chord yet, unlike kChordTone (a deliberate, documented default: melodic
// material is the safest generative target, not a functional chord restack).
// Every choice is `seeded_hash(seed, i)`-keyed, so the same seed always
// yields the same motif (D16).
constexpr Motif generate(std::uint32_t seed, std::uint8_t length, std::uint32_t allowed_steps,
                         int center_degree, std::uint8_t vel, std::uint16_t gate) noexcept {
  Motif m;
  const std::uint8_t n = length > static_cast<std::uint8_t>(kStepsPerBar)
                             ? static_cast<std::uint8_t>(kStepsPerBar)
                             : length;
  if (n == 0) {
    return m;
  }

  std::uint8_t candidates[kStepsPerBar];
  std::uint8_t candidate_count = 0;
  for (std::uint8_t s = 0; s < static_cast<std::uint8_t>(kStepsPerBar); ++s) {
    if (allowed_steps == 0 || (allowed_steps & (1u << s)) != 0) {
      candidates[candidate_count] = s;
      ++candidate_count;
    }
  }
  if (candidate_count == 0) {
    return m;  // an explicit, fully-empty mask: nothing valid to place
  }

  int degree = center_degree;
  for (std::uint8_t i = 0; i < n; ++i) {
    const std::uint32_t h = seeded_hash(seed, i);

    // Onset (§2.3): spread the n onsets evenly across the candidate steps
    // (so a short motif still spans the bar, not clustered at the start),
    // with a seeded jitter inside each onset's own window for seed-to-seed
    // variety.
    const std::uint8_t window =
        static_cast<std::uint8_t>(candidate_count >= n ? candidate_count / n : 1);
    const std::uint8_t base = static_cast<std::uint8_t>((i * candidate_count) / n);
    const std::uint8_t jitter = static_cast<std::uint8_t>(h % window);
    const std::uint8_t idx = static_cast<std::uint8_t>((base + jitter) % candidate_count);

    // Contour (§2.1) / cadence (§2.4): the LAST onset resolves to the
    // nearest stable degree (root or fifth, alternated by the hash); every
    // interior onset takes a bounded leap from the previous degree; the
    // FIRST onset simply sits at the center (nothing to leap from yet).
    if (i + 1 == n) {
      const bool prefer_fifth = ((h >> 16) & 1u) != 0;
      degree = nearest_stable_degree(degree, prefer_fifth);
    } else if (i > 0) {
      const int delta =
          static_cast<int>((h >> 8) % static_cast<std::uint32_t>(2 * kMaxLeap + 1)) - kMaxLeap;
      degree += delta;
    }

    StyleEvent ev{};
    ev.step = candidates[idx];
    ev.tone = static_cast<std::int8_t>(degree < -128 ? -128 : (degree > 127 ? 127 : degree));
    ev.octave = 0;
    ev.vel = vel;
    ev.gate = gate;
    ev.src = NoteSource::kScaleDegree;
    ev.gesture = ChordGesture::kNone;
    m.events[i] = ev;
  }
  m.count = n;
  return m;
}

// ---- Transforms (motif-engine-scope.md §1) ---------------------------------

// kDiatonicTranspose: tone += amount, but ONLY for kScaleDegree/kInterval
// events (§1a). kChordTone events pass through unmodified: there, `tone` is
// already a chord-relative index (0=root, 1=third, ...), so adding a constant
// would reassign which FUNCTIONAL tone plays -- a restack, not a melodic
// transposition -- and is deliberately out of THIS first slice's scope
// (Ottorino, "Open forks" #2).
constexpr Motif transpose_diatonic(const Motif& in, int amount) noexcept {
  Motif out = in;
  for (std::uint8_t i = 0; i < out.count; ++i) {
    StyleEvent& ev = out.events[i];
    if (ev.src == NoteSource::kScaleDegree || ev.src == NoteSource::kInterval) {
      const int t = static_cast<int>(ev.tone) + amount;
      ev.tone = static_cast<std::int8_t>(t < -128 ? -128 : (t > 127 ? 127 : t));
    }
  }
  return out;
}

// kRetrograde: step -> (kStepsPerBar - 1 - step), a reflection within the
// bar. Reordering the array itself is unnecessary: the arranger's fire loop
// only ever asks "does an event match THIS step", so remapping each event's
// own `step` in place is behaviorally identical to reversing the sequence
// order (motif-engine-scope.md §1b describes both together). Valid for every
// NoteSource, including kFixed drum patterns.
constexpr Motif retrograde(const Motif& in) noexcept {
  Motif out = in;
  for (std::uint8_t i = 0; i < out.count; ++i) {
    const int reflected = (kStepsPerBar - 1) - static_cast<int>(in.events[i].step);
    out.events[i].step = static_cast<std::uint16_t>(reflected < 0 ? 0 : reflected);
  }
  return out;
}

// kDisplacement: step += amount (mod kStepsPerBar), a rhythmic phase shift.
// Valid for every NoteSource, including kFixed.
constexpr Motif displace(const Motif& in, int amount) noexcept {
  Motif out = in;
  for (std::uint8_t i = 0; i < out.count; ++i) {
    int s = (static_cast<int>(in.events[i].step) + amount) % kStepsPerBar;
    if (s < 0) {
      s += kStepsPerBar;
    }
    out.events[i].step = static_cast<std::uint16_t>(s);
  }
  return out;
}

// Applies ONE transform with a fixed `amount`, no anti-triviality check (that
// lives in apply_transform below, which callers should use instead of this).
constexpr Motif apply_transform_once(const Motif& in, MotifTransform kind, int amount) noexcept {
  switch (kind) {
    case MotifTransform::kDiatonicTranspose:
      return transpose_diatonic(in, amount);
    case MotifTransform::kRetrograde:
      return retrograde(in);
    case MotifTransform::kDisplacement:
      return displace(in, amount);
    case MotifTransform::kNone:
    default:
      return in;
  }
}

// Repeat-keyed transform amount (D16): a pure function of (seed, repeat), so
// "transpose +1 diatonic step every repeat" or "displace by a repeat-varying
// shift" is computed fresh every tick from a small counter, never
// accumulated drift (motif-engine-placement.md §3). Offset from `generate`'s
// own per-index hashing range (i < kStepsPerBar) so the two do not
// accidentally correlate.
constexpr int transform_amount(MotifTransform kind, std::uint32_t seed,
                               std::uint32_t repeat) noexcept {
  const std::uint32_t h = seeded_hash(seed, repeat + 0x1000u);
  switch (kind) {
    case MotifTransform::kDiatonicTranspose:
      return static_cast<int>(h % 7u) - 3;  // -3..+3 diatonic steps
    case MotifTransform::kDisplacement:
      return static_cast<int>(h % static_cast<std::uint32_t>(kStepsPerBar - 1)) + 1;  // 1..len-1
    case MotifTransform::kRetrograde:  // reflection needs no amount
    case MotifTransform::kNone:
    default:
      return 0;
  }
}

// Applies `kind` to `in`, guarding against a trivial/accidental parameter
// choice (§2.5) that would silently reproduce the seed instead of varying it:
//   - kDiatonicTranspose is trivial iff amount == 0 (the tone sequence is
//     then literally unchanged -- transpose never touches `step`, so an
//     onset-set comparison would not catch this case).
//   - kRetrograde/kDisplacement are trivial iff the OUTPUT onset-step set
//     equals the input's (a palindromic motif, or a displacement amount
//     congruent to 0 mod the span).
// On a trivial result, re-hashes to another amount, bounded to kMaxRetries
// attempts (same cost class as a single groove::hash evaluation -- no
// unbounded loop risk on the realtime path since this runs once per
// generation, not per tick). A motif of 0 or 1 events has no meaningful
// "trivial" case (nothing to compare) and skips the guard entirely.
constexpr Motif apply_transform(const Motif& in, MotifTransform kind, int amount,
                                std::uint32_t retry_seed) noexcept {
  if (in.count <= 1 || kind == MotifTransform::kNone) {
    return apply_transform_once(in, kind, amount);
  }
  const std::uint32_t original_mask = onset_mask(in);
  int try_amount = amount;
  for (int attempt = 0; attempt < kMaxRetries; ++attempt) {
    const Motif out = apply_transform_once(in, kind, try_amount);
    const bool trivial = (kind == MotifTransform::kDiatonicTranspose)
                             ? (try_amount == 0)
                             : (onset_mask(out) == original_mask);
    if (!trivial) {
      return out;
    }
    const std::uint32_t h = seeded_hash(retry_seed, static_cast<std::uint32_t>(attempt));
    try_amount = (kind == MotifTransform::kDiatonicTranspose)
                     ? (static_cast<int>(h % 6u) + 1)  // 1..6, never 0
                     : (static_cast<int>(h % static_cast<std::uint32_t>(kStepsPerBar - 1)) + 1);
  }
  return apply_transform_once(in, kind, try_amount);  // retries exhausted: give up honestly
}

// Applies the call-and-response repeat policy (§2.2) to an already-produced
// seed motif (authored via from_span, or generated via generate()): EVEN
// repeats play it verbatim (the "statement"); ODD repeats apply
// `spec.transform` with a repeat-keyed amount and the anti-triviality guard
// (the "answer") -- Schoenberg's developing-variation idiom (vary rhythm or
// contour while keeping the motif's identity recognizable), the simplest
// concrete form of it, and the SAME section-level statement/variation/return
// vocabulary (SectionType::kVarA..kVarD) one level down.
constexpr Motif apply_repeat(const Motif& seed, const MotifSpec& spec,
                             std::uint32_t repeat) noexcept {
  if (spec.transform == MotifTransform::kNone || repeat % 2 == 0) {
    return seed;
  }
  const int amount = transform_amount(spec.transform, spec.seed, repeat);
  const std::uint32_t retry_seed = seeded_hash(spec.seed, repeat);
  return apply_transform(seed, spec.transform, amount, retry_seed);
}

}  // namespace motif
}  // namespace arrangrr
