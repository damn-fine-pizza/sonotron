# Flows — the intention-driven workstation

Status: **product-level flows** (the "how you use it", after `workstation-vision.md`'s "what it is"
and "what it does"). Experience-level, not wire/ABI detail. Detailed flows are owner-validated one
at a time; the rest are stubs.

## The flow set

1. Start / set the base — style, key, BPM → the band plays. *(stub)*
2. Steer harmony live — chords (even one-finger) → the band follows and holds; queue the next
   (amber→green). *(stub)*
3. **Conduct intention** — set/gesture a target (energy/tension/valence + rate) → the arrangement
   moves toward it on musical boundaries. **✓ detailed & validated (2026-07-07) — see below.**
4. Build structure — move through sections (Intro→A→B→Fill→Break→Ending), trigger fills/transitions
   → capture a trajectory as a Scene. *(stub)*
5. Shape the feel — mute/solo parts, swing/humanize, arp, per-part tweaks. *(stub)*
6. Deploy audio color — record/import a clip → the Arranger deploys it (riser/vocal-chop/loop). *(stub)*
7. Route sound — assign parts to VST / melodd / hardware synths. *(stub)*
8. Observe & override — see what the Director/Arranger is doing, hand-override any decision
   (user-wins). *(stub)*
9. Reproduce / recall — lock a seed, reopen byte-exact; recall a Scene / a set (future). *(stub)*
10. **Author a part by hand, assisted on demand** — write notes in the sequencer yourself; ask the
    copilot to propose / adjust / add; accept as ghosts. **✓ detailed (2026-07-07) — see below.**

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
   **drop**: the resolution lands *in time*, not at random. (No build without a drop.)
5. **At any moment you hand-override and YOU win** — mute the bass, force a section, play a chord:
   your hand beats the Director, which resumes *proposing* from where you now are (user-wins,
   everywhere).
6. **Lock the seed** — the same intention, tomorrow, produces the same build **byte-exact**.

**The WOW:** you programmed no clips and wrote no arrangement — you said *"build to a peak"* and a
whole band **composed and performed** the build, musically, and you could put your hands on it any
instant.

*Still to detail:* the exact intention axes and the precise widget to express them (kept loose /
expandable per the owner) — this captures the EXPERIENCE, not those specifics.

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
