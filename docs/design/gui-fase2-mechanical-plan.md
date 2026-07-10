# GUI Fase 2 — piano d'esecuzione dello strand *mechanical*

Status: **piano, non ancora eseguito.** Deriva da `ux-workstation.md` §13 (salvage/rebuild map) e
ne implementa la sola parte **senza dipendenze dal core** (lo strand GUI mechanical). Lo strand core
(§11: `kChordFollowed`, `kBeat`, primitiva clip) è **fuori da questo piano** e verrà sequenziato dopo.

## Context

Il pivot del 2026-07-10 (`ux-workstation.md`, node 11600) rende autoritativa la *workstation screen*
e supera la shell "conductor dashboard" attuale. La GUI oggi (`apps/gui-sonotron/`) è una shell mock:
layout engine a righe singole + due zone concept (`arrangement*`, `intention*`) con dati finti,
nessuna connessione al brain, 3 zone su 5 vuote. Questo piano riusa il guscio meccanico e **butta il
contenuto concept**, ricentrando lo schermo sulle 6 zone della workstation — restando entro il
contratto *già shippato* (`gui-contract-map.md`: L1 testo in / JSONL out), senza toccare la ABI
congelata e senza attendere lavoro core.

Decisione d'ingresso (owner, 2026-07-10): **GUI mechanical prima**, poi core P0, poi clip primitive.

## Vincoli invarianti

- Solo `*_panel.cpp` e `layout_renderer.cpp` includono ImGui; i *model* sono dati puri e testabili
  host-only (harness `tests/test.hpp`, macro `CHECK`).
- `test_abi_frozen` non è toccato (nessuna modifica ABI in questo strand).
- Nessuna nuova dipendenza host/core (lo spike non ne introduce: solo POSIX + STL).

## Salvataggio dallo spike `spikes/gui-skeleton`

Lo spike contiene codice già scritto e testato, da **portare** (namespace `arrangrr::gui` →
`sonotron`, path `app/gui/` → `apps/gui-sonotron/src/`):

| File spike | Destinazione | Ruolo |
|---|---|---|
| `wire.{hpp,cpp}` | `brain_event.{hpp,cpp}` | `LineBuffer` (framing) + parser JSON flat + decoder `Event` (POD) |
| `uds_client.{hpp,cpp}` | transport dentro `uds_brain_session.{hpp,cpp}` | client AF_UNIX non-bloccante, `send_line`/`poll_lines`, mai `quit` |
| `app_state.{hpp,cpp}` | `app_state.{hpp,cpp}` | riduzione eventi, gating green-a-riposo/amber |
| `tests/test_wire.cpp`, `tests/test_uds_client.cpp` | `tests/test_brain_event.cpp`, ... | test host-only già pronti |

Delta rispetto allo spike: avvolgere transport+decoder+state dietro l'interfaccia astratta
`BrainSession` (§9 dello spec) così che `InProcessBrainSession` sia sostituibile senza toccare i
pannelli. `send()` mantiene la blacklist `quit`/`exit`.

## Sequenza (slice piccoli, ciascuno compila + test verdi + screenshot)

### G0 — Demolizione concept
- DELETE: `arrangement.{hpp,cpp}`, `arrangement_panel.{hpp,cpp}`, `intention.{hpp,cpp}`,
  `intention_panel.{hpp,cpp}`, `tests/test_arrangement.cpp`, `tests/test_intention.cpp`, i generatori
  `mock_*`.
- `layout_renderer.cpp`: rimuovi i due `case` mock e i relativi include.
- Aggiorna `apps/gui-sonotron/CMakeLists.txt` e `tests/CMakeLists.txt` (liste sorgenti/test).
- DoD: build verde, suite verde (con le zone renderizzate come frame titolati vuoti).

### G1 — Layout engine: split verticale annidato + `visible` + font 13
- `layout_model.{hpp,cpp}`: aggiungi `bool visible = true` a `Zone`; estendi `compute_rows` (oggi
  raggruppa solo per `Zone::row`, righe di livello singolo — vedi `layout_model.cpp:89`) con un
  concetto di **sotto-colonna che impila zone in verticale dentro una cella** (la right-rail:
  `intention` sopra `parts` in col 2). Porta `kDefaultFontSizePx` a **13** (solo il default).
- `layout_json.{hpp,cpp}`: leggi/scrivi `"visible"` e la forma dello split annidato.
- `layout_renderer.cpp`: rendi lo split annidato; salta le zone `!visible`.
- `layout_model.cpp::default_layout()`: riscrivi le 6 zone workstation
  (`transport` full-span row 0; `browser`/`grid`/right-rail su row 1; `seqedit` full-span row 2).
- Test: aggiorna `test_layout_model`, `test_layout_json`, `test_layout_roundtrip`; aggiungi
  `test_layout_nested_split.cpp`.
- DoD: build+suite verdi; screenshot mostra le 6 zone nel layout del wireframe (§3).

### G2 — Brain session (contratto già shippato, nessun lavoro core)
- ADD: `brain_session.hpp` (interfaccia astratta §9), `brain_event.{hpp,cpp}` (porta da `wire.*`),
  `uds_brain_session.{hpp,cpp}` (porta da `uds_client.*` + decoder), `app_state.{hpp,cpp}` (porta).
- Decoder scoped alle **5 shape già shippate** (`midi-out`, `chord`, `section`, `transport`, `warn`);
  i campi additivi (`chord-followed`, `beat`, `clip`) restano stub finché lo strand core li emette.
- `main.cpp`: istanzia `UdsBrainSession` (path da `--control`/preferenze), `poll()` una volta per
  frame, riduci in `AppState`; mostra lo stato connessione nel transport.
- Test: `test_brain_event.cpp`, `test_app_state.cpp` (portati/estesi dallo spike).
- DoD: con un brain in ascolto su UDS, il transport mostra ● Connected e il log eventi scorre.

### G3 — Zone panel (mock/parziali, i campi core-dipendenti restano placeholder)
- ADD coppie model+panel: `transport_panel.*`, `browser_panel.*`+`browser_model.*` (i 16 builtin
  styles come drag-source), `grid_panel.*`+`grid_model.*` (matrice/scene; il *launch* reale aspetta
  la clip primitive del core), `seqedit_panel.*`+`seqedit_model.*`, `parts_panel.*`+`parts_model.*`,
  `intention_panel.*` (nuovo, minimale, read-only).
- `main.cpp`: menu bar (`BeginMainMenuBar` — File/Edit/View/Transport/Help), toggle View→Intention/
  Parts.
- Test: `test_grid_model.cpp` (+ eventuali model test).
- DoD: schermo navigabile end-to-end; playhead/visualizer armonico/launch mostrano placeholder
  onesti in attesa dello strand core.

## Confini con lo strand core (fuori da questo piano)

Restano placeholder finché §11 non atterra: playhead reale (dipende da `kBeat`), visualizer armonico
green/amber vivo (dipende da `kChordFollowed`), launch reale delle celle (dipende dalla primitiva
`clip` + verbi `launch/stop/scene quantize`). I pannelli sono progettati per accendersi quando
l'evento arriva, senza riscrittura.

## Verifica end-to-end

1. `cmake --build` del preset host → verde.
2. `ctest` label `unit` per `apps/gui-sonotron` → verde (model puri + brain_event + app_state).
3. Screenshot headless (`screenshot.*`) dopo G1 e G3 → confronto col wireframe §3.
4. Avvio `arrangrr --control /tmp/son.sock`, poi la GUI: transport ● Connected, log eventi scorre,
   invio `transport start` dal menu → eventi `midi-out` visibili.

## Note

- Lo spec cita `product-identity.md` in `docs/design/`; il file reale è `docs/product-identity.md`
  (riferimento da correggere in un doc-sweep, non bloccante).
- Routing suggerito quando si esegue: `nazzareno` implementa gli slice, `torquato` possiede i test/
  golden e la disciplina red-before-green, `corelli`/`fabrizio` per review architettura/riga.
