# UI Motion System — "living instrument" for gui-sonotron

Status: **PROPOSAL** (read-only design doc, no product code touched). Date:
2026-07-16. Author: UI/motion design pass (senior C++ UI + interaction +
motion). Scope: `apps/gui-sonotron/` only (host-only, node `11600`). This is a
contract for a future implementer (Giotto), in the same shape as the other
docs under `docs/proposals/`. It proposes; it does not build. No new
third-party dependency is proposed — every mechanism below is a small internal
implementation flagged for owner sign-off where a decision is genuinely open.

Guiding rule adopted from the brief and enforced throughout this doc: **every
animation must communicate at least one of** action-feedback, state-change,
spatial relationship, hierarchy, data/audio/MIDI flow, musical
synchronisation, or control→result causality. Decorative-only motion is
confined to the ambient layer and is separately disable-able. Where a
spectacular effect would fight an immediate response, the immediate response
wins and the spectacle moves to an overlay / secondary-motion / later beat.

---

## 1. Executive summary

The GUI already has a coherent visual language (the v02 "neon hardware-synth"
theme in `src/theme.hpp`, the draw-list widget kit in `src/neon_widgets.cpp`)
and a clean single-writer frame clock (`src/layout_renderer.cpp:81-82`). What
it does **not** have is a *motion system*: today all motion is a handful of
stateless closed-form sinusoids computed inline per widget off the wall clock,
with no per-element state, no easing tokens, no interruptibility, no
frame-rate discipline beyond "the clock is real", and — critically — **no
animation is actually synchronised to the musical beat**, even though the
authoritative beat is already available on `AppState` (`bar()/beat_num()/
pulse()/beat_phase()`, `src/app_state.hpp:96-99`).

This document proposes: (a) a small internal `MotionSystem` living in the
existing `gui_sonotron_layout` library, keyed by stable *semantic* IDs, with
centralised motion tokens, spring+tween primitives, and a lifetime/GC
strategy; (b) three motion profiles (Focus / Dynamic / Extreme) plus Reduced
Motion, driven by a persisted preferences struct; (c) per-area animation
proposals anchored to the real widgets; (d) a real-time-safe bridge for
audio-reactive data reusing the existing `brain_event` / ring discipline; (e)
a deterministic test plan; and (f) a vertical slice + migration order.

---

## 2. Audit as-built

### 2.1 The frame loop, ImGui and renderer backend

- Entry / main loop: `apps/gui-sonotron/main.cpp:658-690`. One
  `while (glfwWindowShouldClose ...)`; each iteration `glfwPollEvents()` →
  `brain_session.poll(brain_events)` → `app_state.apply(event)` per event
  (`main.cpp:674-682`) → `render_frame(...)` → `present_frame(...)`.
- ImGui init: `main.cpp:480-521` — `CreateContext`, `io.IniFilename = nullptr`
  (**no ImGui state persistence at all**, `main.cpp:485`), `StyleColorsDark()`
  then `theme::apply()` (`main.cpp:486-490`).
- Renderer backend: GLFW + OpenGL3 (`imgui_impl_glfw` / `imgui_impl_opengl3`,
  `main.cpp:51-55`, `520-521`). `glfwSwapInterval(1)` → **vsync on**
  (`main.cpp:478`): the app is presently frame-rate-capped at the monitor
  refresh, so the 120–144 FPS target in the brief is a display/driver question,
  not yet a code one. Frame budget headroom exists (the whole UI is
  immediate-mode over a handful of panels).
- DPI: one-shot content-scale query at startup (`main.cpp:507-518`); no live
  rescale on monitor move (`main.cpp:509-510` says so). Minor, out of scope
  here but relevant if motion metrics are px-based (they must scale with it).

### 2.2 The one and only clock, and how "time" reaches widgets

- `src/layout_renderer.cpp:81-82`:
  ```
  fx.time    = static_cast<float>(ImGui::GetTime());   // absolute wall clock
  fx.playing = state.app_state.transport() == AppState::Transport::kPlaying;
  ```
  This is the single writer of the shared per-frame clock/gate on `V02State`
  (`src/v02_state.hpp:26-29`). Good: panels never call `ImGui::GetTime()`
  themselves (except this one site). **But there is no `DeltaTime` anywhere** —
  confirmed: the only `GetTime`/`DeltaTime` reference in the tree is this line
  and the comment beside the field (`grep` for `DeltaTime`/`GetTime` returns
  exactly two hits). A spring/tween integrator needs `io.DeltaTime`; it is not
  currently plumbed.

### 2.3 Theme / colour / token system

- `src/theme.hpp` is a clean token system: semantic `constexpr ImVec4`
  colours (`kAccent`, `kGreen`, `kAmber`, `kRed`, `kCyan`, `kLead`, …,
  `theme.hpp:33-93`), per-role tint ramp (`kRoleTint`, `theme.hpp:84-86`),
  per-track colours (`kV02TrackColor`, `theme.hpp:91-93`), and `apply()`
  (`theme.cpp`) maps them onto `ImGuiStyle`. This is the right precedent to
  copy for **motion tokens** — a parallel `motion.hpp` of named durations/
  springs, no magic numbers, exactly as colours are named here.
- **Gap:** there are no *geometry/timing* tokens. Every duration, phase
  multiplier and easing constant is an inline literal (see 2.5). The theme
  covers colour + a few `ImGuiStyle` metrics only.

### 2.4 Custom widgets (the draw-list kit)

`src/neon_widgets.{hpp,cpp}` — pure `ImDrawList`, no `BrainSession`, no model
dep (`neon_widgets.hpp:9-13`). The glow system already takes an explicit
`glow` bool and, for animated widgets, an explicit `time` + `playing`/gate so
"the widgets never read the clock themselves" (`neon_widgets.hpp:15-19`) —
this is a good seam and the MotionSystem should extend, not replace, it.

Animated widgets today, all closed-form off `time`:
- `master_vu` (`neon_widgets.cpp:267-298`): `level = 0.28 + 0.66*|sin(time*3 +
  i*0.7)|`, per-segment phase offset. **Free-running wall clock, not beat.**
- `sweep_bar` (`neon_widgets.cpp:348-355`): `phase = fmod(time, 1.7)/1.7`. A
  1.7-second wall-clock loop, **unrelated to tempo or bar length**.
- `glow_rect` / `glow_circle` (`neon_widgets.cpp:72-97`): static halo, 4 fixed
  expand layers, intensity is a caller constant, no time term.
- `knob` / `xy_pad` (`neon_widgets.cpp:158-265`): direct-manipulation, **no
  motion at all** — the value snaps to the mouse (`knob` `*value -= dy*0.006`,
  `xy_pad` sets value straight from `MousePos`). No press/hover spring, no
  settle.

### 2.5 The panels and their inline motion

- Transport (`src/transport_panel.cpp`): the status dot "blink"
  (`transport_panel.cpp:152-165`) is `0.55 + 0.45*|sin(time*4)|`, gated on
  `playing && glow`. `master_vu` db readout `render_master_vu`
  (`layout_renderer.cpp:53-56`) is `-9 + 6*|sin(time*2)|` — **a fake meter, no
  wire level source**, explicitly local-only.
- Clip launcher / Repeat Zone (`src/grid_panel.cpp`): `draw_cell`
  (`grid_panel.cpp:115-193`) glows a cell when `playing && fx.glow`
  (`:135-137`) and sweeps it when `playing && fx.playing` (`:181-183`). The
  playing bool collapses **four** real launch states into two — see 2.8.
- Sequence Edit (`src/seqedit_panel.cpp:144-149`): playhead `x = phase*avail.x`
  with `phase = fmod(time, 2.0)/2.0`. **A 2-second wall-clock loop, not the
  clip's real position, not the beat.**
- Intention (`src/intention_panel.cpp:57-59`): "NEXT →N" countdown derived
  from `beat_num()` (this is one of the few places the real beat is read), but
  the chord cards' lit/at-rest is a hard swap (`chord_card`, `:15-33`), no
  transition.

### 2.6 Interaction / animation state — where it lives, and doesn't

Client-side UI state is `V02State` (`src/v02_state.hpp`): honest local state —
`glow`, `time`, `playing`, `cell_zoom`, intent surface values, selection
bookkeeping (`open_cell`/`open_row`/`open_section`), rename buffer, auto-song
fields. **There is zero per-element *animation* state** — no stored hover
amount, no launch impulse, no spring velocity. Because all motion is
stateless closed-form, nothing needs storing today; the flip side is there is
**no infrastructure for interruptible/retargetable motion**, which the brief
requires.

### 2.7 Model / interaction-state / visual-state coupling

Mostly clean: pure-data models (`GridModel`, `PartsModel`, `SeqEditModel`,
`BrowserModel`) are threaded through `WorkstationState`
(`src/workstation_state.hpp`) with no ImGui dependency, and `AppState`
(`src/app_state.hpp`) is the re-derived engine view. The separation the brief
asks for (model / interaction-state / animation-state / rendering) is *already
half-built*: model and interaction-state exist; **animation-state is the
missing fourth layer.**

### 2.8 Concrete problems (each with evidence)

**P1 — No animation is beat-synchronised; every "musical" motion is a
wall-clock sinusoid.** `master_vu` (`neon_widgets.cpp:283`), `sweep_bar`
(`:351`), the seqedit playhead (`seqedit_panel.cpp:146`), the transport blink
(`transport_panel.cpp:155`) all key off `fx.time` (= `ImGui::GetTime()`,
`layout_renderer.cpp:81`). The authoritative musical position exists and is
unused for motion: `AppState::bar()/beat_num()/pulse()/beat_phase()`
(`app_state.hpp:96-99`), reduced from the `kBeat` heartbeat
(`brain_event.hpp:143-150`). Result: a "pulse" that visibly drifts against the
music and does not change with tempo. This is the single most important gap
versus the brief's "il beat visuale deriva dallo stato musicale ufficiale, MAI
clock".

**P2 — Widget state that is neither persistent nor in a model: transport BPM
and transpose are `static` locals.** `transport_panel.cpp:72-73`:
```
static int bpm = 120;
static int transpose_semitones = 0;
```
Function-local `static` mutable UI state: not persisted across sessions, not
unit-testable, not reset on reconnect, and a latent problem if the panel is
ever instantiated twice. It also means the *displayed* value can silently
disagree with the engine (there is no readback). Belongs in a model/interaction
struct.

**P3 — The clip launcher collapses 4 real launch states into 2, discarding the
exact feedback the brief wants.** `AppState::ClipLaunchState` has
`{kStopped, kArmed, kPlaying, kQueuedStop}` (`app_state.hpp:38`) fed by the
real `kClip` event (`brain_event.hpp:152-159`). `draw_cell` reduces it to
`bool playing = launch_state != kStopped` (`grid_panel.cpp:463-464`), so
*armed/queued* (waiting for the bar) looks identical to *playing*, and
*queued-stop* looks identical to *playing*. The "quant_wait", "queued vs
playing", "stopping" morphs the brief asks for have **no visual distinction at
all today**, despite the data being present.

**P4 — Per-frame heap allocations in hot draw paths.** Examples:
`draw_cell` builds `std::string text = ... + label` for **every filled cell,
every frame** (`grid_panel.cpp:167`); `browser_panel.cpp` allocates
`std::string hay(item)` + `std::string needle` **per item per frame** in
`matches()` (`:54-55`) and `"\xC2\xB7 " + std::string(item)` per leaf per
frame (`:78`); `intention_panel.cpp:56-59` builds `next_label`. These are
small today (tens of items) but they are exactly the "zero alloc per-frame in
hot paths" rule the brief pins, and they will bite once lists virtualise to
hundreds of clips.

**P5 — No motion tokens / magic numbers everywhere.** Durations and phase
constants are literals scattered across widgets: `time*3`, `*0.7`
(`neon_widgets.cpp:283`), `1.7F` period (`:350`), `fmod(time,2.0)`
(`seqedit_panel.cpp:146`), `time*4` (`transport_panel.cpp:155`), glow expand
`2.5F`/`2.0F` and layer count `4` (`neon_widgets.cpp:77-95`). None is named;
none is centrally tunable; two "pulses" that should feel identical use
different constants.

**P6 — `AppState::m_clip_states` is an `unordered_map<int, …>` with no lifetime
policy.** `app_state.hpp:126`. In practice bounded by grid size (6×5), but it
is exactly the "no unordered_map that grows infinitely" pattern the brief
flags, and the MotionSystem's own per-element state store must **not** repeat
it — it needs an explicit last-seen-frame GC (see §3.5).

**P7 — Direct-manipulation controls have no motion feedback and no settle.**
`knob`/`xy_pad` (`neon_widgets.cpp:158-265`) snap to the pointer; there is no
press-scale, no hover-glow ramp, no release settle. Fine for precision, but the
brief wants *microinteraction* feedback (70–100 ms press/hover) that today does
not exist for any control.

**P8 — Drag-and-drop is functional but visually inert.** Browser drag sources
set a payload and render `ImGui::TextUnformatted(name)` as the drag preview
(`browser_panel.cpp:117-121`, `152-157`); grid drop targets accept silently
(`grid_panel.cpp:336-341`, `521-534`). No lift/shadow/depth on pickup, no
"neighbours make space", no target pre-reaction before drop, no ghost of the
real result, no drop spring, no invalid-drop feedback. The whole DnD motion
surface in the brief (area 4) is greenfield.

**P9 — No profiles, no Reduced Motion, no intensity.** The only motion control
is the single global `glow` bool (`v02_state.hpp:21-22`, toggled at
`transport_panel.cpp:175`). There is no intensity slider, no ambient toggle,
no beat-synced toggle, no trails toggle, and no Reduced-Motion path that
preserves *meaning* via fade/highlight/colour-shift.

**P10 — Some draws expand beyond their logical rect without clipping.**
`glow_rect` expands the halo up to `kLayers*2.5 px` outside `[min,max]`
(`neon_widgets.cpp:77-83`) and cells draw glow before the fill
(`grid_panel.cpp:135-137`) — intended for glow bleed, but it means any future
trail/impulse layered the same way must be explicitly clipped to the zone
child or it will paint over neighbours. The label draw *is* correctly clipped
(`grid_panel.cpp:174-178`); the preview and glow are not.

---

## 3. Proposed MotionSystem

### 3.1 Where it lives

New pair `src/motion.hpp` / `src/motion.cpp` added to the **`gui_sonotron_layout`**
library (`apps/gui-sonotron/CMakeLists.txt:99-114`), next to `theme.cpp` and
`neon_widgets.cpp`. It names `ImVec2`/`ImVec4`/`float` and `ImGuiID` only —
**no `BrainSession`, no core enum, no model** — so it respects the same
invariant `theme.hpp`/`neon_widgets.hpp` already hold ("who calls ImGui
widgets / sends commands", `theme.hpp:20-24`). A second header `src/motion_tokens.hpp`
holds the `constexpr` token table, mirroring `theme.hpp`'s constant style.

No new third-party dependency. Rationale for building rather than vendoring
(the brief demands this justification): the required surface is a few hundred
lines (spring integrator + tween + a keyed store), it must be non-retained and
alloc-free in the hot path, and it must key off *our* semantic IDs and *our*
beat source — an external tween lib (retained scene graphs, per-node heap
allocation, its own clock) buys nothing and violates the constraints. **Flag
for owner: confirm "build, don't vendor".**

### 3.2 Data model

```cpp
namespace sonotron::motion {

// Frame context, filled once per frame by layout_renderer (single writer),
// passed to the system's update(). Frame-rate independent by construction:
// consumers integrate against dt, never against absolute time.
struct Frame {
  float dt = 0.0f;          // io.DeltaTime, CLAMPED (see §3.6)
  double wall = 0.0;        // ImGui::GetTime(), for ambient-only phase
  // The musical beat, straight from AppState — the ONLY beat source.
  int   bar = 0;            // AppState::bar()
  int   beat = 0;           // AppState::beat_num()  (1-based)
  int   pulse = 0;          // AppState::pulse()     (0..23)
  float beat_phase = 0.0f;  // AppState::beat_phase() (0..1 within the beat)
  bool  playing = false;
  MotionProfile profile = MotionProfile::kDynamic;
  float intensity = 1.0f;   // global Motion intensity slider, 0..1
  bool  reduced = false;    // Reduced Motion master
};

// A critically-damped (by default) spring toward a target. Retargetable in
// place: set_target() never resets position/velocity.
struct Spring {
  float value = 0.0f, velocity = 0.0f, target = 0.0f;
  // token-selected; see SpringToken. Overshoot only where a token allows it.
  float omega = 0.0f, zeta = 1.0f;
};

// A one-shot tween with delay, duration, easing; interruptible + reversible.
struct Tween {
  float from = 0.0f, to = 0.0f, t = 0.0f;     // t in [0,1]
  float delay = 0.0f, duration = 0.0f;
  Ease  ease = Ease::kOutCubic;
  bool  done = false;
};
```

`Spring`/`Tween` are POD value types operated on by free functions
(`step(Spring&, target, dt)`, `step(Tween&, dt)`) — const-correct, no hidden
allocation, trivially testable in the host harness with hand-fed `dt`.

### 3.3 The keyed store and the public API

```cpp
class MotionSystem {
 public:
  void begin_frame(const Frame& f);   // stamps generation, caches Frame
  void end_frame();                    // GC: drop entries not touched this frame

  // Retained-by-key scalar/vec/color springs & tweens. The key is a STABLE
  // SEMANTIC id (§3.4). First touch initialises to `initial` (or, with
  // at_rest=true, initialises already-settled at target — the "initialise
  // to final state" requirement). Returns the current animated value.
  float  spring(MotionId id, float target, SpringToken tok, float initial);
  ImVec2 spring2(MotionId id, ImVec2 target, SpringToken tok, ImVec2 initial);
  ImVec4 color(MotionId id, ImVec4 target, SpringToken tok, ImVec4 initial);
  float  tween(MotionId id, float from, float to, DurToken tok, Ease e);

  // Fire-and-forget impulse used for launch/drop pulses: an envelope in [0,1]
  // that rises fast and decays; retriggered by re-firing on the same key.
  void   impulse(MotionId id, DurToken tok);
  float  impulse_value(MotionId id) const;

  // Diagnostics snapshot for the Motion Diagnostics window (§6.4).
  MotionStats stats() const;
};
```

`MotionId` is a strong type (`enum class MotionId : std::uint64_t {}`) built
by hashing a small `(WidgetClass, semantic_key)` pair — **never** the ImGui
cursor or list index. Helper: `motion_id(WidgetClass::kClipCell, clip_id)`
where `clip_id` is the already-stable `role*scene_count + scene`
(`grid_panel.cpp:64-66`). Reordering a clip keeps its animation state because
the id follows the clip's identity, exactly as `AppState::clip_state(clip_id)`
already keys by that stable id (`app_state.hpp:105-108`).

### 3.4 Stable semantic IDs

The store is `std::unordered_map<MotionId, Entry>` with a strict lifetime
policy (§3.5) — **not** the unpoliced pattern of P6. IDs derive from meaning:
- clip cell → `clip_id` (stable across scene relayout).
- track row → `role_index` (`grid_panel.cpp:50-62`), not the loop counter.
- scene header → scene index (stable; columns don't reorder today).
- transport controls, XY dot, knobs → a fixed enum per control.

When an element disappears (empty cell, closed panel), its key simply stops
being touched and is GC'd next `end_frame()` — auto-cleanup, requirement met.

### 3.5 Lifetime / GC (answering P6 directly)

Each `Entry` stores `last_seen_generation`. `begin_frame` increments the
generation; every `spring/tween/impulse` call stamps the touched entry;
`end_frame` erases entries whose `last_seen_generation` is stale. To bound
cost, GC is **incremental**: sweep at most K entries per frame (round-robin
cursor), so a pathological store never stalls a frame. An `object_pool` of
`Entry` slots backs the map values to avoid per-insert allocation. Hard cap on
live entries with an LRU evict (oldest-settled first) so a bug can never grow
the map without bound. This is the concrete "last-seen frame, generation,
timeout, GC incrementale, object pool" strategy the brief asks for.

### 3.6 Frame-rate independence & degenerate dt

- `dt` is **clamped** to `[0, kMaxStep]` (e.g. 1/15 s) in `begin_frame`, and a
  large real gap (window unfocused, breakpoint) is *sub-stepped* for springs
  (fixed inner step of e.g. 1/120 s) so a huge `dt` cannot make a spring
  explode. `dt == 0` is a legal no-op (values unchanged) — a required test
  case (§7).
- Springs use the analytic critically-damped solution where `zeta == 1`
  (stable for any `dt`), numeric semi-implicit Euler otherwise (stable under
  the sub-step). This gives the "stabile con DeltaTime piccolissimo/grandissimo"
  guarantee.
- Off-screen / not-visible: callers still call `spring(...)` so logic
  completes, but the *draw* is skipped (the panel already early-returns for the
  unopened seqedit, `seqedit_panel.cpp:107-114`); the system's update cost is a
  cheap integrate with no draw. "Completa la logica senza render."

### 3.7 Integration with the existing clock

`layout_renderer.cpp:78-82` becomes the single writer of a `motion::Frame`
(dt from `ImGui::GetIO().DeltaTime`, beat fields from `state.app_state`,
profile/intensity/reduced from the new preferences), calls
`motion.begin_frame(frame)` before drawing the bands and `motion.end_frame()`
after. `V02State::time`/`playing` stay (ambient layer still wants wall phase),
but **beat-synced motion reads `frame.beat_phase` etc., never `fx.time`** —
this is the fix for P1. The beat pulse for a "downbeat flash" is derived as:
a new pulse envelope fires when `frame.beat != last_beat` (edge-detected in
the system, not a clock), amplitude larger when `frame.beat == 1` (downbeat).
The system **interpolates** between beats for smoothness but the authoritative
edge is the engine's `kBeat` event — never a synthesised metronome.

### 3.8 Motion tokens (no magic numbers)

`motion_tokens.hpp`, `constexpr`, named exactly like `theme.hpp` colours:

| Token | Value (start) | Use |
|---|---|---|
| `kDurFeedback` | 80 ms | press / hover / toggle (brief: 70–100) |
| `kDurMicro` | 140 ms | selection / highlight (120–180) |
| `kDurPanel` | 220 ms | panel open/close (180–260) |
| `kDurStructural` | 320 ms | structural change (260–400) |
| `kDurCinematic` | 520 ms | ambient/scene events only, non-blocking (>400) |
| `kSpringPrecise` | ζ=1.0, ω≈22 | productive controls, ~0 overshoot |
| `kSpringSoft` | ζ=1.0, ω≈14 | panel/settle |
| `kSpringEnergetic` | ζ≈0.6, ω≈26 | musical/performative, visible overshoot |

All are `constexpr` and are the *only* place these numbers exist. Existing
inline constants (P5) get replaced by token lookups as each widget migrates.

---

## 4. The three profiles + controls + Reduced Motion

### 4.1 Preferences struct

New `struct MotionPrefs` (persisted — see below), read once per frame into
`motion::Frame`:
```cpp
enum class MotionProfile { kFocus, kDynamic, kExtreme };
struct MotionPrefs {
  MotionProfile profile = MotionProfile::kDynamic;  // default
  float intensity = 1.0f;      // global Motion intensity 0..1
  bool  ambient   = true;      // ambient/parallax layer
  bool  beat_fx   = true;      // beat-synced effects
  bool  trails    = true;      // motion trails
  bool  reduced   = false;     // Reduced Motion master (overrides the rest)
  bool  decorative = true;     // decorative-only effects, separately killable
};
```

### 4.2 Profile semantics

- **Focus** — motion minima: feedback + state + spatial continuity only. No
  ambient, no parallax, no trails, no beat pulse on peripheral chrome. For long
  editing sessions. Concretely: springs still run (they *are* feedback), but
  `impulse` amplitudes are scaled ~0.4, ambient background phase is frozen,
  `sweep_bar`/trails are off.
- **Dynamic** (default) — full animation set: live modulation, physical drag,
  fluid panels, beat pulse where it carries meaning. This is the current visual
  ambition, made coherent and beat-locked.
- **Extreme** — the headline aesthetic: audio-reactive glow, controlled trails,
  beat-synced pulsation, shape-transition morphs on clip state, energy flow
  along routing, more physical drag/launch/drop, depth layering, ambient
  motion, strong-but-brief scene/fill/break events, recognisable short effects
  for important actions — **all without altering hitboxes or workflow** (the
  hitbox is `InvisibleButton` sized independently of any visual overshoot;
  motion only moves *pixels drawn*, never the item rect).

`intensity` multiplies all non-essential amplitudes (glow strength, overshoot,
trail length, ambient) but **never** durations of feedback that gate perceived
responsiveness. At `intensity == 0`, only meaning-critical state fades remain —
a required test (§7).

### 4.3 Reduced Motion

`reduced == true` (or OS "prefers reduced motion" if we later read it)
short-circuits springs/tweens to **immediate value assignment**, and replaces
each motion-carried meaning with a non-motion equivalent:
- launch impulse → instant colour-shift + outline pop (no scale).
- queued→playing morph → immediate fill/outline swap + a single 1-frame
  highlight, no progress sweep.
- panel open → instant appear with a one-frame border flash for locus.
- playhead → a discrete stepped position per beat, not a continuous slide.

The rule enforced: Reduced Motion never *removes* information, it changes its
*carrier* from motion to colour/contrast/outline/immediate-substitution.

### 4.4 Controls surface & persistence

A new "Motion" section under **Edit ▸ Preferences** (today a disabled
placeholder, `main.cpp:379`) hosts: profile segmented control, intensity
slider, and the four toggles (ambient / beat-synced / trails / reduced) plus a
separate "decorative effects" kill switch. Persistence rides the **existing**
save-on-exit discipline: `MotionPrefs` serialises into `layout.json` alongside
`font_size_px` (the layout load/save already exists, `main.cpp:495-504`,
`704-707`; schema-upgrade path already tolerates missing keys — see the layout
schema-upgrade test). No new file, no new dependency. The transport `glow`
toggle (`transport_panel.cpp:170-181`) is retained as a fast in-reach shortcut
but becomes a thin alias over `MotionPrefs` (glow off ≈ decorative off), so the
two controls never disagree.

---

## 5. Per-area proposals (anchored to real widgets)

For each area: the target motion, the exact file/seam it hooks, and **what must
be refactored first**.

### 5.1 Transport (`transport_panel.cpp`, `layout_renderer.cpp:44-65`)

- **Play/Stop/Record/Pause transitions:** cross-fade the pad fill/border colour
  via `motion.color(id, target, kSpringPrecise)` in `pad_button`
  (`neon_widgets.cpp:130-156`) instead of the instant `filled?accent:kStopDark`.
  A record state (not yet distinct) reads as a steady strong red *fill*, not a
  blink (brief: "recording forte non lampeggiante").
- **Discrete musical pulse:** the status dot (`transport_panel.cpp:152-165`)
  and wordmark stop using `sin(time*4)` and instead pulse on the **beat edge**
  from `frame.beat_phase` with a downbeat accent on `beat==1`. This is the P1
  fix in its smallest form and is part of the vertical slice.
- **Playhead:** a real bar:beat readout already exists
  (`transport_panel.cpp:128-143`); add a thin continuous fill under it driven
  by `beat_phase`, resetting cleanly on the beat edge; tempo changes are
  continuous because `beat_phase` is tempo-derived, not a fixed period.
- **Refactor first:** P2 — lift `static int bpm/transpose` into a small
  `TransportUiState` (interaction-state layer) threaded via `WorkstationState`,
  so the readout can animate on nudge and is testable. Count-in / paused states
  need an `AppState` state to exist before they can be animated (flag: the
  engine already has `kPaused`, `app_state.hpp:31`).

### 5.2 Clip launcher / scenes (`grid_panel.cpp`)

- **Distinguish the four launch states (P3):** map `ClipLaunchState` to distinct
  motion in `draw_cell` (`grid_panel.cpp:463-464`):
  - `kArmed` (queued) → a **progressive quantisation fill** that grows with
    `beat_phase` toward the next bar boundary (the "attesa" the brief wants) +
    a pulsing dashed border, distinct from playing.
  - `kPlaying` → steady glow + the sweep, now beat-locked (sweep period = one
    bar derived from beat, replacing `sweep_bar`'s fixed `1.7s`,
    `neon_widgets.cpp:350`).
  - `kQueuedStop` → the fill **drains** toward the boundary (mirror of armed).
  - `kStopped` → settle to rest via `kSpringPrecise`.
- **Launch impulse:** on click-launch (`grid_panel.cpp:496`) fire
  `motion.impulse(clip_cell_id, kDurStructural)` → a brief scale/glow pop *of
  the drawn pixels only* (hitbox `InvisibleButton` unchanged, `:119`). In
  Extreme, a stronger "at the musical boundary" flash when `kArmed→kPlaying`
  actually flips (edge-detected from the `kClip` event, not guessed).
- **Scene propagation:** a scene-header launch (`grid_panel.cpp:364-388`) sends
  `launch scene`; visualise the column filling left-to-right as each cell
  arms, and light the active-scene header via a `motion.color` ramp instead of
  the instant tint swap (`grid_panel.cpp:352-355`).
- **Refactor first:** none structural — the stable `clip_id` and the real
  `clip_state` readback are already there (`app_state.hpp:105-108`). This area
  is the highest-value, lowest-risk migration.

### 5.3 Arranger / sections (`intention_panel.cpp`, section state on `AppState`)

- **Current / requested / next section, transition point, fill waiting/active,
  ending:** the section name is on `AppState::section()`
  (`app_state.hpp:70`); chord-follow current/next are real events
  (`app_state.hpp:85-89`, `intention_panel.cpp:48-63`). Animate the FOLLOWS/NEXT
  chord cards (`intention_panel.cpp:15-33`) with a colour/opacity spring on
  the lit→unlit change, and a "staged" shimmer on NEXT that **only** starts
  once the engine confirms the pending chord (`chord_followed_next_valid()`) —
  the brief's "MAI animare prima della conferma del motore" is satisfiable
  because the trigger is the confirmed event, not the user's click.
- **Refactor first:** requested-vs-current section needs an "armed section"
  field on `AppState` (today only the committed `section` exists). Flag: this
  is an additive reduce of an existing event, not new wire. Until then, animate
  only what is confirmed.

### 5.4 Drag-and-drop (`browser_panel.cpp` sources, `grid_panel.cpp` targets)

Greenfield (P8). Proposed, all inside the existing ImGui DnD calls:
- **Lift:** on `BeginDragDropSource` (`browser_panel.cpp:117`), draw the drag
  payload with a depth shadow + track/role glow via a `motion.impulse` on a
  "drag" id; the ghost shows the **real** result (a mini clip preview via the
  existing `neon::clip_preview_pianoroll`), not just text.
- **Targets pre-react:** grid cells/scene headers, while a compatible payload
  is being dragged (`ImGui::IsDragDropActive()` + payload type check), spring
  a subtle inset highlight *before* the drop; incompatible targets stay inert.
- **Neighbours make space:** when hovering an insert position, adjacent cells
  spring a small offset (drawn-pixel only) to reveal the slot.
- **Drop:** valid drop fires a short `kSpringEnergetic` settle at the target;
  invalid drop (wrong payload) gives a brief non-punitive shake/return of the
  ghost, no red-flash "error".
- **Hitboxes:** every offset is a draw offset; the `InvisibleButton`/`Selectable`
  rects are untouched, so input stays aligned (brief's hard constraint).
- **Refactor first:** the drag preview needs the resolved preview pattern at
  the source; `preview::preview_for` is already reachable from the layout lib
  (`preview.hpp`), so the browser source can compute a ghost the same way the
  grid does. No structural change.

### 5.5 Panels / navigation (`layout_renderer.cpp`, `main.cpp` menu/dialog)

- **Panel open/close** (View toggles flip `zone->visible`,
  `main.cpp:384-391`; SoundFont modal, `main.cpp:315-340`): emerge from the
  semantic direction (a right-rail panel slides/fades from the right), focus
  preserved, input never blocked mid-transition, instant reversal if re-toggled.
  Drive width/alpha with `motion.spring(kSpringSoft)` keyed by zone id.
- **Shared-element** transition when a clip opens into Sequence Edit: the cell's
  colour/label animates toward the seqedit header (both already share
  `track_color`, `seqedit_panel.cpp:31-34`) — a `motion.tween` on a transient
  overlay, non-blocking.
- **Refactor first:** the modal is a plain `BeginPopupModal`; a fade needs the
  popup alpha to be animatable — minor, wrap in a `motion.spring` on
  `ImGuiStyleVar_Alpha`. The zone-visibility toggles are booleans today; add an
  animated width/alpha per zone in the layout renderer.

### 5.6 Modulation / routing (`intention_panel.cpp`, `parts_panel.cpp`, `neon_widgets` knob)

- **Live parameter motion + base-value/mod-amount arc:** the `knob`
  (`neon_widgets.cpp:158-213`) draws a single value arc; extend it to draw a
  **base** arc (set value) plus a **modulated** arc segment (effective value)
  in a distinct tone — the Serum/Vital idiom. Today the intent values
  (`V02State::energy/tension/valence/part_amount`) are local-only
  (`v02_state.hpp:41-47`), so the "modulated" ring is a design-ready shell
  until a mod source exists (flag).
- **Flow along connections:** when a mod/route is created, animate a directional
  pulse from source to destination proportional to amount; distinct visual
  language per MIDI / audio / control / sidechain (colour + dash pattern
  tokens). Greenfield; depends on a routing model that does not yet exist in
  the GUI — **flag as a later area**, propose the motion vocabulary now.
- **Refactor first:** knob press/hover feedback (P7) first — add
  `motion.spring` press-scale to `knob`/`xy_pad` regardless of the mod work.

### 5.7 Piano-roll / sequencer (`seqedit_panel.cpp`)

- **Playhead (P1):** replace `fmod(time,2.0)` (`seqedit_panel.cpp:146`) with a
  beat-derived position: the clip's play position from `beat`/`beat_phase`
  spanning the clip's bar length, so the playhead tracks the *music*. This is in
  the vertical slice.
- **Note insert / resize preview / move / ghost notes / velocity/probability:**
  the canvas currently draws static resolved blocks (`seqedit_panel.cpp:130-142`).
  As real editing lands, each note gets a `motion.spring` on position/size (move
  glides, resize previews), velocity maps to fill alpha with a spring, and
  quantise is shown as **convergence** (notes spring to the grid) rather than a
  jump.
- **Note-active flash:** when the playhead crosses a note, a brief
  `motion.impulse` on that note id (beat-locked).
- **Refactor first:** the seqedit is preview-only today (no editing model). The
  motion hooks are cheap to add to the existing draw loop for the playhead and
  active-note flash now; the edit motions wait on an edit model (flag).

### 5.8 Mixer / meters (`layout_renderer.cpp` master VU, `neon_widgets.cpp:267`)

- **Credible meter ballistics:** `master_vu` today is a fake `|sin|`
  (`neon_widgets.cpp:283`) and the dB readout is fake
  (`layout_renderer.cpp:55`). Replace with a real attack/release envelope
  **once a level source exists** on the wire/ring; the meter's *ballistics*
  (fast attack, slow release, peak-hold, clip-warning latch) are a
  `motion`-adjacent smoothing that belongs here. **Flag:** no level metering is
  on the wire today (`layout_renderer.cpp:54` says so) — propose a reduced
  audio-reactive datum via the ring bridge (§6), do not fake it further.
- **Mute/solo/arm/monitor:** the grid M/S latches (`grid_panel.cpp:31-39`,
  `427-437`) and the solo-dim (`:412-420`) hard-swap; animate the engage/dim
  with `motion.color`. Fader would settle with `kSpringPrecise`.
- **Refactor first:** meter needs the level bridge (§6.2). Latch animation
  needs nothing.

### 5.9 Microinteractions (all panels)

Hover / press / toggle / selection / focus / rename / duplicate / delete /
undo/redo / invalid / save / loading / background-task / MIDI connect-disconnect
/ audio-MIDI error — each gets an **immediate, brief** (`kDurFeedback`) motion:
press-scale, hover-glow ramp, selection outline spring, rename field
emergence (`grid_panel.cpp:310-333` already swaps to InputText — add a
one-frame focus flash), connect/disconnect a colour spring on the status dot
(`transport_panel.cpp:152-165`, driven by `AppState::connected()`,
`app_state.hpp:45`). Errors (`kError`, `brain_event.hpp:98`) get a brief,
non-punitive attention pulse, never a sustained blink.

---

## 6. Real-time bridge & performance

### 6.1 Constraints honoured

- **No animation in the audio thread.** The audio path is `AudioBackend` +
  `SoundfontEngine` on the miniaudio callback (`main.cpp:564-584`); the engine
  thread owns the Runtime and pushes MIDI onto a ring
  (`in_process_brain_session.hpp:70-78`). The MotionSystem lives entirely on
  the **render thread** and never touches either. No lock, no alloc in the
  render callback of the audio device — unchanged by this proposal.
- **UI consumes snapshots/events only.** Motion reads `AppState` (already
  reduced from `poll()`, `main.cpp:674-682`) and never reaches into the engine
  — there is deliberately no `engine()`/`stage()` accessor
  (`in_process_brain_session.hpp:33-35`). Effects change *pixels*, never MIDI
  timing.

### 6.2 The one new datum: reduced audio-reactive level (bridge proposal)

For credible Extreme-profile audio-reactive glow and a real master meter, the
UI needs a *bounded, pre-reduced* level, not raw audio. Proposal, mirroring the
existing narrow ring discipline: `AudioBackend` (which already sees every
realised note on its device callback, `main.cpp:584`) computes a cheap RMS/peak
per block and publishes a tiny `struct AudioLevel { float rms, peak; }` into a
**single-slot lock-free cell** (an atomic double-buffer, not a growing queue),
which `InProcessBrainSession` exposes via one new accessor of the same shape as
`set_audio_ring` (`in_process_brain_session.hpp:78`). The render thread reads
the latest value once per frame; the meter's ballistics (§5.8) smooth it. This
is the "dati audio-reactive ridotti/bufferizzati prima della UI" requirement,
built from the existing ring/atomic vocabulary, **not** a new library. **Flag
for owner:** this adds one small host-only datum on the integrated path only
(`--control` stays silent, as audio already is); confirm scope.

### 6.3 Performance plan

- **Zero per-frame alloc in hot paths (P4):** cache the cell label string on the
  `GridCell`/interaction layer (rebuild only on change), precompute the
  lowercased browser haystack once per model change (not per frame per item),
  and build the "▶/▷ label" without a fresh `std::string` per cell (write into
  a stack buffer). The MotionSystem itself pools its entries (§3.5) and does no
  per-frame heap work.
- **Clipping (P10):** every trail/impulse/glow that can exceed its logical rect
  is drawn inside an explicit `PushClipRect` of its zone child; the zone
  children already exist (`begin_zone`, `layout_renderer.cpp:37-42`).
- **Virtualisation:** clips/tracks/lists render only the visible/active rows
  (the grid is small today, but the browser lists and any future clip list must
  virtualise before hundreds of rows). Off-screen elements integrate but skip
  draw (§3.6).
- **Caps & degradation:** trail length and impulse count are capped by token;
  under a frame-budget overrun (measured, §6.4) the system auto-drops the most
  expensive optional layers first (ambient, then trails, then glow layers),
  preserving feedback/state motion.
- **Batching:** glow/halo layers already batch on one `ImDrawList`; keep new
  effects on the same list to avoid extra draw calls.

### 6.4 Motion Diagnostics window

A debug ImGui window (behind a compile/runtime flag, sibling to the existing
`SONOTRON_GUI_*` escape hatches in `main.cpp:149-166`) showing: active
animations, stored entries, update time, effect-render time, primitives
produced, elements GC'd this frame, current profile + intensity, and
frame-budget violations. `MotionStats` (§3.3) is the data source; it is also
the observability the brief requires for every animation.

---

## 7. Deterministic test plan (proposed cases, host harness)

All testable **without a GPU** — the MotionSystem operates on POD springs/
tweens with hand-fed `Frame{dt,…}`, exactly like the existing model tests
(`apps/gui-sonotron/tests/`, e.g. `test_app_state.cpp`,
`test_grid_panel_auto_song.cpp`) drive pure logic. Determinism: effects are
closed-form or fixed-step-integrated; any pseudo-random amplitude is seeded
**from the event/id** (like `neon_widgets.cpp:19-24`'s xorshift seeded per
clip), never a global RNG; a fixed-`dt` mode makes runs reproducible.

Proposed cases:
1. **Spring convergence** — settles to target within tolerance under fixed dt;
   critically-damped token never overshoots.
2. **Retargeting mid-flight** — `set_target` changes destination without
   resetting position/velocity; no discontinuity.
3. **Reversal** — toggle target back before settle; returns smoothly.
4. **Cancellation** — element key stops being touched → entry GC'd next frame.
5. **`dt == 0`** — no-op, values unchanged, no NaN.
6. **Frame stall (huge dt)** — clamped + sub-stepped; spring stays bounded.
7. **Cleanup / GC** — store size returns to steady state after elements vanish;
   incremental GC never exceeds K erases/frame.
8. **Reused id** — a new element reusing a freed id starts from `initial`
   (or at-rest), not stale state.
9. **Not-visible element** — logic integrates, draw skipped; state correct when
   it reappears.
10. **Profile change** — switching Focus/Dynamic/Extreme changes amplitudes,
    not correctness; no pop on switch (values continuous).
11. **Reduced Motion** — springs collapse to immediate assignment; meaning
    (state colour) still changes.
12. **Intensity == 0** — only meaning-critical fades remain.
13. **Beat sync** — feeding a `Frame` sequence with a beat edge fires exactly
    one pulse per beat, downbeat amplitude larger; no pulse when `dt` advances
    without a beat change (proves it is edge-driven, not clock-driven).
14. **Stress** — thousands of keyed entries: GC bounded, no unbounded map
    growth (guards P6).

---

## 8. Vertical slice + migration order

### 8.1 First increment (files, in order)

1. `src/motion_tokens.hpp` + `src/motion.{hpp,cpp}` — the system, tokens,
   `Spring`/`Tween`/`impulse`, keyed store, GC. Add to `gui_sonotron_layout`
   (`CMakeLists.txt:99-114`). **Tests first** (cases 1–9, 14).
2. `MotionPrefs` + persistence into `layout.json` (extend the existing
   load/save, `main.cpp:495-504`/`704-707`; layout schema-upgrade already
   tolerates unknown/missing keys). Preferences UI under Edit ▸ Preferences
   (`main.cpp:379`).
3. `layout_renderer.cpp:78-82` — build `motion::Frame` (add `io.DeltaTime` +
   beat fields), call `begin_frame`/`end_frame`. Wire the Motion Diagnostics
   window.
4. **Transport pulse** — status dot + wordmark beat-locked off `beat_phase`
   (P1 fix, `transport_panel.cpp:152-165`).
5. **One openable panel** — animate the SoundFont modal or a right-rail zone
   open/close (`main.cpp:315-340` / `layout_renderer.cpp:112-114`).
6. **One clip cell** — the 4-state morph + launch impulse in `draw_cell`
   (`grid_panel.cpp:463-464`, `496`).
7. **Drag-drop of one clip** — lift + target pre-react + drop settle for the
   browser-style→cell path (`browser_panel.cpp:117-121`,
   `grid_panel.cpp:521-534`).
8. **One modulated parameter** — knob press-scale + base/mod arc on one
   Intention knob (`neon_widgets.cpp:158-213`, `intention_panel.cpp:79-83`).
9. **Reduced Motion** — the immediate-substitution path for all of the above.

Slice acceptance: builds under the existing `-Wall -Wextra -Werror`
(`CMakeLists.txt:113`); new host tests green; visible beat-locked transport
pulse; a clip cell that visibly distinguishes queued from playing; a drag with
a real ghost; all three profiles + Reduced Motion switchable and persisted.

### 8.2 Migration order for the rest (by impact / risk)

| Order | Area | Impact | Risk | Why |
|---|---|---|---|---|
| 1 | Clip launcher 4-state + scenes (5.2) | High | Low | data already present, stable ids |
| 2 | Microinteractions (5.9) | High | Low | pure feedback, no new state |
| 3 | Seqedit playhead beat-lock (5.7) | High | Low | one-line replace of `fmod` |
| 4 | Drag-and-drop full (5.4) | High | Med | greenfield but self-contained |
| 5 | Transport states/record/count-in (5.1) | Med | Med | needs `AppState` states |
| 6 | Panels/navigation (5.5) | Med | Med | zone visibility → animated width/alpha |
| 7 | Arranger sections (5.3) | Med | Med | needs "armed section" reduce |
| 8 | Mixer/meter ballistics (5.8) | Med | Med | needs the level bridge (§6.2) |
| 9 | Modulation/routing flow (5.6) | High(Extreme) | High | needs a routing model |
| 10 | Seqedit editing motions (5.7) | Med | High | needs an edit model |

---

## 9. Risks / what NOT to do / dependencies

**Do NOT:**
- Add an external animation library. Build the small internal system (§3.1);
  a retained-mode tween lib violates non-retained + no-alloc + our-beat-source
  constraints. (Flag: owner confirms build-not-vendor.)
- Introduce a second retained-mode UI or change the UI framework — everything
  stays immediate-mode Dear ImGui.
- Let any effect move a hitbox: motion changes drawn pixels only; every
  `InvisibleButton`/`Selectable` rect stays input-authoritative.
- Synthesise a metronome for the visual pulse. The beat edge is the engine's
  `kBeat` event via `AppState` (`app_state.hpp:96-99`); the system may
  *interpolate* between beats but the authority is the engine.
- Fake data to feed an effect (no fake meter beyond what already exists — the
  current fake dB/`master_vu` should be *replaced* by the real reduced level,
  not extended). Animate only engine-confirmed state (sections, chord-next).
- Grow an `unordered_map` without a lifetime policy (P6) — the store is GC'd
  and capped (§3.5).
- Animate structural layout in a way that loses the user's observation point
  (magnetic/continuous transitions only).

**Risks:**
- vsync cap (`main.cpp:478`) means the 120–144 FPS target needs a display that
  supports it and possibly an opt-out of `glfwSwapInterval(1)`; the motion math
  is dt-correct regardless, so this is a presentation decision, not a
  correctness one.
- Some areas (routing flow, seqedit editing, requested-section, record/count-in,
  audio level) depend on data/models that **do not exist yet**; those proposals
  are motion *vocabulary* ready to attach when the data lands, and are flagged
  as such — they must not be implemented by faking the missing data.
- Per-frame alloc cleanup (P4) touches the browser/grid draw loops; keep it a
  separate, test-covered refactor so it does not entangle the motion slice.

**Dependencies (owner sign-off required — none added unilaterally):**
- No new third-party dependency proposed.
- One new host-only, integrated-path-only datum (reduced `AudioLevel`, §6.2)
  reusing the existing ring/atomic pattern — flagged, not built.
- `MotionPrefs` persisted inside the existing `layout.json` (no new file).

---

### Owner decisions this doc raises

1. **Build vs vendor the MotionSystem** — proposal is a small internal
   implementation in `gui_sonotron_layout`; confirm no external tween library.
2. **The audio-reactive level bridge (§6.2)** — add one reduced `AudioLevel`
   datum on the integrated path so Extreme's audio-reactive glow and the master
   meter are *real* rather than faked, or keep the meter honestly inert until a
   wire source exists?
3. **Default profile & persistence surface** — default to **Dynamic** with
   Reduced Motion honouring a future OS setting, and persist `MotionPrefs`
   inside `layout.json` (vs a new prefs file / vs Focus-by-default for a
   pro-audio audience)?
