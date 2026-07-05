#include "test.hpp"

#include "arrangrr/arranger/voicing.hpp"

using namespace arrangrr;

// Scaffold contract (D40): kAsWritten voicing is a pure identity — it must not
// move any note. This is the byte-identical baseline the Phase 1 kLead
// voice-leading must preserve for kAsWritten while smoothing only under kLead.
static void test_voicing_as_written_identity() {
  VoicingState v;
  v.reset();
  NoteReq reqs[3] = {
      {.note = 60, .vel = 100, .gate = 240, .gesture_delay = 0, .chord_tone = true},
      {.note = 64, .vel = 100, .gate = 240, .gesture_delay = 0, .chord_tone = true},
      {.note = 67, .vel = 100, .gate = 240, .gesture_delay = 0, .chord_tone = true},
  };
  v.voice(TrackRole::kChord1, VoicingPolicy::kAsWritten, reqs, 3);

  CHECK(reqs[0].note == 60);
  CHECK(reqs[1].note == 64);
  CHECK(reqs[2].note == 67);
  CHECK(reqs[0].vel == 100);
  CHECK(reqs[0].gate == 240);
}

int main() {
  test_voicing_as_written_identity();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_voicing: all OK\n");
  }
  return arrangrr::test::failures();
}
