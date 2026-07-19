# "basic" style redesign — Intro1/VarA sameness + intro hiccup

Status: PROPOSAL (design + diagnosis), ready for Giotto to implement mechanically.
Author: Ottorino (style analyst). Read-only on product code; this document is
the only write.

Target file: `components/core/arrangrr/include/arrangrr/arranger/styles/basic.hpp`.

## 0. Method

Measured, not recalled. Read `basic.hpp` in full (338 lines), the note/gate
vocabulary in `arrangrr/arranger/style_model.hpp`, the tick math in
`arrangrr/arranger/groove.hpp` and `arrangrr/timeline/timeline.hpp`
(`kPpqn=960`, `kTicksPerStep = kPpqn/4 = 240`, so 16 steps/bar = 3840
ticks/bar), and the pinned known-values in `apps/gui-sonotron/tests/
test_preview.cpp` and the fire-order/collision probes in
`components/core/arrangrr/tests/test_fx.cpp`,
`components/core/arrangrr/tests/test_dual_arp_collision.cpp`,
`components/core/runtime/tests/test_engine_fire_order.cpp`. Then built
`sonotron-server` and captured the real JSON event stream for `style load
basic` / `style section intro1` under three progressively cleaner probes to
isolate what the arranger itself emits from what an external CLI mechanism
adds. Scripts and raw captures are reproducible; the three probe files are
below for the record.

Probe A (full band, chord sequencer wired like `tests/golden/
arranger_band.acmd` does): `key C major` / `port open out virt as synth` /
`chord out synth:4` / `style load basic` / route drums,bass,chord1,chord2,pad
/ `style section intro1` / `seq new/add C,F 1bar/loop/play` / `transport
start` / `advance 15360`.

Probe B: identical to A minus `chord out synth:4` (isolate the chord-echo
mechanism).

Probe C (closest to how gui-sonotron actually runs a style — see
`docs/reflections` CLI-vs-GUI divergence note: the GUI seeds a static default
key and never calls `seq`): `key C major` / `port open out virt as synth` /
`style load basic` / route drums,bass,chord1 / `style section intro1` /
`transport start` / `advance 7690` — no `seq`, no `chord out`.

## 1. The hiccup — what I found, with numbers, and what I did NOT find

### 1.1 What is NOT a scheduling bug (verified by arithmetic on the real ticks)

I looked for a literal engine-level defect first — an overlapping note-on,
a retrigger before its own note-off, an event landing before tick 0 — and did
not find one in Intro1 as it stands today. With `kTicksPerStep=240`:

- `kIntroBass` step 0 (root, vel 90, `gate=3600`) note-offs at tick 3600,
  240 ticks (one full step) before the bar-2 downbeat at tick 3840. Clean.
- `kIntroBass` step 16 (root, vel 94, `gate=3200`) note-offs at tick 7040,
  160 ticks before the step-30 anticipation note-on at tick 7200. Clean.
- The step-30 anticipation note (fifth, `gate=kGate8th=240`) ends at tick
  7480, 200 ticks before VarA's own downbeat at tick 7680 (Intro1 is 2 bars
  = 7680 ticks). Clean — no note carries across the Intro1→VarA boundary.
- Drums: every hat/kick/snare/crash note-off in `kIntroDrums` lands before
  the next same-role onset; no double note-on on the same pitch was observed
  in the captured stream.

So the hiccup is not a stuck note or a retrigger race. It is a **content**
problem (what fires and when), confirmed by two things I *did* measure:

### 1.2 Confirmed cause #1 (in scope, basic.hpp's own content): a dead bar
opening into an unprepared, isolated hi-hat pop

Probe C — the cleanest signal, closest to what the GUI itself would produce
(no `seq`, no `chord out`) — gives this raw stream at Intro1's start
(`/tmp/.../basic_intro_probe3.acmd`, real captured ticks):

```
{"ev":"section","name":"intro1","@":0}
{"ev":"midi-out","msg":"noteon","ch":2,"note":36,"vel":90,"@":0}
... (nothing else for 1920 ticks — 2 full beats of total silence) ...
{"ev":"midi-out","msg":"noteon","ch":10,"note":42,"vel":60,"@":1922}
{"ev":"midi-out","msg":"noteoff","ch":10,"note":42,"vel":64,"@":1982}
```

Bar 1 of the *current* `kIntroBass`/`kIntroDrums` is: one long held bass
root (`step=0`, `gate=3600` — nearly the whole bar) and then **nothing else
at all until step 8** (beat 3), where a lone closed-hat suddenly appears out
of two full beats of total silence, with no other instrument, no rhythmic
or harmonic motion accompanying its entrance. That isolated "tick" popping
in from dead air, after a static drone with zero rhythmic content, is the
"attacco sgradevole": not a bug, a **production flaw** — an unprepared
entrance with nothing around it to contextualize it. This is fully owned by
`basic.hpp`'s own `kIntroDrums`/`kIntroBass` content and is what my redesign
(§2) fixes directly: continuous, graduated motion from the first bar, not a
held drone broken by one late isolated tick.

### 1.3 Confirmed cause #2 (real, but OUT OF SCOPE for basic.hpp): the chord
sequencer's own audible echo collides with the pickup at tick 0

Probe A (the way `tests/golden/arranger_band.acmd` and most of the project's
own golden scripts exercise a style — with a chord *sequence* driving the
harmony, which is the only way to hear a style actually progress through
chord changes) gives, at tick 0:

```
{"ev":"chord","in":"C4","out":"Cmaj7","deg":"I","@":0}
{"ev":"midi-out","msg":"noteon","ch":4,"note":60,"vel":100,"@":0}
{"ev":"midi-out","msg":"noteon","ch":4,"note":64,"vel":100,"@":0}
{"ev":"midi-out","msg":"noteon","ch":4,"note":67,"vel":100,"@":0}
{"ev":"midi-out","msg":"noteon","ch":4,"note":71,"vel":100,"@":0}
{"ev":"midi-out","msg":"noteon","ch":2,"note":36,"vel":90,"@":0}   <- basic's own pickup
```

A full 4-note vel-100 block chord lands on the exact same tick as Intro1's
quiet vel-90 solo pickup note. Traced this to
`components/core/arrangrr/include/arrangrr/chord/chord_engine.hpp`
(`m_out_port` defaults to port 0, `set_output` only ever called by the CLI's
`chord out <port>` command) and `Engine::fire_chord_seq` in `engine.hpp`
(unconditionally calls `m_chords.sound(...)` — a full-velocity block chord —
every time the chord *sequence* (`seq play`) advances a step, completely
independent of section/style). This is real and reproducible, but it is
**not `basic.hpp`'s mechanism to fix**: it lives in the chord engine, fires
for every style equally, and — checked via
`grep -rl "chord out\|kChordOut" apps/gui-sonotron/` — gui-sonotron **never
calls it**. The GUI seeds a static default key and never runs `seq play`
(confirmed against the CLI-vs-GUI divergence note in
`docs/reflections/`), so this exact collision is not confirmed to be what
plays in today's GUI. I flag it for the owner/Giotto as a real, separate
architecture issue (§4) — not something this style redesign can or should
paper over — but I did design Intro1's new content so that it never puts an
arranger onset exactly on tick 0 (§2.1), which removes `basic.hpp`'s own
contribution to that collision regardless of whether the chord-echo path is
ever wired into the GUI later.

## 2. Intro1 ≈ VarA — measured, not felt

Direct array comparison (both in `basic.hpp`, current code):

`kVarADrums` bar 1 (steps 0–15): kick(36)@0 vel110, snare(38)@4 vel100,
kick(36)@8 vel105, snare(38)@12 vel100, closed-hat(42) every 2 steps
(vel 70/60 alternating).

`kIntroDrums` bar 2 (steps 16–31, i.e. relative 0–15 of its own bar): a
crash added at rel-0, then kick(36)@rel0 vel100, snare(38)@rel4 vel92,
kick(36)@rel8 vel104, snare(38)@rel12 vel98 — **the identical kick/snare
grid position-for-position**, closed-hat every 2 steps (vel 66/54
alternating) — the identical hat grid too. Only the velocities differ by a
handful of units and a crash cymbal is layered on top. This is VarA's own
groove, played a second time with a crash added, immediately followed by
VarA itself playing the same groove a third time. That is the literal,
measured content behind "Intro1 e VarA sono praticamente identiche."

**Bonus finding (not in the original complaint, but the same measured
defect):** `kIntro2Drums` (the *other* built-in intro) has the *exact same*
kick(36)@0/snare(38)@4/kick(36)@8/snare(38)@12 grid and the same closed-hat
8th bed as `kVarADrums` bar 1 — again just re-velocitied. Both of `basic`'s
intros currently ARE VarA's groove wearing a different velocity coat. I
redesigned both (§3) for the same reason and the same cost.

**Scope check on the rest of the corpus (so I don't over-claim or
under-scope):** I diffed VarB/VarC/VarD and the Fills/Endings against
VarA's grid directly. VarB pushes a kick onto step 7 (a real syncopation,
absent from VarA) and uses a full 16th-note hat bed, not VarA's 8ths — a
genuine difference. VarC halves the density (kick@0, snare@8 only,
quarter-note hats) — a genuine half-time feel. VarD adds 16th-pushed kicks
(steps 3, 11) and a 16th hat bed at higher energy — a genuine peak. Fills
and Endings have their own drum vocabulary (toms, staccato cadences) with no
shared grid at all. **None of these reuse VarA's literal grid** — the
sameness defect is confined to the two intros. I therefore scoped the
rewrite to Intro1 and Intro2 only, and left VarA/B/C/D, all four Fills, and
both Endings byte-identical. This has a direct, checked benefit (§5): it
means zero of the project's existing golden `.acmd` fixtures or
`test_preview.cpp` known-values that reference `basic` are touched, because
none of them ever select `intro1`/`intro2` for this style (checked by grep
against all four `basic`-using goldens).

## 3. The redesign

### 3.1 Design principles

- **Intro1 = preparation.** Function: build anticipation into VarA's first
  downbeat, not preview VarA's groove. No kick/snare backbeat anywhere in
  Intro1 (that vocabulary is reserved for VarA's own identity). Chord1 stays
  completely silent through Intro1 — its FIRST sound anywhere in the whole
  arrangement is VarA's downbeat stab. That's a deliberate "held-back
  entrance," a real arranging device: the instrument that never played
  before suddenly playing is itself the "arrival."
- **No event at step 0, on any role, anywhere in Intro1 or Intro2.** This is
  the direct, checked fix for §1.2 (no more instant static drone into
  isolated late tick — the pickup now enters gradually from step 2 with
  continuous motion) and it also means `basic.hpp` never puts an arranger
  onset on the one tick (tick 0) where an external mechanism might also fire
  (§1.3) — basic.hpp's own contribution to that possible collision is
  removed even though the mechanism itself is out of scope.
- **Bar 2 of Intro1 is a LAUNCH, not a loop.** Crash + a driving 8th-note
  kick pulse (not VarA's quarter-note backbeat) under a closing tom/snare
  fill in the last beat — the classic "count off the arrangement, then throw
  it to the band" idiom. No note-for-note overlap with VarA's grid at any
  point.
- **Intro2 keeps its own identity** (the fuller, punchier one-bar
  alternative) but stops reusing VarA's grid too: same principles,
  compressed into 1 bar, with ONE sustained announcement chord instead of
  VarA's twice-per-bar comping figure (a function difference, not just a
  note-count difference).
- **Pad gets its own Intro-only array** (`kIntroPad`, distinct from the
  shared `kPadTriad` used by VarA/VarB/VarD/Ending2) so touching Intro1
  never risks the shared array used by five other sections.
- I deliberately did **not** reach for `ChordGesture`, `MotifSpec`, or
  `VoicingPolicy` in this pass — the redesign below is expressible entirely
  in the existing `kFixed`/`kChordTone` + literal `StyleEvent` vocabulary
  basic.hpp already uses everywhere else, so it stays maximally simple and
  literally 1:1 translatable. No grammar limit was hit; nothing in
  `StyleEvent`/`StylePattern`/`MotifSpec` needed extending for this task.

### 3.2 New Intro1 (`SectionType::kIntro1`, `bars=2`, replaces lines ~90–189)

Illustrative C++ spec, in the exact shape `basic.hpp` already uses —
Giotto translates this directly, no invention required. Verified by hand
against `kTicksPerStep=240`: every gate ends before the next same-role
onset (no overlaps), and nothing fires at step 0.

```cpp
// Intro1 (REDESIGNED): preparation, not a VarA preview. Bar 1 is a graduated
// pickup with continuous motion (no dead bar, no isolated late tick); bar 2
// is a launch (driving kick pulse + closing fill), not VarA's backbeat.
// Nothing fires at step 0 on any role (see docs/proposals/
// basic-style-redesign.md sec1.2/1.3): the pickup breathes in from step 2.
inline constexpr StyleEvent kIntroDrums[] = {
    // bar 1: soft pulse markers (side-stick, NOT VarA's kick/snare) plus a
    // hi-hat roll that accelerates continuously into the bar-2 arrival.
    {.step=4,  .tone=kSideStick, .octave=0, .vel=50, .gate=kGateStaccato},
    {.step=8,  .tone=kClosedHat, .octave=0, .vel=52, .gate=kGateHat},
    {.step=10, .tone=kClosedHat, .octave=0, .vel=58, .gate=kGateHat},
    {.step=12, .tone=kSideStick, .octave=0, .vel=56, .gate=kGateStaccato},
    {.step=12, .tone=kClosedHat, .octave=0, .vel=64, .gate=kGateHat},
    {.step=13, .tone=kClosedHat, .octave=0, .vel=70, .gate=kGateHat},
    {.step=14, .tone=kClosedHat, .octave=0, .vel=78, .gate=kGateHat},
    {.step=15, .tone=kClosedHat, .octave=0, .vel=88, .gate=kGateHat},
    // bar 2: the launch. Crash + a driving 8th-note kick pulse (NOT VarA's
    // quarter-note kick/snare backbeat), then a closing tom/snare fill on
    // the last beat that hands off into VarA's own downbeat.
    {.step=16, .tone=kCrash, .octave=0, .vel=95, .gate=kGateHalfBar},
    {.step=16, .tone=kKick,  .octave=0, .vel=108, .gate=kGateHat},
    {.step=18, .tone=kKick,  .octave=0, .vel=86,  .gate=kGateHat},
    {.step=20, .tone=kKick,  .octave=0, .vel=98,  .gate=kGateHat},
    {.step=22, .tone=kKick,  .octave=0, .vel=84,  .gate=kGateHat},
    {.step=24, .tone=kKick,  .octave=0, .vel=104, .gate=kGateHat},
    {.step=26, .tone=kTomMid,   .octave=0, .vel=90,  .gate=kGateStab},
    {.step=27, .tone=kTomMid,   .octave=0, .vel=86,  .gate=kGateStab},
    {.step=28, .tone=kTomLow,   .octave=0, .vel=98,  .gate=kGateStab},
    {.step=29, .tone=kTomLow,   .octave=0, .vel=92,  .gate=kGateStab},
    {.step=30, .tone=kSnare,    .octave=0, .vel=104, .gate=kGateStab},
    {.step=31, .tone=kSnare,    .octave=0, .vel=114, .gate=90},
};
inline constexpr StyleEvent kIntroBass[] = {
    // bar 1: a breathing pickup, two soft pulses (NOT one static held
    // drone) building dynamic level toward the arrival.
    {.step=2,  .tone=kRoot, .octave=0, .vel=66, .gate=440},
    {.step=8,  .tone=kRoot, .octave=0, .vel=76, .gate=440},
    // bar 2: the root under the launch, released before...
    {.step=16, .tone=kRoot,  .octave=0, .vel=92, .gate=1400},
    // ...a short 5th anticipation into VarA's own root.
    {.step=30, .tone=kFifth, .octave=0, .vel=84, .gate=kGate8th},
};
// Intro-only pad swell (distinct array from the shared kPadTriad, which
// VarA/VarB/VarD/Ending2 all still use unmodified). Enters soft under the
// bar-1 pickup, swells at the bar-2 arrival, decays before VarA's own
// kPadTriad re-triggers fresh at VarA's downbeat -- no hand-off overlap.
inline constexpr StyleEvent kIntroPad[] = {
    {.step=2,  .tone=kRoot,  .octave=0, .vel=42, .gate=1600},
    {.step=2,  .tone=kThird, .octave=0, .vel=40, .gate=1600},
    {.step=2,  .tone=kFifth, .octave=0, .vel=42, .gate=1600},
    {.step=16, .tone=kRoot,  .octave=0, .vel=58, .gate=1600},
    {.step=16, .tone=kThird, .octave=0, .vel=56, .gate=1600},
    {.step=16, .tone=kFifth, .octave=0, .vel=58, .gate=1600},
};
inline constexpr StylePattern kIntroPatterns[] = {
    {.role=TrackRole::kDrums, .policy=RolePolicy::kFixed,     .events=Span<const StyleEvent>(kIntroDrums)},
    {.role=TrackRole::kBass,  .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntroBass)},
    {.role=TrackRole::kPad,   .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntroPad), .gm_program=kPadVoice},
};
```

Overlap check (hand-verified against `kTicksPerStep=240`): bass step 2
(tick 480, gate 440) ends at 920, well before step 8 (tick 1920). Step 8
(gate 440) ends at 2360, well before step 16 (tick 3840). Step 16 (gate
1400) ends at 5240, well before step 30 (tick 7200). Step 30 (gate 240) ends
at 7440, 240 ticks before VarA's downbeat at 7680. Pad step 2 (gate 1600)
ends at 2080, decays fully before its own step-16 re-hit at 3840; step 16's
re-hit (gate 1600) ends at 5440, decays fully before VarA's own `kPadTriad`
re-triggers fresh at 7680. No stuck notes, no retriggers, nothing at tick 0.

### 3.3 New Intro2 (`SectionType::kIntro2`, `bars=1`, replaces lines ~227–243)

Same principles, compressed to 1 bar (16 steps); kept as the fuller/punchier
alternative to Intro1's hushed pickup, so it gets ONE big announcement chord
instead of Intro1's held-back Chord1.

```cpp
// intro2 (REDESIGNED): the fuller one-bar alternative -- same principles as
// the new Intro1 (nothing at step 0, no VarA-grid reuse) but punchier: it
// gets its OWN sustained announcement chord (one hit, not VarA's twice-per-
// bar comping figure) landing WITH the arrival, not comping through it.
inline constexpr StyleEvent kIntro2Drums[] = {
    {.step=2, .tone=kSideStick, .octave=0, .vel=48, .gate=kGateStaccato},
    {.step=6, .tone=kClosedHat, .octave=0, .vel=54, .gate=kGateHat},
    {.step=7, .tone=kClosedHat, .octave=0, .vel=60, .gate=kGateHat},
    {.step=8, .tone=kCrash, .octave=0, .vel=92, .gate=kGateHalfBar},
    {.step=8, .tone=kKick,  .octave=0, .vel=104, .gate=kGateHat},
    {.step=10, .tone=kKick, .octave=0, .vel=88,  .gate=kGateHat},
    {.step=12, .tone=kKick, .octave=0, .vel=98,  .gate=kGateHat},
    {.step=13, .tone=kTomMid, .octave=0, .vel=86, .gate=kGateStab},
    {.step=14, .tone=kTomLow, .octave=0, .vel=94, .gate=kGateStab},
    {.step=15, .tone=kSnare,  .octave=0, .vel=108, .gate=kGateStab},
};
inline constexpr StyleEvent kIntro2Bass[] = {
    {.step=2, .tone=kRoot, .octave=0, .vel=70, .gate=520},
    {.step=8, .tone=kRoot, .octave=0, .vel=90, .gate=700},
};
inline constexpr StyleEvent kIntro2Chord[] = {
    // one sustained 4-note announcement, landing WITH the arrival (step 8)
    // -- a pad-like hit, not a comping rhythm (that's VarA's own function).
    {.step=8, .tone=kRoot,    .octave=0, .vel=76, .gate=1700},
    {.step=8, .tone=kThird,   .octave=0, .vel=76, .gate=1700},
    {.step=8, .tone=kFifth,   .octave=0, .vel=76, .gate=1700},
    {.step=8, .tone=kSeventh, .octave=0, .vel=76, .gate=1700},
};
inline constexpr StylePattern kIntro2Patterns[] = {
    {.role=TrackRole::kDrums,  .policy=RolePolicy::kFixed,     .events=Span<const StyleEvent>(kIntro2Drums)},
    {.role=TrackRole::kBass,   .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntro2Bass)},
    {.role=TrackRole::kChord1, .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kIntro2Chord)},
    {.role=TrackRole::kPad,    .policy=RolePolicy::kChordTone, .events=Span<const StyleEvent>(kPadTriad), .gm_program=kPadVoice},
};
```

`kIntro2Patterns` keeps reusing the shared `kPadTriad` (step 0 entrance,
soft — vel 58/56/58) unmodified: Intro2 is deliberately the *punchier*,
more-immediate option (unlike Intro1 it is not trying to be a hushed
pickup), so a soft pad swell starting right on the downbeat is in character
here and was never implicated in either diagnosed cause.

### 3.4 Everything else in `basic.hpp` — unchanged, and why

`kVarAPatterns`, `kVarBPatterns`, `kVarCPatterns`, `kVarDPatterns`, all four
`kFill*Patterns`, `kEndPatterns`, `kEnd2Patterns`, `kSections`, and the
`Style` groove/tempo record stay byte-identical. §2's scope check (VarB's
step-7 push, VarC's half-time thinning, VarD's 16th pushes, the fills' own
tom/staccato vocabulary) is the evidence that they don't share the
Intro1/Intro2 defect, and the owner's complaint was specifically about
Intro1 vs VarA — I did not manufacture extra rewrite scope beyond what the
measurement actually supports.

## 4. What I flagged, not fixed

The chord engine's default `chord_out` (port 0, unset unless a CLI/host
caller explicitly calls `chord out <port>`) sounding a full vel-100 block
chord every time a chord *sequence* advances a step (`Engine::fire_chord_seq`
in `components/core/arrangrr/include/arrangrr/engine.hpp`, `ChordEngine::
set_output`/`m_out_port` in `.../chord/chord_engine.hpp`) is architecture,
not style content. It is real (§1.3), it is not currently exercised by
gui-sonotron (confirmed: `grep -rl "chord out\|kChordOut" apps/gui-sonotron/`
returns nothing), but it WILL reproduce the tick-0 collision the moment any
caller wires a chord *sequence* with `chord out` bound to the same port as
the band — which is exactly the pattern most of the project's own golden
`.acmd` scripts use (`arranger_band.acmd`, `lead_funk.acmd`, etc.). If the
owner wants that hardened (e.g., scale the chord-echo's velocity down, gate
it off during `section_is_intro()`, or default `chord_out` to "none" instead
of port 0), that is engine-level work for Giotto, not something this
style file can express or that I should silently paper over.

## 5. Impact — what breaks at land, what does not

**Does NOT break (checked directly, not assumed):**
- `apps/gui-sonotron/tests/test_preview.cpp`'s `test_known_value_fixed_role_drums`
  and `test_known_value_resolved_role_bass` read `basic`/`kVarA` role 0 and 2
  — `kVarADrums`/`kVarABass` are untouched, byte-identical.
- `test_scene_section_changes_the_preview` (VarA vs VarB) and
  `test_section_bars_known_value_basic_var_a` (`bars==2`/`bars==1`) — VarA's
  `bars=2` and VarB are untouched.
- `components/core/arrangrr/tests/test_fx.cpp` (`kVarADrums` step-0 kick,
  vel 110, zero-jitter probe) and `test_dual_arp_collision.cpp`
  (`kVarABass` step-0 root → note 36) and
  `components/core/runtime/tests/test_engine_fire_order.cpp` (VarA bass root
  on the downbeat, both bars) — none of these read Intro1/Intro2 content;
  all three are VarA-only probes and VarA is untouched.
- All four `basic`-using goldens (`tests/golden/accompany_melody_detect.acmd`,
  `accompany_restyle.acmd`, `arranger_band.acmd`, `clip_launch.acmd`) —
  checked directly: none of them ever issue `style section intro1` or
  `style section intro2` for `basic` (the engine's default section on style
  load is `kVarA`, `components/core/arrangrr/include/arrangrr/arranger/
  arranger.hpp`); they exercise VarA/VarB/fillA/ending1, which are
  untouched. **None of these four goldens need regeneration.**

**Coverage gap this surfaces (not a break, a recommendation):** *no* existing
golden currently exercises `basic`'s Intro1 or Intro2 content at all, for
either the old or the new arrays. Torquato/Giotto should consider adding
one new golden that does `style load basic` + `style section intro1` (and
ideally `intro2`) explicitly, both to lock in this redesign and to close the
gap that let Intro1≈VarA ship unnoticed by any regression fixture. That is a
NEW golden to author, not a regen of an existing `.golden` file, so it needs
no owner sign-off token under the existing regen protocol — flagging it here
so it isn't lost.

## 6. StyleEvent/StylePattern grammar — no limit hit

Everything above is expressible in the vocabulary `basic.hpp` already uses
(`RolePolicy::kFixed`/`kChordTone`, literal `StyleEvent{step, tone, octave,
vel, gate}`, `StylePattern{role, policy, events, gm_program}`). I did not
need `NoteSource::kScaleDegree`/`kInterval`, `ChordGesture`, `VoicingPolicy`,
or `MotifSpec` to make Intro1 sound and function differently from VarA — the
flatness was never a grammar limitation, it was authored content (two arrays
that happened to copy VarA's grid). No extension request to flag.
