// Torquato QA regression pin (Phase-6 Theme 3 Item #1, docs/reflections/
// phase6-theme3-master-transpose-scope.md): a LIVE master_transpose change
// on a VoicingPolicy::kLead role can be silently folded by an octave (or
// more) by VoicingState::voice()'s nearest_octave() step, producing a
// sounded note whose offset from the pre-transpose note is NOT the semitone
// value the player just set -- for some deltas the effective shift is even
// the WRONG SIGN. Currently RED -- pins an open, unfixed defect; do not
// silence it. See test_master_transpose.cpp / test_arranger.cpp
// (test_master_transpose_resolve) for the surrounding GREEN coverage of
// resolve()'s own late-offset arithmetic and the drop-not-fold boundary,
// which are correct in isolation -- this defect lives one stage downstream,
// in the voice-leading re-octaving that runs AFTER resolve().
//
// Root cause: Arranger::on_tick's D40 pipeline calls resolve() (which adds
// m_master_transpose to the absolute note) BEFORE m_voicing.voice()
// (arranger.hpp: "m_voicing.voice(pattern.role, pattern.voicing, group,
// count);" runs on the already-transposed notes). VoicingState::voice()'s
// kLead policy re-octaves each chord tone via nearest_octave() (voicing.hpp)
// to sit within a tritone of that SAME SLOT's remembered previous pitch
// (m_last[r][slot]) -- but m_last was recorded under whatever transpose was
// in effect the LAST time that role sounded. set_master_transpose() never
// resets VoicingState (Arranger::set_master_transpose, arranger.hpp, touches
// only m_master_transpose; only load_style()/on_transport_start() call
// m_voicing.reset()). So a live transpose delta greater than a tritone (6
// semitones) from the previous voicing gets folded by a whole octave (12) by
// nearest_octave(), leaving an effective, AUDIBLE shift of (delta - 12) or
// (delta + 12) instead of the requested delta -- exactly the "register-fold
// hazard" the design doc (Decision 2) warned about for the EARLY
// (pitch-class) option, resurfacing at the voicing layer for the shipped
// LATE (absolute-note) option because the offset is not actually "a
// constant added once at the earliest point notes exist" (the design doc's
// own justification for skipping a VoicingState change) -- it is a LIVE,
// player-mutable value, and nothing invalidates or re-seeds the per-role
// voicing memory when it changes.
//
// This test is a musical-scope regression matching test_voicing.cpp's own
// documented mechanics (test_voicing_lead_nearest_octave: a >6-semitone jump
// between successive kLead-voiced chords collapses by an octave) -- proven
// here to be REACHABLE through Arranger::on_tick via nothing more exotic
// than the player turning the live transpose control between two bars,
// which is exactly the shipped `transpose <-12..12>` verb's own use case.

#include "arrangrr/arranger/arranger.hpp"

#include "arrangrr/common/static_vector.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;

// One kLead-voicing role (mirrors chord1/pad in the real builtin styles),
// three chord tones (root/third/fifth) authored at step 0 of a one-bar
// section that loops -- so on_tick(0, ...) and on_tick(kTicksPerBar, ...)
// both resolve the SAME step against the SAME chord, isolating the voicing
// stage's own behavior across a live transpose change from any other
// variable (chord change, section change, gesture fan-out).
constexpr StyleEvent kChordToneEvents[] = {
    {.step = 0, .tone = 0, .octave = 0, .vel = 100, .gate = 200, .src = NoteSource::kChordTone},
    {.step = 0, .tone = 1, .octave = 0, .vel = 100, .gate = 200, .src = NoteSource::kChordTone},
    {.step = 0, .tone = 2, .octave = 0, .vel = 100, .gate = 200, .src = NoteSource::kChordTone},
};
constexpr StylePattern kLeadVoicedPattern[] = {{.role = TrackRole::kChord1,
                                                .policy = RolePolicy::kChordTone,
                                                .events = Span<const StyleEvent>(kChordToneEvents),
                                                .voicing = VoicingPolicy::kLead}};
constexpr StyleSection kLeadVoicedSections[] = {
    {.type = SectionType::kVarA,
     .bars = 1,
     .patterns = Span<const StylePattern>(kLeadVoicedPattern)}};
constexpr Style kLeadVoicedStyle{.name = "probe_lead_voicing",
                                 .sections = Span<const StyleSection>(kLeadVoicedSections)};

StaticVector<std::uint8_t, 8> notes_on(Arranger& a, Tick tick, const Key& key,
                                       const ChordState& chord) {
  StaticVector<std::uint8_t, 8> out;
  a.on_tick(tick, key, chord, [&](std::uint8_t, TickOffset, const MidiMessage& m, std::uint8_t) {
    if (m.type() == midi::kNoteOn) {
      CHECK(out.push_back(m.d1));
    }
  });
  return out;
}

// A live transpose of +8 semitones, applied between two bars of the SAME
// chord on a kLead-voiced role, must shift EVERY sounded chord tone by
// EXACTLY +8 -- that is the entire contract of kMasterTranspose ("a signed
// semitone offset applied late... 0 is a no-op", abi.hpp). +8 is chosen
// specifically because it exceeds nearest_octave()'s tritone (6-semitone)
// fold threshold from the anchor register the first (untransposed) bar
// seeds, so a real fold bug is forced to manifest, not just theoretically
// possible.
void test_live_transpose_change_is_not_folded_by_lead_voicing() {
  const Key c_major{.root_pc = 0, .mode = Mode::kMajor};
  const ChordState c_maj{.root_pc = 0, .quality = ChordQuality::kMaj, .valid = true};

  Arranger a;
  CHECK(a.load_style(&kLeadVoicedStyle));
  CHECK(a.set_route(TrackRole::kChord1, 0, 0));
  a.on_transport_start();

  // Bar 1: transpose 0, seeds VoicingState's per-role memory at the
  // untransposed register (kChord1 anchor 60 -> {60, 64, 67} for a C major
  // triad).
  const StaticVector<std::uint8_t, 8> baseline = notes_on(a, 0, c_major, c_maj);
  CHECK(baseline.size() == 3);

  // Bar 2: SAME chord, SAME step -- only the live transpose changes, exactly
  // as a player moving the Transport panel's Transpose slider mid-song
  // would drive it (transport_panel.cpp's fire-and-forget `transpose N`
  // verb -> Engine::cmd_master_transpose -> Arranger::set_master_transpose).
  a.set_master_transpose(8);
  const StaticVector<std::uint8_t, 8> transposed = notes_on(a, kTicksPerBar, c_major, c_maj);
  CHECK(transposed.size() == baseline.size());

  // The defect: nearest_octave() (voicing.hpp) pulls each transposed tone
  // back toward bar 1's remembered voicing, folding the +8 delta by a full
  // octave wherever the transposed note lands more than a tritone from its
  // remembered slot. Correct behavior is transposed[i] == baseline[i] + 8
  // for every slot; the shipped code instead produces transposed[i] ==
  // baseline[i] - 4 (i.e. resolve()'s correct +8 gets silently converted
  // into an audible -4 by the very next pipeline stage).
  for (std::size_t i = 0; i < baseline.size(); ++i) {
    CHECK(transposed[i] == static_cast<std::uint8_t>(baseline[i] + 8));
  }
}

}  // namespace

int main() {
  test_live_transpose_change_is_not_folded_by_lead_voicing();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_master_transpose_voicing_regression: all OK\n");
  }
  return arrangrr::test::failures();
}
