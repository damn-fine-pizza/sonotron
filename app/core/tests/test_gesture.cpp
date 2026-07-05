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

// A live C major triad -> exactly 3 chord tones (root/third/fifth). Shared by
// the strum/roll cases so the expected counts and delays are unambiguous.
static ChordState c_major_triad() {
  return ChordState{.root_pc = 0, .quality = ChordQuality::kMaj, .valid = true};
}

// kStrumUp: tones low->high (0,1,2), fixed micro-stagger per tone. Base event's
// octave/vel/gate carry through; produced specs are terminal chord tones.
static void test_strum_up() {
  const StyleEvent ev{.step = 8,
                      .tone = 0,
                      .octave = 2,
                      .vel = 90,
                      .gate = 240,
                      .src = NoteSource::kChordTone,
                      .gesture = ChordGesture::kStrumUp};
  const StylePattern pat{
      .role = TrackRole::kChord1, .policy = RolePolicy::kChordTone, .events = {}};

  StyleEvent specs[gesture::kMaxGestureFan];
  TickOffset delays[gesture::kMaxGestureFan];
  const int n = gesture::expand(pat, ev, c_major_triad(), specs, delays);

  CHECK(n == 3);
  for (int i = 0; i < n; ++i) {
    CHECK(specs[i].tone == i);
    CHECK(specs[i].src == NoteSource::kChordTone);
    CHECK(specs[i].gesture == ChordGesture::kNone);
    CHECK(specs[i].octave == ev.octave);
    CHECK(specs[i].vel == ev.vel);
    CHECK(specs[i].gate == ev.gate);
    CHECK(delays[i] == i * gesture::kStrumStaggerTicks);
  }
}

// kStrumDown: tones high->low (2,1,0), delays still increase in strum order.
static void test_strum_down() {
  const StyleEvent ev{.step = 8,
                      .tone = 0,
                      .octave = 0,
                      .vel = 110,
                      .gate = 200,
                      .src = NoteSource::kChordTone,
                      .gesture = ChordGesture::kStrumDown};
  const StylePattern pat{
      .role = TrackRole::kChord1, .policy = RolePolicy::kChordTone, .events = {}};

  StyleEvent specs[gesture::kMaxGestureFan];
  TickOffset delays[gesture::kMaxGestureFan];
  const int n = gesture::expand(pat, ev, c_major_triad(), specs, delays);

  CHECK(n == 3);
  CHECK(specs[0].tone == 2);
  CHECK(specs[1].tone == 1);
  CHECK(specs[2].tone == 0);
  for (int i = 0; i < n; ++i) {
    CHECK(specs[i].src == NoteSource::kChordTone);
    CHECK(delays[i] == i * gesture::kStrumStaggerTicks);
  }
}

// kRollUp: tones ascending, spread evenly across the gate (240/3 = 80/tone).
static void test_roll_up() {
  const StyleEvent ev{.step = 0,
                      .tone = 0,
                      .octave = 1,
                      .vel = 100,
                      .gate = 240,
                      .src = NoteSource::kChordTone,
                      .gesture = ChordGesture::kRollUp};
  const StylePattern pat{
      .role = TrackRole::kChord1, .policy = RolePolicy::kChordTone, .events = {}};

  StyleEvent specs[gesture::kMaxGestureFan];
  TickOffset delays[gesture::kMaxGestureFan];
  const int n = gesture::expand(pat, ev, c_major_triad(), specs, delays);

  CHECK(n == 3);
  CHECK(specs[0].tone == 0);
  CHECK(specs[1].tone == 1);
  CHECK(specs[2].tone == 2);
  CHECK(delays[0] == 0);
  CHECK(delays[1] == 80);
  CHECK(delays[2] == 160);
}

// kRollDown: tones descending, same even sub-gate spread.
static void test_roll_down() {
  const StyleEvent ev{.step = 0,
                      .tone = 0,
                      .octave = 1,
                      .vel = 100,
                      .gate = 240,
                      .src = NoteSource::kChordTone,
                      .gesture = ChordGesture::kRollDown};
  const StylePattern pat{
      .role = TrackRole::kChord1, .policy = RolePolicy::kChordTone, .events = {}};

  StyleEvent specs[gesture::kMaxGestureFan];
  TickOffset delays[gesture::kMaxGestureFan];
  const int n = gesture::expand(pat, ev, c_major_triad(), specs, delays);

  CHECK(n == 3);
  CHECK(specs[0].tone == 2);
  CHECK(specs[1].tone == 1);
  CHECK(specs[2].tone == 0);
  CHECK(delays[0] == 0);
  CHECK(delays[1] == 80);
  CHECK(delays[2] == 160);
}

// A seventh chord has 4 tones: the fan grows to 4 and the roll sub-step shrinks
// (240/4 = 60), proving the count is driven by shape_of(quality).count.
static void test_seventh_chord_fan() {
  const StyleEvent ev{.step = 0,
                      .tone = 0,
                      .octave = 1,
                      .vel = 100,
                      .gate = 240,
                      .src = NoteSource::kChordTone,
                      .gesture = ChordGesture::kRollUp};
  const StylePattern pat{
      .role = TrackRole::kChord1, .policy = RolePolicy::kChordTone, .events = {}};
  const ChordState maj7{.root_pc = 0, .quality = ChordQuality::kMaj7, .valid = true};

  StyleEvent specs[gesture::kMaxGestureFan];
  TickOffset delays[gesture::kMaxGestureFan];
  const int n = gesture::expand(pat, ev, maj7, specs, delays);

  CHECK(n == 4);
  for (int i = 0; i < n; ++i) {
    CHECK(specs[i].tone == i);
    CHECK(delays[i] == i * 60);
  }

  // The same seventh chord strummed up: 4 tones, fixed stagger.
  const StyleEvent strum{.step = 0,
                         .tone = 0,
                         .octave = 1,
                         .vel = 100,
                         .gate = 240,
                         .src = NoteSource::kChordTone,
                         .gesture = ChordGesture::kStrumUp};
  const int m = gesture::expand(pat, strum, maj7, specs, delays);
  CHECK(m == 4);
  CHECK(specs[3].tone == 3);
  CHECK(delays[3] == 3 * gesture::kStrumStaggerTicks);
}

// No live chord -> the gesture degenerates to the kNone passthrough (count 1,
// original event, zero delay). Guards resolve() from ever seeing phantom tones.
static void test_invalid_chord_fallback() {
  const StyleEvent ev{.step = 4,
                      .tone = 2,
                      .octave = 1,
                      .vel = 100,
                      .gate = 240,
                      .src = NoteSource::kChordTone,
                      .gesture = ChordGesture::kStrumUp};
  const StylePattern pat{
      .role = TrackRole::kChord1, .policy = RolePolicy::kChordTone, .events = {}};
  const ChordState no_chord{.root_pc = 0, .quality = ChordQuality::kMaj, .valid = false};

  StyleEvent specs[gesture::kMaxGestureFan];
  TickOffset delays[gesture::kMaxGestureFan];
  const int n = gesture::expand(pat, ev, no_chord, specs, delays);

  CHECK(n == 1);
  CHECK(specs[0].tone == ev.tone);
  CHECK(specs[0].octave == ev.octave);
  CHECK(specs[0].vel == ev.vel);
  CHECK(specs[0].gate == ev.gate);
  CHECK(specs[0].src == ev.src);
  CHECK(delays[0] == 0);
}

// A gesture on a kFixed (drum) part must degenerate to the passthrough: a
// gesture-tagged kick stays ONE literal drum note, never chord tones 0..count-1
// (resolve() ignores src for kFixed, so a strum there would emit subsonic junk).
static void test_gesture_fixed_role_passthrough() {
  const StyleEvent ev{.step = 0,
                      .tone = styles::kKick,
                      .octave = 0,
                      .vel = 100,
                      .gate = 60,
                      .src = NoteSource::kChordTone,
                      .gesture = ChordGesture::kStrumUp};
  const StylePattern pat{.role = TrackRole::kDrums, .policy = RolePolicy::kFixed, .events = {}};
  StyleEvent specs[gesture::kMaxGestureFan];
  TickOffset delays[gesture::kMaxGestureFan];
  const int n = gesture::expand(pat, ev, c_major_triad(), specs, delays);
  CHECK(n == 1);
  CHECK(specs[0].tone == styles::kKick);
  CHECK(delays[0] == 0);
}

// A gesture on a melodic (scale-degree/interval) event must also pass through:
// the gesture fans the chord, it must not silently rewrite a melodic line.
static void test_gesture_melodic_passthrough() {
  const StyleEvent ev{.step = 0,
                      .tone = 4,
                      .octave = 0,
                      .vel = 100,
                      .gate = 240,
                      .src = NoteSource::kScaleDegree,
                      .gesture = ChordGesture::kStrumUp};
  const StylePattern pat{.role = TrackRole::kLead, .policy = RolePolicy::kChordTone, .events = {}};
  StyleEvent specs[gesture::kMaxGestureFan];
  TickOffset delays[gesture::kMaxGestureFan];
  const int n = gesture::expand(pat, ev, c_major_triad(), specs, delays);
  CHECK(n == 1);
  CHECK(specs[0].tone == 4);
  CHECK(specs[0].src == NoteSource::kScaleDegree);
  CHECK(delays[0] == 0);
}

int main() {
  test_gesture_none_passthrough();
  test_strum_up();
  test_strum_down();
  test_roll_up();
  test_roll_down();
  test_seventh_chord_fan();
  test_invalid_chord_fallback();
  test_gesture_fixed_role_passthrough();
  test_gesture_melodic_passthrough();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_gesture: all OK\n");
  }
  return arrangrr::test::failures();
}
