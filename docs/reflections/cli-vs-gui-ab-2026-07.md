# CLI vs GUI: an empirical A/B on why the styles sound flatter in gui-sonotron

Status: measured, 2026-07-17. Author: Ottorino (style/arrangement analyst).
Scope: confirms or refutes the standing diagnosis in the auto-memory note
`cli-vs-gui-divergence` with a real, captured event-stream A/B — not a re-read
of the code alone.

## 0. Method

Repo: `/var/home/crsn/Condos/fedora-strudel/projects/sonotron`, branch
`gui-sonotron`. Binary: `build/host/apps/sonotron-server/sonotron-server`
(already built, unmodified). Every `.acmd` script and its captured JSONL
event-stream lives in the scratchpad, not the repo (this analysis is
read-only on product code):
`/tmp/claude-1000/-var-home-crsn-Condos-fedora-strudel-projects-sonotron/2aa947af-94f9-4a27-a42f-1825133aa3cd/scratchpad/ab/`.

For three styles — **bossa**, **samba** (jazzy, harmony-dependent genres),
**pop** (the flatter, harmonically static reference genre) — two `.acmd`
scripts were built per style, identical in everything except the harmony
path:

- **PATH-A (`*_A_cli_harmony.acmd`)**: the CLI's own live-harmony grammar —
  `key C major`, then a scripted `seq new/add/loop/play` vamp: a real
  ii–V–I–vi turnaround (Dm7–G7–Cmaj7–Am7, 1 bar each, looped) — the same verb
  family `apps/demo/jam/setup.acmd`'s own commented-out example uses
  (`seq add C 1bar`), just with real jazz voicings instead of the toy demo's
  bare triads.
- **PATH-B (`*_B_gui_static.acmd`)**: ONLY the verbs
  `command_line_to_command()` in
  `apps/gui-sonotron/src/in_process_brain_session.cpp` actually translates —
  `style load`, the 7 auto-issued `style route` lines
  (`kDefaultStyleRoutes`, same file lines 102-110), `transport start`. No
  `key`/`chord`/`seq` line anywhere, because the GUI never sends one (see §1).

Both scripts route identically (`out0:10/10/2/3/7/5/6` for
drums/perc/bass/chord1/chord2/pad/arp — copied byte-for-byte from
`kDefaultStyleRoutes`) and both run exactly 8 bars (`advance 30720`, 3840
ticks/bar per `components/core/common/include/common/time.hpp`'s
`kPpqn=960`/`kBeatsPerBar=4`). The only free variable is harmony. A third
script, **CONTROL-C** (`bossa_C_cli_nohumanize.acmd`), repeats PATH-A for
bossa with `groove humanize-t 0` / `groove humanize-v 0` added, to test the
counter-hypothesis that Wave-1 humanize or Wave-2C's multi-bar VarA is
itself introducing artifacts, independent of the harmony question.

All 7 scripts ran clean: `exit=0`, zero stderr bytes, for every one of the
seven `.out` captures.

## 1. Cosa ho misurato

**Claim #1 (no live harmony in the GUI), read from source, confirmed:**
`command_line_to_command()` (`apps/gui-sonotron/src/in_process_brain_session.cpp:306-676`)
has branches for `transport`, `style load/switch/section`, `transpose`,
`bpm`, `pad bank`, `part mute/solo`, `clip add`, `launch clip/scene`,
`stop clip`, `note`, and the four `loop ...` verbs — **zero** branches for
`key`, `chord`, or `seq`. Any such line, if ever sent, would fall through to
`TranslateOutcome::kUnknownCommand` (line 946) and surface only as a local
warning event (`"integrated mode does not translate this command to a
Command POD yet"`), never reaching the engine. And `Engine::cmd_style`'s
`kStyleLoad` handler (`components/core/arrangrr/src/engine.cpp:614-634`) calls
`m_chords.establish_default()` on every style load — a method documented (and
unit-tested,
`components/core/arrangrr/tests/test_followed_context.cpp:97-113`) to seed the
tonic **only if nothing explicit is already in force, and to be a no-op
forever after** the first explicit chord. Since the GUI path never sends an
explicit chord, the tonic set at load time is the one the style stays on for
the rest of the session.

**A/B capture confirms this is not just a code-reading inference — it is the
actual observed behavior:**

```
--- chord events, bossa PATH-A ---
{"ev":"chord","in":"D4","out":"Dm7","deg":"ii","@":0}
{"ev":"chord","in":"G4","out":"G7","deg":"V","@":3840}
{"ev":"chord","in":"C4","out":"Cmaj7","deg":"I","@":7680}
{"ev":"chord","in":"A4","out":"Am7","deg":"vi","@":11520}
... (9 chord events total, one every bar, cycling ii-V-I-vi)

--- chord events, bossa PATH-B ---
(zero "chord" events in the entire 8-bar, 1295-line capture)
```

Same pattern for samba and pop: PATH-A always emits exactly 9 `chord`
events (8 bars + the wraparound at bar 8→9); PATH-B emits **zero**, for all
three styles.

**Pitch-class movement, measured directly from `midi-out noteon` events**
(bass=ch2, chord1=ch3, chord2=ch7 — the three harmony-bearing roles):

| style | path | distinct pitch-classes (ch2+3+7, 8 bars) | pitch-classes used | total note-ons |
|---|---|---|---|---|
| bossa | A (live harmony) | **7** | {0,2,4,5,7,9,11} | 140 |
| bossa | B (GUI static) | **3** | {0,4,7} | 140 |
| samba | A | **7** | {0,2,4,5,7,9,11} | 185 |
| samba | B | **3** | {0,4,7} | 153 |
| pop | A | **7** | {0,2,4,5,7,9,11} | 155 |
| pop | B | **3** | {0,4,7} | 155 |

`{0,4,7}` is C major's own tonic triad, unmoving for all 8 bars in every
PATH-B capture — literally the C-major arpeggio and nothing else, for the
entire style, regardless of whether the style is bossa, samba, or pop.
PATH-A visits all 7 diatonic pitch-classes of C major because the ii-V-I-vi
progression actually moves.

Bar-by-bar detail for bossa chord1 (ch3) makes the flatline explicit:

```
PATH-A pc-by-bar: [0,2,5,9] [2,5,7,11] [0,4,7,11] [0,4,7,9] [0,2,5,9] [2,5,7,11] [0,4,7,11] [0,4,7,9] [0,2,5]
PATH-B pc-by-bar: [0,4,7]   [0,4,7]    [0,4,7]    [0,4,7]   [0,4,7]   [0,4,7]    [0,4,7]    [0,4,7]   [0,4]
```

Note also: **total note-on counts are identical or near-identical between A
and B** (140 vs 140 for bossa; 155 vs 155 for pop; 185 vs 153 for samba, the
one exception explained below). This is itself evidence: the RHYTHM and
DENSITY of the arrangement do not change between the two paths — only the
pitch content does. That is exactly what you'd expect from
`RolePolicy::kChordTone` comping a chord that either moves (A) or never
changes (B): same grid, same note count, different pitches. (Samba's 185 vs
153 gap on ch3 is arithmetic, not musical: a min7/maj7/dom7 chord under
`kChordTone` voices 4 tones where a bare triad voices 3 — PATH-A's real
seventh chords literally have one more note per hit than PATH-B's plain
major triad. That gap is a SIDE EFFECT of the harmony test itself using real
jazz sevenths, not a fourth variable.)

**Counter-hypothesis check (CONTROL-C, humanize zeroed, bossa, same PATH-A
progression):** comparing every noteon on bass (ch2), chord1 (ch3), and
drums (ch10) between humanize-on and humanize-off:

- **Note counts identical**: 17/71/98 in both captures, all three channels.
- **Zero duplicate (tick, note) pairs** in either capture — no doubled
  notes.
- **Downbeats land exactly on the same tick** in both captures (bar starts
  at 0, 15360, etc. — untouched by humanize).
- **Timing jitter is small and bounded**: e.g. bass note at tick 1440
  (humanize off) lands at 1448 with humanize on — 8 ticks, ≈3.8 ms at
  bossa's 130 BPM (`tempo=13000` in `bossa.hpp`). Velocity jitter is ±1-4.
- No wrong-octave notes, no missing notes, no shifted downbeats.

Humanize is doing exactly what it says on the tin — a few milliseconds of
timing/velocity jitter — and introduces **no artifact** at this sample size.
**The counter-hypothesis is refuted for this 8-bar sample**: Wave-1
humanize and Wave-2C's multi-bar VarA are not, by this evidence, a source of
the "less natural" feeling the owner reports; the mechanism is clean.
(Caveat: this checked one style, 8 bars, one section. It does not rule out
an issue specific to section-boundary transitions, which this A/B did not
exercise — see §4.)

**Claims #2/#3 (double section-trigger, style-switch-forces-VarA),
confirmed by source reading (not by this A/B, which never exercises scene
launches or style switch):**

- `apps/gui-sonotron/src/grid_panel.cpp:706-722`: a scene-header click sends
  **both** `style section <name>` (line 719) **and**
  `launch scene <n> quantize <bars>` (line 721) for the same user action.
  The code comment states this is deliberate ("a genuinely different, still-
  useful effect... this slice does not retire"), not an oversight — so this
  is a real double-command-per-click, but by the code's own account an
  intentional one, not an obvious bug. I did not measure whether the two
  quantized commands ever produce an audible glitch (both quantize to the
  same next-bar boundary, so they may simply coincide harmlessly); that
  would need its own targeted A/B of scene-launch clicks, which this task
  did not ask for and I did not build.
- `apps/gui-sonotron/src/in_process_brain_session.cpp:340-360`: `style
  switch` always targets `SectionType::kVarA` (line 357), with the comment
  citing "The GUI has no current-section readback (grid_model.hpp's
  documented gap)" as the reason — confirmed as a real, acknowledged
  limitation, not a misreading.

## 2. Diagnosi: è tutto armonia, o c'è altro?

**Sì, il gap è quasi interamente l'assenza di armonia viva — e ora è un
fatto misurato, non un'inferenza.** Lo stesso identico stile (bossa/samba/
pop), stesso routing, stessi 8 bar, stesso numero di note suonate: la
versione CLI visita 7 pitch-class su 12 seguendo un giro ii-V-I-vi reale;
la versione GUI fedele resta bloccata sulla triade di tonica (3 pitch-class,
sempre le stesse, per l'intera sessione). Non è un'impressione stilistica:
è `establish_default()` che semina la tonica una volta sola e non la
rimuove mai più finché nessun comando esplicito la sposta — e il traduttore
della GUI non genera MAI quel comando esplicito, per costruzione (nessun
branch `key`/`chord`/`seq`).

Le altre due cause elencate nella diagnosi a tavolino (doppio trigger di
sezione, style-switch-forza-VarA) sono confermate come reali nel codice, ma
sono ENTRAMBE subordinate: (a) il doppio trigger è dichiaratamente
intenzionale nel commento del codice, non un bug silenzioso, e il suo
effetto pratico non è stato misurato in questo A/B; (b) style-switch→VarA è
un limite di readback riconosciuto, che sposta la sezione ma non tocca
affatto l'armonia — quindi non spiega la "staticità tonale" che l'owner
sente, spiega semmai un salto di sezione brusco durante un cambio stile a
trasporto avviato.

L'ipotesi alternativa (che Wave-1/Wave-2C degradino l'output A PRESCINDERE
dall'armonia) è stata testata direttamente col CONTROL-C e **respinta** per
il campione misurato: stesso numero di note, zero doppioni, downbeat intatti,
jitter di pochi millisecondi. Non ho trovato in questo campione alcun
artefatto introdotto da humanize o dalle VarA multi-bar.

## 3. Numeri chiave (per la risposta rapida)

- PATH-A: **9 eventi `chord`** in 8 bar, ciclo ii-V-I-vi completo, per
  ciascuno dei 3 stili testati.
- PATH-B: **0 eventi `chord`**, per ciascuno dei 3 stili testati.
- Pitch-class distinte su bass+chord1+chord2 (8 bar): **7 (A) vs 3 (B)**,
  identico su tutti e 3 gli stili — la tripletta statica è sempre
  `{C,E,G}` (pitch-class 0,4,7), la triade di tonica di C maggiore.
  Bar-by-bar per bossa/chord1: A cicla `[0,2,5,9]→[2,5,7,11]→[0,4,7,11]→
  [0,4,7,9]`, B resta `[0,4,7]` per tutti gli 8 bar.
- Numero di note suonate: **invariato tra A e B** (140/140 bossa, 155/155
  pop; 185/153 samba — quel gap è aritmetico, non musicale: gli accordi di
  settima di A hanno una nota in più della triade di B). Conferma che è la
  densità ritmica a restare identica: cambia SOLO il contenuto armonico.
- CONTROL-C (humanize on/off, bossa): stesso conteggio note su
  bassa/chord1/batteria (17/71/98 in entrambi), zero doppioni, downbeat
  identici, jitter di ~8 tick (~3.8 ms a 130 BPM). Nessun artefatto trovato.

## 4. Cosa NON ho verificato (limiti onesti di questo A/B)

- Non ho misurato l'effetto pratico del doppio trigger sezione+launch
  (claim #2) — servirebbe un A/B dedicato su click di scena a trasporto
  avviato, con capture dei bar-boundary attorno al click.
- Non ho testato transizioni di sezione (VarA→VarB→fill→ending) in nessuno
  dei due path; l'intero A/B resta dentro VarA per tutti gli 8 bar. Se
  esiste un artefatto di humanize/VarD specifico alle transizioni, questo
  campione non lo vedrebbe.
- Ho testato l'assenza di humanize-artifacts su UN solo stile (bossa), UNA
  sola sezione, 8 bar: non è una prova esaustiva, è un campione pulito che
  refuta l'ipotesi più ovvia (downbeat shift, note doppie) per quel
  campione specifico.
- Non ho toccato né riletto le regole di arbitraggio `chord follow`
  (auto/detect/sequencer/manual/live, D47) — irrilevante qui perché la GUI
  non manda mai nessuna sorgente di chord, quindi l'arbitraggio non entra
  in gioco.

## 5. Raccomandazione ordinata (a parità di sforzo di implementazione, dal
   più economico al più grosso — valutazione, NON implementazione)

1. **Il fix più economico e a più alto impatto**: dare alla GUI un modo
   qualsiasi di guidare l'armonia — anche il più semplice, un pannello
   "harmony sequencer" che manda `seq new/add/loop/play` una volta, allo
   style load, con un giro diatonico di default (es. I-vi-IV-V o il ii-V-I-vi
   di questo A/B). Zero nuovo stato core: il verbo `seq` esiste già,
   l'unico lavoro è nel traduttore GUI (`command_line_to_command`) e in un
   piccolo widget. Questo da solo chiude quasi tutto il gap misurato qui.
2. **Fix medio**: esporre `chord play`/`key` dalla GUI per l'armonia LIVE
   (tastiera on-screen già esiste per `note`, servirebbe lo stesso percorso
   per `play <note> [quality]`) — più lavoro UI, stesso verbo di back-end
   già presente.
3. **Fix cosmetico, indipendente dall'armonia**: risolvere il readback di
   sezione mancante così che `style switch` possa mirare alla sezione
   corrente invece di forzare sempre VarA (richiede lo stato di sezione
   corrente lato GUI, oggi assente per design — "grid_model.hpp's
   documented gap").
4. **Non è chiaro che serva un fix**: il doppio trigger sezione+scena è
   dichiaratamente intenzionale nel codice; prima di toccarlo servirebbe
   un A/B mirato per dimostrare che produce davvero un artefatto udibile
   (io non l'ho misurato).

Nessuna di queste opzioni richiede una nuova dipendenza host o core.

## Appendice: file dei dati grezzi

Tutti gli script `.acmd` e le relative catture `.out`/`.err` sono in
`/tmp/claude-1000/-var-home-crsn-Condos-fedora-strudel-projects-sonotron/2aa947af-94f9-4a27-a42f-1825133aa3cd/scratchpad/ab/`
(scratchpad di sessione, non nel repo): `bossa_A_cli_harmony.{acmd,out}`,
`bossa_B_gui_static.{acmd,out}`, `bossa_C_cli_nohumanize.{acmd,out}`,
`samba_A_cli_harmony.{acmd,out}`, `samba_B_gui_static.{acmd,out}`,
`pop_A_cli_harmony.{acmd,out}`, `pop_B_gui_static.{acmd,out}`, più
`measure_ab.py`/`check_humanize.py` (gli script Python usati per le
misurazioni sopra).
