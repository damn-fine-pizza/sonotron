# Build vs. buy: libfluidsynth as a host-only ISoundEngine, or roll our own DSP on TSF

Status: PROPOSAL — decision-support analysis only, no code written. Answers
the owner's question ("va bene aver libfluidsynth, ma voglio capire se ne
vale la pena o se ha piu' senso che ci riscriviamo tutto noi") for backlog
item #7 ("the instruments sound a bit synthetic"). The owner has already
decided a new audio dependency IS acceptable IF it is isolated behind
`ISoundEngine` as a host-only concrete implementation, never in core, never
on the STM32 target. This document evaluates whether libfluidsynth is worth
taking on that seam, versus writing the missing DSP ourselves on top of the
existing TinySoundFont (TSF) engine.

Author: Ottorino. Evidence-grounded musicology + engineering + licensing
analysis, no product-code edits.

---

## 0. What I measured / read

- `components/core/audio_engine/include/audio_engine/i_sound_engine.hpp` —
  the actual `ISoundEngine` contract: `dispatch(MidiMessage)`,
  `all_notes_off()`, `render(float* out, int frame_count)`, `name()`. POD-only
  signatures, no `std::string`/`std::function`/exceptions in the interface
  itself — this is the seam the owner named, and it is real, built, and
  already core-tier-placed (not just a proposal).
- `components/platform/audio/include/audio/soundfont_engine.hpp` +
  `.../src/soundfont_engine.cpp` — the ONLY concrete `ISoundEngine`
  implementation today, `SoundfontEngine`, a thin wrapper over
  `melodd::Synth` (`components/platform/engines/melodd`). Its header banner
  states plainly: "HOST-ONLY, forever: `melodd::Synth::load_soundfont` takes
  `std::string` paths and does real file I/O... No promotion path exists."
- `components/platform/engines/melodd/src/synth.cpp` — the actual render
  path: `Synth::render()` is `tsf_render_float(m_tsf, out, frame_count, 0)`,
  nothing else. No mixing bus, no send-effects, no post-processing of any
  kind between TSF and the output buffer.
- `third_party/tinysoundfont/tsf.h` (2079 lines, vendored) — TSF's own
  generator support table states explicitly:
  `{ 0, (0) }, //15 ChorusEffectsSend (unsupported)` and
  `{ 0, (0) }, //16 ReverbEffectsSend (unsupported)` (lines 570-571). TSF's
  interior sample-fetch loops (3 call sites, lines ~1294/1315/1335) are all
  labeled "Simple linear interpolation" in the source comments — there is no
  higher-order interpolation mode to switch on; it is compiled in as the only
  algorithm. TSF DOES already carry a gated `.sf3` (Ogg-Vorbis-compressed
  SoundFont) decode path (`tsf_decode_ogg`/`tsf_decode_sf3_samples`, lines
  867-935) behind `#ifdef STB_VORBIS_INCLUDE_STB_VORBIS_H` — it activates the
  moment a consumer vendors `stb_vorbis` and defines that macro before
  including `tsf.h`; nothing else in TSF needs to change.
- `third_party/tinysoundfont/ARRGRR_VENDOR.md` — TSF was chosen "together
  with `miniaudio`... single-header, permissive license, no transitive
  dependencies, widely used, small enough to audit," owner-approved
  2026-07-13 under D43/`0800`. The vendoring discipline is explicit: "Do not
  hand-edit the vendored header" — any change to TSF's own algorithms (e.g.
  patching in cubic interpolation) means forking upstream, not a local diff.
- `docs/proposals/audio-engines-family-layout.md` (Palladio) and
  `docs/proposals/isoundengine-contract.md` (Corelli) — the two prior-pass
  documents that shaped the actual `ISoundEngine` seam. Both independently
  conclude: `soundfont`/`physical-models`-style engines (real file I/O) are
  "host-only, forever," and the only plausible future core-capable engine is
  a from-scratch `analog` synth with no file I/O — irrelevant to this
  decision but confirms the doctrine is consistent. Corelli's §7 also states
  outright: "No new dependency anywhere in this proposal... same
  `miniaudio`/`tinysoundfont` already vendored... If a real `analog`
  implementation later wants an oscillator/filter DSP library instead of
  hand-rolled tables, THAT is a fresh `0800` dependency decision — flagged
  now as a likely future ask, not decided."
- `CMakeLists.txt` fork line (cited in both proposals) and
  `components/platform/engines/melodd/README.md` — confirms `melodd`/TSF is
  excluded from the `arm-none-eabi` branch today; the host/firmware split is
  a real, enforced CMake fork, not aspirational.
- `.github/workflows/build-release.yml` — the actual CI/release matrix:
  Linux (`ubuntu-latest`) is the only `required: true` leg and installs
  system packages via `apt` (ALSA, X11, Mesa — no audio-synthesis lib).
  macOS (`macos-latest`) is wired, `continue-on-error`, installs only
  `ninja` via `brew` (everything else — GLFW's Cocoa/OpenGL deps — comes
  from the OS SDK, nothing fetched). Windows is fully commented out/disabled
  ("CMake maps no MSVC... to the CXX26 dialect the project requires... the
  TUI layer is POSIX-only"). The workflow packages and uploads real binaries
  (`dist/gui-sonotron-<os>-<version>`) and can publish them as a GitHub
  Release — this IS a real binary-distribution pipeline, not a hypothetical.
- `/LICENSE` at repo root — **sonotron itself is GPLv3** ("GNU GENERAL
  PUBLIC LICENSE Version 3"). This is the single fact that reframes the
  whole licensing question (§3).
- `docs/roadmap.md:162`, `docs/phase5-design-reviews.md:40`, `README.md:107,120`,
  `components/platform/engines/melodd/README.md:28` — every existing
  mention of FluidSynth in this repo is the **external system `fluidsynth`
  binary** invoked out-of-process by `apps/demo/lib/launch.sh` (a drop-in
  alternative to `melodd` for the ALSA demo wiring), never a linked library.
  This proposal is the first time linking `libfluidsynth` in-process is on
  the table.
- Researched externally (WebSearch, not recalled): FluidSynth's real
  license is **LGPL-2.1-or-later**; its default reverb is on
  (room-size 0.2, damping 0.0, width 0.5, level 0.9) and default chorus uses
  3 delay lines — consistent with the prior pass's finding that the demo
  CLI's external `fluidsynth` has audible reverb by default, TSF has none;
  its default interpolation is 4th-order (cubic), with 7th-order available
  but reportedly prone to distortion artifacts on sharp transients in some
  reports; it CAN be built without glib since `-Dosal=cpp11
  -Denable-libinstpatch=0`, and `libsndfile`/`readline` are both optional,
  disableable at CMake configure time; `.sf3` decoding is FluidSynth's own
  built-in Vorbis decoder, `libsndfile` is only needed for FluidSynth's
  render-to-audio-file feature, irrelevant to us; static-link builds forward
  FluidSynth's own transitive dependencies to the consumer via
  `FluidSynthConfig.cmake`; LGPL's static-linking obligation is specifically
  to provide relinkable object files/build instructions for a proprietary
  consumer — moot for a GPLv3 project (§3); GPLv3 and "LGPL-2.1-or-later"
  are FSF-recognized compatible via the "or later" escalation to LGPLv3,
  which is explicitly combinable with GPLv3.

---

## 1. The musical case: what TSF is actually missing

Verified in code, not assumed: TSF renders GM SoundFont voices with linear
interpolation and literally ignores the `ChorusEffectsSend`/
`ReverbEffectsSend` SF2 generators (they're wired to constant `0` in TSF's
own generator table). `melodd::Synth::render()` hands that dry, unfiltered
signal straight to the output buffer — no reverb, no chorus, no send bus,
nothing. This is the DOMINANT, well-understood reason a dry SoundFont
render sounds "synthetic" next to a produced reference: no space, no glue,
audibly quantized pitch transitions on legato runs. Linear interpolation
also adds high-frequency aliasing/distortion above roughly the Nyquist-ish
region defined by the sample's own pitch-shift ratio — audible as a subtly
"harsh" or "digital" top end on transposed samples, distinct from the
missing-reverb problem and NOT fixed by adding effects.

Two genuinely separate defects, two genuinely separate fixes:

1. **No ambience/space** (reverb + chorus) — a post-processing DSP problem,
   solvable without touching TSF's synthesis core.
2. **Linear-only interpolation** — baked into TSF's per-voice sample-fetch
   loop, only fixable by changing TSF's own C, which the vendoring doctrine
   treats as "fork upstream," not "patch locally."

FluidSynth fixes both at once, natively, because it is a mature, actively
maintained synthesizer with a real signal chain (voice mixer → chorus send
→ reverb send → master out) and 4th-order interpolation as its default.
That is a genuine, non-trivial capability gap — I am not going to pretend
otherwise to make the BUILD case look better than it is.

---

## 2. Real integration cost against the actual seam

`ISoundEngine` is already the right shape for either path: POD dispatch/
render/all_notes_off/name, no `load_soundfont`-style method on the
interface itself (Corelli's Interface-Segregation call in
`isoundengine-contract.md` §1 — "concrete engines expose their own
configuration API on their own concrete type"). A `FluidSynthEngine :
ISoundEngine` slots in exactly where `SoundfontEngine` does today, wrapping
a `fluid_synth_t*` instead of `melodd::Synth`. This part is clean and
already proven correct by the existing wrap-not-absorb precedent
(`SoundfontEngine` wrapping `melodd::Synth`, not editing it).

What is NOT free:

- **Dependency surface.** Even in the minimal `-Dosal=cpp11
  -Denable-libinstpatch=0 -Denable-libsndfile=0 -Denable-readline=0`
  configuration, FluidSynth is a real multi-file C/C++ library — SF2/SF3
  parsing, a MIDI player/sequencer layer we don't need, a settings system,
  its own thread/timer abstraction — versus TSF's single 2079-line header.
  It is buildable from vendored source via `add_subdirectory` the same way
  `third_party/glfw`/`imgui`/`miniaudio` already are, so it is NOT a system-
  package ask on Linux/macOS CI as currently configured (no `apt`/`brew`
  line would need to change) — but it is a materially larger unit to vendor,
  pin, and re-vet on every upstream bump than anything currently in
  `third_party/`, and its own CMake package (`FluidSynthConfig.cmake`)
  forwards transitive dependencies on static builds, which is exactly the
  kind of "surprise transitive dependency" the TSF vendor doc's whole
  rationale (small, auditable, no transitive deps) was written to avoid.
- **Windows.** Currently fully disabled in CI for unrelated reasons (no
  MSVC C++26 mapping, POSIX-only TUI) — FluidSynth doesn't make Windows
  worse, but it also doesn't get validated there until that leg exists, so
  "cross-platform" claims for this dependency are unverifiable on Windows
  today regardless of which path is chosen.
- **macOS.** Wired but `continue-on-error`, so FluidSynth would ship there
  unverified by a hard CI gate until the ALSA/UDS host-layer three-way
  seam lands (unrelated blocking work, not this decision's to fix).

**Host-only means it never burdens the STM32 build — CONFIRMED.** Both
prior proposals independently establish the fork line
(`CMakeLists.txt`'s `else()` branch) and both state plainly that any
concrete engine doing real file I/O (soundfont-shaped, which FluidSynth
would be) has "no promotion path... forever." A `FluidSynthEngine` sits at
exactly the same host-only tier as today's `SoundfontEngine`, never
reachable from the `arm-none-eabi` branch. This is not a new risk — it's
the same doctrine already enforced for TSF.

---

## 3. Licensing — the first-class factor, resolved by one fact already in the tree

libfluidsynth is **LGPL-2.1-or-later**. The owner's instinct to flag this
hard was correct in general — LGPL static-linking obligations are real for
a closed-source shipped product (provide relinkable object files, build
instructions, and the ability for an end user to swap in a modified
library). **But `/LICENSE` at the repo root shows sonotron itself is
already GPLv3.** That single fact defuses the entire proprietary-shipping
concern: the "or-later" clause on FluidSynth's LGPL-2.1 license is exactly
the FSF-recognized escalation path that makes it combinable with GPLv3 (via
LGPLv3, which GPLv3 is explicitly written to combine with). A GPLv3 project
is already obligated to publish its complete corresponding source for every
release — which is a STRONGER disclosure than anything LGPL asks for. There
is no separate "must offer relinkable object files" burden to design around,
because the whole point is moot when the whole work is already GPL source-
available.

What is still owed, and cheap: FluidSynth's copyright notice and license
text must ship alongside the binary (same discipline the project already
follows for MIT-licensed `third_party/tinysoundfont`, `glfw`, `imgui`,
`miniaudio` — each carries its own `ARRGRR_VENDOR.md` + `LICENSE`). A
`third_party/fluidsynth/ARRGRR_VENDOR.md` mirroring the existing four is
the entire compliance burden. **Flag for the owner, not decided here:** if
sonotron ever wants a future closed-source/proprietary distribution channel
(a commercial build, an App-Store-style binary-only release under different
terms), that decision would have to be revisited THEN — GPLv3 already
forecloses that today regardless of FluidSynth, so this dependency adds
zero incremental constraint on that axis. I am not a lawyer; this reading
is grounded in FSF's own published compatibility guidance found via
WebSearch and the license text present in the repository, not asserted from
memory — a final go/no-go on a GPLv3 project taking on any new
copyleft dependency is still the owner's call to ratify, not mine to bless
unilaterally.

---

## 4. Maintenance/ownership cost

TSF is a single 2079-line vendored header with a one-line `tsf_impl.c` TU,
MIT-licensed, "small enough to audit" per its own vendor doc — the explicit
reason it was chosen over any alternative in D43/`0800`. FluidSynth is an
actively-developed, versioned upstream project (2.x line, real release
cadence, its own CVE history like any synthesizer parsing untrusted SF2/SF3
binary files) that would need periodic re-vetting on every pin bump, the
same discipline `ARRGRR_VENDOR.md` already documents for the four existing
vendored libraries but at meaningfully higher review cost per bump given
FluidSynth's size. This is a real, ongoing tax, not a one-time integration
cost — every future "should we bump the pin" question now involves auditing
a much bigger diff than any `third_party/` bump has required so far.

---

## 5. Option A — BUY: libfluidsynth wrapped as `FluidSynthEngine : ISoundEngine`

- **Musical payoff:** HIGH and immediate — native reverb+chorus send bus,
  4th-order interpolation, native `.sf3`, a real production-grade signal
  chain maintained by people who do nothing else. Closes both defects named
  in §1 in one dependency, correctly tuned by default (the demo CLI's
  external `fluidsynth` already sounds better than `melodd` for exactly
  this reason, per the prior pass's own finding).
- **Engineering cost:** MEDIUM. The `ISoundEngine` seam absorbs it cleanly
  (proven pattern: wrap, don't absorb, same as `SoundfontEngine`/
  `melodd::Synth` today). The cost is not the wrapper — it's vendoring and
  owning a materially larger, actively-changing upstream library long-term,
  with a build-config surface (`-Dosal=cpp11` etc.) to pin correctly on
  every platform this project will eventually need to validate (Linux
  hard-required today; macOS/Windows still unverified for unrelated
  reasons).
- **Dependency/licensing status:** NEW HOST-ONLY dependency, vendorable
  from source, no forced system package on Linux/macOS as CI is configured
  today. LGPL-2.1-or-later, compatible with sonotron's GPLv3 per §3 —
  genuinely low licensing risk given the facts on the ground, contrary to
  the owner's understandable initial wariness, but still needs an explicit
  owner sign-off as a new `0800`-class dependency decision (this document
  does not self-approve it).
- **Feasibility label: HOST-ONLY.** Never reachable from `arm-none-eabi`,
  same doctrine as TSF/melodd today — confirmed, not assumed.

## 6. Option B — BUILD: keep TSF, add the missing DSP ourselves

Two independent, separately-shippable pieces:

**B1 — reverb + chorus post-process stage.** A Freeverb-style
comb-filter/allpass reverb (Jezar's public-domain Freeverb algorithm is the
standard reference: ~8 comb filters + 4 allpass filters per channel, a
well-documented, small, dependency-free design) plus a short LFO-modulated
delay-line chorus. Both are textbook DSP, roughly 400-600 lines of new C++
combined, no external library, no heap required if scratch buffers are
fixed-size (`AudioBackend`'s render callback already deals in bounded
`frame_count` blocks). This fits `ISoundEngine` as a **decorator**: a
`ReverbSoundEngine : ISoundEngine` holding a reference to an inner
`ISoundEngine&`, forwarding `dispatch()`/`all_notes_off()` unchanged, and in
`render()` calling the inner engine into a scratch buffer before mixing the
wet signal into `out`. This is illustrative shape, not a patch:

```cpp
// ILLUSTRATIVE ONLY — not a diff, not reviewed, not sized/tuned.
class ReverbSoundEngine : public audio_engine::ISoundEngine {
 public:
  explicit ReverbSoundEngine(ISoundEngine& inner) : m_inner(inner) {}
  void dispatch(const arrangrr::MidiMessage& msg) noexcept override {
    m_inner.dispatch(msg);
  }
  void all_notes_off() noexcept override { m_inner.all_notes_off(); }
  void render(float* out, int frame_count) noexcept override {
    m_inner.render(out, frame_count);       // dry pass, in place
    m_reverb.process(out, frame_count);     // wet mix, in place
  }
  const char* name() const noexcept override { return "reverb(inner)"; }
 private:
  ISoundEngine& m_inner;
  Freeverb m_reverb;  // fixed-size internal state, no heap
};
```

How much of the FluidSynth gap this realistically closes: it closes the
**ambience/space** defect, which is plausibly the dominant contributor to
"sounds synthetic" in a demo/preset context — reverb tail is what a listener
notices first and most, before they consciously register interpolation
artifacts. It does **NOT** touch TSF's linear-interpolation defect (§1);
that stays baked into the vendored `tsf.h` sample-fetch loop and is not
reachable without forking TSF upstream, which conflicts with the explicit
"do not hand-edit the vendored header" vendoring discipline
(`ARRGRR_VENDOR.md`). Be honest about the residual gap: a hand-rolled
Freeverb, however well-tuned, will not match FluidSynth's chorus/reverb
defaults out of the box — it needs real ear-tuning time (parameter sweeps
against reference material), which is genuine effort beyond "write the DSP
once."

**B2 — native `.sf3` via vendoring `stb_vorbis`.** TSF already has the
gated decode path (`tsf_decode_sf3_samples`, confirmed in §0) waiting on
`STB_VORBIS_INCLUDE_STB_VORBIS_H` — this is a single-header vendoring
exercise (`stb_vorbis.c` is public-domain/MIT-dual-licensed, in the same
family as `third_party/tinysoundfont` itself), NOT new architecture.
Correction to the task's framing worth naming explicitly: `.sf3` is a
**file-size/packaging** benefit (smaller soundfonts to bundle/download), not
a timbral one — it does not, by itself, address "sounds synthetic." It is
cheap (a vendoring exercise, hours not days) and worth doing regardless of
which reverb/chorus path is chosen, since it costs nothing incremental once
B1 exists and directly enables shipping smaller default soundfont packs.

- **Effort estimate:** B1 (reverb+chorus DSP) — implementation is bounded
  (a few days of focused C++ for an experienced implementor: the algorithm
  is well-documented, the seam is a clean decorator), but ear-tuning against
  reference material to avoid a cheap/artificial-sounding reverb is real,
  separate, and harder to bound (this is craft, not a line count). B2
  (`.sf3` via `stb_vorbis`) — small, a day or less, mechanical vendoring +
  wiring the macro.
- **Should these be `ISoundEngine` variants or a post-process stage?**
  Recommend the decorator shape shown above (an `ISoundEngine` wrapping
  another `ISoundEngine`) over baking reverb into `AudioBackend` itself —
  it composes with EITHER concrete engine (TSF today, `FluidSynthEngine` or
  `analog` tomorrow) without `AudioBackend` needing to know effects exist,
  matching the existing "engine-agnostic device layer" principle already
  established in `audio-engines-family-layout.md` §3. This is a genuine
  design fork, not fully closed here — whether reverb should be globally
  always-on (baked into the composition root's chosen engine chain) or a
  user-toggleable per-engine wrap is a product decision, flagged in §8.
- **Longer horizon — does the `analog` engine slot change the calculus?**
  Not for THIS decision. The `physical-models`/`analog` slots named in
  `audio-engines-family-layout.md` are a from-scratch synthesis method (no
  SoundFont involved at all), a materially different, much later
  initiative. They neither compete with nor require resolving the TSF-vs-
  FluidSynth question first — the `ISoundEngine` seam already accommodates
  all of soundfont/physical/analog as siblings, and a reverb decorator
  built for B1 would work unchanged in front of a future `analog` engine
  too, which is an argument FOR building the decorator now regardless of
  which side wins A vs. B for the SoundFont path specifically.
- **Dependency/licensing status:** ZERO new host or core dependency for B1.
  ONE new tiny vendored single-header dependency for B2 (`stb_vorbis`,
  public-domain/MIT, same class as existing `third_party/` entries — trivial
  `ARRGRR_VENDOR.md` addition, not a `0800`-class debate the way FluidSynth
  is).
- **Feasibility label: SHIPPABLE.** B1 is pure DSP, no I/O, genuinely
  promotable in principle (bounded, no heap if scratch buffers are
  fixed-size) even though today it would sit host-only alongside
  `SoundfontEngine` for placement-consistency reasons, not because it must.
  B2 is HOST-ONLY (inherits TSF's own host-only-forever status, file I/O
  unchanged).

---

## 7. Option C — the honest phased answer

Neither A nor B alone is the clean, dominant answer once effort AND
residual-gap-after-each-step are weighed honestly:

- A (FluidSynth) closes the WHOLE gap in one dependency but carries a
  real, ongoing maintenance tax (§4) for a large, actively-changing library,
  and its licensing risk — while genuinely low given §3 — still deserves an
  explicit owner sign-off as a new `0800` dependency, not a rubber stamp
  from this document.
- B (own DSP) closes the LARGER PERCEIVED share of the gap (ambience,
  likely the dominant "synthetic" cue) for zero new dependency and bounded
  effort, but permanently leaves TSF's linear-interpolation artifact
  unaddressed short of forking TSF, and needs real ear-tuning time to not
  sound like a cheap DSP-101 reverb.

**Recommendation: phase it, B first.**

1. **Now — ship B1 (Freeverb-style reverb+chorus decorator on TSF) + B2
   (`stb_vorbis`/`.sf3`).** Zero new dependency, no licensing question to
   litigate, immediately shippable on every current CI leg including the
   unverified macOS/Windows ones (pure DSP, no new platform-specific build
   surface), and it plausibly closes most of what makes the current sound
   read as "synthetic" — the missing space/ambience. This also produces the
   `ISoundEngine`-decorator pattern the family will want regardless of what
   happens with FluidSynth or `analog` later (§6).
2. **Re-evaluate FluidSynth AFTER B1 ships and gets a real ear-test against
   it.** If the remaining gap (interpolation-driven harshness, the
   difference between a hand-tuned Freeverb and FluidSynth's mature signal
   chain) is still musically material once B1 is in, THEN take on
   FluidSynth as an ADDITIONAL, pluggable `ISoundEngine` variant — not a
   replacement — so a user/preset can choose "melodd/TSF (lightweight)" vs.
   "FluidSynth (fuller, heavier dependency)" the same way the family layout
   already anticipates multiple concrete engines coexisting behind one
   seam. This defers the real, non-trivial maintenance commitment (§4) until
   there is a measured reason to pay it, rather than paying it up front on
   the strength of "FluidSynth is objectively more capable" alone — which is
   true but not, by itself, proof B1 won't already have closed the
   practically audible gap.
3. **If time pressure forces a single choice now** (no phasing budget): BUY
   is defensible given §3 neutralizes the licensing risk the owner was
   right to worry about — but I would not call it the dominant answer,
   because B1 is cheaper, dependency-free, and targets the larger perceived
   defect. This is a genuine product-priority call between "ship the
   80%-solution cheaply now" and "ship the 100%-solution with a real
   dependency now" — not one I can collapse to a single verdict without
   knowing how much the owner values shipping speed vs. never revisiting
   this again (§8).

---

## 8. What needs deciding / what I flagged, not resolved

1. **Phasing vs. single-shot (§7).** Whether to ship B1 now and revisit
   FluidSynth later, or take on FluidSynth immediately as the one-shot fix,
   is a genuine product-priority fork — not a musicology or engineering
   question I can close alone.
2. **If/when FluidSynth is taken on, replace `SoundfontEngine` or add a
   second, user-selectable engine?** Corelli's `isoundengine-contract.md`
   §2 already flags live engine-swapping as materially bigger (needs
   `unique_ptr<ISoundEngine>` + a swap protocol) and explicitly
   NEEDS-DECISION, out of scope of the existing seam as built. This
   proposal inherits that open question unresolved — it is not mine to
   settle.
3. **Reverb as always-on vs. user-toggleable (§6).** Whether the
   `ReverbSoundEngine` decorator should be baked permanently into the
   composition root's engine chain, or exposed as a GUI toggle/parameter
   (wet/dry, room size), is a product/UX decision outside this document's
   remit.
4. **Owner ratification of any new dependency.** Per my own charter I do
   not add a host/core dependency — I flag it. Both `libfluidsynth` (if A or
   the later phase of C is chosen) and `stb_vorbis` (B2, trivial but still
   new) need explicit owner sign-off as `0800`-class decisions before
   Nazzareno vendors anything, even though §3's licensing analysis and §0's
   dependency-surface measurement are, I believe, complete enough to make
   that sign-off an informed one rather than a leap of faith.
5. **The GPLv3-compatibility reading in §3** is grounded in FSF's own
   published compatibility guidance and the LGPL-2.1-or-later text found via
   research, cross-checked against the repo's actual `/LICENSE` file — not
   asserted from memory — but it is not a substitute for the owner (or
   counsel, if they want that level of rigor for a real release) making the
   final call on a new copyleft dependency.
