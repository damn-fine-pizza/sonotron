# Reflection — A live generative instrument *around* arrangrr: does it hold, and how not to be clunky

Status: critique / verdict (Prospero). Not a locked decision. Read against
`docs/DESIGN.md` (D1–D38), `docs/reflections/hybrid-arranger-gap-analysis.md`,
and the core/host tree as of branch `host-tui-h3`. Built *on top of* the prior
gap-analysis verdict — it is not repeated here, it is assumed.

This reflection judges a **direction**, not code. Where it touches the pattern
model, voicing, looper, arp and per-port scheduling, it defers to the prior
gap-analysis verdict rather than re-litigating it.

> **Status pointer (2026-07-11).** Parts of this direction have since been decided and
> built: the pure-client-over-UDS boundary and the ImGui + GLFW toolkit explored below are
> now settled and grounded — see `docs/design/gui-contract-map.md` §1 and
> `docs/design/ux-workstation.md`. The `arrangrr_core` name used here predates the repo
> rename; the core library target is now simply **`arrangrr`**.

---

## Assumptions / open questions (declared, not blocking)

The brief said "do not wait for answers". These are the assumptions the judgment
rests on; if one is wrong, the entry that depends on it is the one to revisit.

1. **"DAW" is shorthand, not a literal target.** I read "a kind of DAW/live-
   instrument around arrangrr" as *a desktop generative-arranger / live-instrument
   workstation*, **not** an audio DAW. D1 (never audio) and D2 (STM32 primary) are
   untouched. If a literal DAW (audio tracks, linear audio timeline, plugin host)
   were actually wanted, most of the "makes sense?" verdict would flip — see the
   category-error warning in Deliverable 1.
2. **The GUI is the D38 Dear ImGui client**, a separate process, a pure client of
   the core over the host UDS-JSONL adapter. The brief restates exactly D38; I take
   D38 as the settled shape and judge *around* it. This supersedes the prior gap-
   analysis's web/Tauri lean — the user has since chosen ImGui (D38), and I judge
   that choice as locked, not reopened.
3. **"Make the machine play itself" = the D37 Generative Director**, a deterministic
   parameter-trajectory engine, not AI. The brief's autoplay/co-pilot language maps
   one-to-one onto D37. I do not treat "autoplay" as a request for a generative
   *model*.
4. **"audio-out only as a MIDI sink"** means an *optional* local soft-synth in the
   host/GUI layer so a solo user hears something without external gear — never a
   core concern, never on the timing-authoritative path.
5. Cross-platform MIDI I/O lives in the **host daemon** (the process that links the
   core), not in the ImGui client. The client speaks only the socket. Verified
   against D38's boundary language, not against a per-OS backend that does not yet
   exist in the tree.

---

## On the table

The direction: grow a cross-platform (Windows/Linux/macOS) desktop **live
instrument / generative arranger** *around* arrangrr — sequencer + arranger + MIDI
looper + arpeggiator — whose signature gesture is *the machine playing itself*
(autoplay / generative / co-pilot). The GUI is Dear ImGui, a pure client of the
core over the host adapter; arrangrr the core **never** depends on the GUI, never
ships the GUI to STM32, never sits the GUI on the realtime/MIDI path. The core
stays freestanding, no-heap, dual-target, STM32-anchored (D32/D33). The question is
five-fold: does this hold on both axes; who really uses it; how it avoids becoming
clunky like Zrythm/Genos/Band-in-a-Box; what dream features arrangrr is uniquely
positioned to finally do right; and twenty prioritized, *non-obvious* additions
that exploit what arrangrr specifically is (NTT, role-arranger, generativity,
freestanding core, piano→chord, modularity) — explicitly **not** the trivial DAW
absences.

---

## Deliverable 1 — Does it make sense? (verdict on both axes)

**Yes — with one category error to kill on sight, and one sequencing law to obey.**

### It holds — engineering axis

The direction does not fight the architecture; it *cashes it out*. The boundary the
brief insists on ("arrangrr never depends on the GUI") is not a new constraint to
impose — it is D26 + D38 already paid for. The core emits POD `OutEvent`, the host
already serializes to JSONL, and the adapter is one thin host-only transport. A GUI
built this way is *architecturally incapable* of contaminating the core: it cannot
link it, cannot parse in it, cannot sit on the timing path. That is the strongest
possible form of the boundary — enforced by construction, not by discipline. On
this axis the direction is not merely allowed, it is the intended payoff of D17/D26/
D30/D38. **The one non-negotiable:** per-OS MIDI (ALSA/CoreMIDI/WinMM) lives in the
**host daemon**, never in the ImGui client. Put MIDI in the GUI and you have re-
invented the jitter-and-single-point-of-failure the boundary exists to prevent.

### It holds — musical axis

"The machine plays itself" is not a gimmick bolted on; it is the natural apex of a
role-based, chord-reactive, deterministic arranger. arrangrr already has the three
live harmonic sources (chord play, ChordSequencer, piano→chord), the NTT resolver,
the groove engine and the ArpeggiatorEngine. A co-pilot that *pilots those knobs
over time* (D37) is exactly the layer that turns "a band that follows my chords"
into "a band that arranges a song with me". Musically this is the right ambition: a
one-person live act, or a songwriter who wants a rhythm section that develops rather
than loops. The intersection verdict: **the technical substrate genuinely serves
the musical intent** — a deterministic trajectory engine is *why* the autoplay can
be both alive and reproducible, which is precisely what every canned-arranger
competitor fails to be (see Deliverable 3, Band-in-a-Box).

### The category error to kill: the word "DAW"

Call this thing a DAW — even internally, even as shorthand — and you import a
gravity that D1/D2 spent the entire design resisting: a linear audio timeline, audio
tracks, plugin hosting, mixing. That gravity is exactly what pulled the *previous*
reflection's Reflection-1 toward "validate as a desktop app/plugin first", which was
thrown away for inverting D2. The word is a trap because it reframes the product for
the user's own hands: you will start reaching for the missing "audio track" and
"automation lane" and resent their absence, instead of building the generative
arranger that has no equivalent. **Name it what it is:** a *generative MIDI live
instrument / arranger workstation*. This is not pedantry — it is the difference
between measuring yourself against Ableton (and losing) and measuring yourself
against Genos + Band-in-a-Box + a modular generative rig (and winning on ground
nobody else holds).

### The sequencing law to obey (the D37 discipline, applied to the GUI)

D37 is explicitly *last* because a director has nothing to drive until the tunable
parameters exist. The **same law binds the GUI shell**: a rich generative/co-pilot
surface has nothing to *show* until the core's addressable parameter surface exists
— groove params (landed), arp params (landed), voicing controls (missing), scale-
degree melodic resolution (missing), step probability/density (missing), scenes
(missing). Build the ImGui co-pilot before those land and you build a dashboard
wired to dead knobs. So: the direction is sound, but its *value is gated* on the
core parameter surface (roadmap items 4/5/6 + the prior reflection's Rework 1/2)
landing first. The GUI adapter and the structural editors can start now; the
"co-pilot" apex is correctly downstream.

### Direction, bucketed

- **Keep (whole):** the boundary (GUI as client via adapter, core never depends on
  GUI); the co-pilot = deterministic Director (D37); cross-platform host, STM32 core
  untouched; the TUI-for-live / GUI-for-structure split from the prior reflection.
- **Rework (framing):** drop "DAW" as the mental model — it is a live generative
  arranger workstation; and gate the co-pilot GUI on the core parameter surface.
- **Throw away:** any literal-DAW drift (audio tracks, audio timeline, plugin host,
  bundled mixing) — that is the Reflection-1 inversion already refused, wearing new
  clothes.

---

## Deliverable 2 — Who actually uses this, and for what (concrete)

Not personas for a pitch deck — the people this machine is *shaped for*, and the
gesture each one comes for.

1. **The solo live performer / one-person band.** Plays chords with the left hand,
   melody with the right, and gets a developing band underneath — not a static loop.
   Comes for: chord play + arranger + section changes + the co-pilot pushing energy
   across a song so the outro is not the intro. The Genos/Korg buyer who is tired of
   menu-diving and canned fills. **Highest-value persona; the product's spine.**

2. **The producer / songwriter mining for ideas.** Feeds a progression (played or
   from the ChordSequencer), auditions styles, and lets the generative variation +
   re-harmonization throw up material they would not have written. Comes for: fast
   idea generation with *reproducible* seeds (audition 12 variations, lock the one
   that worked, recall it byte-exact next week). Exports MIDI to the real DAW for
   finishing — arrangrr is the *upstream idea engine*, not the mixing surface. This
   is the Band-in-a-Box use case done without the stiffness.

3. **The player who cannot (yet) play.** Wants to improvise and never hit a wrong
   note. Comes for: NTT-constrained live input — the whole keyboard mapped to chord/
   scale tones over the running band, so any key is a right key. arrangrr's "no wrong
   notes" thesis, extended from *style patterns* to *live human input*, is a genuine
   accessibility feature almost nobody ships well. **The persona no competitor
   serves honestly.**

4. **The generative / ambient musician.** Sets a harmonic field and expressive
   targets and lets the machine evolve within constraints — deterministic enough to
   recall, alive enough to surprise. Comes for: the Director as an evolving-texture
   engine, seeded variation, tension/brightness trajectories over long spans. The
   Wotja/Noatikl user who wants harmonic intelligence, not just note probability.

5. **The live-looping performer.** Builds up loops that *follow the chords* and can
   be re-harmonized on the fly. Comes for: the reversible looper (capture → degrees
   relative to the chord at capture → transposable/re-harmonizable) — the thing the
   live-looping world explicitly still wants and does not have (once a loop is laid,
   there is no way to re-harmonize it algorithmically).

6. **The sound designer / rig integrator.** Uses arrangrr as the deterministic MIDI
   brain of a hardware rig: multi-port routing, per-role voices, program-change,
   MIDI-learn, and *reproducible* sequences to test synths against a golden stream.
   Comes for: interop + determinism, the least glamorous and most defensible axis.

7. **The style author / content builder.** Authors styles (patterns per role) and
   wants to *see* the relative model — root, walk to the fifth, approach the next
   root — rather than decode chord-tone indices. Comes for: the relative-pattern
   editor (the GUI's real prize, per the prior reflection). Could be the user
   themself, or a small community if styles become shareable.

---

## Deliverable 3 — How not to become clunky like Zrythm (researched)

### What the field actually gets wrong (from real complaints)

- **Zrythm** — instability and platform dishonesty: crashes on a blank project,
  non-resizable window, no clean way to quit, "completely unusable" on Pipewire/
  Wayland, MIDI-in and audio-out not working simultaneously
  ([KVR](https://www.kvraudio.com/forum/viewtopic.php?t=609707),
  [AlternativeTo](https://alternativeto.net/software/zrythm/about/)). Lesson: a tool
  that fights the OS audio/MIDI subsystem and can't survive its own window manager
  loses before any feature matters.
- **Yamaha Genos / Korg Pa** — modal dead-ends and menu-diving: on the Pa4X step-
  recorder you *cannot hear the chord you're entering* without leaving the mode,
  saving, and returning; the Genos music-player is rigid (can't load an empty player
  while one runs, can't overlap). Where Korg is praised, it is for **dedicated live
  controls** (change keyboard scanning live with physical Upper/Lower buttons)
  ([Korg Forums](https://korgforums.com/forum/phpBB3/viewtopic.php?t=121128),
  [YamahaMusicians](https://yamahamusicians.com/forum/viewtopic.php?t=20476)).
  Lesson: never trap the user in a mode where the music goes silent; put the live
  gestures on always-available controls, not three menus deep.
- **Band-in-a-Box** — "canned, predictable, stiff", an "XP-era" UI, and a "forest of
  hidden commands" from decades of feature-grafting onto a code base nobody fully
  understands
  ([Wikipedia](https://en.wikipedia.org/wiki/Band-in-a-Box),
  [TDPRI](https://www.tdpri.com/threads/honest-opinions-on-band-in-a-box.151453/)).
  Lesson: stiffness is a *musical* failure (arrangrr's answer: groove engine +
  humanize + Director = alive-but-reproducible), and feature-cruft is an
  *architecture* failure (arrangrr's answer: bounded, no-heap discipline forbids the
  grafting — you cannot casually bolt on unbounded junk).
- **Ardour / LMMS** — "overwhelming", "cluttered", "steep", "dated"; newer tools win
  by being *deliberately cleaner*
  ([SaaSHub](https://www.saashub.com/compare-ardour-vs-lmms),
  [PluginDrop](https://plugindrop.net/posts/best-free-daw-software-2026/)).
  Lesson: the default surface must be small; depth is opt-in (D9 minimal-deep).

### UX principles to bind the GUI (derived, concrete)

1. **Never a silent mode.** Every editing/inspection surface keeps the music
   audible. No "enter step-record and lose the sound" (the Pa4X sin). The core is
   already always-running and authoritative — the GUI must never gate playback.
2. **Live gestures are always one action away.** Play/stop, section change, chord
   detect, part mute/solo, the co-pilot targets — top-level, not menu-buried. The
   TUI already does this (CTRL+SPACE, CTRL+P); the GUI must not regress it. Split the
   labour: **TUI (or a compact GUI transport bar) for live keyboard-first control;
   the GUI's rich 2-D views for structure/editing/inspection** — do not duplicate.
3. **Progressive disclosure (D9).** Default view = transport, current chord, current
   section, co-pilot state, parts. Everything else (relative-pattern editor, routing
   matrix, MIDI-learn) is a panel you open, not clutter you wade through.
4. **Immediate-mode is an anti-bug asset.** ImGui redrawing every frame from the
   mirrored event stream (D38) structurally prevents the GUI/state desync class that
   note-state-heavy tools suffer — the core wins every frame, the GUI cannot hold
   stale truth. Lean on this; do not add GUI-side authoritative state.
5. **Platform honesty (the Zrythm lesson).** Ship only what runs. The GUI touches no
   audio/MIDI subsystem at all (it speaks the socket); the *daemon* owns MIDI per-OS.
   That single boundary makes "unusable on Wayland/Pipewire" impossible for the GUI —
   it has nothing to break there.
6. **No feature-grafting.** The bounded/no-heap core is a cultural firewall against
   the Band-in-a-Box "forest of hidden commands": a feature that cannot be bounded
   does not enter the core, and the GUI grows panels, not core scope.

### Cross-platform architecture to be Win/Linux/macOS-ready with ImGui

- **Two processes, one boundary.** (a) **host daemon** — links `arrangrr` (the core library target),
  owns per-OS MIDI and the optional soft-synth sink, exposes the adapter socket;
  (b) **ImGui client** — pure UI over the socket, links no core, touches no MIDI.
- **ImGui backend.** Platform backend **SDL2 or GLFW** + a renderer backend
  (OpenGL3 as the portable baseline; Metal on macOS / DX11 on Windows if native
  polish is wanted later). `hello_imgui` collapses the multi-platform boilerplate to
  roughly one CMake line and handles asset embedding across all targets — a sane
  default that keeps *zero* GUI framework weight anywhere near the core
  ([hello_imgui](https://github.com/pthom/hello_imgui),
  [ImGui backends](https://skia.googlesource.com/external/github.com/ocornut/imgui/+/master/docs/BACKENDS.md)).
- **Per-OS MIDI in the daemon only.** A thin host MIDI HAL: ALSA seq (Linux),
  CoreMIDI (macOS), WinMM/WinRT-MIDI (Windows) — or a small library like RtMidi/
  PortMidi behind that HAL. This is a *host* concern; the freestanding core and the
  STM32 target never see it. MIDI portability is thus solved once, in the layer that
  is allowed to have an OS.
- **Audio-out as an optional MIDI sink.** A bundled soft-synth (e.g. a small
  SoundFont player) in the daemon so a solo user hears output without external gear.
  Must be optional, isolated, and never on the timing-authoritative path — arrangrr
  stays a MIDI machine (D1); the synth is a *convenience sink*, degradable to
  nothing.
- **The adapter transport, cross-platform.** UDS on Linux/macOS; on Windows, a named
  pipe or localhost TCP carrying the *same* JSONL lines. One protocol, three
  consumers (goldens, REPL, GUI) — unchanged (D30).
- **Packaging.** Linux: AppImage / Flatpak. macOS: `.app` bundle, code-signed +
  notarized (or the daemon is a CLI + a GUI bundle). Windows: portable exe / MSI.
  The daemon and GUI package together but stay separable — killing the GUI never
  touches playback (D38).

---

## Deliverable 4 — Dream features nobody has done right, and why arrangrr can

Speculative but motivated: each is a thing users have asked for for years, an honest
reason nobody shipped it well, and the specific arrangrr property that makes it
reachable here.

1. **Reproducible generativity — the "seed as a musical object".** *Dream:*
   generate variations you can audition, then recall the exact one you liked, next
   week, byte-for-byte. *Why nobody does it:* generative tools are non-deterministic
   (random state lost) or "AI" (unrepeatable). *Why arrangrr can:* D16 seeded PRNG +
   total-order determinism (D29) make every variation an addressable, recallable
   fact. A "variation dice" that is also a "variation library" is unique to a
   determinism-first engine.

2. **Non-destructive harmonic what-if, computed ahead of the playhead.** *Dream:*
   "show/hear what this loop would do under a different progression" *before*
   committing, without touching live playback. *Why nobody does it:* engines are not
   pure/headless; you cannot cheaply render a hypothetical offline. *Why arrangrr
   can:* the core is `advance_ticks`-pure with a virtual clock — a second core
   instance (host-side) can render the hypothetical against a virtual clock while the
   live core plays, and diff the result. Headless purity turns re-harmonization into
   a *previewable* operation, not a destructive gamble.

3. **The reversible looper (capture → re-harmonizable degrees).** *Dream:* loop a
   live phrase and later re-harmonize/transpose it as if it had been written
   relative to the chords. *Why nobody does it:* loops are captured as absolute notes
   with the harmony baked in; re-harmonizing is lossy. *Why arrangrr can:* capture
   against the live `ChordState` into the D28 degree-relative form — the conversion
   is invertible by construction. The live-looping world explicitly still wants this
   ([Loopy Pro forum](https://forum.loopypro.com/discussion/7254)).

4. **"No wrong notes" extended to the human, not just the machine.** *Dream:* hand a
   non-player a keyboard over a running band and let them solo without ever hitting a
   wrong note — with *musical* results, not a dumb scale-lock. *Why nobody does it:*
   scale-lock quantizers sound mechanical and ignore the current chord. *Why arrangrr
   can:* NTT already resolves any degree to a chord-tone against the live chord;
   pointing that resolver at *live input* (not just style patterns) makes the whole
   keyboard chord-aware in real time. The thesis of the machine, turned outward.

5. **Style morphing / interpolation.** *Dream:* blend two styles — 30% funk drums,
   70% bossa feel — instead of hard-switching. *Why nobody does it:* styles are
   opaque baked patterns, not parameterized. *Why arrangrr can:* styles are POD
   tables over roles; a role-wise parametric crossfade + a shared groove parameter
   space makes "between two styles" an addressable point. Arranger keyboards never
   let you stand between two styles; a generative one should.

6. **Polyphonic arrangement (per-role section state).** *Dream:* let the bass stay on
   Variation A while the drums take a Fill and the pad holds a Break — arrangement as
   independent role-lanes, not one monolithic section switch. *Why nobody does it:*
   arranger "sections" are global by tradition. *Why arrangrr can:* roles are already
   independent, routed, mutable; decoupling section state per role is a data-model
   move the architecture already affords. This is a genuinely new arranger idiom.

7. **Version-control for performance.** *Dream:* record a live set as a reproducible
   command log, replay it exactly, branch from any bar, and audition an alternate
   take deterministically. *Why nobody does it:* performances are audio/non-
   deterministic. *Why arrangrr can:* the replayable L0 protocol that feeds the
   golden harness *is* a performance log; "git for a set" falls out of D16/D17 almost
   for free.

---

## Deliverable 5 — Twenty prioritized additions (non-obvious, arrangrr-specific)

Ordered by priority (musical + strategic leverage), highest first. Each: **what** ·
**why this priority** · **[where it lives]**. None are trivial DAW absences; every
one exploits NTT / role-arranger / generativity / freestanding core / piano→chord /
modularity. "Musically worth it" and "technically feasible" are judged separately in
the *why*.

1. **Scale-degree / RelativeInterval / ChordGesture pattern model.** — Extend
   `StyleEvent` beyond chord-tone-only so bass can walk and leads can be scalar
   (thread `Key` into `resolve()`). — *Highest:* the prior reflection's sharpest
   verdict; unblocks melodic parts, the Director's "complexity" axis, and the whole
   co-pilot's musical range. Compile-time, bounded, D32-safe. — **[core]**

2. **Voice-leading + harmonic spillover resolver.** — Replace root-position re-stack
   with common-tone retention and minimal-motion voicing, `ChordGesture`-driven. —
   *Very high:* every chord change today is a jump; this is the single largest jump
   in perceived quality, and the difference between "machine" and "player". Bounded,
   fixed-point. — **[core]**

3. **Generative Director as a live co-pilot surface (D37).** — The deterministic
   trajectory engine in the core; an XY/target surface + target-timeline in the GUI
   ("push energy to 0.8 over 8 bars"). — *Very high:* this *is* "the machine plays
   itself"; but gated on items 1/2/5/6 giving it knobs to turn (the D37 sequencing
   law). Core = engine; GUI = target authoring. — **[entrambi]**

4. **Reversible looper (capture → degrees relative to chord-at-capture).** — Build
   the absent looper so captures store both absolute and degree-relative forms. —
   *High:* a top-3 "everyone wants, nobody solved" gesture; must follow item 1
   (the relative form to convert into). Bounded by the D33 budget. — **[core]**

5. **NTT-constrained live input ("no wrong notes" for the human).** — Map live
   keyboard input through the NTT/scale resolver over the running chord, whole-
   keyboard or per-zone. — *High:* the killer accessibility differentiator (persona
   3); reuses the resolver from item 1, so cheap once that lands. — **[core]** (zone
   config **[host]**)

6. **Reproducible variation browser (seed as a first-class object).** — Surface the
   seeded PRNG so you can audition N deterministic variations of groove/arp/fills and
   *lock + recall* the chosen seed exactly. — *High:* turns D16 determinism into a
   unique feature no competitor can copy; core exposes/labels seeds, GUI is the
   audition/lock/library surface. — **[entrambi]**

7. **Advanced step params (probability / ratchet / tie / rest / conditional-trig /
   micro-timing, seeded).** — The Elektron-style parameter-locks still absent from
   `Step`. — *High:* the raw material the Director's density/complexity axes steer;
   without it the co-pilot has little to move in the sequencer. Bounded POD. —
   **[core]**

8. **Scenes / song mode (snapshot + chain of sections/mutes/routing/targets).** —
   Recallable performance snapshots chained over time, including Director targets. —
   *High:* prerequisite for "co-pilot arranges a whole song" and for any scene
   control; nothing above the bar-level exists today. — **[core]** (editor
   **[host]**)

9. **Per-port latency compensation (adaptive bus scheduler).** — Per-port integer-
   tick offset (incl. negative look-ahead) so a chord on DIN+USB doesn't flam. —
   *Medium-high:* engineering that *is* phrasing — a late DIN note is a flam; fits
   the existing per-port scheduler, not a rewrite. — **[core]**

10. **Chord-context- and section-aware arpeggiator.** — Feed the ArpeggiatorEngine
    from the live `ChordState` (not only held keys) and hook rate/octaves to section/
    Director. — *Medium-high:* the arp engine exists; wiring the two missing call
    sites (style-part, MIDI-FX) is reuse, and makes the arp part of the arrangement,
    not a keyboard toy. — **[core]**

11. **Live chord confidence + interpretation suggestions.** — `ChordDetector`
    returns a confidence + alternative readings; GUI shows "you played X — meant Xm7
    or X6?". — *Medium-high:* small core addition, large payoff for players and
    non-players; makes the harmonic brain legible. — **[entrambi]**

12. **Style morphing / role-wise interpolation.** — Parametric crossfade between two
    styles per role + shared groove space. — *Medium:* musically novel (nobody lets
    you stand between styles); feasible because styles are POD role tables. Needs
    care that intermediate points stay musical. — **[core]** (control **[host]**)

13. **Polyphonic arrangement (per-role section state).** — Decouple section state per
    role (bass on A while drums fill). — *Medium:* a genuinely new arranger idiom the
    role model already affords; musically rich, but demands a clear UI or it confuses
    more than it frees. — **[core]** (surface **[host]**)

14. **Deep MIDI-learn / parameter-mapping matrix.** — Expose the stable param-ID
    space (D17c) as a learnable matrix so any controller drives any knob, live. —
    *Medium:* turns it into a real live instrument; the ID space already exists, the
    GUI makes it usable. — **[entrambi]**

15. **Retroactive capture ("grab the last N bars").** — Always-on ring so you noodle,
    then keep what you just played. — *Medium:* a beloved gesture arranger-world does
    badly; a bounded ring is exactly arrangrr's comfort zone (D32). Pairs with item
    4. — **[core]**

16. **Non-destructive re-harmonization preview (offline against a virtual clock).** —
    A host-side second core instance renders "this loop under that progression"
    ahead of the playhead for preview. — *Medium:* uniquely enabled by headless
    purity (dream #2); real work to wire a second instance + diff view, hence not
    top-tier. — **[entrambi]**

17. **Slash-chord / on-bass resolver.** — The `§11`-described bass-note policy that no
    code implements. — *Medium:* real harmonic expressiveness (inversions, pedal
    basses), small and bounded; below the melodic model that unlocks more. —
    **[core]**

18. **Deterministic song-form co-pilot (Director targets over a chord sequence).** —
    Auto-shape intro/verse/chorus/bridge dynamics by sequencing Director targets
    along the ChordSequencer. — *Medium:* the "play a whole song by itself" apex;
    strictly downstream of items 3 + 8, so ranked here despite its allure. —
    **[core]** (authoring **[host]**)

19. **Cross-role register/collision auto-spacing.** — Use `kRoleAnchor` (D36) to
    detect and spread register clashes between chord1/chord2/pad automatically. —
    *Medium-low:* prevents timbral mush as styles grow to 8+ parts; nice, not urgent,
    partly subsumed by item 2's voicing work. — **[core]**

20. **MPE-aware expressive input into the harmonizer.** — Absorb MPE on ingress
    (per-note channel) without touching the 8-byte `Event`. — *Low-medium:* real
    expressive upside for MPE-controller owners, but a narrow audience and no urgency
    against the musical-core gaps above; explicitly *not* the UMP trap. — **[core]**

---

## Verdict

The direction is sound on both axes and, better, it is the intended cash-out of the
architecture rather than a new demand on it: a Dear ImGui client as a pure mirror
over the host adapter (D38), a deterministic co-pilot as the autoplay (D37), the
freestanding STM32 core untouched — this is arrangrr keeping its own promises. Two
disciplines decide whether it lands well: **kill the word "DAW"** (it is a
generative live-instrument / arranger workstation — the moment you measure it against
Ableton you have lost the ground where you actually win), and **obey the D37
sequencing law for the GUI too** (a co-pilot surface has nothing to steer until the
core's tunable parameters — scale-degree melody, voice-leading, step density,
scenes — exist, so the GUI's structural editors and adapter can start now while the
autoplay apex waits for its knobs). Avoid the documented failure modes with three
firewalls the architecture already gives you for free: never a silent mode (the
Pa4X sin), never GUI-side authoritative state or MIDI-in-the-GUI (the Zrythm sin),
never feature-grafting past the bounded/no-heap line (the Band-in-a-Box sin). Do
that, and the machine that plays itself is not a canned-arranger imitation — it is
the first one whose generativity is *reproducible*, whose loops are *re-harmonizable*,
and whose "no wrong notes" is offered to the human hand and not only to its own.

---

## Sources

- [Zrythm — KVR forum thread](https://www.kvraudio.com/forum/viewtopic.php?t=609707)
- [Zrythm — AlternativeTo](https://alternativeto.net/software/zrythm/about/)
- [Korg Pa4x vs Yamaha Genos — Korg Forums](https://korgforums.com/forum/phpBB3/viewtopic.php?t=121128)
- [Genos 2 vs Korg PA5x vs Montage M — YamahaMusicians](https://yamahamusicians.com/forum/viewtopic.php?t=20476)
- [Band-in-a-Box — Wikipedia](https://en.wikipedia.org/wiki/Band-in-a-Box)
- [Honest Opinions on Band In A Box — TDPRI](https://www.tdpri.com/threads/honest-opinions-on-band-in-a-box.151453/)
- [Ardour vs LMMS — SaaSHub](https://www.saashub.com/compare-ardour-vs-lmms)
- [Best Free DAW Software 2026 — PluginDrop](https://plugindrop.net/posts/best-free-daw-software-2026/)
- [hello_imgui — cross-platform Dear ImGui](https://github.com/pthom/hello_imgui)
- [Dear ImGui — Backends](https://skia.googlesource.com/external/github.com/ocornut/imgui/+/master/docs/BACKENDS.md)
- [Generative apps for ambient — Loopy Pro forum](https://forum.loopypro.com/discussion/7254)
- [LooperGP — loopable sequence model (arXiv)](https://arxiv.org/pdf/2303.01665)
