// Generative motif engine, first slice (roadmap 9210). Subject is ONE new
// producer (motif::generate / motif::apply_transform / motif::apply_repeat,
// plus their wiring into Arranger::on_tick's gather phase) -- same precedent
// as test_restyle/test_gesture/test_voicing -> unit.
//
// Covers: seeded determinism (D16) + property tests over many seeds (leap
// bound §2.1, cadence §2.4), the style's-own-idiom onset mask (§2.3), the
// three transforms' scoping (§1), the anti-triviality guard (§2.5), the
// call-and-response repeat policy (§2.2), and an end-to-end golden through
// the REAL Arranger fire loop proving the opt-in producer plugs into
// resolve()/groove() unchanged.

#include "arrangrr/arranger/arranger.hpp"

#include <cstdint>

#include "arrangrr/arranger/motif.hpp"
#include "arrangrr/common/static_vector.hpp"
#include "test.hpp"
#include "test_harness.hpp"

namespace {

using namespace arrangrr;

// ---- Pure motif:: unit tests: generator ------------------------------------

void test_motif_generate_deterministic() {
  const Motif a = motif::generate(/*seed=*/42, /*length=*/8, /*allowed_steps=*/0,
                                  /*center_degree=*/0, /*vel=*/90, /*gate=*/200);
  const Motif b = motif::generate(42, 8, 0, 0, 90, 200);
  CHECK(a.count == b.count && a.count == 8);
  bool identical = true;
  bool any_step_nonzero = false;
  for (std::uint8_t i = 0; i < a.count; ++i) {
    identical =
        identical && a.events[i].step == b.events[i].step && a.events[i].tone == b.events[i].tone;
    any_step_nonzero = any_step_nonzero || a.events[i].step != 0;
  }
  CHECK(identical);  // same seed -> byte-identical motif
  CHECK(
      any_step_nonzero);  // and it actually varies onset positions, not a degenerate all-zero motif
}

// §2.1 (leap bound) + §2.4 (cadence): property tests over many seeds, not one
// golden seed (motif-engine-scope.md §4.1's own recommended pattern).
void test_motif_generate_property_leap_and_cadence() {
  for (std::uint32_t seed = 1; seed <= 200; ++seed) {
    const Motif m = motif::generate(seed, 8, 0, 0, 90, 200);
    CHECK(m.count == 8);
    for (std::uint8_t i = 1; i + 1 < m.count; ++i) {
      const int delta = static_cast<int>(m.events[i].tone) - static_cast<int>(m.events[i - 1].tone);
      CHECK(delta >= -motif::kMaxLeap && delta <= motif::kMaxLeap);
    }
    // The LAST onset always resolves to a stable scale degree (root or fifth).
    CHECK(motif::is_stable_degree(m.events[m.count - 1].tone));
  }
}

// §2.3: onsets are drawn from the style's own idiomatic onset mask, never
// outside it, over many seeds.
void test_motif_onset_respects_idiom_mask() {
  const std::uint32_t four_on_floor = (1u << 0) | (1u << 4) | (1u << 8) | (1u << 12);
  for (std::uint32_t seed = 1; seed <= 50; ++seed) {
    const Motif m = motif::generate(seed, 4, four_on_floor, 0, 90, 200);
    CHECK(m.count == 4);
    for (std::uint8_t i = 0; i < m.count; ++i) {
      CHECK((four_on_floor & (1u << m.events[i].step)) != 0);
    }
  }
}

// A zero mask (the "no idiomatic drum pattern available" case) falls back to
// every 16th of the bar rather than producing nothing.
void test_motif_onset_falls_back_without_idiom_mask() {
  const Motif m = motif::generate(7, 6, /*allowed_steps=*/0, 0, 90, 200);
  CHECK(m.count == 6);
  for (std::uint8_t i = 0; i < m.count; ++i) {
    CHECK(m.events[i].step < kMaxMotifLen);
  }
}

// ---- Pure motif:: unit tests: transforms -----------------------------------

// §1b: retrograde is a pure step reflection, valid for every NoteSource --
// including a kFixed drum motif -- and never touches tone/vel/gate.
void test_motif_transform_retrograde_any_source() {
  Motif m;
  m.count = 3;
  m.events[0] = StyleEvent{.step = 0, .tone = 36, .octave = 0, .vel = 100, .gate = 200};
  m.events[1] = StyleEvent{.step = 4, .tone = 38, .octave = 0, .vel = 100, .gate = 200};
  m.events[2] = StyleEvent{.step = 8, .tone = 40, .octave = 0, .vel = 100, .gate = 200};
  const Motif r = motif::apply_transform_once(m, MotifTransform::kRetrograde, 0);
  CHECK(r.events[0].step == kMaxMotifLen - 1 - 0);
  CHECK(r.events[1].step == kMaxMotifLen - 1 - 4);
  CHECK(r.events[2].step == kMaxMotifLen - 1 - 8);
  CHECK(r.events[0].tone == 36 && r.events[1].tone == 38 && r.events[2].tone == 40);
}

// §1c: displacement wraps modulo the bar span.
void test_motif_transform_displacement_wraps() {
  Motif m;
  m.count = 2;
  m.events[0] = StyleEvent{.step = 0, .tone = 0, .octave = 0, .vel = 100, .gate = 200};
  m.events[1] = StyleEvent{.step = 14, .tone = 0, .octave = 0, .vel = 100, .gate = 200};
  const Motif d = motif::apply_transform_once(m, MotifTransform::kDisplacement, 4);
  CHECK(d.events[0].step == 4);
  CHECK(d.events[1].step == 2);  // 14 + 4 = 18 -> 18 mod 16 = 2
}

// §1a: diatonic transpose touches kScaleDegree/kInterval tone only; a
// kChordTone event passes through unmodified (scoped out this first slice --
// there, "tone" is a functional chord-tone index, not a melodic pitch).
void test_motif_transform_transpose_scopes_by_source() {
  Motif m;
  m.count = 2;
  m.events[0] = StyleEvent{
      .step = 0, .tone = 3, .octave = 0, .vel = 100, .gate = 200, .src = NoteSource::kScaleDegree};
  m.events[1] = StyleEvent{
      .step = 4, .tone = 1, .octave = 0, .vel = 100, .gate = 200, .src = NoteSource::kChordTone};
  const Motif t = motif::apply_transform_once(m, MotifTransform::kDiatonicTranspose, 2);
  CHECK(t.events[0].tone == 5);  // kScaleDegree: transposed
  CHECK(t.events[1].tone == 1);  // kChordTone: left unmodified
}

// §2.5: over many seeds, an "answer" repeat's transform must not trivially
// reproduce the seed's onset set.
void test_motif_anti_triviality_guard() {
  for (std::uint32_t seed = 1; seed <= 100; ++seed) {
    MotifSpec spec;
    spec.transform = MotifTransform::kDisplacement;
    spec.seed = seed;
    spec.length = 6;
    const Motif s = motif::generate(spec.seed, spec.length, 0, 0, 90, 200);
    const Motif answer = motif::apply_repeat(s, spec, /*repeat=*/1);
    CHECK(motif::onset_mask(answer) != motif::onset_mask(s));
  }
}

// §2.2: even repeats play the seed verbatim (the "statement"); odd repeats
// apply the configured transform (the "answer").
void test_motif_call_and_response_parity() {
  MotifSpec spec;
  spec.transform = MotifTransform::kDisplacement;
  spec.seed = 5;
  spec.length = 6;
  const Motif seed_motif = motif::generate(spec.seed, spec.length, 0, 0, 90, 200);
  const Motif even = motif::apply_repeat(seed_motif, spec, 0);
  const Motif odd = motif::apply_repeat(seed_motif, spec, 1);
  CHECK(motif::onset_mask(even) == motif::onset_mask(seed_motif));
  CHECK(motif::onset_mask(odd) != motif::onset_mask(seed_motif));
}

// An authored seed motif (Ottorino's "Option 1"): from_span copies it
// byte-for-byte, capped to kMaxMotifLen.
void test_motif_from_span_authored_seed() {
  constexpr StyleEvent kAuthored[] = {
      {.step = 0, .tone = 0, .octave = 0, .vel = 90, .gate = 200},
      {.step = 8, .tone = 2, .octave = 0, .vel = 90, .gate = 200},
  };
  const Motif m = motif::from_span(Span<const StyleEvent>(kAuthored));
  CHECK(m.count == 2);
  CHECK(m.events[0].step == 0 && m.events[0].tone == 0);
  CHECK(m.events[1].step == 8 && m.events[1].tone == 2);
}

// from_span caps an over-length authored span to kMaxMotifLen.
void test_motif_from_span_caps_length() {
  StyleEvent oversized[kMaxMotifLen + 4];
  for (int i = 0; i < kMaxMotifLen + 4; ++i) {
    oversized[i] = StyleEvent{.step = static_cast<std::uint16_t>(i % kMaxMotifLen),
                              .tone = 0,
                              .octave = 0,
                              .vel = 90,
                              .gate = 200};
  }
  const Motif m = motif::from_span(Span<const StyleEvent>(oversized, kMaxMotifLen + 4));
  CHECK(m.count == kMaxMotifLen);
}

// ---- Edge cases and defensive branches --------------------------------------
//
// A round-trip through a volatile local: several `motif::` entry points below
// are small enough that GCC's front end constant-folds a call whose arguments
// are ALL literal, at compile time, even unoptimized -- which would make the
// coverage build (a genuine correctness signal, not a style nicety) credit a
// branch that never actually ran as compiled object code. Routing one literal
// argument through a `volatile` read defeats that folding so these edge cases
// are provably exercised at RUNTIME, matching every other seed-driven test in
// this file (a loop variable already defeats folding on its own).
std::uint8_t no_fold(std::uint8_t v) noexcept {
  volatile std::uint8_t hold = v;
  return hold;
}
std::uint32_t no_fold(std::uint32_t v) noexcept {
  volatile std::uint32_t hold = v;
  return hold;
}
int no_fold(int v) noexcept {
  volatile int hold = v;
  return hold;
}

// idiom_onset_mask falls back to 0 when no kFixed pattern of that role exists.
void test_motif_idiom_mask_no_match() {
  static constexpr StyleEvent kLeadSeed[] = {
      {.step = 0, .tone = 0, .octave = 0, .vel = 90, .gate = 200}};
  constexpr StylePattern kPatterns[] = {
      {.role = TrackRole::kLead,
       .policy = RolePolicy::kChordTone,
       .events = Span<const StyleEvent>(kLeadSeed)},
  };
  CHECK(motif::idiom_onset_mask(Span<const StylePattern>(kPatterns), TrackRole::kDrums) == 0);
}

// generate() with length == 0 produces an empty motif (nothing to leap from
// or cadence on).
void test_motif_generate_zero_length() {
  const Motif m = motif::generate(1, /*length=*/no_fold(std::uint8_t{0}), 0, 0, 90, 200);
  CHECK(m.count == 0);
}

// generate() with an allowed-steps mask whose bits all sit past the one-bar
// range (defensive: candidate_count == 0) yields an empty motif rather than
// reading out of bounds.
void test_motif_generate_out_of_range_mask() {
  const Motif m =
      motif::generate(1, 4, /*allowed_steps=*/no_fold(std::uint32_t{1u << 20}), 0, 90, 200);
  CHECK(m.count == 0);
}

// apply_transform_once(kNone) is the identity -- exercised directly (not just
// through apply_repeat, where kNone never reaches the transform at all).
void test_motif_apply_transform_once_none_is_identity() {
  Motif m;
  m.count = 1;
  m.events[0] = StyleEvent{.step = 3, .tone = 5, .octave = 0, .vel = 90, .gate = 200};
  const Motif out = motif::apply_transform_once(m, MotifTransform::kNone, 42);
  CHECK(out.events[0].step == 3 && out.events[0].tone == 5);
}

// transform_amount's kDiatonicTranspose/kRetrograde/kNone branches, exercised
// directly (apply_repeat's own tests above only cover kDisplacement).
void test_motif_transform_amount_branches() {
  const int t =
      motif::transform_amount(MotifTransform::kDiatonicTranspose, no_fold(std::uint32_t{9}), 1);
  CHECK(t >= -3 && t <= 3);
  CHECK(motif::transform_amount(MotifTransform::kRetrograde, 9, 1) == 0);
  CHECK(motif::transform_amount(MotifTransform::kNone, 9, 1) == 0);
}

// displace()'s negative-wrap branch: a negative amount can push the
// intermediate sum below zero before the modulo correction re-enters [0,
// kMaxMotifLen). transform_amount never produces a negative displacement
// (repeat-keyed amounts are always 1..len-1), so this is only reachable via a
// hand-built (out-of-policy) amount -- defensive, exercised directly.
void test_motif_transform_displacement_negative_amount_wraps() {
  Motif m;
  m.count = 1;
  m.events[0] = StyleEvent{.step = 0, .tone = 0, .octave = 0, .vel = 90, .gate = 200};
  const Motif d = motif::apply_transform_once(m, MotifTransform::kDisplacement, no_fold(-3));
  CHECK(d.events[0].step == kMaxMotifLen - 3);  // -3 mod 16 = 13
}

// apply_transform's in.count <= 1 short-circuit: nothing to compare, so the
// anti-triviality guard is skipped entirely.
void test_motif_apply_transform_skips_guard_for_short_motif() {
  Motif m;
  m.count = 1;
  m.events[0] = StyleEvent{.step = 5, .tone = 0, .octave = 0, .vel = 90, .gate = 200};
  const Motif out = motif::apply_transform(m, MotifTransform::kDisplacement, 0, 123);
  CHECK(out.events[0].step == 5);  // amount 0 -> unchanged, guard never ran
}

// A retrograde-palindromic onset set (steps 0 and kMaxMotifLen-1 reflect onto
// each other) is trivial on EVERY attempt, since retrograde ignores `amount`:
// the anti-triviality guard exhausts all kMaxRetries and gives up honestly
// (motif-engine-scope.md §2.5's own documented bounded-search fallback).
void test_motif_apply_transform_retrograde_exhausts_retries() {
  Motif m;
  m.count = 2;
  m.events[0] = StyleEvent{.step = 0, .tone = 0, .octave = 0, .vel = 90, .gate = 200};
  m.events[1] =
      StyleEvent{.step = kMaxMotifLen - 1, .tone = 0, .octave = 0, .vel = 90, .gate = 200};
  const Motif out = motif::apply_transform(m, MotifTransform::kRetrograde, 0, 99);
  CHECK(motif::onset_mask(out) == motif::onset_mask(m));  // trivial, honestly returned
}

// apply_repeat also exercises kRetrograde end-to-end (the other tests above
// only drive kDisplacement) and covers apply_transform's normal (non-guard-
// skipped) retrograde path together with transform_amount's kRetrograde arm.
void test_motif_call_and_response_retrograde() {
  MotifSpec spec;
  spec.transform = MotifTransform::kRetrograde;
  spec.seed = 11;
  spec.length = 5;
  const Motif seed_motif = motif::generate(spec.seed, spec.length, 0, 0, 90, 200);
  const Motif odd = motif::apply_repeat(seed_motif, spec, 1);
  CHECK(motif::onset_mask(odd) != motif::onset_mask(seed_motif));
}

// ---- End-to-end golden through the REAL Arranger fire loop -----------------
//
// A drum (kFixed) four-on-the-floor pattern supplies the idiom onset mask
// (§2.3); a kLead pattern is motif-driven with no authored seed (`events` is
// empty), so its seed motif is GENERATED. Opt-in: neither pattern's presence
// nor the motif engine's own code path perturbs any OTHER existing style --
// the 21 pre-existing goldens (test_arranger.cpp et al.) stay byte-identical,
// asserted by simply not touching their fixtures.

constexpr StyleEvent kDrumFourOnFloor[] = {
    {.step = 0, .tone = styles::kKick, .octave = 0, .vel = 100, .gate = 100},
    {.step = 4, .tone = styles::kKick, .octave = 0, .vel = 100, .gate = 100},
    {.step = 8, .tone = styles::kKick, .octave = 0, .vel = 100, .gate = 100},
    {.step = 12, .tone = styles::kKick, .octave = 0, .vel = 100, .gate = 100},
};
constexpr MotifSpec kLeadMotifSpec{.transform = MotifTransform::kDisplacement,
                                   .seed = 777,
                                   .length = 4,
                                   .center_degree = 0,
                                   .vel = 90,
                                   .gate = 200,
                                   .idiom_role = TrackRole::kDrums};
constexpr StylePattern kMotifPatterns[] = {
    {.role = TrackRole::kDrums,
     .policy = RolePolicy::kFixed,
     .events = Span<const StyleEvent>(kDrumFourOnFloor)},
    {.role = TrackRole::kLead,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(),
     .motif = &kLeadMotifSpec},
};
constexpr StyleSection kMotifSections[] = {
    {.type = SectionType::kVarA, .bars = 1, .patterns = Span<const StylePattern>(kMotifPatterns)}};
constexpr Style kMotifStyle{.name = "motiftest",
                            .sections = Span<const StyleSection>(kMotifSections)};

struct Hit {
  Tick tick = 0;
  std::uint8_t note = 0;
};

// A SECOND fixture exercising Ottorino's "Option 1": an AUTHORED seed motif
// (`events` non-empty) instead of a generated one -- Arranger::on_tick's
// `pattern.events.empty() ? generate(...) : from_span(pattern.events)`
// branch takes the from_span arm here, never taken by the generated-seed
// fixture above.
constexpr StyleEvent kAuthoredLeadSeed[] = {
    {.step = 0, .tone = 0, .octave = 0, .vel = 90, .gate = 200, .src = NoteSource::kScaleDegree},
    {.step = 8, .tone = 2, .octave = 0, .vel = 90, .gate = 200, .src = NoteSource::kScaleDegree},
};
constexpr MotifSpec kAuthoredMotifSpec{
    .transform = MotifTransform::kDisplacement, .seed = 3, .idiom_role = TrackRole::kDrums};
constexpr StylePattern kAuthoredMotifPatterns[] = {
    {.role = TrackRole::kDrums,
     .policy = RolePolicy::kFixed,
     .events = Span<const StyleEvent>(kDrumFourOnFloor)},
    {.role = TrackRole::kLead,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kAuthoredLeadSeed),
     .motif = &kAuthoredMotifSpec},
};
constexpr StyleSection kAuthoredMotifSections[] = {
    {.type = SectionType::kVarA,
     .bars = 1,
     .patterns = Span<const StylePattern>(kAuthoredMotifPatterns)}};
constexpr Style kAuthoredMotifStyle{.name = "motifauthoredtest",
                                    .sections = Span<const StyleSection>(kAuthoredMotifSections)};

void test_motif_authored_seed_through_arranger() {
  Arranger a;
  CHECK(a.load_style(&kAuthoredMotifStyle));
  CHECK(a.set_route(TrackRole::kDrums, 0, 9));
  CHECK(a.set_route(TrackRole::kLead, 0, 3));
  a.on_transport_start();
  const Key c_major{.root_pc = 0, .mode = Mode::kMajor};
  const ChordState no_chord{};
  StaticVector<Hit, 16> hits;
  for (Tick t = 0; t < 2 * kTicksPerBar; ++t) {
    a.on_tick(t, c_major, no_chord, [&](std::uint8_t, TickOffset delay, const MidiMessage& msg) {
      if (msg.type() != midi::kNoteOn || delay != 0 || msg.channel() != 3) {
        return;
      }
      CHECK(hits.push_back(Hit{.tick = t, .note = msg.d1}));
    });
  }
  // The statement bar (repeat 0) plays the two authored onsets verbatim: step
  // 0 -> degree 0 -> C (72), step 8 -> degree 2 -> E (76) over C major.
  CHECK(hits.size() == 4);  // 2 onsets/bar x 2 bars
  CHECK(hits[0].tick == 0 && hits[0].note == 72);
  CHECK(hits[1].tick == 8 * kTicksPerStep && hits[1].note == 76);
}

StaticVector<Hit, 64> run_lead(std::uint32_t bars) {
  Arranger a;
  CHECK(a.load_style(&kMotifStyle));
  CHECK(a.set_route(TrackRole::kDrums, 0, 9));
  CHECK(a.set_route(TrackRole::kLead, 0, 3));
  a.on_transport_start();
  const Key c_major{.root_pc = 0, .mode = Mode::kMajor};
  const ChordState no_chord{};
  StaticVector<Hit, 64> hits;
  for (Tick t = 0; t < bars * kTicksPerBar; ++t) {
    a.on_tick(t, c_major, no_chord, [&](std::uint8_t, TickOffset delay, const MidiMessage& msg) {
      if (msg.type() != midi::kNoteOn || delay != 0 || msg.channel() != 3) {
        return;
      }
      CHECK(hits.push_back(Hit{.tick = t, .note = msg.d1}));
    });
  }
  return hits;
}

void test_motif_end_to_end_deterministic() {
  const auto h1 = run_lead(4);
  const auto h2 = run_lead(4);
  CHECK(h1.size() == h2.size());
  for (std::size_t i = 0; i < h1.size() && i < h2.size(); ++i) {
    CHECK(h1[i].tick == h2[i].tick && h1[i].note == h2[i].note);
  }
}

// The end-to-end golden itself (D16): a fixed sequence of (tick, note) pairs
// over 4 bars of the fixture above. Bars 1 & 3 (repeat 0, 2 -- even) play the
// generated seed motif verbatim; bars 2 & 4 (repeat 1, 3 -- odd) play it
// kDisplacement-transformed. Captured once from this exact fixture; a change
// here means the generator/transform themselves changed, not incidental
// drift -- exactly the golden-vs-property distinction motif-engine-scope.md
// §4 asks for.
void test_motif_end_to_end_golden() {
  const auto hits = run_lead(4);
  CHECK(hits.size() == 16);  // 4 onsets/bar x 4 bars
  // Bars 1 & 3 (repeat 0, 2 -- even, the "statement") play the generated seed
  // motif verbatim: onsets at local steps 0/4/8/12, degrees 0/-2/-4/-3 over C
  // major -> notes 72/69/65/67. Bars 2 & 4 (repeat 1, 3 -- odd, the "answer")
  // play the SAME pitches kDisplacement-shifted in time (+1, +6 steps resp.),
  // which for +6 wraps two onsets past the bar end and so reorders them by
  // absolute tick (67 sounds before 72/69/65 in bar 4).
  constexpr std::uint8_t kExpectedNotes[16] = {72, 69, 65, 67, 72, 69, 65, 67,
                                               72, 69, 65, 67, 67, 72, 69, 65};
  constexpr Tick kExpectedTicks[16] = {0,    960,  1920, 2880,  4080,  5040,  6000,  6960,
                                       7680, 8640, 9600, 10560, 12000, 12960, 13920, 14880};
  for (std::size_t i = 0; i < hits.size() && i < 16; ++i) {
    CHECK(hits[i].tick == kExpectedTicks[i]);
    CHECK(hits[i].note == kExpectedNotes[i]);
  }
  // Bars 1 and 3 (the statement) share the same onset-to-bar-local-step
  // layout; bar 2 (the answer) does not -- call-and-response proven
  // end-to-end (rhythmic displacement, not a pitch change), not just at the
  // pure motif:: layer.
  const Tick bar1_first_local = hits[0].tick % kTicksPerBar;
  const Tick bar2_first_local = hits[4].tick % kTicksPerBar;
  const Tick bar3_first_local = hits[8].tick % kTicksPerBar;
  CHECK(bar1_first_local == bar3_first_local);  // statement repeats identically
  CHECK(bar1_first_local != bar2_first_local);  // answer is rhythmically displaced
}

}  // namespace

int main() {
  test_motif_generate_deterministic();
  test_motif_generate_property_leap_and_cadence();
  test_motif_onset_respects_idiom_mask();
  test_motif_onset_falls_back_without_idiom_mask();
  test_motif_transform_retrograde_any_source();
  test_motif_transform_displacement_wraps();
  test_motif_transform_transpose_scopes_by_source();
  test_motif_anti_triviality_guard();
  test_motif_call_and_response_parity();
  test_motif_from_span_authored_seed();
  test_motif_from_span_caps_length();
  test_motif_idiom_mask_no_match();
  test_motif_generate_zero_length();
  test_motif_generate_out_of_range_mask();
  test_motif_apply_transform_once_none_is_identity();
  test_motif_transform_amount_branches();
  test_motif_transform_displacement_negative_amount_wraps();
  test_motif_apply_transform_skips_guard_for_short_motif();
  test_motif_apply_transform_retrograde_exhausts_retries();
  test_motif_call_and_response_retrograde();
  test_motif_authored_seed_through_arranger();
  test_motif_end_to_end_deterministic();
  test_motif_end_to_end_golden();
  return arrangrr::test::failures();
}
