# Restyle (`9320`) — musical transform scope

Status: **analysis + design scope, not code.** Author: Ottorino (style-and-
arrangement, dual axis). Read-only measurement of `components/arrangrr`,
`components/chorddet`, `components/midisrc`, `components/orchestrator`.
Written for Phase-5 Item A (`docs/design/phase5-execution-plan.md`, item #1),
in parallel with Corelli's placement/ABI review (item C). HOST-ONLY per the
plan; the dual-target no-heap core doctrine is unaffected by anything proposed
here (nothing below runs on the realtime device path).

Restyle ≠ Accompany. Accompany (`9310`, shipped) keeps the input's own melody
verbatim and lays the style's OWN band underneath the chords that melody
implies. Restyle must instead **transform the input's own part** — its
rhythm, its register, its voicing — into the target style's idiom, while
preserving *which notes/harmony it plays*, not its literal notes-as-authored.
That is a different operation on a different piece of data, and today's engine
does not do it anywhere; it does the Accompany operation only.

---

## 0 — What was measured (evidence, not memory)

### 0.1 The corpus is more differentiated than the stale premise assumes

A prior Ottorino reflection (`docs/backlog/style-differentiation-and-generation.md`,
now itself marked "status: future — most of the diagnosis is realized") measured
this corpus before `9100` (per-style feel) shipped. I re-measured the CURRENT
16 files at `components/arrangrr/include/arrangrr/arranger/styles/*.hpp`:

- `RolePolicy` is still binary — every file: `kFixed` (drums, 14–23 events/style)
  + `kChordTone` (everything pitched, 32–35 events/style). Confirmed unchanged
  (`style_model.hpp:46-49`).
- `NoteSource::kScaleDegree` = 35 events, `kInterval` = 14 events, across the
  WHOLE corpus (of ~3700+ note events) — an "uneven sprinkle," concentrated in
  country/disco/house/latin/motown/pop/reggae/rock/shuffle/swing/blues; ballad,
  basic, bossa(interval=0), funk, samba have **zero** melodic-source events.
- `ChordGesture` (strum/roll) = 22 total (`kStrumDown`=15, `kStrumUp`=5,
  `kRollUp`=2), concentrated in reggae (its skank chop) and samba (its
  cavaquinho stabs); most styles use none.
- `VoicingPolicy::kLead` (D41) is used on 0–19 comping patterns per style —
  chord1/pad/chord2 only, **never on bass**, confirming voice-leading is a
  comping-only concern today.
- **But the drum/perc/bass layer IS genuinely idiomatic**, contradicting a
  flat "same skeleton everywhere" read: `styles/reggae.hpp:46-49` is a real
  one-drop (kick+snare together on step 8 = beat 3) with an off-beat skank
  chop (`kIn2C`/`kAC`/`kBC`, steps 2/6/10/14, `ChordGesture::kStrumUp`, always
  an UPSTROKE per the file's own comment); `styles/samba.hpp:38-46` is a real
  surdo/tamborim/agogo pattern (syncopated kick on steps 4/12, continuous
  16th tamborim, agogo bell clave); `styles/blues.hpp:44-50` has a genuine
  blue-note harp line via `NoteSource::kInterval` (b3/b5/b7 against whichever
  of I/IV/V is live) and a boogie root-fifth-seventh bass
  (`kBoogieBass`, `blues.hpp:52`). **`9100` (per-style default `GrooveParams`
  + tempo) is marked done in `docs/strategy/roadmap-numbered.md:263`,** and
  three styles (swing/shuffle/blues) carry a real engine-driven swung feel
  (`blues.hpp:119`: `.groove={.swing=75,.accent=10,.swing_grid=8}`).
- **Net correction to the system's starting diagnosis:** "no genre-defining
  rhythmic signature, no characteristic basslines" is **no longer accurate**
  for drums/perc and for the FEEL layer (both now real and per-style); it
  **is still accurate** for bass FUNCTION (root/fifth/seventh only, zero
  `kInterval` walking/chromatic-approach bass anywhere) and for melodic
  variation (no generator; a `kScaleDegree`/`kInterval` line, where authored
  at all, is the same four notes every bar). This distinction matters for
  scoping Restyle: the ENGINE already knows how to render a genre's rhythm
  section idiomatically — the gap is specifically in transforming an
  ARBITRARY INPUT part into that idiom, which is a different problem from
  authoring one more built-in style well.

### 0.2 The resolution pipeline (D40) — what machinery exists to reuse

`components/arrangrr/include/arrangrr/arranger/arranger.hpp:300-361`, the
per-step fire loop, is exactly gather → `gesture::expand` → `resolve()` →
`m_voicing.voice()` → `groove::apply()` → schedule. Each stage is a small,
freestanding, no-heap, deterministic function — this is the reusable toolkit
for Restyle:

- **`resolve()`** (`arranger.hpp:402-431`) is the forward NTT kernel: given a
  `{tone, octave, NoteSource}` spec plus the live `Key`/`ChordState`, it
  produces an absolute pitch. `kChordTone` maps a tone-index through
  `theory::shape_of(chord.quality)`; `kInterval` adds a signed semitone
  offset to `chord.root_pc`; `kScaleDegree` adds `theory::degree_to_semitones`
  to `key.root_pc`. Each pitched role has a fixed anchor octave
  (`kRoleAnchor[]`, e.g. `kBass=36`, `arranger.hpp:445-448`).
- **`VoicingState::voice()`** (`voicing.hpp:65-98`) re-octaves ONLY the
  `chord_tone=true` members of a role's note group toward the nearest
  pitch to that role's last voicing (`nearest_octave`, `voicing.hpp:107-115`),
  retaining common tones and minimizing motion — genuinely voice-leading,
  not a random re-register. It is per-role memory, reset on style load
  (`arranger.hpp` `load_style`, calls `m_voicing.reset()`).
- **`groove::apply()`** (`groove.hpp:65-114`) is a deterministic,
  position-hashed (seed, tick, role, step) function producing a timing
  offset + velocity for ONE event, from `GrooveParams{swing, humanize_timing,
  humanize_velocity, accent, swing_grid, quantize, seed}`. Critically:
  **`quantize` (`groove.hpp:107-110`) only scales the offset THIS FUNCTION
  itself just computed (swing+humanize) back toward zero — it does not,
  and cannot as written, snap an arbitrary externally-timed tick onto the
  16th grid.** That "snap an arbitrary tick to the nearest grid slot (then
  optionally re-apply swing)" function does not exist anywhere in the graph
  today; it is small and cheap to add (integer round-to-`kTicksPerStep`,
  `timeline.hpp:20`, `kTicksPerStep = kPpqn/4 = 240`), and is squarely
  no-heap/dual-target-safe.
- **`gesture::expand()`** (`gesture.hpp:47-116`) only fans out a
  `kChordTone` event of a `kChordTone`-policy pattern — by construction it
  never touches `kFixed` (drum) or melodic (`kScaleDegree`/`kInterval`)
  events (`gesture.hpp:56-61`, explicit guard with a comment naming exactly
  why: it would "silently rewrite a melodic line into a chord"). This bounds
  what a first Restyle slice can safely reuse from D42: gestures are a
  comping-only tool, not a melody tool.

### 0.3 The Accompany pipeline — what "arrives" as the input part, verified

`components/orchestrator/include/orchestrator/accompany.hpp` composes
`Pipeline<MidiSourceStage<N>, ChorddetStage<kMaxPorts>, Engine>`. Reading
`components/midisrc/include/midisrc/midi_source_stage.hpp:122-133`
(`MidiSourceStage::on_tick`) is the load-bearing fact for Restyle's scope:

> the SMF is parsed ONCE at `load()` into a flat, tick-sorted
> `std::vector<SourceEvent>{Tick tick; MidiMessage msg;}`
> (`build_source_events`); `on_tick` walks a cursor and calls
> `m_scheduler.schedule(m_port, tick, msg)` **verbatim** — the ORIGINAL tick,
> the ORIGINAL note number, the ORIGINAL velocity — then separately
> `forward(...)`s the same raw wire bytes to chorddet.

So today the input part is **never touched** by NTT/gesture/voicing/groove at
all: it is a raw, tick-exact, register-exact MIDI replay ("melody-thru", the
header's own term) running in parallel with the arranger's generated band.
Chorddet (`components/chorddet/include/chorddet/stage.hpp:74-134`) watches
the SAME forwarded bytes through its OWN `ChordDetector`/held-note state and
publishes a `ChordState` via `FollowedContext` — that machinery already
exists and is proven (golden `tests/golden/accompany_melody_detect.golden`,
`test_ntt_resolution_follows_chord`, `components/arrangrr/tests/
test_arranger.cpp:235-253`); Restyle can and should reuse it for harmonic
context rather than re-detecting anything.

**What does NOT exist, confirmed by search:** a function that takes one
INPUT note (absolute pitch) + the live `ChordState`/`Key` and classifies it
back into an NTT tone-index (root/third/fifth/seventh, or "non-chord tone")
— the inverse of `resolve()`. `theory.hpp` has the primitives to build one
(`degree_of(key, pc)` → diatonic degree or -1 chromatic, `shape_of(quality)`
→ chord-tone semitone offsets, both `theory.hpp:105-129`/`132-158`), but the
classifier itself is new code. This is the one genuinely missing piece
between "Accompany's raw melody-thru" and "Restyle's transformed part."

---

## 1 — Dimensions safe for a FIRST SLICE (rhythm + register/voicing)

The plan's own hypothesis (`phase5-execution-plan.md` Item A: "start with
rhythmic re-quantization + register/voicing; defer melodic reharmonization")
is confirmed by the measurement above. Concretely, per dimension:

**(a) Rhythmic re-quantization — SAFE.** The style's own grid
(`kTicksPerStep=240`, `timeline.hpp:20`) and `GrooveParams` (`groove.hpp:21-29`)
are already a complete, deterministic, seed-reproducible model of "push this
grid position by this much." A first slice needs one new small function —
"round this arbitrary input tick to its nearest 16th-grid step" — then the
EXISTING `groove::apply()` re-applies the target style's own swing/humanize/
accent to that snapped step exactly as it already does for every authored
style event. No new state, no heap, both host and `arm-none-eabi` clean by
construction (it's arithmetic over `Tick`/`TickOffset`, same types the core
already uses everywhere). This is the highest-leverage, lowest-risk
transform: it is *the* thing that makes an input "feel like it's IN the
groove" rather than laid over it.

**(b) Register/voicing transfer — SAFE, reusing D41 wholesale.**
`VoicingState::voice()` (`voicing.hpp:65-98`) already does exactly "take a
role's chord-tone notes and re-octave them toward smooth continuity from the
previous voicing" — the SAME operation Restyle needs for "play the input's
harmony in the style's characteristic register," because a style's comping
role (Chord1/Chord2/Pad) already declares a fixed anchor octave
(`kRoleAnchor[]`) and, where authored `VoicingPolicy::kLead`, continuous
voice-leading. Feeding classified input chord-tone hits through the SAME
`VoicingState` the target style's own comping already uses is not new
machinery, just a new caller.

**Why these two, and not more, for a first slice:** both reuse EXISTING,
already-tested, already-golden-covered engine stages (`groove.hpp`,
`voicing.hpp`) with no change to their internals — only a new upstream
classifier feeding them input-derived `NoteReq`s instead of style-authored
ones. The risk surface is the new classifier, not the reused machinery.

---

## 2 — Dimensions to DEFER (melodic reharmonization) and why

**Melodic reharmonization** — changing a melody note's actual scale-degree/
chord-tone content to fit a NEW harmonic language (e.g. reinterpreting a
diatonic passing tone as a chromatic blue-note bend, or restacking a melody
line's implied chord quality) — must be deferred, for reasons the corpus
measurement makes concrete, not just cautious:

1. **No existing classifier to build on.** As measured in §0.3, there is no
   reverse-NTT (pitch → tone-index) function anywhere; a first slice would
   have to invent BOTH the classifier AND a reharmonization policy on top of
   it, compounding two unproven pieces at once.
2. **The corpus itself has almost no melodic-generation precedent to
   imitate.** 35 `kScaleDegree` + 14 `kInterval` events total across 16
   styles (§0.1); where a melodic line exists at all (reggae melodica,
   blues harp) it is one FIXED four-note authored phrase, not a generative
   rule. Reharmonizing an arbitrary input melody "in the style of reggae"
   has no in-corpus reference behavior to validate against — Nazzareno would
   be inventing genre melodic convention from nothing, which is exactly the
   generative-motif problem the plan already scoped SEPARATELY as `9210`
   (Item B, `docs/strategy/roadmap-numbered.md:283`, "Motif + transforms").
   Reharmonization and motif generation are the same open problem twice;
   solving it inside Restyle's first slice would silently duplicate `9210`'s
   scope before that item's own design pass.
3. **"Close but off" risk is real and specific, not generic caution.** A
   rhythmic snap that's a few ticks wrong reads as "slightly stiff" — still
   recognizably the same tune. A voicing re-octave that's off reads as "a
   different inversion" — still the same chord. A REHARMONIZED note that
   picks the wrong chord tone, or resolves a passing tone incorrectly against
   a chord change mid-phrase, reads as **a wrong note in a piece the listener
   already knows** — the single worst failure class this engine's entire NTT
   design (D24) exists to make structurally impossible for the arranger's OWN
   parts. Extending that same wrong-note risk to a transform of someone
   else's melody, without the classifier or a validated genre reharmonization
   rule, is the "musically wrong dressed as a feature" failure the Boundaries
   section names directly.
4. **The dependency-free/no-ML boundary already rules out the strong version.**
   `roadmap-numbered.md:505-506`: "ML style-transfer — stays OUT (`9300`
   scope): on-device infeasible, host-only would need a dependency flag."
   A rule-based reharmonizer is not banned the same way, but a CONVINCING one
   (i.e., one that doesn't just sound like wrong notes with extra steps) is a
   genuinely hard, ear-validated rule-authoring problem — not "small
   engineering," an open musicological design question of its own that
   deserves its own scoping pass, not a rider on this one.

**Recommendation:** ship (a)+(b) as the full first slice; treat
reharmonization as OUT OF SCOPE for `9320`'s first slice, explicitly
resumable later as either (i) a `9320` follow-on once a reverse-NTT
classifier exists and has been separately validated, or (ii) folded into
`9210`'s generative-motif design (same underlying problem, already a
separate roadmap item).

---

## 3 — Concrete first-slice transform definition

For each classified input note (pitch, tick, gate, velocity) arriving from
`MidiSourceStage`'s replay, BEFORE it reaches the raw scheduler:

1. **Classify against the live `ChordState`/`Key`** (chorddet already
   maintains both, reused as-is): compute `pc = pitch % 12`, `rel = (pc -
   chord.root_pc + 12) % 12`; if `rel` matches an offset in
   `theory::shape_of(chord.quality)`, the note is a CHORD TONE at that index
   (root/third/fifth/seventh); otherwise fall back to `theory::degree_of(key,
   pc)` — a diatonic degree if in-key, else "chromatic/passing," which the
   first slice PASSES THROUGH AT ITS ORIGINAL PITCH (no reharmonization, per
   §2) but still rhythmically requantized (per step 2 below). This
   classification is the one genuinely new piece of code (host-only, small,
   uses only existing `theory::` constexpr functions).
2. **Rhythmic re-quantization:** round the note's absolute tick to the
   nearest `kTicksPerStep` (240-tick, 16th-grid) slot, THEN run that slot
   through the TARGET style's own `groove::apply()` with the target style's
   `Style::groove` (the same per-style default `GrooveParams` `9100` already
   seeded for every built-in) — so an input note lands exactly where a style-
   authored event at that slot would, including the target style's own swing/
   accent/humanize. The note's GATE (duration) is preserved through the same
   snap (matching `groove::apply`'s existing "note-on and note-off share one
   offset, gate preserved" contract, `groove.hpp:44` comment).
3. **Register/voicing transfer, chord tones only:** for notes classified as
   chord tones in step 1, run them through the target style's own
   `VoicingState` for whichever comping role they're assigned (see below),
   under that role's authored `VoicingPolicy`. This moves the note by whole
   octaves toward the anchor register the target style's comping already
   lives in (`kRoleAnchor[]`) and toward continuity with the target style's
   own voicing memory — so an input melody that jumped two octaves for
   effect settles into the style's characteristic register instead. Non-
   chord-tone notes are NOT re-voiced (mirrors `VoicingState::voice()`'s own
   existing guard, `voicing.hpp:76-78`, "melodic/drum notes are never
   re-voiced" — this first slice deliberately keeps that invariant rather
   than inventing register rules for passing tones).
4. **Role assignment:** the input becomes ONE part on an existing role —
   `TrackRole::kLead` (index 8) is the natural first choice: it already
   receives a comparable "answering melodic line over the band" role in
   built-in styles (blues harp `kLeadLick`, reggae `kMelodica`), so its
   `gm_program`/anchor octave/idiom precedent already exists to borrow.
   `TrackRole::kPhrase` (index 7) is the alternative — used by ZERO built-in
   styles today (confirmed, `docs/backlog/style-differentiation-and-generation.md`
   line 73), i.e. a genuinely free slot with no borrowed idiom to collide
   with, at the cost of no existing register/voice precedent to anchor
   against. This choice is Corelli's placement call, not Ottorino's, but the
   musical tradeoff is: `kLead` inherits a plausible register/voice
   immediately; `kPhrase` is a clean slate that needs its own anchor/voicing
   defaults authored.
5. **What a musician recognizes as "played AS the style," concretely:** the
   SAME melodic shape (same chord tones hit in the same order, same passing
   tones), but (i) locked to the target style's grid and swung/accented like
   its native parts — a straight-eighths input played "as bossa" acquires
   bossa's 9110-seeded feel, not just a bossa band underneath it; (ii)
   settled into the target style's characteristic register/voicing rather
   than the input's own arbitrary octave choices — a chord-tone line that
   wandered two octaves for expressive effect now sits where that style's
   own comping sits. This is a genuinely different listening experience from
   Accompany (which changes NOTHING about the input) — the input part itself
   now sounds idiomatic, not just accompanied.

---

## 4 — Testing musical acceptability beyond golden byte-equality

The plan's gate discipline already requires a NEW or deliberately-regenerated
golden per feel-changing item (`phase5-execution-plan.md`, "Gate discipline").
Byte-identical goldens prove REPRODUCIBILITY (same seed ⇒ same output, D16) —
they do NOT prove the transform sounds right the first time it is authored.
Beyond ctest green, a musician's ear needs to check, concretely:

1. **Harmony preservation, by ear and by assertion.** Every transformed note
   must still land on the SAME functional tone it started as (root stays
   root, third stays third) even though its tick/register moved — this is
   assertable in a unit test (classify the OUTPUT note against the SAME
   `ChordState` and confirm the tone-index is unchanged from the input's
   classification), not just listened to. This is the automatable half of
   "did we keep the harmony."
2. **Groove authenticity, by ear against a REFERENCE, not just an internal
   golden.** "Requantized to bossa" must actually feel like `9100`'s bossa
   `GrooveParams`/tempo, which itself should be checked (as the per-style-
   feel-values proposal already does, `docs/proposals/per-style-feel-values.md`)
   against a real genre reference (the long:short ratio table there, e.g.
   swing=100/swing_grid=8 = an exact 2:1 triplet) — a musician listening for
   "does this actually swing like the genre" is checking the SAME swing-ratio
   math against ear-known genre conventions, not inventing a new check.
3. **No "stiff" artifacts from the snap.** A rhythmic re-quantization can
   produce clusters (two originally-distinct input notes collapsing onto the
   same grid slot) or unnaturally short/long gates after the snap — a
   musician's check here is "does anything sound truncated, doubled, or
   glued together" that a byte-golden alone would not surface as WRONG (it
   would just be a new, silently-accepted golden). Recommend an explicit
   listening pass on a HANDFUL of real, varied input melodies (not just the
   synthetic golden inputs) before the golden is authored/frozen, precisely
   because a golden only proves stability of whatever was first produced,
   never that the first production was RIGHT.
4. **Register-transfer naturalness across an OCTAVE JUMP.** `nearest_octave`
   (`voicing.hpp:107-115`) is a mechanical "closest register" rule; a
   musician's check is whether an input line that leaps by a tenth for
   expressive reasons still reads as one coherent phrase after re-voicing,
   or whether the octave-snap introduces an audible seam mid-phrase — a
   failure mode the existing `test_voicing_*` suite
   (`components/arrangrr/tests/test_voicing.cpp`) checks for AUTHORED
   chord-tone sequences, never for an arbitrary imported melody's larger,
   less-regular leaps.
5. **A real listening path, not just SMF bytes.** Confirmed by search: there
   is no committed SMF/audio EXPORT tool in this tree today (`midisrc`'s
   `smf.hpp` reads but does not write; no `fluidsynth`/audio-render tool is
   committed — `tests/integration/live_alsa.sh` is the closest, and it is
   excluded from ctest as flaky per the plan's own gate discipline). A
   musician verifying Restyle by EAR, not by golden diff, needs either (i)
   `hostrt::Shell`'s existing live MIDI-out path (already used interactively)
   pointed at a real synth, or (ii) a small new host-only SMF WRITER so a
   transformed take can be rendered and shared for review outside a live
   session — this is a real, currently-missing piece of test infrastructure,
   flagged here rather than assumed.

---

## Open forks / flags (owner or Corelli, not Ottorino's to decide)

1. **Role placement — `kLead` vs `kPhrase`** (§3.4). Musical tradeoff stated
   above; the ABI/data-shape call is Corelli's (item C of this same
   parallel-review batch).
2. **Non-chord-tone (passing/chromatic) handling in the first slice.** This
   scope keeps passing tones at their ORIGINAL pitch (only requantized, never
   re-voiced or reharmonized) — a deliberately conservative choice that
   avoids the reharmonization risk (§2) but means a first-slice "restyled"
   passing tone can still sit in an arbitrary, non-style-idiomatic register.
   Whether that residual arbitrariness is acceptable for a first slice, or
   whether passing tones should ALSO get a (harmony-preserving) octave-only
   register nudge, is a product-scope call, not an engineering one.
3. **A host-only SMF writer for ear-testing** (§4.5) does not exist and is
   not itself in the `9320` plan item — flagging it as test infrastructure
   this item will need, so it can be scoped (by Torquato or as a small
   preceding chore) rather than discovered mid-implementation.
4. **No new dependency is proposed anywhere in this scope** — the entire
   first slice (classifier + requantize + reused voicing) is host-only C++
   over existing `theory::`/`groove::`/`voicing.hpp` primitives. The one
   ADJACENT temptation (a statistical/ML groove or melody transfer) is
   explicitly named OUT per the prior reflection's Option C and the
   roadmap's own "`ML style-transfer` — stays OUT" line
   (`roadmap-numbered.md:505-506`) — not revisited here, only reaffirmed.
