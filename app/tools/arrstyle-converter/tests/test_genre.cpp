#include "genre.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "diagnostics.hpp"
#include "smf.hpp"
#include "test.hpp"

namespace {

using namespace arrstyle;

constexpr std::uint16_t kDivision = 96;      // ticks per quarter
constexpr std::uint8_t kDrumChannel = 9;     // GM drums, 0-based
constexpr std::uint32_t kTicksPer16th = 24;  // kDivision / 4

// A drum note whose onset is placed on a bar-relative 16th step (0..15).
SmfNote drum_on(int step, std::uint8_t note, std::uint8_t vel = 100) {
  SmfNote n;
  n.tick = static_cast<std::uint32_t>(step) * kTicksPer16th;
  n.channel = kDrumChannel;
  n.note = note;
  n.velocity = vel;
  n.gate = kTicksPer16th;
  return n;
}

// A drum note at an absolute tick (used to place swung, off-grid onsets).
SmfNote drum_at(std::uint32_t tick, std::uint8_t note) {
  SmfNote n;
  n.tick = tick;
  n.channel = kDrumChannel;
  n.note = note;
  n.velocity = 100;
  n.gate = kTicksPer16th;
  return n;
}

SmfFile make_smf(std::uint32_t bpm, std::uint8_t num, std::uint8_t den,
                 const std::vector<SmfNote>& notes) {
  SmfFile smf;
  smf.division = kDivision;
  smf.tempo_milli_bpm = bpm * 1000U;
  smf.time_sig_num = num;
  smf.time_sig_den = den;
  SmfTrack track;
  track.notes = notes;
  smf.tracks.push_back(track);
  return smf;
}

// Kick 36, snare 38, closed hat 42, open hat 46.
std::vector<SmfNote> straight_hats() {
  std::vector<SmfNote> v;
  for (int s = 0; s < 16; s += 2) {
    v.push_back(drum_on(s, 42));
  }
  return v;
}

void test_disco_four_on_floor() {
  std::vector<SmfNote> notes;
  for (int s : {0, 4, 8, 12}) {
    notes.push_back(drum_on(s, 36));  // four-on-the-floor kick
  }
  for (int s : {4, 12}) {
    notes.push_back(drum_on(s, 38));  // backbeat snare
  }
  for (int s : {2, 6, 10, 14}) {
    notes.push_back(drum_on(s, 46));  // off-beat open hat
  }
  const SmfFile smf = make_smf(128, 4, 4, notes);
  const GenreGuess g = infer_genre(smf);
  CHECK(g.genre == "disco");
  CHECK(g.confidence > 0.5F);
}

void test_pop_backbeat_midtempo() {
  std::vector<SmfNote> notes = straight_hats();
  notes.push_back(drum_on(0, 36));
  notes.push_back(drum_on(8, 36));
  notes.push_back(drum_on(4, 38));
  notes.push_back(drum_on(12, 38));
  const SmfFile smf = make_smf(98, 4, 4, notes);
  const GenreGuess g = infer_genre(smf);
  CHECK(g.genre == "pop");
}

void test_rock_backbeat_fast() {
  std::vector<SmfNote> notes = straight_hats();
  notes.push_back(drum_on(0, 36));
  notes.push_back(drum_on(8, 36));
  notes.push_back(drum_on(4, 38));
  notes.push_back(drum_on(12, 38));
  const SmfFile smf = make_smf(128, 4, 4, notes);
  const GenreGuess g = infer_genre(smf);
  CHECK(g.genre == "rock");
}

void test_ballad_slow_sparse() {
  std::vector<SmfNote> notes;
  notes.push_back(drum_on(0, 36));
  notes.push_back(drum_on(4, 38));
  notes.push_back(drum_on(12, 38));
  notes.push_back(drum_on(0, 42));
  notes.push_back(drum_on(8, 42));
  const SmfFile smf = make_smf(72, 4, 4, notes);
  const GenreGuess g = infer_genre(smf);
  CHECK(g.genre == "ballad");
}

void test_waltz_triple_metre() {
  std::vector<SmfNote> notes;
  notes.push_back(drum_on(0, 36));
  notes.push_back(drum_on(4, 38));
  notes.push_back(drum_on(8, 38));
  const SmfFile smf = make_smf(90, 3, 4, notes);
  const GenreGuess g = infer_genre(smf);
  CHECK(g.genre == "waltz");
}

void test_swing_triplet_feel() {
  std::vector<SmfNote> notes;
  // Ride on the beat plus swung (2/3) off-beats -> high off-beat mean.
  for (std::uint32_t beat = 0; beat < 4; ++beat) {
    notes.push_back(drum_at(beat * kDivision, 51));       // ride, on beat
    notes.push_back(drum_at(beat * kDivision + 64, 51));  // swung off-beat (64/96)
  }
  notes.push_back(drum_on(4, 42));  // hat on 2
  notes.push_back(drum_on(12, 42));
  const SmfFile smf = make_smf(160, 4, 4, notes);
  const GenreGuess g = infer_genre(smf);
  CHECK(g.genre == "swing");
}

void test_empty_is_unknown() {
  const SmfFile smf = make_smf(120, 4, 4, {});
  const GenreGuess g = infer_genre(smf);
  CHECK(g.genre == "unknown");
  CHECK(g.confidence == 0.0F);
}

void test_parse_path_smoke() {
  // The parse-then-classify path must not crash and must yield a valid label.
  SmfFile smf = make_smf(120, 4, 4, {drum_on(0, 36), drum_on(4, 38)});
  const GenreGuess g = infer_genre(smf);
  CHECK(!g.genre.empty());
  CHECK(g.confidence >= 0.0F);
  CHECK(g.confidence <= 1.0F);
}

}  // namespace

int main() {
  test_disco_four_on_floor();
  test_pop_backbeat_midtempo();
  test_rock_backbeat_fast();
  test_ballad_slow_sparse();
  test_waltz_triple_metre();
  test_swing_triplet_feel();
  test_empty_is_unknown();
  test_parse_path_smoke();
  return arrstyle::test::failures();
}
