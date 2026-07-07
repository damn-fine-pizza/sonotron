# UX/UI concept — the intention conductor

Status: **direction chosen 2026-07-07 → Console / dashboard on Dear ImGui.** Experience-level, not
widget spec. Companions: `workstation-vision.md` (what it is/does), `flows.md` (flow #3 detailed).
**Supersedes** `gui-ux-proposal.md` (the old panel-mirror framing, pre product-reframe).

## Posture

You are a **conductor** in front of a living band, NOT an **editor** in front of a timeline. The
screen is a **dashboard that breathes** — it shows what the music is doing *now* and lets you bend it
live. Three laws, always:
- **everything is observable** — each part shows its true state (via hooks), never guessed;
- **everything is overridable and YOU win** — touch anything → your hand beats the Director;
- **everything is hand-operable** — the whole UI is drivable by hand, *everywhere*. Nothing is
  machine-only. The machine's help — the Director, and the propose/adjust/add copilot — is always
  **optional and on top, never a gate** you must pass through. Manual is a first-class path to every
  action, always.

## Primary screen

```
┌─────────────────────────────────────────────────────────────┐
│ ●▶  ♩=120   key: Cm    SEED 🔒 #a3f9      Scene: “Build”      │ ← transport · seed · scene
├───────────────────────────────┬─────────────────────────────┤
│ INTENTION (the podium)        │ BAND (live)                 │
│  energy  ▓▓▓▓▓░░░ → ▓▓▓▓▓▓▓▓  │  Drums ▓▓▓▓  busy           │
│  tension ▓▓░░░░░░ → ▓▓▓▓▓░░░  │  Bass  ▓▓░░  walking        │
│  valence ◑ neutral → ☀ bright │  Keys  ▓▓▓░  comping   [M]  │
│                               │  Pad   ░░░░  (enters → 4)   │ ← “ghost” = the Director’s
│  ▸ rising…  bar 3 / 8         │  Arp   ░░░░  (off → on)     │   next move, before it lands
├───────────────────────────────┴─────────────────────────────┤
│ HARMONY  now Cm7  ▸ [Fm7] amber   │ STRUCTURE  Intro·[A]·B·Fill│
└─────────────────────────────────────────────────────────────┘
```

## Zones
- **Intention (the podium)** — the heart: energy/tension/valence as *current → target*, with the
  trajectory advancing ("rising, bar 3/8"). Flow #3 made screen.
- **Band (live)** — each part is a **living lane** showing what it does *now* (activity, density,
  mute/solo), NOT a row of clips on a timeline. The Director's upcoming moves show as **ghosts**
  ("Pad enters → 4"): you see the machine's intent *before* it lands.
- **Harmony** — chord now + next queued (**amber→green**), key.
- **Structure** — where you are in the song-form; sections/fills at hand; the trajectory captured
  as a **Scene**.
- **Transport + seed** — play/tempo and the **seed lock**: one visible control that makes the whole
  thing reproducible.

## Interaction model
- **Observe:** every lane is live truth; ghosts show the machine's next move.
- **Override & win:** touch a lane (mute, force a part, change a chord, move a section) → you beat
  the Director, which *re-proposes* from where you now are. What you took by hand stays marked "yours".
- **Musical time:** changes land on bar boundaries; queued ones show a countdown ("→4").

## Toolkit implication (resolves an open fork)
- **Dear ImGui confirmed for the GUI** (consistent with D38). The spatial / game-engine (Godot)
  direction is **parked** — revisit only for a future immersive-WOW pass.
- Because audio is a **separate peer engine** (the GUI never does audio), the **GUI-toolkit choice
  (ImGui)** and the **audio/VST-hosting library choice** are now **decoupled** decisions. Picking
  ImGui does NOT force the audio-hosting lib. The old "ImGui vs JUCE" tension dissolves: ImGui for
  the GUI; audio hosting is separate outer-product work for later.

## The sequencer surface (hand-author + on-demand copilot) — flow #10

A second surface, reached from a Band lane: the **piano-roll / step editor** where you write notes
by hand. Same two laws (you hold the pen; the copilot is **pull-only**, silent until asked).

```
┌ Part: Bass ── key Cm ── sel: bars 5–8 ──────────────────────┐
│  C ─────────────────────────────────────────────────        │
│  A ──────█████──────────────────░░░░░ ← ghost (proposed)     │
│  G ──██──────────██──────────────                            │
│  E ──────────────────██──────────░░░░░                       │
│                                                              │
│  [ your notes = solid ]   [ proposals = ghost, dashed ]      │
├──────────────────────────────────────────────────────────────┤
│  Ask ▸  ( Propose ⌄ )  ( Adjust ⌄ )  ( Add ⌄ )   ⟳ another   │
│         accept ⏎   ·   reject ⌫   ·   nothing lands untouched │
└──────────────────────────────────────────────────────────────┘
```

- **Solid = your notes; dashed/dim = the copilot's proposal** (ghosts), scoped to your selection.
- **Ask ▸** exposes the three verbs (Propose / Adjust / Add); **⟳** cycles a new seeded proposal;
  **⏎** accepts, **⌫** rejects. Until you accept, your notes are untouched.
- The affordance is opt-in and quiet — no suggestion appears unless you ask for it.

## Deferred
- The precise widget to express the axes (kept loose); the axes themselves are now specified in
  `docs/design/director-vocabulary.md` (energy / tension / valence, motion deferred).
- The ask affordance for the copilot (menu / keys / natural-language) and proposal scoping.
- The screens for the other flows (only #3's conductor surface and #10's sequencer surface are
  designed here).
