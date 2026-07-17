# Per-style default harmonic progressions (host-side, feeding the existing `ChordSequence`)

Status: proposal, 2026-07-17. Author: Ottorino (style/arrangement analyst).
Follows directly from `docs/reflections/cli-vs-gui-ab-2026-07.md`, which measured
(not inferred) that the GUI never emits `key`/`chord`/`seq`, so every style sits on
a static tonic triad forever. Owner decision: give every built-in style a
default harmonic progression that loops automatically, so the style sounds
"right" even with zero live input; a live chord/detect/pad path is a later
increment on the same `ChordSequence` object, not a competitor to it.

Owner correction received mid-task (recorded here because it changes §2's
recommendation from the first draft): styles are **not** a fixed set — the
Yamaha/SFF importer (`apps/tools/arrstyle-converter`) is a real, working corpus
pipeline (1010-style corpus, `docs/backlog/yamaha-style-corpus-and-rules.md`).
Baking a progression into `StyleDef` (core) would not scale to imports and
would violate the very doctrine this proposal must respect. The mechanism
below reflects that correction: the progression is host-side data feeding the
core's own `ChordSequence` object through the `seq` verbs that already exist —
never a new `StyleDef`/ABI field.

## 0. Method

Read: the 16 style headers
(`components/core/arrangrr/include/arrangrr/arranger/styles/*.hpp`), the shell
grammar (`components/platform/hostrt/shell_parse.cpp`,
`shell_music_commands.cpp`), the core chord-sequencer
(`components/core/arrangrr/include/arrangrr/chord/{chord_sequence,chord_sequencer,
chord_engine}.hpp`), the ABI (`components/core/arrangrr/include/arrangrr/abi.hpp`),
the GUI translator and auto-router
(`apps/gui-sonotron/src/in_process_brain_session.cpp`), the demo scripts
(`apps/demo/jam/setup.acmd`, `tests/golden/seq_progression.acmd`,
`tests/golden/smart_richness.acmd`), the converter's data model
(`apps/tools/arrstyle-converter/src/model.hpp`), and `docs/DESIGN.md` §9400
(style data format / CASM importer — confirms `9420`'s style compiler and
`9430`'s CASM/SFF import are HOST-ONLY tooling, never wired into the built-in
list or a `StyleDef` field). Grammar was verified by tracing the actual parse
functions, not by recalling the CLI's `help` text — every claim below about
what `seq add` accepts or rejects is read from `engine.cpp`/`theory.hpp`, not
assumed.

## 1. The grammar, exactly as the code parses it (load-bearing for §2)

- `key <root>[#/b] <mode>` — root via `parse_pc` (letter A–G, optional
  `#`/`b`); mode via `parse_mode`: `major, minor, dorian, phrygian, lydian,
  mixolydian, locrian` (`shell_parse.cpp:158-179`).
- `seq new <name>` / `seq add <root>[#/b] [quality] [Nbar(s)|Nbeat(s)]` /
  `seq loop on` / `seq play` (`shell_music_commands.cpp:691-827`).
- Quality tokens, and ONLY these: `maj, min, dim, aug, maj7, min7|m7, 7|dom7,
  m7b5|halfdim, dim7, sus2, sus4` (`shell_parse.cpp:181-209`). **No 9th/11th/
  13th token exists.** A style whose idiom wants a color tone past the 7th
  (disco's 9ths, house's extended jazz voicings) cannot get it through `seq
  add` today — a real, measured grammar gap, not a taste choice.
- **Hard constraint, easy to miss:** `Engine::seq_add` (`engine.cpp:520-537`)
  computes `theory::degree_of(seq->key, root_pc)` and **rejects the whole
  step (`WarnCode::kNotInKey`) if the root is not diatonic to the active
  key/mode** — regardless of whether a quality was given explicitly. A
  secondary dominant is fine (its ROOT is diatonic even though its quality
  isn't, e.g. `A 7` in C major = ii's root borrowing a dominant color); a
  chord whose ROOT itself is foreign to the key (blues' bIII, a borrowed
  bVII in a major key) is not expressible via `seq add` at all today.
- **Smart quality, when none is given** (`theory::smart_quality`,
  `chorddet/theory.hpp:162-194`, exercised by `tests/golden/
  seq_progression.acmd` and `smart_richness.acmd`): bare-root steps ALWAYS
  resolve to a **seventh-chord** quality derived from the scale (maj7 on I/IV,
  min7 on ii/iii/vi, dom7 on V, half-dim7 on vii, with harmonic-minor's V
  forced dominant) — **never a plain triad**. Any style that wants clean
  triads (pop/rock/country/reggae/latin/basic below) must say `maj`/`min`
  explicitly on every step; omitting quality is itself a genre choice (jazzy/
  soul sevenths), not a neutral default.
- **A genuine positive lever, not just a limitation:** picking a non-major
  `key` mode makes otherwise-blocked chromatic-sounding moves fully diatonic.
  `key G mixolydian`'s own scale is `{0,2,4,5,7,9,10}` (verified against
  `theory::scale_of`'s rotation table, `theory.hpp:80-98`) — its own
  diatonic vii is F-natural, i.e. rock's classic borrowed-bVII ("Sweet Home
  Alabama"-style I–bVII–IV) becomes a first-class, fully diatonic `seq add`
  sequence, not a workaround.
- Every style section is uniformly `.bars=2` for `Intro1`/`VarA` and `.bars=1`
  for everything else, measured across all 16 headers (grep, cited in §2's
  header) — so a loop length that is an even multiple of 2 bars always lands
  a chord change on a VarA repeat boundary; I sized every progression below
  to that constraint on purpose.

## 2. The 16 default progressions

Key/mode is `C major` unless stated. `smart` means quality omitted on
purpose (idiomatic sevenths); explicit tokens are written out. All are
directly runnable `.acmd` lines in the shell's own grammar — not pseudocode.

**basic** — *I–IV–V–I*, plain triads. The textbook cadence; "basic" is the
neutral/tutorial style, so it gets the most unmarked possible harmony, not a
genre commitment.
```
key C major
seq new default
seq add C maj 1bar
seq add F maj 1bar
seq add G maj 1bar
seq add C maj 1bar
seq loop on
seq play
```
4-bar loop (2× VarA).

**pop** — *I–V–vi–IV*, plain triads. The "Axis"/four-chord-song progression —
the single most-cited contemporary pop convention (Wikipedia
"I–V–vi–IV progression"). Plain triads: mainstream pop rhythm parts comp
triads, not jazz sevenths.
```
key C major
seq add C maj 1bar
seq add G maj 1bar
seq add A min 1bar
seq add F maj 1bar
```
4-bar loop.

**rock** — *I–bVII–IV–I* in **G mixolydian**, plain triads. This is the
textbook rock-modal move (Skynyrd/Bowie-style flat-VII vamp); the ONLY way
to express it under this grammar's diatonic-root check is to change mode,
not key — see §1. Plain triads for the raw power-chord read; omitting
quality instead gives a genuinely idiomatic alternate color (mixolydian's
own smart-quality resolves the tonic to a dominant 7th, G7-F-C-G7, also a
real blues-rock texture worth trying).
```
key G mixolydian
seq add G maj 1bar
seq add F maj 1bar
seq add C maj 1bar
seq add G maj 1bar
```
4-bar loop.

**funk** — *static I7 vamp*, one chord for the whole loop. Funk's own
harmonic idiom is a static dominant/extended vamp, not chord motion (James
Brown "the one"); the groove carries the interest, not the harmony — this is
the one style where "no progression" IS the idiomatic progression.
```
key C major
seq add C 7 2bars
```
2-bar loop (= 1× VarA exactly): re-fires the same I7 stab every 2 bars.

**disco** — *I–vi–ii–V*, smart sevenths (Imaj7–Am7–Dm7–G7). Disco's lush,
jazz-tinged extended-chord language (Chic-style) over a I-vi-ii-V turnaround;
9ths/13ths are genuinely idiomatic here too but unavailable (§1's grammar
gap) — sevenths are the closest expressible approximation.
```
key C major
seq add C 1bar
seq add A 1bar
seq add D 1bar
seq add G 1bar
```
4-bar loop.

**house** — *i–VII* in **A minor**, 2 bars per chord (Am7–G[dom7 by smart
quality], both bare-root). Deep/minimal house's own convention: a static
2-chord loop, i to the "backdoor" bVII, is a documented deep-house harmonic
device (not I-IV-V motion at all).
```
key A minor
seq add A 2bars
seq add G 2bars
```
4-bar loop, each chord holding exactly one VarA cycle.

**motown** — *I–vi–IV–V*, smart sevenths (Cmaj7-Am7-Fmaj7-G7). The '50s /
doo-wop / "Stand by Me" changes — documented as THE defining Motown-era
turnaround.
```
key C major
seq add C 1bar
seq add A 1bar
seq add F 1bar
seq add G 1bar
```
4-bar loop.

**country** — *I–IV–I–V*, plain triads. The Nashville three-chord turnaround
(revisits I before V, the classic honky-tonk shape); plain triads — country
rhythm guitar does not comp jazz sevenths.
```
key C major
seq add C maj 1bar
seq add F maj 1bar
seq add C maj 1bar
seq add G maj 1bar
```
4-bar loop.

**reggae** — *I–IV*, plain triads. "Two chords, infinite groove" — the
documented foundation of roots reggae (Marley catalog runs largely on I-IV);
plain triads, since reggae's harmonic interest is deliberately minimal (the
skank rhythm and the bass line carry the identity, not chord color).
```
key C major
seq add C maj 1bar
seq add F maj 1bar
```
2-bar loop (= 1× VarA exactly): I on bar 1, IV on bar 2, every repeat.

**latin** — *I–IV–V–IV*, plain triads. The documented son-montuno vamp
convention (Cuban dance-band harmony, "very common in salsa"); a driving
2-bar cell over the clave, not jazz-voiced.
```
key C major
seq add C maj 1bar
seq add F maj 1bar
seq add G maj 1bar
seq add F maj 1bar
```
4-bar loop.

**bossa** — *ii–V–I–vi*, smart sevenths (Dm7-G7-Cmaj7-Am7). Bossa nova's own
harmonic backbone is the ii-V-I family (Jobim's whole catalog runs on it);
extending to vi completes a symmetrical 4-bar turnaround instead of stopping
cold on I.
```
key C major
seq add D 1bar
seq add G 1bar
seq add C 1bar
seq add A 1bar
```
4-bar loop.

**samba** — *I–VI7–ii–V7* (Cmaj7[smart]-A7[explicit dom7]-Dm7[smart]-
G7[smart]). Documented as a real samba-de-enredo/pagode convention: VI7 is a
SECONDARY DOMINANT (major/dominant sonority on scale-degree vi driving into
ii) — its root (A) is diatonic to C major so the grammar accepts it, but its
quality must be forced explicitly since the smart default would otherwise
give the diatonic Am7.
```
key C major
seq add C 1bar
seq add A 7 1bar
seq add D 1bar
seq add G 1bar
```
4-bar loop. Deliberately distinct from bossa (forward-driving secondary
dominant vs bossa's circular ii-V-I) even though both are "Brazilian".

**swing** — *I–vi–ii–V*, smart sevenths. The turnaround/"rhythm changes"
family (Gershwin's "I Got Rhythm" changes, the most-cited big-band/swing
turnaround shape) — same 4 chords as motown, but the ii in slot 3 (not IV)
is the real, documented distinguishing detail between the doo-wop and the
jazz-turnaround families.
```
key C major
seq add C 1bar
seq add A 1bar
seq add D 1bar
seq add G 1bar
```
4-bar loop.

**shuffle** — *V7–IV7–I7–I7*, all explicit dominant 7ths. This is the
documented closing 4-bar cell of a 12-bar blues turnaround, looped on its
own as a compact shuffle vamp — related to `blues` below by chord family
(all-dominant, blues-shuffle feel) but deliberately NOT the full 12-bar form,
so the two styles are audibly distinct, not duplicates.
```
key C major
seq add G 7 1bar
seq add F 7 1bar
seq add C 7 1bar
seq add C 7 1bar
```
4-bar loop.

**blues** — full 12-bar blues, ALL dominant 7ths (I7 even on the tonic —
the single most identifying blues harmonic marker, distinguishing it from
country/rock's plain I-IV-V triads on the same three roots).
```
key C major
seq add C 7 1bar
seq add C 7 1bar
seq add C 7 1bar
seq add C 7 1bar
seq add F 7 1bar
seq add F 7 1bar
seq add C 7 1bar
seq add C 7 1bar
seq add G 7 1bar
seq add F 7 1bar
seq add C 7 1bar
seq add G 7 1bar
```
12-bar loop (= 6× VarA exactly).

**ballad** — *I–vi–ii–V*, smart sevenths, 2 bars per chord. Same family as
`swing`/`disco`'s ii-based turnaround, documented specifically as the
"jazzier, more sophisticated" ballad variant of the '50s progression
(swapping IV for ii); the slower 2-bar-per-chord harmonic rhythm matches the
style's own slow tempo (72 BPM, `ballad.hpp`'s own `.tempo=7200`) and lets it
breathe rather than turning over every bar like the up-tempo styles.
```
key C major
seq add C 2bars
seq add A 2bars
seq add D 2bars
seq add G 2bars
```
8-bar loop (= 4× VarA).

## 3. The mechanism (revised per owner steer — no `StyleDef`/core touch)

### 3.1 Where the harmony already lives, and why it must stay there

`Style`/`StyleSection` (`components/core/arrangrr/include/arrangrr/arranger/
style_model.hpp:150-197`) carry `gm_program`, `voicing`, `groove`, `tempo`,
`beats_per_bar` — **no key, no chord, no progression field of any kind**.
That is not an oversight to fix; it is architecturally correct given `docs/
DESIGN.md` `9400`/`9420`/`9430`: the style corpus is not fixed. `apps/tools/
arrstyle-converter` is a real, working SFF/CASM importer over a validated
1010-style corpus (`docs/backlog/yamaha-style-corpus-and-rules.md`), and its
own data model (`apps/tools/arrstyle-converter/src/model.hpp:140-167`)
already separates the two concerns exactly the way Yamaha arrangers do:
`StyleModel` (the accompaniment phrases, `TranspositionPolicy::kChordTone`
resolved against the NTT) is a completely different object from `SongModel`
(`chords: std::vector<ChordEvent>`, `harmony_source =
PhraseSourceHarmony::kChordSequence`) — and that comment says, verbatim,
"target: runtime ChordSequence + Song". **The harmony was always meant to be
a separate runtime object from the style, and that object already exists**:
`arrangrr::ChordSequence`/`ChordSequencer`
(`components/core/arrangrr/include/arrangrr/chord/chord_sequence.hpp`,
`chord_sequencer.hpp`), reached today by exactly the `seq new/add/loop/play`
verbs §2 uses. Baking a progression into `StyleDef` would (a) not scale to
1010 imported styles, each of which needs ITS OWN progression, not a
recompiled constexpr table entry, and (b) touch the frozen-adjacent core ABI
table for zero architectural gain, since the right object is already there.

**Recommendation: the 16 progressions in §2 are HOST DATA — a small
`constexpr` table in the GUI layer, keyed by the built-in style index — that
feeds the EXISTING core `ChordSequence` through the EXISTING `seq`/`key`
Command surface.** Zero new `StyleDef` field, zero new ABI `Param`, zero
golden touched, zero owner sign-off needed on the core. An imported style
that carries its own `SongModel.chords` (once `9430`'s import path is wired
that far — it is not yet, see `DESIGN.md` `9400`'s own "partial" status) uses
ITS chords the same way, through the same `seq` verbs; a style with none
falls back to a single generic neutral loop. I recommend the fallback be
`basic`'s own I–IV–V–I plain-triad loop from §2 — already designed, most
genre-neutral shape available, no second shape to invent or maintain.

### 3.2 How the GUI actually wires it — read from the code, not assumed

Confirmed (matches the A/B's own §1 finding): `command_line_to_command()`
(`apps/gui-sonotron/src/in_process_brain_session.cpp:306-676`) has no `key`/
`seq` branch; any such text line falls through to `kUnknownCommand`.
**But that translator is not the only, or even the primary, precedent for
this kind of GUI-side automatic wiring already in the tree.** The exact
same problem — "the engine needs several `Command`s issued automatically
right after a style loads, with no user action and no typed line" — is
already solved, shipped, and load-bearing for the routing fix
(`kDefaultStyleRoutes`, same file, lines 98-110): a small `constexpr` table
of `{role, channel}` is turned into raw ABI `Command` PODs by
`make_default_style_route_command()` (lines 678-695) and pushed directly
onto `command_ring` at the exact injection site, lines 990-1000, immediately
after a successful `kStyleLoad`/`kStyleSwitch`.

**Cleanest path: mirror that pattern exactly, do not extend
`command_line_to_command()`.** Add a parallel `kDefaultStyleProgression`
table (key root/mode + an ordered list of `{root_pc, quality_ovr, duration_
bars}` steps per style index) and a `make_default_progression_commands()`
builder that emits the raw `Command` sequence (`kKeySet`, then
`kSeqNew`/`kSeqUse`+`kSeqClear`, then N×`kSeqAdd`, then `kSeqLoop`,
`kSeqPlay`) directly — same site, same idiom, same "no text round-trip"
shape as the routes. Reasons this beats teaching `key`/`seq` to the text
translator: (a) there is currently no free-text console or line-entry widget
anywhere in `gui-sonotron` (checked: `browser_panel.cpp`, `grid_panel.cpp`,
`transport_panel.cpp`, `seqedit_panel.cpp` — the last is a read-only piano-
roll PREVIEW of a clip, `render_seqedit_panel`, it sends no commands at all)
— nothing in the GUI today would ever call `command_line_to_command("key C
major")`, so adding those branches buys no reachable caller; (b) the
existing route table already proves the direct-Command-POD idiom compiles,
tests, and ships cleanly. Note for later, not now: the day a live chord/key
panel ships (A/B's own recommendation #2), THAT widget's user-typed or
clicked input genuinely needs a text (or structured) path into the engine —
at that point extending the translator becomes the right call, for a
different reason (a real caller exists). Not required for this task.

### 3.3 Three concrete engineering traps in "just re-fire this on every
    load/switch", flagged, not fixed

1. **Directly reverses an existing, deliberate design decision.**
   `Engine::cmd_style`'s `kStyleLoad` handler
   (`components/core/arrangrr/src/engine.cpp:614-634`) carries this comment,
   verbatim: *"Owner decision: loading a style changes the BAND, keeps the
   HARMONY. `establish_default` seeds the home key only when nothing
   explicit is in force, so a chord the user steered persists across a style
   load."* Auto-injecting a NEW progression on every style load, mirroring
   how `kDefaultStyleRoutes` re-applies idempotently every time, is the
   opposite of "keeps the HARMONY". **Today this is harmless**: the GUI has
   no live-chord path at all (A/B §1), so there is nothing genuine to
   "keep" — but the day recommendation #2 (a live chord/key panel) ships,
   blind reapplication on every style switch would silently clobber a
   user's live-played chord. `ChordEngine::explicit_set()`
   (`chord_engine.hpp:225`) already exists as exactly the readback
   `establish_default()` itself uses for this same gate — but it lives on
   the engine thread, unreachable synchronously from the GUI thread across
   the one-way `command_ring`/`out_event_ring` split; using it from the GUI
   side would need a new engine-thread-side Param or OutEvent, i.e. a real
   (small) core/ABI change, not host-only. **This is a genuine three-way
   product fork for the owner, not something I will resolve — see §4.**
2. **Sequence-pool exhaustion.** `kSeqNew` calls `ChordSequencer::
   add_sequence()`, which appends a NEW slot to a bounded pool,
   `kMaxChordSequences = 16` (`components/core/arrangrr/include/arrangrr/
   config.hpp:17`, "16 x 128 x 12 B = 24 KB"). Re-issuing `kSeqNew` on every
   style load/switch (rather than once) exhausts the pool after 16 switches
   in one session; every call after that silently warns
   (`WarnCode::kSeqTableFull`) and does nothing. Fix (host-side, cheap): call
   `kSeqNew` exactly once, at the FIRST style load of a session; every
   subsequent load/switch should `kSeqClear` + rebuild the SAME sequence
   (index 0 by construction — the GUI builds raw `Command` PODs directly,
   so it can just target index 0, it never needs `Shell`'s name-based
   `seq use <name>` resolution at all).
3. **Quantize-boundary mismatch on a LIVE `style switch`.** `style switch`
   quantizes the actual style morph to the next bar
   (`Boundary::kNextBar`, `in_process_brain_session.cpp:358`), but
   `Engine::cmd_seq`'s handling of `kSeqNew/kSeqAdd/kSeqLoop/kSeqPlay`
   (`engine.cpp:446-501`) never reads `cmd.boundary` at all — `kSeqPlay`
   sets `m_base = transport_tick()` at whatever tick it is POPPED from the
   ring, immediately. On a `style load` (transport usually stopped/fresh)
   this is moot. On a LIVE `style switch` (browser_panel's playing-time
   morph), the new harmony loop's phase would start "now", not aligned to
   the quantized bar boundary the style morph itself waits for — an audible
   chord/section phase mismatch of up to one bar. Flagged for Nazzareno's
   judgment (a small fix would teach `kSeqPlay` to honor
   `Boundary::kNextBar` when set); not something I will solve here.

### 3.4 Two non-issues, checked and ruled out (asked to flag interactions,
    not invent them)

- **The double section-trigger** (scene-header click sending both `style
  section <name>` and `launch scene ... quantize`,
  `grid_panel.cpp:706-722`) never touches harmony under this design: the
  progression is only re-applied on `kStyleLoad`/`kStyleSwitch`, never on
  `kStyleSection`. A section change inside a style (VarA→VarB) leaves the
  chord sequencer running untouched underneath — which is also the
  musically correct behavior (a real arranger's chord track is independent
  of which style-section variation is currently playing).
- **D47 chord-follow arbitration** (`chord follow auto|detect|sequencer|
  manual|live`) is currently a non-issue because the sequencer is the ONLY
  chord source the GUI will ever create under this proposal. It becomes
  directly relevant the day a second source exists (a live chord/detect
  panel) — same trigger point as trap #1 above, not a new one.

## 4. What needs deciding / what I flagged and refuse to decide alone

1. **The re-apply-on-switch fork (§3.3.1)**, three real options: (a) always
   re-apply on every load/switch (simplest, matches the shipped route-table
   idiom, currently zero-risk since no live-chord path exists yet, but WILL
   need revisiting once one does); (b) apply only once per session (matches
   "keeps the HARMONY" literally, but defeats the actual ask — switching
   genre keeps the wrong-genre progression); (c) gate on
   `ChordEngine::explicit_set()` (architecturally correct, mirrors
   `establish_default()`'s own discipline, but needs a new engine-thread-
   reachable readback — a real, if small, core/ABI change requiring sign-
   off). I recommend (a) for now, given the owner's own framing ("poi, in
   un secondo momento, ci montiamo sopra detector/pad") — but (a) has an
   explicit expiry: revisit at the moment a live chord panel ships.
2. **The fallback shape for imported (Yamaha/SFF) styles with no usable
   `SongModel.chords`** — I recommend `basic`'s I–IV–V–I plain-triad loop
   (§2); the owner may prefer a different neutral shape, that is a genuine
   taste call, not a measurement.
3. **The 9-bar/13th grammar gap (§1)** — disco and house's real idiom wants
   color past the 7th; `seq add` cannot express it today. Not a blocker for
   this proposal (sevenths are a legitimate, documented approximation for
   both), but flagged as a real grammar ceiling if a future style wants a
   9th/13th as its DEFINING sonority (e.g. a jazz-fusion or neo-soul style).
4. Nothing here needs a new host or core dependency.

## Appendix: files read for this proposal

`components/core/arrangrr/include/arrangrr/arranger/styles/*.hpp` (all 16),
`components/core/arrangrr/include/arrangrr/arranger/style_model.hpp`,
`components/core/arrangrr/include/arrangrr/chord/{chord_sequence,
chord_sequencer,chord_engine}.hpp`,
`components/core/chorddet/include/chorddet/theory.hpp`,
`components/core/arrangrr/include/arrangrr/abi.hpp`,
`components/core/arrangrr/include/arrangrr/config.hpp`,
`components/core/arrangrr/src/engine.cpp`,
`components/platform/hostrt/{shell.cpp,shell_parse.cpp,
shell_music_commands.cpp,shell_internal.hpp}`,
`apps/gui-sonotron/src/in_process_brain_session.cpp`,
`apps/gui-sonotron/src/seqedit_panel.cpp`,
`apps/tools/arrstyle-converter/src/model.hpp`,
`apps/demo/jam/setup.acmd`, `apps/demo/clean/setup.acmd`,
`tests/golden/seq_progression.acmd`, `tests/golden/smart_richness.acmd`,
`docs/DESIGN.md` (§9400 area), `docs/reflections/cli-vs-gui-ab-2026-07.md`,
`docs/backlog/yamaha-style-corpus-and-rules.md` (referenced, not re-read in
full — its conclusions are cited, not re-verified, in this pass).
