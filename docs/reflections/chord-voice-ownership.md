# Chord-voice ownership (the D47 → D48 remainder)

**Status:** direction locked as **D48** — sound-ownership rides the SAME
`ChordFollow` axis as steer-ownership. One axis, not two. Direction **A**,
justified and scoped by direction **C**. Direction **B** rejected.

## On the table

D47 named WHO STEERS the arranger-followed chord context
(`ChordEngine::m_state`, consumed by the band's NTT resolution) via
`ChordFollow { kAuto, kDetect, kSequencer, kManual }`, threading a `bool steer`
into `sound`/`play*` so a non-selected producer sounds but does not publish.

D47 did NOT name WHO SOUNDS the direct chord voice. `ChordEngine` owns a single
shared voicing — `m_sounding[4]` on one `m_out_port`/`m_out_channel` — and
`sound()` calls `release()` first. Both the sequencer (`fire_chord_seq → sound`)
and manual `chord play` (`play* → sound`) drive that one buffer. Consequence in
**kManual** with the sequencer running: the sequencer still `sound()`s, so it
releases the manual pad's notes and stacks its own G7. The band harmonizes on
manual's C (steer, correct) but the comping voice you HEAR is the sequencer's G7
and the manual pad was stomped. "Who steers" is separated; "who sounds the
comping voice" is not.

Three candidate directions were weighed (A unify sound with steer, B per-producer
voicing slots, C demote/scope the direct voice). What follows is the verdict.

## Keep (Tenere)

1. **Direction A as the mechanism — sound-ownership = steer-ownership.**
   *(Axis: the intersection.)* D47 separated "sound" from "steer" for an
   ENGINEERING reason — to avoid capture-and-restore of the context around each
   producer. That was a sound implementation convenience and nothing more. It was
   never a musical truth. Musically the comping voice you HEAR and the chord the
   band FOLLOWS are one decision: a listener cannot parse "the band is on C but
   the pad you hear is G7" as anything but a wrong chord. The technical seam
   D47 opened is exactly where the musical error leaks. So sound must follow the
   same predicate that steer follows. This is the core, and it is correct.

2. **Direction C as the framing and the scope-guard.** *(Axis: musical, then
   intersection.)* In a real arranger the ACCOMPANIMENT PARTS are the sound of the
   chord; the directly-stacked `ChordEngine` voicing is a lesser thing — a live
   monitor / comp voice (its reason to exist is D13/golden-G1: hear a chord NOW,
   transport idle, no style loaded). It cannot be deleted — it has honest uses —
   but it is explicitly ONE optional voice, not a first-class multi-producer
   surface. This framing is what makes B obviously wrong and keeps anyone from
   ever reintroducing per-producer slots. Keep C as doctrine, not as a fourth
   option.

3. **The "emit the event, gate only the sound" seam.** *(Axis: engineering.)*
   A non-selected producer must still fire `OutEvent::chord` (display/record) and
   still advance its state — only the audible `sound()` is gated. This is the
   exact split D47 already used for steer, applied one layer out. It is the clean,
   root-cause seam: no restore to forget, no hidden state.

4. **`kAuto` = legacy, all sound.** *(Axis: engineering / compatibility.)* The
   escape hatch keeps `kAuto` byte-identical to today — last-writer-wins on the
   single voice IS `kAuto`'s defined semantics — so the golden tests and existing
   behavior survive untouched. Keep it.

## Rework (Rilavorare)

1. **The "one axis vs two axes" framing — collapse it to one.** *(Axis: the
   intersection; a footgun if left as two.)* The ABI question is the crux and the
   answer is ONE axis: sound follows `kChordFollow`. A second, independent
   "who-sounds-the-pad" axis is the bug rebranded as a feature — every state where
   the two axes DISAGREE (manual steers C, sequencer sounds G7) produces an
   audible harmonic contradiction, which is precisely the defect being fixed.
   The adjacent desire that the two-axis camp gestures at — "keep the sequencer's
   RHYTHMIC comping going, re-harmonized to a live manual override" — is real, but
   it is a RE-VOICING feature (the sequencer's rhythm re-voiced onto the steered
   chord), not two free axes. Note it as a possible future increment; do NOT build
   it as an independent sound axis. D48 is one axis.

2. **`set_chord_follow` must grow a hanging-note release.** *(Axis: engineering —
   determinism / no stuck notes.)* Today it is a trivial `constexpr` setter. Under
   A, switching follow mid-play (e.g. `kSequencer → kManual`) de-selects the
   current sound owner while its voicing is still down. The setter must release
   the sounding voice on any de-selecting change (simplest root-cause: release on
   any change of `m_chord_follow` while `m_chords.sounding()`), routed through the
   scheduler so the note-tracker stays honest. Bounded, deterministic, no heap.

3. **`fire_chord_seq` and `cmd_chord`: split "sound" from "emit + advance/record",
   gate only the sound.** *(Axis: engineering; changes D47's tested behavior.)*
   `fire_chord_seq` currently ALWAYS calls `m_chords.sound(...)`. Under A it must
   sound iff `seq_may_follow()`, while still emitting `OutEvent::chord` and
   advancing the sequencer. Same shape for manual in `cmd_chord` (sound iff
   `manual_may_follow()`, still emit + record). This deliberately CHANGES the D47
   tests that assert "still sounds" (Torquato's): re-ratify red-first — flip the
   assertion to "non-selected producer emits its chord event but does not sound a
   competing voicing," prove it red against today's code, then green.

## Throw away (Buttare)

1. **Direction B — per-producer voicing slots / extra channels.** *(Axis: BOTH,
   simultaneously — the cleanest kill.)* Musically it manufactures the pile-up it
   claims to prevent: two close-position chord voicings sounding at once is two
   harmonies clashing, not richness — the arranger PARTS are where layered
   harmony belongs (see C). Engineering-side it spends scarce state AND a scarce
   MIDI channel on `arm-none-eabi` STM32H743 (16 channels already shared across
   drums/bass/chord/pad/arp/roles) to produce a WORSE musical result. A feature
   that fails on both axes at once to buy a clash is not a candidate. Dead.

2. **The literal two-axis ABI (sound independent of steer).** *(Axis: the
   intersection.)* Covered in Rilavorare #1; it belongs here too as a verdict:
   as a standing, freely-combinable axis it only ever adds harmonic contradiction.
   Buttare. The legitimate re-voicing want it hints at is a separate, later,
   properly-scoped feature — not this.

## Verdetto

The core instinct is right: **A**, and your lean is correct. D47's separation of
"sound" from "steer" was an engineering convenience mistaken for a musical
independence — it is not; who you HEAR and who the band FOLLOWS are one decision,
and the fix is to make the single chord voice ride the SAME `kChordFollow`
predicate. **One axis, not two** — a second axis is the bug wearing a feature's
coat. Frame it with **C** (the direct voice is one optional monitor/comp voice,
the arranger parts are the real chord sound), which is exactly what makes **B**'s
per-producer slots a double waste — channel budget and musical clarity both — and
therefore dead. Lock as **D48**: gate `sound()` on the follow predicate, keep the
`OutEvent::chord` seam observable, release the voice on a de-selecting follow
switch, and re-ratify the "still sounds" tests red-first.
