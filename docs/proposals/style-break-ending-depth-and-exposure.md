# Break and ending: corpus depth audit, CLI-vs-GUI sequencing comparison, and a spec for later work

Status: analysis + spec, not implementation. Written by Ottorino (style analyst) in
answer to the owner's question: "sonotron's styles don't feel like they have break
and ending — did we look at how the old CLI arranger handled them?"

Scope: `SectionType::kBreak`/`kEnding1`/`kEnding2` only (style_model.hpp:31-33).
Companion reading: `docs/proposals/style-section-depth-and-length.md` (general
1-bar-section thinness, written before the Wave-2 style-depth pass that grew
`Intro1` to 2 bars in all 16 styles — that doc's §1.1 claim "every section is
authored as exactly 1 bar" is now stale for `Intro1`; it never specifically
measured break/ending, which is this doc's whole subject).

## 0. Correcting the starting premise

The task brief that seeded this analysis stated kBreak is authored in "~7 styles
(blues, disco, funk, latin, motown, rock)" — that list is 6 names, and measurement
confirms it is exactly **6/16**, not 7, and **`basic` is also missing `kBreak`**
(the brief's "missing" list omitted it). Verified two ways: a direct grep count
(`grep -lc "SectionType::kBreak" styles/*.hpp` → 6 files) and a full parse of every
style's `kSections[]` table. So: **10/16 styles have no break at all** — ballad,
basic, bossa, country, house, pop, reggae, samba, shuffle, swing.

The brief also asserted "the GUI only wires ending1 via an outro-drag + END pad, no
first-class break placement" — this is **wrong for break**, confirmed by reading
`apps/gui-sonotron/src/browser_panel.cpp:53-79`: the Repeat-Zone drag palette's
`kVariations` row set already includes `"break" -> SectionType::kBreak (10)` as a
real drag source, index-parallel with `kVariationSections` and
`kVariationWireNames`. The real, narrower gap is **`ending2`** (and, less centrally
to the owner's question, `intro2`/`fillB`/`fillC`/`fillD`) — see §3 below.

## 1. Diagnosis (one paragraph)

The corpus does not lack break/ending sections as ABI concepts — 15/16 styles
author both endings, and the drop target for placing them is already
unrestricted — it lacks **musical difference between having a break/ending and
not having one, and between ending1 and ending2**. Every authored ending across
all 16 styles is the *same* one-bar, one-hit gesture: Drums (kick + crash, gate
`kGateHalfBar`/`kGateHeld`), Bass (a single held root), Chord1 (held
root-third-fifth[-seventh]), Pad (held root-third-fifth-seventh) — all events at
`step=0` (occasionally one extra hit at `step=8`), zero `ChordGesture`, zero
non-default `NoteSource`, and a chord that is whatever the *live* chord happens to
be (there is no cadential resolution, because that is architecturally a
harmony/progression concern, not a style-data concern — see §2.2). Ending2 is not
a musically distinct gesture from ending1; it is ending1 plus one extra drum hit
and a doubled bass octave — a volume/density bump, not a different idea. Breaks,
where authored, are genuinely better (a real "stop" idiom: accented downbeat,
silence, snare/tom pickup into the next bar) but two of the six (blues, funk) are
near-byte-identical to each other, differing only by a kick velocity value. So the
"no break and ending" feeling is real and is a corpus-authoring gap, not a missing
mechanism: the engine (`Arranger::on_tick`, `arranger.hpp:544-554`) already has a
correct, tested one-shot stop rule for endings, and the CLI's `style section
break|ending1|ending2` verbs (`shell_parse.cpp:256-258`) run over the exact same
core the GUI does — there is no CLI-only capability the GUI lacks at the section-
firing level. What the CLI path never had either, and what nobody has authored
anywhere in the tree, is *musically differentiated* break/ending content per genre.

## 2. Per-section-type depth table

Measured by parsing every `StyleSection`/`StylePattern`/`StyleEvent` table behind
`kBreak`/`kEnding1`/`kEnding2` in all 16
`components/core/arrangrr/include/arrangrr/arranger/styles/*.hpp` files (script-
parsed, cross-checked by hand against `rock.hpp:248-366` and
`blues.hpp:144-190`). Aggregate counts across the corpus (147 role-instances, 465
events total, in break+ending sections only): **0 events use `ChordGesture`, 0
events use non-default `NoteSource`** (`kScaleDegree`/`kInterval`), 4/147 role-
instances use the `MotifSpec` engine (all Pad, in blues/motown-family styles), and
32/147 carry a `gm_program` override with `VoicingPolicy::kLead` (almost entirely
the Pad role, which is the one role in these sections that gets a real voice-
leading treatment).

### kBreak — 6/16 styles (blues, disco, funk, latin, motown, rock)

| Style | Drums events | Bass | Chord1 | Perc | Verdict |
|---|---|---|---|---|---|
| rock.hpp:354-366 | 6 (kick+crash stab, then snare×2/tomMid/tomLow pickup on steps 12-15) | 1 (root pop, stab) | 3 (root-fifth-root8, stab) | — | Real "stop" idiom: accented downbeat, silence, pickup. Comment at rock.hpp:351-353 names it explicitly ("varBreak: the rock stop"). |
| blues (kBrkD/kBrkB/kBrkC, ~172-183) | 6, **byte-identical shape** to funk's (kick+crash, then snare×2/tomMid/tomLow at steps 12-15) except kick velocity 100 vs 120 | 1 (root, octave -1) | 4 (root-third-fifth-seventh, stab) | — | Same idiom as rock/funk; genuinely a copy with a velocity tweak between blues and funk. |
| funk | 6, same shape as blues | 1 (root, octave 0) | 4 (stab) | — | See above — near-duplicate of blues' break. |
| disco | 6 (kick+crash, then tomMid/tomLow/clap/openHat at 12-15) | 1 | 4 (stab) | — | Same skeleton, disco-flavored hit choice (clap, open hat) — a real if modest differentiation. |
| motown | 7 (adds a clap on the downbeat) | 1 | 4 (stab) | — | Same skeleton +1 hit. |
| latin.hpp | 6 (**crash+cowbell downbeat, then hi/lo timbale pickup**) + a dedicated Perc role (5 claves hits) | 1 (root, octave -1) | 4 (stab) | 5 events | The one break that is genuinely idiomatically distinct — timbale/cowbell/claves instead of the rock-kit snare/tom pickup, and it is the only break with a Perc role at all. |

Verdict: the break idiom itself ("accented downbeat → silence → pickup") is a
legitimate, recognizable arranger-workstation convention (Yamaha/Korg call this
exact shape a "Break" fill) and where authored it is not a stub — but it is
**underused** (copy-pasted near-verbatim between blues and funk) rather than
**model-gapped**: nothing in `StyleEvent`/`StylePattern` prevents a genuinely
different break per genre; nobody has written one for 10 of the 16 styles or
diversified it beyond latin for the other 5.

### kEnding1 — 16/16 styles

Representative full dump (all 16 follow this exact shape; see the measurement
appendix data below for the complete per-style listing):

- rock.hpp:248-264 (`kEndDrums`/`kEndBass`/`kEndChord`): Drums = kick (gate
  `kGateHat`) + crash (gate `kGateHalfBar`) at step 0. Bass = single held root at
  step 0 (gate `kGateHeld`, ~3600 ticks, "just under a full bar" per
  `style_model.hpp:259`). Chord1 = root+fifth+root-octave-up, all step 0, all
  held. Pad = root-third-fifth-seventh, all step 0, held, `gm_program` set,
  `voicing=kLead`.
- Every other style (ballad, basic, blues, bossa, country, disco, funk, house,
  latin, motown, pop, reggae, samba, shuffle, swing) authors the *identical*
  4-role shape: Drums (2-3 hits, one of them a `kGateHalfBar`/`kGateHeld` crash),
  Bass (1 held root), Chord1 (3-4 held chord tones), Pad (4 held chord tones,
  `gm_program` + `voicing=kLead`). All events are at `step=0`, one lone exception
  (reggae, below).
- **reggae is the one instance of genuine genre awareness inside an otherwise
  generic template**: its ending1 Drums hits land at `step=8` (kick+snare+crash),
  not `step=0` — beat 3 of 4, matching the one-drop convention (kick+snare land
  together off beat 1) reggae's own main variations use. The *pitch content*
  (held root-third-fifth Chord1/Pad) is still the generic template, but the
  *rhythmic placement* is genre-correct. This is worth crediting: it shows the
  model already CAN carry a genre-specific ending gesture; only 1/16 styles does.

Verdict: **thin stub, confirmed quantitatively** — 0 `ChordGesture`, 0 alternate
`NoteSource`, every event at (or near) `step=0`, and the chord-tone set is
identical (root/third/fifth[/seventh]) across every style regardless of genre.
This is a hold, not a gesture.

### kEnding2 — 16/16 styles

Same 4 roles as ending1 in every style, always with 3-5 more events than
ending1's own version: typically one extra drum hit (a second kick or snare at
step 8/12), the bass doubled at `octave=-1` AND `octave=0` (a low-octave root
under the ending1's single root), and occasionally one extra chord tone (an
octave-doubled third). Example, rock.hpp:309-321 (`kEnd2Drums`/`kEnd2Bass`/
`kEnd2Chord`): Drums grows from 2 to 5 events (crash+kick+snare+kick+crash across
steps 0/8/12), Bass grows from 1 to 2 (root at octave -1 AND 0), Chord1 stays 3
held tones. This pattern — "ending2 = ending1 + a slightly bigger tag" — is
consistent across every style in the corpus (ballad/basic/blues/bossa/country/
disco/funk/house/latin/motown/pop/reggae/samba/shuffle/swing all show the same
+1-drum-hit, +1-bass-octave shape). It is a real, if minimal, escalation
(ending2 reads as "bigger" than ending1, which is directionally correct for a
Yamaha/Korg-style two-tier ending convention) but it is **not a musically distinct
idea** from ending1 — same chord tones, same static hold, same lack of gesture.

## 2.2 What is model gap vs underuse, precisely

- **UNDERUSE** (the data model already supports better; nobody authored it):
  `ChordGesture` (`kStrumUp`/`kStrumDown`/`kRollUp`/`kRollDown`,
  style_model.hpp:70-76) is used extensively in main variations (e.g.
  rock.hpp:343, `kVarDChord` uses `kStrumDown`) but **zero times** in any
  break/ending across the whole corpus — a strummed/rolled final chord instead of
  a flat block-chord hit is a one-line change per event and would be the single
  highest-value cheap fix. `MotifSpec`/the 9210 motif engine is wired into 4/147
  role-instances only. `NoteSource::kScaleDegree`/`kInterval` (used for e.g.
  blues' harp lead, blues.hpp:50-56) is never used to add a melodic tag/riff to
  an ending. Growing `bars` from 1 to 2+ is proven-safe, already-used data
  (16/16 styles already author `Intro1` at `bars=2`, confirmed by grep across all
  16 files) — nothing stops a 2-bar ending today; it just hasn't been authored.
- **MODEL GAP** (the format genuinely cannot express this without a new
  mechanism): a tempo ritardando/tempo-curve during an ending. Confirmed by grep
  (`ritard|tempo_ramp|rallentando` across `components/core/`) — **zero hits,
  nothing exists**. `Transport::set_bpm` (runtime/transport.hpp:39-45) is a
  discrete, host-triggered set, not an authored curve; `StyleEvent`/
  `StylePattern`/`StyleSection` carry no tempo field at all. A cadential
  *harmonic* resolution (e.g. forcing V→I regardless of the live progression) is
  also out of scope for style data by design — the harmony is host-supplied
  (`ChordSequence` per style, memory: harmony-progression-architecture) precisely
  so styles stay user-definable via SFF import without hard-coding a
  progression; an ending's job in this architecture is textural/rhythmic
  (how the final chord is voiced and hit), not harmonic (which chord it is).

## 3. CLI-vs-GUI sequencing and exposure comparison

**Sequencing (core-level, identical for both surfaces).** Both the CLI/shell and
the GUI's in-process translator send the same wire verb, `style section <name>`,
into the same `Engine::cmd_section`/`Arranger::on_tick` path
(`shell_parse.cpp:240-263`, `apps/gui-sonotron/src/in_process_brain_session.cpp:
270-290` — the two verb tables are structurally identical, both listing all 13
`SectionType` names including `ending2`). The core's own one-shot rule
(`arranger.hpp:544-554`) is unconditional and identical regardless of caller:

- **Ending** (`section_is_ending`, style_model.hpp:44): when the section's
  authored `bars` elapse, `result.stop_transport = true` is set unconditionally —
  "an Ending's own auto-stop is PRESERVED regardless of scene ownership... checked
  FIRST" (arranger.hpp:544-547, comment is load-bearing). This is genuinely well
  built: it also correctly handles the SceneChain-owned case
  (`m_scene_hold_bars` overriding the style's own `bars` for timing purposes,
  arranger.hpp:511-513) and is covered by tests (`test_ending_stops_transport`,
  `test_arranger.cpp:617-633`; `test_scene_ending_stops_transport_at_its_own_hold_
  bars`, `test_scene.cpp:291-322`).
- **Break**: `kBreak` matches **none** of `section_is_variation`,
  `section_is_fill`, `section_is_intro`, or `section_is_ending`
  (style_model.hpp:37-44), so it falls into the final `else` branch of the bar-
  boundary dispatch (arranger.hpp:558-570) — the same branch a plain variation
  loop takes. **A break does not auto-return to the previous variation when its
  bar elapses; it loops on itself** until something else explicitly requests a
  section change. This is a real behavioral finding, not obviously a bug (it
  matches "hold on the break until cued off" semantics some arranger workstations
  use) but it is worth an explicit owner call: most Yamaha/Korg-style arrangers
  auto-return a Break after exactly one bar, the same way a Fill does. If the
  owner's mental model of "break" is "one bar then back to the groove," this is
  the actual root cause of that not happening, and it is a `section_is_break`-
  style engine change, not a style-authoring change — **flagged in §4 as
  needs-core-decision**, separate from every other item in this doc (which are
  all pure data).

**Exposure (host-level, and this is where CLI and GUI genuinely diverge).**

| Surface | Section verbs reachable | Evidence |
|---|---|---|
| CLI/shell | All 13 `SectionType` names, including `intro2`, `fillB/C/D`, `break`, `ending1`, `ending2` | `shell_parse.cpp:245-259` |
| GUI wire translator | Same 13, at the wire-protocol level | `in_process_brain_session.cpp:270-290` (identical table) |
| GUI **interactive placement** (Repeat-Zone drag palette) | Only 8: intro1, varA, varB, varC, varD, **break**, fillA, **ending1** | `browser_panel.cpp:53-79` (`kVariations`/`kVariationSections`/`kVariationWireNames`, index-parallel arrays) |
| GUI **transport-panel one-shot button** | Only `ending1` (hardcoded `"style section ending1"`) | `transport_panel.cpp:43-75` (`render_ending_pad`) |

So the actual, narrow exposure gap is: **`ending2` cannot be placed into a Repeat-
Zone column or triggered from the transport panel at all today** — not because the
drop target rejects it (`grid_panel.cpp:903-905`,
`ImGui::AcceptDragDropPayload(kVariationDragPayloadId)` → `model.set_scene_section
(s, section)` takes a raw, **unbounds-checked** `std::uint8_t`, confirmed by
reading `GridModel::set_scene_section`, `grid_model.hpp:134`/`grid_model.cpp:80`)
but because `browser_panel.cpp`'s `kVariations` array simply never lists it.
`intro2`/`fillB`/`fillC`/`fillD` have the identical gap but are outside the
owner's stated question.

## 4. Prioritized recommendations

Every item is separated by feasibility label. "Effort" is a rough order-of-
magnitude, not a commitment.

### [data-only] — style-table changes, zero core/ABI/GUI touch

1. **Give every ending a `ChordGesture` instead of a flat block hit.**
   `kStrumDown` on Chord1 and Pad (already used elsewhere in every style, e.g.
   rock.hpp:343) turns the current "four notes fire simultaneously" hold into an
   audible strum-into-sustain — the single highest musical-payoff-per-line-of-code
   change here. Effort: ~1 line per Chord1/Pad event × 32 role-instances (16
   styles × 2 endings) ≈ small, mechanical, high-value. Do this FIRST.
2. **Differentiate ending2 from ending1 by more than "+1 drum hit, +1 bass
   octave."** Concretely: give ending2 a genuinely different final voicing (e.g.
   a 9th/6th color tone via `NoteSource::kInterval`, mirroring blues'
   `kLeadLick` technique at blues.hpp:50-56) or a short 2-3-note pickup lick on
   Lead/Arp (roles that exist in the corpus but are silent in every ending
   today — 0/147 role-instances use Lead/Arp/Chord2 in break/ending sections
   except latin's break Perc and a handful of full-band endings). Effort:
   medium, per-style hand-authoring (no mechanical shortcut — this is where
   genre judgment lives).
3. **Grow endings to 2 bars with a real cadential arc**, now that `bars=2` is a
   proven, already-used pattern (16/16 styles already do this for `Intro1`; no
   `kMaxBars` cap exists anywhere in `components/core` — grepped, none found; a
   `StyleSection::bars` is `std::uint8_t`, room to 255). Shape per the standard
   arranger-workstation convention (researched: this matches the Yamaha/Korg
   "Ending 1/2" idiom): bar 1 = the current held-chord tag (unchanged), bar 2 =
   a rhythmic tag-out (drum fill or a final unison stab) landing on beat 1 of
   bar 2, THEN the sustain. This is squarely a data change: extend the relevant
   `StyleEvent` arrays with `step=16..31` entries (the existing multi-bar step-
   addressing scheme, confirmed by reading `Intro1`'s own 2-bar tables, e.g.
   rock.hpp:266-279 uses `step=16..30` unremarkably) and bump `.bars=2`. Effort:
   medium-large across 16 styles × 2 endings (32 tables) if done by hand;
   plausibly script-assisted (a "double the bar, add one final hit" transform)
   for a first pass, then hand-tuned per genre family.
4. **Author a break for the 10 styles that lack one, and de-duplicate
   blues/funk's near-identical break.** Per-genre character, grounded in real
   convention:
   - **funk**: keep the stop-time idea but replace the copy-pasted blues shape
     with a genuine "on the one" funk stop (a single unison hit on beat 1,
     total silence for 3 beats, no snare/tom pickup — funk stops are famous for
     the SILENCE, not a fill back in).
   - **reggae**: a dub-style drop-out — bass and drums cut for a full bar
     (leaving only a sparse skank on Chord1/Chord2), a one-drop convention this
     genre already half-implements in its ending1 (see §2, reggae's step=8
     placement).
   - **samba**: a "breque" (stop-time break) — a syncopated unison hit across
     drums/perc landing off the beat, matching samba's own surdo/tamborim
     idiom, not a rock-kit snare/tom pickup.
   - **bossa/country/house/pop/shuffle/swing/ballad/basic**: lower priority —
     these genres use breaks less centrally in real arranger-workstation
     corpora (per the Yamaha SFF ground truth in
     `docs/backlog/yamaha-style-corpus-and-rules.md`); a generic "hold the
     downbeat, silence, pickup" break (the rock/disco/motown shape) is a
     defensible minimum viable version if the owner wants uniform coverage
     rather than genre-by-genre craft.
   Effort: large (10 styles' worth of new authored content, genre research per
   style already partly done above).

### [needs-core-decision] — a real engine-behavior question, not authoring

5. **Decide whether `kBreak` should auto-return after N bars, like a Fill
   does.** Today it loops on itself (arranger.hpp:558-570 fall-through, §3
   above) — this may be the actual mechanism behind the owner's "doesn't feel
   like a break" impression, independent of content quality. If the intended
   semantic is "hold the break until the user/song explicitly cues off it"
   (a deliberate design, matching some workstations' "Break = vamp" mode),
   no change is needed — only documentation. If the intended semantic is "one
   bar, then back to the groove" (the more common workstation convention), this
   needs `section_is_break(SectionType)` added alongside
   `section_is_fill`/`section_is_intro`, wired into the same
   `!m_scene_owns_timing && (...)` branch at arranger.hpp:555-557 that already
   handles Fill/Intro auto-return. Small, contained core change IF the owner
   confirms the semantic — flagged, not assumed. Effort: small once decided.
6. **A tempo ritardando on endings** is a genuine model gap (§2.2) with two very
   different-cost paths: (a) **HOST-ONLY, no core change** — the host already
   has a live `transport tempo <bpm>` verb (`cmd_transport`,
   `shell_io_commands.cpp:349-372`); a GUI/CLI-side script could fire a handful
   of small BPM nudges quantized to beats across the ending's final bar,
   faking a ritard without touching `components/core` at all. Musically
   crude (discrete steps, not a smooth curve) but shippable today, HOST-ONLY.
   (b) **needs-core-decision** — a proper authored tempo-curve field on
   `StyleSection`/`StyleEvent` (continuous, sample-accurate) is a real ABI/data-
   model addition and should not be assumed; flag for owner approval before any
   implementor touches it. Recommend NOT pursuing (b) unless (a) is tried first
   and found insufficient — INSTRUCTIVE-BUT-INFEASIBLE-NOW.

### [GUI-host-only] — exposure, zero core/ABI touch

7. **Add `ending2` (and, if the owner wants full parity, `intro2`/`fillB/C/D`)
   to the Repeat-Zone drag palette.** The drop target already accepts any
   `std::uint8_t` unconditionally (`grid_panel.cpp:903-905`,
   `GridModel::set_scene_section`) — the only change needed is appending one
   entry to each of `browser_panel.cpp`'s three index-parallel arrays
   (`kVariations`, `kVariationSections`, `kVariationWireNames`,
   `browser_panel.cpp:53-79`): a `"outro 2"` (or similarly named) row mapped to
   `SectionType::kEnding2 (12)` / wire name `"ending2"`. Effort: trivial,
   mechanical, host-only GUI code — no dependency, no ABI.
8. **Optionally, extend the transport panel's END pad to a two-state or long-
   press "Ending 2" affordance**, mirroring `render_ending_pad`
   (`transport_panel.cpp:62-75`) but sending `"style section ending2"`. Lower
   priority than #7 (the Repeat-Zone placement covers the same need more
   generally); flag as a nice-to-have, not required.

## 5. What needs an owner decision (collected)

- **#5**: does `kBreak` mean "vamp until cued off" (current behavior, keep) or
  "one bar then auto-return" (common convention, needs a small core change)?
  This single decision may be the real fix for "doesn't feel like a break."
- **#6**: is a host-only, discrete-step fake ritard (6a) good enough, or does
  the owner want a real authored tempo-curve mechanism (6b, a genuine core/ABI
  addition that should not be assumed or started without explicit approval)?
- **Scope of #4**: full genre-by-genre break authoring for all 10 missing
  styles (large effort, per-genre craft) vs a uniform minimum-viable break
  applied everywhere (small effort, lower musical payoff) — a prioritization
  call, not a technical one.
- **Scope of #7**: `ending2` alone (matches the owner's stated question) vs
  full section-palette parity with the CLI (`intro2`/`fillB/C/D` too) — small
  either way, but worth deciding as one slice or several.

No item in this document requires a new dependency, a new library, or an
external toolchain — every recommendation lives entirely inside
`components/core/arrangrr/include/arrangrr/arranger/styles/*.hpp` (data),
`components/core/arrangrr/include/arrangrr/arranger/arranger.hpp` (one
conditional, only if #5 is decided in the "auto-return" direction), or
`apps/gui-sonotron/src/browser_panel.cpp`/`transport_panel.cpp` (host-only GUI
arrays/verbs).
