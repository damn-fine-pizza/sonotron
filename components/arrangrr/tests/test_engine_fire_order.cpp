#include "arrangrr/common/static_vector.hpp"
#include "arrangrr/engine.hpp"
#include "arrangrr/transport/transport.hpp"  // kTicksPerBar
#include "test.hpp"

// Locks the per-tick producer FIRE ORDER of Engine::advance_ticks
// (engine.hpp): each playing tick fires
//   fire_timeline -> fire_chord_seq -> fire_arranger -> fire_arp
// then flushes. The load-bearing half of that order is chord-seq BEFORE
// arranger: on the SAME tick a ChordSequencer chord change must be applied
// (m_chords.sound updates the harmonic context) BEFORE the arranger resolves
// that tick, so the band harmonizes on the NEW chord, not the previous one.
//
// The arranger does not merely enqueue: fire_arranger READS m_chords.state()
// synchronously to run the NTT (arranger.hpp resolve()). That data dependency
// is what makes the ordering OBSERVABLE at the output — the concrete bass note
// on the downbeat differs depending on which producer ran first. If
// fire_chord_seq and fire_arranger were ever swapped, the downbeat of the new
// bar would resolve LAST bar's chord: an audible wrong note. This test promotes
// the comment-only ordering to a regression-proof invariant.

namespace {

using namespace arrangrr;

using Events = StaticVector<OutEvent, 4096>;

// Same style/route conventions as the golden arranger_band.acmd and the
// test_chord_detect Band harness: key C major, style "basic", bass on synth
// port 0 / channel 1 (0-based), comp (chord1) on channel 2.
struct Band {
  Engine e;
  Events ev;

  void cmd(Param p, std::int32_t a = 0, std::int32_t b = 0, std::int32_t c = 0, Op op = Op::kDo) {
    Command command{.op = op, .param = p, .idx = 0, .a = a, .b = b, .c = c};
    e.push_command(command, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void advance(std::uint32_t n) {
    e.advance_ticks(n, [&](const OutEvent& o) { CHECK(ev.push_back(o)); });
  }
  void setup_basic() {
    cmd(Param::kKeySet, 0, 0, 0, Op::kSet);  // C major
    cmd(Param::kStyleLoad, 0);               // "basic"
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kBass), 0 | (1 << 8), 0, Op::kSet);
    cmd(Param::kStyleRoute, static_cast<std::int32_t>(TrackRole::kChord1), 0 | (2 << 8), 0,
        Op::kSet);
  }
  // Programs a looping | C | G | progression, one bar each, and arms playback.
  // kSeqAdd: a = root note (interpreted in the seq key), b = (quality_ovr+1) |
  // (velocity<<8) with quality_ovr 0 => -1 (smart), c = duration in ticks.
  void arm_c_then_g() {
    cmd(Param::kSeqNew);
    cmd(Param::kSeqAdd, 60, 0 | (100 << 8), static_cast<std::int32_t>(kTicksPerBar));  // C, 1 bar
    cmd(Param::kSeqAdd, 67, 0 | (100 << 8), static_cast<std::int32_t>(kTicksPerBar));  // G, 1 bar
    cmd(Param::kSeqLoop, 1, 0, 0, Op::kSet);
    cmd(Param::kSeqPlay);
  }

  // Number of bass (channel 1) NoteOns of `note` scheduled EXACTLY at `tick`.
  int bass_ons_at(std::uint8_t note, Tick tick) const {
    int n = 0;
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kMidi && o.msg.type() == midi::kNoteOn &&
          o.msg.channel() == 1 && o.msg.d1 == note && o.tick == tick) {
        ++n;
      }
    }
    return n;
  }
  // True if a ChordSequencer chord echo with `root_note` fired exactly at `tick`.
  bool chord_echo_at(std::uint8_t root_note, Tick tick) const {
    for (const OutEvent& o : ev) {
      if (o.kind == OutEvent::Kind::kChord && o.msg.status == root_note && o.tick == tick) {
        return true;
      }
    }
    return false;
  }
};

// The invariant: on the downbeat where the sequenced chord becomes G, the
// arranger's bass reflects G (root_pc 7 -> 36 + 7 = 43, G2), NOT C (36, C2).
//
// Bass anchor is 36 + chord.root_pc (arranger.hpp kRoleAnchor[kBass] = 36).
// The "basic" bass pattern plays the chord ROOT on the downbeat (beat 1). So:
//   * with the correct order (chord-seq BEFORE arranger) the G downbeat root
//     is 43;
//   * if reordered, the arranger would still see the stale C context and emit
//     36 there.
// Note 43 also appears within the C bar (as C's fifth on the off-beats) and 36
// appears within the C bar (as C's root) — so the assertion MUST be pinned to
// the exact downbeat tick of the G bar, kTicksPerBar, not to global counts.
void test_chord_seq_resolves_before_arranger_same_tick() {
  Band b;
  b.setup_basic();
  b.arm_c_then_g();
  b.cmd(Param::kTransportStart);  // fires bar-1 downbeat (C)

  // Advance up to, but not across, the bar boundary: still bar 1 (chord C).
  b.advance(kTicksPerBar - 1);

  // Cross the boundary by exactly ONE tick: this single tick fires
  // fire_chord_seq (C -> G) and then fire_arranger, in that order.
  const Tick downbeat = static_cast<Tick>(kTicksPerBar);
  b.advance(1);

  // Precondition: the chord genuinely CHANGED to G on this exact tick (echo
  // root_note 60 + 7 = 67). Guards against a vacuous pass if the scenario ever
  // stops firing G here.
  CHECK(b.chord_echo_at(67, downbeat));

  // The pin: on the G downbeat the bass root is G (43), never the stale C (36).
  CHECK(b.bass_ons_at(43, downbeat) > 0);   // arranger saw the NEW chord (G)
  CHECK(b.bass_ons_at(36, downbeat) == 0);  // it did NOT resolve last bar's C
}

// Sanity anchor for the register/pattern this test relies on: the very first
// downbeat (bar 1, chord C) is the C root (36), never G's root (43). This
// documents that 43-on-the-G-downbeat is meaningful and not an artifact of the
// bass always playing 43.
void test_bar_one_downbeat_is_c_root() {
  Band b;
  b.setup_basic();
  b.arm_c_then_g();
  b.cmd(Param::kTransportStart);
  b.advance(1);  // land just past tick 0

  CHECK(b.chord_echo_at(60, 0));     // bar-1 chord is C (root_note 60)
  CHECK(b.bass_ons_at(36, 0) > 0);   // C root on the downbeat
  CHECK(b.bass_ons_at(43, 0) == 0);  // not G's root
}

}  // namespace

int main() {
  test_chord_seq_resolves_before_arranger_same_tick();
  test_bar_one_downbeat_is_c_root();
  return arrangrr::test::failures();
}
