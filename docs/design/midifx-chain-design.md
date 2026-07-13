# MIDI-FX / Transform chain (#10, nodes `5000`/`5100`/`5200`) — as-built architecture design review

Corelli, architecture critic. This is a structural design review of code that does
**not exist yet** for the chain body, grounded against code that **does** exist
today for its three intended first inserts (groove, arp, the D40 resolution
kernel) and its two immediate siblings (#1 Restyle, #7 Motif). Diagnosis and
structural proposal only — no implementation, per role boundary.

---

## 0. What was traced

Decisions read: `docs/DESIGN.md` §20 (STM32 rules, D32/D33), the `0000` invariants
band (`0200`/`0300`/`0400`/`0600`/`0700`/`0800`), and the canonical tree `5000`–
`5300` (lines 828–849), `7100`/`7300` (860–867), `11720` (1063–1075, the ABI-none
`kMaxInserts` reservation). Execution context: `docs/design/phase5-execution-plan.md`
Item H (lines 146–151) and the S-core serialization table (lines 61–70). Sibling
design precedent: `docs/design/motif-engine-placement.md` §1 (lines 100–167, the
D40 gather/resolve/voice/groove taxonomy and the Pipeline-vs-inline verdict for
#7) and `docs/design/pad-scene-design.md` §4.3 (lines 303–337, Command-family
vocabulary discipline) — no `docs/design/clip-primitive-design.md` exists yet
(confirmed by directory listing), so #2 Clip's shape is only available through
`pad-scene-design.md`'s cross-reference.

Code traced, with the call graph actually walked (not recalled):
- `components/arrangrr/include/arrangrr/arranger/arranger.hpp`: `Arranger::on_tick`
  (232–397, the `3120`/D40 per-role loop: gather → `gesture::expand` → `resolve()`
  → `VoicingState::voice()` → `groove::apply()` → schedule), the role-indexed
  `Route m_routes[kRoleCount]` (465) and the single global `GrooveParams m_groove`
  (468, comment: *"global groove feel applied to every part"*).
- `components/arrangrr/include/arrangrr/arranger/groove.hpp`: `groove::apply`
  (65–114, pure function of `(params, role, step, tick, base_vel)` →
  `{timing_offset, velocity}`), `GrooveParams`/`GrooveField` (21–41, comment:
  *"Per-part grooves are a later refinement; the field-addressed setter keeps the
  ABI stable for it"* — the codebase already names this exact gap).
- `components/arrangrr/include/arrangrr/arp/arpeggiator.hpp`: `ArpeggiatorEngine`
  (81–318), `ArpeggiatorParams`/`ArpField` (57–76), header comment (9–14): *"the
  same engine serves all three roles ... a style part, a track MIDI-FX, and a
  live-keyboard effect"* — track-MIDI-FX reuse was anticipated at the class's
  own design time.
- `components/arrangrr/include/arrangrr/engine.hpp`: `Engine::on_tick` (238–266,
  fixed fire order `fire_timeline → fire_chord_seq → commit_bar →
  fire_arranger → fire_arp`), `Engine::fire_arp` (521–530), `Engine::push_midi_in`
  (124–156) and `observe_arp_input` (511–517) — arp is driven entirely outside the
  `Arranger`'s per-role loop, off live captured input, single global `m_arp`
  instance (558).
- `components/arrangrr/include/arrangrr/restyle/restyle_stage.hpp`: `RestyleStage`
  (176–359) — a `runtime::Pipeline` stage that independently re-derives
  `groove::apply` (316) inside its **own** `note_on`/`note_off` (295–347), and
  correlates a note-on to its eventual note-off via a fixed `Pending m_pending[128]`
  array keyed by input pitch (252–257, 320–346) — the one existing precedent in
  this tree for "transform a live note stream while preserving on/off pairing."
- `components/runtime/include/runtime/pipeline.hpp` (full file) and
  `components/runtime/include/runtime/stage.hpp` (full file) — the
  `Pipeline<StageTs...>`/`StageLike` composition idiom: fixed declared N-slot
  chain, composition not inheritance, `if constexpr (requires {...})` SFINAE
  probing for optional per-stage hooks instead of virtual dispatch.
- `components/arrangrr/include/arrangrr/routing/router.hpp` (full file) — `Route`/
  `Router`, `StaticVector<Route, kMaxRoutes>` (88), the existing bounded-container
  idiom.
- `components/arrangrr/include/arrangrr/abi.hpp` lines 1–199 — the FROZEN-v1
  banner, the `Param` enum, and the **already-reserved** `5100` block (159–189):
  `kMaxInserts = 8` is the only committed symbol; `kFxSet/kFxParam/kFxEnable/
  kFxClear` are named but unassigned, with the `(idx=track, a=slot, b=type/param,
  c=value)` packing pre-documented.
- `components/arrangrr/include/arrangrr/config.hpp` (full file) — `kMaxTracks = 16`
  (Timeline tracks) vs. `kRoleCount` (10, `arranger.hpp`'s `TrackRole`, confirmed
  independently by `pad-scene-design.md` line 140's citation of
  `arranger.hpp:366`) — **two different "per-X" partitions already coexist** in
  the tree.
- `docs/design/motif-engine-placement.md` §1 — the taxonomy that classifies
  `resolve()`/`voice()`/`groove()` as three **transforms over an already-produced
  spec**, versus a *producer* that originates the spec (motif, gesture-expand);
  and the finding that a `Pipeline<StageT...>` stage is transitively HOST-ONLY
  today (`RestyleStage` is core code, but its only real external driver,
  `midisrc::MidiSourceStage`, is host-only), even though the stage itself is
  freestanding.

What I could **not** trace: no code for `5100`/`5200`/`5300` exists (confirmed —
`abi.hpp`'s own comment says so, and no `insert`/`chain`/`fx` symbols turned up
outside that reserved-block comment and the `kMaxInserts` constant). Everything
below about the chain's shape is therefore a **structural proposal against
present evidence**, not a report on built code — flagged plainly per this role's
"never bluff" rule, and distinguished throughout from the parts of this review
that ARE built-code critique (groove's fusion, arp's placement, Restyle's
Pending idiom, Pipeline's composition idiom).

---

## 1. L'architettura com'è costruita — dove il chain dovrebbe innestarsi

### 1.1 Il D40 kernel oggi è gather → resolve → voice → groove → schedule, non uno stream

`Arranger::on_tick` (232–397) non produce un "event stream" su cui un secondo
passaggio potrebbe poi operare: produce un `NoteReq group[kMaxVoiceNotes]` per
ogni ruolo/step, lo passa a `m_voicing.voice()`, poi **nello stesso ciclo `for`**
chiama `groove::apply()` e chiama `schedule()` due volte (note-on, note-off) con
l'offset appena calcolato condiviso fra le due chiamate (388–397). Non esiste oggi
un punto in cui un "evento" risolto viva come valore indipendente prima di essere
schedulato: `groove::apply()` è **fuso** nel ciclo di risoluzione, non è un passo
di uno stream.

Questo è il fatto strutturale decisivo per il punto 2 del compito: un Insert che
"processi un event stream" richiede che un evento esista come valore a sé, con
identità propria (per correlare on↔off), PRIMA di raggiungere lo scheduler. Oggi
non esiste — esiste solo `RestyleStage::note_on`/`note_off` (295–347), che è
l'UNICO posto della codebase in cui questa correlazione già avviene, tramite
`Pending m_pending[128]` indicizzato per pitch d'ingresso. Quel `Pending` è
overhead **nuovo** rispetto a `groove::apply()` fuso: `groove::apply()` oggi non
tiene NESSUNO stato fra note-on e note-off (usa una variabile locale condivisa nel
medesimo scope), mentre `RestyleStage` lo ricostruisce ex novo perché il suo
note-on e il suo note-off arrivano in chiamate separate, potenzialmente a molti
tick di distanza.

### 1.2 Groove è già duplicato, non unico — due call site indipendenti

`groove::apply` ha oggi **due** chiamanti strutturalmente diversi:
`Arranger::on_tick` (groove.hpp:65 chiamato da arranger.hpp:388, con `step`/`tick`
dal grid della sezione stilistica) e `RestyleStage::note_on` (317, con uno `step`
ricavato da `snap_to_grid(m_now)` su un tick di arrivo live). Questi non
convergono in un solo punto — sono due riletture indipendenti della stessa
funzione pura, applicate a due produttori di note diversi (l'Arranger e Restyle),
ciascuno con la propria nozione locale di "step". **Un chain di insert unico e
condiviso non può assorbire questa realtà senza prima decidere se groove resta un
parametro che OGNI produttore di note applica alla propria uscita (lo status quo,
duplicato ma coerente), oppure diventa un unico stadio a valle che TUTTI i
produttori attraversano** (il che richiederebbe che sia l'Arranger sia
RestyleStage smettano di chiamare `groove::apply()` da soli e comincino a
emettere verso un chain condiviso — un cambio di flusso, non un refactor
meccanico).

### 1.3 Arp non è un transform di stream: è un generatore stateful sul proprio clock

`ArpeggiatorEngine` (81–318) non processa un flusso di note in ingresso in
uscita nello stesso tick: accumula note tenute su `note_on`/`note_off` (117–150)
e le riemette a un proprio ritmo indipendente da `on_tick` (166–190), pilotato
da `fire_arp` (engine.hpp 521–530) **fuori** dal ciclo per-ruolo dell'Arranger,
su un unico `m_arp` globale con singola porta in/out (557–562). La forma "Insert
processa un evento e produce zero-o-più eventi nello stesso istante" non
descrive l'arp: l'arp trasforma **il tempo** stesso della relazione tra ingresso
e uscita (poche note tenute → molte note nel tempo). Il commento della classe
(9–14) anticipa correttamente il riuso come "track MIDI-FX", ma la FORMA
dell'interfaccia richiesta per ospitarlo onestamente in un chain generico non è
la stessa che serve a un insert come lo scale-lock o il velocity-proc.

---

## 2. Deriva dalle decisioni

- **`5100`/`5200` (piano canonico) vs. la realtà del kernel D40**: il piano dice
  "Arp/groove/scale-lock diventano ISTANZE della chain, non moduli disconnessi"
  (DESIGN.md:831-832). Il codice mostra che groove e arp sono OGGI disconnessi in
  modi STRUTTURALMENTE DIVERSI fra loro (groove: fuso per valore in un loop
  altrui, senza identità di evento; arp: generatore autonomo su proprio clock,
  fuori dal loop). Non è una deriva nel senso di "il codice ha tradito una
  decisione già presa" — `5100`/`5200` sono ancora `○ planned` — ma è una deriva
  nel senso che il piano descrive un'unica operazione di refactoring ("diventano
  istanze") dove il tracciamento del grafo mostra **due problemi strutturali
  differenti** che nessun'unica interfaccia `Insert` uniforme risolve entrambi
  onestamente. **Chi deve cedere: il piano, non il codice.** Il nodo `5200` va
  scisso esplicitamente in due percorsi (§4 sotto), non trattato come un refactor
  singolo e simmetrico.
- **`0600` (open/hackable) vs. l'assenza di un punto di estensione unico**: `0600`
  chiede "una superficie parametrica uniformemente indirizzabile." Oggi esistono
  GIÀ due schemi di indirizzamento "per-X" non riconciliati — `Route
  m_routes[kRoleCount]` (10 ruoli fissi, arranger.hpp:465) e `kMaxTracks = 16`
  (Timeline, config.hpp:14) — e il testo `5100` stesso dice "per-track" senza
  specificare quale dei due. Non è ancora una deriva codificata (nessun codice
  `5100` esiste), ma è un'ambiguità pre-esistente nel vocabolario che una
  progettazione onesta deve risolvere ESPLICITAMENTE prima di scrivere
  `StaticVector<Insert, kMaxInserts>` da qualche parte, o eredita la stessa
  confusione a due enumerazioni parallele che `pad-scene-design.md` §2.7 già
  documenta per `TrackRole` (10 core vs. 9 GUI, hand-copiate).

---

## 3. Proposte strutturali

### 3.1 Dove vive il chain e qual è l'unità di indirizzamento — NEEDS-DECISION

**Raccomandazione: "per-track" in `5100` deve significare "per-ROLE-slot"
(`kRoleCount = 10`, lo stesso spazio ordinale di `Arranger::m_routes`), non
"per-Timeline-Track" (`kMaxTracks = 16`).** Motivazione: i primi tre insert target
(echo, note-repeat, scale-lock) e i due refactor (`5210` groove, `5220` arp) sono
TUTTI concetti che oggi vivono già indicizzati per ruolo (`m_routes[kRoleCount]`,
`kRoleAnchor[kRoleCount]`, il `GrooveParams` unico applicato "a ogni parte").
Il Timeline (`kMaxTracks=16`) è un meccanismo di step-sequencer scritto a mano,
un primitivo diverso (`1400`), non il contenitore che l'Arranger o l'arp
attraversano. Trattare "track" come "ruolo" evita di inventare un terzo spazio
ordinale. **Questo è però un chiamata dell'owner, non mia**: se l'intenzione
originale di `5100` era davvero il Timeline Track (per applicare un chain anche a
sequenze scritte a mano, non solo a stile/arp), la capacità totale
(`kRoleCount=10` vs `kMaxTracks=16`) e il punto d'aggancio nel codice cambiano
interamente — flag esplicito, sezione 5.

Forma proposta (SHIPPABLE, nessuna dipendenza nuova):
```cpp
// one array, indexed by the SAME ordinal space as Arranger::m_routes
StaticVector<Insert, kMaxInserts> m_chain[kRoleCount];
```
`Insert` **non** è una classe base virtuale (D32: "virtual only at the HAL
boundaries" — un dispatch per-nota nel hot path non è un HAL boundary,
esattamente come `stage.hpp`'s commento già argomenta per `StageLike`). La forma
coerente con l'idioma già presente nel tree (l'`enum`→handler a compile time che
`ArpField::set_field`/`GrooveField::set_field` già usano) è un **tag + union
chiuso**, non un `std::variant` (non garantito freestanding-friendly allo stesso
modo, e comunque non necessario con un enum chiuso di 4-8 tipi):
```cpp
enum class InsertType : std::uint8_t { kEcho, kNoteRepeat, kScaleLock, kVelocityProc, /* ... */ };
struct Insert {
  InsertType type = InsertType::kEcho;
  bool enabled = true;
  union { EchoParams echo; NoteRepeatParams note_repeat; ScaleLockParams scale_lock; /* ... */ } params;
  // process(): switch (type) { case kEcho: return echo_process(params.echo, ev); ... }
};
```
Questo è enum-dispatch risolto a compile time via `switch`/`if constexpr`, zero
vtable, zero heap, dimensione fissa = `max(sizeof di ogni ParamsT)` — coerente
con D32.

### 3.2 Groove come insert — SHIPPABLE ma NON un refactor a bit identici gratuito, honestly flagged

**Sì, è raggiungibile byte-identico per i golden esistenti, MA solo se il
refactor non sposta groove fuori dal punto in cui l'Arranger lo applica oggi.**
Se `5210` significa "l'Arranger smette di chiamare `groove::apply()` da solo e
lo fa invece invocando `m_chain[role]` con un insert `kGrooveLegacy` che
INCAPSULA esattamente la stessa chiamata, nello stesso posto, con lo stesso
`step`/`tick`/`role`" — i golden restano bit-identici per costruzione (stesso
input, stessa funzione pura, stesso punto di applicazione). Se invece `5210`
significa "groove diventa un vero insert POST-schedulazione, che rilegge un
event stream indipendente dal `role`/`step` locali del ciclo che lo produce" —
serve il `Pending`-per-pitch di `RestyleStage` (o equivalente), la correlazione
on↔off cambia da "stessa variabile locale" a "lookup in un array indicizzato per
pitch", e QUESTO **può** cambiare l'ordine di emissione se due note della stessa
altezza si sovrappongono nello stesso ruolo (voice-leading può riusare un pitch)
— un caso limite reale, non teorico, che i golden `feel_swing`/`shuffle`/`blues`
potrebbero non coprire oggi. **Flag onesto: la bit-identicità è raggiungibile SE
e SOLO SE il chain è innestato nello stesso punto del kernel D40 dove
`groove::apply()` vive oggi (dopo `voice()`, prima di `schedule()`), non se
diventa un passo veramente indipendente a valle dello scheduler.**

### 3.3 Arp come insert — NEEDS-DECISION, l'interfaccia uniforme non basta onestamente

`5220`/`7130` non possono condividere l'`Insert` interface di 3.1 senza
indebolirla. Due strade oneste, nessuna delle due un refactor meccanico:
1. **Arp resta un componente Engine-level, ma diventato un ARRAY per-ruolo**
   invece di un singolo `m_arp` globale (557), pilotato ancora da `fire_arp` fuori
   dal ciclo dell'Arranger, con il proprio `EmitFn` che scrive nello STESSO
   `OutScheduler` — "istanza della chain" nel senso di "una `ArpeggiatorEngine`
   per slot", non nel senso di "attraversa la stessa interfaccia `Insert::process`
   degli altri sette tipi." SHIPPABLE, costo per istanza piccolo
   (`ArpeggiatorParams` ~9 B + `m_notes`/`m_vels[kMaxArpNotes=8]` ~16 B +
   contatori, arp/arpeggiator.hpp:57-79).
2. **`Insert` cresce una seconda capability opzionale** (`on_tick`, probata via
   `if constexpr (requires {...})` — lo STESSO idioma SFINAE che
   `pipeline.hpp`'s `fire_on_tick`/`fire_push_midi_in` già usa per gli hook
   opzionali per-stage) per gli insert che, come l'arp, hanno bisogno di un
   proprio clock oltre a un `process(event)`. Più onesto architetturalmente (un
   solo `Insert` type-erased, capacità eterogenee dichiarate a compile time),
   ma più ceremonia per un chain che oggi ha solo 2-3 inserti candidati.

**Non è mia la scelta fra le due — è una decisione di forma dell'interfaccia che
l'owner deve prendere prima che `5100`/`5220` vengano implementati**, perché
cambia la firma di `Insert` per tutti gli otto slot, non solo per l'arp.

### 3.4 ABI — SHIPPABLE, il surface è già disegnato bene, un solo avvertimento

Il blocco RESERVED in `abi.hpp` (159–189) è un buon lavoro: `kFxSet/kFxParam/
kFxEnable/kFxClear` con `(idx=track, a=slot, b=type|flags, c=value)` rispecchia
esattamente la forma che `kGroove`/`kArp` già usano (`a=Field, b=value`) — nessun
nuovo pattern di Command da inventare. **Un solo avvertimento da coordinare con
`pad-scene-design.md` §4.3**: quel documento raccomanda `SceneColumnTrigger` vs.
`PerformanceTrigger` per disambiguare `SceneTrigger`, E nota la disciplina
"riusa il verbo, non ri-derivarlo" per Pad/Clip. Il chain `5100` non tocca quella
famiglia (`kFx...` è un namespace di verbi ortogonale, mai sovrapposto ai verbi
di lancio-clip), quindi **non c'è conflitto oggi** — ma se un futuro insert (es.
un ipotetico "arm/launch insert" per sincronizzare un insert a un pattern-change)
volesse riusare la semantica di quantizzazione al bar che `kChordPlay`'s `idx`
già usa (abi.hpp:81-86, "SHIFT-quantized"), quel riuso va fatto esplicitamente,
non ri-derivato una quarta volta (lo stesso monito che `pad-scene-design.md`
§2.4 fa per `Sync: Immediate/ToBeat/ToBar/ToPattern`).

### 3.5 Dual-target / budget — SHIPPABLE, costo quantificato, nessuna dipendenza

Con lo slot per-ruolo (§3.1): `kRoleCount=10 × kMaxInserts=8 = 80` slot totali.
Se `Insert` è un tag+union dimensionato sul più grande dei parametri candidati
(`EchoParams`/`ScaleLockParams`, verosimilmente 8-16 B ciascuno) più 2 B di
tag/enable, il costo per l'intero chain è dell'ordine di **80 × ~16-20 B ≈
1.3-1.6 KB** — trascurabile contro il budget `≤512 KB` (D33, DESIGN.md:584).
**Il vero costo nascosto non è la config, è lo STATO runtime per gli insert
stateful**: se `5210` (groove) richiede un `Pending`-per-pitch come
`RestyleStage` (128 B per istanza, restyle_stage.hpp:357) moltiplicato per ogni
slot/ruolo che lo ospita, e se `5220` (arp) richiede un `ArpeggiatorEngine`
completo per slot/ruolo (~30-40 B ciascuno, arpeggiator.hpp:312-317), il costo
runtime sale a diverse centinaia di byte per ruolo attivo — ancora ben dentro il
budget, ma è la cifra che deve comparire nello `static_assert` sul pool totale
quando `5100` viene implementato, non la sola dimensione dei `Params` POD.
**Nessuna dipendenza nuova è implicata da nessuna proposta qui.**

### 3.6 Relazione con `runtime::Pipeline<StageT...>` — genuinamente diversa, machinery parzialmente riusabile

`Pipeline<StageTs...>` compone **componenti indipendenti a livello di intero
motore** (MidiSource, chorddet, arrangrr — `pipeline.hpp`:1-25), con costruzione
per-stage da factory eterogenee e un fan-out di CAPABILITY opzionali
(`push_midi_in`, `on_tick` a 3 argomenti) probate via `if constexpr (requires
{...})`. Il MIDI-FX chain compone **trasformazioni omogenee sullo stesso tipo di
evento, dentro il tick di UNA sola stage** (l'Arranger/arrangrr stage) — un
livello sotto, esattamente come motif è "un produttore un livello sopra" nel
verdetto di `motif-engine-placement.md` §1. **Non è lo stesso stage-shape**:
usare `Pipeline<StageT...>` per il chain erediterebbe la stessa trappola che
quel documento ha già diagnosticato per Motif — la shape `Pipeline` è pensata
per comporre pezzi con un "arrivo" esterno, non per un chain di trasformazioni
sempre presenti sullo stesso stream interno; forzarla qui sarebbe ceremonia senza
un confine guadagnato, la stessa obiezione già sollevata contro un quinto
sibling-package ipotetico. **Cosa è riusabile**: l'IDIOMA, non il tipo — il
capability-probing SFINAE (`if constexpr (requires {...})`) per dare a un insert
come l'arp un hook opzionale `on_tick` oltre al `process()` uniforme (§3.3
opzione 2) è esattamente la tecnica che `pipeline.hpp`'s `fire_on_tick`/
`fire_push_midi_in` già dimostrano funzionare, freestanding, zero virtual. Vale
la pena copiarla, non la classe che la ospita.

---

## 4. Cosa ho flaggato / cosa decide il proprietario

1. **"Per-track" in `5100` = per-ROLE (`kRoleCount=10`) o per-Timeline-Track
   (`kMaxTracks=16`)?** Raccomando ruolo (§3.1); è una scelta dell'owner perché
   cambia la capacità totale e il punto d'aggancio nel codice.
2. **Groove come insert: fuso nello stesso punto del kernel D40 (bit-identico
   garantito) o vero passo indipendente post-scheduler (bit-identico NON
   garantito sui casi limite di pitch-overlap)?** §3.2 — chiamata musicale/
   strutturale che solo l'owner può fare, perché cambia cosa "diventare
   un'istanza della chain" significa per groove.
3. **Forma dell'`Insert` uniforme: stream-transform puro, o capability-probed con
   un hook `on_tick` opzionale per ospitare l'arp onestamente?** §3.3 — decide la
   firma per tutti gli otto slot, non solo per l'arp; non è mia da decidere da
   sola perché è una scelta di interfaccia con conseguenze su ogni insert futuro.
4. **Nessuna proposta qui richiede una nuova dipendenza** (CLI-deps policy,
   `0800`): tutto è `StaticVector`/tag-union/enum-dispatch, già nel vocabolario
   della codebase.

---

*Nessun file prodotto è stato modificato all'infuori di questo documento. Nessun
codice è stato scritto o proposto come patch — solo direzione strutturale, come
da mandato.*
