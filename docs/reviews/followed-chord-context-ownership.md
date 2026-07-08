# As-built architecture audit — routing seams + followed-chord-context ownership

Branch: `harmony-global-steer` (D47 chord-follow, D49 input-zone, D53 quantized
chord entry landed). Read-only audit of the arrangrr core. Corelli, 2026-07-06.

Scope: (1) the routing seams — are they cohesive and additive, is a route/zone
change local; (2) the *followed-chord-context* lifecycle (`ChordEngine::m_state` /
`m_pending`) — who writes it and when it resets vs persists, and why the spread of
that ownership is a SINGLE defect surfacing as three bug symptoms.

Decisions read: D24 (arranger consumes `ChordState`), D26 (POD ABI), D40/D46
(per-tick fire order), D47 (chord-follow selector), D49 (two-zone input), D53
(quantized next-bar chord entry). Verified against `docs/DESIGN.md:79-84`.

---

## 1. Routing seams — verdict: cohesive, additive, genuinely local

The routing surface the audit named is not one table but **four independent
output-destination owners plus three input-side selectors**, and that separation is
correct, not a defect:

Output destinations
- `Router m_router` — general MIDI in→out matrix (`kRouteAdd`/`kRouteClear`),
  `components/arrangrr/include/arrangrr/routing/router.hpp:53`; used only in `push_midi_in`
  (`engine.hpp:104`).
- Arranger per-role routes `m_routes[kRoleCount]` (`kStyleRoute`), written by
  `Arranger::set_route` (`arranger.hpp:62-69`) via `style_route` (`engine.cpp:576`).
  This does NOT go through `m_router` — it is a separate, arranger-owned table.
- ChordEngine destination `m_out_port/m_out_channel` (`kChordOut`),
  `chord_engine.hpp:49`.
- Arp destination `m_arp_out_port/channel` (`kArpOut`), `engine.hpp:433`.

Input-side selectors
- `m_chord_detect_port` (`kChordDetect`) — which port the detector OBSERVES.
- `m_input_zone[port]` (`kInputZone`, D49) — route-vs-suppress per port.
- `m_arp_in_port` — which port the arp captures.

The coupling here is **low and the cohesion high**. The whole input-side decision
lives in one place — the `push_midi_in` callback (`engine.hpp:93-110`): three
booleans (`arp_captures`, `harmony_suppress`, detect-observe) read the three
selectors and fan to router / arp / detector. Changing a zone or a detect port is a
single field write in a command handler (`chord_input_zone`, `engine.cpp:260`;
`chord_detect_cmd`, `engine.cpp:241`) read at exactly one site. It does **not**
ripple. `kStyleRoute` is equally local: one `m_routes[idx]` write plus an idempotent
`apply_arranger_voices` re-emit (`engine.cpp:584`).

ABI is honest and additive: `kChordFollow = 41`, `kInputZone = 42` are appended,
ids never reused (`abi.hpp:98-107`), `Command` stays POD (`static_assert(sizeof <=
20)`), each documented in the enum. No new dependency, freestanding.

So the owner's intuition is correct and traceable: **routing itself is malleable.**
The only mild smell is that "where does sound go out" is answered by four different
owners with no unified view — acceptable, because each is a distinct producer, but
worth remembering if a future "output map" panel wants one read model.

---

## 2. The followed-chord context — verdict: DIFFUSE OWNERSHIP, one defect

`m_state` (the arranger-followed chord, read every tick at `engine.hpp:338`
`m_arranger.on_tick(..., m_chords.state(), ...)` and shown as host `current key:`,
`shell.cpp:165`) and `m_pending` (`next key:`, `shell.cpp:166`) are written by
**four un-arbitrated writers across three different layers**, with the reset-vs-
persist decision scattered per-handler:

Writers of `m_state`
- `ChordEngine::sound(..., steer)` when `steer` — `chord_engine.hpp:131-134`.
  Reached from the **sequencer** every step boundary (`fire_chord_seq`,
  `engine.hpp:320`, gated by `seq_may_follow()`) and from **manual** `chord play`
  (`chord_play` → `play*`, `engine.cpp:298-311`, gated by `manual_may_follow()`).
  IMMEDIATE, un-quantized.
- `ChordEngine::commit_pending()` — `chord_engine.hpp:182-187`, called at the bar
  boundary (`engine.hpp:205-206`). The quantized (D53) path — but ONLY the detect
  producer feeds it.
- `ChordEngine::init_context_to_key()` — `chord_engine.hpp:196-200`, called from
  **transport-start** (`engine.cpp:95`) AND **style-load** (`engine.cpp:513`).
  RESET semantics — overwrites `m_state` unconditionally.

Writers of `m_pending`
- `stage_context()` from `observe_chord_input` (`engine.hpp:384`) — detect only.
- `commit_pending()` clears it; `reset_pending()` clears it (`engine.cpp:96,514`).

Dead abstraction: `ChordEngine::set_context()` (`chord_engine.hpp:162`) — the old
immediate detect setter, now referenced only by a test
(`test_chord_follow.cpp:216`). A leftover seam from before D53. It should be deleted
(hand the line to Fabrizio/Nazzareno).

There is **no single object that decides who writes the followed context and whether
a lifecycle event resets or persists it.** The D47 selector is threaded as three
separate `bool steer` arguments through `sound()`/`play*()` (`detect_may_follow`,
`seq_may_follow`, `manual_may_follow`, `engine.hpp:303-311`); the D53 quantization
gate lives half in `advance_ticks` and half in `ChordEngine`; the reset semantics
live inline in two unrelated command handlers. The concern is spread across
`engine.cpp`, `engine.hpp` and `chord_engine.hpp` with no home.

### Why this is one defect, not three bugs

All three reported symptoms are the same missing owner:

- **Clobber of an explicit chord on transport-start.** A manual `chord play` sets
  `m_state` (via `sound(steer=true)`, default `kAuto`). Transport-start then calls
  `init_context_to_key()` (`engine.cpp:95`) which unconditionally rewrites `m_state`
  to the home key. The manual chord is gone. Root cause: a lifecycle handler owns a
  direct write to the followed context and knows nothing about an explicit chord
  being in force.

- **Section / style change resets the chord.** Same writer: `kStyleLoad` calls
  `init_context_to_key()` (`engine.cpp:513`). Any host action that re-loads or
  switches the style re-homes `m_state` regardless of an explicit chord. (A pure
  `kStyleSection` does NOT touch `m_chords` — so if the host reproduces "section
  reset", it is arriving through the load/switch path, and the fix is the same
  unconditional re-home.)

- **Self-drift with no user input.** `m_state` has four writers and no arbiter, so
  it changes without a fresh user gesture. Predicted origins, in order of
  likelihood, for Torquato's observed-chord timeline:
  1. **Sequencer re-assert under `kAuto`** — `fire_chord_seq` re-writes `m_state`
     at *every* step boundary and loop wrap (`chord_sequencer.hpp:132-145` fires the
     step whose `start == pos`; `engine.hpp:320` `sound(..., seq_may_follow())`).
     A chord the user committed mid-loop is silently overwritten back to the
     sequence's chord at the next step — "drifts on its own." *Trace signature: the
     drifted value equals a sequence step's chord, and the change lands on a step
     boundary.*
  2. **Lifecycle re-home** — `init_context_to_key` snapping `m_state` to the tonic
     on a transport-start / style-load the user did not associate with a chord
     change. *Trace signature: the drifted value equals the loaded style's
     single-finger home chord (`single_finger_quality(key, key.root_pc)`).*
  3. **Partial-release re-detect** — under single-finger (`min_notes=1`), releasing
     one finger of a held chord makes `observe_chord_input` recognize the *remaining*
     note as a new chord and stage it (`engine.hpp:378-384`), committing a different
     chord one bar later. *Trace signature: the change lands exactly one bar after a
     note-off, value = the remaining held note's single-finger chord.*

The common cause under all three: `m_state` is a shared mutable cell with writers in
the transport handler, the style handler, the sequencer fire, the manual handler and
the bar-boundary commit, and **no one owns the write-vs-persist policy.**

---

## 3. Deriva dalle decisioni

- **D53 is only half-built.** D53 says a steered chord "is STAGED as a pending
  'next' chord and only COMMITS at the next bar." Built: ONLY the detect producer
  stages (`observe_chord_input` → `stage_context`); the sequencer and manual still
  write `m_state` immediately through `sound(steer)`. The comment at
  `chord_engine.hpp:172-173` states this asymmetry as intentional ("they are already
  time-aligned"). That is defensible for the sequencer (it fires on its own step
  grid) but it is exactly what leaves three immediate writers racing one quantized
  one. **Which side yields: the *code*.** Not by forcing quantization on everyone
  (that would fight D53's own "time-aligned" note), but by forcing every writer —
  immediate or quantized — through ONE owner that holds the D47 gate and the
  explicit-vs-default latch. D53's staging model stays; its *ownership* consolidates.

- **D47 gate is threaded, not owned.** D47's shape ("thread a `bool steer` so a
  non-selected producer never publishes") is sound as a mechanism but is replicated
  at three call sites with three predicates. The decision itself is fine; the code
  should collapse the three predicates into one policy the owner reads.

- No ABI drift. `kChordFollow`/`kInputZone` match their D-decisions and are additive.

---

## 4. Proposta strutturale — a single owner for the followed context

Introduce ONE owner of `m_state` + `m_pending` — either a small `FollowedContext`
sub-object or a hardened facade on `ChordEngine` — that is the **only** code able to
change the followed chord, exposing exactly these operations and NOTHING that writes
`m_state` directly:

- `stage(Producer, ChordState)` — the single entry every producer (detect,
  sequencer, manual) calls. The D47 follow-gate is applied *inside* (one policy
  field, replacing the three threaded bools). A non-selected producer's stage is a
  no-op. This also SETS an `m_explicit` latch: "a real producer has established a
  context since the last reset."
- `commit_bar()` — the ONLY writer of `m_state` for quantized producers, called at
  the bar boundary. Replaces `commit_pending`.
- `commit_now(Producer, ChordState)` — for producers D53 keeps immediate (sequencer;
  optionally manual): writes `m_state` AND sets `m_explicit`, but still through this
  one object and one gate. Preserves D53's "time-aligned" exception without a second
  writer.
- `establish_default(Key)` — sets the home-key context **only if `!m_explicit`**.
  This is a no-op when any producer has set a real chord. Lifecycle handlers call
  THIS instead of `init_context_to_key`.
- `reset()` — the genuine new-song reset: clears `m_explicit` + `m_pending`, then
  re-homes. Called only where the owner truly wants to forget the chord.

**The ownership rule (the invariant that makes the three bugs impossible by
construction):**
> `m_state` changes ONLY via `commit_bar()`/`commit_now()` from a producer-staged
> value, or via `establish_default()` when no explicit context exists. Lifecycle
> handlers (transport-start, style-load, style-switch, section) have NO API that can
> overwrite an explicit chord — the setter that used to do so (`init_context_to_key`)
> no longer exists as a public verb.

The three symptoms then fall out correctly, not per-patch:
- Transport-start calls `establish_default(m_key)` → no-op under a manual chord →
  **no clobber.**
- Style-load / switch calls `establish_default(m_key)` → **chord persists across a
  section/style change.**
- `m_state`'s only writer is a producer stage → **no self-drift**; the sequencer's
  per-step re-assert becomes one visible, D47-gated policy decision in the owner
  (and the user can gate it off with `chord follow manual`), and the lifecycle
  re-home can no longer fire silently.

Call-site changes (direction, Nazzareno implements):
- `engine.cpp:95-96` (transport-start): `init_context_to_key(); reset_pending();`
  → `establish_default(m_key); m_chords.reset_pending();` (keep D53's "drop staged
  detect on start"; do NOT drop an explicit chord).
- `engine.cpp:513-514` (style-load): same substitution. Whether style-load should
  ever `reset()` (new song) vs `establish_default()` (keep chord) is the one owner
  call — see §5.
- `engine.hpp:206` (bar commit): `commit_pending()` → `commit_bar()`.
- `engine.hpp:320` / `chord_play` (`engine.cpp:298-311`): drop the threaded
  `seq_may_follow()/manual_may_follow()` bools; producers call
  `stage()/commit_now()` with a `Producer` tag; the gate moves inside the owner.
- `observe_chord_input` (`engine.hpp:384`): `stage_context` → `stage(Producer::kDetect, …)`.
- Delete dead `set_context` (`chord_engine.hpp:162`) and its lone test use.

**Feasibility: SHIPPABLE.** Pure internal refactor. Two `ChordState` + one `bool`
latch + a `Producer` enum — freestanding, no heap, no dependency, cross-builds
arm-none-eabi unchanged. **ABI implication: NONE** — `Param`, `Command`, `OutEvent`
untouched; the host still reads `state()`/`pending()`.

**What this makes trivial afterwards (the owner's actual question):**
- *Who steers* (D47): one policy field in the owner; a new producer = one enum tag +
  one `stage()` call.
- *When it commits* (D53 granularity — bar / half-bar / beat): one place
  (`commit_bar` cadence), not four scattered handlers.
- *Reset semantics* (does a style-load keep or drop the chord): one call
  (`establish_default` vs `reset`), decided once.
- Routing (§1) is already malleable and is untouched.

---

## 5. Cosa ho flaggato / cosa decide il proprietario

- **NEEDS-DECISION — does `kStyleLoad` mean "new song" (drop the chord) or "change
  the band under the same chord" (keep it)?** The reported symptom says *keep*, so
  the proposal defaults style-load to `establish_default` (persist). Confirm; the
  owner call is one line either way.
- **NEEDS-DECISION — should manual `chord play` become bar-quantized like detect
  (D53 uniformity), or stay immediate (D53's current "time-aligned" exception)?**
  The proposal preserves the current immediacy via `commit_now`; unifying it under
  `stage`+`commit_bar` is a feel choice, not a structural one.
- No dependency tension: the consolidation adds none. Routing seams need no change.
- `set_context` is dead product code — flag to Fabrizio/Nazzareno for removal.
- Torquato: the self-drift origin I predict is, most likely, the sequencer's
  per-step `m_state` re-assert under `kAuto` (§2, signature #1); the lifecycle
  re-home (#2) and partial-release re-detect (#3) are the alternates. The
  distinguishing trace signatures are in §2.
