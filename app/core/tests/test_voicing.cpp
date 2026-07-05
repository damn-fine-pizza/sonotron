#include "test.hpp"

#include "arrangrr/arranger/voicing.hpp"

using namespace arrangrr;

namespace {

NoteReq chord_tone(int note) {
  return NoteReq{.note = note, .vel = 100, .gate = 240, .gesture_delay = 0, .chord_tone = true};
}

}  // namespace

// kAsWritten is a pure identity — it must not move any note. This is the
// byte-identical baseline every existing style relies on (all default here).
static void test_voicing_as_written_identity() {
  VoicingState v;
  v.reset();
  NoteReq reqs[3] = {chord_tone(60), chord_tone(64), chord_tone(67)};
  v.voice(TrackRole::kChord1, VoicingPolicy::kAsWritten, reqs, 3);
  CHECK(reqs[0].note == 60);
  CHECK(reqs[1].note == 64);
  CHECK(reqs[2].note == 67);
  CHECK(reqs[0].vel == 100);
  CHECK(reqs[0].gate == 240);
}

// The first chord after a reset has nothing to lead from: kLead keeps the
// authored register untouched (and records it for the next chord).
static void test_voicing_lead_first_chord_identity() {
  VoicingState v;
  v.reset();
  NoteReq reqs[3] = {chord_tone(48), chord_tone(52), chord_tone(55)};  // C major
  v.voice(TrackRole::kChord1, VoicingPolicy::kLead, reqs, 3);
  CHECK(reqs[0].note == 48);
  CHECK(reqs[1].note == 52);
  CHECK(reqs[2].note == 55);
}

// The second chord leads: each slot moves to the nearest octave of its previous
// pitch. Previous voicing {48,52,55}; a high {69,72,76} target collapses down to
// the nearest octave per slot -> {45,48,52}.
static void test_voicing_lead_nearest_octave() {
  VoicingState v;
  v.reset();
  NoteReq first[3] = {chord_tone(48), chord_tone(52), chord_tone(55)};
  v.voice(TrackRole::kChord1, VoicingPolicy::kLead, first, 3);

  NoteReq second[3] = {chord_tone(69), chord_tone(72), chord_tone(76)};
  v.voice(TrackRole::kChord1, VoicingPolicy::kLead, second, 3);
  CHECK(second[0].note == 45);  // 69 -> nearest octave of A to 48
  CHECK(second[1].note == 48);  // 72 -> nearest octave of C to 52
  CHECK(second[2].note == 52);  // 76 -> nearest octave of E to 55
}

// A held common tone does not move at all: same pitch class already within a
// tritone of the previous slot stays put.
static void test_voicing_lead_common_tone_held() {
  VoicingState v;
  v.reset();
  NoteReq first[3] = {chord_tone(60), chord_tone(64), chord_tone(67)};  // C major
  v.voice(TrackRole::kChord1, VoicingPolicy::kLead, first, 3);

  // C E G -> C F A (F major, same register). Slot0 C stays; E->F up 1; G->A up 2.
  NoteReq second[3] = {chord_tone(60), chord_tone(65), chord_tone(69)};
  v.voice(TrackRole::kChord1, VoicingPolicy::kLead, second, 3);
  CHECK(second[0].note == 60);  // common tone C: no motion
  CHECK(second[1].note == 65);
  CHECK(second[2].note == 69);
}

// Melodic / drum notes (chord_tone == false) are never re-voiced, even under
// kLead — only chord tones are eligible.
static void test_voicing_lead_skips_non_chord_tones() {
  VoicingState v;
  v.reset();
  NoteReq first[1] = {chord_tone(60)};
  v.voice(TrackRole::kChord1, VoicingPolicy::kLead, first, 1);

  NoteReq second[2] = {
      NoteReq{.note = 84, .vel = 100, .gate = 240, .gesture_delay = 0, .chord_tone = false},
      chord_tone(72),
  };
  v.voice(TrackRole::kChord1, VoicingPolicy::kLead, second, 2);
  CHECK(second[0].note == 84);  // melodic: untouched
  CHECK(second[1].note == 60);  // chord tone C: led down to nearest octave of prev 60
}

// reset() clears the memory: after it, the next chord is a fresh "first chord"
// and keeps its authored register.
static void test_voicing_reset_clears_memory() {
  VoicingState v;
  v.reset();
  NoteReq first[3] = {chord_tone(48), chord_tone(52), chord_tone(55)};
  v.voice(TrackRole::kChord1, VoicingPolicy::kLead, first, 3);
  v.reset();
  NoteReq again[3] = {chord_tone(72), chord_tone(76), chord_tone(79)};
  v.voice(TrackRole::kChord1, VoicingPolicy::kLead, again, 3);
  CHECK(again[0].note == 72);  // fresh: authored register kept, no lead-down
  CHECK(again[1].note == 76);
  CHECK(again[2].note == 79);
}

// A step with NO chord tones (a lone melodic note under kLead) must not erase
// the voicing memory: the next chord still leads from the previous voicing, as
// if the melodic step never happened.
static void test_voicing_melodic_step_preserves_memory() {
  VoicingState v;
  v.reset();
  NoteReq first[3] = {chord_tone(48), chord_tone(52), chord_tone(55)};  // C major, recorded
  v.voice(TrackRole::kChord1, VoicingPolicy::kLead, first, 3);

  NoteReq melodic[1] = {
      NoteReq{.note = 84, .vel = 100, .gate = 240, .gesture_delay = 0, .chord_tone = false}};
  v.voice(TrackRole::kChord1, VoicingPolicy::kLead, melodic, 1);
  CHECK(melodic[0].note == 84);  // melodic note untouched

  NoteReq third[3] = {chord_tone(69), chord_tone(72), chord_tone(76)};
  v.voice(TrackRole::kChord1, VoicingPolicy::kLead, third, 3);
  CHECK(third[0].note == 45);  // still leads from {48,52,55}, not a wiped memory
  CHECK(third[1].note == 48);
  CHECK(third[2].note == 52);
}

int main() {
  test_voicing_as_written_identity();
  test_voicing_lead_first_chord_identity();
  test_voicing_lead_nearest_octave();
  test_voicing_lead_common_tone_held();
  test_voicing_lead_skips_non_chord_tones();
  test_voicing_reset_clears_memory();
  test_voicing_melodic_step_preserves_memory();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_voicing: all OK\n");
  }
  return arrangrr::test::failures();
}
