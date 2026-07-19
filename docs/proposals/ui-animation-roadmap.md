# UI Animation Roadmap — Dear ImGui motion plan for Sonotron

Status: **PROPOSED** (owner-authored, 2026-07-17; branch `gui-sonotron`). Scope:
`apps/gui-sonotron/` only (host-only, node `11600`). This is a concrete
implementation plan — code blocks, state tables, priorities (P0/P1/P2) and a
recommended build order — for whoever executes the motion work next
(`giotto-cpp-implementor`), not a redesign of the GUI's visual language.
Translated into English from the owner's Italian analysis with all technical
content preserved; a small number of code-grounded claims were checked against
the tree and one is corrected below (marked **[Correction]**).

---

## Relation to prior motion analysis

A prior, independently-authored document, `docs/proposals/ui-motion-extreme-2026-07.md`
(2026-07-16), performed a from-scratch audit of the GUI's motion surface and
proposed a full `MotionSystem` (springs/tweens, a keyed per-element store, three
motion profiles, a real-time audio-reactive bridge, a deterministic test plan).
Its single most important finding, **P1** in that document: *"No animation is
beat-synchronised; every 'musical' motion is a wall-clock sinusoid... the
authoritative musical position exists and is unused for motion"* (citing
`AppState::bar()/beat_num()/pulse()/beat_phase()`, `app_state.hpp:96-99`).

This document is that finding acted on: it starts from the same diagnosis (the
same `fmod`/`sin` wall-clock sweeps, verified again below) and turns it into a
concrete, phased build order — infrastructure first (`motion.hpp/.cpp`,
`MotionFrame`), then the real musical clock, then per-panel specs area by area,
with an explicit "what NOT to build yet" tier gated on data that does not exist
today. The two documents are **complementary, not duplicates**:

- `ui-motion-extreme-2026-07.md` is the wider audit and systems design (ten
  numbered problems P1–P10, three motion profiles, Reduced Motion, an
  audio-reactive bridge proposal, a 14-case test plan, a full migration-order
  table by impact/risk).
- This document is the narrower, owner-prioritized execution roadmap: it
  reaches the same P0 conclusion (build the motion infrastructure, wire the
  real beat) by a shorter path, adds per-panel visual specs (exact alpha/
  thickness/duration numbers for Repeat Zone, Sequence Edit, Transport,
  Intention, Browser/Parts) that the prior document does not spell out at that
  level of detail, and stops the "future work" tier at the same boundary (new
  core/audio/GPU data required) that the prior document's §9 "Risks" section
  also names.

**Verdict: keep both, cross-referenced, do not retire either.** The prior
document is the fuller systems design (profiles, GC/lifetime policy for the
per-element store, the deterministic test plan, the audio-reactive bridge) that
an implementor should still read before writing `motion.hpp`; this document is
the concrete per-panel spec and the owner's chosen sequencing. Where the two
disagree only in emphasis (e.g. `MotionSystem` as a generic `MotionId`-keyed
store in the prior doc vs. an explicit `UiMotionState`/`CellMotion` struct with
named fields per cell in this one — see §1 below), that is a design-shape
choice left open for the implementor, not a factual contradiction; both agree
on the underlying requirement (explicit, testable, GC'd per-element animation
state, not a hidden static inside a widget).

---

## Summary assessment

Sonotron already has a foundation suited to a professionally animated UI:

- a centralized design system in `theme`;
- custom widgets via `ImDrawList`;
- a shared `neon_widgets` layer;
- a global clock and glow flag in `V02State`;
- real state for beat, chords, and clip lifecycle.

The current limitation is that most of the motion is **decorative and
wall-clock based**:

- the VU meter is generated from sinusoids;
- the clip sweep is fixed at 1.7 seconds;
- the piano-roll playhead is fixed at 2 seconds;
- the status dot pulses continuously.

This can look pleasant, but it risks the "cyberpunk demo" effect. The correct
direction is:

> **fewer continuous animations, more animation caused by music, events, and
> state transitions.**

The project itself requires that the UI work perfectly without animation, and
that motion be reserved for playhead, launch, and pending state.

---

# Work to do now

None of the following requires new core features or new wire events.

## 1. Centralized motion infrastructure — priority P0

Add:

```text
apps/gui-sonotron/src/
    motion.hpp
    motion.cpp
```

With three distinct concepts:

```cpp
struct MotionFrame {
    float now;
    float delta_time;

    float beat_phase;   // 0..1
    float bar_phase;    // 0..1

    bool beat_edge;
    bool bar_edge;
    bool playing;
    bool reduced_motion;
};
```

Minimal utilities:

```cpp
float damp(float current, float target, float half_life, float dt);
float spring(float current, float target, float& velocity,
             float frequency, float damping, float dt);

float ease_out_cubic(float t);
float ease_out_back(float t);
float pulse(float phase, float width);
```

### Why it is needed

Today `V02State` distributes `time` and `playing`, but keeps no memory of
transitions.

Animating professionally requires:

- the previous value;
- the event's timestamp;
- the target state;
- the spring's velocity;
- a distinction between the UI clock, the beat, and the bar.

Hidden static storage inside widgets should not be used. Explicit, testable
state should be added instead:

```cpp
struct CellMotion {
    float activation = 0.0F;
    float focus = 0.0F;
    float pending = 0.0F;
    float velocity = 0.0F;
    AppState::ClipLaunchState previous_state{};
};

struct UiMotionState {
    MotionFrame frame;
    std::array<CellMotion, 30> cells;

    float transport_started = 0.0F;
    float panic_flash = 0.0F;
    float chord_changed = 0.0F;
    float seqedit_transition = 1.0F;
};
```

For Sonotron this is preferable to a generic `unordered_map<ImGuiID, Animation>`:
more deterministic, more readable, and consistent with the project's explicit
state discipline.

---

## 2. Use the real musical clock — priority P0

`AppState` already has:

- bar;
- beat;
- pulse;
- `beat_phase()`;
- the real clip lifecycle.

**[Correction, verified against `apps/gui-sonotron/src/app_state.hpp:99]** —
confirmed as claimed: `beat_phase()` is implemented as
`static_cast<float>(m_pulse) / 24.0F`.

The following should therefore be gradually eliminated:

```cpp
fmod(fx.time, 1.7F)
fmod(fx.time, 2.0F)
sin(fx.time * ...)
```

**[Verified]** These three literal patterns are exactly what is on the tree
today: `apps/gui-sonotron/src/neon_widgets.cpp:367-368` (`sweep_bar`, period
`1.7F`), `apps/gui-sonotron/src/seqedit_panel.cpp:146` (`std::fmod(fx.time,
2.0F) / 2.0F`), and the wall-clock sinusoids at
`apps/gui-sonotron/src/neon_widgets.cpp:283-284` (`master_vu`,
`time * 3.0F + i * 0.7F`), `apps/gui-sonotron/src/transport_panel.cpp:174`
(status dot, `fx.time * 4.0F`), and `apps/gui-sonotron/src/layout_renderer.cpp:55`
(fake dB readout, `fx.time * 2.0F`). These should be replaced with:

```cpp
const float beat_phase = app_state.beat_phase();

const float bar_phase =
    (static_cast<float>(app_state.beat_num() - 1) + beat_phase)
    / beats_per_bar;
```

**[Correction]** The report's original text states: *"Until `beats_per_bar` is
exposed to the GUI, it can be hardcoded to 4 with an explicit comment; it will
come from the transport snapshot in the future."* This is stale: `AppState`
**already exposes** `beats_per_bar()` today
(`apps/gui-sonotron/src/app_state.hpp:111`), reduced from the real `kTimeSig`
`OutEvent` (Phase 7 node `T0`, the variable time-signature engine), defaulting
to `kDefaultBeatsPerBar == 4` only until a real announce arrives. The formula
above should use `app_state.beats_per_bar()` directly rather than a hardcoded
literal — the accessor this section anticipated as future work is already
built.

### Result

Everything breathes together:

- the playhead;
- active cells;
- the scene indicator;
- the logo cursor;
- the chord countdown;
- any activity meters.

This alone makes the UI feel far more like a "musical instrument" and far less
like an "animated dashboard."

---

## 3. Repeat Zone: animate the real lifecycle — priority P0

Today every state other than `stopped` is treated simply as "playing/lit."

**[Verified]** `apps/gui-sonotron/src/grid_panel.cpp:813-814`:
```cpp
const AppState::ClipLaunchState launch_state = app_state.clip_state(static_cast<int>(id));
const bool playing = launch_state != AppState::ClipLaunchState::kStopped;
```
confirms the collapse: `AppState::ClipLaunchState` (`app_state.hpp:38`) really
does carry four distinct values (`kStopped`, `kArmed`, `kPlaying`,
`kQueuedStop`), and `draw_cell` really does reduce them to one boolean today.

The design instead calls for a visual distinction between `armed`, `playing`,
and `queued_stop`.

### Proposed states

| State       | Static appearance         | Motion                                                 |
| ----------- | -------------------------- | ------------------------------------------------------ |
| Stopped     | faint track-color border   | none                                                    |
| Armed       | amber border                | a line completing the perimeter toward the next bar    |
| Playing     | full track-color border    | light pulse on the downbeat                            |
| Queued stop | pink border                 | border retracting/fading out toward the next bar       |
| Opened      | double track-color border   | 140 ms focus ring, then static                          |

### Specific animations

#### Launch kick

When a clip goes from `stopped` to `armed`:

- the border expands by 2–3 px;
- a 100–140 ms flash;
- no change to the widget's geometry;
- no scaling of the whole cell, which would make the grid shake.

#### Beat breathing

A playing cell subtly increases:

- fill alpha: roughly `0.22 → 0.29`;
- border thickness: `1.5 → 1.9`;
- only for the first 20–25% of the beat.

It must not breathe with a continuous sinusoid.

#### Preview note chase

**[Verified]** The previews already have 16 real steps:
`apps/gui-sonotron/src/neon_widgets.hpp:42` — `static constexpr int kSteps = 16;`
(`ClipPattern::kSteps`).

The active step can be computed as:

```cpp
const int active_step =
    static_cast<int>(motion.bar_phase * ClipPattern::kSteps)
    % ClipPattern::kSteps;
```

The current step's note:

- turns near-white for 60–90 ms;
- produces a small track-color halo;
- then returns to its standard color.

This is a very appealing effect, but also semantically useful: it makes the
pattern immediately legible.

#### Active scene column

The whole active column should have:

- a very subtle vertical wash;
- a brighter title underline;
- a single pulse on scene change;
- a progress bar for the section's duration.

The code already knows the active scene, its starting bar, elapsed bars, and
section duration during auto-song. A real progress indicator can therefore be
shown without any core change.

---

## 4. Sequence Edit — priority P0

### Playhead

Replace the fixed two-second cycle with `bar_phase`. **[Verified]** The
current playhead is disconnected from the musical position, confirmed at
`apps/gui-sonotron/src/seqedit_panel.cpp:146`:
`const float phase = std::fmod(fx.time, 2.0F) / 2.0F;`

Recommended appearance:

- main line 1–1.5 px;
- a semi-transparent 5–7 px trail;
- no oversized glow;
- a small triangular marker at the top.

### Note activation

When the playhead crosses a note:

- fill transitions from the track color to near-white;
- a very brief glow;
- return within 100 ms.

This produces a piano-roll that feels alive without turning it into a
festival-style visualizer.

### Clip change

When a new clip opens:

1. the old pattern fades to 0 alpha over 70 ms;
2. the new pattern enters from `y + 3 px`;
3. fade-in over roughly 130 ms.

No zoom or shared-element animation from the cell to the canvas for now: it
would require storing cross-panel geometry and would complicate the renderer
significantly.

### Mode tabs

For `piano-roll` / `step`:

- text color transitions gradually;
- a cyan underline that slides from one tab to the other;
- no whole-canvas fade until the `step` view is genuinely different from the
  piano-roll view.

---

## 5. Transport rack — priority P1

### Play

- a single flash when it starts;
- a very light pulse only on the downbeat;
- must not be permanently drawn "filled=true" regardless of state.

**[Correction]** The report's claim that the Play button is *"always drawn
filled"* is verified as currently accurate:
`apps/gui-sonotron/src/transport_panel.cpp:59`:
```cpp
if (neon::pad_button("play", "\xE2\x96\xB6", pad, theme::kCyan, /*filled=*/true, fx.glow)) {
```
`filled` is passed as a literal `true`, not conditioned on transport state —
this remains an open gap, not a stale observation.

### Stop

When pressed:

- every musically-animated element stops with a roughly 120 ms decay;
- the playhead must not vanish abruptly in the same frame;
- the text position may still return immediately to its parked state.

### Panic

A suitable effect:

- a pink ring expanding from the button;
- 180–220 ms duration;
- no loop;
- a brief pink wash confined to the transport panel.

This is one of the few cases where a more theatrical reaction stays
professional, because the command is exceptional.

### Bar / beat

On the number change:

- the new number enters from `y + 2 px`;
- the old number exits toward `y - 2 px`;
- 80–100 ms duration.

### Status indicator

**[Verified]** The continuous blink is confirmed as currently present:
`apps/gui-sonotron/src/transport_panel.cpp:174`:
`dot_alpha = 0.55F + 0.45F * std::fabs(std::sin(fx.time * 4.0F));`

The continuous blink should be removed. Better:

- stopped: stable gray dot;
- playing: stable green;
- new connection/start: a single pulse;
- error/disconnection: pink with two brief pulses.

An indicator that blinks constantly ends up reading as a permanent warning.

---

## 6. Intention and chords — priority P1

### Chord cards

When `FOLLOWS` changes:

- the old chord fades out toward the left;
- the new chord enters from the right by 3–4 px;
- a single green glow;
- 120–160 ms duration.

When `NEXT` becomes valid:

- a single amber pulse;
- the countdown displayed as a four-segment bottom line;
- each beat extinguishes one segment.

**[Verified]** The panel already computes the beats-remaining countdown:
`apps/gui-sonotron/src/intention_panel.cpp:58`:
`const int cd = 4 - ((std::max(1, app_state.beat_num()) - 1) % 4);`

### XY pad

During drag:

- the point follows the mouse with no smoothing;
- a 2–3-position trail at low alpha;
- glow grows slightly with mouse velocity.

On release:

- a micro-spring of at most 1–2 px;
- no visible overshoot.

### Knob

Currently the value updates directly during drag. This should be split into:

```cpp
logical_value
display_value
```

- during drag: nearly identical;
- on future external changes: `display_value` reaches the target with damping;
- the pointer may have a slight lag, the arc must not.

---

## 7. Browser and Parts — priority P2

### Browser

- a cyan selection bar that grows vertically over 100 ms;
- the row wash enters with a brief fade;
- a drag source with a mini-card of the material;
- a drop target in the grid whose border expands inward.

`TreeNode` expansion should not be animated: in a compact list it tends to
produce annoying vertical movement.

No "style applied" transition should be shown, because today the selection is
still a local echo and there is no real snapshot/ack.

### Parts and mute/solo

- mute: progressive dimming of the row over 120 ms;
- solo: other rows gradually lose saturation;
- M/S latch: a brief flash, then a static state;
- amount knob: pointer smoothing.

The mute/solo transition is immediate today, even though the same state is
correctly shared between the rail and the grid.

---

# Future work

These require new data from the core, the audio engine, or an additional GPU
renderer.

## 1. Precise per-clip playhead

The immediate playhead can follow the global bar position, but to be exact it
needs:

- the clip's length;
- the current tick relative to the clip;
- loop iteration;
- quantization state;
- any clip/scene meter.

Possible new event:

```text
clip-position:
    clip_id
    tick
    length_ticks
    loop_iteration
```

At that point:

- every cell would have its own real playhead;
- Sequence Edit could show the precise position;
- clips of different lengths would not look artificially synchronized.

---

## 2. Real meters and activity

The current master VU is admittedly local and artificially generated.

Data from the audio backend will be needed:

```cpp
struct MeterSnapshot {
    float peak_l;
    float peak_r;
    float rms_l;
    float rms_r;
    bool clipped;
};
```

Animations:

- fast attack;
- slow release;
- peak hold;
- a static clip marker until acknowledged by the user.

For MIDI parts:

```text
role-activity:
    role
    active_notes
    velocity_peak
    note_on_count
```

This would allow:

- a track square that flashes on note-ons;
- a real activity meter;
- a piano-roll showing which roles are producing notes;
- routing pulses genuinely tied to MIDI.

---

## 3. Real ack and snapshot

The project still documents the absence of snapshot-on-connect and positive
acknowledgment.

With ack and authoritative state, the following could be represented:

```text
idle → commanded → pending → applied
                         ↘ rejected
```

Examples:

- style switch: amber pending, cyan/green applied;
- mute: amber latch while awaiting confirmation;
- scene launch: border progress until the boundary;
- error: a brief horizontal shake, then a stable pink state.

Without ack, these transitions should not be simulated.

---

## 4. Intention genuinely connected to the Director

Energy, tension, valence and amount are local-only today.

When they actually drive generation:

- the background halo could react to energy;
- grid density could breathe with tension;
- patterns could morph visually;
- ghost notes could show the previous and new pattern;
- a trajectory in the XY pad could visualize the Director's path over time.

Here a more spectacular effect would make sense, because it would show a real
musical transformation.

---

## 5. Real audio waveform

The project has correctly left `clip_preview_waveform` unused until real audio
content exists.

Once the pad has a real audio `LoopBuffer`:

- a downsampled waveform cache;
- a playback point;
- record/overdub regions;
- transient markers;
- fade-in/out;
- a waveform that lights up only in the played-back portion.

Procedurally generated waveforms should not be used: the current code
correctly avoids this.

---

## 6. Selective GPU bloom

The current glows are series of outlines or transparent circles. They work
well for small elements, but do not produce a genuinely soft bloom.

In the future:

1. render emissive elements into a separate texture;
2. horizontal and vertical blur;
3. additive compositing onto the main framebuffer;
4. limit the bloom to:

   - the playhead;
   - chord change;
   - a just-launched cell;
   - clip indicator;
   - warning.

Do not apply it to:

- all text;
- every panel's borders;
- the background grid;
- inactive controls.

A global bloom would make the UI less legible and visually cheaper.

---

## 7. High-impact future views

### MIDI routing view

Once telemetry is available:

- static nodes;
- small impulses along the links on note-ons;
- color per role;
- thickness tied to velocity;
- no motion without events.

### Motif evolution

For the generative motif engine:

- original notes solid;
- future transformations as ghost notes;
- retrograde shown with a reversed trajectory;
- displacement visualized as a shift;
- call/response with two different intensities.

### Performance mode

A separate mode, not a desktop-layout animation:

- the current chord, enormous;
- the next chord;
- current and next scene;
- a progress bar;
- six track statuses;
- animations exclusively beat/bar-synced.

---

# Things to avoid

1. **Sinusoids everywhere.** Two elements pulsing at different frequencies
   immediately read as "UI demo."
2. **Hover that scales widgets.** In a dense grid this creates jitter.
3. **Glow on everything.** Glow must mean energy or live state.
4. **Decorative particles.** No musical information, a lot of noise.
5. **Continuously moving gradients.** They steal attention from the Repeat
   Zone.
6. **Colors animated outside their semantics.** Green must keep meaning
   live/ok; amber, pending; pink, danger.
7. **A falsely precise playhead.** Until per-clip phase exists, it must be
   presented honestly as transport/bar position.
8. **Permanent motion at rest.** With the transport stopped, almost everything
   must settle.

---

# Recommended implementation order

## Step 1 — Foundation

- `motion.hpp/.cpp`;
- `MotionFrame`;
- edge detection for beat, bar, chords, and clip state;
- `reduced_motion`;
- a lockable clock for tests and screenshots.

The binary already has a final-frame screenshot mode; with growing animation a
deterministic clock is needed, otherwise snapshots would vary with execution
time.

## Step 2 — Hero experience

- the full cell lifecycle;
- note chase;
- active scene progress;
- beat-synced Sequence Edit playhead;
- note activation;
- transport start/stop/panic.

This is the part that genuinely changes the product's perception.

## Step 3 — Polish

- chord transition;
- XY trail;
- knob smoothing;
- mute/solo transition;
- browser selection/drop feedback;
- tab underline.

## Step 4 — Real data

- ack/snapshot;
- clip phase;
- per-role MIDI activity;
- audio peak/RMS;
- real waveform;
- GPU bloom.

## Conclusion

Sonotron does not need an external animation library. The current ImGui
architecture is already sufficient. `neon_widgets` needs to be transformed
from a collection of widgets with time-based effects into a system built on:

```text
real event
    → explicit transition
        → short easing
            → readable static state
```

The change with the best impact/risk ratio is the combination:

**shared musical clock + animated clip lifecycle + synchronized piano-roll +
chord transition.**

These four things alone would make the UI markedly more modern and
spectacular, while remaining credible as a professional workstation.
