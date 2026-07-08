# Reflection — Hybrid chord-reactive arranger: gap analysis vs arrangrr as-is

Status: critique / verdict (Prospero). Not a locked decision. Read against
`docs/DESIGN.md` (D1–D37) and the code as of branch `host-tui-h3`.

Two long reflections (market research + an architectural proposal for a modern
"chord-reactive pattern arranger") were put on the table. The question was NOT
"design a generic greenfield", but: **what in arrangrr-as-it-is-today is worth
keeping, reworking, or replacing** with the reflections' ideas, to make it
"useful, modern, functional, complete". Rewriting from scratch is permitted where
it genuinely earns it. This is therefore a gap analysis: arrangrr vs the
reflections, on the engineering axis, the musical axis, and above all the
intersection where both reflections actually live (a harmonically reactive
arranger under realtime/embedded constraints).

Anchored in: `docs/DESIGN.md` (D1–D37), `components/arrangrr/include/arrangrr/engine.hpp`,
`arranger/arranger.hpp`, `arranger/style_model.hpp`, `chord/theory.hpp`,
`chord/chord_engine.hpp`, `chord/chord_detector.hpp`, `arp/arpeggiator.hpp`,
`arranger/groove.hpp`, `abi.hpp`, and `components/hostrt/` (jsonl, alsa_midi, TUI
panels/shell).

---

## On the table

Both reflections describe the same object — a modern chord-reactive pattern
arranger: live chords treated as a high-priority harmonic control flow, patterns
stored in **relative** form and resolved through a Harmonic Mapper, a section
manager with musical launch quantization, a reversible performance↔pattern MIDI
looper, energy/tension macros, a headless/CLI-first architecture portable from
Linux x86 to STM32, a hybrid versioned project format, MPE/MIDI2-readiness, and
per-port latency compensation. The verdict is a gap analysis: where arrangrr
already has what the reflections "propose" (often in a more disciplined form),
where the reflections point at something genuinely better or missing, and where
they are premium-groovebox feature-creep that would betray arrangrr's lean,
embedded-first thesis. Governing thesis for the judgment: a chord-tone-only
pattern is a **musical** decision, not merely a data-model one; and a JSON daemon
in the core is a musical decision wearing a transport costume.

---

## Keep

**1. The no-heap / dual-target / integer-tick total-order determinism spine
(D29/D32/D33).** — Engineering axis, rooted in the intersection. This is
arrangrr's real capital and it is *sturdier* than any stack the reflections
propose (JUCE, RTNeural, Lua, SBC CM4 + coprocessor MCU). The core is already
`advance_ticks`-pure, injected time, total order `(@tick, class_priority,
seq_no)` with `NoteOff < NoteOn`, 8-byte `Event`, static pools under 512 KB.
Total-order determinism is not pedantry: it is *reproducible groove* — the swing
and humanize in `groove.hpp` are a deterministic position hash, so "same seed ⇒
same feel" is a verifiable fact. The reflections list "golden tests + deterministic
seeds" as an aspiration; arrangrr has it as a constructive property. Untouchable.

**2. The NTT mechanism as the Harmonic Mapper (D24, `Arranger::resolve`).** —
Intersection. Reflection 1 lists "relative-pattern engine + Harmonic Mapper" as
must-have #2; arrangrr already has it: `RolePolicy::kChordTone` + `resolve()`
mapping a chord-tone index → concrete note against the live `ChordState`, with
`kRoleAnchor` per-role registers (D36). The *mechanism* — a neutral pattern
resolved at playback against the live chord — is exactly right and survives. It is
the *vocabulary* of that mechanism that is too poor (→ Rework 1); do not throw the
frame to widen it.

**3. The headless doctrine: binary core ABI (D26) + host-only L0 JSONL / L1 paths
/ L2 REPL.** — Engineering axis, intersecting the embedded target. Reflection 2
wants "state-centric core, CLI-first, GUI as client, no JSON in the hot path" —
and then proposes JSON-RPC over UDS as the control plane, contradicting itself.
arrangrr already *resolved* that contradiction: the core receives `Command{op,
param, idx, a, b, c}` POD; JSONL (de)serialization lives in `platform/host`. This
is *better* than JSON-RPC-in-the-core because there is no parsing/heap crossing
the STM32 boundary. The current TUI is not in conflict with headless: it is an L2
client on the same protocol. Keep the binary ABI as the sovereign contract.

**4. The section manager (`style_model.hpp` + `Arranger::on_tick`).** —
Intersection. 13 `SectionType`s from day one (ABI-stable), bar-quantized switching,
one-shot fills/intros returning to the active variation (`m_return_to`), endings
that stop the transport, style-switch landing *together* with its section on the
same downbeat. This is exactly the "arranger semantics" reflection 1 accuses modern
sequencers of lacking. It holds, and beats the described competition.

**5. Chord intelligence: diatonic smart-quality + shell completion +
chord-memory/hold-last + `set_context` without double-voicing (D12/D19/D34).** —
Intersection. Reflection 1 lists "robust live chord detection" as must-have #1;
arrangrr has it, with a finesse the reflections never name — the piano→chord path
does *not* sound a second voicing (`set_context` only steers NTT; the keys already
sound through routing), and hold-last is pro-arranger behaviour. `smart_quality`,
derived by stacking thirds on the mode with the V-dominant-in-minor exception, is
correct theory tested at compile time. Keep.

**6. Parametric groove + the *planned* deterministic Generative Director (D37)
instead of AI.** — Intersection. Reflection 1 proposes an "Energy Morph Engine"
and a "light AI module (RTNeural)"; arrangrr already has `groove.hpp`
(deterministic swing/accent/humanize) and has already *conceived* the
energy/tension macro as an interpolated `DirectorState`+`DirectorTarget`,
explicitly non-AI. That is the right answer: a deterministic parameter trajectory
that pilots Arranger/Arp/Groove without bypassing them. Keep the plan; it is
superior to the proposal.

**7. Role-based free routing with a parts mixer (`Arranger` routes +
`kPartMute/kPartSolo`).** — Engineering/musical. Reflection 2 warns: "do NOT
hard-code 1/2/4 sequencers per device; use logical tracks with free routing,
device-groups only as a view". arrangrr already does this: `TrackRole` →
`Route{port, channel}`, per-role mute/solo. Already aligned with the reflection's
correct advice. Keep.

---

## Rework

**1. The pattern event model: `kFixed`/`kChordTone` → add `ScaleDegree`,
`RelativeInterval`, `ChordGesture` (and an explicit `RhythmOnly`).** —
Intersection; the sharpest verdict. Today `StyleEvent.tone` is only a chord-tone
index (or a fixed note). The code comment boasts of it: "every resolved note is a
chord tone by construction, so wrong notes are impossible". But "wrong notes
impossible" also means **right passing notes impossible**: a bass that can only
hit root/third/fifth/seventh cannot *walk*, cannot use approach notes, cannot play
the diatonic 2 or 6 as a passing tone; a melody cannot be scalar. This is the
aesthetic amputation the thesis hunts. The reflections name the fix with the
taxonomy: `ScaleDegree` (1,b3,5,9 — diatonic melodies/basslines, resolved against
**key+scale**, not the chord shape alone), `RelativeInterval` (+7,−12 —
transposable riffs preserving the gesture, e.g. a blues lick), `ChordGesture`
(close/spread/drop2/slash-bass — a trigger for the voicing engine). **What →
toward what:** extend `RolePolicy`/`StyleEvent` with these types (the
infrastructure is already there: `theory::scale_of`/`degree_of` are constexpr, and
`ChordState` carries enough to resolve scale-degrees if given the `Key`). Do **not**
rewrite the core: this is a data-model extension plus one more branch in
`resolve()`. It is the single change that raises musical quality the most, and it
is all compile-time/bounded (D32-compatible).

**2. Voicing: root-position close-stack → voice-leading (common-tone retention,
close/drop2) + Harmonic Spillover.** — Intersection. `ChordEngine::sound()`
releases the previous voicing and re-stacks from root position: every chord change
is a jump, not a voice-leading. Reflection 1 names it well as "Harmonic Spillover"
(sustained notes led by step toward the new chord instead of a brutal retrigger).
**What → toward what:** a voicing resolver that keeps common tones and moves the
rest by minimal step, with `ChordGesture` (from point 1) choosing the spacing. It
is tied to point 1 — `ChordGesture` is the trigger, the resolver is the engine.
Cost: bounded (max 4–5 voices), fixed-point, no heap. Latency here *is* phrasing:
a dry retrigger on every chord is a musical choice, and today it is the wrong
default.

**3. The MIDI Looper: planned but unbuilt (§13/M7) → build it AND make it
reversible performance↔pattern.** — Intersection. The reflections call reversible
conversion "one of the three things everyone asks for and nobody solved well", and
they are right: capturing a live loop and converting it to degrees/roles
**relative to the chord active at capture time** makes it re-harmonizable and
transposable — exactly the "capture" gesture of D10 and the functional storage of
D28. **What → toward what:** implement the looper (absent from the core today: no
`looper.hpp`) with capture that records both the absolute stream and, against the
current `ChordState`, the relative form — so the conversion is invertible. Bounded
by the D33 budget (8×3072 ev) and to be built *after* point 1, because that is
where the relative form to convert into lives. High priority, but after (1).

**4. Per-port scheduler → add per-port latency compensation.** — Engineering axis
that *is* musical. arrangrr already has a separate queue and bandwidth model per
port (§9), but no compensation: a chord starting simultaneously on DIN (3125 B/s,
~1 ms per 3-byte message) and on USB arrives as a flam. A late DIN note is a flam,
not a detail. **What → toward what:** a per-port offset (including negative,
look-ahead) in the `OutScheduler`, bounded, integer-tick. The reflections call it
the "Adaptive Bus Scheduler". It fits cleanly into the existing architecture; not
a rewrite.

**5. Section launch quantization: bar/immediate only → next-step/next-beat/
next-bar/end-of-pattern.** — Musical axis. `Arranger::request` applies at the bar
or immediately. The reflections propose the full launch-quantize grid; it is cheap,
deterministic, and changes the *feel* of live section changes. **What → toward
what:** a quantization enum on the pending switch. Minor, but real.

**6. Arpeggiator live-keyboard-only → chord-context-aware and section-aware.** —
Musical axis. `ArpeggiatorEngine` arps the physically held keys; it knows neither
the arranger's `ChordState` nor the section. Reflection 1 wants a "Section-aware
Arp", reflection 2 a context-adaptive arp. **What → toward what:** an alternate
note source from the live chord (the `on_tick`+`EmitFn` shape already supports it),
and a hook into section parameters (rate/octaves via the future Director, D37).
Extension, not rewrite.

**7. MPE input: the message-type exists but is unexercised → real MPE handling,
WITHOUT going to UMP.** — Engineering axis. MPE on input is MIDI 1.0 spread across
channels: it can be absorbed **without touching the 8-byte `Event`**. **What →
toward what:** MPE-aware parsing on ingress (zone + per-note channel), keeping the
POD Event. Distinguish this sharply from the UMP trap (→ Throw away 7).

---

## Throw away

**1. Desktop-first / JUCE GUI / plugin validation (Reflection 1).** — Engineering
axis, contradicts D2 and D7. Reflection 1 proposes "core validated as a desktop
app/plugin, then hardware". arrangrr's arrow is the opposite and locked: STM32
primary, Linux only devenv/sim, headless. Inverting the priority would grow the
design around a JUCE UI that will never run on the target. The reflection yields:
the GUI, if ever, is a client on the protocol — not the application. (See the
Desktop GUI section for what *is* possible constructively.)

**2. AI / RTNeural module for harmonic suggestions.** — Intersection, contradicts
D16/D32/D37. A neural net in a no-heap, determinism-first, 512 KB-budget core is a
category error: non-deterministic, allocating, not falsifiable with goldens.
arrangrr already has the right and *better* answer: the deterministic Generative
Director (D37). The reflection yields. AI here is not ambition, it is noise.

**3. Two-brain SBC-Linux (CM4) + coprocessor-MCU architecture (Reflection 1).** —
Engineering axis, contradicts D2/D33. It is a *different product*, heavier and more
expensive, dissolving all budget discipline onto a single STM32H743. The D33 anchor
(H743, 512 KB envelope) is what makes budgets falsifiable; the coprocessor throws
it away. The reflection yields.

**4. Embedded Lua/DSL scripting in the engine.** — Engineering axis, contradicts
D32 and §6 ("no embedded VM in realtime"). And it is *redundant*: the scriptability
the reflections seek arrangrr already offers better via the L0/L1 text protocol,
replayable and golden-testable. A Lua VM in the realtime path would bring only GC
and non-determinism. The reflection yields.

**5. "≥8 tracks extensible to 64" — the number 64.** — Engineering axis,
contradicts D33. 64 real tracks blow the RAM envelope (8×3072 ev is already
192 KB). It is premium-groovebox feature-creep (OXI/Hapax) imported uncritically.
arrangrr's bounded 8–16 tracks are the *honest* choice for the target. Keep 8–16,
throw away 64.

**6. BLE MIDI / Wi-Fi / Ethernet / Network-MIDI as core concerns.** — Engineering
axis, contradicts §6. A BLE stack (BlueZ/D-Bus) or a network stack in the realtime
perimeter is heap, non-deterministic ~5–6 ms latency, and an enormous surface.
arrangrr already has the right discipline: networking never in the core, at most a
far P2 host adapter. The reflections themselves admit BLE is "not master by default
due to latency" — already a confession. The reflection yields (relegate to a future
adapter, not an architectural concern).

**7. Full MIDI 2.0 / UMP / "neutral" event model NOW.** — Engineering axis,
tension with D33(d). The 8-byte `Event` is a locked budget decision; a UMP-neutral
model (32–128 bit, groups, per-note) bloats it on the target exactly where RAM is
scarce. §6 already says: "leave the abstraction, do not implement". **Sharp
distinction:** MPE-input yes (→ Rework 7, it is MIDI 1.0), UMP/MIDI-CI/
Property-Exchange no-for-now. Keep the abstract boundary, throw away the premature
implementation. The reflection yields.

**8. CV/gate and MIDI-CI/Property-Exchange (JSON over SysEx) as core work.** —
Engineering axis. They do not betray the thesis like AI does (CV is still
deterministic control), but they are HAL/hardware concerns (M13 phase) and
interop-tool concerns, not the music engine. **Do not throw the idea away, relocate
it:** CV/gate as a future HAL adapter; MIDI-CI as a P2 host tool. Outside the
current perimeter.

---

## Verdict

Sound foundations, wrong edges — but the right edges come from the reflections,
and they are few. arrangrr's core (no-heap, dual-target, total-order determinism,
NTT, section manager, headless binary ABI, chord memory) is *more disciplined and
more honest* than nearly everything the two reflections "propose": much of that
document is premium groovebox — JUCE, RTNeural, Lua, CM4+MCU, 64 tracks, BLE, UMP —
which, imported, would dissolve the very lean, embedded-first thesis that makes
arrangrr defensible. The reflections' real value condenses into **three things
alone, all at the intersection**: the relative-pattern model must go from
chord-tone-only to `ScaleDegree`/`RelativeInterval`/`ChordGesture` (the amputation
to heal first), the voicing must go from root-position to voice-leading with
spillover, and the looper — already planned but unwritten — must be built
*reversible* toward that relative form; with per-port latency compensation and
finer launch-quantize as trim. Do not rewrite the core: **extend the data-model
and the resolver**, which is precisely what arrangrr's architecture was designed to
absorb. The reflections do not teach you how the engine is built — you know that
better than they do; they only remind you that a bass that cannot walk is not a
guarantee against wrong notes, it is a sentence never to find the right ones.

---

## Desktop GUI: what we can do

The question is not "GUI as foundation" (that was correctly thrown away above,
D2/D7). The question is: given how arrangrr is actually built, a *well-made*
desktop GUI — what can we really do, and how? Constructive position follows.

### The invariable boundary (non-negotiable)

The GUI is a **client on the existing protocol (L0 JSONL / L1 paths)**, never a
dependency of the core, never on the STM32 target, never in the realtime path. The
core stays the authoritative source of truth; the GUI mirrors state via
`OutEvent`/JSONL (state-diff push) and commands via `Command`/L1. This is not a new
architecture — it is exactly D17a/D23/D26/D30 already in the tree.

What the GUI must **NOT** be able to do:
- It must **not** link the core, include core headers as a build dependency of its
  render loop, or run any core logic in-process on the musical path. It talks the
  wire protocol; that is all it is entitled to.
- It must **not** sit on the realtime/MIDI path. Notes you play live must reach the
  core through the normal MIDI ingress (`push_midi_in`), **not** round-trip through
  the GUI. A GUI in the note path is jitter and a single point of failure.
- It must **not** hold authoritative state. It is a mirror; if it and the core
  disagree, the core wins, always. No "GUI-side truth", no edits that live only in
  the GUI.
- It must **not** become a required component. Killing the GUI must not touch
  playback. The daemon/core runs headless with or without it.
- It must **not** leak into the core's scope creep (no "the GUI needs X so add X to
  the core"). The core's ABI grows for musical reasons, never for pixels.

### The transport (a small optional HOST addition, not a core rewrite)

Today the host speaks JSONL over stdio in the REPL (`jsonl.cpp`, the shell). To
serve a GUI you need one thin host-side adapter that exposes the *same* line
protocol over a socket instead of a pipe. Ranked by effort/fit:

1. **Unix domain socket carrying the existing JSONL, line-oriented** — smallest
   possible step. It is literally the current stdio protocol on a socket: commands
   in as `{"cmd":…}` lines, `OutEvent`→JSONL out. Local-only, no network surface,
   no new serialization. This is the recommended transport for a *local* desktop
   GUI. Reuses `to_jsonl`/`jsonl` verbatim.
2. **WebSocket bridge (UDS → WS shim)** — only if the GUI is a web frontend.
   A ~100-line host shim that forwards the UDS JSONL both ways. Adds a dependency
   to the *host tool*, never to the core. Justified only if you pick the web-UI
   option below.
3. **stdio double-pipe (GUI spawns the daemon as a child)** — zero new transport
   code, but couples GUI lifetime to the process tree; weaker than a socket for
   "GUI dies and reopens". Fine for a demo, not for a tool.

All three are **host-only adapters**. The core is untouched: it already emits POD
`OutEvent` and the host already knows how to render them. This is D26 paying off.

### Technology options, judged on the intersection + cost

- **Extend the existing TUI (`platform/host` panels).** Cost: lowest — it exists,
  it works, it is already the L2 client. Unlocks: nothing *new* in kind, only more
  panels. Risk: near zero. Betrays the thesis: no. Verdict: the correct *default*
  and the thing that already earns its keep — but it is not a "desktop GUI" and
  hits a readability ceiling on 2-D musical structures (piano-roll, timeline,
  routing matrix).
- **Web frontend over WebSocket (the UDS→WS shim).** Cost: medium — a WS shim on
  the host + a JS/TS app; *zero* core cost, *zero* native-toolkit cost, no C++ GUI
  framework in the tree. Unlocks: the richest 2-D musical views (piano-roll with
  degree/role overlays, section timeline, routing matrix) with the least commitment,
  and it dies/reopens trivially (a browser tab). Risk: low-medium (you own a JS
  build). Betrays the thesis: no — it is provably a client, it *cannot* link the
  core, which enforces the boundary by construction. Verdict: **best fit for a
  real GUI** precisely because it is architecturally incapable of contaminating the
  core.
- **Tauri (Rust shell + web view).** Cost: medium — like the web option plus a
  native shell for packaging (AppImage-friendly). Unlocks: same views as web, as a
  desktop app. Risk: medium (extra toolchain). Betrays the thesis: no. Verdict:
  the web option "as an app" if you want a distributable binary; otherwise
  unnecessary.
- **Qt native.** Cost: high — a heavy C++ dependency in the host tree, tempting
  people to link the core "for convenience". Unlocks: strong native widgets. Risk:
  medium-high (the boundary is only a discipline, not enforced by the language gap).
  Betrays the thesis: not inherently, but it *invites* the violation. Verdict: only
  if a native C++ desktop app is a hard product requirement — otherwise the coupling
  risk is not worth it.
- **JUCE app.** Cost: high — a large framework whose gravity is audio/plugin, none
  of which arrangrr wants (D1). Unlocks: nothing the web/Qt options don't, minus
  the audio parts you must not use. Risk: high (framework pulls the design toward
  "make it a plugin", i.e. the thrown-away Reflection-1 inversion). Betrays the
  thesis: yes, by gravity. Verdict: **no.** This is the option to refuse — the same
  refusal as the gap analysis, for the same reason.
- **Electron.** Cost: high (runtime weight) for the same capability as the web/Tauri
  options. Risk: low technically, high in bloat. Verdict: no — Tauri dominates it.

### What a GUI unlocks musically that the TUI struggles to give

- **Live chord strip with confidence/history** — current chord + key + inversion,
  a scrolling history, and a confidence indicator. Note: the `ChordDetector`
  currently returns a bool, not a score; a real confidence lane is a small
  core-side addition (low priority) that a GUI makes worth surfacing.
- **Section timeline** — a horizontal arrangement view (intro/A/B/fill/break/
  ending, launch-quantize state, bars remaining). The TUI can list it; a GUI makes
  the *shape* legible at a glance.
- **Relative-pattern editor (the real prize).** This is where the GUI earns its
  existence: a piano-roll with an **overlay of the new degree/role types** from
  Rework 1 (`ScaleDegree`/`ChordRole`/`RelativeInterval`/`ChordGesture`). Editing a
  relative pattern as text or step-grid is exactly the thing the TUI does badly and
  a 2-D view does brilliantly — you *see* "root, walk up to the 5, approach the
  next root" instead of decoding indices.
- **Routing matrix** — role → port/channel as a grid with mute/solo/voice. A matrix
  is a 2-D object; the TUI flattens it, a GUI shows it.
- **Voicing view** — the resolved voicing per chord (from Rework 2), the spillover
  voice-leading drawn as motion. Makes the voicing engine's decisions inspectable.

What the TUI already does well and is **not** worth duplicating: the live
performance surface (CTRL+SPACE chooser, CTRL+P play/stop, the parts mixer, the
mode-aware help), the MIDI monitor, the piano panel, and the terse L2 command REPL.
Those are keyboard-first, low-latency, and already good; a GUI that re-implements
them adds surface for no gain. Split the work: **GUI for 2-D structure and
editing/inspection; TUI for live keyboard-first control.**

### State synchronization (already free, given `OutEvent`/JSONL)

The core is authoritative; the GUI is a mirror reconstructed from the event stream.
On connect, the GUI requests a state dump (D17d `state dump/inspect`) and then
applies the incremental `OutEvent`/JSONL stream as diffs. Because the core owns all
state and emits every change as an event, the GUI can **die and reopen without
touching playback** — it re-dumps and re-subscribes. No shared mutable state, no
lock, no GUI-side authority. This is D16/D17 cashing out: the same replayable event
log that feeds the golden harness feeds the GUI mirror. One protocol, three
consumers (goldens, REPL, GUI).

### Honest cost/value

For **editing and inspection**: real value, not a luxury. The relative-pattern
editor with degree/role overlays (Rework 1) is materially harder to do well in a
TUI, and it is the feature that makes the arranger's harmonic model *authorable* by
someone who is not you. Value: high, for style authors and for your own content
pipeline; medium for a pure live player.

For **live playing**: a luxury, and a slightly dangerous one — anything on the note
path must stay off the GUI. A GUI is a control/inspection surface, not an
instrument. Do not let "GUI for live" pull the design toward round-tripping notes.

Recommendation, **ranked**:
1. **Keep extending the TUI** for live/keyboard-first control — lowest cost,
   already earning it.
2. **Add the UDS-JSONL host adapter** (small, optional, reuses `to_jsonl`) so *any*
   external client can attach. This is the enabling step and is worth doing
   regardless of which GUI you pick.
3. **Build the GUI as a web/Tauri client over that socket**, focused on the 2-D
   structural views — above all the relative-pattern editor tied to Rework 1.
4. **Refuse JUCE/plugin/native-linking-the-core** — same refusal, same reason as
   the gap analysis: it inverts D2 and invites the core contamination the whole
   architecture was built to prevent.

The GUI is worth building *if and only if* it stays what arrangrr's architecture
already says a client must be: a mirror that reflects and a keyboard that commands,
never a truth and never a note in the timing path.
