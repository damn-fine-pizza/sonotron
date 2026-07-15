// Unit tests for the LoopBuffer payload primitives (Phase 7, node 6000, the
// Looper -- docs/reflections/phase7-scope-6000-8100-clip-timeline-seam.md,
// SLICE 1): LoopEvent's decompose_note()/resolve_note() round-trip (Fork A,
// the chord-relative capture) and LoopClip's own record/quantize/length-mode/
// remove surface (mirrors test_chord_seq.cpp's ChordSequence-only unit
// coverage, no Engine needed here -> unit).

#include "arrangrr/loop/loop_event.hpp"

#include "test.hpp"

namespace {

using namespace arrangrr;

// C major, no chord sounding: only key/scale-degree or interval fallback
// paths are reachable.
constexpr Key kCMajor{.root_pc = 0, .mode = Mode::kMajor};

void test_decompose_resolve_chord_tone_round_trip() {
  // Chord = C major (root_pc 0, offsets {0,4,7}). note=100: 100-0-7=93,
  // 93%12 != 0; 100-0-4=96, 96%12==0 -> chord-tone index 1 (the third),
  // octave 8.
  const ChordState chord{.root_pc = 0, .quality = ChordQuality::kMaj, .valid = true};
  std::int8_t tone = 0;
  std::int8_t octave = 0;
  LoopNoteSource source = LoopNoteSource::kInterval;
  decompose_note(100, chord, kCMajor, tone, octave, source);
  CHECK(source == LoopNoteSource::kChordTone);
  CHECK(tone == 1);
  CHECK(octave == 8);
  const LoopEvent ev{.start = 0,
                     .duration = 10,
                     .tone = tone,
                     .octave = octave,
                     .velocity = 100,
                     .source = source};
  CHECK(resolve_note(ev, chord, kCMajor) == 100);
}

void test_decompose_resolve_scale_degree_fallback() {
  // No chord sounding: a diatonic note (D, degree 1 in C major) falls
  // through to the scale-degree branch. 62 = key.root_pc(0) +
  // degree_to_semitones(major, 1)=2 + 12*5 -> octave 5.
  const ChordState no_chord{};  // valid == false
  std::int8_t tone = 0;
  std::int8_t octave = 0;
  LoopNoteSource source = LoopNoteSource::kChordTone;
  decompose_note(62, no_chord, kCMajor, tone, octave, source);
  CHECK(source == LoopNoteSource::kScaleDegree);
  CHECK(tone == 1);  // degree 1 (D)
  CHECK(octave == 5);
  const LoopEvent ev{.start = 0,
                     .duration = 10,
                     .tone = tone,
                     .octave = octave,
                     .velocity = 100,
                     .source = source};
  CHECK(resolve_note(ev, no_chord, kCMajor) == 62);
}

void test_decompose_resolve_interval_fallback_chromatic() {
  // No chord, and the note is chromatic to the key (C# in C major): the
  // interval fallback ALWAYS succeeds.
  const ChordState no_chord{};
  std::int8_t tone = 0;
  std::int8_t octave = 0;
  LoopNoteSource source = LoopNoteSource::kChordTone;
  decompose_note(61, no_chord, kCMajor, tone, octave, source);
  CHECK(source == LoopNoteSource::kInterval);
  CHECK(tone == 61);  // anchor is key.root_pc (0) since no chord
  CHECK(octave == 0);
  const LoopEvent ev{.start = 0,
                     .duration = 10,
                     .tone = tone,
                     .octave = octave,
                     .velocity = 100,
                     .source = source};
  CHECK(resolve_note(ev, no_chord, kCMajor) == 61);
}

void test_resolve_reharmonizes_on_chord_change() {
  // The whole point of Fork A: a chord-tone event resolves against whatever
  // chord is CURRENT at playback time, not the one it was captured under.
  // note=4: chord-tone index 1 (the third of C major, offset 4), octave 0.
  const ChordState c_maj{.root_pc = 0, .quality = ChordQuality::kMaj, .valid = true};
  std::int8_t tone = 0;
  std::int8_t octave = 0;
  LoopNoteSource source = LoopNoteSource::kInterval;
  decompose_note(4, c_maj, kCMajor, tone, octave, source);
  CHECK(source == LoopNoteSource::kChordTone && tone == 1 && octave == 0);
  const LoopEvent ev{.start = 0,
                     .duration = 10,
                     .tone = tone,
                     .octave = octave,
                     .velocity = 100,
                     .source = source};
  CHECK(resolve_note(ev, c_maj, kCMajor) == 4);
  // Re-harmonize to G major: the SAME captured "chord-tone index 1" now
  // resolves to the third of G major (11), no re-capture needed.
  const ChordState g_maj{.root_pc = 7, .quality = ChordQuality::kMaj, .valid = true};
  CHECK(resolve_note(ev, g_maj, kCMajor) == 11);
}

void test_resolve_drops_chord_tone_with_no_chord() {
  const LoopEvent ev{.start = 0,
                     .duration = 10,
                     .tone = 0,
                     .octave = 0,
                     .velocity = 100,
                     .source = LoopNoteSource::kChordTone};
  const ChordState no_chord{};
  CHECK(resolve_note(ev, no_chord, kCMajor) == -1);
}

void test_loop_clip_record_length_and_quantize() {
  LoopClip clip;
  // The FIRST pushed event is the one that reaches furthest -- proves
  // content_length() scans every event rather than trusting the last one
  // pushed (unlike ChordSequence::length(), valid only for its strictly-
  // sequential single-voice chain; a LoopClip is polyphonic/unordered).
  CHECK(clip.record(LoopEvent{.start = 0, .duration = kTicksPerBar}));  // end = 1 bar (the max)
  CHECK(clip.record(LoopEvent{.start = 100, .duration = 200}));         // end = 300
  CHECK(clip.record(LoopEvent{.start = 50, .duration = 100}));          // end = 150
  CHECK(clip.count() == 3);
  CHECK(clip.content_length() == kTicksPerBar);
  clip.quantize();
  // Each event snaps independently: start -> nearest grid, duration -> nearest
  // grid (min one grid unit).
  for (std::size_t i = 0; i < clip.count(); ++i) {
    CHECK(clip.event(i).start % kTicksPerBar == 0);
    CHECK(clip.event(i).duration >= kTicksPerBar);
  }
}

void test_loop_clip_length_modes() {
  LoopClip clip;
  CHECK(clip.record(LoopEvent{.start = 0, .duration = kTicksPerBar}));
  CHECK(clip.record(LoopEvent{.start = kTicksPerBar, .duration = kTicksPerBar / 2}));
  CHECK(clip.length_mode == LoopLengthMode::kAuto);
  CHECK(clip.length() == kTicksPerBar + kTicksPerBar / 2);  // furthest end

  clip.length_mode = LoopLengthMode::kFixed;
  clip.fixed_length = 4 * kTicksPerBar;
  CHECK(clip.length() == 4 * kTicksPerBar);

  clip.length_mode = LoopLengthMode::kQuantized;
  clip.quantize_grid = kTicksPerBar;
  CHECK(clip.length() == 2 * kTicksPerBar);  // 1.5 bars of content snaps UP to 2

  // An empty clip's quantized length stays 0 (no modulo-by-zero, no bogus
  // grid-unit floor for silence).
  LoopClip empty;
  empty.length_mode = LoopLengthMode::kQuantized;
  empty.quantize_grid = kTicksPerBar;
  CHECK(empty.length() == 0);
}

void test_loop_clip_remove_does_not_shift_other_events() {
  // Unlike ChordSequence::remove (a strictly-sequential single-voice chain),
  // LoopClip's events are polyphonic/independent: deleting one never shifts
  // any other event's own start.
  LoopClip clip;
  CHECK(clip.record(LoopEvent{.start = 0, .duration = 10}));
  CHECK(clip.record(LoopEvent{.start = 50, .duration = 10}));
  CHECK(clip.record(LoopEvent{.start = 100, .duration = 10}));
  CHECK(clip.remove(1));
  CHECK(clip.count() == 2);
  CHECK(clip.event(0).start == 0);
  CHECK(clip.event(1).start == 100);  // NOT shifted to 50
  CHECK(!clip.remove(9));             // out of range: false, no crash
  clip.clear();
  CHECK(clip.count() == 0 && clip.content_length() == 0);
}

}  // namespace

int main() {
  test_decompose_resolve_chord_tone_round_trip();
  test_decompose_resolve_scale_degree_fallback();
  test_decompose_resolve_interval_fallback_chromatic();
  test_resolve_reharmonizes_on_chord_change();
  test_resolve_drops_chord_tone_with_no_chord();
  test_loop_clip_record_length_and_quantize();
  test_loop_clip_length_modes();
  test_loop_clip_remove_does_not_shift_other_events();
  if (arrangrr::test::failures() == 0) {
    std::printf("test_loop_event: all OK\n");
  }
  return arrangrr::test::failures();
}
