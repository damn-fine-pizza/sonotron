# Reflection — external audio *input*: the one un-regenerable artifact, and where it may live

> **Status — future / not current phase.** Builds on the not-yet-scheduled `melodd` direction
> (DESIGN.md D43/`0910`); no roadmap node exists for external audio input yet. Out of scope for the
> current host-GUI mechanical strand (node 11600) + core P0.

Status: critique / verdict (Prospero). Not a locked decision. Read against
`docs/DESIGN.md` (D1–D42) and, above all, `docs/backlog/melodd-audio-companion.md`
— this is the single open node that verdict left unresolved and named as the next
reflection: not internal render (melodd already covers it), but **sound that enters
from the world** (voice, guitar, microphone, line-in). Built *on top of* the melodd
verdict — its rule, its Trojan-horse taxonomy, its `core → melodd` one-way clock —
not repeated here, assumed. Also assumed: the "kill the word DAW" verdict
(`docs/reflections/live-daw-around-arrangrr-ux.md`), whose firewalls this extends to
the input side. This judges a **direction**, not code.

---

## Assumptions (declared, not blocking)

The brief said "proceed without waiting". These are the load-bearing assumptions; if
one is wrong, the entry that depends on it is the one to revisit.

1. **The subject is external audio *capture*, not internal render.** melodd already
   owns rendering the core's MIDI into sound (a *regenerable photograph* of a
   performance the seed already authored). Here the subject is the opposite polarity:
   a signal that arrives from a jack — a vocal, a guitar DI, a mic, a line-in — that
   the seed *never contained* and can *never* reproduce. The entire novelty and the
   entire danger is that single word: **un-regenerable**.
2. **melodd exists in the melodd-verdict shape:** a host-only, downstream,
   clock-slaved audio companion linked into the host daemon, a sibling of the
   ALSA/CoreMIDI output backend, degradable to silence, of which the core is ignorant
   and whose name the core never learns. Audio *input* capture is judged as a *facet
   of melodd* — the same host-only citizen, now also owning an input path — not as a
   new subsystem grafted somewhere new.
3. **The core stays exactly what D1/D2/D32/D33 lock it to be:** MIDI-only,
   freestanding, no-heap, bounded, dual-target, STM32-anchored, authoritative on time
   via injected ticks. Nothing here is allowed to touch that, and every verdict is
   measured against whether the proposal keeps that promise.
4. **The session/clip paradigm is fixed** (melodd verdict, live-daw verdict):
   Session-view launchable clips **yes**, a linear Arrangement *audio* timeline
   **no**. A captured audio take is judged against *that* grid, not against a DAW's
   audio-arrangement ruler.
5. **The seed-as-object thesis is the pillar being stress-tested:** a performance is
   a reproducible command/MIDI/seed log (D16/D17/D29), replayable byte-exact. The
   whole question is whether an opaque recorded take can coexist with that without
   cracking it.

What I could **not** verify and did not bluff: concrete input-monitoring latency
numbers of any backend (not measured — I do not benchmark); the final hardware; and
whether the user wants audio-to-MIDI onset capture at all (I judge it as a *named
temptation* and flag where the answer changes the verdict).

---

## Is it worth it? — the musical case, honestly

Yes — and precisely *because* it is the one thing melodd's render can never be.
melodd's audio is a photograph of a performance the log already owns byte-exact;
delete it and you regenerate it. External input is a photograph of something the log
**never held** — the only genuinely irreplaceable artifact the product can produce.
That asymmetry is not a weakness to apologize for; it is the entire reason the
feature has musical worth. The real use-cases, and who asks for them:

- **The singer/looper over the generative band (personas 1 & 4).** Sing a hook, loop
  it, layer harmonies over an arranger that is following your chords and a Director
  that is building energy underneath. This is the canonical live-looping act, and it
  is exactly what the Boss RC line, Ableton's Looper and Loopy Pro exist to serve —
  audio loops launched and layered in a Session-style grid, synced to a host tempo
  ([Ableton live-looping guide](https://lofimonster.com/blog/live-looping-ableton-2026-guide),
  [Loopy Pro](https://loopypro.com/)). The generative-MIDI-plus-live-voice
  combination is a real, current performance idiom
  ([Attack — generative MIDI in Live](https://www.attackmagazine.com/technique/tutorials/getting-started-with-ableton-lives-generative-midi-tools/)).
- **The instrumentalist tracking over the arranger (persona 2, the producer mining
  ideas).** Play a guitar or bass take over the generated bed to capture a phrase
  worth keeping while the idea is hot. Here audio is scaffolding for composition, not
  the final artifact.
- **On-the-fly sampling (the Koala idiom).** Grab a sound from the world and play it
  back rhythmically, sliced or pitched, triggered by the clock. Note this is subtly
  *different*: the captured audio becomes an **instrument**, not a **take** — closer
  to imported content played by MIDI than to a recorded performance. Loopy Pro models
  exactly this (audio clips targeted by MIDI, played as pitched notes or slices —
  [Loopy Pro Wiki, Samplers](https://wiki.loopypro.com/Samplers)).
- **"Monitor-rec continuous", extended to input.** The always-on retroactive-capture
  ring (item #15 / dream #7) that the melodd verdict defined for render, now fed from
  the input jack: so a spontaneous vocal or riff is *already recorded* the instant you
  decide it was good. This is the strongest and most dangerous of the four — dangerous
  for the reason set out below.

So: musically desirable, genuinely, and distinct from melodd's render. That does not
make it *technically* wise without conditions. The rest of this document is the
conditions.

---

## The determinism thesis, without discounts — and the two-tier clip model

The pillar is that a performance is a byte-exact reproducible log. A recorded vocal
take is **not** regenerable from a seed — float, converters, the world itself
guarantee it. Audio render is already non-deterministic (float non-associativity,
buffer-alignment, order — the melodd verdict established this); external input is a
harder case, because it is not even *derived* from anything the log holds. This forces
the question the brief names: does a **two-tier clip model** appear, and does it
crack the session/clip unity just fixed?

**A two-tier model does appear. It is acceptable — on one precise condition.** The two
tiers must differ on exactly **one** axis and no other:

- **MIDI clip** — degree-relative (D28/D39), re-harmonizable, re-voiceable (D41),
  transposable, **regenerable from the seed byte-exact**. A *formula*.
- **Audio-input clip** — time-referenced, opaque, frozen, un-transposable in any
  musical sense, **not regenerable** — the seed can only *point at* it. A *pasted
  value*.

The asymmetry is provenance/regenerability, and *only* that. It does **not** extend to
launch semantics, scene-following, loop behaviour, or grid placement — on those axes
the two clip kinds are identical citizens of the same Session grid. This is the whole
discipline. The model already *must* admit opaque imported content — importing a WAV
is a real, wanted feature — and a live-captured take **is just a WAV that arrived
through a jack instead of a file dialog**. Seen that way, input capture introduces no
new category at all: it reuses the "imported opaque asset" category the project needs
regardless. The two-tier model breaks unity **only if** you demand that *every* clip
be re-harmonizable — which was never the promise. Get that framing right and there is
no fracture; get it wrong (two divergent clip products with different launch rules)
and you have quietly built two apps.

---

## The precise rule that saves the seed-as-object

Draw this line at full detail, because everything rests on it.

**The log is the single authoritative reproducible object. External audio is never
*part* of the log; it is an external asset the log *references* by a content id.** The
log records the *command*: "at command-time T, capture-clip #K was created,
content-addressed asset A (hash H), anchored at tick τ, length L, routed to slot S".
Replaying the log reproduces every MIDI/command/seed byte the arranger ever authored,
**and re-points** clip #K at asset A. It does **not** regenerate A's samples — that is
impossible and the model never claims it. The asset is opaque, content-addressed,
host-side, heap/disk-backed, entirely **outside** the deterministic model, exactly as
an imported WAV would be.

Three consequences, each load-bearing:

1. **The seed reproduces the pointer, never the payload.** The seed-as-object thesis
   evolves — cleanly — into: *the seed reproduces everything it authored (every note,
   every command, every random draw); imported content it merely points at,
   byte-preserving the reference, never the bytes.* D16's recall guarantee survives
   intact: the MIDI performance is byte-identical next week; the audio take is
   attached, not regenerated.
2. **The MIDI object is whole without the audio.** A seed shared *without* its asset
   blobs still replays the entire MIDI performance perfectly — the audio clips are
   simply absent/silent, precisely as a project with a missing sample file behaves.
   This is the proof the thesis is uncracked: the deterministic musical object does
   not *depend* on the opaque attachment. "Seed = musical object" stays literally
   true; the audio is luggage, not the traveller.
3. **The opaque asset must never be a *cause*.** The existence, timing, level or
   content of a captured clip must never change **which MIDI events fire, or when**.
   The moment a recorded take's onset retroacts on the scheduler — auto-tempo from the
   vocal, an onset that triggers a core event, a capture whose length nudges the bar —
   the same Trojan horse the melodd verdict named has walked in through the input jack.
   Audio capture is a **leaf**: downstream, referenced, stamped by core time, never a
   source of it.

That is the rule that keeps the thesis intact under an un-regenerable artifact: **the
log owns the truth of MIDI+commands+seed; audio is an external referenced asset, a
pointer the seed preserves and never dereferences into determinism.**

---

## Where it lives, and where it does not — plus the clip/timeline boundary

**Lives:** host-only, inside melodd, as an *input* facet of the same downstream
companion. It owns the capture buffer — a bounded, heap/disk-backed ring, the exact
structure the melodd verdict sanctioned for audio monitor-rec, now fed from the input
path. Input **monitoring** is a melodd concern and the default should be
**direct/hardware monitoring** (zero added latency — the signal goes jack→interface→
phones without touching the computer), with software monitoring through melodd offered
only when an effected monitor is wanted; either way the core is untouched and unaware
([Sweetwater — direct vs input monitoring](https://www.sweetwater.com/sweetcare/articles/direct-monitoring-vs-input-monitoring/)).
This matters because input monitoring's latency is a real, physical cost that no core
decision can hide, and the honest answer is to route around it in hardware, not to
compensate for it in the timing path.

**Does not live:** in the core, ever; on the STM32, ever; in the deterministic model,
ever; in the seed's regenerable content, ever. The STM32 refusal is not merely "no
audio ADC" — D33's RAM/flash budget is built for *regenerable* content (factory data
in flash, mutable MIDI in RAM); a multi-megabyte opaque un-regenerable blob has no
home in that budget by construction. The core never sees a sample; the seed never
claims a take.

**Time authority, restated for the input side:** the **core remains the clock**. A
capture is *stamped* with core time (the tick at which recording began) so the clip
knows where it sits in the bar and can loop to the grid — but capture **never becomes
a time source** and **never retroacts** on scheduling. This is the melodd Trojan-horse
taxonomy, transposed: (a) the audio input clock must not become authoritative over the
scheduler; (b) input onset/level detection must not fire core events on the
timing-critical path. If audio-to-MIDI (onset → note) is ever wanted, it is legitimate
**only** as a host-side gesture that enters through the existing `push_midi_in` path as
ordinary MIDI-in — quantized and deterministic *thereafter*, indistinguishable from a
key press — **never** as the audio buffer directly steering the core.

**The clip/timeline boundary — the single line to police:**

- **Legitimate (Session).** A live-captured audio clip that is fixed-length,
  launchable, scene-following, and **loops to the core clock**. Keeping an audio loop
  in sync as tempo moves is a downstream, best-effort **resample/varispeed** inside
  melodd — non-authoritative, one-way, the core neither knows nor compensates. This is
  the Loopy Pro / Ableton Session model exactly, and it holds
  ([Loopy Pro Wiki — sync](https://wiki.loopypro.com/Sync_Tricks_With_Loopy_AUv3)).
- **The DAW, refused.** Editing, cutting, comping, or **warping** the audio on a
  linear arrangement ruler; per-sample warp markers; time-stretch editing; an
  unbounded audio lane. This is the gravity D1 exists to resist, now wearing a
  microphone. The boundary is sharp: **loop-to-clock yes (resample downstream); warp,
  edit, comp, arrange no.** The instant you need true warp markers you are building
  the DAW, and "monitor-rec continuous" is the specific force that will push you there
  — its appetite for "the last N minutes of input" wants a scrolling linear timeline.
  Hold the melodd line: the input ring **crystallizes into a launchable clip**, it
  never accretes into a growing lane.

---

## Verdict — Keep / Rework / Throw

### Keep

Ordered most-important first. Each carries its axis.

**1. External audio input as a host-only, opaque, content-addressed asset the log
*references* — the one genuinely un-regenerable artifact, worth having.** —
*Intersection.* The musical case is real and distinct from melodd's render (vocal/loop
over the generative band; guitarist tracking; on-the-fly sampling), and the artifact
is architecturally containable as the "imported opaque content" category the project
already needs. It survives because it costs the core nothing and buys the one thing
render cannot: sound from the world, kept.

**2. The asset-referenced-by-log rule that preserves the seed-as-object.** —
*Engineering / Intersection.* "The seed reproduces the pointer, never the payload; the
MIDI object is whole without the audio" is the exact wording that lets an
un-regenerable take coexist with D16 without cracking it. Keep it verbatim: the log
owns MIDI+commands+seed; audio is external, referenced, out of the deterministic model.

**3. Session/clip treatment of captured audio: launchable, scene-following,
loop-to-core-clock.** — *Intersection.* Coherent with the paradigm already fixed; the
two-tier model is an asymmetry on *one* axis (provenance) and no other, so audio and
MIDI clips remain identical citizens of the same grid. This is what keeps it one
product.

**4. Direct/hardware monitoring as the default; melodd owns the bounded capture
ring.** — *Engineering.* Zero-latency monitoring routed in hardware, the core
untouched and unaware; the capture buffer is the host-only heap/disk ring the melodd
verdict already sanctioned. Honest about a physical latency cost the core cannot hide,
and it puts the cost where it belongs.

### Rework

Ordered most-severe first. What is half-right; what it must become; the axis it fails
on.

**1. The two-tier clip model.** — *Intersection.* Half-right: MIDI and audio clips
genuinely *are* different (one regenerable, one frozen). Wrong if the difference is
allowed to grow past provenance into divergent launch/scene/loop semantics — then the
session model fractures into two products. **What → toward what:** the two tiers differ
on regenerability/provenance **only**; on every launch/grid/scene axis they are one
kind. An audio-input clip is *a WAV that arrived through a jack* — reuse the imported-
opaque-content category, do not mint a second clip universe.

**2. "Monitor-rec continuous" for audio input.** — *Intersection.* Half-right: an
always-on input ring so a spontaneous take is never lost is a real, high-value idea.
Wrong: audio's inherent linearity makes "grab the last N minutes" pull toward a
scrolling linear timeline — the Arrangement audio view D1 refuses. **What → toward
what:** a bounded host-only ring in melodd that **crystallizes into a launchable
clip**, never accretes into a growing input lane. Same firewall as the melodd verdict;
this is where it is most likely to be breached.

**3. Loop-sync of captured audio to the core clock.** — *Intersection.* Half-right:
looping an audio clip to the grid is legitimate and expected. Wrong if it grows warp
markers / time-stretch editing to "correct" a drifting take. **What → toward what:**
tempo-follow is a **downstream, best-effort resample** inside melodd, non-authoritative
and one-way. Draw the line precisely: **loop-to-clock keep; warp/edit throw.** The
moment you want per-marker warp you have chosen to build the DAW.

**4. Audio-to-MIDI / onset capture (if ever wanted).** — *Engineering / Intersection.*
Half-right: turning a hummed line or a strummed rhythm into MIDI is a legitimate
musical wish. Wrong if the audio buffer steers the scheduler. **What → toward what:**
it enters **only** as a host-side gesture through `push_midi_in`, as ordinary MIDI-in,
quantized/deterministic thereafter — indistinguishable from a key press. The audio
domain never touches which core events fire or when.

### Throw away

Ordered most-severe first. Why it must die, on which axis, with the argument.

**1. Any path where the audio input clock, level, or onset becomes a time source or
retroacts on which MIDI fires.** — *Engineering; contradicts D29/D32.* This is the
melodd Trojan horse on the input side: auto-tempo from the vocal, an onset that
triggers a core event on the timing path, a capture length that nudges the bar. It
subordinates the core's total-order integer determinism to a non-deterministic float
domain and, since the STM32 has no audio input, splits the timing soul of the product
in two. Capture is a leaf, stamped by core time, never a cause of it. This reading must
die.

**2. External audio input in the core, on the STM32, in the deterministic model, or in
the seed's regenerable content.** — *Engineering; contradicts D1/D32/D33/D16.* The core
never sees a sample; the STM32 budget (D33) has no home for a multi-megabyte
un-regenerable blob by construction; the seed never claims to regenerate a take. Audio
input lives in melodd, host-only, or it does not exist.

**3. Audio editing / warping / comping on a linear arrangement timeline.** —
*Intersection; contradicts D1, the refused Reflection-1 inversion.* This is the DAW the
whole prior chain of verdicts exists to resist, now with a microphone attached. A
different, heavier, undifferentiated product that would swallow the project while the
real differentiators (reproducible generativity, NTT no-wrong-notes, voice-leading, the
reversible looper) go unbuilt. Throw it away as framing, scope, and roadmap.

**4. Promoting captured audio to authoritative project content.** — *Intersection;
contradicts D16/D17.* If the reproducible truth silently migrates from the command/MIDI
log into the recorded take, the seed-as-object thesis dies and arrangrr becomes just
another non-deterministic tool that happens to record. Imported/captured audio is
opaque attached media, referenced, never the authoritative model. (Not a rejection of
recording — a rejection of *promoting* the recording above the log.)

---

## The rule, in one line

**External audio input enters only as an opaque, content-addressed asset the log
points at but never regenerates: the seed owns every note and not one sample; capture
may follow the clock, never set it, and the core never learns a microphone exists.**

---

## Verdict (synthesis)

External audio input belongs in arrangrr — **with conditions, host-only, inside
melodd** — precisely because it is the one artifact the product cannot otherwise make:
sound from the world, kept. It survives the determinism thesis intact by refusing to
join it: the take is an external referenced asset, a pointer the seed preserves and
never dereferences, and the MIDI object stays whole and byte-exact without it — so
"seed = musical object" remains literally true, with the audio as luggage, not the
traveller. The two-tier clip model is acceptable because the asymmetry is provenance
and provenance *alone*; on every launch, scene and loop axis a captured take is just a
WAV that arrived through a jack, a citizen of the same Session grid. The line that
must not move is the same one the melodd verdict drew, now on the input side: capture
follows the core's time and never becomes it, an audio loop may sync to the clock by
downstream resampling but may never be warped or edited on an arrangement ruler, and
the instant an onset retroacts on which MIDI fires, or an input lane starts to grow, the
DAW you refused has walked in through the microphone.

---

## Sources

- [Sweetwater — Direct Monitoring vs. Input Monitoring (latency)](https://www.sweetwater.com/sweetcare/articles/direct-monitoring-vs-input-monitoring/)
- [Loopy Pro — professional looper/DAW (audio loops, sync)](https://loopypro.com/)
- [Loopy Pro Wiki — Samplers (audio clips targeted by MIDI, pitched/sliced)](https://wiki.loopypro.com/Samplers)
- [Loopy Pro Wiki — Sync Tricks (MIDI Clock / Ableton Link tempo sync)](https://wiki.loopypro.com/Sync_Tricks_With_Loopy_AUv3)
- [Lofi Monster — Live Looping in Ableton 2026 guide](https://lofimonster.com/blog/live-looping-ableton-2026-guide)
- [Attack Magazine — Generative MIDI tools in Ableton Live](https://www.attackmagazine.com/technique/tutorials/getting-started-with-ableton-lives-generative-midi-tools/)
