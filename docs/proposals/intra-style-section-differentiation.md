# Intra-style section differentiation: measurement, the two structural causes,
# the endings audit, and the Wave-2.5 program

Ottorino, static corpus analysis (read-only on product code). Owner feedback
after Wave-1 (`59660fd`, "per-genre humanize + VarD motif on all 16 styles"):
*"gli stili variano pochissimo internamente"* — within a style, `kVarA..kVarD`
(and the intros) sound nearly identical, and a next-bar section switch feels
like nothing changed. This document quantifies that claim per style, verifies
what Wave-1 actually changed (and did not), answers the owner's two specific
questions (why sections are long-and-identical; why no style has an ending),
and lays out the Wave-2.5 differentiation program.

This supersedes nothing in `docs/proposals/style-section-depth-and-length.md`
(pre-Wave-1; already the ground truth for the 1-bar/`kDefaultSceneBars`
mechanism and the then-unwired motif engine) — it re-measures the corpus
**after** Wave-1 landed, adds the intra-style similarity metrics that doc did
not compute, and adds a full endings audit that doc did not do.

Corpus measured: `components/core/arrangrr/include/arrangrr/arranger/styles/*.hpp`
(16 files, 2965 lines total). Method: a purpose-built parser
(`/tmp/.../scratchpad/parse_styles.py`, run against the real headers, not
memory) extracts every `StyleEvent[]`/`StylePattern[]`/`StyleSection[]`
constexpr array into a role→section model, then computes per-role Jaccard
overlap on `(step, tone, octave)` keys, mean velocity distance over shared
steps, event-count delta, and role-set (instrumentation) delta for every
`VarA..VarD` pair and `Intro1 vs Var*`. No cmake/ctest was run (build dirs are
in use elsewhere, per instruction); this is header-text analysis only.

---

## 1. What I measured

### 1.1 Structural fact, corpus-wide: every section is 1 bar

```
grep -o '\.bars=[0-9]*' <every styles/*.hpp> | sort | uniq -c
```
returns **`.bars=1`, 100% of occurrences, in all 16 files** (12–13 sections
per file, all `.bars=1`; zero instances of any other value). `StyleSection`
(`components/core/arrangrr/include/arrangrr/arranger/style_model.hpp:169-173`)
has a real `std::uint8_t bars` field the engine already honors
(`arranger.hpp:444`, `len = section->bars * ticks_per_bar`) — nothing stops a
multi-bar section today except that no style author has ever set `bars > 1`.
This is unchanged since the pre-Wave-1 doc; confirmed again here because it is
the direct cause of issue (c) below.

### 1.2 Intra-style similarity, `VarA..VarD`, per role (the new measurement)

Mean pairwise Jaccard similarity of the `(step, tone, octave)` event-set,
averaged over all 6 `VarA/B/C/D` pairs, for the three roles present in every
style (Drums = 100% `kFixed`, never transposed; Bass and Chord1 =
`kChordTone`):

| style   | drums avg J | bass avg J | chord1 avg J |
|---------|:-----------:|:----------:|:------------:|
| basic   | 0.130 | 0.037 | 0.062 |
| rock    | 0.297 | 0.258 | 0.606 |
| reggae  | 0.338 | 1.000 | 0.794 |
| samba   | 0.456 | 1.000 | 0.423 |
| country | 0.483 | 0.667 | 0.250 |
| shuffle | 0.494 | 1.000 | 0.488 |
| funk    | 0.516 | 0.445 | 0.284 |
| ballad  | 0.511 | 0.381 | 0.287 |
| house   | 0.544 | 0.524 | 0.714 |
| latin   | 0.522 | 1.000 | 0.450 |
| blues   | 0.557 | 1.000 | 0.425 |
| motown  | 0.580 | 1.000 | 0.430 |
| pop     | 0.571 | 0.503 | 0.631 |
| bossa   | 0.607 | 0.778 | 0.374 |
| disco   | 0.611 | 0.637 | 0.631 |
| swing   | 0.644 | 1.000 | 0.384 |

Jaccard = 1.0 means "literally the same onset set on every VarA/B/C/D pair" —
i.e. zero authored rhythmic difference between variations for that role.
Jaccard near 0 (e.g. `basic` drums 0.130) means the four variations are
substantially different hand-written ideas.

**Reading it**: `basic` (the neutral scaffold style) is, ironically, the LEAST
self-similar style in the corpus on every axis — its `kVarADrums`..`kVarDDrums`
are four genuinely distinct patterns (verified by reading
`components/core/arrangrr/include/arrangrr/arranger/styles/basic.hpp:81-259`
directly: different note values, not just velocity nudges). `swing`, `motown`,
`samba`, `reggae`, `shuffle`, `latin`, `blues` cluster at the flat end for
**bass**: exactly `1.000`, meaning one bass array is reused byte-identically
across all four variations.

### 1.3 Bass reuse: 7 of 16 styles share ONE bass array across all of VarA–D

```python
# confirmed by checking events_ref for kBass in VarA..VarD per style
```
| style   | shared bass array | reused in VarA/B/C/D |
|---------|--------------------|:---------------------:|
| blues   | `kBoogieBass`      | all 4 |
| latin   | `kTumbao`          | all 4 |
| motown  | `kSoulBass`        | all 4 |
| reggae  | `kRootBass`        | all 4 |
| samba   | `kSyncBass`        | all 4 |
| shuffle | `kShufBass`        | all 4 |
| swing   | `kWalkBass`        | all 4 |

This matches and reconfirms the pre-Wave-1 finding
(`docs/proposals/style-section-depth-and-length.md` §1.4, the `blues.hpp`
`kBoogieBass`×8 example) and the same measurement in
`docs/reflections/phase7-scope-9210-9320-antisameness.md` §2 — Wave-1 wired
`MotifSpec` onto these bass roles in most (not all) of these styles, but the
**authored base array is still one idea reused verbatim**; any variation
between VarA and VarB today comes ONLY from the motif transform's
call-and-response parity (see §1.5), never from the base data.

### 1.4 Worst single-pair drums offenders: `VarB` vs `VarD` is near-identical in 5 styles

Ranked by the single most-similar `VarX–VarY` drums pair per style:

| style   | worst pair | drums Jaccard | vel dist (mean) | event-count delta |
|---------|:----------:|:--------------:|:----------------:|:------------------:|
| **latin**   | VarB–VarD | **1.000** | **0** (byte-identical) | 0 |
| **blues**   | VarB–VarD | **1.000** | 2 | 0 |
| **shuffle** | VarB–VarD | **1.000** | 1 | 0 |
| **bossa**   | VarB–VarD | **1.000** | 0 | 0 |
| **funk**    | VarB–VarD | **1.000** | 0 | 0 |
| motown  | VarB–VarD | 0.957 | – | 1 |
| country | VarB–VarD | 0.955 | – | 1 |
| samba   | VarB–VarD | 0.929 | – | 2 |
| reggae  | VarB–VarD | 0.867 | – | 2 |
| swing   | VarA–VarC | 0.800 | – | 2 |
| house   | VarA–VarB | 0.783 | – | 5 |
| disco   | VarA–VarB | 0.778 | – | 4 |
| ballad  | VarC–VarD | 0.714 | – | 0 |
| pop     | VarB–VarD | 0.696 | – | 7 |
| rock    | VarB–VarD | 0.647 | – | 2 |
| basic   | VarA–VarB | 0.571 | – | 9 |

Two concrete, load-bearing examples, read directly from the header text:

- **`latin.hpp`** — `kBD` (VarB drums, line 109) and `kDD` (VarD drums, line
  137) are a **literal copy-paste**: same 20 `{step, tone, octave, vel, gate}`
  tuples in the same order, including velocities (`88, 68, 78, 68, 88, 68,
  78, 68, 72, 76, 70, 78, 74, 76, 70, 80, 70, 64, 70, 66` — identical on both
  sides). `kDP` (the VarD pattern) carries **no motif** on the drums role at
  all (`patterns_by_array['kDP'][drums].motif == None`). Result: VarB and
  VarD in `latin` are audibly the SAME drum bar, full stop — no engine
  mechanism differentiates them.
- **`blues.hpp`** — `kBD` (line 83) and `kDD` (line 110) are the same 14-event
  onset grid with a **uniform +2 velocity bump on every single event** (92→94,
  76→78, 90→92, 98→100, 100→102, 60→62) and nothing else authored differently.
  `kDP` does carry `.motif=&kPeakDrumsMotif` (`kDisplacement`), so live
  playback differs — but only on repeat parity within an 8-bar scene hold
  (§1.5); the moment you switch from VarB to VarD, what actually changes is
  "+2 velocity everywhere," which is inaudible.

### 1.5 Verifying the owner's suspicion: Wave-1 humanize/motif do NOT differentiate sections

**Humanize is a single style-wide value, not per-section — confirmed
structurally, not just observationally.** `GrooveParams`
(`components/core/arrangrr/include/arrangrr/arranger/groove.hpp:21-29`) is
declared with the comment *"One global groove feel applied to every arranger
part"* (line 19), and `Style` (`style_model.hpp:175-184`) carries exactly one
`GrooveParams groove{}` member. There is no per-`StyleSection` or
per-`StylePattern` groove override anywhere in the type. `rock.hpp:304`'s
`.groove={.humanize_timing=8, .humanize_velocity=18}` (and every other
style's equivalent) applies **identically to Intro1, every Var, every Fill,
Break, and both Endings** — Wave-1 gave every style a genre-appropriate wobble
amount, but that wobble is the same number wherever the transport currently
is. It cannot be the source of "VarB sounds different from VarD," because it
literally cannot vary between them.

**Motif is real, but its coverage is heavily VarD-weighted, thin at VarC, and
near-zero everywhere else** — counted directly (role-pattern entries carrying
a non-null `.motif=` pointer, out of all role-patterns in that section type,
summed across all 16 files):

| section  | motif-wired / total role-patterns | % |
|----------|:----------------------------------:|:---:|
| Intro1   | 0 / 34    | 0% |
| Intro2   | 10 / 65   | 15% |
| VarA     | 15 / 81   | 19% |
| VarB     | 19 / 95   | 20% |
| VarC     | 35 / 100  | 35% |
| **VarD** | **87 / 127** | **69%** |
| FillA–C  | 14 / 34 each | 41% |
| FillD    | 14 / 47   | 30% |
| Break    | 0 / 19    | 0% |
| Ending1  | 2 / 64    | 3% |
| Ending2  | 2 / 64    | 3% |

This confirms the owner's diagnosis precisely: the motif engine's
call-and-response variety is concentrated almost entirely on the "peak"
variation (VarD, 69%) via the Wave-1 "wire the most-heard instance" pass
(`rock.hpp:72-81`'s own comment names this granularity choice explicitly), a
secondary bleed into VarC only because several styles reuse the SAME
authored array + motif pointer for both VarC and VarD (e.g. `rock.hpp`'s
`kPadRehit`/`kSharedPadMotif` wired into both `kVarCPatterns` and
`kVarDPatterns`, lines 255 and 263). VarA, VarB, both Intros, Break, and both
Endings are essentially motif-silent.

There is also a real, previously-flagged design characteristic that limits
motif's cross-section payoff even where it IS wired: `m_motif_repeat`
(`arranger.hpp:863`) is **one global counter for the whole arranger**, reset
only on style load / transport start (`arranger.hpp:88,363`), and incremented
**only when a plain variation loops back onto itself** (`arranger.hpp:480`,
`section_end` with `next == m_current`) — moving `VarA→VarB` never advances
it. `docs/reflections/phase7-scope-9210-9320-antisameness.md` §1/Fork C
already named this precisely ("moving from VarA→VarB does not advance it...
This is a real scope characteristic, not a bug"). Net effect: even on the one
variation that IS motif-wired, the call-and-response state a listener hears
on arrival depends on how many prior same-section repeats happened
song-wide, not on which variation they just switched to — so a fresh switch
into VarD does not reliably sound different from the last time VarD played.

---

## 2. Diagnosis, per axis (model gap vs underuse)

- **Rhythm/instrumentation**: role-set genuinely ramps up VarA→VarD in **all
  16 styles** (`same_roles_all_vars=False` universally; typical shape 5 roles
  in VarA → 7–8 roles by VarD, e.g. `rock`: Drums/Bass/Chord1/Pad/Chord2 (5) →
  +Arp/+Lead/+Perc (8) by VarD). This is a genuine, already-shipped strength
  — **not flat**, and worth citing as a positive baseline, not just a
  complaint target.
- **Drums** (backbone role): flat in 5/16 styles specifically at the
  `VarB↔VarD` seam (§1.4) — **UNDERUSE**, not a model gap. `StyleEvent` and
  `StylePattern` already support authoring two genuinely different onset
  grids; nobody did for these 5.
- **Bass**: flat (Jaccard 1.0, one array) in 7/16 styles across ALL of
  VarA–D — **UNDERUSE** for the "author a second idea" fix (Option 1,
  already the 9210-recommended slice); a partial **MODEL LIMIT** for going
  further, because `MotifTransform` only offers 3 shapes
  (`kDiatonicTranspose`/`kRetrograde`/`kDisplacement`,
  `style_model.hpp:112-117`) — none of which is "walking bass" or "chromatic
  approach tone," the two idioms `docs/reflections/phase7-scope-9210-9320-
  antisameness.md` §2 already named as still entirely absent from the corpus
  (bass FUNCTION, not just bass pattern, is unrepresented).
- **Groove/feel across sections**: a **hard MODEL GAP**. `GrooveParams` is a
  single per-`Style` field (§1.5); there is no data path for "VarD swings
  harder than VarA" today. Closing this needs a new field (a per-section or
  per-pattern `GrooveParams` override, or a delta applied by section
  "energy tier") — real engine/model work, not just authorship.
- **Melodic/lead content**: unchanged from the prior reflection's finding —
  still one hand-typed phrase per style where it exists at all (`kScaleDegree`
  + `kInterval` events), now sometimes motif-varied on VarD only.

---

## 3. Issue (c) — "le sezioni sono molto lunghe e identiche al loro interno, come mai?"

Two compounding, independently-verified causes:

**(i) Every built-in section is 1 bar** (§1.1). A "section" in the corpus
today is never more than a single 16-step bar of authored content — there is
no per-bar-2, per-bar-3 distinct material anywhere in the 16 built-ins. This
is purely an authoring gap: `StyleSection::bars`
(`style_model.hpp:171`) already accepts any value, and the engine already
loops correctly across `bars > 1` (`arranger.hpp:444-490`, the "mid-section
bar boundary must not reset `pos`" comment proves multi-bar sections are a
tested, working path — see also
`docs/proposals/style-section-depth-and-length.md` §1.3, which cites the
regression proving it). A parallel Wave-2 C workstream is now authoring real
multi-bar content on top of this already-working mechanism.

**(ii) The GUI holds a scene for `kDefaultSceneBars = 8` bars before
advancing** — `apps/gui-sonotron/src/grid_model.hpp:147` (`static constexpr
int kDefaultSceneBars = 8;`), applied per-scene
(`apps/gui-sonotron/src/grid_model.cpp:85-89`, `scene_bars()` returns the
per-scene override or this default) and consumed by both the auto-song
advance logic and the grid panel's UI
(`apps/gui-sonotron/src/grid_panel.cpp:471`, `868`). **The two causes
compound multiplicatively**: a 1-bar section (i) loops **8 times identically**
(ii) before the GUI ever lets the transport move to the next scene/section —
so today's actual listening unit is not "1 bar of VarA," it is "the same 1
bar of VarA, 8 times in a row," which is exactly the "very long and identical
internally" symptom the owner named.

**The fix is already two-thirds staged, and the GUI half already exists as a
tool, just needs pointing at real content**:
1. Multi-bar sections (Wave-2 C, in flight) turns the *authored* unit from 1
   bar into N real, distinct bars — directly attacks (i).
2. A per-bar variation inside that multi-bar section (density ramp,
   turnaround on the last bar, a distinct bar-2 fill-in) makes even a single
   8-bar scene hold contain real internal movement instead of one bar on
   repeat — this is exactly what Wave-2.5 (§5) should target, and it composes
   cleanly with (1) because a "bar 2 turnaround" is orthogonal to a
   "VarA→VarD density ramp": one varies content *within* a section's own
   bars, the other varies content *across* sections.
3. The per-scene length editor is **not hypothetical** — `GridModel` already
   exposes `scene_bars()`/`set_scene_bars()` as a real, tested API
   (`grid_model.hpp:148-149`, exercised directly in
   `apps/gui-sonotron/tests/test_grid_panel_auto_song_real_backend.cpp:119`,
   `model.set_scene_bars(0, 1)`), so shortening the demo/default hold (e.g.
   from 8 bars down to 2–4 once sections are genuinely multi-bar) is a
   product configuration change, not new engineering — but it should wait
   until (1)/(2) land, or a shorter hold on still-1-bar, still-flat content
   just makes the flatness cycle faster, not better.

---

## 4. Issue (d) — "nessuno stile ha ending, come mai?" — AUDIT AND VERDICT

**Verdict: this premise is false for the corpus. All 16 built-in styles
define BOTH `Ending1` and `Ending2`, with real, distinct content.** The gap is
entirely on the GUI side, and even there it is a demo-content choice, not a
missing mechanism.

### 4.1 Corpus coverage matrix (grepped `SectionType::k*` per file)

| style   | Intro1 | Intro2 | VarA | VarB | VarC | VarD | FillA | FillB | FillC | FillD | Break | **Ending1** | **Ending2** |
|---------|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| ballad  | X | X | X | X | X | X | X | X | X | X | . | **X** | **X** |
| basic   | X | X | X | X | X | X | X | X | X | X | . | **X** | **X** |
| blues   | X | X | X | X | X | X | X | X | X | X | X | **X** | **X** |
| bossa   | X | X | X | X | X | X | X | X | X | X | . | **X** | **X** |
| country | X | X | X | X | X | X | X | X | X | X | . | **X** | **X** |
| disco   | X | X | X | X | X | X | X | X | X | X | X | **X** | **X** |
| funk    | X | X | X | X | X | X | X | X | X | X | X | **X** | **X** |
| house   | X | X | X | X | X | X | X | X | X | X | . | **X** | **X** |
| latin   | X | X | X | X | X | X | X | X | X | X | X | **X** | **X** |
| motown  | X | X | X | X | X | X | X | X | X | X | X | **X** | **X** |
| pop     | X | X | X | X | X | X | X | X | X | X | . | **X** | **X** |
| reggae  | X | X | X | X | X | X | X | X | X | X | . | **X** | **X** |
| rock    | X | X | X | X | X | X | X | X | X | X | X | **X** | **X** |
| samba   | X | X | X | X | X | X | X | X | X | X | . | **X** | **X** |
| shuffle | X | X | X | X | X | X | X | X | X | X | . | **X** | **X** |
| swing   | X | X | X | X | X | X | X | X | X | X | . | **X** | **X** |

16/16 define Ending1 + Ending2 (100%). Only 6/16 (`blues`, `disco`, `funk`,
`latin`, `motown`, `rock`) define `Break`. Both endings carry real, distinct
authored content — e.g. `rock.hpp` `kEndPatterns` (lines 175-191, a crash +
held power chord + `kPad7`) vs `kEnd2Patterns` (lines 236-248, a bigger
crash-kick-snare-crash cadence, a root-fifth-octave bass stack, a fuller
held chord) — these are not stubs.

### 4.2 The engine's ending behavior is real and correctly wired

`section_is_ending()` (`arranger.hpp:44`) gates a real `stop_transport`
signal (`arranger.hpp:472-474`) fired when a section landing in
`kEnding1`/`kEnding2` reaches its own bar length. The host engine reacts to
it correctly: `components/core/arrangrr/include/arrangrr/engine.hpp:829-838`
stops the transport, releases any held chord notes, emits MIDI Stop, and
publishes a transport-state `OutEvent` to the host. This is not a stub either
— it is a complete, already-correct behavior. **The corpus and the core
engine are not the gap.**

### 4.3 The actual gap: the GUI never lands on an ending, by content choice, not by missing mechanism

`apps/gui-sonotron/src/grid_panel.cpp:156-220` (`seed_demo()`) seeds the 5
default demo scene columns as `Intro1, VarA, VarB, VarC, VarD` only
(`kDemoSections`, lines 207-213) — **no ending column is ever seeded by
default.** This is the literal source of "nessuno stile ha ending" as
experienced by anyone opening the app: the demo never shows one.

However, **a real mechanism to reach an ending already exists and is wired
end to end** — it is just not surfaced as "the ending":
- `apps/gui-sonotron/src/browser_panel.cpp:26-44` — the "variations" browser
  panel's draggable row list includes `"outro"` mapped to
  `kVariationSections[7] = 11 // outro -> kEnding1` (comment, line 43).
- `apps/gui-sonotron/src/grid_panel.cpp:571-576` — a scene-header drop
  target (`kVariationDragPayloadId`) accepts that drag and calls
  `model.set_scene_section(...)`, genuinely changing that column's
  `SectionType` to `kEnding1`.
- The engine-side behavior (§4.2) then correctly stops transport when that
  column is reached.

So the answer to issue (d) precisely: **endings are missing from neither the
corpus (100% coverage, real content) nor the engine (correct, tested
`stop_transport` wiring) — they are missing from the GUI's demo/default
content, and the one real user-facing path to reach one ("drag the 'outro'
row onto a column header") is unlabeled as an ending/terminator and easy to
never discover.** One thing this audit could NOT verify without a live build
(out of scope per instruction — no cmake/ctest): whether `fx.playing`/the
GUI's own transport-state flag is actually updated on receiving the engine's
`OutEvent::transport(...)` after a `stop_transport`, or whether auto-song's
purely bar-count-driven advance timer (`update_auto_song`,
`grid_panel.cpp:436-486`, no `stop_transport`/`OutEvent::transport` read
found anywhere in `grid_panel.cpp`/`main.cpp`) would silently re-launch the
next scene on the following bar regardless of the engine having stopped.
This is a real open question, flagged, not asserted either way.

### 4.4 Concrete proposal

1. **Add a 6th default demo column mapped to `Ending1`**, the same one-line
   change shape as `kDemoSections` (`grid_panel.cpp:207-213`) already uses
   for the other 5 — cheapest possible fix, makes every fresh launch audibly
   prove endings exist. **SHIPPABLE**, GUI-only, no core change.
2. **Relabel/promote the existing "outro" drag row** as an explicit "ending"
   affordance (e.g. a distinct icon/color in `browser_panel.cpp`'s
   `kVariations` list, or a dedicated "set ending" button on the scene
   header context menu) so the mechanism that already works is discoverable.
   **SHIPPABLE**, GUI-only.
3. **Verify (and, if needed, fix) that auto-song respects `stop_transport`**
   — read `OutEvent::transport`/the stop signal in `update_auto_song` and
   suppress the next scheduled advance when the engine has genuinely
   stopped, rather than relying on bar-count alone. This is the one item in
   this section that is a real code question, not just a content/labeling
   one — flagged for whoever picks this up to confirm against a live build
   (Giotto/Torquato territory, not a static-analysis verdict I can make
   honestly from headers alone).
4. **`Ending2` is currently reachable only manually** (no `kVariationSections`
   entry maps to it, `browser_panel.cpp:35-44` only has one `"outro"` row
   for `kEnding1`) — if a "big ending vs quiet ending" choice is wanted in
   the GUI, add a second draggable row. Small, same shape as (2).

---

## 5. Wave-2.5 differentiation program

Constraints respected in every item below: freestanding fixed
`constexpr` arrays (no heap, D32/D33), `StyleEvent` stays pinned at 10 bytes
(`style_model.hpp:105`, `static_assert`), no single 16th-grid step may exceed
`kMaxVoiceNotes = 16` simultaneous notes after gesture expansion
(`arranger.hpp:684`), and any `motif::generate`-driven (Option 2) lane is
capped at `kMaxMotifLen = 16` onsets per bar (`motif.hpp:43`) — fine for
one-bar sections today, a real limit to track once Wave-2 C multi-bar
sections need a *generated* (not authored) motif spanning more than one bar.
Ordered by payoff/effort; each composes with Wave-2 C's per-bar work rather
than competing with it (a VarA→VarD density ramp is a *cross-section* axis;
Wave-2 C's per-bar turnaround is a *within-section* axis — orthogonal by
construction).

1. **(Highest payoff/effort) Re-author the 5 `VarB≈VarD` drums duplicates**
   (`latin`, `blues`, `bossa`, `funk`, `shuffle`, §1.4) so VarD's onset grid is
   genuinely denser/different, not a velocity-only or byte-identical copy of
   VarB — e.g. add the crash/extra 16th-note push already present in every
   OTHER style's VarD (`rock.hpp:259`, `samba.hpp:102` both show a real
   VarD-specific drum idea for comparison). Pure data edit, zero engine
   change, directly answers the owner's exact complaint on the 5 worst
   offenders. **SHIPPABLE now.**
2. **Give the 7 single-bass-array styles (`blues/latin/motown/reggae/samba/
   shuffle/swing`, §1.3) a second hand-authored bass idea for the VarC/D
   tier**, distinct from the VarA/B idea (not just a motif transform of the
   same notes) — e.g. `latin`'s VarA/B keep `kTumbao` as-is, VarC/D get a
   tumbao variant with a passing chromatic approach tone on beat 4 (an
   `NoteSource::kInterval` event, already-proven vocabulary, `rock.hpp:64`).
   This is exactly 9210's own recommended "Option 1" first slice
   (`phase7-scope-9210-9320-antisameness.md` §3, Fork A) applied to the
   7 flat styles it did not yet reach. **SHIPPABLE now**, no new engine code.
3. **Vary the motif transform/seed per Var-tier instead of one shared
   pointer across VarA–D** (currently e.g. `samba.hpp`'s `kSyncBassMotif` is
   the SAME `MotifSpec` wired into VarA/B/C/D+FillA-D alike, §1.5) — give
   VarA/B a milder transform (or `kNone`) and VarC/D a stronger one
   (`kDisplacement` + a bigger `amount`, or `kRetrograde`), so a section
   switch itself carries a felt change in addition to the existing
   within-section call-and-response. Pure style-data edit (new `MotifSpec`
   constants, still zero-cost `constexpr`). **SHIPPABLE now.**
4. **Density ramp A→D as an explicit authoring checklist per style**: most
   styles already ramp instrumentation (§2, "role-set genuinely ramps up") —
   make the *event count* ramp as deliberately as the *role count* does
   (`pop`'s 7-event drums delta on its worst pair is a good sign already;
   `basic`'s 9-event delta likewise). For any style whose VarA→VarD role-set
   is already full (i.e. no new role after VarB), add one clearly audible
   density/register move instead (an octave push, a new stab subdivision) so
   VarC/D still reads as "more" even without a new instrument. **SHIPPABLE**,
   authoring-only; budget check per edit against `kMaxVoiceNotes=16`.
5. **(Real model work, not this wave) A per-section/per-pattern groove
   override** — the only item here that is NOT a data-only fix (§2, "hard
   MODEL GAP"). Needed to let VarD swing/humanize harder than VarA the way
   real arrangers do (a "peak variation pushes the time" idiom). Requires a
   new field on `StyleSection` or `StylePattern` (a `GrooveParams` delta or
   override) plus `Arranger::on_tick` reading it instead of the single
   style-wide `m_groove`. Flagging this explicitly as **INSTRUCTIVE-BUT-
   INFEASIBLE for Wave-2.5** (it is the right musical idea, but it is new
   engine/ABI surface, not a Wave-2.5-shaped data edit) — a candidate for its
   own scoped follow-up, not something to slip into this pass.

Everything in items 1–4 is pure `constexpr` style-table authorship: no new
dependency, no heap, no change to `arranger.hpp`/`motif.hpp`/`style_model.hpp`,
and (per the owner's own instruction) goldens will move — that sign-off is
the owner's, not mine to grant here.

---

## 6. What needs deciding / what I flag

- **Item 5 above (per-section groove)** is a real model-gap fork the owner
  should decide to schedule or not; I have not sized it beyond "new field +
  new read path," which is a genuinely separate, smaller-than-9210-sized
  scoping pass.
- **§4.4 item 3** (auto-song's handling of `stop_transport`) is a real
  functional question I could not verify statically — it needs a live-build
  check (Giotto/Torquato), not a static-analysis verdict.
- **The `MotifTransform` vocabulary gap** (§2: no "walking bass"/"chromatic
  approach" transform exists) is a real, already-once-flagged model
  limitation (`phase7-scope-9210-9320-antisameness.md` §2) that Wave-2.5's
  item 2 works around with hand-authored data rather than closing — closing
  it for real is a separate, larger 9210/9220-shaped decision the owner
  already has open forks on, not something to fold into this program.

Files referenced (all read, all absolute paths):
- `components/core/arrangrr/include/arrangrr/arranger/styles/*.hpp` (all 16)
- `components/core/arrangrr/include/arrangrr/arranger/style_model.hpp`
- `components/core/arrangrr/include/arrangrr/arranger/arranger.hpp`
- `components/core/arrangrr/include/arrangrr/arranger/groove.hpp`
- `components/core/arrangrr/include/arrangrr/arranger/motif.hpp`
- `components/core/arrangrr/include/arrangrr/engine.hpp`
- `apps/gui-sonotron/src/grid_model.hpp`, `grid_model.cpp`
- `apps/gui-sonotron/src/grid_panel.cpp`
- `apps/gui-sonotron/src/browser_panel.cpp`
- `apps/gui-sonotron/src/in_process_brain_session.cpp`, `main.cpp`
- `docs/proposals/style-section-depth-and-length.md` (prior Ottorino pass)
- `docs/reflections/phase7-scope-9210-9320-antisameness.md` (prior Ottorino pass)
