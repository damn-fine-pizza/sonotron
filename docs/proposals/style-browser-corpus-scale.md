# Style browser at corpus scale (~1010 styles) — taxonomy + UX plan

Owner task #30: the GUI style browser must scale from the 16 built-ins to a
~1010-style imported Yamaha/Korg corpus with search/filter/categories before mass
import is surfaced. **PLAN-ONLY.** Read-only on product code; this document is the
only write. Implementation is a HOST lane that lands later — it currently conflicts
with in-flight seqedit work — and depends on the corpus importer (`9400`/`9430`,
DESIGN.md) reaching a state where imported styles are actually loadable at runtime,
which is not true yet (see §1).

---

## 0. Executive summary

- The current browser (`apps/gui-sonotron/src/browser_model.{hpp,cpp}` +
  `browser_panel.cpp`) is a **flat, hand-copied literal array of 16 names**, filtered
  by a single case-insensitive substring match, rendered as one unconditionally-open
  `ImGui::TreeNodeEx` with every row drawn every frame. Every one of those four facts
  breaks at 1010 rows.
- The deeper blocker is NOT the GUI: **no runtime path yet gets a converted style, or
  any metadata about it, in front of the browser at all.** The core's `Style` struct
  and the host's `StyleModel` carry **no genre/category field**; the corpus's content
  genre classifier (`genre.cpp`) exists but is a disconnected CLI subcommand, never
  wired into the importer or the wire protocol; and imported styles are not yet in
  `kBuiltins` or reachable via any UDS verb (DESIGN.md `9430`: "deliberately never
  wired into the built-in style list"). This proposal designs the taxonomy and UX
  against what the corpus *can* honestly offer today, and flags what's still missing.
- Recommended taxonomy: a **7+1-family primary tree** derived from the corpus's own
  measured rhythmic-family clustering (already documented in
  `docs/style-corpus-and-generation.md` §7.4), populated by a **hybrid** classifier
  (filename token when present, content inference as fallback, confidence surfaced,
  manual override always available) — plus **tempo band** and **time signature** as
  reliable, already-captured secondary filters, and **feel (straight/swung)** as a
  one-field-away secondary filter once a small serialization gap is closed. "Energy"
  is NOT currently derivable by anything in the tree — flagged as new work, not
  assumed.
- Recommended UX: free-text search (broadened, not rebuilt) + per-axis filter chips
  (AND-combined, mirroring Yamaha's own Music Finder field set) + family grouping as
  **flat lists per group** (not nested trees) + `ImGuiListClipper` virtualization —
  already vendored in `third_party/imgui`, used nowhere yet in gui-sonotron, zero new
  dependency. The one genuine engineering tension, cited from the vendored library's
  own demo comment, is that list-clipping composes awkwardly with tree nodes
  (`imgui_demo.cpp:4200`) — the flat-list-per-group design sidesteps it.

---

## 1. What I measured — the real state of the pipeline

### 1.1 The browser today (flat, hardwired, unvirtualized)

`apps/gui-sonotron/src/browser_model.hpp:25-28`:

```cpp
inline constexpr std::array<std::string_view, 16> kBuiltinStyleNames = {
    "basic", "pop",   "rock",   "ballad",  "funk",  "disco",   "house", "swing",
    "bossa", "samba", "reggae", "country", "blues", "shuffle", "latin", "motown",
};
```

The comment on this array is explicit about *why* it's a literal, hand-copied list:
"gui-sonotron never includes the core (D38, pure client)". That constraint does not
go away at 1010 styles — the browser can never `#include` a style header to read a
tag off it; every field the browser shows must arrive over the wire (L0 JSONL/L1) or
be read from a host-side manifest file the GUI is explicitly allowed to touch.

`BrowserModel::style_matches_filter` (`browser_model.cpp:23-30`) is a single
`to_lower` + `std::string::find` substring test against the name only — no
category/tag axis exists to filter on.

`browser_panel.cpp::render_styles` (`:95-141`) loops `for (i = 0; i < model.style_count(); ++i)`
unconditionally, calling `matches()` and `leaf_row()` (an `ImGui::Selectable` +
manual `AddRectFilled` for the active-style border) for **every** entry, every frame,
inside a single `ImGui::TreeNodeEx(..., ImGuiTreeNodeFlags_DefaultOpen)` — there is no
row culling. At 16 rows this is free. At 1010 rows this is 1010 `Selectable` widgets
laid out and hit-tested per frame with no windowing — I have not benchmarked this (no
1010-entry build exists to benchmark), so I state this as a **structural risk from
reading the code**, not a measured frame-time number: an unclipped loop over a
1010-row single-level tree is the wrong shape regardless of exact cost.

### 1.2 What a converted style actually carries (read, not assumed)

The core runtime type, `arrangrr::Style`
(`components/core/arrangrr/include/arrangrr/arranger/style_model.hpp:175-207`):

```cpp
struct Style {
  const char* name;
  Span<const StyleSection> sections;
  GrooveParams groove{};       // swing/humanize/accent/quantize/swing_grid/seed — a FEEL, not a tag
  BpmX100 tempo = kDefaultBpm;
  std::uint8_t beats_per_bar = kBeatsPerBar;
  ...
};
```

**No genre/category/tag field exists on `Style`.** Same story one layer down, the
host-side interchange type the importer actually targets,
`arrstyle::StyleModel` (`apps/tools/arrstyle-converter/src/model.hpp:129-137`):

```cpp
struct StyleModel {
  std::string name;
  SourceFormat source_format = SourceFormat::kUnknown;
  std::uint16_t source_ppqn = 960;
  std::uint32_t tempo_milli_bpm = 120000;
  std::uint8_t time_sig_num = 4;
  std::uint8_t time_sig_den = 4;
  std::vector<StyleSection> sections;
};
```

Confirmed by the JSON writer (`serialize.cpp:71-88`, `to_json(const StyleModel&)`):
it emits exactly `name`, `source_format`, `tempo_milli_bpm`, `time_sig_num`,
`time_sig_den`, `sections` — **no genre key is ever written.** Tempo and time
signature ARE real, reliable, already-captured fields (read off the MIDI header,
`source_ppqn`/`tempo_milli_bpm`/`time_sig_num/den`) — these are the two axes I can
call SHIPPABLE metadata today without qualification.

### 1.3 Genre inference exists — but is disconnected

`apps/tools/arrstyle-converter/src/genre.hpp` + `genre.cpp` is a real, working,
content-based rule cascade (`infer_genre(const SmfFile&)`): it reads drum onset
histograms (kick/snare/hat/side-stick on the 16th grid), tempo, time signature, and
an off-beat-position swing estimate, and returns `{genre: string, confidence: float}`
— cascade order: triple-metre → waltz (0.80); swing feel → swing/jazz; four-on-floor +
118-145 BPM → disco; no-backbeat + side-stick → samba/bossa; syncopated kick +
backbeat + 90-120 BPM → funk; backbeat → ballad/rock/pop by tempo; weak tempo-only
fallback; else unknown. **I traced every call site** (`grep` across `cli.cpp`,
`main.cpp`): `infer_genre` is invoked ONLY from `cmd_infer_genre`
(`cli.cpp:326-345`), a standalone `arrstyle-converter infer-genre <file>` CLI
subcommand that prints a string to stdout. It is **never called from `import_sff`**,
**never attached to a `StyleModel`**, **never serialized** — genre today is a
print-on-demand guess, not corpus metadata sitting anywhere. Two consequences for
this proposal: (a) wiring it into the import path is a small, real, already-flagged
piece of HOST-ONLY work, not something I can claim already exists; (b) the cascade's
own vocabulary is narrow — it explicitly produces only
`{waltz, swing, jazz, disco, samba, bossa, funk, ballad, rock, pop, unknown}` (11
tokens) — it has **no dedicated branch for country, reggae, house, latin-ballroom
subtypes, tango, or march**, all of which are real, frequent filename tokens in the
corpus (§1.4). Those genres will currently fall through to an adjacent bucket by
tempo/backbeat proxy (e.g. reggae's no-backbeat-but-no-side-stick pattern likely
lands in the weak fallback, not a reggae label) — a genuine coverage gap, not a
detail I can wave away.

Also: the cascade computes `offbeat_mean` (a genuine swing-feel measurement, 0..1)
internally but **discards it** — `GenreGuess` only carries the derived genre string,
not the float. Exposing feel/swing as a filter axis needs that value threaded out to
a stored field; it is not free today (§3.3).

### 1.4 The corpus itself — measured, not recalled

I re-counted independently rather than trusting the prior doc's numbers:

```
find /var/home/crsn/.../projects/resources/styles -iname "*.sty" | wc -l   → 1010
```

Genre-token counts on the 1010 basenames (`grep -ic <token>` over the file-list),
matching `docs/style-corpus-and-generation.md` §6.1 exactly: pop 77, country 76,
rock 39, ballad 38, swing 29, waltz 21, jazz 21, dance 20, bossa 20, shuffle 18, funk
16, disco 14, blues 13, latin 12, samba 10, reggae 4, tango 5, march 4, polka 3.
Files matching **none** of a ~30-token vocabulary (pop/rock/country/.../foxtrot/
beguine): **548 of 1010 (54%)** — close to the prior doc's "~546/1184" figure across
the full multi-format set, confirming that figure independently.

**Directory structure carries almost no genre signal.** 988 of 1010 files sit under
`extra/<PACK>/` where `<PACK>` is a **hardware/pack name** (`PSR-S950-03`,
`PA600StylesPack`, `Indianindonesian-part1..6`, `Italian-styles`,
`korg-pa800-greek-01`, …) — provenance, not genre (two packs embed a genre hint in
the pack name itself, `PSR-S950-LATIN` and `PST-S950-02-Country`, the exception not
the rule). Only 22 files (`jjazzlab_user_styles/`) sit under an actual **genre-named
folder tree** (`Ballad/Ballroom/Country/Dance/Latin/Movie&Show/Pop&Rock/R&B/
Swing&Jazz/World`) — and even there, most of those folders are **empty** (only
Dance=4, Latin=6, Pop&Rock=7, Swing&Jazz=5 populated). Pre-existing folder taxonomy
covers ~2% of the corpus: real, free, but not a bulk source. The pack/provenance
name itself, however, IS a free, reliable tertiary tag (useful for the "World/
Regional" bucket — Indian/Indonesian/Greek/Italian packs are how the doc's own
"World" grouping would actually get populated).

`apps/tools/arrstyle-extractor/kb-reference/discovery-report.md` §8 independently
notes era hints in filenames ("50s"/"60s"/"70s") that are "not parsed into fields" —
consistent with what I found; a cheap future regex extraction, not built.

### 1.5 The importer's actual status (why "1010 styles in the browser" isn't just a GUI change)

DESIGN.md node `9430` (verified current, `docs/DESIGN.md:1043-1061`): CASM decode is
real (SFF1 fully decoded, SFF2 partially — `Ctb2`'s richer per-chord-group
sub-structure not fully unpacked, `casm.cpp:132-134`), proven against two real corpus
files (`test_sff_import.cpp::test_real_corpus_samples`), but explicitly **"never
wired into the built-in style list"**. Node `9420` (style compiler, data→`.cpp`) is a
first slice on an unmerged sibling worktree branch, not on this branch. There is
today **no mechanism** — no blob loader (D44 Layer B, "○ planned"), no bulk `style
list` wire extension — by which 1010 imported styles become visible to *any*
runtime client, GUI or TUI. The existing `style list` verb
(`components/platform/hostrt/shell_music_commands.cpp:613-616`) enumerates only the
in-core `styles::kBuiltins` array as bare `index  name` pairs — no metadata fields at
all. **This proposal's taxonomy and UX are therefore designed against a future state
that does not exist in the tree yet**; I say so plainly rather than implying the
corpus is one GUI patch away from browsable.

---

## 2. Taxonomy design

### 2.1 Primary axis — genre FAMILY tree (musicological, not filename-vibes)

Per `docs/style-corpus-and-generation.md` §7.4 ("family taxonomy by (feel × rhythmic
engine), not by name") — a claim I re-verified is grounded in real measured drum
signatures, not asserted — the corpus's genres cluster into a small number of
rhythmic families that share a drum/bass skeleton and differ by voice, density,
gesture, and motif. Extending that clustering to cover the corpus's full measured
genre-token vocabulary (§1.4) plus the content-classifier's real output tokens
(§1.3), the primary tree:

| Family | Genres inside | Signature (musicological) |
|---|---|---|
| Pop/Rock/Ballad | pop, rock, ballad, country, motown, soul/R&B | straight-8 backbeat, snare 2&4 |
| Dance/Four-on-floor | disco, house, dance | kick every beat (0,4,8,12), offbeat hat |
| Funk/Groove | funk | syncopated 16th kick, ghost snares |
| Swing/Shuffle/Jazz | swing, shuffle, blues, jazz/bigband, boogie | triplet subdivision, walking bass |
| Latin/Clave | bossa, samba, latin (cha-cha/rumba/mambo/beguine/bolero/salsa/merengue/calypso), reggae | clave/anticipated bass, no plain backbeat |
| Ballroom/Traditional | waltz, tango, march, polka, musette, foxtrot | 3/4 or marcato-4, oom-pah/habanera bass |
| World/Regional | arabic, greek, indian/indonesian, italian-pack items | provenance-tagged (pack name), not rhythm-tagged |
| **Needs review** (explicit 8th bucket) | anything below a confidence floor, or with no genre signal at all | never silently mis-bucketed |

The 8th bucket is not decorative: `infer_genre`'s own confidence ranges from 0.10
(weak fallback) to 0.80 (waltz), and the cascade has no branch at all for
reggae/country/house/tango/march (§1.3) — a fifth to a quarter of the corpus will
legitimately not have a confident family assignment on day one, and showing that
honestly (a filterable "needs review" state, not a forced guess) is the difference
between a taxonomy and a lie players will notice the first time they play a
"bossa" that's actually a mislabeled reggae.

**How the family gets populated — options, not one bluffed answer:**

- **Option 1 — filename-token lookup.** Deterministic, cheap, SHIPPABLE (host-only,
  no dependency): extend the token vocabulary already measured in §1.4/§6.1 into a
  lookup table, matched against the basename. Covers ~46% of files (454/1010)
  immediately with high confidence (a file named `Club Bossa.STY` really is
  intended as a bossa by whoever authored/named it). **Not currently built** — no
  code anywhere reads a filename for genre; `infer_genre` is content-only.
- **Option 2 — content inference (`genre.cpp`, already exists).** HOST-ONLY, real
  code, but narrow vocabulary (§1.3) and needs its cascade widened (add
  reggae/country/house/latin-subtype/tango/march branches) before it can populate
  the fuller family tree with confidence. Real, scoped engineering work — cite it
  as such, not "just call the function."
- **Option 3 — hybrid (recommended).** Filename token when present (fast, high-
  confidence path) → content inference as fallback when absent → confidence always
  carried into the UI (dim/italic "guessed: bossa · 68%" treatment, distinct from a
  filename-confirmed "bossa") → "needs review" bucket for anything under a
  confidence floor (e.g. <0.5) or fully unknown. Mirrors the parallel A/B/C
  structure the style-corpus doc already uses for the stylizer (§7.2) — this
  proposal reuses that pattern for the taxonomy fork rather than inventing a new one.
- **Option 4 — manual override table (always needed, regardless of 1-3).** A tiny
  `name → family` override map for the tail of misclassifications every automatic
  classifier will produce. Cheap once the metadata store exists; this is not
  optional infrastructure, it's the safety valve for a guess-based taxonomy.

### 2.2 Secondary axes — what's derivable today vs what needs new work

| Axis | Status | Evidence |
|---|---|---|
| **Tempo band** | SHIPPABLE metadata today | `tempo_milli_bpm` already captured off the MIDI header into `StyleModel` (`model.hpp:133`, confirmed in `serialize.cpp:78`). Bucket into bands (e.g. Ballad <90, Mid 90-120, Up 120-150, Fast 150+) mirroring the measured tempo table already in `docs/style-corpus-and-generation.md` §6.1 (Bossa 120, Samba 114, Funk 105, Disco 140, Swing 202, Waltz 90 …). |
| **Time signature** | SHIPPABLE metadata today | `time_sig_num/den` already captured (`model.hpp:134-135`, `serialize.cpp:79-80`). Overwhelmingly 4/4; 3/4 waltz/musette; 6/8 some ballads/marches (measured, §6.1). |
| **Feel (straight/swung)** | ONE FIELD AWAY, not built | `genre.cpp`'s cascade already computes `offbeat_mean`/`swing_feel` internally (`genre.cpp:41-44,143`) but discards it — `GenreGuess` never exposes it and `StyleModel` has no field to receive it. Small, real plumbing: expose the swing estimate, add a `feel` (or reuse the `groove` idea from the runtime `Style`) field to the interchange model, serialize it. HOST-ONLY, no dependency, but genuinely not built — do not claim it's free. |
| **"Energy"** | NOT DERIVABLE by anything in the tree | No code anywhere computes a density/velocity aggregate across a style's sections. Would need a new heuristic (e.g. mean onset count + mean velocity per Main A→D, normalized) — genuine new engineering, and the FORMULA itself is an open musical judgment call, not something to invent here. Flag as its own decision, not assume a definition. |
| **Provenance/pack** | FREE, already on disk | Directory names under `extra/<PACK>/` (PSR-S950-XX, PA600StylesPack, Indian-part-N, Italian-styles, korg-pa800-greek-01, …) are a zero-cost tertiary tag — exactly what would populate the "World/Regional" family bucket precisely, since rhythm-family clustering alone can't distinguish "Greek" from "Italian" from "Indian" the way a pack name can. |
| **Era** | Filename-hinted, unparsed | `discovery-report.md` §8 notes "50s"/"60s"/"70s" tokens in filenames, not parsed into a field. Cheap regex extraction alongside the genre-token parse (§2.1 Option 1); low priority, optional tertiary tag. |

---

## 3. Browser UX plan at 1010-scale

### 3.1 Precedent (researched, not invented)

- **Yamaha Genos**: 15 hardware style categories + a magnifying-glass search (type a
  term like "ballad", matching styles pop up); the older **Music Finder**
  (Tyros-era) is the closer precedent for *filtering*: a sortable/filterable table
  with exactly the column set **Style / Beat (time signature) / Tempo / Genre** —
  the same four axes this proposal lands on independently (genre family, time
  signature, tempo, plus feel as this corpus's addition, since Music Finder's
  records were per-song not per-style-file). ([How to instantly find any style —
  ePianos](https://www.epianos.co.uk/how-to-instantly-find-any-style-voice-or-song-on-your-yamaha-genos-or-sx-keyboard/),
  [Understanding the Music Finder — psrtutorial](https://psrtutorial.com/lessons/playing/mf/10_Intro.html))
- **Korg Pa5X**: styles reorganized as one-file-per-style (`.STG` inside a `.KST`
  set) rather than monolithic banks, plus **User/Favorite/Direct** banks and a
  search function — the "Favorites" bank is worth borrowing directly: at 1010
  entries, a small user-curated shortlist matters more than at 16.
  ([Pa5X product page — Korg](https://www.korg.com/us/products/synthesizers/pa5x/))

### 3.2 Concrete plan

**Data model (`browser_model.hpp/cpp`, host-only, no core include — D38 stands).**
Replace the constexpr 16-name array with a `std::vector<StyleEntry>` where each
entry carries: `name`, `family` (enum, §2.1), `genre_tag` (string, may be empty),
`genre_confidence` (float, 0 = unknown), `tempo_bpm`, `time_sig_num/den`, `feel`
(enum: straight/swung/unknown), `pack` (string, provenance). Populated at
connect/import time from a **new** bulk metadata source — this is new host-side
scope, not a GUI-only change, and depends on a decision this proposal does not make
(§4): either (a) extend the wire protocol with a bulk `style list-meta` verb
returning one JSONL row per style, or (b) have the GUI read a host-side corpus
manifest file directly (if D44's Layer B blob format lands with a discoverable
index) rather than round-tripping 1010 rows over the socket. Both are legitimate,
neither is decided, and the browser's population code is the wrong place to decide
it silently.

**Search.** Keep the existing shape (`to_lower` + `std::string::find`,
`browser_model.cpp:10-15`), just broaden the haystack from `name` alone to
`name + genre_tag + family label + pack` — a small, low-risk extension of code that
already exists, not a rewrite.

**Filter chips.** One active predicate per secondary axis (family multi-select,
tempo-band single-select, time-sig single-select, feel single-select), ANDed with
the free-text search — the same shape as Music Finder's BEAT/GENRE/TEMPO fields
(§3.1), implemented as a small filter-state struct + predicate function parallel to
today's `style_matches_filter`.

**Grouping — flat lists per family, NOT nested trees.** This is the one place I
diverge from today's `TreeNodeEx`-per-section pattern
(`browser_panel.cpp::section_header`) on purpose: the vendored ImGui's own demo
states plainly, **"Using ImGuiListClipper with trees is a less easy than on arrays
or grids"** (`third_party/imgui/imgui_demo.cpp:4200`) — a real, cited engineering
cost, not a guess. Rendering each family as its own always-visible header followed
by a **flat, clipped list of leaves** (rather than a collapsible tree per family)
keeps the clipper usage in ImGui's easy, well-supported "array/grid" case. Families
default to **collapsed** except the currently-active style's family (an explicit
implementation choice, low-stakes, not escalated).

**Virtualization.** `ImGuiListClipper` over each family's filtered leaf list —
vendored today (`third_party/imgui/imgui.cpp`), confirmed used nowhere yet in
gui-sonotron, so this is genuinely zero new dependency, not a "should be fine"
guess. `imgui_demo.cpp` demonstrates the exact idiom at lines 2882/3007/3367/6381/
7339/7576/7840/9446/9715 — a load-bearing, already-in-tree pattern.

**Alternative shape, worth naming.** Instead of family-grouped flat lists, a single
`ImGuiTable` (Name / Family / Tempo / TimeSig / Feel columns, sortable, one clipped
list total) is ImGui's more conventional "big searchable list" idiom and would let
a player sort by tempo or feel directly rather than only filter. It loses the
"browse by genre bucket" affordance the family tree gives. Both are SHIPPABLE
(host-only, zero dependency) — this is a genuine UX fork for the owner, not a
technical one (§4).

### 3.3 What this plan does NOT touch

Per the `harmony-progression-architecture` memory: default chord progressions stay
a host-supplied `ChordSequence`, never a `StyleDef`/taxonomy field — nothing in
this proposal adds a progression to the style metadata; family/tempo/feel/pack are
orthogonal to harmony and stay that way.

---

## 4. What needs deciding / what I flagged

1. **Metadata transport: wire query vs local manifest.** The browser's population
   code depends on whether imported-style metadata arrives over L0/L1 (a new bulk
   verb) or is read from a host-side corpus manifest file. Neither exists; this is
   an owner/Giotto-lane decision, not something I resolve here.
2. **Grouped-flat-list vs sortable-table UX** (§3.2, "alternative shape") — a real
   product fork (browse-by-family vs sort-by-any-column), not a technical one.
3. **Genre classifier scope**: is widening `genre.cpp`'s cascade (adding reggae/
   country/house/latin-subtype/tango/march branches) in scope now, or does the
   corpus ship first with filename-token-only classification plus a large "needs
   review" bucket? A real product-timeline fork, not mine to call.
4. **"Energy" axis**: not derivable by anything in the tree today; whether it's
   worth building, and what formula would define it, is an open musical judgment
   call I explicitly did not invent an answer for.
5. **Dependency status: none required.** Every technique in this plan
   (`ImGuiListClipper`, `std::vector`-based host model, filter predicates, JSONL
   bulk verb) is either already vendored or fits the existing host-only, no-heap-
   is-not-required-outside-core regime. No new dependency is proposed; nothing to
   flag on that front beyond the transport-choice fork in item 1.
6. **This whole browser change is gated behind the importer actually reaching a
   runtime-loadable state** (§1.5) — I flag, not fix, that the "1010 styles
   browsable" premise assumes work (`9420`/`9430`/D44 Layer B) that is `◑ partial`/
   `○ planned`, not done.

---

## Sources

- [How to Instantly Find Any Style, Voice or Song on Your Yamaha Genos or SX Keyboard — ePianos](https://www.epianos.co.uk/how-to-instantly-find-any-style-voice-or-song-on-your-yamaha-genos-or-sx-keyboard/)
- [Understanding the Music Finder — psrtutorial](https://psrtutorial.com/lessons/playing/mf/10_Intro.html)
- [Searching the MFD — psrtutorial](https://psrtutorial.com/lessons/playing/mf/70_Searching.html)
- [Korg Pa5X — Professional Arranger](https://www.korg.com/us/products/synthesizers/pa5x/)
- [Pa5X Video Manual Part 4: Styles — ePianos TV](https://www.epianos.co.uk/tv/pa5x-video-manual-part-4-styles/)
