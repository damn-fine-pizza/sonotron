// Freestanding gate for the PRODUCT, not just the primitives (D20: "a
// feature enters the core only if CI compiles it on both targets"): this TU
// instantiates the full runtime kernel + the arranger stage together —
// transport, parser, router, scheduler, chord engine, sequencer, arranger,
// timeline — and exercises the ABI entry points, so the arm cross-build
// certifies real object code for all of it.
//
// Phase-1 runtime extraction: relocated from
// components/arrangrr/src/engine_checks.cpp (which instantiated a bare
// Engine). Now that Transport/OutScheduler moved to components/runtime, this
// TU must link BOTH components together — a dependency `arrangrr`-the-library
// must never carry itself (docs/design/runtime-extraction-phase1-move-plan.md
// §6), hence its home is here, not back inside libarrangrr.a.
//
// Phase-4d (docs/design/orchestrator-pipeline-extraction.md §16.1/§16.6,
// phase4-execution-plan.md 4d): `Engine` now needs a `FollowedContext&` AND a
// `ChorddetStage&` (both Pipeline-owned in production); `engine_link_gate`
// keeps proving `Engine` ALONE cross-builds freestanding, owning its own
// externally-supplied FollowedContext + ChorddetStage instances (both
// dual-target/freestanding components in their own right, §16.1/§16.6).
// `pipeline_link_gate` is the NEW, additive proof (§16.7's arm-smoke test
// strategy: "gains a [chorddet]->[arrangrr] two-stage freestanding link-gate
// proof, additive to, not a replacement of, the existing single-stage
// smoke") that the REAL two-stage `Pipeline<ChorddetStage<N>, Engine>`
// composite -- the exact shape `hostrt::Shell` drives -- also cross-builds
// and links freestanding.

#include "arrangrr/engine.hpp"
#include "arrangrr/restyle/restyle_stage.hpp"
#include "runtime/pipeline.hpp"
#include "runtime/runtime.hpp"

namespace arrangrr {

// Referenced from the firmware stub so the linker resolves the whole
// runtime + arranger stage.
bool engine_link_gate() {
  // static: the full state lives in .bss, not stack.
  static FollowedContext followed;
  static ChorddetStage<kMaxPorts> chorddet(followed);
  static runtime::Runtime<Engine, kSchedulerCapacity> rt(followed, chorddet);
  int events = 0;
  const Engine::EventSink sink = [&events](const OutEvent&) { ++events; };

  Command key;
  key.op = Op::kSet;
  key.param = Param::kKeySet;
  rt.push_command(key, sink);

  Command play;
  play.param = Param::kChordPlay;
  play.a = 62;
  play.b = -1;
  play.c = 100;
  rt.push_command(play, sink);

  const std::uint8_t bytes[] = {0x90, 60, 100};
  rt.stage().push_midi_in(0, Span<const std::uint8_t>(bytes), sink);

  Command start;
  start.param = Param::kTransportStart;
  rt.push_command(start, sink);
  rt.advance_ticks(kTicksPerBar, sink);

  return events > 0;
}

// Phase-4d: the real two-stage `[chorddet, arrangrr]` composite (the same
// shape production drives) cross-builds and links freestanding too --
// `push_midi_in` here exercises Pipeline's Seam-C fan-out (pipeline.hpp) all
// the way through the firmware toolchain, not just on host.
bool pipeline_link_gate() {
  static FollowedContext followed;
  static runtime::Runtime<runtime::Pipeline<ChorddetStage<kMaxPorts>, Engine>, kSchedulerCapacity>
      rt([](auto&, auto&) { return ChorddetStage<kMaxPorts>(followed); },
         [](auto& sched, auto& transport, auto& chorddet) {
           return Engine(sched, transport, followed, chorddet);
         });
  int events = 0;
  const Engine::EventSink sink = [&events](const OutEvent&) { ++events; };

  Command detect;
  detect.op = Op::kSet;
  detect.param = Param::kChordDetect;
  detect.a = 1;
  detect.b = 0;
  rt.push_command(detect, sink);

  const std::uint8_t bytes[] = {0x90, 60, 100};
  rt.stage().push_midi_in(0, Span<const std::uint8_t>(bytes), sink);

  Command start;
  start.param = Param::kTransportStart;
  rt.push_command(start, sink);
  rt.advance_ticks(kTicksPerBar, sink);

  return events > 0;
}

// Roadmap 9210 (Motif engine), docs/design/motif-engine-placement.md's
// verdict: the motif engine is NOT a distinct runtime::Pipeline stage -- it
// lives entirely inside Arranger::on_tick's own gather phase, so it is
// already structurally proven to cross-build by engine_link_gate/
// pipeline_link_gate above (every build of Engine compiles Arranger's motif
// branch, taken or not). This gate is additive proof that the producer
// FUNCTIONALLY fires on the real target, not just that its code compiles: a
// style with a motif-driven pattern (a generated seed motif, kDisplacement
// transform) actually emits notes through Arranger alone, freestanding, no
// heap -- same "gains a proof" precedent §16.7 established for chorddet and
// restyle_link_gate below established for RestyleStage.
bool motif_link_gate() {
  static constexpr StyleEvent kDrumFourOnFloor[] = {
      {.step = 0, .tone = styles::kKick, .octave = 0, .vel = 100, .gate = 100},
      {.step = 4, .tone = styles::kKick, .octave = 0, .vel = 100, .gate = 100},
      {.step = 8, .tone = styles::kKick, .octave = 0, .vel = 100, .gate = 100},
      {.step = 12, .tone = styles::kKick, .octave = 0, .vel = 100, .gate = 100},
  };
  static constexpr MotifSpec kLeadMotifSpec{.transform = MotifTransform::kDisplacement,
                                            .seed = 777,
                                            .length = 4,
                                            .center_degree = 0,
                                            .vel = 90,
                                            .gate = 200,
                                            .idiom_role = TrackRole::kDrums};
  static constexpr StylePattern kMotifPatterns[] = {
      {.role = TrackRole::kDrums,
       .policy = RolePolicy::kFixed,
       .events = Span<const StyleEvent>(kDrumFourOnFloor)},
      {.role = TrackRole::kLead,
       .policy = RolePolicy::kChordTone,
       .events = Span<const StyleEvent>(),
       .motif = &kLeadMotifSpec},
  };
  static constexpr StyleSection kMotifSections[] = {
      {.type = SectionType::kVarA,
       .bars = 1,
       .patterns = Span<const StylePattern>(kMotifPatterns)}};
  static constexpr Style kMotifStyle{.name = "motiflinkgate",
                                     .sections = Span<const StyleSection>(kMotifSections)};

  static Arranger arr;
  arr.load_style(&kMotifStyle);
  arr.set_route(TrackRole::kDrums, 0, 9);
  arr.set_route(TrackRole::kLead, 0, 3);
  arr.on_transport_start();
  const Key c_major{.root_pc = 0, .mode = Mode::kMajor};
  const ChordState no_chord{};
  int lead_notes = 0;
  // Two bars: repeat 0 (the generated statement) and repeat 1 (the
  // kDisplacement-transformed answer) both actually fire.
  for (Tick t = 0; t < 2 * kTicksPerBar; ++t) {
    arr.on_tick(t, c_major, no_chord, [&](std::uint8_t, TickOffset, const MidiMessage& msg) {
      if (msg.type() == midi::kNoteOn && msg.channel() == 3) {
        ++lead_notes;
      }
    });
  }
  return lead_notes > 0;
}

// Roadmap 9320 (Restyle), docs/design/restyle-placement.md §4: RestyleStage
// itself must stay freestanding-clean even though its only real-world
// producer (midisrc::MidiSourceStage) is host-only and therefore absent
// here -- additive to (not a replacement of) `pipeline_link_gate` above,
// same "gains a proof" precedent §16.7 already established for chorddet.
// Proves the real 3-stage `[chorddet, restyle, arrangrr]` composite
// cross-builds and links freestanding, exercising RestyleStage's own
// push_midi_in (a chord-tone note, anchored + voiced) through the firmware
// toolchain, not just on host.
bool restyle_link_gate() {
  static FollowedContext followed;
  static runtime::Runtime<
      runtime::Pipeline<ChorddetStage<kMaxPorts>, RestyleStage<kMaxPorts>, Engine>,
      kSchedulerCapacity>
      rt([](auto&, auto&) { return ChorddetStage<kMaxPorts>(followed); },
         [](auto& sched, auto&, auto& chorddet) {
           return RestyleStage<kMaxPorts>(sched, chorddet, followed, /*port=*/2);
         },
         [](auto& sched, auto& transport, auto& chorddet, auto&) {
           return Engine(sched, transport, followed, chorddet);
         });
  int events = 0;
  const Engine::EventSink sink = [&events](const OutEvent&) { ++events; };

  Command key;
  key.op = Op::kSet;
  key.param = Param::kKeySet;
  rt.push_command(key, sink);

  Command chord_play;
  chord_play.param = Param::kChordPlay;
  chord_play.a = 60;
  chord_play.b = -1;
  chord_play.c = 100;
  rt.push_command(chord_play, sink);

  auto& restyle = rt.stage().template stage<1>();
  restyle.load_style(styles::kBuiltins[0]);

  const std::uint8_t note_on[] = {0x90, 60, 100};
  rt.stage().push_midi_in(0, Span<const std::uint8_t>(note_on), sink);
  rt.advance_ticks(kTicksPerStep, sink);
  const std::uint8_t note_off[] = {0x80, 60, 0};
  rt.stage().push_midi_in(0, Span<const std::uint8_t>(note_off), sink);
  rt.advance_ticks(kTicksPerStep, sink);

  return restyle.loaded();
}

}  // namespace arrangrr
