# Reflection — `melodd`: an audio companion around arrangrr, without becoming a DAW

Status: critique / verdict (Prospero). Not a locked decision. Read against
`docs/DESIGN.md` (D1–D42), `docs/reflections/live-daw-around-arrangrr-ux.md`
(the "kill the word DAW" verdict and its three firewalls), and
`docs/reflections/hybrid-arranger-gap-analysis.md`. Built *on top of* those two
verdicts — not repeated here, assumed. This judges a **direction**, not code.

---

## Assumptions (declared, not blocking)

The brief said "proceed without waiting". These are the assumptions the judgment
rests on; if one is wrong, the entry that depends on it is the one to revisit.

1. **`melodd` = the soft-synth sink of assumption #4 grown into a real audio
   engine.** In the UX reflection the only sanctioned "sound" was an *optional*
   host-side soft-synth sink, degradable to nothing, never on the timing path,
   never in the core/STM32. `melodd` is that sink promoted to a component that can
   *render, record and monitor* audio. I judge it as that, not as a new audio
   subsystem grafted somewhere new.
2. **The core stays exactly what D1/D2/D32/D33 lock it to be:** MIDI-only,
   freestanding, no-heap, dual-target, STM32-anchored, authoritative on time via
   injected ticks. Nothing in this reflection is allowed to touch that, and every
   verdict below is measured against whether the proposal keeps that promise.
3. **"low-level timing binding" and "deeper bindings in init"** are read as *host*
   wiring between the host daemon and `melodd`, not as a request to link `melodd`
   into the core or teach the core about audio. Where the phrase is read the other
   way, it is named as the trap.
4. **"an evolution of a DAW"** is read as the user's shorthand for ambition
   ("also handle audio"), **not** as a literal target (audio-track arrangement
   timeline, plugin host, mixing surface). If a literal audio DAW is actually
   wanted, most of this verdict flips — and the flip is exactly the Reflection-1
   inversion already refused. This is the pivot of the whole document; see the
   DAW-question below.
5. **Two host consumers already exist and are precedent:** the ImGui client (D38)
   and the per-OS MIDI I/O in the host daemon. `melodd` is judged as a *third*
   host-only citizen of the same adapter boundary, not as a special case.

What I could **not** verify and did not bluff: the exact latency/robustness
numbers of any concrete audio backend (not measured — I do not benchmark); the
final hardware; and whether the user intends external *audio input* (vocal/guitar)
capture or only internal synth rendering — I judge both and flag where the answer
changes the verdict (monitor-rec, below).

---

## The DAW-question, without discounts

**Does the "not a DAW / MIDI-only (D1)" verdict become obsolete with `melodd`?**

No. It survives, and it becomes *more* load-bearing, not less — for two reasons
that must be kept separate because they are two different claims.

**D1 was always scoped to the core, and stays true by re-reading, not by yielding.**
D1 says *the core* never produces audio: "a self-contained MIDI music machine that
drives external synths". A host-only sister that *consumes* the core's MIDI and
clock and turns them into sound does not make the core produce audio, any more than
an external Nord or a bundled SoundFont player does. `melodd` is, precisely, an
external synth that happens to live in the same repository. So the honest statement
of D1 evolves from "the product has no audio" to its true form: **the core never
has audio; audio exists only as a host-only downstream sink/companion the core is
totally ignorant of** — the same status the GUI already has under D38. D1 does not
fall; it is stated correctly for the first time.

**"Not a DAW" survives *harder*, because audio is the gravity that the prior
verdict named.** The UX reflection killed the word "DAW" not out of pedantry but
because the word imports a gravity — linear audio timeline, audio tracks, plugin
host, mixing — that D1/D2 spent the whole design resisting, and that pulled
Reflection-1 toward "validate as a desktop app/plugin first". Audio is *exactly*
the mass that generates that gravity. The user's own phrasing — "make arrangrr an
evolution of a DAW" — is the sentence I flagged. So this reflection does not
weaken the prior verdict; it is the first real test of it. `melodd` can exist.
"arrangrr is an evolution of a DAW" cannot be the framing, because the moment you
measure `melodd` against a DAW's audio-arrangement surface you will start building
the linear audio timeline and the plugin host, and that is the refused inversion
wearing headphones.

### The defensible line vs. the Trojan horse — drawn once, sharply

The whole judgment turns on a single technical fact, and it is an intersection
fact, not a taste: **the instant audio is real, the audio sample clock is the
natural timing master.** Sample-accurate MIDI↔audio alignment *requires* MIDI
events to be placed onto the audio buffer timeline — i.e. MIDI slaves to the audio
callback. And audio rendering is **not** deterministic: float non-associativity,
buffer-size / block-alignment dependence, and processing-order effects mean two
renders of the same input are not bit-identical
([admiralbumblebee — DAW automation](https://www.admiralbumblebee.com/music/2019/06/22/Daw-V-Daw-Automation-Part-4.html),
[William Ashley — fixed vs variable buffering](https://medium.com/@12264447666.williamashley/fixed-vs-variable-buffer-processing-in-real-time-audio-dsp-performance-determinism-and-66da78390b0f)).
This is the opposite polarity to arrangrr's core, which is authoritative on time,
injected-tick, integer, total-order deterministic (D29/D32).

From that single fact the line falls out:

- **Legitimate (the sister-component).** The clock flows **core → `melodd`**,
  never back. `melodd` is a *downstream MIDI sink* that consumes the core's
  `OutEvent` stream and clock exactly as an external synth consumes MIDI — it does
  its own best-effort sub-buffer placement of incoming events onto its audio grid,
  DOWNSTREAM, within one buffer of latency, and the core neither knows nor
  compensates. The core stays authoritative and ignorant. **One legitimate
  low-level binding exists:** on the host, the *daemon's tick source* may be
  *derived from* the audio device callback (the audio clock advances host time).
  That is legitimate precisely because the core cannot tell — its injected-tick
  contract is unchanged; whether a tick arrives from a system timer or an audio
  callback is a host secret. This is the Ableton-Link-style relationship (share a
  time *reference*, each engine renders on its own) — not the ReWire-style
  shared-buffer marriage ([Ableton — Link/MIDI sync](https://www.ableton.com/en/manual/synchronizing-with-link-tempo-follower-and-midi/)).

- **The Trojan horse (the trap the brief named).** Any of these, and the sin is
  committed: (a) `melodd`'s audio sample clock becomes authoritative *over the
  core's scheduler* — the core chasing the audio buffer inverts D29 and, since
  STM32 has no audio clock, splits the timing *soul* of the product in two;
  (b) the core's data model or `init` learns about samples, buffers, latency
  compensation-in-samples, or `melodd` at all — this is the "deep bindings in init"
  read the wrong way, and it breaks core ignorance exactly as linking the GUI into
  the core would; (c) audio latency compensation is pushed *into* the core rather
  than handled downstream in `melodd`; (d) any non-determinism from the float audio
  domain feeds back into *which MIDI events fire when*. Each of these drags the
  buffer/latency/allocation/DAW-gravity the brief warned about into or beside the
  core's timing path — the precise thing D1/D2/D32 were built to prevent.

The rule, in one line: **`melodd` may be as authoritative as it likes over *sound*;
it may never be authoritative over *time*, and the core may never learn its name.**

---

## Keep

Ordered most-important first. Each carries its axis.

**1. `melodd` as a host-only downstream MIDI *sink* that renders/records audio —
i.e. the soft-synth sink of assumption #4, grown up.** — *Intersection.* This is
the whole defensible form and it costs the core nothing: `melodd` is a sibling of
the ALSA/CoreMIDI output backend whose "device" is a synth instead of a DIN port.
It consumes `OutEvent` + clock, it is degradable to silence, and it is
architecturally incapable of contaminating the core because it sits where the
existing sink already sits. The musical payoff is real (see below); the
engineering cost to the core is zero. This survives because it is not a new axis —
it is D1's external synth, moved in-house.

**2. D1, re-scoped to the core, made honest.** — *Engineering axis.* The value to
keep is the *re-statement*: "the core never has audio; audio is a host-only sister
the core is ignorant of." Keeping this precise wording is what lets `melodd` exist
without any locked decision yielding. The device/STM32 target is byte-identical
with or without `melodd`; only the desktop grows a consumer. Keep the invariant
that the bifurcation lives at the adapter boundary, never in the core.

**3. The "upcoming" live view — as a MIDI feature, not an audio one.** — *Intersection.*
Deterministic preview of what the arranger/Director is about to do is dream #2 of
the UX reflection (a second, host-side core instance rendering the hypothetical
against a virtual clock). It is enabled entirely by headless purity and needs *no*
audio. Keep it, and keep it clearly labelled as MIDI: it is the strongest of the
three "live views" and `melodd` is irrelevant to it. Do not let audio's arrival
confuse a MIDI capability for an audio one.

**4. The musical case for audio at all.** — *Musical axis.* Being able to *record
and compose* the sound — not merely hear it — genuinely closes the "I need external
gear to hear anything" gap for the solo performer (persona 1), the producer mining
ideas (persona 2) and the generative/ambient musician (persona 4). A self-contained
instrument that makes sound is a real product, not a vanity. This ambition is worth
keeping — under every discipline below.

---

## Rework

Ordered most-severe first. What is half-right; what it must become; the axis it
fails on.

**1. "an evolution of a DAW" as the framing.** — *Intersection; the framing sin.*
Half-right: the ambition (also handle audio) is legitimate. Wrong: the *word*
re-imports the exact gravity the prior verdict spent itself refusing, and with
audio present that gravity is no longer theoretical. **What → toward what:** name it
what it is — *a generative MIDI live-instrument / arranger workstation with an
optional audio companion (`melodd`)*. The measuring stick is Genos + Band-in-a-Box
+ a modular generative rig, never Ableton's audio-arrangement surface. This is not
cosmetic: it decides whether you build the reproducible generative arranger nobody
else has, or start resenting the missing audio-track lane you were never supposed
to want.

**2. "bind them at low level for timing / deeper bindings in init".** — *Engineering
axis; the timing-authority sin.* Half-right: `melodd` *does* need a low-latency,
sample-placeable event stream and a shared time reference — that is real and
sanctioned. Wrong if it means the core chases `melodd`'s audio clock, or the core's
`init`/data model learns about `melodd`. **What → toward what:** the only binding is
host-side — the daemon may derive its tick source from the audio callback and wires
`melodd` to the core's `OutEvent` ring at daemon-init. The core's injected-tick
contract is untouched and the core never sees the word `melodd`. Clock flows
core → `melodd`, one way. Fail this and you invert D29 and split the STM32/desktop
timing soul.

**3. Reproducibility: the seed-as-musical-object under non-deterministic audio.** —
*Intersection.* Half-right: the recall guarantee (dream #1, D16/D17 — "recall the
exact variation byte-for-byte next week") is real. Wrong to assume it extends to
rendered audio: audio is not bit-reproducible (float / buffer / order). **What →
toward what:** keep the hierarchy explicit — **the authoritative reproducible
object is the command/MIDI/seed log; audio is a *non-authoritative render*, a
photograph of a performance, regenerable but never bit-identical.** External audio
*input* (a vocal, a guitar) is the one genuinely un-regenerable artifact — treat it
as *imported opaque content*, outside the deterministic model, never as something
the seed can reproduce. Get this hierarchy right and the seed thesis survives audio
intact; get it wrong (audio treated as authoritative content) and you have quietly
killed D16's payoff.

**4. The "recorded" and "monitor-rec continuous" live views — must stay Session,
must not grow an Arrangement audio timeline.** — *Intersection.* Half-right: a
recorded MIDI clip (reproducible) and a recorded audio clip (frozen render) can
both live in the Session/clip grid we already fixed. "Monitor-rec continuous" maps
to retroactive capture (item #15, a bounded always-on ring) + performance
version-control (dream #7). Wrong: audio recording has an inherent linearity (a
take has a start and a length in samples), and "grab the last N minutes of audio"
naturally wants a scrolling *linear* timeline — which is exactly the Arrangement
audio view D1 refuses. **What → toward what:** the audio ring must **crystallize
into a launchable clip**, not accrete into an unbounded arrangement lane. On MIDI,
monitor-rec is core-portable and deterministic (the command-log ring, arrangrr's
comfort zone). On audio, it is a host-only bounded ring in `melodd` (heap/disk, big
buffers) — legitimate there, impossible in the core. Keep the session/clip
paradigm; the moment an unbounded growing audio timeline appears, the DAW you
refused has arrived.

---

## Throw away

Ordered most-severe first. Why it must die, on which axis, with the argument.

**1. Any timing arrangement where the audio sample clock is authoritative over the
core's scheduler.** — *Engineering axis; contradicts D29/D32/D33.* If the core must
chase `melodd`'s audio buffer to stay sample-accurate, then (a) the core's
total-order integer determinism is subordinated to a non-deterministic float
domain, and (b) since STM32 has no audio clock, the desktop and the device no
longer share a timing master — they become two products with two souls. This is the
literal reading of "bind them at low level for timing", and it is the one reading
that must die. `melodd` slaves to the core, forever, or it does not ship.

**2. Any binding that makes the core aware of `melodd`, samples, or audio buffers
("deep bindings in init").** — *Engineering axis; contradicts D32/D38's boundary
logic.* The core is ignorant of the GUI *by construction*; it must be equally
ignorant of `melodd`. A core `init` that pre-sizes audio pools, an `Event` that
carries a sample offset, a scheduler that knows about latency-in-samples — each
poisons the freestanding, no-heap, STM32-portable core for a host-only concern.
The core's ABI grows for musical reasons, never for audio pixels or audio samples.

**3. A literal audio DAW around arrangrr — audio-track arrangement timeline, plugin
host, bundled mixer, comping.** — *Intersection; the refused Reflection-1 inversion,
now with audio.* This is the gravity the whole prior verdict exists to resist. It is
a *different product*, heavier and undifferentiated, and it would swallow the
project whole while the actual differentiators (reproducible generativity, NTT
no-wrong-notes, voice-leading, the reversible looper) go unbuilt. Ableton already
wins that ground; arrangrr wins the ground nobody else holds only by not going
there. Throw it away as framing, as scope, and as roadmap.

**4. Audio treated as authoritative, un-regenerable *project* content by default.**
— *Intersection; contradicts D16/D17.* If the reproducible truth silently migrates
from the command/MIDI log into rendered audio, the seed-as-object thesis dies and
arrangrr becomes just another non-deterministic tool. Rendered audio is a byproduct;
imported external audio is opaque attached media. Neither is ever the authoritative
model. (Not a rejection of recording audio — a rejection of *promoting* it above
the deterministic log.)

*One note on ordering:* #3 is the most dangerous *strategically* (it can consume the
project), #1 the most dangerous *architecturally* (it silently inverts D29). Both
are fatal; they fail on different axes, which is why both are listed at the top.

---

## The live view, made concrete in arrangrr terms

Three views, three different natures — and the discipline is to keep them from
collapsing into a linear audio timeline.

- **Upcoming** = a deterministic MIDI preview of the arranger/Director's next moves
  (dream #2): a second, host-side core instance rendering the hypothetical against
  a virtual clock while the live core plays, diffed for display. **Pure MIDI, no
  audio, enabled by headless purity.** `melodd` plays no part. This is coherent
  with the session paradigm and is the strongest of the three.

- **Recorded** = launchable **clips** in the Session grid: MIDI clips (reproducible,
  degree-relative per D28, re-harmonizable per the reversible looper) and,
  optionally, `melodd` audio clips (frozen renders / imported takes). Coherent with
  Session-view *as long as clips stay fixed-length and launchable* — not lanes on a
  linear ruler.

- **Monitor-rec continuous** = an always-on **bounded ring** that crystallizes into
  a clip on demand (retroactive capture, item #15; performance version-control,
  dream #7). MIDI ring = core-portable, deterministic, arrangrr's comfort zone.
  Audio ring = host-only, in `melodd`, heap/disk-backed. **The trap is here:** audio
  monitor-rec's appetite for "the last N minutes" is the one force that pulls the
  Session model toward the Arrangement audio timeline you buried. Hold the line —
  ring → clip, never ring → growing lane.

Where these stay coherent with the fixed session/clip paradigm: all three, *as
clips*. Where they push you back toward the linear timeline you threw away: audio
monitor-rec, specifically, and only if the ring is allowed to become an unbounded
arrangement lane. That is the single boundary to police.

---

## The proposed architecture — if it holds (it does, in one shape only)

```
   arrangrr core  (MIDI-only, freestanding, no-heap, STM32-anchored,
      |            authoritative on time via injected ticks — D32/D33)
      |  POD OutEvent ring (SPSC)          host-only JSONL (D26/D38)
      v                                          v
   HOST DAEMON  (links the core, owns per-OS MIDI I/O)          ImGui CLIENT
      |  \___ per-OS MIDI out (ALSA/CoreMIDI/WinMM) ── hardware   (separate
      |  \___ melodd  (audio engine: render / record / monitor)   process,
      |         ^ consumes OutEvent + clock as a downstream sink;  pure mirror
      |         ^ does its own sub-buffer placement, DOWNSTREAM;   over socket,
      |         ^ may *provide* the daemon's derived tick source   never audio,
      |         ^ but never commands the core                      never MIDI)
```

The answers to the four architectural questions:

- **Is `melodd` a third consumer of the adapter?** No — not of the *JSONL* control
  stream (that is text, host-side, non-realtime; it is the ImGui client's channel
  for state mirroring). `melodd` is a **sibling of the MIDI output backend**,
  consuming the realtime POD `OutEvent` ring the ALSA/CoreMIDI sink already
  consumes. Framing it as a MIDI *sink* (device = synth) is what keeps it clean.

- **Who is authoritative on time?** The core, via injected ticks — always. On the
  host, the tick *source* may be derived from `melodd`'s audio callback (legitimate,
  invisible to the core). `melodd` never tells the core when to fire; the core
  fires, `melodd` renders and places events sub-buffer, downstream.

- **Process or module?** `melodd` is a **module linked into the host daemon** (the
  process that already links the core and owns MIDI), running on the audio thread,
  pulling from the core's output ring — *not* the ImGui client, *not* a third
  separate process (IPC on the audio path is latency you don't want on a monitor).
  The ImGui client stays a separate pure-mirror process (D38). "Deep bindings in
  init" are legitimate **here**, at daemon↔`melodd` wiring — and forbidden at
  core↔`melodd`.

- **Does "kill the GUI never touches playback" survive with audio?** Yes, refined
  into two independent invariants. Killing the **ImGui client**: playback and audio
  untouched (D38). Killing **`melodd`**: the core's MIDI playback to hardware is
  untouched — only the audio monitor goes silent (degradable to nothing,
  assumption #4). The one new exposure is *data loss* if `melodd` dies mid-record —
  a host-grade robustness concern (autosave), never a core-timing concern. The core
  remains alive, deterministic, and MIDI-authoritative regardless of `melodd`'s
  life. The invariant holds; it simply now has two subjects instead of one.

- **Does the core stay totally ignorant of `melodd`?** Yes — *if and only if*
  `melodd` is a downstream `OutEvent` sink. It holds under exactly the same logic
  that keeps the core ignorant of the GUI. It breaks the instant "deeper bindings in
  init" are read as core↔`melodd` rather than daemon↔`melodd`.

**Focus is the unlisted cost.** An audio engine (sampler/SoundFont, FX, mixer, disk
streaming for record) is enormous, and it is a gravity well for the project's own
energy. Its sequencing law is the same as D37's and the GUI's: `melodd` is
**downstream of the MIDI musical core maturing** — scale-degree melody, voice-
leading, the reversible looper, scenes (the gap-analysis Rework list). Build
`melodd` before those land and you will have a beautiful synth attached to a band
that still cannot walk its bassline. The soft-synth *sink* (assumption #4, minimal)
can exist early to make the desktop audible; the full `melodd` engine waits.

---

## Verdict

`melodd` is defensible — as the assumption-#4 soft-synth sink grown into a
host-only, downstream, clock-slaved audio companion: a MIDI sink whose device is a
synth, sitting exactly where the GUI and the MIDI backends already sit, leaving the
freestanding STM32 core byte-identical and ignorant of its very name. D1 does not
become obsolete; it is finally *stated correctly* — the **core** never has audio,
and audio lives only as a downstream sister the core cannot see. The "not a DAW"
verdict does not merely survive; it becomes the load-bearing wall, because audio is
the exact gravity that verdict exists to resist, and "an evolution of a DAW" is that
gravity speaking. The line is one sentence: **`melodd` may own the sound, never the
time, and the core may never learn its name** — clock flows core → `melodd`, one
way; the only legitimate low-level binding is the host deriving its tick source from
the audio callback, invisible to the core. The Trojan horse is the inverse: an audio
sample clock made authoritative over the scheduler (which inverts D29 and splits the
STM32/desktop soul), a core `init` that learns about samples, or an audio monitor-rec
that grows into the linear Arrangement timeline. Keep the reproducible truth in the
command/MIDI/seed log and let audio be a non-authoritative render, and the
seed-as-object thesis survives intact. Do all of that, and `melodd` is not a DAW and
not a betrayal — it is arrangrr's external synth finally moved in-house, and the
first generative arranger whose sound you can keep without ever surrendering the
determinism that makes it worth keeping.

---

## Sources

- [Ableton — Synchronizing with Link, Tempo Follower, and MIDI](https://www.ableton.com/en/manual/synchronizing-with-link-tempo-follower-and-midi/)
- [MIDI Specification — Syncing Sequence Playback](http://midi.teragonaudio.com/tech/midispec/seq.htm)
- [admiralbumblebee — DAW v DAW: Plugin Automation (non-determinism across hosts)](https://www.admiralbumblebee.com/music/2019/06/22/Daw-V-Daw-Automation-Part-4.html)
- [William Ashley — Fixed vs Variable Buffer Processing in Real-Time Audio DSP](https://medium.com/@12264447666.williamashley/fixed-vs-variable-buffer-processing-in-real-time-audio-dsp-performance-determinism-and-66da78390b0f)
- [KVR — Consistent MIDI clock across multiple grooveboxes](https://www.kvraudio.com/forum/viewtopic.php?t=554430)
- [Polyend Play — sample + MIDI groovebox (audio + MIDI tracks reference)](https://vintageking.com/polyend-play-sample-and-midi-based-groovebox)
