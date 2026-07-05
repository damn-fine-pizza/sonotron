#include "test.hpp"

#include "arrangrr/arranger/gesture.hpp"

using namespace arrangrr;

// Scaffold contract (D40): the gesture stage's default (kNone — and, until the
// concrete variants land in Phase 1, every value) passes exactly one event
// through unchanged, with zero extra delay. This locks the byte-identical
// baseline the real strum/roll/arpeggiate gestures must extend without breaking.
static void test_gesture_none_passthrough() {
  const StyleEvent ev{.step = 4,
                      .tone = 2,
                      .octave = 1,
                      .vel = 100,
                      .gate = 240,
                      .src = NoteSource::kChordTone,
                      .gesture = ChordGesture::kNone};
  const StylePattern pat{
      .role = TrackRole::kChord1, .policy = RolePolicy::kChordTone, .events = {}};
  const ChordState chord{.root_pc = 0, .quality = ChordQuality::kMaj, .valid = true};

  StyleEvent specs[gesture::kMaxGestureFan];
  TickOffset delays[gesture::kMaxGestureFan];
  const int n = gesture::expand(pat, ev, chord, specs, delays);

  CHECK(n == 1);
  CHECK(specs[0].tone == ev.tone);
  CHECK(specs[0].octave == ev.octave);
  CHECK(specs[0].vel == ev.vel);
  CHECK(specs[0].gate == ev.gate);
  CHECK(specs[0].src == ev.src);
  CHECK(delays[0] == 0);
}

int main() {
  test_gesture_none_passthrough();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_gesture: all OK\n");
  }
  return arrangrr::test::failures();
}
