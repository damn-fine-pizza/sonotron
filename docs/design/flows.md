# Flows — the intention-driven workstation

Status: **product-level flows** (the "how you use it", after `workstation-vision.md`'s "what it is"
and "what it does"). Experience-level, not wire/ABI detail. #3 and #10 owner-validated; #1, #2,
#4–#9 **reviewed (Prospero + Puccini, 2026-07-08)** and corrected — flagged forks/futures noted
in-flow.

> **Realization altitude (read this first).** This doc stays at experience/product altitude. The
> concrete GUI screen that realizes several of these flows (notably #1, #2, #4, #5, #10) is specified
> authoritatively in `ux-workstation.md` (the sonotron workstation screen, node `11600`, decided
> 2026-07-10 — *later* than the 2026-07-08 review above); where this doc and that one disagree about
> the *screen*, `ux-workstation.md` wins and this doc is deliberately silent on screen detail. The
> most consequential drift since the review: the **intention/conductor idea of flow #3 is demoted**
> on that screen to an **optional, read-only side rail** (`ux-workstation.md` §4.7), pending the
> Director (node `10000`, planned CAPSTONE — not built). This doc keeps flow #3 as the product-vision
> spine; its *screen* status today is the demoted rail.

## The flow set

1. **Start / set the base** — style, key, BPM → the band plays. **✓ reviewed — see below.**
2. **Steer harmony live** — chords (even one-finger) → the band follows and holds; queue the next
   (amber→green); opt-in "no wrong notes" for your hand too. **✓ reviewed — see below.**
3. **Conduct intention** — set/gesture a target (energy/tension/valence; motion/rate deferred, see
   `director-vocabulary.md`) → the arrangement moves toward it on musical boundaries. **✓ detailed &
   validated (2026-07-07); on the current screen demoted to an optional read-only rail
   (`ux-workstation.md` §4.7), pending the Director (node `10000`) — see below.**
4. **Build structure** — move through sections (Intro→A→B→Fill→Break→Ending), trigger
   fills/transitions → capture the path as a **Song**. **✓ reviewed — see below.**
5. **Shape the feel** — shape by hand and what you touch stays *yours*; mute/solo, swing/humanize,
   arp, per-part tweaks. **✓ reviewed — see below.**
6. **Deploy audio color** — record/import a clip → the Arranger deploys it (riser/vocal-chop/loop).
   **✓ reviewed — HOST-ONLY, deferred — see below.**
7. **Route sound** — the same live band on many voices at once (VST / melodd / hardware).
   **✓ reviewed — see below.**
8. **Observe & override** — see what the Director/Arranger is doing, hand-override any decision
   (user-wins). **✓ reviewed — see below.**
9. **Reproduce / recall** — lock a seed, reopen byte-exact; recall a Song / a Scene / a set (future).
   **✓ reviewed — see below.**
10. **Author a part by hand, assisted on demand** — write notes in the sequencer yourself; ask the
    copilot to propose / adjust / add; accept as ghosts. **✓ detailed (2026-07-07) — see below.**

> **Vocabulary note (DESIGN.md §7):** a **Scene** is a *snapshot* of state (which variation / mute /
> routing, recalled in an instant — Roland/Ableton sense); a **Song** is the *path over time* (an
> ordered, reproducible sequence of sections and moves — not a timeline you paint on). Flow #4
> captures a **Song**.

---

## Flow #1 — Start / set the base (from zero to a living band)

**Start point:** you open sonotron; nothing is playing yet.

1. **Pick the base** — style (genre + feel), key, BPM, time signature. A few one-line choices, not
   menus-deep.
2. **The band comes in** — press play and a full, coherent band plays the style at that key/BPM
   *immediately* — no tracks laid, no arrangement authored.
3. **It's already musical** — a sensible default section (Intro or A) and parts (drums/bass/keys).
   The starting expressive state (mid energy, low tension, neutral valence) is what the style/section
   default *implies* — on bare arrangrr / STM32 it's just that default; the true **Director** ground
   point is host-side (sonotron), not present on the naked core.
4. **This is the ground state everything steers from** — flows #2 (harmony), #3 (intention), #4
   (structure) all steer *this* living band; nothing here is a dead-end setting.
5. **Seeded from the first note** — base + seed define a reproducible starting point (flow #9).

**The WOW:** three choices and a whole band is alive in front of you — no timeline, no clips, no
arranging, just a band waiting to be conducted.

*Still to detail:* the pick affordance (a style browser?); default section/parts per style. **Live
BPM/key change** lands on boundaries — but a live BPM change must not desync audio color (flow #6):
either it's **suspended while a clip is looping**, or melodd resyncs at the loop point (owner-call,
not free — arrangrr never processes audio).

---

## Flow #2 — Steer harmony live (one finger conducts the band)

**Start point:** the band is playing (flow #1).
**Principle:** you play chords, the band **follows and HOLDS**; the hand beats the machine
(`kLivePriority`); no wrong notes for the *machine* (NTT — every arranger note is a chord tone by
construction), and *optionally* for your hand too.

1. **Play a chord** — even one finger; the recognizer names it from the piano/split zone
   (chord-recognition is core-portable; the piano panel is host-live).
2. **The band re-harmonizes onto it, lossless** — the running loop re-voices onto your chord with
   no wrong notes, because every note is a chord tone by construction (NTT).
3. **It HOLDS until you change it** — *latching* with no chord-sequencer under it; with a sequencer
   running, releasing lets the queued progression resume (*momentary* — `2340`). You always know
   what sounds a beat after you let go.
4. **Queue the next** — the next chord shows **amber** (queued), lands **green** on the bar
   boundary, with a countdown ("→2"); immediate-vs-next-bar is a choice.
5. **You win over the Director** — a played chord beats whatever the arranger/Director would have
   chosen; harmony you took by hand stays "yours" (user-wins, everywhere).
6. **Solo over it — with a safety net you switch on, not one that cages you** — the NTT resolver can
   point at your live melody hand so you can't play a wrong note, but it is **opt-in per zone** (off
   by default for players who can play), and its **strength is declared** — *hard-snap* (every note
   forced onto the chord) · *nearest-chord-tone* (nudged, room left) · *off*. It's the MIDI-FX insert
   of band `5000`, not an always-on tutor: you keep the right to bend a blue note when you mean it.

**The WOW:** one finger conducts the harmony of a whole band, live — and, if you ask for it, you
can't play a wrong note either, without losing the freedom to play a "wrong" one on purpose.

*Still to detail:* the split/zone config (chord hand vs solo hand — core-portable); how queued
chords are edited/cancelled. Note: "instant" is literal for single-finger (direct key→chord); a
fingered/full chord carries a small hold/hysteresis window to avoid flicker (the exact debounce
control is still to detail — no such config field exists yet).

---

## Flow #3 — Conduct intention (the first-WOW, the distinctive spine)

**Start point:** the band is already playing a groove (flow #1); you are steering the chords by hand.

1. **See the current expressive state** — where the music sits *now*: mid energy, low tension,
   neutral valence. The band plays coherently there.
2. **Express a target** — you don't set "the value now", you set **where you want it to GO and how
   fast**: e.g. *"toward more energy + more tension, rising over 8 bars"* — by moving the controls to
   where you want to ARRIVE, or with an "up" gesture.
3. **The system walks toward the target, on musical boundaries** — the key distinction:
   - **structure = stepped (thresholds):** halfway up the arp switches on, the pad re-enters, it
     moves to Var C, a fill fires before the peak — with **hysteresis**, so it doesn't flap around
     the switch point;
   - **color = continuous:** groove intensity rises, register lifts, dynamics open — smooth.
4. **Tension builds, then RESOLVES** — at the peak a roll/fill, and on the next downbeat the
   **drop**: the resolution lands *in time*, not at random. (Build→drop is the style-default, not a
   universal law — held-and-unresolved tension is a legitimate choice; see `director-vocabulary.md`.)
5. **At any moment you hand-override and YOU win** — mute the bass, force a section, play a chord:
   your hand beats the Director, which resumes *proposing* from where you now are (user-wins,
   everywhere).
6. **Lock the seed** — the same intention, tomorrow, produces the same build **byte-exact**.

**The WOW:** you programmed no clips and wrote no arrangement — you said *"build to a peak"* and a
whole band **composed and performed** the build, musically, and you could put your hands on it any
instant.

*Still to detail:* the precise widget to express the axes (kept loose per the owner) — this captures
the EXPERIENCE, not the widget.

*Honest status:* the **motion/rate axis is deferred** — the proposed axis set is `energy · tension ·
valence` only (`director-vocabulary.md`); "over 8 bars"/"rising" above is the intended experience,
not a shipped trajectory control. And on today's GUI screen this flow is **not** the center: it is a
demoted, read-only intention rail (`ux-workstation.md` §4.7) until the **Director** (node `10000`,
planned CAPSTONE) exists. This flow remains the product-vision spine; it is simply not yet realized
as such on the screen.

---

## Flow #4 — Build structure (song-form without a timeline)

**Start point:** the band is playing; you want a song's shape without a timeline you paint on.
**Principle:** sections/variations/fills are **launchable states on musical time**; the path you
take is captured as a **Song** (an ordered, reproducible sequence — the Session paradigm, *home*;
the Arrangement editing timeline, *no*).

1. **Move through sections** — Intro → A → B → Break → Ending, each a launchable state; trigger the
   next and it lands on a boundary (amber→green, like harmony).
2. **Trigger fills & transitions** — a fill/turnaround fires *before* the change, landing the
   section switch in time (the scheduler inserts the fill until the next bar boundary, then
   switches); one-shot, then back to the groove.
3. **Variations per part** — A/B/C/D density/variation *per track* — the Arranger's realization
   axis, distinct from the Director's perceptual intention.
4. **Capture the path as a Song** — the path you took (sections + moves over time) becomes a
   recallable **Song**: song-form as a launchable object, not a painted timeline. (A **Scene** — an
   instant config snapshot — is a *different* recall, see the vocabulary note above.)
5. **Manual and assisted both** — you drive sections by hand; the Director (flow #3) can also drive
   them toward an intention. NOTE: "you win over the Director" *on sections* is **not yet built** —
   user-wins is shipped for chords (`kLivePriority`); generalizing arbitration to sections is a real
   ABI commitment, still-to-detail (`hook-interface.md`, NEEDS-DECISION).
6. **Seeded** — the Song + seed replay byte-exact (flow #9).

**The WOW:** you build a song's shape by *launching sections live*, and the shape you played becomes
a thing you can recall — no timeline **you paint on**, no arranging. (The recall is still an ordered,
timed structure internally — determinism isn't free of sequence; what's refused is the audio-editing
surface, not the existence of chronological data.)

*Still to detail:* the section vocabulary per style; how a Song is captured / named / edited; the
fill & transition triggering affordance; section-level user-wins arbitration (above).

---

## Flow #5 — Shape the feel (you hold the feel; the Director never overwrites your hand)

**Start point:** the band is playing; you want to shape *how* it plays.
**Principle — the arbitration IS the flow:** you shape the feel by hand, and **whatever you touch
stays *yours* — the Director will not overwrite it.** That persistent human-over-machine hold on the
feel is the thing a Genos/Korg can't do; the levers below are ordinary, the *ownership* is not.

1. **Touch a lever → it's yours** — the instant you set a value by hand, that lane is marked "yours";
   the Director proposes around it, never through it, until you release it.
2. **Mute / solo parts** — drop the keys, solo the bass; on a boundary or immediate.
3. **Groove / swing / humanize** — global feel (swing amount, humanize timing/velocity), engine-
   driven and **genre-conditioned** (per node 9100 — feel lives downstream).
4. **Arp** — turn an arp on a **harmonic-role part** (chord / pad / lead), pick pattern/rate; a
   part-level engine (not on drums/bass, where "an arp" isn't a thing a player would reach for).
5. **Per-part tweaks** — density, register, octave, dynamics per track — the Arranger's realization
   levers (explicitly **not** Director axes; see `director-vocabulary.md`).

**The WOW:** you dial the feel of a live band — swing it, thin it, solo it — and it *stays* the way
you left it; the machine keeps composing everywhere except where your hand is.

*Still to detail:* which levers are global vs per-part; the arp config surface; how long a "yours"
hold persists before the Director may resume; that per-part density/dynamics is partly aspirational
today (only register, `kRoleAnchor`/`3230`, is shipped as a manual lever).

---

## Flow #6 — Deploy audio color (arrangeable, not a track) · HOST-ONLY, deferred

**Start point:** the band is playing; you want audio color — a riser, a vocal chop, a loop.
**Principle:** arrangrr **DECIDES** audio symbolically (opaque clip references — "deploy clip N at
bar X, loop to clock"); **melodd/sampler REALIZE** it. No waveform editing, no linear timeline.
**Regime:** HOST-ONLY, and melodd **does not exist yet** — this flow is **NEEDS-DECISION, deferred**
(owner-call on the audio-host library, see `docs/reflections/melodd-audio-companion.md`).

1. **Bring in a clip** — record your voice/gear, or import a sample/loop; it becomes an **opaque,
   content-addressed asset**.
2. **The Arranger deploys it as a decision** — a riser before the drop, a vocal-chop on the offbeat,
   a loop under section B — placed by **musical role, on the clock**, not painted on a timeline. (The
   layering/stinger vocabulary is borrowed honestly from game adaptive-music middleware, but driven
   *live* by a human + a deterministic Director, not baked at build-time.)
3. **It follows the arrangement** — the clip loops to the clock, drops with the section, rises with
   the intention (energy/tension) like any other part.
4. **Opaque and reproducible** — the seed *points at* the asset but never regenerates its samples;
   the decision (when/where) is seeded, the audio is referenced (flow #9).
5. **Optional peer — and the absence is VISIBLE** — no audio engine (STM32, or a host without
   melodd) → the color isn't deployed **and you're told**: a section that expected a riser shows the
   gap, not a silent hole (a silently-vanished riser is a bug to a musician, not graceful elegance).

**The WOW:** you drop a vocal chop or a riser and the arranger *places and loops it musically* —
audio as an arrangeable color, never a track to edit.

*Still to detail:* the record/import affordance; how the Arranger chooses placement (role tags?);
the clip-role vocabulary (riser / chop / loop / texture); and the whole thing waits on melodd.

---

## Flow #7 — Route sound (the same live band, on many voices at once)

**Start point:** the band is playing (MIDI); you want to choose what makes the sound.
**Principle:** arrangrr emits **MIDI symbols**; sonotron routes each part to a sound source. Routing
is **host, never core** — a real architectural merit, but not the point *to the player*. The point
to the player is below.

**Two halves, honestly different:**
- **MIDI → hardware (shipped, real):** assign parts to external synths over multi-port DIN/USB
  (Zone / RoutingProfile — `1260`, ✅). A player with gear on the desk uses this today.
- **VST/CLAP/LV2 + melodd sources (host, future):** hosted instruments + the audio peer — **not
  built** (NEEDS-DECISION, with flow #6's melodd dependency).

1. **Assign a part to a source** — drums → a VST drum plugin, bass → melodd, keys → a hardware synth.
2. **Mix sources freely** — each part independently; the pipeline composes many sources (the
   orchestrator).
3. **MIDI-FX in the chain** — a VST MIDI-FX between arranger and instrument (host chain).

**The WOW:** the **same living arrangement — the same generative band, the same live harmony —
sounds at once on drums-VST + bass-melodd + a hardware synth, and never becomes a track to edit.**
It's not "you assigned outputs" (every DAW does that); it's one live band, many voices, still one
arrangement.

*Still to detail:* the routing surface (a patchbay?); plugin scan/host UX (future, host-side);
per-part MIDI channel/program mapping.

---

## Flow #8 — Observe & override (nothing hidden, nothing locked)

**Start point:** any time; you want to see what the machine is doing and take control.
**Principle:** the three laws — everything **observable** (via hooks), everything **overridable and
you win**, everything **hand-operable**. TUI and GUI mirror the *same* truth.

1. **See the true state** — every part shows what it does NOW (activity, density, mute/solo, chord,
   section) via hooks — never guessed.
2. **See the machine's next move** — the Director's upcoming moves show as **ghosts** ("Pad enters
   →4"), before they land. (Closest precedent is outside music — telegraph-then-override, as in
   *Into the Breach*; no live-music tool does this today, which is exactly the opportunity.)
3. **Override anything** — mute a part, force a section, change a chord, move the intention — your
   hand beats the Director instantly.
4. **You win, and it's marked** — what you took by hand stays "yours"; the Director re-proposes from
   where you now are, never overwriting your hand.
5. **Uniform everywhere** — the same observe+override on every component, in TUI and GUI alike.

**The WOW:** nothing is hidden and nothing is locked — you can watch the whole band's mind and put
your hand on any decision, and you always win.

> **Honest status — this is the target, not today's reality.** Observability is currently
> **bifurcated** (a poor cross-process channel of a few `OutEvent::Kind` + a rich TUI-only one via
> direct accessors), and "user-wins everywhere" is **decided, not built** (shipped only for chords).
> If this promise doesn't become true *uniformly* (every component, TUI *and* GUI), the WOW collapses
> from "nothing is hidden" to "something is hidden, depending on the client" — the kind of lie a
> musician finds on the second session. The hook layer (ABI-additive) is where it gets made real.

*Still to detail:* the hook layer made real (`hook-interface.md`) and the TUI reading from hooks
(host refactor) — engineering, tracked separately.

---

## Flow #9 — Reproduce / recall (the good take is a fact, not luck)

**Start point:** you made something good; you want it back byte-exact, or to recall a saved state.
**Principle:** determinism — same intention/seed → same result next week; the seed is a **musical
object**.

1. **Lock the seed** — one visible control; the whole performance (arrangement decisions, variations,
   the build) becomes reproducible.
2. **Reopen byte-exact** — next week the same seed replays the same music, note-for-note (audio
   clips are *referenced*, not regenerated — the seed owns every note, not one sample).
3. **Audition and lock a variation** — audition N seeded variations, lock the one you like, recall
   it later (reproducible generativity).
4. **Recall a Song, a Scene, or a set** — a captured **Song** (the path, flow #4) reopens as-was; a
   **Scene** snapshots an instant config; a **set** (future) groups them for a whole performance.
5. **Your hand-moves fold in** — overrides you made fold into the seed/log, so "yours" replays too.

**The WOW:** the good take isn't a lucky one-off — it's a fact you can lock, reopen, and trust to be
identical, forever.

> **On originality (honest framing):** "a reproducible seed" alone is *not* new — Wotja/Noatikl do it
> in ambient. What's unclaimed is the **combination**: byte-exact reproducibility on a **live,
> chord-reactive arrangement**, with **your hand-moves folded into the seed**. Say *that*, not "nobody
> does it".

*Still to detail:* the seed / Song / Scene / set persistence format; the audition-N-variations
surface; set-level recall (future — the Scene/song-mode level, node `8000`, is not built).

---

## Flow #10 — Author a part by hand, assisted on demand (you always hold the pen)

**Principle:** manual note entry stays first-class; the sequencer is a **pull-based copilot** —
silent until asked, and nothing lands until you accept. The note-level form of "the machine
proposes, you command."

1. **You write notes** — step or piano-roll, directly, as always. Nothing is generated unless you
   ask. This is the default posture.
2. **You ask for help, scoped to a selection/region** — three verbs, all opt-in:
   - **Propose** — *"a bassline for these chords", "a fill here", "a counter-melody"* → arrives as
     **ghost notes** you can **accept**, **cycle** ("give me another" — a new seed), or **dismiss**.
   - **Adjust** — *"tighten to the groove", "fix the voice-leading", "humanize", "snap into the
     key"* → shown as a **diff (ghost)** over your notes; accept or reject.
   - **Add** — *"a second voice", "passing tones / ghost-notes", "an octave doubling"* → **additive
     ghosts** you accept.
3. **You accept, cycle, or reject** — ghosts never sound as committed until accepted; rejecting
   leaves your notes untouched. You beat the machine at the note level, always.
4. **Deterministic** — every proposal is **seeded**, so "give me another" walks seeds and any
   accepted notes fold into the seed/log — the part stays byte-reproducible.

**The WOW:** it's your part, in your hand — but a musical assistant is one ask away for the bass
line you can't be bothered to voice or the fill you can't quite hear, and it never touches a note
you didn't approve.

*Still to detail:* the ask affordance (menu / keys / natural-language), and how a proposal's scope
is chosen (selection vs region vs whole part).
