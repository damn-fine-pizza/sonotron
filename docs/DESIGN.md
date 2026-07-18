# sonotron — Design Exploration & Architecture Plan
*(`arrangrr` is the core symbolic MIDI-brain component this document was
originally written for and still centers on; `sonotron` is the owner-validated
product name for the outer workstation — see `docs/product-vision.md`.)*

## Context

`arrangrr` is a **MIDI-only, realtime-first** system: arranger + step/live sequencer + MIDI looper. **Primary runtime target: STM32.** Linux is **only a devenv + simulation environment**; the project stays **OS-generic** (other hosts/OSes may come tomorrow — the platform layer must not be Linux-specific). It never produces audio (no synth, sampler, FX, audio looper): it is a *self-contained MIDI music machine* that drives external synths/DAWs/hardware and can be synchronized via MIDI clock.

The project is greenfield but **is already a git repo** (branch `main`, still without commits). `../seed-arranger` is a **separate, unrelated project** (Python, offline MIDI generation): **to be ignored** in arrangrr's design — no cousin, no pipeline.

**Language: C++26 where reasonable, with embedded realism.** It also cross-compiles for STM32, so the core uses a freestanding-friendly subset (own containers/`Span`, `-fno-exceptions`/`-fno-rtti` in the firmware path). **The dual-target build is set up from day one** (host + arm-none-eabi cortex-m), not at the end of the project. Modern tooling (CMake ≥4), **few dependencies unless there are reasons worth discussing**.

This document is a living **design exploration** (not minimalist): it first widens the space of possibilities, then ranks by priority. The goal shared with the user is to converge iteratively on *(a)* a **dream feature list** and *(b)* a **milestone roadmap**, continuing until understanding reaches ~95%. The architectural mandate is:

```
portable musical core  +  host simulator  +  I/O adapters (host / STM32)
```

**Execution note:** the "executable" output of this plan is (a) creating the repo skeleton with the HAL and the module boundaries defined here, and (b) committing this document as `docs/DESIGN.md` in the project. There is no code to modify yet; this plan *is* the starting specification.

**⏭ IMMEDIATE STEP UPON APPROVING THIS PLAN — first commit (requested by the user: "commit everything, especially under docs"):**
1. Create **`docs/DESIGN.md`** in the `arrangrr` repo = a full copy of this document (versioned source of truth in the project, D31/Francesco #9).
2. Create a minimal **`README.md`** (one line: what arrangrr is + a pointer to docs/DESIGN.md) and **`.gitignore`** (build/, .cache, etc.).
3. **`git add -A && git commit`** — first commit of the repo (today branch `main` has no commits).
4. No other code in this step; the scaffold/CMake/core work (band `1000`) starts afterwards, as per the numbered roadmap in `docs/roadmap.md` (§22 below is a pointer to it).

---

## 0. Roadmap & decisions — unified into the numbered tree

The former "consolidated decisions" log (the `D1..D54` codes) and the milestone
roadmaps (the `M0..M13` / `Phased #` / `H*` codes, formerly the §22–§27 block —
MVPs, incremental & milestone roadmaps, dream list) are
**retired as nomenclature**. There is now ONE canonical structure: the **numbered
roadmap**, where the hierarchical number is the only identity — every
former milestone and every former decision is a NODE in that tree, with its
rationale folded in. Cross-cutting principles live in the invariants band `0000`;
the old `M*`/`D*`/`Phased #*`/`H*` codes map to their new numeric IDs in the
migration appendix.

**As of 2026-07-18, that canonical tree lives in `docs/roadmap.md`, not in this
document** — §22 below is now a short pointer to it, to stop the two files from
drifting against each other the way they had. See `docs/roadmap.md` for the
tree, the recommended sequence, the open owner-decisions, and the migration
cross-reference. (Residual inline `M*`/`D*` references still present in the
architecture/spec sections resolve through that appendix.)
## 1. Product vision

**What it is.** A "MIDI brain" for live performance and composition: you play chords/notes from a master keyboard, and the device generates accompaniments in real time (drums/bass/chord/pad/arp), manages arrangement sections (Intro/Variation/Fill/Break/Ending), sequences patterns, records MIDI loops in overdub, and routes/transforms MIDI toward multiple external devices on different channels/ports. It syncs to/from external clock.

**What it is NOT.** It is not a synth, not a sampler, not an effects engine, not an audio looper, not a DAW plugin, not a desktop score editor. It contains no "sounds": a "Program" here is a *MIDI reference to an external sound* (port/channel/bank/PC/CC init), not an internal timbre.

**The central value** is: *excellent MIDI timing + chord-aware musical transformation + total MIDI interoperability/compliance + determinism/testability*. If timing and compliance are not impeccable, the rest doesn't matter.

**Three primary usage modes.** (1) Live "one-man-band" arranger; (2) Groove/step sequencer with song mode; (3) Synchronized MIDI looper. The three share the same Transport, Router, Scheduler and Chord Engine.

---

## 2. Architectural principles (hard rules)

1. **Pure core, sink-and-source.** The core does not read the system clock, does not touch the filesystem, makes no syscalls, does not heap-allocate during play/record. It receives injected *ticks* and *incoming events*; it emits *scheduled outgoing events*. All time enters as a parameter.
2. **Thin and total HAL.** Every interaction with the world (time, MIDI in/out, storage, physical input, display) goes through abstract interfaces implemented differently on Linux and STM32. The core depends only on the interfaces.
3. **Determinism as an instrument-property, where needed (not a cage).** The core is *reproducible given identical inputs* (same state + same sequence of timestamped inputs ⇒ same output): this is a **capability** that enables golden tests, replay and debug — activatable when needed. Randomness only via an explicit **seeded** PRNG, so humanize/probability are reproducible *when you want them* (e.g. in tests) but free to vary in live play. Determinism must **not** stiffen musical life (feel, swing, real timing, human interaction). See D15/D16.
4. **Bounded everything.** No unbounded-growth container in the realtime path: `StaticVector<T,N>`, array+count, fixed-capacity ring buffers. Limits are `constexpr` and verified with `static_assert`.
5. **Realtime path with no surprises.** No exceptions, no RTTI, no `iostream`, no `std::string`, no `new`/`malloc`, no blocking locks in the audio-rate path (use lock-free SPSC ring buffers between ISR and loop).
6. **Fixed-point by default.** Time in integer ticks; BPM as `bpm_x100` (uint). Float allowed only where truly needed (e.g. scoring curves in laptop tools), never in the STM32 realtime path. **DEFERRAL (docs/proposals/isoundengine-contract.md §6 item 6, owner-flagged, 2026-07-16):** `ISoundEngine::render(float*, int)` (`components/core/audio_engine`) is host-only in every concrete implementation today and pays no on-device cost yet; the tension between this rule and a future `analog` engine rendering `float*` on the STM32H743 (which has an FPU) is real but deferred — revisit only before an `analog` engine actually renders on-device, not now.
7. **Data/logic/IO separation.** Three layers: *model* (serializable POD), *engine* (pure transformations on the model + events), *platform* (adapters). Laptop tools (YAML/JSON, SMF, editors) live outside the core and never enter it.
8. **One single versioned binary format** for the project/style on device, with `magic + version + CRC`. YAML/JSON only on the tool side.
9. **Headless testability first of all.** Every engine is testable with recorded MIDI input → MIDI output compared against a golden file, without hardware and without real time.
10. **Explicit budgets.** RAM/flash/CPU/latency have declared target numbers and a validator that rejects a project exceeding them before export to the device.

---

## 3. Candidate software modules (table)

Priority: **P0** = indispensable MVP · **P1** = MVP2 · **P2** = future/architectural.
STM32 = also runs on firmware. Tool = exists only on the laptop side.

| Module | Description | Why it's needed | Prio | STM32 | Tool | Risk |
|---|---|---|---|---|---|---|
| **Transport/Clock** | Master tick, PPQN, tempo, play/stop/continue, song position, tick→beat/bar | Foundation of all timing | P0 | Yes | No | Medium |
| **MIDI In Processor** | Byte-stream parsing, running status, NoteOn v0→Off, input merge, timestamp | Correct and compliant input | P0 | Yes | No | Medium |
| **MIDI Out Scheduler** | Timestamped event queue, ordering, DIN throttling/bandwidth, per-port flush | Stable output timing | P0 | Yes | No | High |
| **MIDI Router** | In→out routing per port/channel, thru/soft-thru, filters, channel remap | Interoperability/flexibility | P0 | Yes | No | Medium |
| **Note/Voice Tracker** | Tracks active notes per channel/port, sustain, for panic/all-notes-off, anti-stuck | Prevents stuck notes | P0 | Yes | No | Medium |
| **Sequencer** | Step patterns + live record, per-track length, mute/solo, velocity/gate/prob | Rhythmic heart | P0 | Yes | No | High |
| **Chord Engine** | Chord recognition from live notes, fingering modes, chord hold/memory | Makes it "follow the chords" | P0 | Yes | No | High |
| **Arranger/Style Engine** | Sections (Intro/Var/Fill/Break/End), patterns per role, transformation onto the chord | Identity feature | P0/P1 | Yes | No | High |
| **MIDI Looper** | Record/overdub/replace/erase/undo, quantize-after, loop len, sync | Identity feature | P1 | Yes | No | High |
| **Scale/Key Engine** | Current key/scale, scale-lock, correction of out-of-scale notes | Musical coherence | P1 | Yes | No | Medium |
| **Quantizer** | Quantizes recorded input (grid, swing, strength) | Recording quality | P1 | Yes | No | Medium |
| **Groove/Swing Engine** | Deterministic micro-timing/swing/humanize (seeded PRNG) | Non-mechanical feel | P1 | Yes | No | Medium |
| **Arpeggiator** | up/down/updown/random/as-played, octave, latch, sync, arp patterns | Expressiveness | P1 | Yes | No | Medium |
| **Phrase/Pad Engine** | MIDI pads: one-shot/loop/hold/toggle, sync, chord/drum/CC pads, fill/scene trigger | Live performance | P1 | Yes | No | Medium |
| **Chord Sequencer** | Records/plays back a chord progression to drive the arranger hands-free | Frees the hands | P1 | Yes | No | Medium |
| **CC Automation Engine** | CC/pitchbend automation lanes per track, with step interpolation | External timbral movement | P1 | Yes | No | Medium |
| **Performance/Scene Manager** | Recallable live state (style, var, mute, split, routing, transpose, pad map) | Instant recall | P1 | Yes | No | Medium |
| **Song/Scene/Chain Manager** | Chains patterns/scenes/sections into a song | Song structure | P1 | Yes | No | Medium |
| **Project/Preset Manager** | Load/save binary project, slots, defaults | Persistence | P1 | Yes (I/O via HAL) | No | Medium |
| **Storage Model + Serializer** | Versioned binary layout + CRC + migration | Safe persistence | P1 | Yes | Part. | High |
| **UI State Machine (ui_model)** | Headless UI state: pages, cursors, encoders, LED intent — separated from rendering | Small/portable UI | P1 | Yes | No | Medium |
| **Diagnostics/MIDI Monitor** | Event log, counters, jitter meter, last chord/section | Debug/interop | P1 | Part. | Yes | Low |
| **Panic/Reset Controller** | All Notes Off, All Sound Off, Reset All Controllers, per port/channel | Live safety | P0 | Yes | No | Low |
| **MIDI Compliance Layer** | Policies: running status out, v0 vs Off, active sensing, channel mode | Guaranteed interop | P0 | Yes | No | Medium |
| **MIDI Learn / Controller Map** | Maps physical CC/notes → internal functions | Customization | P2 | Yes | Part. | Medium |
| **Program/Bank Manager** | Emission of PC + Bank MSB/LSB + CC init for external devices | External sound setup | P1 | Yes | No | Low |
| **Device Profile Registry** | Model of external synths (see §18) | Targeted interop | P2 | Yes (data) | Yes (editor) | Medium |
| **SysEx Engine** | SysEx receive/send for backup/device profiles (bounded, chunked) | Backup/interop | P2 | Yes (opt.) | Yes | High |
| **Clock Sync Manager** | Master/slave, drift/jitter handling, SPP, start/continue | Sync with the outside world | P0/P1 | Yes | No | High |
| **Style Compiler** | YAML/JSON style → validated binary for the device | Content pipeline | P2 | No | Yes | Medium |
| **Pattern/Style Editor** | Graphical pattern/style editor on the laptop side | Authoring | P2 | No | Yes | Medium |
| **SMF Import/Export** | Standard MIDI File ↔ pattern/song | File interop | P2 | No (or opt.) | Yes | Medium |
| **Regression Runner** | Runs deterministic golden tests | Quality | P0 (dev) | No | Yes | Low |
| **Fault Injector** | Injects jitter/corrupt bytes/clock drift into the simulator | Robustness | P2 | No | Yes | Low |

### Additional modules/concepts I recommend planning for (not in your list)

- **Latch/Hold Manager**, cross-cutting (chord hold, arp latch, pad hold) — better as one reused concept.
- **Transpose/Octave Engine**, global + per-track (master transpose, key transpose, octave shift), with a "who follows the chord and who doesn't" rule.
- **Velocity Curve / Note Range Mapper** per-destination (part of the device profile but used in the realtime path).
- **Tap Tempo / Tempo Nudge** (input) and **Metronome/Click out** (as MIDI events or on a dedicated channel).
- **Fill-on-change / Auto-fill** logic (automatic fill when you change variation) — a classic arranger concept.
- **Ending/Intro count-in scheduler** (the "one-shot" sections that then move on to a variation).
- **Mute/Solo Group + "Track enable mask"** as a first-class object (recallable from Performance/Scene).
- **Chord→Bass inversion / "on-bass" (slash chord) resolver** as a sub-module of the Chord/Arranger.
- **Voicing/Voice-leading resolver** (close/open/drop-2, smoothing between chords) — decides *how* the notes follow the chord.
- **Event Priority/Collision resolver** in the scheduler (NoteOff before NoteOn on the same tick, deterministic order).
- **Time Signature Engine** (meter, for polymeter/section length) — often forgotten but needed early.
- **Song Position/Locate** (jumping to bar N with state reconstruction) — non-trivial with an arranger.
- **"MIDI thru mute during record"** and **input channel filter** — routing details that prevent doubled notes.
- **Snapshot/Undo ring**, generic (for the looper and for edits) as a reused bounded structure.

---

## 4. Essential modules (credible MVP)

An MVP that demonstrates the product's identity requires **impeccable timing + sequencing + minimal chord-aware accompaniment**:

1. **Transport/Clock** (internal PPQN, play/stop, tempo, tick→bar/beat).
2. **MIDI In Processor** (compliant parsing, v0→Off, merge, timestamp).
3. **MIDI Out Scheduler** (timestamped queue, deterministic ordering, DIN throttling).
4. **MIDI Router** (in→out per channel/port, configurable thru, basic filters).
5. **Note/Voice Tracker + Panic** (no stuck notes, All Notes Off).
6. **Sequencer** (at least multi-track step patterns + quantized live record).
7. **Chord Engine** (basic fingering: Fingered + Single Finger; chord hold).
8. **Arranger core** (one Style with Variation A/B + Fill + Intro/Ending, fixed drums + bass/chord that follow the chord).
9. **Clock Sync (minimal slave)** (follow external clock + Start/Stop) — or at least clean master out.
10. **Compliance Layer + Regression Runner** (because "very MIDI-compliant" is a primary requirement, not an extra).

Without (1)-(5) you don't have a serious MIDI device; without (6)-(8) you don't have *this* device.

---

## 5. Interesting but non-MVP modules (plan for, don't implement right away)

- **Full MIDI Looper** (overdub/undo/replace) — architecture yes, feature after the sequencer.
- **Arpeggiator, Phrase Pads, Chord Sequencer** — great live value, but MVP2.
- **Groove/Humanize, advanced Quantizer, Scale-lock**.
- **Full Performance/Scene/Song Manager** (for the MVP, "load 1 project" is enough).
- **CC Automation lanes**, **Device Profile Registry**, rich **Program/Bank Manager**.
- **SysEx**, **SMF import/export**, **MIDI Learn**, graphical **Style Compiler/Editor**.
- **Fault injection**, advanced **jitter meter**.

Rule: *plan for the extension points (interfaces, reserved fields in the format, hooks in the scheduler) but not the code.*

---

## 6. Modules to avoid (to keep things from exploding)

- Any **audio/DSP/synthesis/sampling/FX** — excluded by mandate.
- **Notation/score engine**, score editor.
- **Complex desktop UI** inside the core (only headless `ui_model` + a thin render).
- **Embedded scripting / VM** (Lua etc.) in the realtime firmware.
- **JSON/XML/YAML parsers in the core** — only in the tools.
- **Networking/OSC/Wi-Fi/Bluetooth** in the MVP (possibly P2 as an adapter, never in the realtime core).
- **Full MIDI 2.0/MIDI-CI** now — too broad; leave an abstraction, don't implement.
- **Sophisticated custom allocators/GC**, **dynamic plugins**, **reflection**.
- **Unlimited undo / persistent history** — only a bounded ring.
- **Multi-user / cloud / accounts** — out of scope.

---

## 7. Data taxonomy (model entities)

Recommended hierarchy (from the live container down to the building block):

```
Project
 ├─ MIDISetup            (ports, sync mode, global transpose, master channel)
 ├─ DeviceProfile[]      (description of external synths)
 ├─ Program/ExternalSound[]   (external sound reference: port+ch+bank+PC+CC init)
 ├─ Style[]
 │   └─ Section[]        (Intro1/2, VarA..D, FillA..D, Break, End1/2)
 │       └─ TrackRef[]   (role → Pattern + chord/transpose/voicing policy)
 ├─ Pattern[]            (grid relative to degrees + timing; referenced by Section/Sequencer)
 ├─ Song[]               (chain of Scenes/Sections + ChordSequence + tempo map)
 │   └─ Scene[]          (snapshot: variation, mute mask, routing, transpose…)
 ├─ ChordSequence[]      (timestamped chord progression)
 ├─ Performance[]        (recallable live state — the "registration")
 ├─ Pad[]                (pad assignments → phrase/chord/CC/fill/scene)
 ├─ RoutingProfile[]     (in→out matrix, filters, thru)
 └─ ControllerMap[]      (MIDI learn: physical control → function)
```

Key differences (to avoid conceptual confusion):

| Entity | What it is | What it is NOT |
|---|---|---|
| **Project** | The savable document containing *everything* (styles, songs, setup, profiles). Root of the serialization. | It is not the volatile live state. |
| **MIDISetup** | Global I/O config: ports, master/slave sync, master transpose, master keyboard channel. | It does not contain musical patterns. |
| **Style** | A complete accompaniment: set of **Sections**, each with patterns per track **role** + policy. | It is not a song; it has no fixed progression. |
| **Section** | A part of the arrangement (VariationA, FillB, Intro1…) with length in bars and the roles' patterns. | It is not a chord; it is "neutral" and gets transformed onto the chord. |
| **Variation / Fill / Intro / Break / Ending** | Subtypes of **Section** with transition semantics (loop vs one-shot). | — |
| **TrackRole / TrackRef** | The role (Drums, Bass, Chord1…) and the policy: follows the chord? transpose? voicing? range? MIDI destination. | It is not the pattern itself: it is the "how to play it". |
| **Pattern** | The *relative* rhythmic/melodic content (degrees relative to the root, steps, gate, velocity), resolution-free. | It contains no absolute MIDI notes until resolved onto the chord/key. |
| **Event** | A single resolved/recorded event: tick, type, channel, data. Building block of looper/recording/output. | It is not an abstract pattern; it is concrete. |
| **Song** | Song structure: ordered chain of Scenes/Sections over time + ChordSequence + tempo map. | It is not a Style; it uses Styles/Patterns. |
| **Scene** | Snapshot of performance state at a point in the song (variation, mute mask, routing, transpose). | It is not a Section; it is "which configuration". |
| **ChordSequence** | Timestamped chord progression (to drive the arranger hands-free). | It does not generate notes by itself; it drives the arranger. |
| **Performance** | Live state recallable with one button (the "Registration/Combi"): selected style, variation, split, mute, routing, transpose, pad map, arp state. | It is not persistence of the whole project; it is a *state preset*. |
| **Program / ExternalSound** | Reference to an *external* sound: port+channel+bankMSB/LSB+PC+CC init+range+velocity curve+transpose. | It is not an internal timbre (those don't exist). |
| **DeviceProfile** | Description of an external device (drum map, supported CCs, quirks, init/panic). A Program references it. | It is not a single sound; it is the device's "manual". |
| **Pad** | Performance trigger: phrase/chord/drum/CC/fill/scene, with mode (one-shot/loop/hold/toggle) and sync. | It is not an arranger track. |
| **RoutingProfile** | In→out matrix + filters + thru + remap. Referenced by Performance/Scene. | It is not music; it is routing. |
| **ControllerMap** | Mapping of a physical control (CC/note/encoder) → internal function (MIDI learn). | It is not MIDI→MIDI routing. |

In-memory representation (embedded-friendly): all these entities are **POD** (simple structs, `enum class`, `StaticVector<T,N>` or array+count). No owning pointers; references are **indices/IDs** (`u16`) into Project tables, not pointers — so the serialization is memcpy-friendly and relocatable.

---

## 8. Analogy with Korg / Yamaha / Roland / Casio (with sources)

The goal is to translate *general, public* concepts (not proprietary features) from arrangers/workstations/grooveboxes into a **MIDI-only** project. The central point is that in the big instruments these concepts mix "performance control" and "sound generation"; in our case the "sound" part always becomes an **external MIDI reference**.

### 8.1 Korg (Pa series)
A Korg **Style** typically contains 8 style tracks, with **3 Intros, 4 Variations, 4 Fills, Break, 3 Endings**, plus **4 STS (Single Touch Settings)** and **4 Pads** and one **Style Performance** per style. **STS** recalls the sounds for the real-time tracks (Upper/Lower). The **SongBook** is a user music database that stores *all* the settings for playing a song (style/MIDI file/tempo/volumes/sounds/mute/FX/STS/transpose) and is organized into **Set Lists**. ([Korg — Songbook & Set List](https://support.korg.co.uk/en-US/songbook-and-set-list-setup-for-pa-keyboards-351393), [Korg Pa300 features](https://www.korg.com/us/products/synthesizers/pa300/page_1.php), [Korg Pa300 User Manual PDF](https://www.bhphotovideo.com/lit_files/252801.pdf))

**arrangrr translation:** Style→`Style`; its sections→`Section[]`; **STS/Style Performance**→`Performance` (but the "sounds" are external `Program`s: bank/PC/CC init); **Pad**→`Pad[]`; **SongBook/Set List**→`Song[]` + a `SetList` (ordered list of Performances/Songs with MIDI setup). The 8 style tracks → our **TrackRoles** (drums/perc/bass/chord/pad/…).

### 8.2 Yamaha (Genos/Tyros/PSR/Montage)
Concepts: **Registration Memory** (1–10) recalls a complete panel of settings; **One Touch Setting (OTS)** recalls the most appropriate settings (Keyboard Parts, Harmony/Arp, Multi Pad) for the Style; **Multi Pad** in **Banks** of 4 rhythmic/melodic phrases; Style sections with **Intro/Main(Variation)/Fill/Break/Ending**. ([Yamaha Genos Owner's Manual](https://usa.yamaha.com/files/download/other_assets/7/1130977/genos_en_om_h0.pdf), [Genos Reference Manual](https://usa.yamaha.com/files/download/other_assets/7/1131007/genos_en_rm_h0.pdf), [Yamaha: Single Finger vs Fingered](https://faq.yamaha.com/usa/s/article/U0002033)) Internally, Yamaha Styles use transposition tables (rules of the NTR/NTT kind) to make the patterns *follow the chord* — a public concept that inspires our **voicing/transpose resolver**.

**Translation:** Registration Memory→`Performance`; OTS→a "suggested" subset of `Program`/mute per Style (optional); Multi Pad Bank→`Pad[]` in banks of 4; the NTR/NTT logic→per-role **note-transposition policy** in the Chord/Arranger.

### 8.3 Roland (Fantom/BK/E/MC)
Fantom structures sounds as **Tone → Zone → Scene**: a Tone is a sound, it lives in a Zone (up to 16 tones/zones, with key range/volume/pan/controller reception), and Zones+settings are saved in a **Scene**. ([Roland: What is a Zone](https://support.roland.com/hc/en-us/articles/12869134186523-FANTOM-6-FANTOM-7-FANTOM-8-What-Is-a-Zone), [Sweetwater: Tones, Zones, Scenes](https://www.sweetwater.com/sweetcare/articles/roland-fantom-tones-zones-and-scenes/)) The BK/E-series arrangers use Rhythm/Style + registrations.

**Translation:** **Zone**→an extremely powerful concept for us: a **Zone** = portion of the keyboard (key range) + destination channel/port + transpose + velocity curve = exactly how we want to route the master keyboard to external devices (split/layer). I adopt **Zone** as the input→destination routing entity. **Scene**→`Scene`/`Performance`. **Tone**→`Program` (external).

### 8.4 Casio (CTK/LK/WK arrangers)
Fingering modes: **Casio Chord** (one-finger chords with simple rules), **Fingered 1/2/3** (up to 15 chord types), **Full Range Chord**. **Registration Memory** for recalling setups. ([Casio CTK-6200 — chord fingering modes](https://www.manualslib.com/manual/595359/Casio-Ctk-6200.html?page=28), [Casio Auto Accompaniment PDF](https://support.casio.com/pdf/008/lk50_e_08.pdf), [Casio Memory Function PDF](https://support.casio.com/pdf/008/lk50_e_11.pdf))

**Translation:** the fingering modes confirm the set I want in the **Chord Engine**: `SingleFinger`, `Fingered`, `FullKeyboard`, (+ optional `FingeredOnBass`, `AIFingered`). Registration→`Performance`.

### 8.5 Groovebox / MPC / Elektron
MPC: **Sequence → Track → Program** structure; a Sequence can be a section or an entire song; no separate "clip" concept, notes live inside the tracks. ([MPC Standalone OS User Guide](https://cdn.inmusicbrands.com/akai/MPC3-NI/MPC%20Standalone%20OS%20-%20User%20Guide%20-%20v3.4.pdf), [Sound on Sound: Akai MPC Basics](https://www.soundonsound.com/techniques/akai-mpc-basics)) Elektron: per-step **parameter locks** and pattern/chain, the sequencer as a "modulator". ([Elektronauts discussion](https://www.elektronauts.com/t/parameter-locks-on-the-mpc-live-nope-but-heres-a-workaround/39832))

**Translation:** MPC Sequence/Track→our **Pattern/Song**; MPC "Program" (in the MPC it is a kit of sounds)→for us an **external Program** (MIDI map). Elektron **parameter locks**→**per-step CC/velocity/prob lanes** in the Sequencer (general concept: "lock" = per-step value). **Pattern chain**→`Song`/scene chain.

### 8.6 Summary of the inspired taxonomy

| Industry concept | Source | arrangrr entity | MIDI-only notes |
|---|---|---|---|
| Style + sections | Korg/Yamaha | `Style`+`Section[]` | patterns relative to degrees |
| Performance / Registration / STS / Scene | all | `Performance` | recalls *MIDI references*, not sounds |
| Sound / Program / Tone | all | `Program`/`ExternalSound` | port+ch+bank+PC+CC init |
| Zone (key range→dest) | Roland | `Zone` (inside RoutingProfile) | split/layer toward devices |
| SongBook / Set List | Korg | `Song[]` + `SetList` | performance/song chain |
| Multi Pad / Pad | Yamaha/Korg | `Pad[]` (banks of 4) | phrase/chord/CC/fill trigger |
| Sequence/Track/Pattern chain | MPC/Elektron | `Pattern`/`Song`/scene chain | — |
| Parameter locks | Elektron | per-step lanes | CC/vel/prob per step |
| Chord fingering modes | Yamaha/Casio | `ChordMode` enum | chord recognition |
| Note transposition rules | Yamaha | voicing/transpose policy | who follows the chord |

---

## 9. MIDI Compliance Checklist

I distinguish five levels (as requested):

- **A. Protocol compliance** — the bytes are correct per the MIDI 1.0 standard.
- **B. Musical correctness** — the right notes at the right time (chords/scale/voicing).
- **C. Timing quality** — jitter/latency/clock stability.
- **D. Device interoperability** — works with real synths/DAWs and their quirks.
- **E. Routing flexibility** — configurable routing/merge/thru/filters.

### A. Protocol compliance
- [ ] **Running status** parsing on input; option to *use it or not* on output (config for bandwidth/compat).
- [ ] **Note On velocity 0 == Note Off** (accepted on input; on output configurable policy: emit a real Note Off by default).
- [ ] **Real Note Off** with release velocity (default 64) supported.
- [ ] Correct handling of **status byte vs data byte** (high bit), real-time messages interleavable inside other messages.
- [ ] **System Real-Time** (Clock F8, Start FA, Continue FB, Stop FC, Active Sensing FE, Reset FF) handled outside the main stream.
- [ ] **SysEx** (F0…F7) bounded/chunked parsing, with timeout and reset on interruption; safe passthrough.
- [ ] **Channel Voice**: Note On/Off, Poly Pressure, CC, Program Change, Channel Pressure, Pitch Bend — all round-trip.
- [ ] **Channel Mode messages** (CC 120 All Sound Off, 121 Reset All Controllers, 122 Local, 123 All Notes Off, 124–127 Omni/Mono/Poly).
- [ ] **Bank Select** CC0 (MSB) + CC32 (LSB) followed by Program Change, in the correct order.
- [ ] **RPN/NRPN**: CC 101/100 (RPN MSB/LSB) or 99/98 (NRPN), then CC6 (Data MSB)/CC38 (Data LSB), with **RPN NULL (127/127)** to close; increment/decrement CC96/97.
- [ ] **Pitch Bend** 14-bit (LSB+MSB) correct; center 0x2000.
- [ ] **14-bit CC** (MSB/LSB pairs 0–31 / 32–63) — at least correct handling/pass-through.
- [ ] **System Common**: Song Position Pointer (F2, 14-bit in MIDI beats=6 clocks), Song Select (F3), Tune Request (F6).
- [ ] **Active Sensing** (FE): optional on output; if received, ~300 ms timeout → panic if the stream stops (configurable, off by default to avoid being intrusive).
- [ ] Robustness: corrupted bytes, incomplete status, orphan data bytes → discarded without crashing.

### B. Musical correctness
- [ ] Correct chord voicing per role (bass = root/inversion, chord = triad/extensions, pad = smooth).
- [ ] Transposition that respects who "follows the chord" and who doesn't (drums/perc never transposed).
- [ ] Optional scale-lock/quantize-to-scale consistent with the Key Engine.
- [ ] Notes outside the device's range clamped/folded per policy (no unintentional silent mute).
- [ ] No duplicate note (same pitch+channel) without an intervening Note Off; correct re-trigger handling.

### C. Timing quality
- [ ] Precise **MIDI Clock 24 PPQN** on output; internal at a higher PPQN (96/192) sub-divided to 24 for the clock.
- [ ] Declared **jitter budget** (e.g. < 1 ms on DIN, < 0.5 ms preferable) and measured by the jitter meter.
- [ ] Declared input→output **latency budget** (e.g. < 3 ms internal).
- [ ] **Look-ahead** scheduler with timestamps; NoteOff before NoteOn on the same tick; deterministic ordering of simultaneous events.
- [ ] **DIN bandwidth** (correct numbers): 31250 baud / 10 bits-per-byte = **3125 bytes/s → ~320 µs/byte**; a 3-byte message ≈ **960 µs**; a 4-note chord (12 bytes) ≈ **3.8 ms** on a single DIN ⇒ sub-ms "simultaneity" on the DIN wire is physically impossible: the jitter budget must be defined per **message-start**, not per-note. Prioritization (clock/realtime > notes > CC).
- [ ] **PER-PORT scheduler & bandwidth** (Francesco #10): queue and bandwidth model **separate for each port** — USB (fast) and DIN (slow, 3125 B/s) do not share a global budget.
- [ ] **USB-MIDI packetization** (Francesco M6): USB-MIDI 1.0 class = **4-byte packets** (Code Index Number + cable number), no running status on the wire, flow-control different from DIN ⇒ distinct throttling path; map cable→port.
- [ ] No blocking/GC/alloc in the scheduling path.

### D. Device interoperability
- [ ] Per-device init sequence (Bank/PC/CC init, optional GM Reset) when a Program is activated.
- [ ] Robust panic: All Notes Off **+** All Sound Off **+** Reset All Controllers on all used channels/ports, with anti-stuck (sends explicit Note Offs for every tracked note).
- [ ] Sustain pedal (CC64) handled: notes held while the pedal is down; panic also releases the sustain.
- [ ] GM/GM2/GS/XG drum map: configurable device profile (drum channel 10, note map).
- [ ] Tests with real targets: DAW (Reaper/Bitwig), Volca (fixed channel/note), DIN synth, USB-MIDI interface.

### E. Routing flexibility
- [ ] Multi-port, multi-channel; in→out matrix.
- [ ] Hard **thru** and **soft-thru** (regenerated by the scheduler, with re-timing/filters).
- [ ] Merge of multiple inputs with timestamps and collision resolution.
- [ ] Filters by message type/channel/note range per route.
- [ ] Thru mute during record to avoid duplicate notes.

---

## 10. Timing model

- **Internal scheduling PPQN: 960** (D27). 960 = 40 × 24 ⇒ MIDI clock F8 every **40 ticks** (integer, clean sync). At 120 BPM, 1 tick = **0.52 ms**: fine enough for swing/humanize/micro-timing (integer offsets in ticks, no sub-tick/float). **96 PPQN remains the "musical grid"** used for reasoning/quantizing (1/16 = 24 ticks at 96 = 240 ticks at 960; triplets and 1/32 all integer at 960). `Event.tick` = `i32` at 960. 480 as a fallback (480/24=20). At ~300 BPM, 960 PPQN = ~4800 ticks/s: trivial for a Cortex-M.
- **BPM**: `bpm_x100` (`uint16`/`uint32`), e.g. 12000 = 120.00 BPM. Tempo→tick duration computed in fixed-point; no floats in the realtime path.
- **Tick source**: a hardware timer (STM32) or a high-priority thread (Linux) calls `core.onTick()` at the internal PPQN resolution, or the core derives ticks from a faster timer with a fixed-point accumulator (preferable: µs timer → accumulate → emit tick at threshold, so the tempo can change without reprogramming the timer).
- **MIDI clock mapping**: in **master** emit F8 every 40 ticks (at 960 PPQN). In **slave** receive F8 and reconstruct the tempo with a PLL/filter (moving average of the intervals) to absorb jitter; the internal tick is interpolated between the received clocks.
- **Start/Stop/Continue + SPP**: Start=restart from 0; Continue=resume from SPP; SPP in "MIDI beats" (1 beat = 6 MIDI clocks = 16th). Locate reconstructs the arranger state at that bar.
- **Scheduling**: event queue ordered by tick (bounded min-heap or timing wheel/bucket per tick within the look-ahead). An event = `{absolute_tick, type, payload}`. Draining happens "just-in-time" on each tick with a look-ahead of N ticks to absorb computation cost.
- **Quantization (record)**: input recorded with raw timestamps → optional quantize to grid with **strength** (0–100%) and **swing**; preserves the off-grid feel if strength<100.
- **Swing**: delay of the even sub-beats (e.g. odd 16ths) as a % of the step; deterministic. Applied as a scheduling offset, not by modifying the pattern.
- **Jitter handling**: look-ahead + absolute timestamps; the drain compares the current tick and does not "catch up" by emitting disordered bursts — if late, it emits in order preserving the sequence.
- **Overdub**: records onto an event layer that merges with the existing loop without erasing (see §13).
- **Section switching**: the variation change is *quantized to the bar/beat boundary* (configurable: end-of-bar, end-of-pattern, immediate). Until the boundary, the current section continues; then an atomic swap of the per-role patterns.
- **Fill scheduling**: when Fill is pressed, the scheduler inserts the fill pattern until the next bar boundary, then switches to the target variation (optional auto-fill on-change).

---

## 11. Arranger design

**Sections** (subtypes of `Section`, with loop/one-shot semantics):

| Section | Behavior | Transition |
|---|---|---|
| Intro 1/2 | one-shot, then → current Variation | non-looping; counts the bars then moves on |
| Variation A/B/C/D | continuous loop | swap quantized to the boundary |
| Fill A/B/C/D | short one-shot (usually 1 bar) | then back to the variation (or on to the target) |
| Break | partial reduction/silence, short loop | manual |
| Ending 1/2 | one-shot, then **stop** transport | ends the song |

**Chord following.** The Chord Engine produces a `ChordState {root_pc, type, bass_pc, note_set}`. Each `TrackRole` has a **policy**:

| Role | Follows chord? | Transpose | Voicing | Notes |
|---|---|---|---|---|
| Drums | No | No | — | fixed pattern, drum channel |
| Percussion | No | No | — | fixed |
| Bass | Yes | root/bass | on-bass/inversion | respects slash chord |
| Chord 1 | Yes | yes | triad/close | primary voicing |
| Chord 2 | Yes | yes | alternate voicing (open/drop) | avoids unison with Chord1 |
| Pad | Yes | yes | smooth/voice-leading | minimal movement between chords |
| Arp | Yes | held notes/chord | — | see §14 |
| Phrase | Config: fixed or transposed | opt. | opt. | per-pad choice |
| Lead | Opt. scale-lock | opt. | — | quantize-to-scale |
| CC | — | — | — | automation only |

**Bass inversion / on-bass.** If the chord has a bass note different from the root (slash chord, e.g. C/E), the Bass uses `bass_pc`; the policy chooses between: root-only, root+fifth, walking (from a pattern), or automatic inversion to minimize the jump.

**Voicing / voice-leading resolver.** Transforms the Pattern's *relative degrees* into concrete MIDI notes given `ChordState` + Key. Modes: `Close`, `Open`, `Drop2`, `RootPosition`, `SmoothVoiceLeading` (minimizes movement relative to the previous chord, within a range). Clamps into the device's note-range.

**Chord modes (fingering).** Enum `ChordMode`: `SingleFinger`, `Fingered`, `FingeredOnBass`, `FullKeyboard`, `AIFingered` (opt.). 
- Split keyboard: below the `split_point` → chord recognition; above → realtime parts routed to the Zones.
- Full keyboard mode: recognizes chords of ≥3 notes anywhere, without a split.
- Single finger: 1–2 keys → major/minor/7 chord according to rules.
- Fingered: you play all the notes of the chord (up to ~15 types).

**Chord hold / memory.** `Chord Hold` keeps the last recognized chord even with hands lifted (the arranger keeps going). `Chord Memory` (opt. P2) associates a stored chord with a pad/key.

**Split & layer.** `Zone[]` (Roland-inspired): each Zone = key range + destination (port+channel) + transpose + velocity curve + on/off. Allows multiple splits and layers toward different devices. The "chord recognition" zone is a special Zone that feeds the Chord Engine instead of playing.

**Track enable / mute mask** as a recallable object (Performance/Scene).

**Live piano→chord harmonizer (host-live driver + core-portable detector) (`2310`/`2320`).** The problem: on a *running* arrangement, playing a chord on the keyboard should make the whole band follow it from there on — the "substantial impact" a pro arranger keyboard gives the player, not just a one-shot voicing. Today the arranger's NTT resolution (`3110`) reads the `ChordState` set by `chord play` or by the recorded `ChordSequencer` (`2430`); this feature adds a **third, live source**: the keys you are holding right now. `Arranger::on_tick` / `fire_arranger` keep reading `m_chords.state()` unchanged — only the *origin* of that state can now be the live keys.

- **Detector (core-portable).** A freestanding `ChordDetector` (`components/arrangrr/include/arrangrr/chord/chord_detector.hpp`) maintains the set of currently-held input notes — a 128-bit held-note set (16 bytes), zero heap, bounded, `constexpr`-friendly, in the same flash/RAM discipline as the rest of the core (`0200`). When ≥3 notes are held it recognizes the chord exactly like shell-mode entry (`theory::complete_shell_full`, `2210`/`2230` mode C): the lowest held note is the root, the pitch classes above it complete the quality (smart per `2240`, override rules unchanged).
- **Steering, not sounding.** The recognized chord is pushed into the `ChordEngine` through a new `ChordEngine::set_context(root_pc, quality)` that updates the harmonic context **without** emitting a voicing. The played notes already sound through normal routing; the detector only *steers* the arranger's NTT `ChordState`, so there is **no double-voicing**.
- **Engine wiring (host-live driver).** The engine gains a live-detection toggle and a designated chord-detect input port. When enabled, `push_midi_in` on that port taps note-on/note-off into the detector and updates the context on every successful recognition.

Two locked behavioral decisions:

- **Chord-memory / hold-last.** The context updates *only* when a chord is recognized (≥3 notes). Releasing the keys (dropping below 3 held) leaves the **last** recognized chord in place — the band keeps playing on it until you play a new one. This matches professional arranger keyboards and extends the "Chord hold / memory" rule above.
- **Whole-keyboard toggle (MVP).** For the MVP the *whole* simulated keyboard acts as chord input while `chord detect` is ON (a simple toggle). Keyboard SPLIT / zones (low = chords, high = melody) — the special chord-recognition Zone of "Split & layer" above — is an explicitly **deferred** refinement (core-portable too, out of this pass).

**ABI (`0700`/`1330`).** New stable command `Param::kChordDetect = 34` — `set: a = 0/1` (enable live chord detection from the input port), `b = input port`; the next id after `kStyleSwitch = 33`. `kChordHold = 17` (previously reserved for "live-keyboard gestures") stays reserved/unsupported; this feature uses the dedicated `kChordDetect` id.

**STM32 footprint (honest, `0400`/`12200`).** Not zero: the `ChordDetector` (16-byte held-set + small counters) plus the two engine flags and the port add a small, honest amount — roughly a couple dozen bytes on the engine object. The detector and `set_context` are core-portable (compile on both targets, `0300`); the `chord detect on\|off` command surface and the `kChords` panel that shows the recognized chord name are host-live (Host UI track, band `11000` / docs/gui-and-ux.md).

**Register anchors + per-role default voice (`3230`).** `resolve()` (the NTT core) anchors each `TrackRole` at its own default register — `kRoleAnchor[kRoleCount]` = chord-tone 0 at octave 0 for that role: bass low (36), pad under the mid comp (48), chord1/chord2/drums/perc/cc mid (60), arp/phrase/lead above (72). Before this, only Bass and Chord1 had a sane register; adding Pad/Perc/Chord2/Arp to a style meant either silence-by-omission or every new role fighting Chord1 for the same octave. `StyleEvent.octave` still fine-tunes per pattern; `RolePolicy::kFixed` roles (drums/perc) ignore the anchor entirely (their `tone` is already a literal MIDI note). Paired with this, `StylePattern.gm_program` (default `-1` = leave the synth's voice as-is) lets a style declare a default GM instrument per role, emitted as a Program Change by `Arranger::emit_voices` on style load/switch/route (same ABI as `kProgram`, `3240`) — so switching from `basic` to `bossa` can also switch the pad from a string patch to a nylon guitar patch without a manual `program` command. Both are the direct enabler for the 8-style-parts effort (band `9000`): they are what makes a `kPad`/`kArp`/`kChord2` addition to a style *sound* right by default instead of requiring hand-tuned octave/voice per pattern.

**Program change / voice selection (`3240`/`12500`).** `Engine::cmd_voice` (ABI `Param::kProgram = 35`) sends a Program Change on a given `port:channel` so `arrangrr` — not only the player twiddling the external synth by hand — can pick the voice a role plays through. Host surface: `program <port>[:ch] <voice>`, where `<voice>` resolves against a 128-entry GM instrument name table (host-side only, per `0700`/`1330`: the core never carries display strings, just the numeric 0..127 program id). This is deliberately the thin slice of the full §18 `Program`/`ExternalSound` model — no bank-select (CC0/CC32), no CC-init sequence, no `DeviceProfile` — those remain `12500`; today it is exactly one MIDI message (Program Change) per invocation, wired for both manual use (`program 1:3 "Nylon Guitar"`) and automatic use (`emit_voices` above).

---

## 12. Sequencer design

- **Model**: `Pattern` per track with independent `length_steps` ⇒ natural **polymeter** (track A 16 steps, track B 12 steps). Each track has a resolution (steps/beat) and a MIDI destination.
- **Step data (per step)**: `on/off`, `note(s)`, `velocity`, `gate/length`, `tie`, `rest`, `probability`, `ratchet` (repetitions within the step), `micro-timing offset`, `condition` (e.g. "1/2", "fill only"). This covers parameter-lock-style (Elektron) for per-step CC/velocity.
- **Live recording**: timestamped input → optional quantize-after (grid+strength+swing); replace vs overdub; count-in; record loop.
- **Mute/Solo**: per-track mask, with exclusive solo; recallable from Scenes.
- **CC lanes**: per-track automation (CC/pitchbend) with step points, step/linear interpolation (linear only in the tools → in realtime pre-computed to steps to avoid floats).
- **Song mode / Scene**: chain of patterns/scenes with repetitions; `Scene` = snapshot of which patterns/mutes/routing are active.
- **Probability/ratchet/tie/rest/gate**: all deterministic (probability via seeded PRNG for reproducibility in tests).
- **Interaction with the Arranger**: the sequencer and arranger share the Transport and Scheduler; a sequencer track can be routed as an "extra track" alongside the arranger roles.

---

## 13. MIDI Looper design

- **Buffer**: per-track, bounded `StaticVector<Event, N>` (declared capacity, e.g. 4k events/loop). If full → warning, not crash.
- **States**: `Empty → Recording → Playing → Overdub → (Replace) → Stopped`. Explicit FSM.
- **Record**: captures incoming MIDI events with absolute ticks (relative to the loop start).
- **Overdub**: adds events to the existing buffer (merge), the loop keeps going.
- **Replace**: replaces events within a window (erases in the range and records new ones).
- **Erase/Undo**: bounded `Undo ring` (lightweight snapshots or an operation journal) — e.g. last 1–4 states.
- **Quantize after record**: applies optional grid+strength post-recording (non-destructive if you keep the raw).
- **Loop length**: fixed (set beforehand), or auto (first cycle defines the length), or quantized to bars. Per-track loop length (polymeter) or global loop.
- **Sync**: aligned to the arranger's Transport (loop start at a bar boundary) and to the external clock if slave. `Capture` (retrospective): an always-active ring buffer enables "capture the last N bars already played".
- **Note safety**: on stop/erase, sends Note Offs for the looper's active notes (anti-stuck).
- **Re-harmonize/transpose live via the Note tracker (Francesco #5, a hard constraint, not a footnote)**: every re-voicing of sounding material (chord/key change while a loop is running) must go through the **Note tracker**, which owns all the active notes and emits the **NoteOffs before the new NoteOns**. Otherwise the chord-change-in-loop is a stuck-note factory.

---

## 14. Arpeggiator design

**Is it worth including?** Yes but in **MVP2** — high live value, medium complexity, hooks in well with Chord Engine + Transport.

- **Modes**: `Up, Down, UpDown (incl/excl endpoints), DownUp, AsPlayed, Random, ChordRepeat (whole chord rhythmically played), Gated`.
- **Octave spread**: 1–4 octaves, direction.
- **Rate**: sub-division of the tick (1/8, 1/16, triplets…), gate length, swing (shared with the Groove Engine).
- **Latch/Hold**: keeps the notes even with hands lifted; adds/removes notes to/from the latched set.
- **Sync**: to the Transport (start on beat/bar) and to external clock.
- **Pattern arp / Rhythm arp**: sequence of steps (on/rest/velocity/accent) applied to the note order ⇒ a rhythmic-melodic "pattern" (concept like the Roland/Korg pattern-arp).
- **Chord Engine interaction**: the arp can take (a) the physically held notes, or (b) the recognized chord (`ChordState`) expanded according to voicing. Configurable per-arp.
- **Style Engine interaction**: the arranger's `Arp` role *is* an instance of the arpeggiator fed by the current chord; the "performative" arp (on a Zone) is another instance. ⇒ A **single reusable ArpEngine**, instantiated N times with different sources.

---

## 15. Phrase Pads / Pad Engine

- **Pad types**: `Phrase` (recorded/preset MIDI sequence), `Chord` (one-shot/held chord), `Drum` (note(s) on drum channel), `CC` (sends CC/value), `FillTrigger` (launches an arranger Fill), `SceneTrigger`/`VariationTrigger` (changes state), `NoteRepeat`.
- **Trigger modes**: `OneShot`, `Loop`, `Hold` (plays while pressed), `Toggle` (on/off).
- **Sync**: `Immediate`, `ToBeat`, `ToBar`, `ToPattern` (quantizes the launch to the boundary).
- **Pitch behavior**: `FixedPitch` or `TransposeWithChord` (follows `ChordState`) — for phrase/chord pads.
- **Structure**: `Pad {type, mode, sync, source(patternId/note/cc), destination(port+ch), pitchPolicy}`. Banks of 4 (Yamaha Multi Pad inspiration) → `PadBank[]`.
- **Reuse**: the "phrase" pads play back a `Pattern`/`Event[]` through the same scheduler; no separate engine.

---

## 16. Chord Sequencer

- **Model (D28, functional)**: `ChordSequence = StaticVector<ChordStep, N>` with per-sequence `key_ref`; `ChordStep {tick_start, dur_ticks, degree, quality_ovr?, alt[], inversion, abs_override?}`. The degree is relative to `key_ref`; `abs_override` (root_pc+quality) is used only for chromatic/borrowed chords (`mod sec/borrow`). The resolution degree→concrete chord→voicing happens at playback via **NTT** (D24). `transpose to <key>`/`±semi` and mode change re-derive the degrees (not blind transposition).
- **Recording**: from live keyboard (with quantization to bar/beat) or step-input.
- **Playback**: emits `ChordState` toward the Arranger in place of the hands ⇒ you free your hands to play the lead on top.
- **Editing**: insert/delete/transpose steps; loop; global transpose.
- **Use with arranger**: when active, it is the *chord source* (priority over live recognition, or configurable merge).
- **Live use**: 4–8 bar loop of a progression while you play on top; combinable with Song mode.

---

## 17. Performance / Registration / Preset model

`Performance` = live state recallable with one button (the "Registration/Combi/STS"). It saves **references + state**, not heavy contents (which live in the Project):

```
Performance {
  name
  style_id, current_variation
  tempo_x100, master_transpose, key
  split_point, chord_mode, chord_hold
  track_enable_mask, mute/solo state
  routing_profile_id            // in→out matrix + Zones
  zones[]                       // split/layer toward device
  per_track_program_id[]        // references to external Programs (bank/PC/CC init)
  pad_bank_id                   // pad assignments
  arp_state[]                   // mode/rate/latch per arp instance
  chord_sequence_id (opt.)
  scene_refs[] (opt.)           // for song mode
  controller_map_id             // MIDI learn
}
```

- **Recall**: applies the state atomically; the "init" emissions (PC/Bank/CC) toward the devices happen in an ordered and throttled way on activation.
- **SetList**: `SetList = ordered list of Performance/Song` (Korg Set List / Yamaha Registration Sequence inspiration) for the live setlist.
- **Snapshot vs preset**: the Performance is a *state preset*; a `Scene` (inside Song) is a timed snapshot. They share the fields but Scene is anchored to time.

---

## 18. Device Profile / External Program model

Since there are no internal sounds, an external synth is described by a **DeviceProfile**, and a specific sound by a **Program/ExternalSound**:

```
DeviceProfile {
  name                          // "Volca Keys", "MODX ch1", "Reaper track 3"
  default_port
  drum_map (opt.)               // name→note (GM/GS/XG/custom)
  supported_cc[]                // known CCs + name (for UI/learn)
  note_range_min/max
  velocity_curve_id
  bank_select_mode              // MSB-only / MSB+LSB / none
  init_messages[]               // bounded: init CC/PC/SysEx sequence
  panic_messages[]              // panic override (devices with quirks)
  quirks_flags                  // e.g. "no running status", "needs GM reset", "ignores CC64"
}

Program / ExternalSound {
  name (local, for UI)
  device_profile_id
  port, channel
  bank_msb, bank_lsb, program_change
  cc_init[]                     // bounded: (cc, value) pairs on activation
  transpose, octave
  note_range_min/max (override)
  velocity_curve (override)
}
```

- **Init messages**: emitted (ordered, throttled) when the Program is activated in a Performance/Scene.
- **Per-device panic**: global default, override for devices with quirks.
- **Velocity curve**: 128-entry LUT (fixed) per curve; no floats in realtime.
- **Editor** for the profiles: laptop-only tool (YAML/JSON), then compiled into the Project binary.

---

## 19. PC simulator (design)

Three executables/targets on top of the **same core**, distinguished only by the HAL adapters:

1. **Headless deterministic runner** (`sim_headless`):
   - Input: file of timestamped MIDI events + user-command script (JSON/text, *outside* the core).
   - **Simulated** clock (ticks advanced by the runner, virtual time) ⇒ zero dependence on the wall-clock.
   - Output: stream of MIDI events (canonical textual dump) compared with **golden file**.
   - Use: unit/regression tests, CI, TDD of the engines.

2. **Live virtual MIDI** (`sim_live`):
   - HAL adapter on **ALSA seq / JACK / PipeWire** virtual MIDI (creates virtual in/out ports).
   - Real high-priority clock; the core runs as on the device.
   - Use: playing with a real keyboard → engine → real synths/DAW.

3. **DAW/hardware integration**: same `sim_live`, with routing toward external ports; master/slave clock modes toward DAW/hardware.

4. **Debug UI** (`sim_ui`, thin, *not in the core*):
   - Shows: BPM, position (bar:beat:tick), recognized chord, section/variation, tracks+mute, routing/Zones, MIDI in/out stream, jitter meter.
   - Renders the **same headless `ui_model`** that will run on the small display ⇒ the final UI is already validated.

**Simulator components** (all tool-side):
- **MIDI Monitor** (readable dump + filter).
- **Replay** (replays a recorded session → determinism).
- **Golden test harness** (`regression_runner`): runs N scenarios, diff against golden, report.
- **Fault injection**: jitter on the incoming clock, corrupted bytes, aggressive running status, clock drift, buffer starvation ⇒ verifies robustness.
- **Clock simulation**: virtual (headless) and real (live); the **external clock sim** generates F8/Start/Stop/SPP with controlled drift/jitter to test the slave.

---

## 20. STM32 portability (concrete rules)

- **Zero dynamic allocations in the core, everywhere (D32)**: no `new`/`malloc`/`std::vector`/`std::string`/`std::map`/`std::function` — neither in the hot path nor at init/load. Load fills **pre-sized static pools** (`std::array`/`StaticVector`), it does not allocate. The heap (if ever) exists only in the host tools.
- **Compile-time-first (D32)**: `constexpr`/`consteval`/`constinit`, `constexpr` data tables (chords/scales/NTT/velocity/GM), enum→handler dispatch resolved at compile time, `concepts` for contracts, `static_assert` on every dimension/limit. Compile-time polymorphism (templates/CRTP) in the hot path; `virtual` only at the HAL boundaries.
- **Bounded containers**: `StaticVector<T,N>`, array+count, SPSC ring buffer; `constexpr` capacities + `static_assert`.
- **No exceptions / no RTTI / no `iostream` / no `std::string`** in the firmware path. Errors via codes/`enum`/**our own `Result<T,E>`** (Francesco M9: **NOT** `std::expected` — not guaranteed freestanding on arm-none-eabi).
- **C++ standard: C++26 as target, with realism.** Host = GCC 16 (already present, excellent `-std=c++26` support). Firmware = recent `arm-none-eabi-gcc` (GCC 14+/15+) compiled **freestanding** with `-std=c++26 -fno-exceptions -fno-rtti -fno-threadsafe-statics`. The **shared core** uses a *subset* that compiles on *both*: modern "zero-cost" features are leveraged (`constexpr`/`consteval`, `enum class`, `std::array`, `std::span` where available, `[[nodiscard]]`, `concepts`, designated initializers, `std::bit_cast`), but **everything that requires heap/RTTI/exceptions/`<iostream>`/`<string>`/`<expected>` is avoided** and **our own `Span<T>`, `StaticVector<T,N>`, `RingBuffer`, `Result<T,E>`** are provided so as not to depend on the completeness of the embedded libc++/libstdc++. **C++26 realism (Francesco M8/M9):** static reflection and contracts are **NOT in any shipping arm GCC** and will not be soon → **do not design anything that *depends* on C++26**; treat `-std=c++26` as "real C++20/23 sugar + a few extras where it compiles". **Pin the minimum version** of `arm-none-eabi-gcc` (≥14, preferably 15) in the toolchain file. Operating rule: *a feature enters the core only if CI compiles it on both targets* (the list is **positive/CI-verified**, not aspirational).
- **No filesystem / no OS calls in the core**: storage/time/MIDI via **HAL** (`IClock`, `IMidiIn`, `IMidiOut`, `IStorage`, `IInput`, `IDisplay`).
- **Ring buffers** between ISR (UART/USB MIDI) and main loop (lock-free SPSC). The ISR does the minimum: copies bytes into the ring; parsing happens in the loop.
- **Static asserts** on struct sizes, alignments, budgets (`static_assert(sizeof(Project) <= BUDGET)`).
- **Explicit, anchored budgets (D33)**: anchor **STM32H743** (1 MB RAM / 2 MB flash), total pool envelope **≤512 KB** verified by `static_assert`; hot path in the **192 KB DTCM** zero-wait; compiled read-only contents **in memory-mapped flash** (RAM only for what is mutable); `sizeof(Event)==8` asserted; tick jitter < 1 ms (per message start), in→out latency < 3 ms. A **validator** (tool) rejects projects that exceed the limits before export.
- **Binary serialization**: fixed layout, declared endianness (little-endian), `magic + version`, reserved fields for extension, **CRC32** over the whole blob. Versioning with migration *tool-side only* (the device reads its own version or refuses with a message).
- **Graceful degradation**: if a buffer is full → drop CC before notes, never clock; if storage is absent → volatile mode; watchdog + crash-recovery (all-notes-off at boot).
- **Determinism**: no `Date.now`/unseeded random; explicit PRNG.
- **Open finding (2026-07-03): static core state lands in `.data`.** The ARM link gate shows the ~97 KB `Engine` in `.data` (flash copy at boot) because member defaults are non-zero (`Track.length`, default velocities). Future core task: zero defaults + explicit runtime init so large pools live in `.bss`.
- **Open finding (2026-07-03): `sizeof(ScheduledEvent)` is 16 B, not the 12 B budgeted in D33** (scheduler pool 64 KB vs 48 KB). Either revise the budget or shrink the sequence counter to u16; `packed` is not acceptable on Cortex-M7 without an alignment/performance discussion.

**HAL interfaces (draft):**
```cpp
struct IClock   { virtual uint64_t nowMicros() = 0; /* or tick source */ };
struct IMidiIn  { virtual size_t read(Span<uint8_t> buf) = 0; };      // non-blocking
struct IMidiOut { virtual size_t write(Span<const uint8_t>) = 0; };   // per port
struct IStorage { virtual bool read(uint32_t off, Span<uint8_t>) = 0;
                  virtual bool write(uint32_t off, Span<const uint8_t>) = 0; };
struct IInput   { virtual InputEvent poll() = 0; };  // debounced buttons/encoders
struct IDisplay { virtual void present(const UiFrame&) = 0; };
```
The core exposes: `onTick()`, `pushMidiIn(port, bytes, ts)`, `pushControl(cmd)`, `drainMidiOut(sink)`.

---

## 21. File formats

Two separate worlds (golden rule: **text on the tool side, binary on the device side**):

- **Authoring source (tool, laptop)**: **YAML/JSON** for Style/Pattern/DeviceProfile/Performance, readable and diffable in git. Schema with `schema_version`. Validation with JSON Schema.
- **Compiled binary (device)**: produced by the **Style Compiler**; fixed layout, `magic("ARGR") + format_version + payload + CRC32`. Sections: header, tables (styles, patterns, programs, device profiles, performances, songs), event pool. `u16` indices/IDs, no pointers.
- **Style Compiler pipeline (tool)**: YAML/JSON → validation → **limit validation** (against the STM32 budgets) → binary. Rejects with a report if it overshoots.
- **SMF (Standard MIDI File) import/export (tool, P2)**: import → pattern/song (with optional quantize); export → SMF Type 1 for sharing/backup. Laptop-side only; the core does not parse SMF.
- **Schema versioning & compatibility**: `format_version` in the binary; the device accepts versions ≤ its own and migrates *upstream* (in the tool), not at runtime. Reserved fields for minimal forward-compat.
- **Device backup/restore**: via SysEx (P2) or via storage HAL (SD/flash) — dump of the binary blob + CRC.

---

## 22. Canonical numbered roadmap (WBS) — pointer

**The canonical numbered roadmap and decision record now lives in
`docs/roadmap.md`, not here.** This section used to carry a full, independently
maintained copy of the tree; keeping two canonical copies is exactly the kind
of drift this reconciliation exists to stop (`docs/roadmap.md` had gone stale
on `1270`, the `2500` band, and `5210`/`5220`/`7130` while this section stayed
current — see `docs/roadmap.md`'s own header for the full account). As of
2026-07-18, `docs/roadmap.md` is the single canonical roadmap: every node
status, feasibility label, dependency flag, and owner decision lives there and
only there. Edit `docs/roadmap.md`, not this section, when a node's status
changes.

The old D-code/milestone migration appendix (mapping every retired `D1`–`D54`
and `M0`–`M13` identifier to its numeric node ID) has moved with the rest of
the tree — see `docs/roadmap.md`'s own "Appendix — migration cross-reference"
at the end of that file.

For the invariants band (`0000`), the full canonical tree (`1000`–`12000`),
the recommended sequence, the constraints/flags, and the full owner-decision
log — go to `docs/roadmap.md`. For long forensic evidence (commit-by-commit
citations) on already-✅ nodes that was trimmed out of the live tree to keep
it lean, see `docs/roadmaps/completed-nodes-archive.md`.

## 23. CLI API Design (final — 3-layer architecture, `1320`)

**Philosophy (`1320` CLI design + `0600` openness).** The `arrangrr` CLI is a **thin client** over a headless core, designed in **3 stacked layers** — so a single API honors musical ergonomics, openness and determinism all at once:

| Layer | Name | What it is | Who it serves |
|---|---|---|---|
| **L2** | **Surface — "Musician REPL"** | Terse verb-first sugar (`key C major`, `play D`, `loop on`, `start`). Convenient aliases, little punctuation, designed for **playing by hand**. | Live use, minimal-deep. |
| **L1** | **Model — addressable paths** | Every command is an operation on a **parameter space** (`get`/`set`/`do <path>`). Every piece of state is readable/settable/**mappable (MIDI-learn)**/automatable in a uniform way (D17b). | Openness/hackability, tooling. |
| **L0** | **Wire — JSONL protocol** | **Machine-first** wire: `{"cmd":…}` → / `{"ev":…}` ←, versioned. Deterministic, replay, **daemon+client, future GUI** (`0600`, `0100`). | Integration, golden, GUI. |

**Expansion rule:** L2 sugar **expands** into L1 operations (`play D` → `do chord.play D`), which **serialize** into L0 messages (`{"cmd":"chord.play","note":"D"}`). No hidden magic: `--echo-expand` shows the L2→L1→L0 expansion. The CLI can speak at **any layer** (`--format human|jsonl`, `--sugar on|off`). **Warning (`0700`/`1330`, Francesco fix #1/#2):** L0-JSONL with string paths is the **host encoding** (for CLI/GUI/tools); the **core sees neither JSONL nor strings** — it receives **binary typed** `Command`s (`{op, coll:u16, idx:u16, param_id:u16, value}`) over a ring buffer and emits POD `Event`s. The *string/JSONL → binary* translation happens in `platform/host`. That way there is no JSON parser nor `std::string` in the STM32 realtime path; collections are fixed-capacity arrays and user names stay host-side (standalone device: numeric slots).

### 23.1 Invocation
```
arrangrr [--backend alsa|jack|pipewire|null]   # host MIDI sink/source (default: auto)
         [--in <port|alias>]... [--out <port|alias>]...
         [--clock real|virtual]                # virtual = deterministic (batch)
         [--ppqn 96] [--tempo 120]
         [--script FILE | -]                   # runs commands (or stdin); then exits unless --repl
         [--replay FILE]                        # file with @tick timestamps, virtual clock, canonical event-log
         [--events all|none|<filter>]           # event stream verbosity
         [--load STATEFILE] [--seed N]
         [--format human|jsonl]                 # ack/event format
```
- **No args** → REPL, auto backend, real clock, event stream on the main types.
- **`--script s.acmd`** (or `arrangrr < s.acmd`) → runs and (without `--repl`) exits. With `--clock virtual` it is deterministic.
- **`--replay r.acmd`** → virtual clock driven by the timestamps, prints the **canonical event-log** on stdout for the **golden diff**.

### 23.2 Syntax
- One line = one command: `namespace verb [args…]`. Very frequent commands have short aliases.
- **Parameter paths** with dots: `set transport.tempo 120`, `get track.bass.dest.channel`.
- **Comments** `# …`. Optional **timestamp** at the start of the line (batch/virtual only): `@<tick>` or `@<bar:beat:tick>`; absent ⇒ "now / next tick".
- Notes: name (`C`, `F#3`, `Bb`) or MIDI number (`60`). Tempo BPM: `120` or `120.00` (internally `bpm_x100`).
- **Responses:** `ok [value]` / `err <code> <msg>`. In `--format jsonl`, acks and events are per-line JSON objects.

### 23.3 Namespaces & commands (v0)

**Transport / clock**
```
transport start | stop | continue | toggle
transport tempo <bpm> | transport ppqn <n>
transport locate <bar:beat:tick> | transport sync internal|external
advance <ticks|Nbars>          # virtual clock ONLY: advances time deterministically
```

**Ports / routing** (multi-port DIN+USB → on host these are virtual ports)
```
port list | port open in|out <name> [as <alias>]
route <in>[:ch] -> <out>[:ch] [drop cc|note|clock … | only …]
thru <in> -> <out> [soft|hard|off]
panic [<port>|all]
```

**Key / scale**
```
key <root> <mode>              # key C major | key A minor
scale <name>                   # override current scale
```

**Chord intelligence** (`2240` smart per degree, `2110`/`2240` diatonic+modifiers)
```
chord mode diatonic|single|shell
chord play <note> [mod …]      # mod: maj7 min7 dom7 dim7 sus2 sus4 add9 9 11 13
                               #      sec (secondary dominant) borrow (borrowed) inv<n>
chord hold on|off | chord stop
chord detect on|off [<port>]   # 2310/2320: held keys on the input port re-harmonize live
```
Default: *smart* richness per degree; `mod`s override it. Examples:
```
key C major
chord play D            # -> Dm7  (ii, smart)
chord play G            # -> G7   (V, smart)
chord play G mod sec    # -> D7   (V/V, secondary dominant)
chord play A mod borrow # -> Ab   (bVI borrowed) [explicit]
```

**Chord sequence** (`2410` — free durations; live-rec quantize-after or step-edit)
```
seq new <name> | seq use <name>
seq rec [quantize <grid>] | seq stop
seq add <chord> [len <beats|bars>]     # step entry with free duration
seq edit <i> [chord <c>] [len <d>] | seq del <i>
seq loop on|off | seq transpose <±semi | to <key>>
seq play [<name>] | seq show [<name>] | seq list
```

**Track** (the primitive; 3 gestures = 3 ways to fill)
```
track new <name> role <role> dest <out>:<ch>
track fill <name> write|generate|capture
track follow <name> on|off             # follows the chord (transform)
track voicing <name> close|open|drop2|smooth
track mute|solo|unmute <name>
```

**Arranger / style** (syntax planned for `3000`)
```
style load <name> | style section intro1|varA|varB|fill|break|ending1
```

**Program / voice selection** (`3240`/`12500` — thin ABI slice ahead of the full §18 model)
```
program <port>[:ch] <voice>    # <voice> = GM instrument name or 0..127; kProgram=35
```

**Addressable parameter model** (D17b)
```
get <path> | set <path> <value> | ls <path>
# e.g.: transport.tempo · chord.mode · track.bass.dest.channel · seq.verse.loop
```

**State / persistence** (`8400`/`8500`)
```
state dump [file] | state load <file> | state inspect [path]
project save <name> | project load <name>     # versioned binary + CRC
```

**MIDI raw / monitor / learn** (D17a/c/d)
```
midi send <port> <bytes…>
monitor on|off [filter: midi|chord|clock|section|<ev-type>]
learn <param-path>            # then move a control -> mapped
map list | map del <path>
```

**Meta**
```
help [topic] | echo <text> | wait <ms|Nt> | seed <N> | quit
```

**panel / piano / notes / view / filter (Host UI track — see docs/gui-and-ux.md)**
```
panel list | open <p> | close <p> | toggle <p> | close all
panel focus <p>|repl|next | panel status | panel help
piano octave <N>|up|down | piano channel <1..16> | piano velocity <1..127>
piano view keyboard|active-notes|event-log | piano panic       (11200)
notes names cde|doremi|toggle                                   (11200)
view show ... | filter channel|port|event|clear                 (11200)
```
Panel lifecycle lives ONLY under `panel ...` (the earlier `help open`/`help close` forms are removed); `help <topic>` remains — it sets help content and opens the help panel.

### 23.4 Event stream (←)
Each event is a line `ev <type> <fields…>` (or JSON with `--format jsonl`), filterable with `monitor`:
```
ev clock    tick 96 bar 1 beat 1
ev chord    in C -> Cmaj7 deg I
ev midi-out port synth ch 1 noteon 60 vel 100 @0
ev section  varA
ev warn     buffer-full dropped cc
```
In **replay/batch** the ordered event-log is the **canonical output** compared against the golden file.

### 23.5 Determinism & replay (`1000` bridge)
- With `--clock virtual` time advances **only** via `advance`/`@tick` ⇒ a session file with timestamps produces **identical output** (golden). `seed N` pins the PRNG (humanize/probability) when needed; without it, live may vary (`0100`).
- Deterministic session example (`hello_chord.acmd`):
```
# key + one smart diatonic chord, towards a virtual port
key C major
port open out virt as synth
chord mode diatonic
@0   chord play D          # ev: Dm7 -> noteon…
@96  chord stop
advance 192
```
  `arrangrr --clock virtual --replay hello_chord.acmd` → deterministic event-log → `diff` against golden.

### 23.6 Live REPL example (FIRST WOW, `2400`)
```
arrangrr> key C major
arrangrr> port open out virt as synth
arrangrr> seq new verse
arrangrr> seq rec quantize 1/1
arrangrr> chord play D        # Dm7
arrangrr> chord play G        # G7
arrangrr> chord play C        # Cmaj7
arrangrr> chord play A        # Am7
arrangrr> seq stop
arrangrr> seq loop on
arrangrr> transport start     # the progression loops and plays towards 'synth'
arrangrr> seq transpose +2    # the whole progression goes up a whole tone, live
```

### 23.7 Resolved conventions (`1320`) & layer mapping
- **Syntax:** L2 uses `verb` / `namespace verb` **with the space** (musician: `play D`, `seq verse`); L1 uses **dotted paths** (`do chord.play D`, `set seq.verse.loop on`, `get transport.tempo`). They are not "two conflicting styles": they are **two layers** — the former expands into the latter. `--sugar off` forces the L1 form.
- **Traceable expansion:** `--echo-expand` prints for each line the `L2 → L1 → L0` chain, so the surface is never magic (consistent with "open/inspectable").
- **Time in batch:** both mechanisms — `@<tick>`/`@<bar:beat:tick>` prefix to **schedule** a line, and `advance <ticks|Nbars>` to **advance** the virtual clock deterministically.
- **Format:** `--format human` (default, readable L2/L1) · `--format jsonl` (canonical L0, used for replay/golden and for clients/GUI). The ordered JSONL event-log is the **golden** artifact.
- **Example mapping (all 3 layers, same action):**
```
L2 (you type): play D
L1 (model):    do chord.play note=D
L0 (wire):     {"cmd":"chord.play","note":"D"}
   event ←:    {"ev":"chord","in":"D","out":"Dm7","deg":"ii"}
               {"ev":"midi-out","port":"synth","ch":1,"noteon":62,"vel":100,"@":0}
```

### 23.8 Open task: parameter-space schema (L1)
The **complete schema of addressable paths** (`transport.*`, `chord.*`, `seq.<name>.*`, `track.<name>.*`, `port.*`, `style.*`, `state.*`, `map.*`) is **itself a mini-spec** and must be defined in **`1000`/`1400`** together with the (versioned) L0 protocol. It is the contract that underpins CLI, MIDI-learn, automation, replay and (tomorrow) the GUI: it must be designed once and with care. Principles: stable names, explicit types, declared units (tick, `bpm_x100`, semitones), closed enums, every path `get`-able and (where sensible) `set`/`learn`-able. **The v0 spec is §24.**

---

## 24. L1 Param-Space & L0 Protocol — Spec v0 (contract)

This section is the **contract**: the addressable parameter space (**L1**) and the wire protocol (**L0**). The musician surface (**L2**) is pure sugar that expands into L1 operations. It covers what is needed up to **M5** (arranger); later namespaces (looper, arp, pad, performance, device) are added following the same schema.

### 24.1 Types & conventions
- **Types:** `bool` · `int` · `int(a..b)` (range) · `fixed(bpm_x100)` (integer ×100) · `enum{…}` · `string` · `note` (name `C`/`F#3`/`Bb` **or** 0..127) · `pos` (`bar:beat:tick`) · `ticks` · `semitones` · `array<T>` · `id` (string alias).
- **Access:** `r` (get) · `rw` (get+set) · `do` (action). **`learn`** = path bindable via MIDI-learn.
- **Units always declared.** No floats on the wire for realtime values: tempo is `bpm_x100`, durations in `ticks`/`bars`.
- **Collections** with parametric paths: `seq.<name>.*`, `track.<name>.*`, `port.<alias>.*`. The name is a stable user-chosen `id`.
- **Closed, versioned enums:** adding a value = `proto` minor bump.

### 24.2 L1 catalog (v0)

**transport.**
| path | type | access | notes |
|---|---|---|---|
| `transport.tempo` | fixed(bpm_x100) | rw · learn | default 12000 (=120.00) |
| `transport.ppqn` | int | rw | default 96; settable only when stopped |
| `transport.state` | enum{stopped,playing,paused} | r | |
| `transport.position` | pos | r | bar:beat:tick |
| `transport.sync` | enum{internal,external} | rw | slave to external clock |
| `transport.start` / `.stop` / `.continue` / `.toggle` | — | do · learn | |
| `transport.locate` | do(pos) | do | rebuilds arranger state |
| `transport.advance` | do(ticks\|bars) | do | **virtual clock only**: advances deterministically |

**key. / scale.**
| path | type | access |
|---|---|---|
| `key.root` | enum{C,C#,D,…,B} | rw · learn |
| `key.mode` | enum{major,minor,dorian,phrygian,lydian,mixolydian,locrian} | rw · learn |
| `scale.name` | string | rw | explicit override |

**chord.** (D19 smart, D20 diatonic+modifiers)
| path | type | access | notes |
|---|---|---|---|
| `chord.mode` | enum{diatonic,single,shell} | rw · learn | default diatonic (B) |
| `chord.richness` | enum{smart,triad,seventh,extended} | rw | default smart |
| `chord.hold` | bool | rw · learn | |
| `chord.current` | string | r | last recognized chord (e.g. "Dm7") |
| `chord.play` | do(note, mod:array<enum>?) | do · learn | mod: `maj7 min7 dom7 dim7 halfdim sus2 sus4 add9 9 11 13 sec borrow inv1 inv2 inv3` |
| `chord.stop` | — | do | |

**seq.** (collection of ChordSequence; D14 free durations)
| path | type | access | notes |
|---|---|---|---|
| `seq.<n>.loop` | bool | rw · learn | |
| `seq.<n>.length` | ticks | r | |
| `seq.<n>.transpose` | semitones | rw · learn | |
| `seq.<n>.playing` | bool | r | |
| `seq.<n>.steps` | array<{i,chord,len_ticks}> | r | inspectable structure |
| `seq.new` | do(name) | do | |
| `seq.use` | do(name) | do | "current" seq for the short commands |
| `seq.<n>.rec` | do(quantize?) | do · learn | live-rec, quantize-after |
| `seq.<n>.stop` | — | do | |
| `seq.<n>.add` | do(chord, len?) | do | step-entry, free duration |
| `seq.<n>.edit` | do(i, chord?, len?) | do | |
| `seq.<n>.del` | do(i) | do | |
| `seq.<n>.play` / `.show` | — | do | |
| `seq.<n>.transpose!` | do(by\|to) | do · learn | action (in addition to the rw leaf) |
| `seq.list` / `seq.count` | do / r | | collection meta |

**track.** (the primitive; 3 gestures)
| path | type | access | notes |
|---|---|---|---|
| `track.<n>.role` | enum{drums,perc,bass,chord1,chord2,pad,arp,phrase,lead,cc} | rw | |
| `track.<n>.dest.port` | id | rw | out port alias |
| `track.<n>.dest.channel` | int(1..16) | rw · learn | |
| `track.<n>.fill` | enum{write,generate,capture} | rw | which gesture fills the track |
| `track.<n>.follow` | bool | rw · learn | follows the chord (transform) |
| `track.<n>.voicing` | enum{close,open,drop2,smooth,root} | rw | |
| `track.<n>.transpose` | semitones | rw · learn | |
| `track.<n>.octave` | int | rw · learn | |
| `track.<n>.mute` / `.solo` | bool | rw · learn | |
| `track.<n>.length` | int(steps) | rw | polymeter |
| `track.new` | do(name, role, dest) | do | |
| `track.<n>.remove` | — | do | |

**style.** (arranger, M5 — contract already now)
| path | type | access |
|---|---|---|
| `style.current` | string | r |
| `style.load` | do(name) | do |
| `style.section` | enum{intro1,intro2,varA,varB,varC,varD,fillA,fillB,fillC,fillD,break,ending1,ending2} | rw · learn |
| `style.section.trigger` | do(name) | do · learn | (quantized to the boundary) |

**port. / route. / thru.** (multi-port DIN+USB; on host = virtual)
| path | type | access |
|---|---|---|
| `port.list` | do → array | do |
| `port.open` | do(dir:enum{in,out}, name, as?) | do |
| `port.close` | do(alias) | do |
| `port.<alias>.dir` | enum{in,out} | r |
| `route.add` | do(from:"alias[:ch]", to:"alias[:ch]", filter?) | do |
| `route.list` / `route.del` | do / do(id) | do |
| `thru.set` | do(from, to, mode:enum{soft,hard,off}) | do |
| `panic` | do(port?) | do · learn |

**map.** (MIDI-learn — D17c)
| path | type | access |
|---|---|---|
| `map.learn` | do(param-path) | do | arms the learn: the next MIDI source → bind |
| `map.add` | do(param-path, source:"port:ch:cc\|note") | do |
| `map.list` / `map.del` | do / do(param-path) | do |

**state. / project.** (D21)
| path | type | access |
|---|---|---|
| `state.dump` | do(file?) | do | inspectable text/JSON snapshot |
| `state.load` | do(file) | do |
| `state.inspect` | do(path?) | do |
| `state.seed` | int | rw | PRNG seed (humanize/probability) |
| `project.save` / `project.load` | do(name) | do | versioned binary + CRC |

**monitor. / meta.**
| path | type | access |
|---|---|---|
| `monitor.set` | do(filter:enum{all,none,midi,chord,clock,section,warn}, on:bool) | do |
| `meta.hello` | do(proto) | do | version handshake |
| `meta.version` | string | r |
| `meta.help` | do(topic?) | do |
| `meta.echo` / `meta.wait` / `meta.quit` | do | | `wait` in ms (real) or ticks (virtual) |

### 24.3 L0 protocol (JSONL, versioned)
One JSON object per line, UTF-8. **client→core = commands**, **core→client = responses + events**.

**Command envelope** (any of the 4 ops — `get`/`set`/`do`/`ls` — on an L1 path):
```json
{"op":"do","path":"chord.play","args":{"note":"D"},"@":0,"id":5}
{"op":"set","path":"transport.tempo","value":12000}
{"op":"get","path":"seq.verse.loop","id":6}
{"op":"ls","path":"chord"}
```
- `@` (optional): absolute execution tick (virtual clock). Absent ⇒ "now/next tick".
- `id` (optional): correlation; the response echoes it back in `re`.

**Responses** (core→client):
```json
{"re":6,"ok":true,"value":true}
{"re":5,"ok":true}
{"re":7,"ok":false,"err":"bad_note","msg":"unknown note 'H'"}
```

**Events** (core→client, unsolicited; filtered by `monitor`):
```json
{"ev":"chord","in":"D","out":"Dm7","deg":"ii","@":0}
{"ev":"midi-out","port":"synth","ch":1,"msg":"noteon","note":62,"vel":100,"@":0}
{"ev":"midi-in","port":"kbd","ch":1,"msg":"noteon","note":62,"vel":100,"@":0}
{"ev":"clock","tick":96,"bar":1,"beat":2,"@":96}
{"ev":"section","name":"varA","@":384}
{"ev":"transport","state":"playing","@":0}
{"ev":"warn","code":"buffer_full","detail":"dropped cc","@":123}
```

**Handshake / version** (first line):
```json
-> {"op":"do","path":"meta.hello","args":{"proto":1}}
<- {"ev":"hello","proto":1,"core":"arrangrr 0.1.0","ppqn":96,"format":"jsonl"}
```
Error codes (stable enum): `bad_path` · `bad_arg` · `bad_note` · `not_found` · `read_only` · `busy` · `unsupported` · `overflow`.

### 24.4 L2 → L1 → L0 expansion (examples)
```
L2:  play D
L1:  do chord.play note=D
L0:  {"op":"do","path":"chord.play","args":{"note":"D"}}

L2:  loop on
L1:  set seq.<current>.loop on
L0:  {"op":"set","path":"seq.verse.loop","value":true}

L2:  xpose +2
L1:  set seq.<current>.transpose +2
L0:  {"op":"set","path":"seq.verse.transpose","value":2}

L2:  play G mod sec
L1:  do chord.play note=G mod=[sec]
L0:  {"op":"do","path":"chord.play","args":{"note":"G","mod":["sec"]}}
```

### 24.5 Golden sessions (deterministic, virtual clock)
Format `.acmd` (L2 sugar with `@tick`); running with `--clock virtual --format jsonl` produces the canonical event-log compared against the `.golden`. (MIDI notes and ticks are illustrative; the exact values get pinned once the core exists — these files *are* the spec of the expected behavior.)

> **Robust goldens (Francesco #4):** separate the **protocol/timing** goldens (stable: event order, ticks, ports, `@`) from the **musical** ones. For harmony, **assert on chord identity + degree** (`{"ev":"chord","out":"Dm7","deg":"ii"}`), **not** on the exact MIDI notes of the voicing — otherwise every tweak to the voicing/NTT rewrites all the goldens and the tests become noise. Exact notes are asserted only in dedicated voicing goldens.

**G1 — `hello_chord.acmd`** (one smart diatonic chord → notes out)
```
meta.hello proto=1
key C major
port open out virt as synth
chord mode diatonic
@0    play D            # ii -> Dm7 (smart)
@96   chord stop
advance 192
```
Expected (excerpt):
```json
{"ev":"chord","in":"D","out":"Dm7","deg":"ii","@":0}
{"ev":"midi-out","port":"synth","ch":1,"msg":"noteon","note":62,"vel":100,"@":0}
{"ev":"midi-out","port":"synth","ch":1,"msg":"noteon","note":65,"vel":100,"@":0}
{"ev":"midi-out","port":"synth","ch":1,"msg":"noteon","note":69,"vel":100,"@":0}
{"ev":"midi-out","port":"synth","ch":1,"msg":"noteon","note":72,"vel":100,"@":0}
{"ev":"midi-out","port":"synth","ch":1,"msg":"noteoff","note":62,"@":96}
... (other noteoffs @96)
```

**G2 — `ii_V_I_vi.acmd`** (free-duration progression, loop, live transpose)
```
key C major
port open out virt as synth
seq new verse
seq.verse rec quantize 1/1
@0    play D            # Dm7
@384  play G            # G7   (1 bar later, at 96ppqn*4)
@768  play C            # Cmaj7
@1152 play A            # Am7
@1536 seq.verse stop
seq.verse loop on
transport start
advance 1536            # one loop cycle
seq.verse transpose +2  # whole progression +2 semitones, live
advance 1536
```
Expected: `chord` events for Dm7/G7/Cmaj7/Am7 on the first cycle; after `transpose +2`, the same degrees with raised root (Em7/A7/Dmaj7/Bm7) on the second cycle. Order and ticks are deterministic.

**G3 — `routing_panic.acmd`** (thru + note injection + panic)
```
port open in virt as kbd
port open out virt as synth
thru set kbd synth soft
@0    midi send kbd 90 3C 64     # noteon C4 on kbd
@10   panic all
advance 48
```
Expected: `midi-in noteon 60`, `midi-out noteon 60` (soft-thru), then on panic `midi-out` with All-Notes-Off/All-Sound-Off/Reset and explicit `noteoff 60` (anti-stuck) on every active channel.

**G4 — `secondary_dominant.acmd`** (explicit modifier, D20)
```
key C major
port open out virt as synth
@0    play G mod sec     # V/V -> D7 (secondary dominant)
@96   chord stop
advance 192
```
Expected: `{"ev":"chord","in":"G","out":"D7","deg":"V/V","@":0}` + D7 notes.

### 24.6 What remains to be pinned once the core exists
- Exact default voicing/octave per degree (the precise notes in the golden event-logs).
- Loop note-off policy on chord change (legato vs re-trigger).
- Final names of some enums (e.g. sections beyond M5).
- Whether `@tick` implies auto-`advance` up to that tick in "dense" script mode (proposed: no — `advance` is explicit, `@` is scheduling only).

### 24.7 Folded review nits (Francesco minor/major)
- **Chord modifiers on ORTHOGONAL axes (M11):** `chord.play` does not take a flat array but **separate axes** — `quality` (maj/min/dom/dim/halfdim/…), `extensions[]` (add9/9/11/13/…), `function` (diatonic/sec/borrow), `inversion` (0..3). Per-axis conflict validation; no ambiguous `[min7,maj7]`.
- **Durations in MUSICAL UNITS (M12):** steps/patterns store durations in **beats/fractions** (ticks at 960 are *derived*), so changing resolution does not reinterpret the contents. `transport.ppqn` in any case immutable **after** contents have been loaded.
- **`state.dump` does NOT touch the filesystem in the core (M13):** the core op is `serialize → Span<byte>`; file writing lives in `platform/host`. `state.dump [file]` is host sugar (consistent with D26).
- **`scale.name` = closed enum/registry** (not a free string), like `key.mode` → validatable/deterministic.
- **Structured `deg` in events** (`{"degree":2,"quality":"min7","alt":[]}`), not a free string, for machine consumption.
- **Key-aware enharmonic spelling** (Ab vs G#): `chord.current`/display use a key-dependent spelling algorithm — to be defined in M2.
- **Harmonizer velocity source:** dynamics of the generated chord = (a) input velocity or (b) fixed per-track value; **default = input**.
- **§9 compliance completed:** in addition to CC64 (sustain) include **CC66 (sostenuto)** and **CC67 (soft)**; parser note: real-time bytes (F8/FA/…) **do not reset** running status.
- **Chord-detection window + hysteresis (M8):** live modes use an aggregation **window** for notes + **hysteresis** (do not change chord until stable for N ms) to avoid chord-flicker → params `chord.window_ms`, `chord.hysteresis_ms`.
- **QEMU (M0):** the ARM gate is **freestanding compile+link**, not "run with peripherals"; a QEMU run is a bonus, not a criterion.
- **Polymeter × section-switch:** with tracks of different lengths, the switch "boundary" is defined on the **transport's global bar** (not on the individual track's length); to be detailed in M5/M6.

---

## 25. "Auto-accompaniment" feature evaluation (the classic 17)

Mapping of the 17 "the keyboard plays by itself" features onto the project. **Verdict: 12 already covered, 5 additive/to be elevated.** It confirms that the Living Timeline model, with chord intelligence first, contains them all. The chain the user summarizes — *minimal human input → harmonic interpretation → intelligent MIDI generation* — **is** exactly the north-star (`2000`/`0130`) and the first/second WOW.

| # | Feature | In arrangrr? | Where | Milestone | Note |
|---|---|---|---|---|---|
| 1 | **Arranger** | ✅ core | §11, band `3000` (generate-from-chord gesture) | `3000` | it is the 2nd WOW |
| 2 | **Chord recognition** (single/fingered/**multi**/full/**bass-inv**) | ✅ (add multi-finger) | §11, `2210`/`2220`/`2230` | `2100`/`2220` | multi-finger = variant to add; bass-inversion/slash already in §11 |
| 3 | **Style engine** | ✅ | band `3000`, §8 | `3000` | adapted MIDI phrases, not audio |
| 4 | **NTT / Note Transposition Table** | 🔼 **elevated** | `3110`, §8.2 | `3000` (concept from now) | was buried in the voicing resolver → now a core module |
| 5 | **Arpeggiator** | ✅ | §14, band `7000` | `7000` | reusable ArpEngine |
| 6 | **Chord memory** (1 key → chord) | 🔼 **elevated** | was §11 "opt P2" | `2220`/`7000` | maps key→memorized chord; becomes a MIDI-FX insert (`5000`) |
| 7 | **Chord sequencer / looper** | ✅ **1st WOW** | `2000`, §16 | `2400` | frees the left hand |
| 8 | **Auto-accompaniment** (the whole chain) | ✅ | §11+§14+§16 integrated | `3000` | = integrated arranger |
| 9 | **OTS / Keyboard Set** | ✅ | §17, §8 | `8000` | MIDI-only: auto-emits program/bank/CC + split/tempo/routing to the Style |
| 10 | **Registration / Performance / Scene** | ✅ | §17, band `8000` | `8000` | recallable snapshot |
| 11 | **Pads / Multi / Phrase Pads** | ✅ | §15, band `7000` | `7000` | follow the chord or fixed; banks of 4 |
| 12 | **Quantization** | ✅ | §12, §10 | `4000` (+quantize-after `6000`) | grid/swing/groove templates |
| 13 | **Scale assist / quantizer** | ✅ (elevate input-quantize) | scale-lock (band `5000`) | `4000`/`7000` | force input into scale → MIDI-FX insert (`5000`) |
| 14 | **Harmonizer** (melody → harmony) | 🔼 **additive** | new | `7000` | distinct from chord intelligence: 1 line → harmonized voices over chord/scale; MIDI-FX insert |
| 15 | **MIDI effects** (echo/strum/ratchet/prob/humanize/vel/scale-filter/delay/repeat/random/transpose) | 🔼 **elevated to a chain** | `5000` | `4000`/`7000` | new unifying concept "MIDI-FX chain" |
| 16 | **Pattern sequencer / clip launcher** | ✅ | §12 | `4000` | "write" gesture + scene launch |
| 17 | **Song mode / Scene chain** | ✅ | §12, band `8000` | `8000` | Intro→Verse→Chorus→…→Ending |

### 25.1 The 5 additive / elevated items (what changes in the plan)
- **#4 NTT — Style-Follow Resolver → first-class core module (`3110`).** It is *the* reason a phrase in Cmaj sounds right even over Am/D7/Fsus4/G-B without wrong notes: not mechanical transposition, but musical rules (degree, source-chord→target-chord tables, avoiding the "wrong" notes). It is the arranger's number-1 quality factor: without a decent NTT, "generate-from-chord" sucks. It must be designed in `3000`, but its place in the architecture (Transform layer, between ChordState and the track's output) must be reserved from now.
- **#15 MIDI-FX chain → first-class unifying concept (`5000`).** Instead of treating arp, humanize, scale-lock, transpose as scattered modules, they become **inserts** of a composable, bounded per-track/zone chain. Extremely strong for the "open/hackable" north-star (every insert has L1-addressable, mappable/automatable parameters). Many inserts are easy wins (transpose, velocity, scale-filter, note-repeat, echo/delay, strum, ratchet, probability, randomize, humanize).
- **#14 Harmonizer → additive insert.** A single melodic line → added voices following chord/scale. Different from chord intelligence (which starts from sparse input *as a chord*): here the input is *melody* and the output *harmonized*. Lives as an insert of the MIDI-FX chain (`5000`). `7000`.
- **#6 Chord memory → explicit insert/feature.** Maps "1 key → memorized chord". Realizable as a `chord-memory-expand` insert (`5000`) or as a Chord Engine mode. Elevated from a P2 note to a named feature. `2220`/`7000`.
- **#2 multi-finger → `chord.mode` variant.** Add `multi` to the `chord.mode` enum (§24.2) alongside diatonic/single/shell/full. `2220`.

### 25.2 Impact on modules/roadmap (no substantial ordering change)
- §3 (module table) and the roadmap tree (`docs/roadmap.md`, formerly §22): add **NTT Resolver** and **MIDI-FX chain** as first-class modules; **Harmonizer** and **Chord-memory** as inserts; **multi-finger** as a mode. *(Done: `3110` NTT resolver and `5000` MIDI-FX chain both exist as nodes in `docs/roadmap.md`.)*
- §24 (L1 contract): provide for the **`fx.<track>.<slot>.*`** namespace for the MIDI-FX chain (insert type, parameters, on/off, order) and `chord.mode += multi`. NTT has its parameters under `style.*`/`track.*` (follow rule, table).
- Roadmap: NTT inside `3000` (non-negotiable part of the arranger); MIDI-FX chain started in `4000` (transpose/velocity/scale-filter/note-repeat) and extended in `7000` (arp/harmonize/strum/echo as inserts). No slippage of the WOWs (`2400`/`3000`).
