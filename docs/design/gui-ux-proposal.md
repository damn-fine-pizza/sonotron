# arrangrr Host GUI (node 11600) — UX & Product Proposal

Status: **Phase 1 direction VALIDATED by the owner (2026-07-07).** Decisions locked below in §9.
No GUI code is written until the toolkit's concrete backend dependencies are approved (Phase 2).

Synthesised from `puccini-product-critic` (personas/value), `verdi-roadmap-strategist` (product
arc/scope), and the Phase-0 orientation in `docs/design/gui-contract-map.md`. Companion docs:
`docs/design/chord-following.md`, `docs/TUI_SPEC.md`, `docs/DESIGN.md` §22 (nodes 11500/11600/
11700), `docs/product-identity.md`, `docs/reflections/hybrid-arranger-gap-analysis.md`.

---

## 0. The identity that decides everything

The GUI is **a mirror that reflects and a keyboard that commands — never a truth and never a
note in the timing path.** It is a separate desktop process that never links the core; it sends
plain text L1 lines over a UDS socket and reads JSONL events back. The musical gesture — playing
the chord the band follows — enters through the MIDI keyboard / computer-keyboard straight into
the core via `feed_midi`; **it does not pass through the GUI.** The GUI is the music stand, the
mixer, and the conductor's overhead view — never the instrument itself.

**The golden layout rule that follows:** *the GUI commands what you set with your eyes (style,
section, mixer, groove, key) and reflects what you play with your hands (harmony, section,
playhead). It never tries to be the hands.* Put the mirror (the harmony surface) at the centre,
never a clickable chord-pad.

---

## 1. Personas & Jobs-to-be-Done

Ranked by who the FIRST GUI is for.

### A — The live arranger-keyboard player, performing solo *(PRIMARY / the WOW target)*
Wedding/piano-bar/resident-gig keyboardist. Today plays a Yamaha Genos or Korg Pa (€4–6k). Lives
on Single-Finger, fills, and variations; *is* the band's chord track, live, with the left hand.
- **Job:** *"Let me be a whole band with two hands, in front of real people, without ever
  staring at a screen while I play."*
- **Success in the hands:** plays a C one-finger → the band lands on it *within the bar* and
  **holds** it; a right-hand solo has no wrong notes; a fill before the chorus attacks quantized
  to the bar. Success is kinaesthetic — he *feels* it, doesn't watch it.
- **Why a desktop GUI (not the TUI, not hardware):** for HIM the GUI is the biggest risk — live,
  he looks at nothing. The GUI serves him **before and around** the gig: prepare the set, glance
  once to confirm the section, a music-stand-sized harmony readout legible at 2 metres (which the
  dense text TUI cannot give). The GUI beats the TUI **only** on glanceable legibility and 2-D
  structure, **never** on live command speed — the keyboard-first TUI already wins there. It
  beats hardware on determinism, reproducibility, and weight (a laptop, free of a €6k board).

### B — The bedroom producer sketching a song over an auto-band *(SECONDARY / depth & retention)*
Makes beats/pop/lo-fi in a DAW; not a virtuoso — one-finger chords, hunting the idea. Today uses
Scaler 2 (inside the DAW) or ChordPulse.
- **Job:** *"Give me a progression that sounds like a real band in thirty seconds, so I can hear
  if the idea holds before I open the DAW."*
- **Success in the hands:** loads `funk`, one-fingers `Am F C G`, hears a credible groove, mutes
  the pad, raises the swing, has a mood in two minutes — then **locks the seed** and finds it
  byte-identical next week (arrangrr's edge over Scaler's static chord-grid).
- **Why a desktop GUI:** for him it IS the primary surface (mouse + computer-keyboard, no MIDI
  hardware required). He wants to *see* the parts mixer, style, and groove together, clickable.

### C — The performer building/rehearsing a set *(REAL but NOT v1-serviceable)*
Cover band / solo looper / worship leader; must prep 15 songs with different styles/sections/
tempos/keys and recall them without panic. The set is intrinsically 2-D — exactly what the TUI
does worst, so the GUI is the right home for it.
- **Honest limit:** song-mode/recall (node 8000) is **behind the freeze line, not built.** In v1
  this persona is served only by manual `style load` / `bpm` / `key` changes. A **future
  retention** persona, not a launch persona.

### D — The beginner *(plausible market, not a served job)*
The "no wrong notes" claim seems made for him, but without song-mode / guided progressions /
didactic feedback he gets only "a backing that follows your finger." A market, not a job yet.
Do not build the WOW on him.

**Explicitly NOT a persona:** the orchestral/linear-arrangement composer. That is DAW gravity,
rejected by construction (`product-identity.md`). Promising it would be a lie.

---

## 2. Use cases

1. **The solo piano-bar (A).** Saturday night. Loads `bossa`, 120 BPM, key F. Steers chords with
   the left hand, improvises with the right, raises a fill into the chorus (`varB`). Glances at
   the GUI **once** to confirm the section. The GUI is the stand, not the instrument.
2. **The 90-second sketch (B).** Midnight idea. `funk`, one-fingers `Dm7 G7 Cmaj7`, mutes `pad`
   and `phrase`, swing to 40, feels the groove breathe. Locks the seed; reopens it identical.
3. **The set rehearsal (C, v1-honest).** Before the gig, preps three songs by hand-changing
   `style load` / `bpm` / `key`, noting the settings. Clumsy without recall — but it works. The
   full one-touch-recall case arrives with node 8000.
4. **The group jam (A/C).** The keyboardist is band-in-a-box for a drummer + singer in rehearsal:
   steers chords live, the MIDI band fills bass + comping. `kLivePriority` matters — while a live
   chord is HELD, his hand beats the sequencer; on release the sequencer resumes.
5. **The case we KILL.** "Mixing/automating audio tracks in the GUI." Does not exist: no audio in
   the core, no mixer, no linear timeline. Whoever asks wants Ableton.

---

## 3. Primary flows (each step → exact ABI command / event)

Grounded in `docs/design/gui-contract-map.md`.

### 3.1 The FIRST-WOW loop — and the position on the P0 gap

| # | Player gesture | GUI→core command | core→GUI event |
|---|---|---|---|
| 1 | Pick a style | `style load 4` → `kStyleLoad` | **none** — no ack on load-while-stopped. GUI reflects its own optimistic state. |
| 2 | Start transport | `transport start` → `kTransportStart` | `{"ev":"transport","state":"playing"}` **may not fire** on explicit start; the honest "it's playing" signal is the **onset of the `midi-out` stream**. |
| 3 | Play a one-finger chord | (hardware / computer-piano → `feed_midi`, **not** the GUI) — or `chord play C` → `kChordPlay`; `chord mode single` → `kChordMode` | the `midi-out` stream re-roots (band follows) |
| 4 | Band follows & HOLDS | — | **THE GAP.** The only event that says "the band now follows Cmaj7" would be `kChordFollowed`. It **does not exist.** The existing `kChord` fires **only** from the recorded sequencer, never from `chord play` nor live detection. |
| 5 | Read the harmony | — | GREEN = followed **this bar**, AMBER = staged for **next bar** |

> **Prospero refinement — the third option both critics missed.** The choice is not only
> (a) build `kChordFollowed` or (b) infer from `midi-out`. There is a third path: for a player
> **authoring harmony through the GUI** in **fingered/literal** mode, GREEN = "the chord I just
> sent with `chord play`" and AMBER = "the chord I staged with the SHIFT variant" are honest
> **command-optimistic facts** — not inference from sounded bytes — the *same* optimistic
> discipline step 1 already uses for `style load`. So a first WOW slice ships at **zero ABI
> cost**. The caveat that keeps the proposal RIGHT: in **single-finger** mode the scale-aware
> resolution lives in the core, so echoing the resolved NAME would be the forbidden client-side
> recompute — *there* `kChordFollowed` is the only honest wire. **Plan:** ship the optimistic
> fingered-mode slice now; let `kChordFollowed` later PROMOTE the display from "commanded X" to
> "band follows X" and unblock the live-hardware (Persona A) path. Visually distinguish "you
> asked X" from "the band follows X" — the same observed-vs-labelled discipline already invented
> for the lit keys, applied to the command.

**Position (Puccini + Verdi agree, strongly): the harmony readout MUST be driven by a real core
event, NOT inferred from the `midi-out` stream.** Three reasons:
1. Inference is exactly the *"probably a chord"* guessing the project forbids (`TUI_SPEC.md`:
   "honestly report what is observed. No inferred state"). The band plays spread voicings, passing
   tones, NTT chord-tones across parts — reconstructing "the followed chord" from sounded bytes is
   lossy and wrong precisely in the moments that matter (tensions, arp, voicing spread). A stand
   that says "Cmaj7" then "C6/9" for the same chord loses the player's trust on night one.
2. **AMBER is by construction non-inferable** — "pending / next bar" is a chord that *has not
   sounded yet*; there is no `midi-out` byte to deduce it from. The staged-chord semantics (the
   SHIFT variant of steering that queues to the next bar) — the single most original, valuable
   readout — is impossible without `kChordFollowed{current, pending, valid, source}`.
3. Today the *only* faithful harmony on the wire is the recorded sequencer (`kChord`), while live
   steering — *the whole point of the product* — arrives mute. That inversion alone motivates the
   fix as the first unblock.

**What v1 CAN do honestly without the core event:** light the piano keys from `midi-out` as
*"here is what the band is sounding now"* — true, observed, honest — but **never** label it "the
followed chord is X." The chord name and the green/amber semantics stay dark until `kChordFollowed`
ships. A partial honest surface beats a lying one.

**Second P0 — the transport heartbeat (gap #2).** No playhead/beat-count exists on the wire. A
live instrument needs it, because the moment the player *waits for* is "when does the amber chord
promote to the next bar." Propose additive `kBeat`/`kPosition`, or extend `kTransport` to fire on
start/stop/continue. Both P0s are **additive-ABI** (the freeze is additive-only) — they do not
break v1, but they touch the core, so they are **owner decisions** (see §7).

### 3.2 Shape the feel (observable today, or nearly)

| Gesture | Command | Event |
|---|---|---|
| Change section | `style section fillB` → `kStyleSection` (quantized to next bar) | `{"ev":"section","name":"fillB"}` → `kSection` **fires reliably** on change. Already solid. |
| Combined style+section | `style switch funk varB 0` → `kStyleSwitch` | `kSection` on change |
| Swing / humanize / accent | `groove swing 40`, `groove humanize_timing 20`, `groove accent 30`, `groove swing_grid 16` → `kGroove` | no ack; effect **heard** in `midi-out` timing; no readback (gap #3) |
| Mute / solo parts | `part mute bass 1`, `part solo drums 1` → `kPartMute`/`kPartSolo` (roles: drums,perc,bass,chord1,chord2,pad,arp,phrase) | those parts vanish/return in `midi-out`; no state readback (gap #3) |

`section` is the one structural feedback already faithful — build the section breadcrumb on it now.

### 3.3 Loop / song-mode / arp (sketch only)
- **Arp:** `arp enabled 1`, `arp rate 1/16`, `arp direction updown`, `arp octaves 2`, `arp gate
  60`, `arp latch 1` → `kArp`; `arp out 0:1` → `kArpOut`. Output sounds via `midi-out`.
- **Chord sequences / song:** `seq new/rec/add/loop/play/stop/transpose` → `kSeqNew..`. Secondary
  for the MVP; note it is the *only* path that emits `kChord` today.
- **Looper (node 6000):** not built, behind the freeze line. **Reserve UI space, wire nothing** —
  same discipline as the MIDI-FX chain (shape-reserved, no live ids).

---

## 4. Screen / panel inventory & proposed layout

Tiered by proximity to the playing hand. **Closer to the hand ⇒ bigger and more central.**

**Tier 0 — THE STAGE (large, central, live-critical, continuous refresh):**
1. **Green/amber harmony readout** — the identity surface. **BLOCKED on P0 #1 (`kChordFollowed`).**
   Without the core event it is honest only as "notes sounding now," missing the name and amber.
2. **Current section + breadcrumb** — feedable from `kSection` **today**. Build it now (free).
3. **Playhead / beat** — **BLOCKED on P0 #2**. The live pulse.
4. **Transport (play/stop, BPM)** — always in reach, always visible.

**Tier 1 — HANDS ON THE FEEL (one gesture away, not central):**
5. Style selector (the 16 builtins now genuinely differ, node 9100 ✅).
6. Parts mixer (mute/solo per role) — clickable, immediate.
7. Groove/swing sliders — live.
8. Section/fill triggers (`varA..D`, `fillA..D`, quantized).

**Tier 2 — CONFIG (set, then forgotten):** key/scale, port routing / GM programs, arp, chord
mode/detect on-off.

**Tier 3 — RESERVED SPACE, ZERO WIRING (born aware, wire nothing):** looper (6000), song-mode/
recall (8000), MIDI-FX chain (5100, shape-reserved). Reserve the slot so the GUI is born aware,
exactly as the ABI already did. Do not pretend they do anything.

### Proposed layout (ASCII wireframe — indicative, not final)

```
┌──────────────────────────────────────────────────────────────────────────┐
│  ● FUNK   ▸ Var B          key: F maj        ⏵ 124 BPM         ● playing   │  ← transport strip (Tier 0.4 + 0.2)
├──────────────────────────────────────────────────────────────────────────┤
│                                                                            │
│                    ┌────────────────────────────────┐                      │
│                    │   NOW:  C maj7        (this bar)│   ← green            │
│   THE STAGE        │   NEXT: A min7      (next bar)  │   ← amber            │  ← harmony readout (Tier 0.1)
│                    └────────────────────────────────┘                      │
│   ▐▐ ▌ ▐▐▐ ▌ ▐▐ ▌ ▐▐▐ ▌   ← piano/keys lit: green=followed, amber=pending  │
│                                                                            │
│   Intro▸[VarA][VarB*][VarC][VarD]  fill:(A)(B)(C)(D)   ◀ bar 3 ▸▏▏▏▏·····   │  ← section strip + playhead (Tier 0.2/0.3)
├───────────────────────────────┬────────────────────────────────────────────┤
│  PARTS            M  S  ▮act   │  GROOVE                                     │
│  drums            ·  ·  ▮▮▮    │  swing        [####------]  40%             │
│  bass             M  ·  ·      │  humanize-t   [##--------]  20%             │  ← Tier 1 (mixer + groove)
│  chord1           ·  S  ▮▮     │  accent       [###-------]  30%             │
│  pad              M  ·  ·      │  swing-grid   1/16                          │
│  …                             │  [ r reseed ]                              │
├───────────────────────────────┴────────────────────────────────────────────┤
│  style: [basic][pop][rock][ballad][FUNK*][disco][house][swing][bossa]…      │  ← Tier 1 style selector
├──────────────────────────────────────────────────────────────────────────┤
│  key/scale · routing · arp · chord mode        [ reserved: looper · song ]  │  ← Tier 2 config / Tier 3 reserved
└──────────────────────────────────────────────────────────────────────────┘
```

The stage (harmony NOW/NEXT + lit keys + section/playhead) dominates the top half. Feel controls
sit beneath in reach; config and reserved space are marginal. Nothing clickable competes with the
hands for the centre.

---

## 5. Interaction model

Mouse + computer-keyboard + live MIDI, together — the keyboard/MIDI path stays first-class.
- **Live steering** enters via MIDI hardware or the computer-piano keys, straight to the core —
  **never** through a GUI widget in the timing path. `chord play` by mouse exists as fallback/demo
  only; it is never the headline. If a clickable chord-pad ever sits at the centre, the design has
  already failed.
- **Keep/evolve the TUI conventions**, with the *shipped* bindings (not the older docs' names):
  transport play/stop is **CTRL+P**; the style/section chooser is **backtick** `` ` `` (the docs'
  "CTRL+SPACE chooser" is stale — CTRL+SPACE is the piano momentary↔toggle switch). Mirror these
  in the GUI (a global play/stop, a quick style/section chooser) rather than inventing new ones.
- **Glanceable, not fiddly:** the stage is readable at 2 metres; feel controls are direct-
  manipulation (click a mute, drag a slider); config is tucked away.
- **Immediate-mode discipline:** the core wins every frame; the GUI holds no authoritative state.
  On reconnect it shows optimistic state until (future) snapshot-on-connect exists (gap #3).

---

## 6. What to mirror vs. NOT duplicate (scope discipline)

| | Element | Verdict |
|---|---|---|
| **IN (v1)** | UDS connect via `--control`; one real command round-trip | pure client, no new core dep |
| **IN (v1)** | Big legible harmony surface (NOW/NEXT, bold-green/amber) | **blocked on `kChordFollowed`** |
| **IN (v1)** | Playhead / beat | **blocked on transport heartbeat** |
| **IN (v1)** | Section strip (current/next) | zero ABI gap — `kSection` exists |
| **IN (v1, 2nd slice)** | Style selector as a second real write | safe, no gap |
| **OUT (defer)** | Parts mixer as true mirror | limited by gap #3 — shippable only as "attach-blind" |
| **OUT** | MIDI-FX panel | shape-reserved (5100), reserve space, wire nothing |
| **OUT** | Relative-pattern / piano-roll editor | needs a core node that does not exist yet (see §7 orphan) |
| **OUT** | Scene / song mode | 8000/10000 not built — nothing to mirror |
| **NEVER** | Linear audio timeline, mixer, plugin host | `product-identity.md`, categorical |
| **NEVER** | A live MIDI note routed through the GUI process | hard rule, three docs agree |
| **NEVER** | GUI-side authoritative state / client-side chord recompute | collapses the whole arc |
| **NEVER** | Window-close wired to a bare `quit` line | kills the host for every client |

---

## 7. Flagged for the owner (decisions that are yours, not mine)

1. **[NEEDS-DECISION · core · additive-ABI] P0 #1 — `kChordFollowed{current, pending, valid,
   source}`.** Additive to the frozen v1 ABI (allowed — freeze is additive-only), but it touches
   the core. It is the hard dependency of the central harmony surface. **Recommendation: do it
   first.** Without it the GUI has no honest centre. **Corelli seam correction (supersedes the
   earlier "one emit point in `FollowedContext::commit`"):** `FollowedContext` is a pure,
   dependency-free value type and MUST stay so — the emit does NOT go inside it. It belongs in
   `engine.hpp` (where `EventSink sink` is already in scope) at **four** call sites — `chord_play`
   (`kManual`), `fire_chord_seq` (`kSequencer`), `observe_chord_input` (`kDetect` — its signature
   must grow a `sink` param), and the `commit_bar` bar-promote — funnelled through a single private
   `Engine::emit_chord_followed(Producer, EventSink)` helper so the four sites cannot diverge. The
   bar-promote also needs a small additive field `Producer m_pending_source` in `FollowedContext`
   (still no-heap) so `source` is honest on promote. `OutEvent`'s 16 bytes hold current+pending+
   valid+source comfortably (~20 bits of 40 available), packed like the existing `kChord`. Pinned
   in `test_abi_frozen` + a golden, rendered in `jsonl.cpp`. Still additive — no existing id touched.
2. **[NEEDS-DECISION · core · additive-ABI] P1 — transport heartbeat** (`kBeat`/`kPosition`, or
   `kTransport` firing on start/stop/continue). Same additive nature. The live pulse of the
   playhead. **Prospero downgrade: this is P1, not P0** — `kSection` already gives the structural
   glance for free, and amber promotion is bar-quantized and *felt*; the beat-pulse is honest
   polish, chiefly for the persona who actually watches the screen. Do not spend equal owner-
   urgency on both: `kChordFollowed` is the backbone, the heartbeat is refinement. **Ordering:**
   ship the zero-ABI optimistic slice first (see §3.1 Prospero refinement); `kChordFollowed`
   is the first additive event; the heartbeat follows.
3. **[NEEDS-DECISION · minor] Gap #3 — snapshot-on-connect / readback.** All Params are write-only,
   `Op::kGet` unwired. Without it a GUI attaching mid-session shows optimistic state. Honest to
   defer for v1; decide if you want it true. Recommendation: accept the "attach-blind" limit to
   keep the first slice thin.
4. **[PRODUCT FORK — the headline decision] Primary persona for the FIRST GUI.** The initial
   draft picked Persona A (live keyboardist) as the WOW target. **Prospero's sharpest finding
   flips this:** the draft names as WOW target the persona who, by its own admission, *does not
   look at the screen during the act* ("his success is kinaesthetic — he feels it, doesn't watch
   it"). A screen structurally serves **B (the producer)** — B watches continuously, B authors
   the harmony *through* the GUI, B is the one for whom "the GUI is the primary surface" is
   literally true; and the node-11700 thesis ("validate feel in the hands") needs a persona who
   *plays AND watches* — B's posture, not A's eyes-closed one. **Recommendation: make B the
   primary inhabitant of the first GUI; keep A as the product's SOUL (the live finger-steering is
   the identity) but not the audience of the music-stand.** This also de-risks scheduling (B's
   WOW ships at zero ABI cost via the optimistic slice). Consequence if adopted: the layout
   promotes parts-mixer / groove / reseed to first-class (they ARE B's hands on a mouse-driven
   screen), keeps the harmony surface prominent, keeps the no-central-chord-pad ban. **Your
   call** — it decides where the screen's weight goes.
5. **[TOOLKIT — already decided in D38, RECONCILE not re-open] Phase 2 framing.** The brief asks
   for a fresh 3–4-toolkit bake-off, but **D38 (commit `a092e21`) already LOCKED Dear ImGui** as
   the client — *"Rejected JUCE/web/Tauri/Qt with rationale"* — reconfirmed in `product-identity.md`
   and two reflections. §22 text still says the toolkit is "OPEN / not picked here" in three
   places — that text is stale. **Question for you:** is Phase 2 a genuine re-decision (D38
   reopened), or is ImGui settled and Phase 2 collapses to "confirm ImGui + name the concrete
   window/render backend deps (SDL2 or GLFW + OpenGL3) for `0800` approval"? Either way the §22
   stale text should be reconciled to D38 on the next merge.
6. **[ROADMAP ORPHAN — independent of the GUI] `ScaleDegree`/`RelativeInterval` style-data
   extension** (the reflections' "GUI's real prize" — the relative-pattern editor) has **no node
   number in §22**. It needs one (likely under 3300) regardless of when the GUI builds its editor,
   else it is a verdict with no home in the tree.

---

## 8. Prospero verdict (direction stress-test) — what changed

Prospero judged the direction **keep / rework / throw** on the engineering and musical axes.

- **KEEP (sound foundations):** the pure-client / immediate-mode / never-in-the-timing-path
  architecture (rated "genuinely excellent on both axes" — the core wins every frame, the same
  technical choice that protects the musical identity: hands stay the instrument, screen stays the
  stand); `kChordFollowed` as the only honest source of the *followed* chord for the single-finger
  signature gesture (survives the stress-test — that resolution lives in the core, the client must
  not recompute it); the observed-vs-labelled discipline; the whole NEVER list; the no-central-
  chord-pad rule; the D38/toolkit reconciliation flag; reserved-space-zero-wiring.
- **REWORK (applied above):** the persona inversion → §7.4 (make B primary for the first GUI);
  the "surface is BLOCKED until `kChordFollowed`" framing → §3.1 (ship the zero-ABI optimistic
  fingered-mode slice now); the layout "closer-to-hand" rule applied to A's MIDI hand instead of
  B's mouse → promote mixer/groove to first-class; the two P0s as equal urgency → §7.2 (heartbeat
  is P1).
- **THROW (the *justification*, not the pixel):** the "2-metre music-stand for the live performer"
  rationale for a large central stage. The draft itself says A doesn't watch during the act, so
  sizing the whole layout around a reader who isn't reading is toxic to every downstream sizing
  decision. **The harmony readout stays** — but re-justified as **B's close-up sketching aid**
  (the producer authoring and watching at 40 cm), not A's distant performance stand. The §4
  wireframe's "THE STAGE / legible at 2 metres" language is superseded by this: same surface,
  honest reason.

**Verdetto (paraphrase):** sound foundations, one crooked edge, and a crack running under the
part the draft calls "the heart." Make B the first inhabitant of the GUI, keep A the soul (not
the stand's audience), ship the optimistic slice now and let `kChordFollowed` promote it from
"commanded" to "followed" — and the direction moves from elegant theatre to living instrument.

---

## 10. B-CENTERED USE CASES & STORYBOARDS (supersedes the A-first framing in §2/§3)

Authored by the orchestrator (pen-holder on this doc) after the primary-persona flip to B.
For OWNER VALIDATION, then a musical-value verdict from puccini-product-critic.

### 10.1 Use cases, B-first
1. **PRIMARY — the midnight sketch-improviser (B).** No MIDI hardware. Opens the GUI, loads a
   style, hits Play, steers chords by clicking chord buttons / computer-keys, WATCHES green/amber,
   queues the next chord and watches it land, improvises a melody on the computer keyboard on top.
   The GUI window IS the playing surface.
2. **The feel-sculptor (B).** Rides swing, mutes/solos parts, swaps sections/fills — all by mouse,
   live, while it plays.
3. **The A/B auditioner (B).** Locks a groove seed, auditions styles over the same steering,
   compares the feel, keeps the winner.
- Secondary: **A the piano-bar** (plays MIDI hardware; GUI is a glance-only confirm of section/
  chord — A never drives through the GUI). **C the set-prep** (manual `style load`/`bpm`/`key`;
  no recall until node 8000). Kill-list unchanged (no audio/DAW).

### 10.2 Storyboard — B's first WOW (the amber→green "feel it land" loop)
Row = [screen state] → [B's action] → [command sent] → [event & screen update].

| # | Screen | B does | Command | Event / update |
|---|---|---|---|---|
| 1 | "disconnected" | app connects to `--control` socket | (connect) | status → "connected", empty stage |
| 2 | 16 style names, none active | click **funk** | `style load funk` | none — funk highlights optimistically |
| 3 | ▶ stopped | click **Play** | `transport start` | `midi-out` begins; transport → "playing"; section strip shows current section (`section` event) |
| 4 | stage empty (green off at rest) | click chord **C** (or press a computer-key) | `chord play C` | **`chord-followed cur:Cmaj7 src:manual`** → piano lights **GREEN** on C·E·G·B; readout "NOW: Cmaj7" |
| 5 | NOW: Cmaj7 (green) | **SHIFT**-click **A** (queue next bar) | `chord play A` (staged) | `chord-followed cur:Cmaj7 next:Amin7` → **AMBER** on A·C·E; readout "NEXT: Amin7" |
| 6 | watching amber | (waits for the bar) | — | at the bar: `chord-followed cur:Amin7 next:-` → amber **PROMOTES to green**; the "feel it land" moment |
| 7 | band on Amin7 | improvise a melody on computer-keys | note-ons (melody port) | `midi-out`; band follows the held harmony |

Real-in-v1: steps 1–7 all work on the wire **now** — the `chord-followed` event just landed in the
core (verified). Aspirational flag: step 6's smooth *countdown* to the promotion wants the deferred
**transport heartbeat (P1)**; without it the promotion still HAPPENS (driven by the event at the
bar) but there is no ticking playhead animating toward it.

### 10.3 Storyboard — B sculpts the feel
| Screen | B does | Command | Event / update |
|---|---|---|---|
| section strip: Var A | click **Fill B** | `style section fillB` | `section` event → strip shows fill, returns to a variation after |
| swing slider at 0 | drag to **40** | `groove swing 40` | heard in `midi-out` timing — **flag: no value readback** (gap #3) |
| mixer: pad on, drums on | click **M** on pad, **S** on drums | `part mute pad 1`, `part solo drums 1` | those parts drop/isolate in `midi-out` — **flag: no state readback** (gap #3) |

### 10.4 Storyboard — B auditions (seed / style A/B)
| Screen | B does | Command | Event / update |
|---|---|---|---|
| groove panel | set a seed | `groove seed 7` | deterministic feel locked |
| style row: funk active | switch to **disco** over the same steering | `style load disco` | compare the feel by ear |
Flag: seed-lock / current-values readback and **snapshot-on-connect are NOT on the wire** (gap #3) —
auditioning works forward, but a GUI reattaching mid-session cannot re-sync its shown state.

### 10.5 Secondary storyboard — A (glance-only, still works)
A plays a MIDI keyboard into the core directly; the GUI mirrors NOW/NEXT + section as a 2-metre
confirm. A never steers through a GUI widget. Proves the pure-client mirror serves A without
pretending to be A's hands.

---

## 9. OWNER DECISIONS — locked 2026-07-07

1. **Primary persona for the first GUI = B (the producer) — EXTENDED to the experimental /
   improvising performer** who uses the GUI as an *active surface while playing* (watches and
   interacts, unlike Persona A the wedding-gig arranger who never looks at the screen). Persona A
   (live finger-steering) remains the product's **soul**, not the audience of the music-stand.
   Layout consequence: parts-mixer / groove / reseed are first-class citizens (they are B's hands
   on a mouse-driven screen); the harmony surface stays prominent and re-justified as B's close-up
   sketching/improvising aid (≈40 cm), not A's 2-metre performance stand; the no-central-chord-pad
   ban stands.
2. **ABI: the "real fix" is approved — additive `kChordFollowed` event.** The core will announce
   the followed chord on every change, from any source (MIDI hardware / single-finger / sequencer),
   so GREEN/AMBER is always truthful, including single-finger. This is a small, **dependency-free
   CORE** change, additive-only (does not break the frozen v1 — it is the next free
   `OutEvent::Kind`), and is the prerequisite of the central surface. The owner chose this over the
   zero-ABI optimistic-only slice: the GUI's centre is built on core truth from the start, not on
   command-optimism. **Transport heartbeat (playhead) = P1, DEFERRED** — not selected; the first
   GUI ships with a section strip (`kSection`, free) but no moving playhead until the owner
   greenlights the heartbeat event later.
3. **Toolkit = Dear ImGui, settled by D38 — Phase 2 is a reconciliation, not a bake-off.** Phase 2
   collapses to: confirm ImGui + name the concrete window/render backend dependency set (e.g. SDL2
   or GLFW + OpenGL3, or hello_imgui) for policy-0800 approval, and reconcile the stale §22
   "toolkit is OPEN" text to D38 on the next merge. No fresh 3–4-toolkit evaluation.
```
