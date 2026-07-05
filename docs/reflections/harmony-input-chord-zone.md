# Reflection — Harmony input / silent chord-recognition zone (two-zone split)

Owner's direction sanity-check, dissected at the C++-systems x electronic-music
intersection. Verdict form: Tenere / Rilavorare / Buttare + synthesis.
Incorporates the owner's surface/behavior refinement (see Sul tavolo) and the
single-finger (D45) maj/min refinement (see Addendum).

## Sul tavolo (the reflection as understood, refined)

The harmony-driving surface is the CHORDS panel (NOT the styles panel). When the
chords panel is focused, playing keys IS the chord-recognition zone: press a chord
(or, in single-finger mode, one key as the ROOT) -> re-harmonize the whole band
SILENTLY (the raw keys do not sound), honoring the existing chord-mode
(single/diatonic/shell) and the D47 follow axis. Simultaneously the PIANO panel
must STOP steering: today the piano panel IS the chord-detect input
(D34: `m_chord_detect_port == kPianoInputPort` -> `observe_chord_input`), and the
owner wants the piano to become a PURE instrument (keys sound, nobody follows).
So chord-recognition MOVES off the piano onto the chords panel. This is an explicit
TWO-ZONE SPLIT: piano = melody zone (sounds, no steer), chords = harmony zone
(silent, re-harmonizes). The styles-panel-keys question is now moot (styles keys can
return to scale-setting or nothing; they are not the harmony surface). Rulings asked:
(a) is moving chord-recognition off the piano — a change to D34 — the right call, or
should D34's piano-as-chord-input survive as an option; (b) the two-zone split as the
core concept (harmony-input seam feeding only `observe_chord_input`/`set_context`,
never the router/sound), host panels piano vs chords as the two zones, the STM32
keyboard split as the on-device form; (c) whether it rides D47 follow + chord-mode
with no new axis. Addendum ruling: the single-finger (D45) redefinition — one key ->
the diatonic MAJOR-or-MINOR triad of that root, never dim/aug.

## Tenere

- **The two-zone split as the core concept.** (Asse musicale + intersezione.)
  Piano = melody zone (sounds, no steer), chords = harmony zone (silent, steers) is
  the canonical split-keyboard arranger, and it is exactly the split/Zone framing the
  LOCKED design already carries: §11 line 423 "below the split_point -> chord
  recognition", line 430 "the chord recognition zone is a special Zone that feeds the
  Chord Engine instead of playing". D34(b) *deferred* this split; it did not reject
  it. The refinement does not add scope — it un-defers a decision already made and
  makes the more canonical arranger default the default. Ruling (b): confirmed, this
  IS the right core concept.

- **It rides D47 `kDetect` + the detector's chord-mode with NO new axis, and the
  move off the piano REMOVES a concurrency worry.** (Asse ingegneristico.) The
  harmony zone is still the DETECT producer: it feeds the one `ChordDetector`,
  publishes via `set_context`, gated by `detect_may_follow()`, and single-finger's
  "one key as root" is the detector's `min_notes`=1 / `ChordMode::kSingle` path
  already present. So D47's `ChordFollow::kDetect` and the single/diatonic/shell mode
  cover it unchanged — no new follow axis, no second recognizer. Crucially, because
  the piano no longer feeds the detector, there is now exactly ONE steer surface: the
  earlier hazard of two detect surfaces sharing one held-note set evaporates. Ruling
  (c): confirmed, no new axis.

- **"Observe-without-route" as a CORE primitive, split as DATA.** (Intersezione.)
  The one genuinely new mechanism is a note OBSERVED by the detector but SUPPRESSED
  from output. It belongs to the core, because on arm-none-eabi there is no chords
  panel — there is one keyboard and a split point. The panels are just the host form
  of the two zones; the split is the on-device form. The harmony zone's "destination"
  is the Chord Engine (the §11 special Zone); the melody zone's destination is an
  output port. Keep the primitive core-side, bound to the split/Zone.

## Rilavorare

- **Ruling (a): move the DEFAULT off the piano, but do NOT delete D34 — demote it to
  a fingering MODE.** (Asse ingegneristico + musicale.) Split (silent harmony zone)
  and FullKeyboard (whole keyboard both sounds AND steers) are two DISTINCT, both-real
  arranger modes — the design's own fingering enum lists `Fingered` and `FullKeyboard`
  side by side. D34 shipped FullKeyboard as the MVP whole-keyboard toggle; it is
  locked, functional-tested (`test_chord_follow`), and comes up configured from the
  `~/.arrangrr.init` example ("single-finger + chord follow detect"). Its
  sound-and-steer, no-double-voicing property IS FullKeyboard's defining behavior and
  stays valid. So the right shape is a MODE selector: default = Split (the owner's
  intent and the canonical arranger default), with FullKeyboard reachable — not a
  deletion. Note the two modes are NOT two split-point positions: FullKeyboard notes
  SOUND, the harmony zone is SILENT, so they are genuinely separate modes.

- **The seam shape: observe-not-route is right, a parallel `push_harmony_in` pipeline
  is not.** (Asse ingegneristico.) `push_midi_in` already parses once and observation
  already runs as a step independent of `m_router.route`. The real delta is *routing
  suppression*: notes chosen by the split/Zone are observed but not emitted. Keep ONE
  input entry and make the split DATA (a recognition-zone destination), not a second
  code path that duplicates the parser. Whether it surfaces as a Zone destination flag
  or a thin seam is Fabrizio's line-level call; the concept is suppress-from-output.

- **Amend D34 explicitly and record the boot-behavior migration.** (Asse
  ingegneristico.) Making Split the default changes what the `~/.arrangrr.init`
  example and `test_chord_follow` assume (piano == detect port). The new decision must
  state that it amends D34(b) (un-defers the split), redefines D34's piano-detect as
  the FullKeyboard branch, and migrates the default init/tests accordingly.

## Addendum — single-finger (D45): one key -> diatonic maj/min, never dim/aug

Owner's refinement: D45 single-finger changes from "one key -> always MAJOR" to
"one key -> the diatonically-correct MAJOR-or-MINOR triad of that root; never
dim/aug". Proposed rule: root = pressed key; quality = diatonic triad quality via
`smart_quality`; where the diatonic degree is dim/aug, SNAP to maj/min; a chromatic
(out-of-scale) root defaults to major.

- **Tenere — the rule is musically sound, and chromatic -> major is the idiomatic
  arranger default.** (Asse musicale.) "Play the scale-degree root, the band follows
  in key" is exactly what a non-keyboardist wants from a one-finger mode, and it is a
  real improvement over "always major" (which fights the key on the ii/iii/vi
  degrees). Chromatic -> major is not a cop-out: a chromatic root in an arranger most
  often functions as a secondary dominant (in C major, a D chord tends toward D7->G),
  so MAJOR is the idiomatic default for out-of-scale roots, and it satisfies the
  "never blocked mid-phrase" performance requirement.

- **Rilavorare — the snap must be PRINCIPLED, not "nearest".** (Asse musicale +
  intersezione.) "Snap to the nearest maj/min" is underspecified, and an
  underspecified snap is a wrong note waiting on the hot path. The theory-driven,
  deterministic rule: a diminished triad is a minor triad with a lowered fifth, so
  dim -> MINOR (keep the minor third, restore the perfect fifth); an augmented triad
  is a major triad with a raised fifth, so aug -> MAJOR (keep the major third, restore
  the perfect fifth). This resolves vii° in major -> a minor triad on the leading
  tone, ii° in minor -> a minor triad, III+ in harmonic minor -> a major triad —
  no tie-break ambiguity, no coin-flip. Also: NAME it honestly. This is NOT classic
  Yamaha Single Finger, which is key-INDEPENDENT (one key = major, quality specified by
  ADDING keys: root+left = 7th, root+white = minor). It is a scale-aware one-finger
  mode (Casio-Chord / "smart"/"easy" lineage). Document it as arrangrr's scale-aware
  single-finger; do not claim the Yamaha convention it does not implement.

- **Buttare/caveat — do NOT let single-finger collapse into diatonic mode.** (Asse
  ingegneristico.) The question "are single-finger and diatonic now the same minus the
  full-triad requirement?" — answer: NO. They share only the maj-vs-min lookup
  (`smart_quality`'s quality bit). They differ on three invariants that MUST stay
  explicit and tested, or a refactor silently merges them: (1) RICHNESS — diatonic
  mode uses D19 smart richness (7ths, sixths, extensions); single-finger is TRIADS
  ONLY, plain maj/min. (2) CHROMATIC HANDLING — diatonic mode is D20-strict: an
  out-of-key root is REJECTED (degree -1, silent, "no surprises"); single-finger is
  NON-rejecting and defaults chromatic -> major (a performance shortcut must never
  block). (3) DIM/AUG — diatonic mode presents the true diatonic quality (vii° stays
  dim); single-finger snaps it away. Those three invariants are the definition of the
  mode; lose them and single-finger IS diatonic. Keep them; test them.

## Buttare

- **Any new `ChordFollow` axis or a second `ChordDetector` for the silent path.**
  (Asse ingegneristico.) Redundant: the harmony zone is still `kDetect` into the one
  detector. A parallel axis or second recognizer would duplicate state and re-open the
  last-writer race D47 just closed. Dead on arrival.

- **Play-to-set-scale (forced Major) as a musical primitive — do not let it migrate.**
  (Asse musicale.) The styles-panel scale-set is now moot, and the owner may keep it or
  drop it; that is his call. But it must NOT migrate onto the chords panel or become the
  harmony gesture: in the arranger idiom a played note is a CHORD, not a key change, and
  tonality is slow-moving CONFIG owned by the `scale`/`key` command D47 already built
  (with the `scale:` vs `chord:` label split). Keep the gesture off the harmony surface.

- **"Snap to nearest maj/min" as the single-finger dim/aug rule.** (Asse musicale.)
  Superseded by the principled dim->minor / aug->major rule above. A "nearest" heuristic
  is nondeterministic groove/harmony and has no place in a chord engine.

## Verdetto

The refinement strengthens the core: a silent chords-panel harmony zone plus a pure
piano melody zone IS the canonical split-keyboard arranger, already latent in the
locked Zone/split design (§11) and merely deferred by D34(b) — so ruling (b) is a
clean yes, and (c) rides D47 `kDetect` + the detector's chord-mode with no new axis,
the move off the piano even erasing the shared-held-set hazard. On (a): move the
DEFAULT off the piano, but demote D34's piano-as-chord-input to an explicit
FullKeyboard mode rather than deleting a locked, tested, real arranger behavior. On
the single-finger addendum: the owner's "one key -> diatonic maj/min" is a sound,
more-musical rule and the correct evolution of arrangrr's (already non-Yamaha)
single-finger — accept it, but make the dim/aug snap PRINCIPLED (dim->minor,
aug->major by fifth-restoration, never "nearest"), keep chromatic->major, and hold
the three invariants (triad-only, non-rejecting, no dim/aug) that keep single-finger
from collapsing into diatonic mode. Lock it: a new Dxx that un-defers D34(b), names
the observe-without-route recognition-zone primitive bound to the split/Zone (host
panels piano vs chords as the two zones, the STM32 split as the on-device form), makes
chord-mode a Split/FullKeyboard selector with Split as default, redefines D45
single-finger to scale-aware diatonic maj/min with the principled snap, keeps
everything on D47 `kDetect` with no new axis, and records the init/test migration.
Suggested id: the next free after the D48 the D47 note earmarks for the shared-voicing
split.
