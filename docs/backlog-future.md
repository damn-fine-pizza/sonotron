# Future backlog

Three related directions that are judged and desirable but not yet scheduled:
splitting the arranger from the sequencer as a code boundary; the still-unbuilt
`melodd` audio work (recording, monitoring, live views); and external audio
*input* capture. Each is read against `docs/DESIGN.md` (D1–D52+) and the
"kill the word DAW" verdict (`docs/gui-and-ux.md`).
The audio sections share one discipline, stated once and extended per topic:
audio may own the *sound*, never the *time*, and the core never learns it exists.

---

## 1. arrangrr vs sequencrr — split the arranger and the sequencer?

The question: today, are the arranger and the sequencer separate in arrangrr, or
entangled? Does it make sense to cleave the project into two — `arrangrr` (the
arranger) and `sequencrr` (the sequencer) — as a module/project boundary,
independent of runtime (on a PC they could be two libs/threads/processes; on
STM32 they would be one binary regardless)? Concretely: (1) map the real seams —
arranger-only vs sequencer-only vs SHARED state/logic; (2) is a sequencer a
distinct musical object from an arranger, or two faces of one engine; (3) if
split, what is the boundary — is this the D43 name-blind-peers-wired-by-an-
orchestrator pattern applied *inside* the MIDI brain, or over-engineering;
(4) separate code-topology from runtime-topology per target (Linux-sim /
STM32H743 / SBC), respecting freestanding/no-heap/one-clock/POD-ABI; (5) name the
trap — does splitting BUY anything real, or is it interface tax on a thing that
shares clock + scheduler + chord-context anyway.

### The seam, as the code actually stands

`Engine::advance_ticks` is the whole answer. Each tick, while playing, it fires —
in this fixed order — `fire_timeline` → `fire_chord_seq` → `fire_arranger` →
`fire_arp`, then flushes the scheduler. Four PRODUCERS, one loop.

Producer-owned, disjoint state (the seam already exists at class level):

- **Step sequencer** = `Timeline` / `Track` / `Step` (`timeline/timeline.hpp`).
  Absolute authored notes on a 16th grid, per-track length (polymeter),
  mute/solo. It emits `note_on(channel, s.note)` DIRECTLY — no chord resolution.
  This is the "write" gesture, and it is THIN: no probability / ratchet / tie /
  rest / micro-timing / CC-lanes / song-mode / live-record (§12 wants all of
  these; §27 gap analysis confirms they are "entirely absent from `Step`").
  Mostly aspirational.
- **Chord sequencer** = `ChordSequencer` (`chord/chord_sequencer.hpp`). Record /
  quantize-after / loop / D28-functional-degree storage / musical transpose. The
  MATURE "sequencer" here — but it is an arranger *feeder*: on tick it calls
  `m_chords.sound(...)` AND emits `OutEvent::chord`, i.e. it PRODUCES into the
  harmonic bus that the arranger consumes. It is a harmony source, not a peer
  instrument.
- **Arranger** = `Arranger` (`arranger/`). Style material resolved through the
  NTT kernel against the live chord (`on_tick(tick, key, chord_state,
  schedule)`), sections, groove, voice-leading. Chord-driven, reactive.

Shared substrate every producer sits on (the "kernel"):

- ONE transport / clock — `m_transport`, `m_now` (D16, D27, D29).
- ONE out-scheduler — `m_scheduler`, a single min-heap in D29 total order
  `(@tick, class_priority, seq_no)` with a single monotonic `seq_no` tie-break.
- ONE note tracker (`m_tracker`, anti-stuck/panic), ONE router, ONE `ChordEngine`
  (`m_chords`) — the harmonic-context bus the chord-seq writes and the arranger
  reads.
- ONE ABI — the flat `Param` enum in `abi.hpp` (`kTrack*` step-seq, `kSeq*`
  chord-seq, `kStyle*`/`kPart*`/`kGroove` arranger, `kArp*`), one `Command` /
  `OutEvent` POD surface for all of them (D26).

So: **already separate in the data model, hardwired in the orchestration.** The
four producers share the common shape `on_tick(Tick, <context>, ScheduleFn)` and
never reference each other; the Engine hand-wires them and pins their order. That
fixed order is not incidental — the comment in `Arranger::on_tick` is explicit:
feed the chord AFTER the chord sequencer has fired this tick "so bar downbeats
resolve against the fresh chord." The tie-break order IS harmony correctness. It
is groove.

### Verdict by bucket

**Keep**

1. **The instinct that arranger and sequencer are already separable — correct.**
   *Engineering axis.* They are distinct classes with disjoint state and a shared
   `on_tick(..., ScheduleFn)` producer shape; neither references the other. The
   seam exists; it does not need to be manufactured, only named.
2. **The "shared bus" model — chord/harmonic context as the bus the arranger
   consumes and the chord-sequencer produces — is exactly the implemented
   reality.** *The intersection.* `ChordEngine` IS that bus; `fire_chord_seq`
   writes it, `fire_arranger` reads it. The mental model in the question is a
   description, not a proposal. Keep it; it is right on both axes.
3. **Separating code-topology from runtime-topology as explicit axes — the right
   lens.** *Engineering axis.* Conflating "two modules" with "two processes" is
   the classic category error; naming the two axes up front is what lets the
   runtime answer be "never split" while the code answer stays open.
4. **Making "does the split BUY anything, or is it interface tax" the deciding
   test — keep it.** *The intersection.* It is the correct knife; applied honestly
   it does most of the cutting below.

**Rework**

1. **The `sequencrr` framing itself — two symmetric products — is the wrong model;
   rework it to "one kernel + N tick-producers."** *The intersection (severe).*
   The two things are not peers. The mature "sequencer" (`ChordSequencer`) is an
   arranger FEEDER — it produces the harmonic context the arranger eats. The only
   truly time-driven, user-authored, chord-blind sequencer (`Timeline`) is the
   LEAST-built component in the tree. "Split into arrangrr + sequencrr" over-
   dignifies the thinnest part and mis-types the chord-seq's role. The honest
   topology is one MIDI brain = a **kernel** (transport + scheduler + tracker +
   router + ABI + harmonic-context bus) + a set of **tick producers** (arranger,
   step-seq, chord-seq, arp, looper-to-come). Not two products.
2. **"Apply D43 inside the MIDI brain" — half right; scale it down.** *Engineering
   axis.* The name-blind producer shape D43 describes ALREADY exists:
   `on_tick(tick, ctx, ScheduleFn) -> TickResult`, with `ScheduleFn` as the blind
   sink. Worth making explicit as a compile-time `TickProducer` concept (D32:
   compile-time polymorphism in the hot path, no vtables, no heap). But D43's FULL
   apparatus — POD-over-a-transport, process-agnostic, mutually name-blind across
   a wire — is over-engineering HERE, because these producers must share one
   clock, one scheduler and one chord bus WITHIN a single tick: they couple by
   reference, not by message. A ring buffer between the arranger and the sequencer
   would buy a decoupling that the same-tick harmonic dependency forbids. Reserve
   D43's real apparatus for melodd (the cross-chip audio peer); inside the brain,
   formalize the concept and the fire-order invariant, nothing heavier.
3. **The timing of the whole question is premature.** *The intersection.* You
   cannot draw a durable boundary around the step sequencer while its data model
   (§12: prob/ratchet/tie/rest/micro-timing/CC-lanes/song-mode/live-record) is
   still almost entirely unbuilt. A boundary drawn around an unfinished shape gets
   drawn wrong and then ossifies. Settle the `Step`/`Track` model first; the seam
   will tell you where it wants to be once the sequencer has earned its shape.
   Corollary: this also revisits D10 — the code has ALREADY diverged from "one
   Living Timeline, three gestures" into de-facto parallel engines (the arranger
   does not write into `Timeline`; it runs its own fire loop). That divergence is
   fine, but it should be acknowledged rather than left as an unstated
   contradiction.

**Throw away**

1. **Two repositories / two projects. Kill it.** *Engineering axis (most severe).*
   The ABI is deliberately ONE surface (D26); the clock is ONE (D16/D29); the
   scheduler is ONE min-heap with ONE monotonic `seq_no`. Two repos force a shared
   kernel vendored across a repo boundary plus cross-repo versioning of a POD ABI
   that exists precisely to be singular — pure tax. Worse, it endangers the
   green-from-day-one dual-target build (D3): every ABI change would become a
   two-repo lockstep migration. Zero gain on either axis; real cost on both.
   Refuse.
2. **Any runtime split (two threads / two processes) between arranger and
   sequencer, on ANY target. Kill it.** *The intersection (most severe).* D29's
   total-order determinism requires a single scheduler and a single emission
   counter; a second thread or a second scheduler destroys the goldens and breaks
   the SPSC discipline. The chord-seq → arranger order is a same-tick data
   dependency on the harmonic bus — a second thread would race it, and "the band
   resolved last bar's chord" is an audible wrong note, not a subtle bug. On
   STM32H743 it is simply outside the budget and the discipline (D32/D33). The
   producers are one tick, one clock, one heap, one thread — everywhere.

### Concrete recommendation

- **(a) Module / project boundary:** ONE repo, ONE binary. Do not create
  `sequencrr`. Re-describe the existing structure as **kernel + tick-producers**,
  not arranger-vs-sequencer. Keep `arranger/`, `timeline/`, `chord/`, `arp/` as
  the modules they already are.
- **(b) Interface, if you formalize anything:** a compile-time `TickProducer`
  concept — `on_tick(Tick, const HarmonicContext&, ScheduleFn) -> TickResult` —
  that the four producers already de-facto satisfy (verify with `static_assert`,
  D32; no vtables, no heap, no ring buffer between producers). The harmonic
  context stays a by-reference `ChordEngine`, not a POD-over-a-wire — that wire is
  melodd's (D43), not the sequencer's.
- **(c) Runtime topology per target:** one thread / one process for arranger +
  sequencer on Linux-sim, STM32H743 AND SBC alike — the sim must match the device
  or the goldens lie (D2/D3). The ONLY cross-process peer is melodd (D43), and
  audio never crosses that seam.
- **(d) Smallest first step, IF worth it:** not a split. Make the implicit
  contract explicit — extract the `TickProducer` concept, route the four `fire_*`
  through it, and lift the chord-seq → arranger ordering into a named invariant
  with a `static_assert`-backed producer registry. But do it AFTER the step
  sequencer's data model settles; before that, the correct first step is to write
  nothing and keep it monolithic.

The instinct is sound and the seam is real — but it is already in the code, and
it is not the seam the name `sequencrr` implies. What arrangrr has is not two
products straining to separate; it is one deterministic kernel with a growing set
of tick-producers that couple by reference within a single tick. Two names, one
brain.

---

## 2. `melodd` — the remaining audio work

`melodd` is arrangrr's host-only downstream audio companion (DESIGN.md D43 /
`0910`): a MIDI sink whose "device" is a synth, sitting where the ImGui client
and the per-OS MIDI backends already sit, leaving the freestanding STM32 core
byte-identical and ignorant of its very name. The governing rule is one sentence:
**`melodd` may own the sound, never the time, and the core may never learn its
name** — the clock flows core → `melodd`, one way; the only legitimate low-level
binding is the host deriving its tick source from the audio callback, invisible to
the core. The Trojan horse is the inverse and must be refused: an audio sample
clock made authoritative over the scheduler (which inverts D29 and splits the
STM32/desktop timing soul), a core `init` that learns about samples or buffers, or
audio latency compensation pushed into the core rather than handled downstream.
This section is the still-unbuilt work that sits on top of that sink.

### In-process peer path

The standalone-over-ALSA wiring is the smallest thing that makes the workstation
audible; the follow-up is an **in-process peer**, where the GUI/daemon feeds
arrangrr's `OutEvent` MIDI stream directly to a `melodd::Synth` instance in the
same process — no ALSA round-trip, no separate binary — via the orchestrator's
opaque-reference pattern (`components/orchestrator`: consume a peer through a
contract, not a concrete type). The synth is reused unchanged; only the wiring
(who calls `note_on`/`render`) moves from an ALSA MIDI parser + miniaudio device
callback to a direct in-process call from the daemon's own output sink. "Deep
bindings in init" are legitimate at that daemon↔`melodd` seam, and forbidden at
core↔`melodd`. Killing `melodd` leaves the core's MIDI playback to hardware
untouched — only the audio monitor goes silent; the one new exposure is data loss
if it dies mid-record, a host-grade robustness concern (autosave), never a
core-timing one.

### Recording and the reproducibility hierarchy

Recording and monitoring audio is the next capability beyond rendering. It forces
the reproducibility question, and the answer is a strict hierarchy: **the
authoritative reproducible object is the command/MIDI/seed log; rendered audio is
a *non-authoritative render*, a photograph of a performance — regenerable but
never bit-identical** (float non-associativity, buffer-size/block-alignment
dependence, processing-order effects mean two renders are not bit-identical). Keep
that hierarchy explicit and the seed-as-object thesis (D16/D17 — recall the exact
variation byte-for-byte next week) survives audio intact; treat rendered audio as
authoritative project content and you have quietly killed D16's payoff. Rendered
audio is a byproduct, never the authoritative model.

### The three live views

Three views, three different natures; the discipline is to keep them from
collapsing into a linear audio timeline.

- **Upcoming** = a deterministic MIDI preview of the arranger/Director's next
  moves (dream #2): a second, host-side core instance rendering the hypothetical
  against a virtual clock while the live core plays, diffed for display. **Pure
  MIDI, no audio, enabled by headless purity.** `melodd` plays no part, and this
  is the strongest of the three. Do not let audio's arrival confuse a MIDI
  capability for an audio one.
- **Recorded** = launchable **clips** in the Session grid: MIDI clips
  (reproducible, degree-relative per D28, re-harmonizable per the reversible
  looper) and, optionally, `melodd` audio clips (frozen renders / imported takes).
  Coherent with Session-view *as long as clips stay fixed-length and launchable* —
  not lanes on a linear ruler.
- **Monitor-rec continuous** = an always-on **bounded ring** that crystallizes
  into a clip on demand (retroactive capture, item #15; performance
  version-control, dream #7). The MIDI ring is core-portable and deterministic
  (the command-log ring, arrangrr's comfort zone); the audio ring is host-only, in
  `melodd`, heap/disk-backed. **The trap is here:** audio monitor-rec's appetite
  for "the last N minutes" is the one force that pulls the Session model toward the
  Arrangement audio timeline D1 refuses. Hold the line — the ring **crystallizes
  into a launchable clip**, never accretes into a growing lane.

A literal audio DAW around arrangrr — audio-track arrangement timeline, plugin
host, bundled mixer, comping — is thrown away as framing, scope, and roadmap: it
is a different, heavier, undifferentiated product that would swallow the project
while the real differentiators (reproducible generativity, NTT no-wrong-notes,
voice-leading, the reversible looper) go unbuilt.

### Sequencing law

The full audio engine (sampler/SoundFont, FX, mixer, disk streaming for record) is
enormous and is a gravity well for the project's own energy. It is **downstream of
the MIDI musical core maturing** — scale-degree melody, voice-leading, the
reversible looper, scenes. Build it before those land and you will have a
beautiful synth attached to a band that still cannot walk its bassline.

---

## 3. External audio *input* — the one un-regenerable artifact

External audio *capture* — a vocal, a guitar DI, a mic, a line-in — is the
opposite polarity to `melodd`'s render. A render is a regenerable photograph of a
performance the seed already authored; a captured signal is one the seed *never
contained* and can *never* reproduce. The entire novelty and the entire danger is
that single word: **un-regenerable**. It is judged as a *facet of `melodd`* — the
same host-only, downstream, clock-slaved citizen, now also owning an input path —
and it extends `melodd`'s rule to the input side: capture follows the core's time
and never becomes it; the core never learns a microphone exists.

The core stays exactly what D1/D2/D32/D33 lock it to be: MIDI-only, freestanding,
no-heap, bounded, dual-target, STM32-anchored, authoritative on time via injected
ticks. The session/clip paradigm is fixed: Session-view launchable clips **yes**,
a linear Arrangement *audio* timeline **no**.

### Is it worth it? — the musical case

Yes, precisely *because* it is the one thing `melodd`'s render can never be:
external input is a photograph of something the log **never held** — the only
genuinely irreplaceable artifact the product can produce. That asymmetry is the
entire reason the feature has musical worth. The real use-cases:

- **The singer/looper over the generative band (personas 1 & 4).** Sing a hook,
  loop it, layer harmonies over an arranger following your chords and a Director
  building energy underneath. This is the canonical live-looping act (the Boss RC
  line, Ableton's Looper, Loopy Pro — audio loops launched and layered in a
  Session-style grid synced to a host tempo), and generative-MIDI-plus-live-voice
  is a real current performance idiom.
- **The instrumentalist tracking over the arranger (persona 2).** Play a guitar or
  bass take over the generated bed to capture a phrase while the idea is hot. Here
  audio is scaffolding for composition, not the final artifact.
- **On-the-fly sampling (the Koala idiom).** Grab a sound from the world and play
  it back rhythmically, sliced or pitched, triggered by the clock. This is subtly
  *different*: the captured audio becomes an **instrument**, not a **take** —
  closer to imported content played by MIDI than to a recorded performance. Loopy
  Pro models exactly this (audio clips targeted by MIDI, played as pitched notes or
  slices).
- **"Monitor-rec continuous", extended to input.** The always-on
  retroactive-capture ring (item #15 / dream #7), now fed from the input jack, so a
  spontaneous vocal or riff is *already recorded* the instant you decide it was
  good. This is the strongest and the most dangerous of the four.

### The two-tier clip model

A recorded vocal take is not regenerable from a seed — float, converters, the
world itself guarantee it. This forces a **two-tier clip model**, which is
acceptable on one precise condition: the two tiers must differ on exactly **one**
axis and no other.

- **MIDI clip** — degree-relative (D28/D39), re-harmonizable, re-voiceable (D41),
  transposable, **regenerable from the seed byte-exact**. A *formula*.
- **Audio-input clip** — time-referenced, opaque, frozen, un-transposable in any
  musical sense, **not regenerable** — the seed can only *point at* it. A *pasted
  value*.

The asymmetry is provenance/regenerability, and *only* that. It does **not** extend
to launch semantics, scene-following, loop behaviour, or grid placement — on those
axes the two clip kinds are identical citizens of the same Session grid. The model
already *must* admit opaque imported content — importing a WAV is a real, wanted
feature — and a live-captured take **is just a WAV that arrived through a jack
instead of a file dialog**. Seen that way, input capture introduces no new category
at all: it reuses the "imported opaque asset" category the project needs
regardless. The model breaks unity **only if** you demand that *every* clip be
re-harmonizable — which was never the promise. Get it wrong (two divergent clip
products with different launch rules) and you have quietly built two apps.

### The precise rule that saves the seed-as-object

**The log is the single authoritative reproducible object. External audio is never
*part* of the log; it is an external asset the log *references* by a content id.**
The log records the *command*: "at command-time T, capture-clip #K was created,
content-addressed asset A (hash H), anchored at tick τ, length L, routed to slot
S". Replaying the log reproduces every MIDI/command/seed byte the arranger ever
authored, **and re-points** clip #K at asset A. It does **not** regenerate A's
samples — that is impossible and the model never claims it. The asset is opaque,
content-addressed, host-side, heap/disk-backed, entirely **outside** the
deterministic model, exactly as an imported WAV would be.

Three consequences, each load-bearing:

1. **The seed reproduces the pointer, never the payload.** The seed reproduces
   everything it authored (every note, every command, every random draw); imported
   content it merely points at, byte-preserving the reference, never the bytes.
   D16's recall guarantee survives intact: the MIDI performance is byte-identical
   next week; the audio take is attached, not regenerated.
2. **The MIDI object is whole without the audio.** A seed shared *without* its asset
   blobs still replays the entire MIDI performance perfectly — the audio clips are
   simply absent/silent, precisely as a project with a missing sample file behaves.
   The deterministic musical object does not *depend* on the opaque attachment. The
   audio is luggage, not the traveller.
3. **The opaque asset must never be a *cause*.** The existence, timing, level or
   content of a captured clip must never change **which MIDI events fire, or when**.
   The moment a recorded take's onset retroacts on the scheduler — auto-tempo from
   the vocal, an onset that triggers a core event, a capture whose length nudges the
   bar — the Trojan horse has walked in through the input jack. Audio capture is a
   **leaf**: downstream, referenced, stamped by core time, never a source of it.

### Where it lives, and the clip/timeline boundary

**Lives:** host-only, inside `melodd`, as an *input* facet of the same downstream
companion, owning a bounded heap/disk-backed capture ring. Input **monitoring** is
a `melodd` concern and the default should be **direct/hardware monitoring** (zero
added latency — the signal goes jack→interface→phones without touching the
computer), with software monitoring through `melodd` offered only when an effected
monitor is wanted; either way the core is untouched and unaware. Input monitoring
latency is a real physical cost no core decision can hide, and the honest answer is
to route around it in hardware, not compensate for it in the timing path.

**Does not live:** in the core, ever; on the STM32, ever; in the deterministic
model, ever; in the seed's regenerable content, ever. The STM32 refusal is not
merely "no audio ADC" — D33's RAM/flash budget is built for *regenerable* content
(factory data in flash, mutable MIDI in RAM); a multi-megabyte opaque
un-regenerable blob has no home in that budget by construction.

**Time authority:** the **core remains the clock**. A capture is *stamped* with
core time (the tick at which recording began) so the clip knows where it sits in
the bar and can loop to the grid — but capture **never becomes a time source** and
**never retroacts** on scheduling. If audio-to-MIDI (onset → note) is ever wanted,
it is legitimate **only** as a host-side gesture that enters through the existing
`push_midi_in` path as ordinary MIDI-in — quantized and deterministic *thereafter*,
indistinguishable from a key press — **never** as the audio buffer directly
steering the core.

**The single line to police:**

- **Legitimate (Session).** A live-captured audio clip that is fixed-length,
  launchable, scene-following, and **loops to the core clock**. Keeping an audio
  loop in sync as tempo moves is a downstream, best-effort **resample/varispeed**
  inside `melodd` — non-authoritative, one-way, the core neither knows nor
  compensates. This is the Loopy Pro / Ableton Session model exactly.
- **The DAW, refused.** Editing, cutting, comping, or **warping** the audio on a
  linear arrangement ruler; per-sample warp markers; time-stretch editing; an
  unbounded audio lane. The boundary is sharp: **loop-to-clock yes (resample
  downstream); warp, edit, comp, arrange no.** The instant you need true warp
  markers you are building the DAW, and "monitor-rec continuous" is the specific
  force that will push you there — its appetite for "the last N minutes of input"
  wants a scrolling linear timeline. The input ring **crystallizes into a
  launchable clip**; it never accretes into a growing lane.

### The rule, in one line

**External audio input enters only as an opaque, content-addressed asset the log
points at but never regenerates: the seed owns every note and not one sample;
capture may follow the clock, never set it, and the core never learns a microphone
exists.**

---

## Sources

- [Ableton — Synchronizing with Link, Tempo Follower, and MIDI](https://www.ableton.com/en/manual/synchronizing-with-link-tempo-follower-and-midi/)
- [admiralbumblebee — DAW v DAW: Plugin Automation (non-determinism across hosts)](https://www.admiralbumblebee.com/music/2019/06/22/Daw-V-Daw-Automation-Part-4.html)
- [William Ashley — Fixed vs Variable Buffer Processing in Real-Time Audio DSP](https://medium.com/@12264447666.williamashley/fixed-vs-variable-buffer-processing-in-real-time-audio-dsp-performance-determinism-and-66da78390b0f)
- [Sweetwater — Direct Monitoring vs. Input Monitoring (latency)](https://www.sweetwater.com/sweetcare/articles/direct-monitoring-vs-input-monitoring/)
- [Loopy Pro — professional looper/DAW (audio loops, sync)](https://loopypro.com/)
- [Loopy Pro Wiki — Samplers (audio clips targeted by MIDI, pitched/sliced)](https://wiki.loopypro.com/Samplers)
- [Loopy Pro Wiki — Sync Tricks (MIDI Clock / Ableton Link tempo sync)](https://wiki.loopypro.com/Sync_Tricks_With_Loopy_AUv3)
- [Lofi Monster — Live Looping in Ableton 2026 guide](https://lofimonster.com/blog/live-looping-ableton-2026-guide)
- [Attack Magazine — Generative MIDI tools in Ableton Live](https://www.attackmagazine.com/technique/tutorials/getting-started-with-ableton-lives-generative-midi-tools/)
