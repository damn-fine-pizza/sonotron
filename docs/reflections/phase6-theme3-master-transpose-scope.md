# `master_transpose` — global transpose musical scope (Phase-6 Theme 3, node `8200`/D24)

Ottorino, DESIGN (2026-07-14). Pre-code musical-scope review, no code written
or edited by this review. Companion piece to `docs/phase6-design-reviews.md`
(same house style; kept as a separate `docs/reflections/` file rather than an
in-place append, per this reviewer's read-only-on-existing-docs discipline).
Traces `docs/phase6-plan.md` Theme 3's own framing: *"`master_transpose`: add
the missing engine backing state (musical-scope decision on what 'global
transpose' shifts — Ottorino/owner), then wire the reserved
`Performance.master_transpose` field."* This doc is that musical-scope
decision, laid out as options for the owner — no code follows from it yet.

## What Theme 3 Item #1 is

`arrangrr::Performance` (`components/arrangrr/include/arrangrr/perf/
performance.hpp:67-69`) carries:

```cpp
std::uint16_t master_transpose = 0;  // RESERVED: no engine backing yet -- ALWAYS 0
                                     // (locked decision; do not wire without an
                                     // explicit owner/musical-scope decision)
```

It round-trips through `serialize()`/`deserialize()` (the `SNPF` wire format)
today, but nothing reads it — no code path adds it to anything. The task is
to decide, in musical terms, what a nonzero value should DO before anyone
wires it.

## Traced ground

- **The reserved field itself.** `performance.hpp:67`, `std::uint16_t`
  (UNSIGNED, no sign bit) — a real semitone offset needs both directions, so
  the current type cannot represent "down" without a reinterpretation
  decision (Decision 4 below). `sizeof(Performance) == 96` is pinned
  (`static_assert`, `performance.hpp:81`); `kPerformanceRecordWireSize == 94`
  is pinned too (`performance.hpp:116`) — any change that keeps the field's
  byte position and width needs **no** wire-format version bump, only a
  semantic reinterpretation of already-reserved bits.

- **The existing "drums never transpose" invariant.** `RolePolicy` itself
  documents it: `kFixed = 0, // literal MIDI notes (drums/percussion — never
  transposed)` (`arrangrr/arranger/style_model.hpp:47`). `Arranger::resolve()`
  repeats it as a load-bearing comment: *"kFixed roles short-circuit to the
  literal note REGARDLESS of src — drums never transpose"*
  (`arrangrr/arranger/arranger.hpp:545-547`), and the code backs the words:

  ```cpp
  static int resolve(const StylePattern& pattern, const StyleEvent& ev, const Key& key,
                     const ChordState& chord) noexcept {
    if (pattern.policy == RolePolicy::kFixed) {
      return ev.tone;
    }
    ...
  ```

  `ev.tone` is returned untouched — `kFixed` patterns never see `key`/`chord`
  at all. This is not a new mechanism to build for master_transpose; it is
  the EXACT boolean already gating the arranger's existing NTT chord-follow
  transposition, and it can gate a semitone offset for free.

- **Measured, not assumed: the exemption already covers 100% of drums/perc
  in the shipped corpus.** Grepping the `.role=`/`.policy=` designated
  initializers across all 16 style tables
  (`components/arrangrr/include/arrangrr/arranger/styles/*.hpp`):

  | role, policy pair | count |
  |---|---|
  | `TrackRole::kDrums, RolePolicy::kFixed` | 198 |
  | `TrackRole::kBass, RolePolicy::kChordTone` | 198 |
  | `TrackRole::kChord1, RolePolicy::kChordTone` | 119 |
  | `TrackRole::kPad, RolePolicy::kChordTone` | 113 |
  | `TrackRole::kChord2, RolePolicy::kChordTone` | 64 |
  | `TrackRole::kPerc, RolePolicy::kFixed` | 59 |
  | `TrackRole::kArp, RolePolicy::kChordTone` | 32 |
  | `TrackRole::kLead, RolePolicy::kChordTone` | 11 |

  Every single `kDrums` (198/198) and every single `kPerc` (59/59) pattern in
  the corpus is `RolePolicy::kFixed`; there is no mixed/pitched-percussion
  counter-example to worry about. `TrackRole::kCc` (= 9) appears in **zero**
  style patterns anywhere in the corpus (`grep -c TrackRole::kCc
  components/arrangrr/include/arrangrr/arranger/styles/*.hpp` → 0 hits), and
  `Arranger`'s own per-role register table comments it `60, // kCc (unused)`
  (`arranger.hpp:601`); `components/hostrt/parts_view.hpp:26` independently
  states "kLead/kCc are excluded — they are not arranger band parts." `kCc`
  is a routing/ABI slot (raw MIDI CC destination), not a pitched role, and it
  is already structurally inert in the resolve path.

- **The D40 pipeline and where absolute note numbers are actually born.**
  `Arranger::on_tick` (`arranger.hpp:315-509`) runs, per D40 (node `3120`):
  gather → `gesture::expand` (1 event → N specs) → `resolve()` (NTT,
  D24/node `3110`) → `m_voicing.voice()` (voice-leading, D41/node `3140`) →
  `groove::apply()` → `schedule()`. **`resolve()` is the one and only place
  the absolute MIDI note integer is computed** — `anchor + chord.root_pc +
  offset + 12*(ev.octave+wrap)` for chord-tone/interval/scale-degree
  branches, clamped `(note < 0 || note > 127) ? -1 : note` right there. Every
  later stage (`voice()`, `groove::apply()`, the FX chain, `schedule()`)
  only ever moves or fans out notes that already exist; none of them
  recompute pitch.

- **Where `Key`/`ChordState` come from, every tick.**
  `Engine::fire_arranger` (`components/arrangrr/include/arrangrr/
  engine.hpp:565-567`): `m_arranger.on_tick(transport_tick, m_chords.key(),
  m_chords.state(), ...)`. Both are read FRESH each tick straight off
  `ChordEngine`, which itself proxies the single-owner `FollowedContext`
  (`components/chorddet/include/chorddet/followed_context.hpp`). `Key::
  root_pc` and `ChordState::root_pc` are both `std::uint8_t`, explicitly a
  **pitch class, 0..11** (`chorddet/theory.hpp:35-38`; `ChordEngine::sound()`
  computes `root_pc = root_note % 12`, `chord_engine.hpp:144`).

- **The register-fold hazard of transposing at the pitch-class level.**
  Because `resolve()`'s formula adds `chord.root_pc` (0..11) directly to a
  role's FIXED octave anchor (`kRoleAnchor[role]`, `arranger.hpp:591-602`,
  e.g. bass = 36, chord1 = 60), a semitone offset applied to `root_pc` mod 12
  can only ever move the sounding pitch within a single octave (0..11
  semitones). Any multiple-of-12 component of a wider offset (a full octave
  or more, which a reasonable ±24 UI range would include) is silently
  absorbed by the modulo and produces **zero** audible change, because the
  octave register comes from the fixed anchor, not from `root_pc`'s
  magnitude — there is no octave-carry path from a pc-typed field into the
  anchor today. This is a real, provable engineering defect in any option
  that tries to transpose by mutating `root_pc`/`key.root_pc` rather than
  the final absolute note.

- **The chord-tracking / detection path is a separate producer, gated by
  D47.** `FollowedContext` (`chorddet/followed_context.hpp`) is the single
  owner of the followed chord; `ChordEngine::steer_detect()` publishes the
  LIVE-detected root (from the piano→chord detector) into it exactly like
  the sequencer/manual paths (`chord_engine.hpp:180-188`). Nothing in that
  publish path involves absolute note numbers — it is pure pitch-class +
  quality. Detection is architecturally decoupled from the arranger's own
  note synthesis (`resolve()` reads `chord.root_pc` only after publish).

- **Three, not one, note-emitting paths exist system-wide — this bears
  directly on "how global is global."**
  1. `Arranger::on_tick`/`resolve()` — the per-role band output (D40/D24),
     traced above.
  2. `ChordEngine::sound()` (`chord_engine.hpp:139-162`) — the chord voicing
     ChordEngine sounds DIRECTLY (manual `chord play`, `play_single`,
     `play_shell`, and the `ChordSequencer`'s playback all route through
     it): `const int n = root_note + shape.offsets[i]; if (n > 127) continue;`
     scheduled on its own `m_out_port`/`m_out_channel` — entirely OUTSIDE
     `Arranger::on_tick`. This is the note stream the player actually HEARS
     as "the chord I pressed," separate from the band's comping.
  3. `Timeline::Track` step data (`timeline/timeline.hpp:54-73`) — `Step{
     std::uint8_t note; ...}` is a LITERAL, already-absolute recorded MIDI
     note (D10, the "Living Timeline" write-gesture primitive), unrelated to
     the Style/NTT system. A `ContentKind::kStepTrack` clip
     (`Engine::apply_clip_content`, `engine.cpp:828-833`) just mutes/unmutes
     one of these tracks — it never touches `resolve()`.
  All three funnel their final `note_on`/`note_off` through the SAME
  `Engine::schedule_or_warn` choke point (`engine.hpp:440`), but at that
  point only `port`/`channel`/raw `MidiMessage` are visible — no
  `TrackRole`/`RolePolicy` survive that far, so a transpose applied there
  cannot cheaply re-derive the kDrums exemption without new plumbing.

- **Pads that wrap chords/phrases don't add a fourth path — they re-enter
  one of the three above.** `PadType::kPhrase`/`kChord`
  (`arrangrr/pad/pad_bank.hpp:20-28`) both "wrap a ClipMatrix clip." A fired
  Clip's `ContentKind` (`clip/clip_matrix.hpp:31-35`) is one of
  `kStyleSection` (re-enters path 1, the Arranger, via
  `m_arranger.request()`), `kChordSequence` (drives path 2, `ChordEngine`,
  via `fire_chord_seq`/`ChordSequencer`), or `kStepTrack` (path 3, a literal
  recorded track, `engine.cpp:797-835`). So the "pad scope" question
  (item #5) is really: does master_transpose cover path 2 and path 3, or
  only path 1?

- **The existing drop-not-fold convention, twice.** `resolve()`:
  `(note < 0 || note > 127) ? -1 : note` (silently drops). `ChordEngine::
  sound()`: `if (n > 127) { continue; // clamp: drop tones that leave the
  range }`. Two independent code paths already agree: an out-of-range note
  is DROPPED, never octave-folded. Any transpose design should match this
  rather than invent a third convention.

## Decision 1 — kDrums/kPerc/kCc exemption

**Recommendation: gate the offset on `pattern.policy != RolePolicy::kFixed`
inside `resolve()`.** This is not new logic — it is the exact boolean
already used to keep drums immune to the arranger's own NTT chord-follow
transposition, reused for free. It is measured to cover 100% of the corpus's
percussive content (198/198 `kDrums`, 59/59 `kPerc`), and `kCc` never
reaches `resolve()` at all today (0/0 usages), so it is exempt by
construction, not by a special case that could rot. **No decision needed
here beyond "yes, reuse `RolePolicy::kFixed` as the exemption" — the
codebase already answered this question when it built the NTT.**

## Decision 2 — early vs late (the core musical fork)

Two readings of "shift the whole arrangement a whole step up" are audibly
different, and the codebase's own field TYPES decide which one is even
buildable without new bugs:

- **"Early" (re-derive the harmonic frame — transpose `key.root_pc`/
  `chord.root_pc` before `Arranger::on_tick` reads them).** Philosophically
  closest to the player's mental model ("the band re-keys itself"), and
  echoes the *existing* precedent set for `ChordSequence::transpose_to`/
  `transpose_by` (D28, node `2410`): re-derive degree-relative content
  against a new key rather than blindly shifting output. **But it does not
  actually work as a semitone offset here**, because `root_pc` is a
  pitch-class field (0..11) added directly to a role's FIXED octave anchor —
  the register-fold hazard traced above means any multiple of 12 in the
  offset vanishes, and a ±24 UI range would silently stop doing anything for
  its outer half. Fixing this properly would mean threading an explicit
  octave-carry term into `resolve()`'s anchor computation for every
  `NoteSource` branch, AND teaching `VoicingState::voice()`'s nearest-octave
  history (`m_last[r]`, `voicing.hpp`) about a transpose that can change
  mid-performance (a live edit of `master_transpose` would otherwise cause a
  stale-octave jump the next time a `kLead`-voiced pattern re-voices). This
  is real, scoped, and doable — but it is NOT a one-line change, and nothing
  in the existing NTT/voicing code was built expecting the harmonic root to
  carry an octave component. Marking this **INSTRUCTIVE-BUT-INFEASIBLE as a
  first cut** — the instinct (re-derive, don't blindly shift) is musically
  the more honest one and worth keeping on the roadmap, but it needs a
  follow-up design pass on octave-carry + voicing-history invalidation
  before it is safe to build.

- **"Late" (shift the final absolute note number, inside `resolve()`, before
  its own `[0,127]` clamp).** Mechanically: add `master_transpose` to `note`
  in the `kInterval`/`kScaleDegree`/`kChordTone` branches only, right before
  the existing `(note < 0 || note > 127) ? -1 : note` line. This operates on
  the TRUE absolute pitch, so it has no register-fold hazard at any offset
  magnitude, needs no change to `VoicingState` (voice-leading still computes
  on already-transposed absolute notes, consistently, since the offset is a
  constant added once at the earliest point notes exist), and reuses the
  drop-not-fold convention already sitting on that exact line. It is
  audibly close to indistinguishable from the "early" reading for any
  transpose that does not cross the register-fold boundary (i.e. most single
  ±1-octave use), and it is honest about being an output-stage shift for the
  rest. **Recommended**, purely because it is the only one of the two that
  is both mathematically correct at every offset and buildable now without
  touching voice-leading state.

## Decision 3 — chord-tracking interaction

Because Decision 2 recommends transposing the ABSOLUTE note at `resolve()`,
not the pitch-class fields, the detected chord in `FollowedContext`/
`ChordState` is **left untouched** by construction — live piano→chord
detection keeps reporting the root the player is actually playing (a
`chord_followed` `OutEvent`/host readout stays correct), and only the
arranger's synthesized notes move. This matches the arranger-keyboard
convention this reviewer is aware of in general (transpose lets you keep
playing/reading in the written key while the instrument sounds elsewhere) —
detection and transposition are cleanly orthogonal here, which is a genuine
point in the "late" option's favor beyond the pure engineering argument
above: a live-detected chord AND a global transpose can coexist with no
special-case interaction code at all.

## Decision 4 — range, type, clamping

- **Type/encoding.** `master_transpose` is `std::uint16_t`, unsigned, "ALWAYS
  0" today. A signed semitone offset needs a sign. Two ABI-compatible paths,
  neither requiring `kPerformanceFormatVersion` to bump (the field's byte
  position/width is unchanged either way):
  1. Reinterpret the low byte as `std::int8_t` (two's complement, −128..127)
     at point-of-use, leaving the high byte reserved for later. Keeps `0 ==
     no transpose` — the exact identity the "ALWAYS 0" comment already
     guarantees for every existing on-disk Performance record, so old
     records silently mean "no transpose" under the new interpretation too.
  2. Widen the field itself to `std::int16_t` in a future format bump
     (cleaner long-term, but only worth it alongside the `format_version 2`
     bump Theme 3 already lists for the FX-chain/Router snapshot — no reason
     to force a migrator just for this).
  Recommend (1): zero-cost, `0` keeps meaning "no transpose" for every
  existing saved Performance, and the type question genuinely does not need
  to block the musical-scope decision.
- **Clamping at 0/127.** Match the codebase's own established convention
  (`resolve()`, `ChordEngine::sound()`): **drop the note**, never
  octave-fold. Folding would put a deliberately-transposed note in a
  register the offset was never asking for — the opposite of what a
  transpose is for.
- **UI range.** No authoritative number was verifiable from primary sources
  in this pass (a `roland-arranger.com` forum thread on exactly this
  "designating a style track to NOT transpose" question turned up in search
  but was unreachable to fetch/verify, so it is NOT cited as evidence — only
  named here as a pointer for the owner to check by hand if useful). General,
  widely-known keyboard convention favors small ranges (commonly ±1 octave);
  this reviewer flags ±12 as the conservative default and ±24 as the wider
  option the corpus's own per-role anchor spread (36..72, a 3-octave span
  across roles) can absorb without any role's output leaving a sane MIDI
  register in ordinary use — but the exact UI bound is a product call, not a
  musicological one, and is listed as open below.

## Decision 5 — pads & scope boundary

Three real, independently-scopable surfaces exist (traced above), and "make
`master_transpose` act" does not by itself say which of them move together:

1. **The Arranger's own band output** (`Arranger::on_tick`/`resolve()`,
   reached directly AND via a `PadType::kVariation`/`kFill`/`kPhrase`/
   `kChord` pad whose `Clip.kind == kStyleSection`). Covered by Decisions
   1-4 above. **In scope for any option** — this is the literal target the
   Theme names ("global/master transpose").
2. **`ChordEngine`'s own sounded voicing** (manual `chord play`, and any
   `PadType::kChord`/`kPhrase` pad whose `Clip.kind == kChordSequence`,
   which drives `ChordSequencer` → `ChordEngine::sound()`). This is a
   SEPARATE emission path outside `Arranger::on_tick` entirely. If it is NOT
   transposed, a player who presses a chord pad while `master_transpose` is
   active hears the band shift key underneath an UNSHIFTED reference chord —
   audibly incoherent for a feature whose whole point is "the same song, a
   different key." Recommend extending the SAME late, absolute-note-offset
   treatment to `ChordEngine::sound()`'s own note computation (one more
   `+ master_transpose` before its existing `if (n > 127) continue`), so the
   "live performance surface" (band + the chord you're pressing) moves as
   one. `steer_detect()` itself stays untouched (Decision 3).
3. **Timeline step-track literal content** (`Track::steps[i].note`, reached
   only via a `PadType::kPhrase`/`kChord` pad whose `Clip.kind ==
   kStepTrack`). This is user-RECORDED absolute pitch data — the closest
   analogue in this codebase to a DAW MIDI clip. Whether a "master"
   transpose retroactively shifts a user's own recording is a genuine
   product-philosophy fork (most DAWs do NOT auto-transpose a recorded clip
   under a global transpose control unless the user explicitly links it),
   not something this review can settle from the code alone. **Flagged,
   not decided.**

## What needs deciding / what I flagged

Concrete forks the owner (not this reviewer) should close:

| # | Question | This review's read | Status |
|---|---|---|---|
| 1 | Exempt `kFixed` (drums/perc) and unused `kCc`? | Yes, reuse `RolePolicy::kFixed` — free, already 100%-measured correct across the corpus | Not really open — recommend closing as stated |
| 2 | Apply the offset early (re-key `root_pc`) or late (shift the final absolute note)? | Late — early is the more honest musical instinct but is provably broken today (register-fold) without a follow-up octave-carry + voicing-history design pass | Owner call between "ship the correct-now option" vs "wait for the more elegant one" |
| 3 | Does transpose affect the DETECTED chord root? | No, by construction of the "late" choice — detection and output stay orthogonal | Falls out of Decision 2, not independently open |
| 4a | Field encoding (signed reinterpret vs format-version widen)? | Reinterpret low byte as `int8_t`, no version bump | Cheap either way, owner's call on tidiness vs effort |
| 4b | UI-exposed range (±12 vs ±24 vs other)? | No verified external number to anchor this; ±12 conservative / ±24 upper bound both technically safe under the "drop, don't fold" rule | Open, product call |
| 5 | Does transpose reach `ChordEngine::sound()` (the pad/manual chord voicing)? | Recommend yes — otherwise a transposed band plays under an untransposed reference chord | Open, but this reviewer has a clear recommendation |
| 5b | Does transpose reach recorded `kStepTrack` content? | No strong recommendation either way — mirrors a real DAW product-philosophy fork | Open, flagged only |

## Feasibility summary

| Item | Label | Dependency |
|---|---|---|
| `resolve()` offset gated on `pattern.policy != RolePolicy::kFixed` (Decision 1/2) | SHIPPABLE | none — arithmetic + widened bounds check only, no heap, no realtime-budget impact (one `int` add per resolved note, already on the hot per-tick path) |
| Late, absolute-note-number transpose in `resolve()` (Decision 2, recommended) | SHIPPABLE | none |
| Early, pitch-class transpose of `key.root_pc`/`chord.root_pc` (Decision 2, alternative) | INSTRUCTIVE-BUT-INFEASIBLE | needs a new octave-carry mechanism through `resolve()` + `VoicingState` history invalidation before it is safe; not a dependency issue, a design-completeness issue |
| Detected chord root left untouched (Decision 3) | SHIPPABLE | none — falls out of Decision 2 for free |
| `int8_t` reinterpretation of the low byte, no format bump (Decision 4) | SHIPPABLE | none |
| `int16_t` widen alongside `format_version 2` (Decision 4, alternative) | SHIPPABLE | none, but bundle with the already-planned Theme-3 format bump rather than doing it alone |
| Extend the same offset to `ChordEngine::sound()` (Decision 5) | SHIPPABLE | none — same pattern, second call site |
| Extend the offset to `Timeline`/`kStepTrack` recorded content (Decision 5b) | NEEDS-DECISION | product-philosophy call, not structural; not costed until scoped |
