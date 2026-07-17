roadmap task #26 — Rename proposal: retire the "v02" version-numbered identifiers
=================================================================================

Status: PROPOSAL (Palladio, structure-steward pass, 2026-07-17). Read-only audit —
no source file touched. This is the "audit + propose" step; a separate mechanical
pass (Nazzareno / Taddeo / Figaro class) executes it after owner approval.

Trigger: owner, on seeing `apps/gui-sonotron/src/v02_state.hpp` — "v02????" — dislikes
source identifiers/filenames with a design/version number baked in. See memory
`no-version-numbered-source-names.md`. **This is the standing end-of-work audit the
memory entry calls for.**

---

## 0. Ground truth: what "v02" actually is

`v02` is **not our version tag** — it is the literal name of an external DesignSync
design deliverable the GUI was built to match: `Sonotron v02 Workstation.dc.html`
(project `bb28c6bd-41f4-4fd5-9f0a-78cdf1e0249f`, see memory
`v02-design-groundtruth-designsync.md`). Our own code then reused that external name
as a prefix for local identifiers (`V02State`, `V02Row`, `V02Part`, `kV02TrackColor`,
`v02_state.hpp`, `docs/v02-feature-list.md`). That is exactly the "ages badly, reads
as accidental" pattern the owner flagged — the fix is to rename **our own**
identifiers to describe what they ARE, while leaving citations of the **external**
artifact's own name (e.g. `Sonotron v02 Workstation.dc.html`,
`design-ref-v02/v02-workstation-spec.md`) verbatim where they appear as a citation —
rewriting a citation to an external file's actual name would be a false trail, not a
cleanup.

---

## 1. Full inventory (real tree only — worktrees excluded, see §6)

`grep -rlI 'v02\|V02'` under `apps/gui-sonotron/` and `docs/` (excluding
`.claude/worktrees/*`, which are stale copies of the same tree, not independent
targets) returns **48 files**, 282 case-insensitive hits. Breakdown by kind:

### 1a. The file that names the concept
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/apps/gui-sonotron/src/v02_state.hpp` — defines `struct V02State` (lines 19–153, namespace `sonotron`).
- `/var/home/crsn/Condos/fedora-strudel/projects/sonotron/apps/gui-sonotron/docs/v02-feature-list.md` — "living checklist" doc (its own line 1: `# Sonotron v02 Workstation — feature list`).

### 1b. Every file that names `V02State` as a type (verified via `grep -rlc 'V02State'`)
Headers that declare functions taking `V02State&`/`const V02State&` (all `#include
"v02_state.hpp"`):
- `apps/gui-sonotron/src/browser_panel.hpp:6,8,18,29`
- `apps/gui-sonotron/src/grid_panel.hpp:8,10,11,31`
- `apps/gui-sonotron/src/intention_panel.hpp:4,6,7,16,18,20`
- `apps/gui-sonotron/src/parts_panel.hpp:3,5,6,12,15`
- `apps/gui-sonotron/src/seqedit_panel.hpp:4,6,7,14,18`
- `apps/gui-sonotron/src/transport_panel.hpp:5,7,8,20,21`
- `apps/gui-sonotron/src/workstation_state.hpp:9,26` (`V02State& fx;` field — see §2 collision analysis)

Implementation files that use `V02State` (no include of their own — they get it
transitively via their matching `.hpp`, except `app_state.cpp`/`layout_renderer.cpp`
which reach it through `workstation_state.hpp`/panel headers):
- `apps/gui-sonotron/main.cpp:815,836,840,841,863,875` (constructs the owned instance, local variable literally named `v02_state`)
- `apps/gui-sonotron/src/app_state.cpp:124` (comment only)
- `apps/gui-sonotron/src/browser_panel.cpp:21,25,96,203`
- `apps/gui-sonotron/src/grid_model.hpp:215` (comment only)
- `apps/gui-sonotron/src/grid_panel.cpp:117–1114` (13 sites: type usage + local struct `V02Row`, see §1c)
- `apps/gui-sonotron/src/intention_panel.cpp:37`
- `apps/gui-sonotron/src/layout_renderer.cpp:17,18,21,44,78,79`
- `apps/gui-sonotron/src/neon_widgets.hpp:8,9,17` (comment only — the header itself takes no `V02State`, just documents the flag it mirrors)
- `apps/gui-sonotron/src/parts_panel.cpp:15,17,24,30,43` (type usage + local struct `V02Part`, see §1c)
- `apps/gui-sonotron/src/seqedit_panel.cpp:85,88,89`
- `apps/gui-sonotron/src/theme.cpp:21` (comment only)
- `apps/gui-sonotron/src/theme.hpp:28,29,30,58,65,88,89,91` (comment + `kV02TrackColor`, see §1c)
- `apps/gui-sonotron/src/transport_panel.cpp:16,54,62,79`

Tests (all `#include "src/v02_state.hpp"` + `using sonotron::V02State;` + local
`V02State fx;` instances — 16 files):
- `apps/gui-sonotron/tests/test_browser_style_switch_while_playing_ui_automation.cpp`
- `apps/gui-sonotron/tests/test_grid_cell_launch_open_seqedit_ui_automation.cpp`
- `apps/gui-sonotron/tests/test_grid_cell_preview_vs_seqedit_ui_automation.cpp`
- `apps/gui-sonotron/tests/test_grid_ms_latch_ui_automation.cpp`
- `apps/gui-sonotron/tests/test_grid_panel_auto_song.cpp`
- `apps/gui-sonotron/tests/test_grid_panel_auto_song_launches_next_scene.cpp`
- `apps/gui-sonotron/tests/test_grid_panel_auto_song_real_backend.cpp`
- `apps/gui-sonotron/tests/test_grid_panel_auto_song_stop_restart_ui_automation.cpp`
- `apps/gui-sonotron/tests/test_grid_panel_master_play_restart_rewind_ui_automation.cpp`
- `apps/gui-sonotron/tests/test_repeat_zone_owner_trace_replay.cpp`
- `apps/gui-sonotron/tests/test_repeat_zone_playhead_ui_automation.cpp`
- `apps/gui-sonotron/tests/test_repeat_zone_scene_header_next_bar_ui_automation.cpp`
- `apps/gui-sonotron/tests/test_transport_ending_button_ui_automation.cpp`
- `apps/gui-sonotron/tests/test_transport_play_stop_ui_automation.cpp`
- `apps/gui-sonotron/tests/imgui_headless_harness.hpp:8,17,56` (comment only)
- `apps/gui-sonotron/tests/CMakeLists.txt:228` (comment only)

Note: **none** of the test *filenames* carry "v02" — only their bodies (`#include
"src/v02_state.hpp"`, `using sonotron::V02State`, local `V02State fx;`). No test
executable name, no `CMakeLists.txt` target/source-list entry needs to change.

### 1c. File-local secondary identifiers riding on the same prefix
- `struct V02Row` — `apps/gui-sonotron/src/grid_panel.cpp:126` (anonymous namespace, only used at lines 131, 664, 949, 1003, 1097 **within the same file**).
- `struct V02Part` — `apps/gui-sonotron/src/parts_panel.cpp:17` (anonymous namespace, only used at lines 30, 43 **within the same file**).
- `kV02TrackColor` — `apps/gui-sonotron/src/theme.hpp:91` (`namespace theme`, `inline constexpr std::array<ImVec4, 6>`). Referenced across **7 files**: `theme.hpp` (def), `grid_panel.cpp:1098`, `seqedit_panel.cpp:88-89`, `test_grid_cell_launch_open_seqedit_ui_automation.cpp:122`, `test_grid_cell_preview_vs_seqedit_ui_automation.cpp:196,255`, `test_repeat_zone_owner_trace_replay.cpp:175,180`, `test_repeat_zone_playhead_ui_automation.cpp:186,192,194`, `test_repeat_zone_scene_header_next_bar_ui_automation.cpp:192,205`.

### 1d. Prose-only mentions ("the v02 redesign", "v02 neon", etc.) — no identifier
`browser_panel.cpp:21`, `grid_model.hpp:215`, `layout_renderer.cpp:17,18,21,78`,
`neon_widgets.hpp:8,9`, `theme.cpp:21`, `theme.hpp:28,29,30,58,65,88,89`,
`docs/gui-and-ux.md:519`. Cosmetic — see §4 batch D.

### 1e. Doc-to-doc citations of the path `docs/v02-feature-list.md`
- `apps/gui-sonotron/src/intention_panel.hpp:18`
- `apps/gui-sonotron/src/browser_panel.cpp:25`
- `docs/strategy/garageband-ipad-feature-map.md:46,72,99,117` (a live strategy doc — these citations must stay resolvable)

### 1f. Historical/closed-issue narrative docs that cite `V02State`/`v02_state.hpp` in past tense
`docs/proposals/flow-verification-matrix-2026-07.md:140`,
`docs/proposals/green-tests-broken-app-gate.md:31,48,66,199,295`,
`docs/proposals/looper-in-gui-contract.md:331,333,334,373`,
`docs/proposals/repeat-zone-real-contract.md:184,443`,
`docs/proposals/song-form-autoarrange.md:121`,
`docs/proposals/song-form-option-a-wiring-plan.md:245,250,439,458,549`,
`docs/proposals/ui-animation-roadmap.md:66,133`,
`docs/proposals/ui-motion-extreme-2026-07.md:23,73,74,86,135,229,388,599,600` — see §4 batch E.

---

## 2. Collision analysis: `V02State` vs. `apps/gui-sonotron/src/workstation_state.hpp`

Investigated explicitly per the task brief. **Verdict: NOT a collision, NOT a merge
candidate — they are two different architectural roles and must stay two types.**

`apps/gui-sonotron/src/workstation_state.hpp:19-27`:
```cpp
struct WorkstationState {
  AppState& app_state;
  BrainSession& brain_session;
  BrowserModel& browser;
  GridModel& grid;
  SeqEditModel& seqedit;
  PartsModel& parts;
  V02State& fx;  // v02 redesign: glow flag, frame clock, local intent surface
};
```
`WorkstationState` is a **pure aggregate of references** — a per-frame parameter
bundle, all fields mandatory, no defaults, no owned data of its own. It exists solely
to carry the frame's models through `render_layout`/`render_rail`
(`layout_renderer.cpp:66,77`), which immediately unpacks it back into bare references
(`state.fx`, `state.app_state`, …) before calling each `render_*_panel` — confirmed at
`layout_renderer.cpp:67,71,72,79,81,95,101,106,117`. Every panel function takes
`V02State& fx` **directly**, not `WorkstationState&`.

`V02State`, by contrast, is an **owned, default-initialized model** — 20+ data members
with real defaults (`glow = true`, `cell_zoom = 52.0F`, `auto_song = true`, …),
constructed exactly once in `main.cpp:836` (`sonotron::V02State v02_state;`) and
threaded everywhere else by reference. It is the same category as `AppState`,
`GridModel`, `PartsModel`, `SeqEditModel` — one of the "zone panel models" the
`WorkstationState` bundle happens to also carry a reference to.

Renaming `V02State` to `WorkstationState` (or anything that reads as "the" workstation
state) would be a real clobber: `workstation_state.hpp` already owns that exact name
for a semantically different thing (bundle-of-refs vs. owned-model). **This proposal
does not do that.** The chosen replacement name (§5) keeps `Workstation` only as a
qualifying prefix, mirroring how `WorkstationState.fx` already *contains* it — i.e.
"the UI-state that belongs to the workstation", not "the workstation's overall state".

---

## 3. Diagnosi di collocazione (fuori scope, per completezza)

Non ho trovato file nella directory sbagliata da questo audit: `v02_state.hpp` vive
correttamente in `apps/gui-sonotron/src/` accanto agli altri modelli host-only
(`app_state.hpp`, `grid_model.hpp`, …) — nessun file core/firmware coinvolto, nessuna
violazione dual-target. Questo è un audit di **naming**, non di collocazione
fisica: nessuno spostamento di directory è proposto, solo rename in loco + edit dei
riferimenti.

---

## 4. Piano di rename — batch ORDINATI

Tutti i batch operano SOLO dentro `apps/gui-sonotron/` e `docs/` (albero reale, non
worktree — vedi §6). Ordine pensato perché l'albero non passi mai per uno stato rotto:
rinominare prima il file (nessun altro file lo referenzia per contenuto, solo per
`#include`), poi il simbolo (find-replace testuale, il compilatore lo verifica),
poi le prose facoltative.

### Batch A — file rename + include-path fixup (NEEDS-BUILD-EDIT, ma puramente meccanico)
1. `git mv apps/gui-sonotron/src/v02_state.hpp apps/gui-sonotron/src/workstation_ui_state.hpp`
   (nome scelto in §5).
2. Aggiornare **ogni** `#include "v02_state.hpp"` → `#include "workstation_ui_state.hpp"`
   nei 7 header: `browser_panel.hpp:6`, `grid_panel.hpp:8`, `intention_panel.hpp:4`,
   `parts_panel.hpp:3`, `seqedit_panel.hpp:4`, `transport_panel.hpp:5`,
   `workstation_state.hpp:9`.
3. Aggiornare **ogni** `#include "src/v02_state.hpp"` → `#include "src/workstation_ui_state.hpp"`
   nei 14 test file elencati in §1b (i due che includono `imgui_headless_harness.hpp`
   non includono direttamente v02_state.hpp — verificare comunque a compile time).
4. CMake: nessuna riga da toccare — l'header non è mai listato esplicitamente in
   `apps/gui-sonotron/CMakeLists.txt` (verificato, nessun match su `\.hpp` nel file) né
   in `tests/CMakeLists.txt` (i target referenziano solo i `.cpp` dei test, non
   `v02_state.hpp`).
   Label: **SAFE** una volta fatto lo step 2/3 (il build fallisce rumorosamente se un
   include è dimenticato — nessun rischio di "rottura silenziosa").

### Batch B — symbol rename `V02State` → `WorkstationUiState` (SAFE, find-replace esatto)
Sostituzione testuale del token esatto `V02State` (case-sensitive, nessuna sottostringa
ambigua trovata — non esiste altro identificatore che contenga `V02State` come
sottostringa) in tutti i 27 file di §1b, incluso il rename del campo-non-di-tipo:
`workstation_state.hpp:26`'s commento (`// v02 redesign: glow flag, frame clock, local
intent surface` — testo libero, va comunque aggiornato per onestà, vedi batch D).
Include anche la variabile locale `main.cpp:836,840,841,863,875` (`v02_state` →
`ui_state`, l'unica occorrenza del nome-variabile minuscolo con "v02"; il parametro
`fx` usato ovunque negli altri file resta invariato, è fuori scope — vedi §5 nota).

Ordine interno: fare batch A e B nello stesso commit (il file rinominato e il tipo
rinominato devono essere coerenti, altrimenti il build non parte tra i due passi).

### Batch C — identificatori file-locali satellite (SAFE, singolo file ciascuno)
- `grid_panel.cpp:126` `struct V02Row` → `struct LaunchRow` (coerente con "launch-grid"
  già usato ovunque nei commenti dello stesso file, es. riga 117, 124). Solo
  `grid_panel.cpp` tocco (nessun altro file referenzia `V02Row`).
- `parts_panel.cpp:17` `struct V02Part` → `struct PartAmountKnob` (descrive cosa
  contiene: label + colore per un knob "amount"). Solo `parts_panel.cpp` tocco.
- `theme.hpp:91` `kV02TrackColor` → `kLaunchGridTrackColor` (si distingue da
  `kRoleTint`, riga 84, che è il ramp completo a 9 ruoli; questo è il subset a 6 righe
  della launch-grid). Tocca 7 file: `theme.hpp` (def) + `grid_panel.cpp:1098` +
  `seqedit_panel.cpp:88-89` + i 4 test file elencati in §1c. Find-replace esatto,
  nessuna ambiguità di sottostringa.

### Batch D — prose facoltativa nei commenti del codice (JUDGMENT, cosmetico, zero impatto sul build)
Softening facoltativo di frasi come "v02 redesign" → "the neon workstation redesign",
"v02 neon" → "the neon palette", "v02 launch grid" → "the launch grid", nei punti
elencati in §1d. **Non tocca identificatori**, quindi zero rischio di rottura; è
puramente una questione di gusto/onestà storica. **ECCEZIONE — NON toccare**: ogni
citazione letterale del deliverable DesignSync esterno (`Sonotron v02 Workstation.dc.html`,
`design-ref-v02/v02-workstation-spec.md`, `v02-workstation-spec.md §…`) — quello è il
nome REALE di un file esterno che non ci appartiene rinominare; riscriverlo
romperebbe la tracciabilità verso il design originale. Questi appaiono in:
`browser_panel.cpp:25` (testo `docs/v02-feature-list` — path nostro, va aggiornato,
non è il caso esterno), `grid_panel.cpp:117,169`, `intention_panel.hpp:6,7`,
`layout_renderer.cpp:17,18`, `neon_widgets.hpp:9`, `parts_panel.hpp:6`,
`seqedit_panel.hpp:6`, `theme.hpp:89`, `transport_panel.hpp:7`,
`docs/v02-feature-list.md:4-5` stessa — lasciare la citazione `v02-workstation-spec.md`
verbatim, softare solo il resto della frase.

### Batch E — rename del doc "vivo" + fixup delle sue citazioni (NEEDS build-doc edit, mechanical)
1. `git mv apps/gui-sonotron/docs/v02-feature-list.md apps/gui-sonotron/docs/workstation-feature-list.md`
   (è la "living checklist", non un incidente chiuso — va rinominato insieme al codice).
2. Aggiornare il titolo interno (riga 1: `# Sonotron v02 Workstation — feature list` →
   `# Sonotron Workstation — feature list`, riga 3-5 idem) MA lasciare intatta la
   citazione `design-ref-v02/v02-workstation-spec.md` (nome del file esterno, batch D).
3. Aggiornare i 2 riferimenti-codice al vecchio path:
   `intention_panel.hpp:18` (`docs/v02-feature-list.md` → `docs/workstation-feature-list.md`),
   `browser_panel.cpp:25` (idem).
4. Aggiornare le 4 citazioni in `docs/strategy/garageband-ipad-feature-map.md:46,72,99,117`
   (doc vivo, le citazioni rotte sarebbero un dead-link reale per chi la legge dopo).
   Label: **SAFE** (pure path-string edits, nessun codice compilato coinvolto).

### Batch F — narrativa storica nei doc chiusi (NEEDS-DECISION, opzionale, bassa priorità)
`docs/proposals/flow-verification-matrix-2026-07.md`, `green-tests-broken-app-gate.md`,
`looper-in-gui-contract.md`, `repeat-zone-real-contract.md`, `song-form-autoarrange.md`,
`song-form-option-a-wiring-plan.md`, `ui-animation-roadmap.md`,
`ui-motion-extreme-2026-07.md` citano `V02State`/`v02_state.hpp` come narrativa di un
lavoro già chiuso (con numeri di riga specifici, es.
`song-form-option-a-wiring-plan.md:439` cita `v02_state.hpp:123-129`). **Non le
tocco per default**: sono log storici di decisioni passate, e riscriverli
retroattivamente per far tornare i nomi rischia di falsificare "cosa si vedeva
quando quella decisione fu presa". **Decisione del proprietario richiesta**: se si
preferisce coerenza forward-looking anche qui, è un find-replace testuale sicuro
(nessun impatto sul build, sono solo file `.md`) ma cambia riferimenti a righe di
codice che dopo batch A/B/C puntano comunque al file giusto sotto il nuovo nome — le
citazioni di NUMERO di riga restano valide (il rename non sposta le righe), solo il
NOME del file/tipo citato andrebbe aggiornato.

---

## 5. Nome raccomandato

**Tipo**: `V02State` → **`WorkstationUiState`**
**File**: `apps/gui-sonotron/src/v02_state.hpp` → **`apps/gui-sonotron/src/workstation_ui_state.hpp`**
**Doc**: `apps/gui-sonotron/docs/v02-feature-list.md` → **`apps/gui-sonotron/docs/workstation-feature-list.md`**

Motivazione:
- Rispetta la convenzione già in uso ovunque nell'albero: `AppState`→`app_state.hpp`,
  `GridModel`→`grid_model.hpp`, `BrowserModel`→`browser_model.hpp`,
  `WorkstationState`→`workstation_state.hpp` — il filename è sempre lo
  snake_case ESATTO del type name. `WorkstationUiState`→`workstation_ui_state.hpp`
  segue la stessa regola (la memoria del proprietario suggeriva `gui_ui_state.hpp`
  per lo stesso tipo: preferisco `workstation_ui_state.hpp` per restare aderente
  alla convenzione filename==type già stabilita, ma è una scelta cosmetica minore
  che il proprietario può correggere in fase di approvazione).
- Descrive cosa il tipo realmente è (per il proprio commento di testa,
  `v02_state.hpp:7-15`): "client-side, engine-free UI state" del workstation —
  glow flag, frame clock, intent surface locale, selezione/rename, bookkeeping
  auto-song. `WorkstationUiState` lo dice senza numero di versione.
  `energy/tension/valence` restano "UI"/"intent" honestamente locali (nessun verbo
  wire), quindi "Ui" nel nome non è fuorviante.
- **Niente collisione** con `WorkstationState` (§2): sono ruoli diversi
  (bundle-di-riferimenti vs. modello posseduto), e il prefisso condiviso
  "Workstation" è semanticamente corretto — `WorkstationState.fx` è, letteralmente,
  la `WorkstationUiState` del workstation corrente. Segnalo comunque la vicinanza
  di nome per un ultimo sguardo del proprietario (due tipi che iniziano entrambi
  con "Workstation", inclusi nello stesso file `workstation_state.hpp`) prima di
  eseguire — è una lettura leggermente più densa alla prima occhiata, anche se
  tecnicamente inequivocabile.
- Alternativa più corta se si preferisce sganciare il nome da "Workstation":
  `UiState` / `ui_state.hpp`. Più snello, ma più generico — rischia ambiguità
  futura se un giorno esistesse un secondo "UI state" per un'altra superficie
  (es. una futura TUI). Non la scarto, la segnalo come opzione B.

Il campo/parametro minuscolo `fx` (usato letteralmente ovunque:
`V02State& fx`, `state.fx`, `fx.glow`, …) **resta invariato** — non contiene "v02",
non è nello scope di questo task, e rinominarlo sarebbe una modifica di scala molto
più larga (100+ siti) per zero guadagno rispetto all'obiettivo (eliminare i nomi
version-numbered). Lo segnalo solo per completezza.

---

## 6. Nota sui worktree

Lo stesso pattern "v02" esiste identico dentro
`.claude/worktrees/{agent-*,gui-live}/apps/gui-sonotron/...` (copie di lavoro dello
stesso albero, non target indipendenti — confermato dal contenuto identico). Non le
includo nel piano: sono worktree di lavoro che convergeranno o verranno scartate
indipendentemente; il rename va eseguito una volta sul ramo principale/di lavoro
corrente e poi normalmente ereditato/rebasato dai worktree attivi, non duplicato a
mano in ognuno.

---

## 7. Cosa NON è stato toccato (fuori mandato)

Nessun file spostato, nessun simbolo rinominato, nessun contenuto editato: questo è
un audit read-only. L'unico file scritto è questo stesso documento
(`docs/proposals/rename-v02-identifiers.md`). Nessuna domanda di confine
logico/architetturale è emersa (nessun coupling, nessuna ABI, nessuna scelta di
astrazione in gioco) — è puro naming/placement, quindi resta interamente di mia
competenza, nessun hand-off a Corelli necessario.
