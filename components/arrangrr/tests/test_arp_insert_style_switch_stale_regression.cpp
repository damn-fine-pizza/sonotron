// Torquato QA heavy-pass (Phase-6 Theme 4, target 6 -- session-only role-arp
// state). Kept in its OWN file/CTest entry (mirrors
// test_master_transpose_voicing_regression.cpp's own isolation precedent) so
// its failure never masks the surrounding GREEN test_arranger.cpp coverage.
// Currently RED; do not silence it.
//
// Arranger::reset_role_arps()'s own doc comment (arranger.hpp) lists THREE
// reset boundaries -- "a fresh style load (load_style()), a transport
// (re)start (on_transport_start()), and a Panic" -- and load_style() itself
// calls reset_role_arps() with the comment "a new style must not arpeggiate a
// stale chord". Arranger::request_style() -- the LIVE style-switch entry
// point (Engine::style_switch/cmd_style's kStyleSwitch ABI verb, and
// apply_performance's own "keep current style" recall branch) -- is a
// SEPARATE code path from load_style() and calls reset_role_arps() in
// NEITHER its immediate NOR its deferred (bar-boundary, on_tick) branch. A
// role's arp-insert can therefore keep arpeggiating a chord resolved under
// the OLD style after a live switch to a style that never resolves that role
// again -- exactly the failure mode reset_role_arps()'s own comment says
// must not happen, just reached through the one entry point that forgot the
// call.

#include "arrangrr/arranger/arranger.hpp"

#include "test.hpp"

namespace {

using namespace arrangrr;

constexpr StyleEvent kArpSwitchBassStepZero[] = {
    {.step = 0, .tone = 0, .octave = 0, .vel = 100, .gate = 200},
};
constexpr StylePattern kArpSwitchStyleAPatterns[] = {
    {.role = TrackRole::kBass,
     .policy = RolePolicy::kChordTone,
     .events = Span<const StyleEvent>(kArpSwitchBassStepZero)},
};
constexpr StyleSection kArpSwitchStyleASections[] = {
    {.type = SectionType::kVarA,
     .bars = 1,
     .patterns = Span<const StylePattern>(kArpSwitchStyleAPatterns)}};
constexpr Style kArpSwitchStyleA{.name = "arpswitchA",
                                 .sections = Span<const StyleSection>(kArpSwitchStyleASections)};

// Style B deliberately has NO pattern for ANY role at all (an empty patterns
// span) -- it never resolves a single note for kBass, modeling "the new
// style doesn't even have this part".
constexpr StyleSection kArpSwitchStyleBSections[] = {
    {.type = SectionType::kVarA, .bars = 1, .patterns = Span<const StylePattern>()}};
constexpr Style kArpSwitchStyleB{.name = "arpswitchB",
                                 .sections = Span<const StyleSection>(kArpSwitchStyleBSections)};

void test_live_style_switch_leaves_a_stale_arp_insert_held_chord_regression() {
  Arranger arr;
  CHECK(arr.load_style(&kArpSwitchStyleA));
  CHECK(arr.set_route(TrackRole::kBass, 0, 0));
  CHECK(arr.set_fx(TrackRole::kBass, 0, InsertType::kArp));
  CHECK(arr.set_fx_param(TrackRole::kBass, 0, /*rate=*/0,
                         static_cast<std::int32_t>(ArpRate::kSixteenth)));
  CHECK(arr.set_fx_param(TrackRole::kBass, 0, /*gate=*/3, 100));

  const ChordState chord{.root_pc = 0, .quality = ChordQuality::kMaj, .valid = true};
  arr.on_transport_start();

  // Tick 0: style A's own bass step-0 event ingests+sounds note 36 through
  // the arp-insert's P5 pass.
  bool sounded_before = false;
  arr.on_tick(0, Key{}, chord, [&](std::uint8_t, TickOffset, const MidiMessage& m, std::uint8_t) {
    if (m.channel() == 0 && m.type() == midi::kNoteOn && m.d1 == 36) {
      sounded_before = true;
    }
  });
  CHECK(sounded_before);

  // A LIVE style switch (Arranger::request_style, immediate) to style B,
  // which never resolves a bass note at all.
  CHECK(arr.request_style(&kArpSwitchStyleB, SectionType::kVarA, /*immediate=*/true));

  // One arp step (240 ticks) later: a correctly reset arp-insert would hold
  // NOTHING (style B never re-ingests kBass) and stay silent. It does not --
  // the P5 ungated per-role pass (which iterates every routed role
  // regardless of whether the ACTIVE style's own patterns mention it) keeps
  // re-emitting the STALE note 36 from before the switch.
  bool sounded_after = false;
  arr.on_tick(240, Key{}, chord, [&](std::uint8_t, TickOffset, const MidiMessage& m, std::uint8_t) {
    if (m.channel() == 0 && m.type() == midi::kNoteOn && m.d1 == 36) {
      sounded_after = true;
    }
  });
  CHECK(!sounded_after);  // FAILS today: request_style() never calls reset_role_arps()
}

}  // namespace

int main() {
  test_live_style_switch_leaves_a_stale_arp_insert_held_chord_regression();
  return arrangrr::test::failures();
}
