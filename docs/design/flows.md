# Flows — the intention-driven workstation

Status: **product-level flows** (the "how you use it", after `workstation-vision.md`'s "what it is"
and "what it does"). Experience-level, not wire/ABI detail. #3 and #10 are owner-validated; #1, #2,
#4–#9 are **detailed drafts (2026-07-07), pending owner validation** — validated one at a time.

## The flow set

1. **Start / set the base** — style, key, BPM → the band plays. **✎ detailed draft — see below.**
2. **Steer harmony live** — chords (even one-finger) → the band follows and holds; queue the next
   (amber→green). **✎ detailed draft — see below.**
3. **Conduct intention** — set/gesture a target (energy/tension/valence + rate) → the arrangement
   moves toward it on musical boundaries. **✓ detailed & validated (2026-07-07) — see below.**
4. **Build structure** — move through sections (Intro→A→B→Fill→Break→Ending), trigger
   fills/transitions → capture a trajectory as a Scene. **✎ detailed draft — see below.**
5. **Shape the feel** — mute/solo parts, swing/humanize, arp, per-part tweaks. **✎ detailed draft.**
6. **Deploy audio color** — record/import a clip → the Arranger deploys it (riser/vocal-chop/loop).
   **✎ detailed draft — see below.**
7. **Route sound** — assign parts to VST / melodd / hardware synths. **✎ detailed draft — see below.**
8. **Observe & override** — see what the Director/Arranger is doing, hand-override any decision
   (user-wins). **✎ detailed draft — see below.**
9. **Reproduce / recall** — lock a seed, reopen byte-exact; recall a Scene / a set (future).
   **✎ detailed draft — see below.**
10. **Author a part by hand, assisted on demand** — write notes in the sequencer yourself; ask the
    copilot to propose / adjust / add; accept as ghosts. **✓ detailed (2026-07-07) — see below.**

---

## Flow #1 — Start / set the base (from zero to a living band) — *draft*

**Start point:** you open sonotron; nothing is playing yet.

1. **Pick the base** — style (genre + feel), key, BPM, time signature. A few one-line choices, not
   menus-deep.
2. **The band comes in** — press play and a full, coherent band plays the style at that key/BPM
   *immediately* — no tracks laid, no arrangement authored.
3. **It's already musical** — a sensible default section (Intro or A), sensible parts
   (drums/bass/keys), a starting expressive state (mid energy, low tension, neutral valence — the
   Director's ground point).
4. **This is the ground state everything steers from** — flows #2 (harmony), #3 (intention), #4
   (structure) all steer *this* living band; nothing here is a dead-end setting.
5. **Seeded from the first note** — base + seed define a reproducible starting point (flow #9).

**The WOW:** three choices and a whole band is alive in front of you — no timeline, no clips, no
arranging, just a band waiting to be conducted.

*Still to detail:* the pick affordance (a style browser?); default section/parts per style; whether
BPM/key change live without stopping (yes — on boundaries).

---

## Flow #2 — Steer harmony live (one finger conducts the band) — *draft*

**Start point:** the band is playing (flow #1).
**Principle:** you play chords, the band **follows and HOLDS**; the hand beats the machine
(`kLivePriority`); no wrong notes (NTT — every arranger note is a chord tone by construction).

1. **Play a chord** — even one finger; the recognizer names it from the piano/split zone
   (chord-recognition is core-portable; the piano panel is host-live).
2. **The band re-harmonizes onto it, lossless** — the running loop re-voices onto your chord with
   no wrong notes, because every note is a chord tone by construction (NTT).
3. **It HOLDS until you change it** — the band stays on your chord; it does not wander off.
4. **Queue the next** — the next chord shows **amber** (queued), lands **green** on the bar
   boundary, with a countdown ("→2"); immediate-vs-next-bar is a choice.
5. **You win over the Director** — a played chord beats whatever the arranger/Director would have
   chosen; harmony you took by hand stays "yours" (user-wins, everywhere).
6. **Solo over it, too** — point the NTT resolver at your live melody hand: no wrong notes *for the
   human hand*, not only for the machine.

**The WOW:** one finger conducts the harmony of a whole band, live — and you cannot play a wrong
note, whether you're comping or soloing.

*Still to detail:* the split/zone config (where the chord hand ends and the solo hand begins —
core-portable); chord-entry affordances; how queued chords are edited or cancelled.

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

---

## Flow #4 — Build structure (song-form without a timeline) — *draft*

**Start point:** the band is playing; you want a song's shape without a linear audio timeline.
**Principle:** sections/variations/fills are **launchable states on musical time**; the path you
take is captured as a **Scene** (the Session paradigm — *home*; the Arrangement timeline — *no*).

1. **Move through sections** — Intro → A → B → Break → Ending, each a launchable state; trigger the
   next and it lands on a boundary (amber→green, like harmony).
2. **Trigger fills & transitions** — a fill/turnaround fires *before* the change, landing the
   section switch in time; one-shot, then back to the groove.
3. **Variations per part** — A/B/C/D density/variation *per track* — the Arranger's realization
   axis, distinct from the Director's perceptual intention.
4. **Capture a trajectory as a Scene** — the path you took (sections + moves over time) becomes a
   recallable **Scene**: song-form as a launchable object, not a painted timeline.
5. **Manual and assisted both** — you drive sections by hand; the Director (flow #3) can also drive
   them toward an intention — and you always win over it.
6. **Seeded** — the Scene + seed replay byte-exact (flow #9).

**The WOW:** you build a song's shape by *launching sections live*, and the shape you played becomes
a thing you can recall — no timeline, no arranging.

*Still to detail:* the section vocabulary per style; how a Scene is captured / named / edited; the
fill & transition triggering affordance.

---

## Flow #5 — Shape the feel (how it plays, not what) — *draft*

**Start point:** the band is playing; you want to shape *how* it plays.
**Principle:** per-part and global feel levers — mostly the Arranger/engine's realization — all
hand-operable, all overridable, all observable.

1. **Mute / solo parts** — drop the keys, solo the bass; on a boundary or immediate.
2. **Groove / swing / humanize** — global feel (swing amount, humanize timing/velocity), engine-
   driven and **genre-conditioned** (per node 9100 — feel lives downstream).
3. **Arp** — turn an arp on a part, pick pattern/rate; a part-level engine.
4. **Per-part tweaks** — density, register, octave, dynamics per track — the Arranger's realization
   levers (explicitly **not** Director axes; see `director-vocabulary.md`).
5. **All observable, all yours** — each lane shows its true feel state (hooks); a hand-tweak marks
   that lane "yours" and the Director won't overwrite it.

**The WOW:** you dial the feel of a live band — swing it, thin it, solo it — like a desk-of-feel,
without a mixing desk in sight.

*Still to detail:* which levers are global vs per-part; the arp config surface; how long a "yours"
tweak persists before the Director may resume.

---

## Flow #6 — Deploy audio color (arrangeable, not a track) — *draft*

**Start point:** the band is playing; you want audio color — a riser, a vocal chop, a loop.
**Principle:** arrangrr **DECIDES** audio symbolically (opaque clip references — "deploy clip N at
bar X, loop to clock"); **melodd/sampler REALIZE** it. No waveform editing, no linear timeline.

1. **Bring in a clip** — record your voice/gear, or import a sample/loop; it becomes an **opaque,
   content-addressed asset**.
2. **The Arranger deploys it as a decision** — a riser before the drop, a vocal-chop on the offbeat,
   a loop under section B — placed by **musical role, on the clock**, not painted on a timeline.
3. **It follows the arrangement** — the clip loops to the clock, drops with the section, rises with
   the intention (energy/tension) like any other part.
4. **Opaque and reproducible** — the seed *points at* the asset but never regenerates its samples;
   the decision (when/where) is seeded, the audio is referenced (flow #9).
5. **Optional peer** — no audio engine (STM32, or a host without melodd) → the color simply isn't
   deployed; the band degrades gracefully.

**The WOW:** you drop a vocal chop or a riser and the arranger *places and loops it musically* —
audio as an arrangeable color, never a track to edit.

*Still to detail:* the record/import affordance; how the Arranger chooses placement (role tags?);
the clip-role vocabulary (riser / chop / loop / texture).

---

## Flow #7 — Route sound (pick the voices; the brain doesn't care) — *draft*

**Start point:** the band is playing (MIDI); you want to choose what makes the sound.
**Principle:** arrangrr emits **MIDI symbols**; the outer product (sonotron) routes each part to a
sound source — VST/CLAP/LV2, melodd, or hardware. Routing is **host, never core**.

1. **Assign a part to a source** — drums → a VST drum plugin, bass → melodd, keys → a hardware synth
   over MIDI-out.
2. **Mix sources freely** — each part independently: hosted plugin, melodd, or external gear; the
   pipeline composes many sources (the orchestrator).
3. **MIDI-FX in the chain** — a VST MIDI-FX between arranger and instrument (host chain).
4. **Standalone or driving gear** — self-contained with hosted plugins/melodd, or MIDI-out to real
   synths — independent of the arrangement itself.
5. **The core stays ignorant** — arrangrr doesn't know a VST or a synth exists; it emits symbols,
   the orchestrator realizes.

**The WOW:** the same living arrangement drives your favorite plugins, melodd, or the hardware on
your desk — you pick the voices, the brain doesn't care.

*Still to detail:* the routing surface (a patchbay?); plugin scan/host UX (later outer-product
work); per-part MIDI channel / program mapping.

---

## Flow #8 — Observe & override (nothing hidden, nothing locked) — *draft*

**Start point:** any time; you want to see what the machine is doing and take control.
**Principle:** the three laws — everything **observable** (via hooks), everything **overridable and
you win**, everything **hand-operable**. TUI and GUI mirror the *same* truth (hooks, not privileged
accessors).

1. **See the true state** — every part shows what it does NOW (activity, density, mute/solo, chord,
   section) via hooks — never guessed.
2. **See the machine's next move** — the Director's upcoming moves show as **ghosts** ("Pad enters
   →4"), before they land.
3. **Override anything** — mute a part, force a section, change a chord, move the intention — your
   hand beats the Director instantly.
4. **You win, and it's marked** — what you took by hand stays "yours"; the Director re-proposes from
   where you now are, never overwriting your hand.
5. **Uniform everywhere** — the same observe+override on every component (Director, Arranger,
   sequencers, harmony, groove, arp, looper, MIDI-FX), in TUI and GUI alike.

**The WOW:** nothing is hidden and nothing is locked — you can watch the whole band's mind and put
your hand on any decision, and you always win.

*Still to detail:* the hook layer made real (ABI-additive; the three decided directions in
`hooks-design-notes.md`) and the TUI reading from hooks (host refactor) — engineering, tracked
separately from this experience.

---

## Flow #9 — Reproduce / recall (the good take is a fact, not luck) — *draft*

**Start point:** you made something good; you want it back byte-exact, or to recall a saved state.
**Principle:** determinism — same intention/seed → same result next week; the seed is a **musical
object**.

1. **Lock the seed** — one visible control; the whole performance (arrangement decisions, variations,
   the build) becomes reproducible.
2. **Reopen byte-exact** — next week the same seed replays the same music, note-for-note (audio
   clips are *referenced*, not regenerated — the seed owns every note, not one sample).
3. **Audition and lock a variation** — audition N seeded variations, lock the one you like, recall
   it later (reproducible generativity).
4. **Recall a Scene / a set** — a captured Scene (flow #4) reopens as-was; a set of Scenes (future)
   for a whole performance.
5. **Your hand-moves fold in** — overrides you made fold into the seed/log, so "yours" replays too.

**The WOW:** the good take isn't a lucky one-off — it's a fact you can lock, reopen, and trust to be
identical, forever.

*Still to detail:* the seed / Scene / set persistence format; the audition-N-variations surface;
set-level recall (future).

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
