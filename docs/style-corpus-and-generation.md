# Style corpus, data format, and generation

This document is the single reference for the arranger STYLE subsystem: the style
data model as built, why the 16 built-ins feel same-y and what would make them
genuinely different, the per-style FEEL values, the Yamaha SFF/CASM corpus and its
genre rules, the generation/stylizer options, the pure-data (non-compiled) style
format, and the parked live-modulation "Pivot" transpose.

## Status and scope

The compiled-C++ style path ships today. Per-style feel and tempo are realized
(roadmap `9100`–`9120`, DESIGN.md). The remaining generative / stylizer / corpus
work (`9200` / `9300` / `9400`) sits behind the GUI freeze line (`11700`) and is
not scheduled for the current host-GUI strand. The SFF/CASM importer is
inspect-only in the tree (`apps/tools/arrstyle-converter/src/sff_import.cpp`); the
pure-data style format is a roadmapped DESIGN D44 direction, not a current phase.
The "Pivot" live-transpose feature is PARKED — the owner chose Path 1 (plain
literal chord-follow) for the JAM that ships. Everything below labelled SHIPPABLE
is no-heap, dual-target, dependency-free; everything HOST-ONLY or
INSTRUCTIVE-BUT-INFEASIBLE is labelled as such, and every new dependency is flagged
for owner approval, never smuggled.

---

## 1. The style data model as built

Arranger STYLES are C++ source: `inline constexpr StyleEvent[]` tables wired into
`StylePattern` / `StyleSection` / `Style` (16 built-ins under
`components/core/arrangrr/include/arrangrr/arranger/styles/`). They are POD + `Span`,
land in flash memory-mapped on the STM32 target, and cost zero RAM and zero parsing
(D32/D33).

**The one fact that governs serialisation.** Only `StyleEvent` is serialisable
as-is: it is 10 bytes of value fields, no pointers
(`static_assert(sizeof==10)`, `style_model.hpp:103`). Every container above it —
`StylePattern`, `StyleSection`, `Style` — carries a `Span<const T>` (a raw pointer
+ `size_t`) and `Style::name` is a `const char*`. Spans are NOT serialisable and
NOT portable: their width differs host (16 B) vs arm32 (8 B), and a stored pointer
is meaningless after a load or across a flash image. This forces the format split
used in §7 and matches DESIGN.md line 260 ("references are indices/IDs into tables,
not pointers — serialization is memcpy-friendly and relocatable").

**The role and policy vocabulary.**

- `RolePolicy` has exactly two values (`kFixed`, `kChordTone`, `style_model.hpp:44`).
  Every pitched part in every style is `kChordTone` ("play this chord's tones");
  every drum/perc part is `kFixed`. There is no third policy — riff, walking,
  ostinato, melody are not policies.
- `NoteSource` adds `kScaleDegree` and `kInterval` (D39) as event-level pitch
  sources; `ChordGesture` adds `kStrumUp/Down`, `kRollUp/Down` (D40); `VoicingPolicy`
  adds `kLead` smooth voice-leading (D41); gestures are D42.
- The role roster is 8 — {Drums, Perc, Bass, Chord1, Chord2, Pad, Arp, Lead} — plus
  `kPhrase` (index 7, `timeline.hpp:33`), an available second melodic slot used by
  **0** styles. The section set is Intro1/2, VarA–D, FillA–D, Ending1/2 (13
  `SectionType`s, ABI-stable from day one); 6 styles add a real `kBreak`.
- `GrooveParams` (`groove.hpp:21`) is ONE GLOBAL runtime state
  (swing / humanize / accent / quantize / swing_grid / seed), not an attribute a
  style can declare. The `Style` struct carries `name` + `sections` (plus the
  shipped `groove` + `tempo`, §4). The grid is 16th-note only (`step` is a 16th
  slot).
- Caps for any generative fan-out: `kMaxVoiceNotes=16` (`arranger.hpp:369`),
  `kMaxGestureFan=8` (`gesture.hpp:24`), `StyleEvent` pinned at 10 bytes.

**The Harmonic Mapper.** The NTT mechanism (D24) is `Arranger::resolve(pattern, ev,
key, chord)`: a neutral, chord-relative pattern resolved at playback against the
live `ChordState`, with `kRoleAnchor` per-role registers (D36). `RolePolicy::kChordTone`
maps a chord-tone index → concrete note; every resolved note is a chord tone by
construction, so wrong notes are impossible. This mechanism is the frame all
generation reuses (§5, §6); its poverty of vocabulary is the subject of §2 and §3.

---

## 2. Diagnosis — why the 16 built-ins feel like "one song rearranged"

All counts below were MEASURED on the 16 `styles/*.hpp`, `style_model.hpp`,
`groove.hpp`, `timeline.hpp` (event = one `{.step=...}` record).

| style | events | melodic(scaleDeg+interval) | gestures | kLead patterns | drums signature (VarA kick / snare) |
|---|---:|---:|---:|---:|---|
| basic | 255 | 0 + 0 | 0 | 0 | — |
| ballad | 159 | 0 + 0 | 0 | 13 | — |
| blues | 196 | 0 + 5 | 0 | 19 | kick 0,8 / snare 4,12 |
| bossa | 231 | 0 + 0 | 1 | 6 | kick 0,8 / (side-stick clave) |
| country | 202 | 4 + 1 | 8 | 0 | kick 0,8 / snare 4,12 |
| disco | 294 | 4 + 0 | 1 | 8 | kick 0,4,8,12 (four-on-floor) |
| funk | 283 | 0 + 0 | 0 | 0 | kick 0,3,6,10 / snare 4,7,11,12 |
| house | 273 | 4 + 0 | 0 | 15 | kick 0,4,8,12 |
| latin | 312 | 4 + 1 | 0 | 7 | (perc-driven) |
| motown | 233 | 4 + 0 | 0 | 19 | kick 0,8 / snare 4,12 |
| pop | 260 | 4 + 0 | 0 | 11 | backbeat |
| reggae | 202 | 4 + 0 | 23 | 11 | kick 8 + snare 8 (one-drop) |
| rock | 227 | 3 + 1 | 8 | 7 | hard backbeat |
| samba | 244 | 0 + 0 | 18 | 7 | kick 0,4,8,12 |
| shuffle | 200 | 1 + 4 | 0 | 18 | kick 0,8 / snare 4,12 |
| swing | 195 | 3 + 2 | 0 | 18 | kick 0,8 |

Corpus totals: **3766 note events; 35 are `kScaleDegree` (0.9%), 14 are `kInterval`
(0.4%); the remaining 3717 (98.7%) are chord-tone comps or fixed drum hits.** No
bassline uses `kScaleDegree` or `kInterval` — every `*Bass*` array is chord-tone
indices only (root/fifth dominant, a little third/seventh in motown/swing); there
is no walking or chromatic-approach bass anywhere, because the notes that make one
(`kInterval` ±1/±2 into the next root) are never used on `kBass`. Gestures total 59
(`kStrumUp` 36, `kStrumDown` 21, `kRollUp` 2), concentrated in reggae (23), samba
(18), country (8), rock (8), bossa/disco 1 each; **the other 10 styles use zero
gestures.** All 16 share the same role roster and section set; `kPhrase` is unused.

**Per-axis diagnosis (MODEL GAP vs UNDERUSE).**

- **Rhythmic signature (drums).** EXPRESSIBLE and genuinely USED. The `kFixed` drum
  layer carries real idiom: reggae one-drop (kick+snare on beat 3 = step 8),
  disco/house/samba four-on-the-floor (0,4,8,12), funk syncopated 16th kick
  (0,3,6,10) with ghost snares, backbeat (snare 4,12). The one axis where the corpus
  is actually differentiated. Not the problem.
- **Bass function.** UNDERUSE bordering on model-adjacent. Real genres separate by
  what the bass DOES: root-fifth two-beat (country), octave-jump 8ths (disco),
  syncopated 16ths (funk), anticipated tumbao (latin), walking quarter with
  chromatic approach (swing/jazz), sparse melodic root (reggae). Octave-jump and
  chromatic-approach basses are describable TODAY (`NoteSource::kInterval`, D39) but
  used on bass by ZERO styles. The residual true gap: a walking line also wants a
  per-beat grid and a declarable feel.
- **Comping / voicing.** PARTIAL. `VoicingPolicy::kLead` is used (6–19 patterns) and
  gives smooth voice-leading; register is set via octave and `gm_program`. But every
  comp is the same operation — stack the chord's tones at step X. There is no open
  voicing, no inversion policy, no drop-2/rootless-jazz, no power-chord-only
  (root+fifth, omit third) policy — DESIGN lists "no open voicing, no inversion
  policy" as an Arranger gap (line 893). MODEL GAP for voicing SHAPE; UNDERUSE for
  the rhythm/register that IS available.
- **Melodic content.** MODEL-thin + UNDERUSED. Only 0.9% of events are diatonic and
  0.4% intervallic; 4 styles (basic, ballad, funk, samba) have no melodic content at
  all. Where melody exists it is a FIXED authored line — the same four notes every
  bar. A `kScaleDegree` event resolves to exactly one note, and gestures only fan
  CHORD tones, never a line. Nothing in the model produces a VARYING melodic line.
- **Groove / microtiming / feel.** MODEL GAP. Swing, shuffle, bossa and swing-jazz
  are FEEL genres — their identity is triplet subdivision and microtiming — but the
  style cannot declare it: no per-style swing (until 9100, §4), a 16th grid that
  can't place triplets. Yamaha rule U11 ("groove feel is per-genre") had no home in
  the `Style` struct. The single most consequential gap for "same song".
- **Instrumentation.** EXPRESSIBLE and used (distinct `gm_program` per role) — but
  the shallowest difference: a different GM patch playing the identical chord-tone
  comp is exactly the "same song, different sound" complaint.
- **Form.** UNIFORM: same 12-section skeleton, mostly 1-bar loops, a modest
  VarA→VarD density ladder. Form does not differentiate genres here.

**Root cause.** The pitched half of every style is the SAME operation — "resolve
this chord's tones at these steps" (98.7% of events). The axes that actually
separate genres — bass FUNCTION, melodic LINE, comp VOICING shape, and FEEL — are
collapsed into "chord-tone at step X" plus a global groove knob the style does not
own. That is an aesthetic amputation encoded in `RolePolicy` having two values and
`Style` (originally) having no feel.

---

## 3. The pattern-model and engine gaps (keep / rework / throw)

A prior gap analysis judged arrangrr-as-built against two market/architecture
reflections describing a modern chord-reactive pattern arranger. The durable
verdict: sound foundations, few wrong edges. A chord-tone-only pattern is a
**musical** decision, not merely a data-model one; and a JSON daemon in the core is
a musical decision wearing a transport costume.

### Keep (arrangrr's real capital)

1. **No-heap / dual-target / integer-tick total-order determinism (D29/D32/D33).**
   `advance_ticks`-pure, injected time, total order `(@tick, class_priority,
   seq_no)` with `NoteOff < NoteOn`, 8-byte `Event`, static pools under 512 KB.
   Determinism is *reproducible groove*: swing/humanize in `groove.hpp` is a
   deterministic position hash, so "same seed ⇒ same feel" is a constructive
   property, not an aspiration.
2. **The NTT mechanism as the Harmonic Mapper (D24, `Arranger::resolve`).** The
   *mechanism* is exactly right and survives; only its *vocabulary* was too poor
   (Rework 1). Do not throw the frame to widen it.
3. **The headless doctrine: binary core ABI (D26) + host-only L0 JSONL / L1 paths /
   L2 REPL.** The core receives `Command{op, param, idx, a, b, c}` POD; JSONL
   (de)serialization lives in `platform/host`. Better than JSON-RPC-in-the-core:
   no parsing/heap crossing the STM32 boundary.
4. **The section manager** (`style_model.hpp` + `Arranger::on_tick`): 13
   `SectionType`s, bar-quantized switching, one-shot fills/intros returning to the
   active variation (`m_return_to`), endings that stop transport, style-switch
   landing together with its section on the same downbeat.
5. **Chord intelligence** (D12/D19/D34): diatonic `smart_quality` (stack thirds on
   the mode, V-dominant-in-minor exception, compile-time tested), shell completion,
   chord-memory/hold-last, `set_context` steering the NTT without sounding a second
   voicing.
6. **Parametric groove + the planned deterministic Generative Director (D37)**,
   explicitly non-AI — an interpolated `DirectorState`+`DirectorTarget` trajectory
   that pilots Arranger/Arp/Groove without bypassing them.
7. **Role-based free routing** (`TrackRole` → `Route{port, channel}`, per-role
   mute/solo).

### Rework (the few right edges)

1. **The pattern event model** — realized as D39/D40/D41/D42. A bass that can only
   hit root/third/fifth/seventh cannot *walk*, cannot use approach notes; "wrong
   notes impossible" also means "right passing notes impossible". The fix was the
   taxonomy now in the model: `kScaleDegree` (diatonic melodies/basslines resolved
   against key+scale), `kInterval` (transposable riffs preserving the gesture),
   `ChordGesture` (spacing trigger for the voicing engine). The infrastructure was
   already present (`theory::scale_of`/`degree_of` constexpr; `ChordState` carries
   enough to resolve scale-degrees given the `Key`). The types shipped; the residual
   work is USAGE (§2 UNDERUSE) plus voicing SHAPE and FEEL.
2. **Voicing: root-position close-stack → voice-leading + Harmonic Spillover.**
   `ChordEngine::sound()` releases the previous voicing and re-stacks from root
   position: every chord change is a jump. A voicing resolver should keep common
   tones and move the rest by minimal step, with `ChordGesture` choosing the
   spacing. Bounded (max 4–5 voices), fixed-point, no heap. Still open.
3. **The MIDI Looper: planned but unbuilt (§13/M7) → build it reversible
   performance↔pattern.** Capture both the absolute stream and, against the current
   `ChordState`, the relative (degree/role) form, so conversion is invertible and
   the captured loop is re-harmonizable and transposable (D10 capture, D28
   functional storage). Bounded by the D33 budget (8×3072 ev); build *after* the
   relative form exists. Still open.
4. **Per-port scheduler → add per-port latency compensation** (the "Adaptive Bus
   Scheduler"): a per-port offset (including negative look-ahead) in `OutScheduler`,
   bounded, integer-tick — otherwise a chord on DIN (~1 ms/message) and USB arrives
   as a flam. Remains valid/open.
5. **Section launch quantization**: bar/immediate → next-step/next-beat/next-bar/
   end-of-pattern via a quantization enum on the pending switch. Remains valid/open.
6. **Arpeggiator → chord-context-aware and section-aware**: an alternate note source
   from the live `ChordState`, plus a hook into section parameters (rate/octaves via
   the Director). Extension, not rewrite.
7. **MPE input**: absorb MPE on ingress (zone + per-note channel) as MIDI 1.0,
   WITHOUT touching the 8-byte `Event` and WITHOUT going to UMP. Remains valid/open.

### Throw away (feature-creep that would betray the embedded-first thesis)

Desktop-first / JUCE-GUI / plugin validation (inverts D2/D7); AI/RTNeural harmonic
suggestion (non-deterministic, allocating — D16/D32/D37 give the deterministic
Director instead); two-brain SBC-Linux (CM4) + coprocessor MCU (D2/D33); embedded
Lua/DSL in the realtime engine (D32, and redundant with the L0/L1 text
protocol); "64 tracks" (D33 — keep bounded 8–16); BLE/Wi-Fi/Ethernet/Network-MIDI
as core concerns (relegate to a far P2 host adapter); full MIDI 2.0/UMP now
(D33(d) — keep the abstract boundary, refuse the premature implementation);
CV/gate and MIDI-CI/Property-Exchange as core work (relocate to a future HAL
adapter / P2 host tool). Do not rewrite the core: **extend the data-model and the
resolver**, which is exactly what the architecture was designed to absorb.

**On a desktop GUI.** The GUI is a **client on the existing protocol (L0 JSONL /
L1 paths)**, never linking the core, never on the STM32 target, never in the
realtime/note path, never holding authoritative state, never a required component —
the core stays the sole source of truth and the GUI is a mirror reconstructed from
the `OutEvent`/JSONL stream (die-and-reopen safe: re-dump via D17d, re-subscribe).
The musical prize a GUI unlocks over the TUI is the **relative-pattern editor** — a
piano-roll overlaying the degree/role/interval/gesture types so an author *sees*
"root, walk up to the 5, approach the next root" instead of decoding indices —
alongside a live chord strip, section timeline, routing matrix, and a voicing view
of the spillover motion. (The shipped toolkit is Dear ImGui + GLFW3, DESIGN node
`11600`; see `docs/gui-and-ux.md`.)

---

## 4. Per-style feel values (roadmap 9100)

The concrete per-style FEEL TABLE the 9100 implementor bakes into the built-in style
tables (`9110` default `GrooveParams`, `9120` default tempo), plus an exact
statement of what `GrooveParams.swing` / `swing_grid` can and cannot express (input
to `9130` triplet/shuffle grid). Targets `groove.hpp` and `common/time.hpp`
(`BpmX100`, clamps 2000..40000).

### 5.0 How the swing model actually behaves (measured)

Constants: `kTicksPerStep = kPpqn/4 = 240` ticks per 16th; one beat = 960 ticks.
`groove::apply` (`groove.hpp:65`) swings ONLY the off-beat of `swing_grid`:
`swing_grid = 8` → delays `step % 4 == 2`, i.e. steps **2, 6, 10, 14** (the off-8th);
`swing_grid = 16` → delays every **odd** step (the off-16th). The delay is
`offset = kTicksPerStep*2*swing/300 = 1.6 * swing` ticks, so the swung off-8th
(nominal 480) lands at `480 + 1.6*swing`:

| swing % | off-8th tick | long:short | feel |
|---:|---:|---:|---|
| 0 | 480 | 1.0 : 1 | straight |
| 50 | 560 | 1.4 : 1 | light swing |
| 62 | 579 | 1.52 : 1 | medium jazz swing (~3:2) |
| 72 | 595 | 1.65 : 1 | firm shuffle |
| 75 | 600 | 1.67 : 1 | (5:3) |
| 100 | 640 | 2.0 : 1 | exact triplet 8th (hard bebop/shuffle) |

**Key result:** `swing=100, swing_grid=8` places the off-8th at exactly 2/3 of the
beat = a real triplet 8th. The two-note (long-short) swing/shuffle feel is FULLY
expressible by the existing model — no new grid required for it. Two caveats:

1. Swing only moves the OFF-beat step. The current feel-genres fake their swing by
   authoring the swung note on **step 3/7/11/15** (e.g. blues/shuffle ride at steps
   `{0,3},{4,7},{8,11},{12,15}` = 720 ticks = a *dotted* 3:1, harder than a
   triplet). Those sit on steps swing does NOT touch. To let `swing` drive the feel,
   the feel-genre patterns must be re-authored onto the straight off-8ths
   (2,6,10,14) — deliberate golden churn on those 3 styles, the whole point of 9100.
2. `accent` (`groove.hpp:80`) boosts beat 1 (+30%·a) and beat 3 (+15%·a) but
   subtracts on beats 2 & 4 (−8%·a on steps 4,12). The snare backbeat that defines
   pop/rock/motown/country/funk sits on steps 4 & 12, so a large `accent` *softens
   the backbeat* — wrong for those genres. Keep `accent` small and only where the
   pulse (not the backbeat) is the identity.

### 5.1 The feel table (copy into the style tables)

`swing_grid` is don't-care wherever `swing = 0`; shown as 8 = default.

| style | tempo (BpmX100) | swing % | swing_grid | accent % | triplet grid (9130)? | rationale |
|---------|---:|---:|---:|---:|---|---|
| basic | 12000 | 0 | 8 | 0 | no | Neutral demo. Straight — byte-identical golden. |
| pop | 12000 | 0 | 8 | 0 | no | ~120 canonical, straight backbeat. Frozen. |
| rock | 13000 | 0 | 8 | 0 | no | ~120–140, straight 8ths, hard backbeat. Frozen. |
| ballad | 7200 | 0 | 8 | 0 | no | 60–80, straight, long gates. Frozen. |
| funk | 10800 | 0 | 8 | 0 | no | ~100–115. Identity is 16th SYNCOPATION, not swing. Frozen. |
| disco | 12200 | 0 | 8 | 0 | no | 110–130, four-on-the-floor, straight octave bass. Frozen. |
| house | 12800 | 0 | 8 | 0 | no | ~128, four-on-the-floor, straight. Frozen. |
| swing | 14000 | 62 | 8 | 12 | no (two-note swing covered) | **FEEL-GENRE.** Medium jazz 120–160; swing=62 ≈ 3:2. Re-author ride/comp onto straight off-8ths. |
| bossa | 13000 | 0 | 8 | 0 | no | NOT swung — feel is clave SYNCOPATION, already authored (kTwoBass 0,6,8,14). Frozen. |
| samba | 10400 | 0 | 8 | 0 | no (subtle 16th ≠ triplet) | Counted in 2, ~96–104. Surdo + tamborim 16ths; real swing is a subtle 16th push, not a triplet. Frozen. |
| reggae | 7500 | 0 | 8 | 0 | no | 60–90, one-drop + off-beat skank authored. Frozen. |
| country | 12000 | 0 | 8 | 0 | no | Boom-chick train ~120, straight. Frozen. |
| blues | 6600 | 75 | 8 | 10 | **YES for authentic 12/8** | **FEEL-GENRE.** Slow blues 12/8, 60–80; swing=75 ≈ firm shuffle. Two-note covered; true three-note 12/8 ride needs 9130 (§4.3). |
| shuffle | 13000 | 72 | 8 | 10 | no | **FEEL-GENRE.** Texas shuffle ~120–140, firm triplet-8ths; swing=72. Re-author onto straight off-8ths. |
| latin | 18000 | 0 | 8 | 0 | no | Salsa/mambo cut time ~180 quarter (≈90 in 2/2). Clave + tumbao authored. Frozen. |
| motown | 12400 | 0 | 8 | 0 | no | Soul backbeat ~120–130, straight 8th soul bass. Frozen. |

**Which styles MOVE vs STAY.** STRAIGHT / frozen goldens (13): basic, pop, rock,
ballad, funk, disco, house, bossa, samba, reggae, country, latin, motown — at
`swing=0, accent=0`, `groove::apply` returns zero timing offset and unchanged
velocity ⇒ byte-identical output; only tempo (9120) may touch these. FEEL-GENRES
that MOVE (3): swing, shuffle, blues — their identity IS the swing; they get real
swing values and are expected to churn (and must be re-authored onto straight
off-8ths). Musicological correction: **bossa and samba are feel-genres but their
feel is clave syncopation, not triplet swing** — they stay straight; only
swing/shuffle/blues are *swing* genres.

### 5.2 Ranges, feasibility, flags

- **All values in-range.** Tempos 6600..18000 inside `kMinBpm=2000`..`kMaxBpm=40000`;
  swing/accent 0..75 inside 0..100; `swing_grid ∈ {8,16}`. Nothing clamps.
- **No dependency added.** Every value is data baked into existing `constexpr`
  tables. SHIPPABLE within the core doctrine.
- **Golden churn is bounded and intentional:** limited to the 3 feel-genres.
- **FLAG — tempo vs goldens (9120):** per-style default tempo does NOT move note tick
  positions (scheduling is tick-based); it only changes the tempo meta / real-time
  playback. If the golden harness captures a tempo meta event, styles whose default
  ≠ 120.00 churn *only that meta*. Confirm whether goldens pin tempo before landing
  9120. Orthogonal to the swing churn.
- **FLAG — latin felt tempo:** 18000 (180.00) is the genre-correct cut-time
  salsa/mambo quarter tempo but high relative to the others (makes the authored 16th
  montuno fast). Authentic, noted as a conscious choice; owner may prefer a tamer
  default (e.g. 15000) for uniform-feeling built-ins.

### 5.3 9130 — what a true triplet grid must add (and what it need NOT)

Already covered by `swing` + `swing_grid`, do NOT build a model addition for these:
two-note long-short **8th swing** (jazz ride, Texas shuffle, blues shuffle ride,
boogie shuffle bass): `swing_grid=8`, swing 62–100 (=exact triplet 8th at 100);
two-note **16th swing** (optional funk/latin micro-push): `swing_grid=16`.

NOT expressible by any `swing` value — the ONLY thing 9130 must add: **three
evenly-spaced triplet notes per beat.** Triplet-8th positions are 0, 320, 640 ticks;
the 16th grid offers 0, 240, 480, 720, so ticks 320 and 640 are not representable.
A pattern that must *articulate all three* triplet notes (a true 12/8
"spang-a-lang" playing the middle triplet, boogie-woogie 8th-note-triplet bass/piano,
gospel 12/8, triplet drum fills) cannot be authored today. Among the 16 built-ins
**only `blues`** has a genre norm (12/8) that genuinely wants it; its current
authoring fakes it as a two-note long-short which `swing=75` reproduces faithfully
as a shuffle. So 9130 is a **quality upgrade for blues (and any future
gospel/boogie style), not a blocker for any current style's basic feel.** shuffle
and swing are two-note by nature and need nothing beyond `swing`.

Design hint (not code): a per-style opt-in subdivision adding authorable step
positions at `beat*k/3` — a 12-slot (triplet-8th) or 24-slot (triplet-16th) bar
grid — so an event can land on the middle triplet. Additive metadata + a
resolve-time position map; pure integer tick math (same regime as `kTicksPerStep`);
no-heap, dual-target, does NOT touch `GrooveParams`.

### 5.4 Swing re-authoring spec (implementer-ready)

The event-by-event re-authoring for `swing`, `shuffle`, `blues` so the shipped
`GrooveParams.swing` / `swing_grid` produce the feel from STRAIGHT-grid notes.

**Per-style offsets** (`offset = (kTicksPerStep*2*swing)/300 = (480*swing)/300`,
integer division), for `swing_grid == 8` which delays steps 2, 6, 10, 14:

| style | swing % | off-8th offset (ticks) | step 2 lands at | long:short |
|---------|--------:|-----------------------:|----------------:|-----------:|
| swing | 62 | 99 | 579 | 1.52 : 1 |
| shuffle | 72 | 115 | 595 | 1.63 : 1 |
| blues | 75 | 120 | 600 | 1.67 : 1 |

Consequences: steps 3/7/11/15 (the current dotted authoring) are NOT touched by the
engine — swung material must MOVE onto 2/6/10/14; steps 0/4/8/12 (down-/back-beat)
get offset 0 and stay put (keep downbeat kicks, backbeat snares, walking-bass
quarters exactly where they are); the engine swings EVERY note on 2/6/10/14 globally
(correct for comping stabs; a negligible artifact for the odd 16th-pickup tom in
`blues kBrkD`).

**Exact `GrooveParams` per style** (field order `swing, humanize_timing,
humanize_velocity, accent, swing_grid, quantize, seed`; in the `Style` aggregate
`groove` comes BEFORE `tempo`):

```cpp
// swing.hpp
inline constexpr Style kStyle{.name="swing",   .sections=Span<const StyleSection>(kSections),
    .groove={.swing=62, .accent=12, .swing_grid=8}, .tempo=14000};
// shuffle.hpp
inline constexpr Style kStyle{.name="shuffle", .sections=Span<const StyleSection>(kSections),
    .groove={.swing=72, .accent=10, .swing_grid=8}, .tempo=13000};
// blues.hpp
inline constexpr Style kStyle{.name="blues",   .sections=Span<const StyleSection>(kSections),
    .groove={.swing=75, .accent=10, .swing_grid=8}, .tempo=6600};
```

`humanize_timing = humanize_velocity = quantize = 0` (DELIBERATE — isolates swing so
the regenerated golden is verifiable to the tick; humanize is an orthogonal later
pass). `seed` default (1), `swing_grid = 8`, all in range. Accent effect at 10–12
(integer math): `(30*a)/100 = +3` on step 0, `(15*a)/100 = +1` on step 8,
`(8*a)/100 = 0` on steps 4 & 12 — so the backbeat is NOT softened at these small
values; a light pulse lift only. Keep it, or drop to 0 for zero velocity churn.

**Rule R (the global remap, all three styles):** in every event array, for every
event with `step ∈ {3, 7, 11, 15}`, remap `3→2, 7→6, 11→10, 15→14`. Nothing else
changes (`tone`, `octave`, `vel`, `gate`, `src`, `gesture` stay); notes already on
even steps stay; onbeat drums/bass stay. **Exceptions — DO NOT remap** (16th figures
/ ornaments, not swung eighths):

- `kFDD` in ALL three — the peak dense 16th descending tom fill (populates both
  2/6/10/14 and 3/7/11/15, a straight 16th blast). Leave entirely.
- `blues kBrkD` — the stop-time pickup roll on 12,13,14,15 is a 16th pickup. Leave.
- `kLeadLick` step 11 in ALL three — the chromatic/blue grace note is a half-step
  pickup INTO step 12; remapping to 10 would collide with the step-10 line note and
  destroy the bebop approach. Leave step 11 (its swung eighths at 10/14 are already
  even and swing correctly).

The per-array change list is the exhaustive expansion of Rule R (verified by grep)
and carries no information beyond it plus these exceptions.

**Verification intent.** After Rule R + `.groove`, a re-authored off-8th emits at
`step*240 + offset`: swing (offset 99) step 2→579, 6→1539, 10→2499, 14→3459;
shuffle (115) 2→595, 6→1555, 10→2515, 14→3475 (the `kShufBass` fifth that was at
step 3=720 now emits at 595, engine-driven); blues (120) 2→600, 6→1560, 10→2520,
14→3480. Invariants: onbeat events (0/4/8/12) emit at exactly `step*240` (offset 0,
unchanged); gate is preserved (the same offset rides note-on AND note-off,
`groove.hpp:100,108`); exception-array notes on odd steps emit at `step*240` (no
swing); the 13 straight styles are untouched → byte-identical goldens. If the golden
shows the swung notes at exactly these ticks (not merely "some churn"), the feel
landed.

**What CANNOT be reached this way (needs 9130 — do NOT block on it).** The model
swings into TWO-note long-short per beat — exactly right for `swing` and `shuffle`
(jazz ride and Texas shuffle are two-note by nature). `blues` is genuinely 12/8; the
ship-now approximation renders it as a two-note shuffle (`swing=75` faithful): the
ride in `kIn1D/kIn2D/kAD/kBD/kDD`, `kBoogieBass`, `kArpTrip` (climbs on swung
eighths, NOT true triplets despite the name), the dominant-7 comp stabs. 9130 would
later add a true three-note "spang-a-lang", a genuine triplet arp, and an optional
8th-note-triplet boogie bass — needing slots at tick 320 and 640 the 16th grid
cannot name. The swing pass ships complete WITHOUT 9130.

### 5.5 Owner decisions for 9100

1. Re-author the 3 feel-genres onto straight off-8ths so `swing` drives the feel
   (recommended — otherwise the per-style swing knob only partially engages).
2. Whether 9130 is built now for `blues` (authentic three-note 12/8) or deferred
   (the two-note shuffle ships today without it).
3. Optional accent / 16th-swing on pulse/funk styles — musically minor, churns
   otherwise-frozen goldens; recommended: leave at 0.
4. `latin` default tempo (180.00 authentic-fast vs a tamer uniform default).

---

## 5. The Yamaha SFF/CASM corpus and its genre rules

The downloaded arranger-style corpus under `projects/resources/` is the
order-of-magnitude prize: a borrowed-chord, real-world accompaniment vocabulary far
past 16 hand-authored styles. All counts below were measured on the actual files.

### 6.1 Verdict and corpus facts (measured)

**The files are VALID.** 1010 `.sty`, plus 120 `.prs` (Korg), 74 `.syx`, 22 `.sst`,
32 `.mid`. The `.sty` are Yamaha **Style File Format** (SFF1 and SFF2) — Standard
MIDI Files (Format 0, one `MTrk`) carrying pattern data, followed by proprietary
chunks (`CASM`, `OTS`, `MDB`, `MH`). **99%** start with a valid SMF header `MThd`
(20 files, 1%, do not — junk/misnamed/partial, discard). A random `inspect` sweep of
300 files: 100% parsed (`rc==0`), 100% detected as SFF, 293/300 (98%) carry a `CASM`
chord-transposition block, 287/300 expose a decodable embedded SMF, 0 crashes.
Sections per file are the full Yamaha set (Intro A/B/C, Main A/B/C/D, Fill In
AA/BB/CC/DD (+BA), Ending A/B/C, sometimes Break — 8–15 named sections each).

- **Packs:** PSR-S950/S910/1700 (Yamaha PSR/Tyros), PA600/PA800/pa3x (Korg, some
  re-saved as `.sty`), plus Indian/Indonesian, Greek, Italian sets.
- **Genre distribution** (filename tokens, top): pop 77, country 76, rock 39, ballad
  38, "beat" 37, swing 29, waltz 21, jazz 21, dance 20, bossa 20, shuffle 18, slow
  17, funk 16, disco 14, blues 13, latin 12, samba 10; long tail: tango, foxtrot,
  bolero, rumba, cha-cha, beguine, reggae, march, soul, r&b, salsa, merengue, mambo,
  calypso, arabic, ska, schlager, musette.
- **PPQN:** mostly 1920 (Tyros/S-series), some 96 (older PSR-1700). An importer must
  read `MThd` division and rescale to arrangrr's PPQN=960.
- **Time signature:** overwhelmingly 4/4; 3/4 for waltz/musette; 6/8 for some
  marches/ballads.
- **Measured tempi (one representative each):** Bossa 120, Samba 114, Funk 105,
  Disco 140, Swing 202 (double-time), Waltz 90 (3/4), Country 92, Rock 121, Pop 98,
  Reggae 93, Blues 172 (shuffle-counted), Tango 123, Ballad 76, March 126, Latin 130.

**Converter status.** `arrstyle-converter inspect <f.sty>` works on 100% of the
sample, 0 crashes (detects `format: sff`, CASM present/absent, embedded SMF, PPQN,
tempo, time signature, track count). `import-sff <f.sty>` is an **inspect-only MVP**:
returns a graceful `"SFF import is not implemented (inspect-only in this MVP)"`
(rc=1). The recogniser routes `.sty/.sst/.prs` to `kYamahaSff` (`cli.cpp:71`) and
`looks_like_sff()` keys on the `CASM/Sff1/Sff2/CSEG` markers
(`sff_import.cpp:38-41`), so the front door is built; only the CASM/CSEG/OTS decoder
body is missing (`sff_import.cpp:70-76`). Finishing SFF decode (CASM→NTT) is the one
piece of work that turns a valid reference set into an import pipeline.

### 6.2 SFF structure (what the decoder must read)

1. **Container:** SMF Format 0. `MThd` → division (PPQN). One `MTrk` holds ALL
   sections back-to-back, delimited by marker meta-events (`FF 06 len text`):
   `"Intro A"`, `"Main A"`, `"Fill In AA"`, `"Ending B"`, … Each section loops
   `bars` measures.
2. **Channel map (the 8 style parts)** — Yamaha fixes parts to MIDI channels
   (1-based): **9** Rhythm Sub, **10** Rhythm Main (drums), **11** Bass, **12**
   Chord 1, **13** Chord 2, **14** Pad, **15** Phrase 1, **16** Phrase 2 (confirmed:
   parts cluster on 0-based ch 8–15). Maps 1:1 onto arrangrr roles.
3. **Source chord:** the recorded notes are written over a fixed source chord (almost
   always **CMaj7**, source root C, type Maj7); at playback the engine transposes
   them to the played chord.
4. **`CASM`** (Chord Arrangement Style Music) — the transposition brain, one
   `Ctab`/`Ctb2` per channel: source-chord root+type, **NTR** (Note Transposition
   Rule: `Root Fixed` for drums, `Root Transpose` for bass, `Guitar` for strummed
   chords), **NTT** (Note Transposition Table: `Bypass`, `Melody`, `Chord`, `Bass`,
   `Melodic Minor`, `Harmonic Minor`, …), note low/high limits, and a retrigger rule
   (held note on chord change: stop / retrig / pitch-shift). **This IS arrangrr's NTT
   (D24) in another dialect.**
5. `OTS` (One Touch Setting) = 4 voice/effect presets → arrangrr per-role
   `gm_program`, not pattern shape. `MDB` (Music Finder), `MH` (Multi Pad) =
   peripheral, ignore for patterns.

### 6.3 The one dual-use function, and the Yamaha → arrangrr mapping

**"Apply a pattern on the fly" and "generate a pattern statically" are the SAME
operation run in two places.** In arrangrr that operation already exists —
`Arranger::resolve(pattern, ev, key, chord)` — and CASM NTR/NTT is its Yamaha
equivalent. Apply-on-the-fly = feed a `StylePattern` of chord-relative `StyleEvent`s
through `resolve()` against the *live* `ChordState` (every tick). Generate-statically
= feed the SAME events through the SAME `resolve()` against a *chosen* `ChordState`
(bake a CMaj7 or a target chord), emitting concrete notes to a table/MIDI. Identical
function, different `chord` argument and different sink — no second code path, the
whole design win (D24 NTT). A genre rule that generates well also applies well.

| Yamaha SFF | arrangrr |
|---|---|
| Rhythm Main/Sub channel, NTR=Root Fixed | role kDrums/kPerc, `RolePolicy::kFixed` |
| Bass channel, NTR=Root Transpose, NTT=Bass | role kBass, `kChordTone` (root/fifth) + `NoteSource::kInterval` for approach notes |
| Chord 1/2, NTT=Chord | kChord1/kChord2, `kChordTone` indices 0..n; `VoicingPolicy::kLead` (D41) for guitar/piano |
| Pad, NTT=Chord, long gates | kPad, `kChordTone`, held gates |
| Phrase 1/2, NTT=Melody/Melodic Minor | kPhrase/kLead, `NoteSource::kScaleDegree` (D39) |
| Guitar NTR + strum offset | `ChordGesture::kStrumUp/Down` (D40) |
| Source chord CMaj7 | the `chord` passed to `resolve()` when baking |
| Section marker "Main A"/"Fill In AA"/"Ending B" | `SectionType::kVarA/kFillA/kEnding2` |
| CASM note low/high limit | `kRoleAnchor` + octave clamp in `resolve()` |

### 6.4 Universal rules (all genres)

**U1.** Rescale time: read `MThd` division, map every tick to PPQN=960
(`tick_960 = tick_src * 960 / div_src`). 1920→÷2, 96→×10.
**U2.** One style = up to 13 `SectionType`s; always populate at least `kVarA`
(Main A), one `kFill`, one `kIntro`, one `kEnding`. Main A→D = increasing
energy/density.
**U3.** Drums/perc are `kFixed` (literal GM notes), NEVER transposed — mirror CASM
NTR=Root Fixed. Everything melodic/harmonic is chord-relative.
**U4.** Bass is chord-relative but bass-anchored: root on beat 1, fifth/octave on
strong beats; approach notes as `NoteSource::kInterval` (±1, ±2 into the next root).
Anchor low (kRoleAnchor kBass=36).
**U5.** Chord parts are chord-tone stacks (`kChordTone` 0,1,2[,3]); apply
`VoicingPolicy::kLead` so voicings don't jump octaves between chords (D41).
**U6.** Pad = chord held with long gates (kGateHeld/kGateHalfBar), few onsets, lowest
activity — fills register under the comp (kRoleAnchor kPad=48).
**U7.** Phrase/lead = melodic, key-diatonic: `NoteSource::kScaleDegree`, NOT chord
tones, so lines walk the scale (D39). Sparse; motif on section starts.
**U8.** Bake the source chord as CMaj7 when generating a reference table, then
re-resolve to any target chord with the same `resolve()` — no separate code path.
**U9.** Density ladder per section: Intro < Main A < Main B < Main C < Main D; Fill =
1 bar, highest drum density, resolves to the returning Main (arrangrr one-shot).
**U10.** Ending decrescendos: fewer onsets, a held final tonic chord, then
`stop_transport` (arrangrr kEnding semantics).
**U11.** Groove feel is per-genre: set `GrooveParams.swing` for shuffle/jazz/bossa,
`accent` on beat 1 always, `humanize_timing/velocity` small (2–6%) for realism.
**U12.** Velocity shape: downbeat > backbeat > offbeat; ghost notes 30–50 vel on
hats/snare for funk/shuffle. Encode in `StyleEvent.vel`, not post-hoc.
**U13.** Chord gestures (D40): `kStrumUp/Down` for guitar/harp (fixed micro-stagger),
`kRollUp/Down` for piano/harp flourishes (spread across the gate).
**U14.** Section length: most Mains are 1–2 bars looped; multi-bar Mains exist —
respect `StyleSection.bars`, don't assume 1.
**U15.** Retrigger policy: when the live chord changes mid-note, chord/pad parts
re-resolve on the next grid step (arrangrr already re-resolves each tick) — mirror
CASM "retrigger" vs "stop".
**U16.** Two chord channels differ by register/rhythm, not notes: Chord 1 = on-beat
block/stab, Chord 2 = offbeat/syncopated upper voicing. Keep them distinct.
**U17.** Note limits: clamp each role to its CASM low/high (arrangrr: `kRoleAnchor` +
`resolve()` 0..127 clamp) so transposition never runs off the register.
**U18.** Discard the 1% non-`MThd` files and any with 0 sections; validate every
import with `arrstyle-converter inspect` first (the cheap gate).

### 6.5 Per-genre rules (apply on the fly AND generate)

Tempi from measured data + convention (~ = conventional).

**Pop** (~98 BPM, 4/4, straight). P1 Straight 8th, swing=0; kick 1 & 3 (+ "and" of
2), snare backbeat 2 & 4. P2 Closed hats every 8th, open hat on the "and" of 4 into
the bar. P3 Bass: root on 1, root/fifth on 3, an 8th approach into bar 2. P4 Chord 1
= quarter block triads (`kChordTone` 0,1,2), Chord 2 = offbeat "and" stabs; Pad
holds; voice-lead.

**Ballad** (~76 BPM, 4/4, often 6/8). B1 Low density, long gates; hats on quarters or
brushed; snare soft on 2 & 4. B2 Bass: root on 1, fifth on 3, whole-note feel. B3
Pad dominates (sustained chord, `kLead` essential). B4 Phrase: sparse `kScaleDegree`
motif on bar starts; `kRollUp` harp/piano bloom on section changes.

**Rock** (~121 BPM, 4/4, straight, hard). R1 Kick 1 & 3 (double-kick "and-3" for
driving rock), snare 2 & 4 hard (vel 110+). R2 Hats/ride straight 8ths; crash on
section starts (kFixed kCrash). R3 Bass locks to kick, root-driven eighth pulse;
octave jumps on Main C/D. R4 Power-chord comp = root+fifth only (`kChordTone` 0,2),
omit the third; medium gate; `kStrumUp` for guitar attack.

**Country** (~92 BPM, 4/4; also shuffle & waltz). C1 Train/two-beat bass: root on 1,
fifth on 3 (alternating) — the signature; encode `kChordTone` 0 then 2. C2
Brushed/rim snare on 2 & 4, hats straight or light shuffle. C3 Chord 2 = offbeat
guitar "chank" (`kStrumUp`, short gate, on the "ands"). C4 Pedal-steel/lead phrase =
`kScaleDegree` with bends emulated by adjacent degrees; sparse.

**Swing / Jazz / Big Band** (up to ~202, 4/4, TRIPLET swing). J1 `GrooveParams.swing`
high (offbeats pushed ~2/3). J2 Ride spang-a-lang (kFixed kRide), hats on 2 & 4
(foot). J3 Walking bass: one note per beat, `kChordTone` root/third/fifth +
`kInterval` chromatic approach to the next root — continuous quarters. J4 Comp =
syncopated upper-structure stabs (Chord 2), NOT every beat; `kLead`, seventh chords
(`kChordTone` up to index 3). J5 Big-band: brass "hits" as block stabs on
anticipations (the "and" before 1 and 3); Phrase 1/2 carry the shout-chorus line.

**Bossa Nova** (~120, 4/4, straight-16 with clave). BN1 No backbeat: rim/side-stick
clave (kSideStick) — the signature. BN2 Bass: root on 1, fifth on the "and of 2" (the
two-feel), tie into 3. BN3 Guitar comp = syncopated jabs off the clave, `kChordTone`
with 7ths/9ths; `kStrumUp` tiny stagger. BN4 Light swing (subtle), soft velocities;
pad optional and quiet.

**Samba** (~114, 4/4, fast-16 surdo). SM1 Surdo bass-drum on beat-2 emphasis (kFixed
kKick low), agogô/shaker 16ths. SM2 Bass: root–fifth ostinato with the surdo,
syncopated pushes. SM3 Percussion-dense (cabasa/tamborim/agogô 16ths, kFixed) —
highest perc density of any genre; populate kPerc heavily. SM4 Chord jabs short and
syncopated; `kScaleDegree` phrase optional.

**Latin ballroom — ChaCha / Rumba / Mambo / Beguine / Bolero** (~130). L1 Cha-cha:
cowbell + the "cha-cha-cha" on 4-&-1; bass root on 1 & the "and of 2". L2
Rumba/Bolero (~100): clave + conga tumbao; bass anticipated (the "and of 2" and 4) —
`kChordTone` + `kInterval` push. L3 Mambo (faster): montuno piano — a repeating
syncopated 2-bar `kChordTone` arpeggio; prime `kRollUp`/arpeggiated candidate. L4
Congas/timbales/bongos define the genre — dense kFixed perc; keep clave direction
(2-3 vs 3-2) consistent. L5 Bass never on every beat — the anticipated bass IS the
Latin feel.

**Funk** (~105, 4/4, straight-16, syncopated). F1 Ghost notes everywhere: snare/hat
16ths at vel 30–50 between accents (critical). F2 Kick syncopated 16ths ("the one"
heavily accented, vel 120); tight hats. F3 Bass is the star: syncopated 16th
`kChordTone` root + `kInterval` octaves, slap accents (high vel) — highest bass
density. F4 Chord = short 16th stabs on offbeats (Chord 2), muted-guitar `kStrumUp`
tiny gate.

**Disco / Dance / House** (Disco ~140, 4/4, four-on-floor). D1 Four-on-the-floor kick
every beat (kFixed), open hat on every "and", clap/snare on 2 & 4. D2 Bass:
octave-jumping 8th ostinato (root then root+octave via `kInterval` +12) — the "octave
bass". D3 Strings/pad = sustained `kChordTone` with `kLead`; string stabs on
offbeats. D4 House: syncopated piano chord stabs (`kChordTone` 7th/9th), sparser than
disco; keep the four-on-floor.

**Blues / Shuffle** (Blues ~172 shuffle-counted, 4/4 triplet). BL1 Triplet shuffle:
`GrooveParams.swing` high; hats/ride play the shuffle (1-and-a, middle triplet
dropped). BL2 Shuffle bass: the 1-5-6-b7 boogie walk as `kChordTone`+`kInterval`, one
note per shuffle-eighth. BL3 Dominant-7 comp (`kChordTone` up to index 3 = b7); stabs
on the shuffle grid.

**Reggae** (~93, 4/4, offbeat). RG1 The "skank": chord ONLY on the offbeats, silent
on downbeats — Chord 1 as short `kChordTone` stabs on 2-&, 3-&, 4-& (or all "ands").
RG2 One-drop: kick + snare together on beat 3 (not 1). RG3 Bass = deep, sparse,
melodic root/fifth line with rests; long gaps.

**Waltz / Musette** (~90, 3/4). W1 OOM-pah-pah: bass root on 1, chord stabs on 2 & 3
(`kChordTone` block). W2 3/4 grid — set section bar math to 3 beats; snare/brush on
2 & 3. W3 Musette: fast 3/4, accordion `kScaleDegree` phrase + tremolo feel.

**Tango** (~123, 4/4, marcato). T1 Marcato 4: staccato accented chord/bass on all
four beats (short gates, high vel). T2 Habanera bass option: dotted-8th + 16th + 2
quarters (`kChordTone`+`kInterval`). T3 Dramatic `kScaleDegree` phrase with the 3-3-2
rhythmic grouping.

**March / Polka** (March ~126, 2/4 or 4/4). M1 Bass on 1 & 3 (oom), chords on 2 & 4
(pah) — the two-beat march engine. M2 Snare rolls/flams in fills (kFixed toms +
snare), crash on downbeats. M3 Polka: fast 2/4 M1 with an offbeat `kStrumUp` chord
bounce.

### 6.6 Using this corpus (both directions, one engine)

- **Static generation of arrangrr styles:** pick a genre block → author
  `StylePattern`s per role following its rules (drums `kFixed`, harmony
  `kChordTone`/`kScaleDegree`/`kInterval`, gestures/voicing per D40/D41) → a pure
  `constexpr` table, STM32-safe (D33).
- **On-the-fly application:** the same `StylePattern`s run through `resolve()` against
  the live chord each tick — no second code path (D24 NTT).
- **Finishing the importer:** implement `import_sff` (`sff_import.cpp`) to decode
  CASM → per-role NTR/NTT → arrangrr `RolePolicy`/`NoteSource`, read section markers →
  `SectionType`, rescale PPQN (U1). The recogniser and `inspect` path already work on
  100% of the corpus; only the CASM→NTT decode body is missing. No new dependency,
  host-only work.

---

## 6. Generation: stylizer, melody, and taxonomy

### 7.1 How much genuine difference is shippable today

Most missing difference is UNDERUSE, recoverable now, with a short list of true model
gaps. What makes two accompaniments feel like different music, ranked by ear-weight,
with today's expressibility:

1. Drum idiom + groove/feel — **half here**: idiom EXPRESSIBLE and used; feel
   (swing/tempo) now shipped per §4.
2. Bass function — **EXPRESSIBLE today, still under-used**: `kInterval` gives
   octave-jump, chromatic approach, anticipation.
3. Melodic line + its variation — **thin + generative GAP**: `kScaleDegree` exists;
   generation does not.
4. Comp rhythm/register — **EXPRESSIBLE, under-varied**; voicing SHAPE a gap.
5. Instrumentation — **fully expressible, already used** (weakest differentiator).

A large step up in genuine difference is SHIPPABLE now purely by AUTHORING what the
model already allows: `kInterval` basslines per genre (walking/octave/anticipated),
`kScaleDegree` signature phrases on the unused `kPhrase`/thinly-used `kLead` roles,
genre gestures on the 10 gesture-less styles, distinct comp rhythms per §5.5. The
CEILING — swing that FEELS swung (shipped, §4) and a melody that CHANGES — needs the
melodic generator (§6.3).

### 7.2 A standalone stylizer: plain MIDI → pop/rock/samba/…

A stylizer is the dual-use `resolve()` (§5.3) run offline over an analyzed input.
Offline is fine. Options with tradeoffs:

- **Option A — Host offline "re-accompanier" (rule-based). SHIPPABLE (host tool).**
  Analyze input MIDI → chord track (the core-portable `ChordDetector`, D34, already
  exists) + section segmentation + key; keep the input's own melody on
  `kLead`/`kPhrase`; run a CHOSEN `Style`'s patterns through `resolve()` against the
  detected progression; emit MIDI. "Play the genre's band over YOUR song's chords and
  form." Reuses the arranger wholesale, no new core. Limit: it re-accompanies; it does
  not restyle the input's OWN drum/bass rhythm — it replaces them. **Recommended first
  increment** (same engine, different sink, no core code).
- **Option B — True part-restyler (rule-based, HOST-ONLY, more effort).** Map the
  INPUT's parts onto genre behavior: input drums → genre kit/pattern, input bass →
  genre bass FUNCTION, requantize to the genre's groove. Needs the per-style feel
  (§4, now shipped) and a genre mapping ruleset — the §5.5 rules ARE that ruleset.
  The richer "restyle".
- **Option C — Statistical/ML groove & melody transfer. INSTRUCTIVE-BUT-INFEASIBLE on
  device; HOST-ONLY with a flagged dependency.** A learned model could transfer feel
  convincingly but needs weights + a runtime/toolchain. DEPENDENCY FLAG: any ML
  runtime or weights is a new dependency requiring owner approval; the core stays
  dependency-free. Not shippable to the no-heap core.

An on-device stylizer of an arbitrary file is not the core's job (file analysis + I/O
is host work; the device already does the live equivalent). Mapping onto constructs:
input → `ChordSequence` (D28) + `Key`; target → `Style`; feel → `GrooveParams`;
gestures/voicing per D40/D41/D42; output via the existing MIDI writer; the §7 blob
lets the target style be swapped without a recompile.

### 7.3 Changing the MELODY, not just re-comping the chord

The point is a melodic LINE that varies, not the same authored four notes. Directions,
each costed against the no-heap dual-target core:

- **Direction 1 — Motif + transformation grammar. SHIPPABLE.** Author a short
  per-style motif as `kScaleDegree`/`kInterval` events; generate variations with
  deterministic, bounded operators — diatonic transposition, retrograde/inversion,
  rhythmic displacement, ornament/passing-tone insertion, contour-preserving reshape —
  constrained to the Key/NTT envelope so output stays wrong-note-proof.
  Seed-deterministic (D16), no heap, bounded fan — the D37 Director philosophy applied
  to pitch. Needs a new small stateful "melodic generator" module + a motif store;
  device-safe.
- **Direction 2 — Baked Markov/grammar over scale degrees + rhythm. SHIPPABLE
  runtime, HOST-ONLY training.** A per-style order-1/2 transition table over scale
  degrees and durations, TRAINED offline and baked `constexpr` into flash (the
  `canon-builder` codegen pattern proves "host program emits a freestanding
  artifact"). On device: a tiny deterministic sampler, no heap, table in `.rodata`.
  FLAG: training is host-only; nothing new on the device except a sampler + table.
- **Direction 3 — Neural melody model (RNN/transformer).
  INSTRUCTIVE-BUT-INFEASIBLE on device.** Musically strongest, but weights (100s
  KB–MB) + runtime blow the flash/RAM/realtime budget and add a dependency. HOST-ONLY
  tool at best, explicit DEPENDENCY FLAG. The ceiling, not a shippable plan.

Cross-cutting: melody generation is a NEW module ABOVE the arranger (the Director's
cousin), not a style-table change. The model already has the melodic SOURCE
(`kScaleDegree`/`kInterval`, D39) and empty melodic ROLES (`kPhrase` unused, `kLead`
thin) to receive a generated line; what is missing is the GENERATOR and its state. A
richer rhythmic grid (9130, §4.3) would let the line phrase against triplet/16th
feels.

### 7.4 Organizing the corpus (taxonomy, families, D44)

- **Family taxonomy by (feel × rhythmic engine), not by name.** Measured drum
  signatures cluster into: straight-8 backbeat (pop, rock, country, motown, ballad),
  four-on-the-floor (disco, house), 16th-funk (funk), triplet/shuffle (blues,
  shuffle, swing), latin-clave (bossa, samba, latin, reggae). Within a family, styles
  share the drum/bass skeleton and differ by voice, density, gesture and motif —
  forcing each new style to justify what it adds.
- **Factor shared skeleton from unique payload.** The shared part (role roster,
  section set, density ladder) becomes a reusable template; the UNIQUE payload (drum
  idiom, bass FUNCTION, groove/feel, signature gesture + motif) is the per-style data
  — exactly what §7's data format enables.
- **The order-of-magnitude prize is scale:** ingesting the validated 1010-style Yamaha
  corpus via the `arrstyle-converter` CASM→NTT decode (§5) — no new dependency,
  host-only. Recommended sequence: teach the converter to emit today's constexpr
  header from its `StyleModel` (§7 Step 0) → author far more, richer styles as DATA →
  then the blob + loader for no-recompile.

### 7.5 What needs an owner decision (generation)

1. **Per-style feel in the `Style` struct** — shipped as 9100 (§4). (Was the single
   biggest lever; it was an ABI/struct change.)
2. **Is generative melody in scope as a NEW module** (Director-adjacent, Direction
   1/2), or do we stay with authored `kScaleDegree` lines? Product fork.
3. **Stylizer scope** — Option A (host re-accompanier, shippable, no new code) vs B
   (part-restyler) vs C (ML, dependency). Any ML path = DEPENDENCY FLAG, owner
   approval; the core stays dependency-free.
4. **Corpus expansion** via finishing the SFF/CASM→NTT importer — no new dependency,
   host-only — is largely an execution decision, not a design one.

---

## 7. The pure-data (non-compiled) style format (DESIGN D44)

**Owner decision (roadmapped, not now):** keep BOTH representations — a pure-data
format for interchange/authoring/runtime-loading AND a generator that emits the
C++-style form from the data, so the STM32 target can keep using compiled styles.
Foreseen SW pieces: a style *inspector*, *generators*, *serialize/deserialize*, a
*style compiler* (data → `.cpp`), and a *non-binary* interchange format (an on-device
text parser stays refused). The authoring format, the on-device format, and the
built-ins' baking policy are three DIFFERENT layers, judged separately.

Payoffs worth keeping: **ingesting the 1010-style Yamaha corpus without a recompile**
(the actual prize — the event payload is small, `events * 10 B`, far below ~20–28 KB
of C++ *source* per style; blobs live comfortably on SD/flash D33) and
**runtime-loadable user styles** (a bounded no-heap loader + the storage HAL D33
foresees; strictly downstream of the format+loader). Refuse as premature scope: a
style library / marketplace as a design driver (the chosen blob enables it later for
free). Hot-reload is a HOST-only affordance (edit JSON → re-emit → reload), never a
device feature.

### Layer A — Authoring / interchange (HOST-ONLY): JSON, already exists

`arrstyle-converter` already has a host `StyleModel` and a byte-stable JSON writer
(`serialize.hpp`, schema v1). That IS the authoring/interchange layer. Every importer
(SFF/CASM, MIDI, ChordPro) already targets `StyleModel`; JSON is its serialisation. A
hand-authoring DSL/TOML is a possible future sugar, never with an on-device reader.
Reuse, do not reinvent.

### Layer B — On-device format: a flat, mmap-able, offset-based binary blob

One file per style (working name `.arrsty`). Properties:

- **Little-endian, fixed alignment (≥ 4).** Host x86 and Cortex-M7 are both LE —
  declare LE and `static_assert`/reject otherwise. `StyleEvent` has `uint16` fields
  (2-byte alignment); align section starts to 4. `packed` is refused on Cortex-M7
  (DESIGN.md line 627) — the blob is padded, not packed.
- **Versioned header carrying the invariants.** magic (`'A''S''T''Y'`),
  `format_version`, and `event_size` which MUST equal the in-RAM `sizeof(StyleEvent)`
  (== 10). The 10-byte invariant is enforced across the disk/RAM boundary: a blob
  built for a different event layout is rejected at load, not silently misread. Header
  also holds section count and the section-table offset.
- **Offset-based index, blittable leaves.** Section table, pattern table and name are
  byte offsets from the blob base. The `StyleEvent[]` arrays are stored verbatim
  (blittable — zero-copy preserved). The LEAF (`StyleEvent`) is identical on disk and
  in RAM (shared, not copied); the INDEX structs (`Style`/`StyleSection`/`StylePattern`)
  are NOT serialised — they hold `Span`s and are reconstructed at load.

Sketch (illustrative, not a spec):

```
Header      : magic u32 | format_version u16 | event_size u16 (==10)
              | section_count u16 | _pad u16 | name_off u32 | section_table_off u32
SectionRec  : type u8 | bars u8 | pattern_count u16 | pattern_table_off u32
PatternRec  : role u8 | policy u8 | voicing u8 | _pad u8
              | gm_program i16 | _pad u16 | event_count u32 | event_off u32
Events      : StyleEvent[]  (verbatim, 10 B each, 4-aligned section start)
Name        : NUL-terminated UTF-8
```

### Layer C — Built-ins: STAY baked as constexpr

Keep `kBuiltins[16]` compiled into firmware: zero RAM (flash mmap), guaranteed
present, CANNOT fail to load — the factory floor a performer relies on. Longer term
the same authoring JSON can emit BOTH the baked constexpr header and the blob from one
pipeline, so built-ins and user styles share a source of truth; desirable convergence,
not step one.

### The loader story — no heap, no filesystem

1. The blob arrives via the storage HAL (SD/internal flash, D33) or is memory-mapped
   from a flash region — its base address is known.
2. **Validate**: magic, `format_version`, `event_size == sizeof(StyleEvent)`, counts
   within the static caps. Reject → keep the current style; never stall the fire loop.
3. **Adapt-in-place into a pre-sized static pool.** Walk the section/pattern tables
   and build a `Style` + `StyleSection[]` + `StylePattern[]` in a `StaticVector` sized
   to `kMaxSections` / `kMaxPatterns` (D32 caps, `static_assert`-verified). Each
   rebuilt `Span<const StyleEvent>` points at `blob_base + event_off` — event arrays
   are consumed ZERO-COPY straight from the mmap'd blob; only the lightweight index is
   materialised in RAM. Name pointer = `blob_base + name_off`. This is filling a pool,
   not allocating (the D32 distinction).
4. The arranger keeps holding `const Style*` and iterating `Span`s. **Zero fire-loop
   changes.** For seamless style switch, use two index pools (double buffer),
   consistent with the existing `m_pending_style` swap.

Endianness: LE, asserted. Alignment: padded to 4, never `packed`. Versioning: in the
header, and the 10-byte event invariant is a load-time gate.

### Migration path

- **Step 0 (smallest, zero device risk — recommended first).** Add to
  `arrstyle-converter` an emitter that lowers its host `StyleModel` into a
  `style_model.hpp`-shaped constexpr header (the exact form `bossa.hpp` has today).
  Styles become authored-as-DATA (JSON) and mechanically lowered to the EXISTING baked
  format — unlocks ingesting the 1010 corpus into built-ins and kills hand-authoring
  of C++ tables, with NO core change and NO new runtime code. It still requires a
  recompile, acceptable for the first step because it de-risks the lowering (event
  resolution, section mapping, gate/PPQN units) against the compiler before any binary
  loader exists. Reuse the `emit_canon_header` *technique* ("host program emits a
  freestanding `<cstdint>`-only artifact"), not the canon-builder itself (it distils
  statistical genre aggregates, not full `StyleEvent` tables).
- **Step 1 — the real payoff.** Define Layer B, add the `StyleModel → .arrsty` emitter
  (same lowering, different sink), write the no-heap adapt-in-place loader. Removes the
  recompile, delivers runtime-loadable styles.
- **Step 2 — convergence.** Emit both baked header and blob from one JSON. Optional.

### Non-goals / traps to refuse

An on-device JSON/TOML parser or any human-readable format parsed on the target
(humans author on the HOST; on device this buys only allocation and parse latency on a
path that may run during a live style switch — refuse, D32); any load path that
allocates (D32); serialising the `Span`-based structs directly (non-portable);
`packed` structs on Cortex-M7 (line 627); growing `StyleEvent` past 10 bytes for
on-disk metadata (put metadata in the blob HEADER); designing for a marketplace now;
dropping the baked built-ins in favour of "everything loads" (the zero-RAM,
cannot-fail guarantee is a feature).

---

## 8. Live modulation — the "Pivot" transpose (PARKED)

**Pivot** is a played gesture that pivots a running progression to a new key — the
pivot-chord modulation device, made live. **Status: PARKED.** The owner chose Path 1
(plain-arranger literal chord-follow) for the JAM that ships now; this is the settled
design for when the feature returns.

**The model.** A running chord loop plays one chord per bar. On a live chord press,
the pressed root defines an ABSOLUTE global transpose measured against the ORIGINAL
chord at the currently-playing bar position — never cumulative:
`offset = pressed_root - original_root[P] (mod 12)`, recomputed from the untouched
original progression on every press. Owner's locked answers:

1. **Absolute, from the original**, recomputed each press (original `C A F G`; at the
   C bar press D → offset +2 → `D B G A`; later at the bar whose original is C press C
   → offset 0 → back to `C A F G`).
2. **Root-only**: each original chord keeps its stored quality (`C A(min) F G` +2 →
   `D B(min) G A`). Open sub-case (deferred): a press whose quality differs from the
   scheduled chord uses only its root.
3. **Persists** across loop wraps forever, until the next press.
4. **Reset** = press the original chord of the current bar (offset 0 falls out of rule
   1 arithmetically).

**Fact that reframes it (confirmed in code).** The `C A F G` loop is NOT the style.
The `basic` style has no progression of its own — its patterns re-root to whatever is
in the followed context. The loop is a SEPARATE, OPTIONAL `ChordSequencer` (the jam's
`setup.acmd` runs `seq new / seq add / seq loop on / seq play`). WITHOUT a sequencer
running, a live chord already persists across every bar — the plain "press a chord and
the band holds it" behavior already works. A live chord is clobbered ONLY when the
competing `ChordSequencer` fires under the default `kAuto` gate. So this is not a bug
fix and not the style's job; it is an OPTIONAL performance layer: a backing loop you
can live-modulate by interval.

**The unification insight.** "No progression" == a one-chord progression `[X]`;
pressing D sets an offset and the band holds D. So live steering with and without a
sequence is ONE mechanism — a global transpose offset over the current chord source.
It is genuinely true on the ROOT axis (with a one-cell source, `original_root[P]` is
constant, so `offset = pressed − constant` makes the band hold the pressed root — the
plain latched hold; one offset scalar, one code path, one steering writer). It is NOT
clean on QUALITY (the seam, ruled below).

### How commercial arrangers do it (researched)

Across Yamaha, Korg and Roland the live harmonic controls are three SEPARATE
mechanisms; none fuses them this way:

1. **Literal chord detection.** Every fingering mode is WYSIWYG — the chord you play
   is the chord the band plays (Yamaha Single Finger / Fingered / Full Keyboard / AI
   Fingered / AI Full Keyboard / Multi Finger; Roland E-A7; Korg Pa Expert mode). AI
   modes only improve anticipation of the *next* chord; they do not transpose a stored
   loop.
2. **A separate recorded chord loop** (Yamaha Chord Looper, Korg Chord Sequencer) for
   hands-free playback: it records your left-hand chords and loops them so the style
   engine plays them while your hands are free. While the loop plays, the loop drives
   the harmony — it is not a target you re-key by pressing chords over it.
3. **A separate, context-free global Transpose** (Yamaha TRANSPOSE buttons) shifting
   overall pitch in semitone steps as an independent function.

**No mainstream arranger lets you press a chord to RELATIVELY transpose a running
recorded progression.** The owner's model is a genuine novel hybrid: it fuses the
Chord Looper (mechanism 2) and the relative Transpose (mechanism 3) into a single
gesture, using the chord press to supply the transpose interval.

### Musical judgment

- **Absolute-from-original is correct.** "Put the song in D" yields the same key
  regardless of how you got there — no accumulating error, no path dependence; a
  performer thinks in absolute keys.
- **Root-only, quality-preserving is correct — because the feature IS a key change.**
  Root-only preserves HARMONIC FUNCTION: I-vi-IV-V stays I-vi-IV-V, re-keyed; the
  voice-leading contour is intact. Quality-propagation would be a MOOD-MORPH that
  destroys identity (C-Am-F-G under a minor press → Cm-Am-Fm-Gm, no longer the same
  progression). The only defensible use of the played quality is LOCAL: color the
  CURRENT bar only (a one-off substitution), which must not leak into the transpose.
- **Persist-until-next-press is correct** for a sustained modulation.
- **Reset-by-pressing-the-original-chord is arithmetically sound but ergonomically
  thin.** It asks the player to remember the ORIGINAL chord of the CURRENT bar after
  the display already shows the transposed chord — and with a moving loop that
  original is not necessarily the tonic. Musicians intuit "play the tonic to come
  home." Keep the gesture; add a context-free home (a Transpose-reset).
- **The gesture is overloaded.** A chord press everywhere else means "play THIS chord,
  literally." Repurposing it to silently re-key four bars belongs behind an
  explicitly-armed "modulate" mode, never as the default meaning of a press. (The
  owner already conceded this by choosing Path 1.)

### The quality seam (ruled, not glossed)

- **Single-cell source:** the press defines root AND quality of the held chord
  (literal follow).
- **Multi-cell recorded source:** the press supplies root only (transpose); each
  recorded cell keeps its own quality.

Reconciliation keeping ONE mechanism: the live press always SOUNDS the pressed chord
in full (root+quality) for the current bar at the instant of press (the normal
live-sound path), and sets the global root offset for subsequent bars. With a one-cell
source every bar is "the current bar", so the played quality persists — latched follow
recovered — with no persistent per-bar quality state. With a recorded loop, subsequent
bars fire their own recorded qualities on transposed roots. Same code, one documented
quality-locality rule.

### SPEC (parked — buildable shape)

**State (bounded, no heap, dual-target).** `std::int8_t m_root_offset = 0;` — global
root transpose, applied `% 12`, normalized to `[-6, +6]` for display (direction
cosmetic: `+2 == -10 mod 12`). Lives on `ChordSequencer`/`Engine`. The stored
`ChordSequence` is the immutable original reference, NEVER mutated. No new mode enum
for the root math (behavior is unified); one explicit arm flag to enable
"chord-press = transpose" vs the default literal follow.

**Capture / apply.** On a live steering press (`Producer::kDetect` or `kManual`) while
armed: determine the source cell — if `m_seq.playing()` with `length>0`, `P` = current
playback position and cell = `current_step_at(P)` (a bounded lookup over
`≤ kMaxChordSteps`; `on_tick` only matches exact starts, so mid-bar presses need this
separate active-step lookup), `original_root = resolve_root(seq, cell, /*offset*/0)`;
else the single home/followed cell. Then `offset := (pressed_root - original_root) mod
12`, normalized — absolute from the original, recomputed every press (never reads the
current offset → no drift). Immediate (default): set `m_root_offset`; SOUND the pressed
chord now (root+quality) for the current bar; subsequent bars fire from the sequencer
at their own downbeats using recorded qualities on transposed roots. Quantized
(opt-in, existing quantize flag): stage the offset to the next bar via
`stage`/`commit_bar` and expose it as `pending()` (this also fills the "next key"
readout). Boundary tie-break: a press exactly on a bar boundary reads the step active
AFTER that boundary's fire (a scheduler tie-break IS groove — leaving it undefined
makes the feel nondeterministic).

**Reset / lifecycle.** Press the original chord of the current bar → offset 0 (falls
out, no special case). `seq new` / `seq use` / `key set` / transport restart → offset
:= 0 (a new song is in its written key; "persists forever" is about loop WRAPS, not
session boundaries). Add a context-free `seq home` safety and a UI readout of the
original progression — backstop for the discoverability gap.

**How this REPLACES the `kAuto` clobber race (D47).** The live press no longer writes
the followed context as a competing producer. The `ChordSequencer` remains the SOLE
steering writer (`Producer::kSequencer` `commit_now` each bar), now deriving its root
from `original[P] + offset`. One writer per bar → the last-writer race is structurally
gone; `kAuto` is safe because only one producer of `current` exists while a sequence
plays. In the degenerate case only detect writes (also single-writer). The D47 gate
remains for the genuinely different detect-vs-manual selection but no longer has to
arbitrate seq-vs-live.

**ABI surface (minimal).** One arm flag (transpose vs literal follow); reuse the
existing quantize flag for the staged variant; reset needs no new command (falls out)
plus an optional `seq home`; one read query exposing `m_root_offset` and the staged
offset for the UI "current key / next key" readout. Single-finger vs fingered is
transparent — both yield a `root_pc` (the offset reads `root_pc` only; quality follows
the rule above). Determinism: one `int8` offset, `current_step_at` a bounded loop, one
add+modulo on the hot path, `fire` callback signature unchanged, constexpr-friendly,
identical host/arm.

### Sources

- [Playing Chords on Yamaha Keyboards — PSR Tutorial](https://psrtutorial.com/lessons/start/s50_fingering.html)
- [Single Finger and Fingered methods — Yamaha FAQ](https://faq.yamaha.com/usa/s/article/U0002033)
- [Tyros 5 fingering styles guide — ePianos](https://www.epianos.co.uk/tyros-5-fingering-styles-guide/)
- [Mastering AI Fingered Mode — Yamaha Hub](https://hub.yamaha.com/keyboards/workstations/genos-power-playing-mastering-ai-fingered-mode/)
- [Step Edit / Chord Looper — Sand, software and sound](https://sandsoftwaresound.net/step-edit-chord-looper/)
- [Genos2 overview — Yamaha USA](https://usa.yamaha.com/products/musical_instruments/keyboards/arranger_workstations/genos2/index.html)
- [Genos reference, Transpose in semitones — ManualsLib](https://www.manualslib.com/manual/1333828/Yamaha-Genos.html?page=36)
- [Chord Sequencer — Korg Pa1000 guide](https://manualzz.com/doc/o/12vopf/korg-pa1000-guide-the-chord-sequencer)
- [Chord recognition / Chord Sequencer — Korg Pa4X](https://www.korg.com/us/products/synthesizers/pa4x/page_2.php)
- [Chord recognition / accompaniment — Roland E-A7 manual](https://www.manualslib.com/manual/1062762/Roland-E-A7.html?page=15)
