# arrangrr — Design Exploration & Architecture Plan

## Context

`arrangrr` è un sistema **MIDI-only, realtime-first**: arranger + step/live sequencer + MIDI looper. **Target di runtime primario: STM32.** Linux è **solo devenv + ambiente di simulazione**; il progetto resta **OS-generico** (altri host/OS potranno arrivare domani — il platform layer non deve essere Linux-specifico). Non produce mai audio (niente synth, sampler, FX, audio looper): è una *macchina musicale MIDI autonoma* che pilota synth/DAW/hardware esterni ed è sincronizzabile via MIDI clock.

Il progetto è greenfield ma **è già un git repo** (branch `main`, ancora senza commit). `../seed-arranger` è un **progetto separato e non correlato** (Python, generazione MIDI offline): **da ignorare** nel design di arrangrr — nessun cugino, nessuna pipeline.

**Linguaggio: C++26 dove ragionevole, con realismo embedded.** Si cross-compila anche per STM32, quindi il core usa un subset freestanding-friendly (container/`Span` propri, `-fno-exceptions`/`-fno-rtti` nel path firmware). **La build dual-target si mette in piedi da subito** (host + arm-none-eabi cortex-m), non a fine progetto. Tool moderni (CMake ≥4), **poche dipendenze salvo motivi da discutere**.

Questo documento è una **esplorazione di design** viva (non minimalista): prima allarga lo spazio delle possibilità, poi classifica per priorità. L'obiettivo condiviso con l'utente è convergere iterativamente su *(a)* una **dream feature list** e *(b)* una **roadmap di milestone**, continuando finché la comprensione non è ~95%. Il mandato architetturale è:

```
core musicale portabile  +  host simulator  +  adattatori I/O (host / STM32)
```

**Nota di esecuzione:** l'output "eseguibile" di questo piano è (a) creare lo scheletro repo con l'HAL e i confini di modulo qui definiti, e (b) committare questo documento come `docs/DESIGN.md` nel progetto. Non c'è ancora codice da modificare; questo piano *è* la specifica di partenza.

**⏭ PASSO IMMEDIATO APPROVANDO QUESTO PIANO — primo commit (richiesto dall'utente: "fai commit di tutto, specialmente sotto docs"):**
1. Creare **`docs/DESIGN.md`** nel repo `arrangrr` = copia integrale di questo documento (fonte di verità versionata nel progetto, D31/Francesco #9).
2. Creare **`README.md`** minimo (una riga: cos'è arrangrr + puntatore a docs/DESIGN.md) e **`.gitignore`** (build/, .cache, ecc.).
3. **`git add -A && git commit`** — primo commit del repo (oggi branch `main` senza commit).
4. Nessun altro codice in questo passo; M0 (scaffold/CMake/core) parte dopo, come da §27.

---

## 0. Decisioni consolidate (living — aggiornato durante la discussione)

Fatti confermati con l'utente, in ordine di conferma. Questa sezione è la fonte di verità sulle scelte prese; il resto del documento va letto alla sua luce.

| # | Decisione | Stato |
|---|---|---|
| D1 | **Mai audio.** MIDI-only, nessun synth/sampler/FX/audio-looper. | ✅ Fermo |
| D2 | **Runtime primario STM32**, realtime-first; Linux solo devenv/sim; **restare OS-generici**. | ✅ Fermo |
| D3 | **C++26 dove ragionevole + subset embedded**; dual-build (host GCC16 + arm-none-eabi) **verde da subito**; una feature entra nel core solo se compila su entrambi. | ✅ Fermo |
| D4 | **Poche dipendenze**, salvo motivi discussi. Dep. da installare: `arm-none-eabi-gcc`. Da discutere: test framework, backend MIDI host. | ✅ Fermo |
| D5 | `../seed-arranger` **irrilevante** per il design (progetto separato). | ✅ Fermo |
| D6 | **IO fisico target**: 1× MIDI **DIN-5 in** + 1× **DIN-5 out** *e* **USB MIDI in/out** → **multi-porta (≥2 porte, ciascuna in+out) dal design iniziale**. | ✅ Fermo |
| D7 | **Focus immediato = versione Linux guidata da CLI.** Bottoni/encoder/display **out of scope ora**: niente UI fisica, niente `ui_model` adesso; la **CLI è l'interfaccia** (+ virtual MIDI). | ✅ Fermo |
| D8 | **Uso live + studio 50/50.** Il design bilancia performance immediata ed editing. | ✅ Fermo |
| D9 | **Filosofia: minimal-deep + nucleo profondo con strati opzionali** (progressive disclosure; vincoli come feature). | ✅ Fermo |
| D10 | **Architettura/identità = "Timeline Vivente" (primitiva unificante).** Il cuore è `Timeline × Track × Transform`: ogni track si riempie in 3 modi — *scrivi* (sequencer), *generi-da-accordo* (arranger), *catturi* (looper). Arranger/sequencer/looper NON sono motori separati ma **strati/gesti** sopra la stessa timeline. | ✅ Fermo |
| D11 | **Gesto eroe / primo WOW = "Chord Intelligence + Chord Sequencer": da input scarno → accordo pieno → progressione registrabile/loopabile** che diventa la *sorgente d'armonia* per tutto (arranger, arp, pad, re-harmonize). È il layer Transform armonico, il mattone più profondo e riusato. Ordine roadmap: spina → timeline/track → **Chord Engine + Chord Sequencer (primo WOW)** → arranger → altri gesti. | ✅ Fermo |
| D12 | **Interpretazione input accordo (primo modo) = B: Diatonico key-aware** — imposti tonalità, una nota = accordo diatonico di quel grado. Modi **A (single-finger assoluto)** e **C (shell/parziale→completamento)** come modi *selezionabili* subito dopo (`chord_mode`). | ✅ Fermo |
| D13 | **Output della chord intelligence = doppio da subito**: (i) *harmonizer live* (l'accordo esce ora verso il synth esterno) **e** (ii) *ChordSequence registrata* (progressione editabile/loopabile/trasponibile che pilota arranger/arp/pad). | ✅ Fermo |
| D14 | **ChordSequence = durate libere.** Struttura a step ma ogni accordo dura N beat/bar a piacere (Cmaj7 per 2 bar, poi G7 per 1). Input **sia live** (registrato a tempo, quantize-after) **sia editabile a step**. | ✅ Fermo |
| D15 | **NORTH-STAR / firma = MIX BILANCIATO dei quattro**, nessuno dominante: **(1) Chord intelligence** (input scarno → armonia ricca e musicale), **(2) Timeline unificata** (scrivi/genera/cattura come continuum, con re-harmonize), **(3) Collante MIDI perfetto** (timing/compliance/interop verso il tuo gear), **(4) Aperto/hackable** (ispezionabile, mappabile, scriptabile). La firma è la *combinazione*, non un singolo asse. | ✅ Fermo |
| D16 | **DETERMINISMO = proprietà-strumento, applicata DOVE SERVE, non sempre.** «Il determinismo è utile quando serve, che non vuol dire sempre.» → Deterministico *dove aiuta*: **core logico riproducibile dati input identici** (abilita golden test/replay/debug), storage/serializzazione, PRNG **seedato** (così l'humanize è riproducibile *quando lo vuoi*, es. nei test). **NON** deterministico *dove conta la vita musicale*: feel live, humanize/swing espressivi, timing reale, interazione umana — questi possono variare ed è giusto così. Regola: *il determinismo è un interruttore disponibile, non una gabbia.* | ✅ Fermo |
| D17 | **Conseguenze di "Aperto/hackable" (D15.4) = architettura di prima classe** (non P2): **(a)** core **headless** con **protocollo testo comandi→/eventi←** stabile e versionato, **scriptabile e replayable** (la CLI è un thin client su questo protocollo); **(b)** **spazio parametri uniformemente indirizzabile** — ogni parametro/stato ha un **ID/path stabile**, leggibile/settabile via CLI/MIDI/automazione/mapping; **(c)** **assegnabilità profonda / MIDI-learn** cittadino di prima classe; **(d)** **state dump/inspect** in qualsiasi momento; **(e)** golden/replay come workflow di sviluppo centrale. | ✅ Fermo |
| D18 | **Ambizione del sogno = ampia E profonda su tutte e quattro le aree** (transform armonico, arranger/style, sequencer/looper, interop/rig). Roadmap lunga accettata; disciplina "core-first + strati" la mantiene gestibile. | ✅ Fermo |
| D19 | **Ricchezza accordi = "smart per grado/contesto"** (default): `V`→dominante 7 (G7), `I`/`IV`→maj7, `ii`/`iii`/`vi`→min7, `vii`→ø7. Regole musicali automatiche, **override sempre possibile** via modificatore. | ✅ Fermo |
| D20 | **Scope armonico = diatonico + modificatori espliciti.** Puro diatonico di default (nessuna sorpresa); accordi presi in prestito, **dominanti secondarie**, alterazioni/estensioni disponibili solo su **richiesta esplicita** (modificatori). Niente cromatismi automatici. | ✅ Fermo |
| D21 | **Persistenza in fase CLI = entrambi.** Stato live effimero in RAM **+** `state dump`/`load` su file testo ispezionabile (abilita replay/golden). Progetto binario versionato+CRC disponibile ma non obbligatorio da subito. | ✅ Fermo |
| D22 | **Modello d'uso CLI = REPL su protocollo testo scriptabile.** La CLI è un *thin client* su un protocollo line-oriented comandi→/eventi←; **stesso protocollo** per (a) REPL interattivo (clock reale) e (b) batch/replay (clock virtuale, deterministico → golden). Headless-friendly. Dettaglio in §28. | ✅ Fermo |
| D24 | **NTT / "Style-Follow Note-Transposition Resolver" = modulo core di prima classe** (prima era annegato nel "voicing resolver"). È la logica musicale che adatta le frasi MIDI dello style all'accordo live **senza note sbagliate** (mapping grado/root + regole/tabelle di trasposizione per source-chord, concetto pubblico à la Yamaha NTR/NTT). Vive nel layer Transform, consumato da "genera-da-accordo". Qualità dell'arranger = qualità dell'NTT. Design in M5, concetto core da ora. | ✅ Fermo |
| D25 | **"MIDI FX / Transform insert chain" = concetto di prima classe** (allinea il north-star "aperto/hackable", D15.4). Catena componibile e **bounded** (max insert fissi) di trasformazioni MIDI applicabili per-track/zone: transpose · scale-filter · velocity-proc · humanize · note-repeat/ratchet · echo/MIDI-delay · strum · arpeggiate · **harmonize** · probability · randomize · **chord-memory-expand**. Arp/groove/scale-lock diventano *istanze* di questa catena, non moduli scollegati. Design incrementale M6/M8. **Modello unificante CONFERMATO** dall'utente. | ✅ Fermo |
| D27 | **Timing = PPQN interno alto 960 (fix Francesco #3/C1).** Lo scheduler gira a **960 PPQN interni** (0.52 ms @120 BPM), **disaccoppiato** dal MIDI-clock: F8 ogni **40 tick** (960/24, intero). **96 PPQN resta la griglia musicale** (quantize/notazione/vista). `Event.tick` è `i32` a 960; swing/humanize/micro-timing = **offset interi in tick a 960** (niente sub-tick, niente float). Il campo `@` di §29 è in tick a 960. Tutto intero ⇒ golden deterministici. 480 accettabile come fallback; il budget "jitter <1 ms" ora è coerente (grid 0.52 ms). | ✅ Fermo |
| D28 | **ChordSequence = memorizzazione funzionale a gradi + override (fix Francesco C2).** Ogni step = **grado + qualità/alterazioni** relativi a una **tonalità di riferimento per-sequenza**; accordi cromatici/prestati (`mod sec/borrow`, D20) = **override assoluti espliciti** sul grado. `transpose to <key>` ri-deriva i gradi nella nuova tonalità (musicale); `transpose ±semi` sposta la key di riferimento di N e ri-deriva; cambio modo (maggiore↔minore) ri-deriva i gradi nel nuovo modo. Abilita **re-harmonize / cambio tonalità / cambio modo**. La risoluzione grado→accordo concreto→voicing (via **NTT**, D24) avviene al playback. Aggiorna §16; **ridefinisce il golden G2** (transpose non è più cromatico cieco). | ✅ Fermo |
| D29 | **Determinismo clock virtuale & ordine totale eventi (fix Francesco C3/M10).** In `--clock virtual`, **`transport.advance` è l'unico motore del tempo**: avanza il clock e spara tutti gli eventi con `@ ≤ target` in **ordine totale `(@tick, class_priority, seq_no)`** — `class_priority` fissa NoteOff < NoteOn (e realtime/clock prima), `seq_no` è un contatore monotono d'emissione dal core come tie-break finale (⇒ min-heap stabile, golden riproducibili build-to-build). `transport.start/stop/continue` sono **eventi sulla timeline**, non prerequisiti di `advance`. `@tick` **schedula** soltanto (non auto-avanza). Il golden G2 va riscritto con `advance` intercalati alla registrazione. | ✅ Fermo |
| D26 | **Core ABI = comandi/eventi BINARI tipizzati su ring buffer; L0-JSONL+path-stringa = encoding SOLO host.** (Fix Francesco #1+#2.) Il core **non parsa mai** JSON né stringhe: riceve `Command{op, coll:u16, idx:u16, param_id:u16, value:Variant}` ed emette `Event{…}` POD. La risoluzione *path-stringa→(coll,idx,param)* e la (de)serializzazione JSONL vivono in `platform/host`. Le **collezioni sono array a capacità fissa** indicizzati per `u16`; i **nomi utente vivono solo lato host** (device standalone mostra slot numerici: SEQ 1, TRK 3). MIDI-learn/automazione on-device bindano a **param-ID interi**. Così §28/§29 restano l'API *host*, ma il contratto del core è binario e STM32-safe. | ✅ Fermo |
| D23 | **Design API CLI = stratificato a 3 livelli.** **L2 Superficie** = "Musician REPL" (sugar terso verb-first, per suonare a mano). **L1 Modello** = spazio parametri indirizzabile (`get`/`set`/`do <path>`): ogni comando è un path, mappabile/automatizzabile/learnable (D17b). **L0 Filo** = protocollo **JSONL** strutturato e versionato (`{"cmd":…}`→ / `{"ev":…}`←) per replay/daemon/GUI (D17a, D16). Il sugar L2 si espande in operazioni L1, che serializzano in messaggi L0. La CLI opera a qualsiasi livello (`--format human\|jsonl`, sugar on/off). | ✅ Fermo |
| D30 | **Onestà di scope (Francesco #6): M0–M5 = PRODOTTO, M6–M13 = ASPIRAZIONE.** "Minimal-deep" (D9) si onora facendo **M0–M5 bene e in verticale** (spina → chord intelligence → chord-sequencer → arranger), non aprendo tutto in orizzontale. La Dream List (§26) "ampia e profonda" (D18) resta l'orizzonte, ma M6–M13 (looper/arp/pad/FX-chain/performance/device/tool/STM32) sono esplicitamente aspirazione, riprioritizzabili. Rischio n.1 in solo: partire in orizzontale e non chiudere niente in verticale. | ✅ Fermo |
| D31 | **`arm-none-eabi-gcc` = prerequisito BLOCCANTE di M0** (Francesco #7), non "dipendenza da discutere": senza, la build ARM di M0 non esiste (oggi non è installato). E **committare `docs/DESIGN.md` nel repo** (Francesco #9): oggi tutto il design vive fuori dal progetto, il repo è vuoto. | ✅ Fermo |
| D32 | **Realtime-first MASSIMO + compile-time-first + ZERO allocazioni dinamiche (direttiva utente, principio sovrano).** **(a) Nessun heap da nessuna parte nel core** (non solo hot path): niente `new`/`delete`/`malloc`/`std::vector`/`std::string`/`std::map`/`std::function`. Storage statico/stack/arena **bounded** (`StaticVector`, `StaticString`, `RingBuffer`, `std::array`, pool fissi); **anche load/init riempie pool pre-dimensionati**, non alloca. **(b) Compile-time-first**: preferire `constexpr`/`consteval`/`constinit`, template, `concepts`, **tabelle `constexpr`** (qualità accordi, scale, regole NTT, velocity-curve, GM map), dispatch **enum→handler a compile-time**, `static_assert` ovunque su dimensioni/limiti. **(c)** Callback via **function-pointer / callable non-owning / template**; polimorfismo **compile-time (template/CRTP) nel hot path**, `virtual` solo ai confini HAL grossolani. Niente RTTI/eccezioni/`<iostream>` nel core. **(d)** Allocazioni e non-determinismo (se mai) **solo nei tool host**. Hot path = tutto **O(bounded)**, lock-free **SPSC**, niente syscall, niente lock bloccanti. | ✅ Fermo |
| — | *Stato:* comprensione ~99%; **tutti i blocker rossi di Francesco risolti** (D26 #1/#2, D27 #3, D28 C2, D29 C3); manutenzione (§9/§20 numeri, per-porta, USB) foldata. Prossimo: fold restanti nit o partire con M0. | 🔄 In corso |

---

## 1. Visione del prodotto

**Cos'è.** Un "cervello MIDI" per performance live e composizione: suoni accordi/note da una master keyboard, e il dispositivo genera in tempo reale accompagnamenti (drums/bass/chord/pad/arp), gestisce sezioni di arrangiamento (Intro/Variation/Fill/Break/Ending), sequenzia pattern, registra loop MIDI in overdub, e instrada/trasforma MIDI verso più device esterni su canali/porte diversi. Sincronizza da/verso clock esterno.

**Cosa NON è.** Non è un synth, non un sampler, non un motore di effetti, non un audio looper, non un plugin DAW, non un editor di partiture desktop. Non contiene "suoni": un "Program" qui è un *riferimento MIDI a un suono esterno* (port/channel/bank/PC/CC init), non un timbro interno.

**Il valore centrale** è: *timing MIDI eccellente + trasformazione musicale accordo-consapevole + interoperabilità/compliance MIDI totale + determinismo/testabilità*. Se il timing e la compliance non sono impeccabili, il resto non conta.

**Tre modi d'uso primari.** (1) Arranger live "one-man-band"; (2) Groove/step sequencer con song mode; (3) MIDI looper sincronizzato. I tre condividono lo stesso Transport, Router, Scheduler e Chord Engine.

---

## 2. Principi architetturali (regole dure)

1. **Core puro, sink-and-source.** Il core non legge l'orologio di sistema, non tocca filesystem, non fa syscall, non alloca heap durante play/record. Riceve *tick* iniettati e *eventi in ingresso*; emette *eventi in uscita schedulati*. Tutto il tempo entra come parametro.
2. **HAL sottile e totale.** Ogni interazione col mondo (tempo, MIDI in/out, storage, input fisico, display) passa da interfacce astratte implementate diversamente su Linux e STM32. Il core dipende solo dalle interfacce.
3. **Determinismo come proprietà-strumento, dove serve (non una gabbia).** Il core è *riproducibile dati input identici* (stesso stato + stessa sequenza di input timestampati ⇒ stesso output): questa è una **capability** che abilita golden test, replay e debug — attivabile quando serve. Randomness solo via PRNG **seedato** esplicito, così humanize/probability sono riproducibili *quando lo vuoi* (es. nei test) ma liberi di variare nel gioco live. Il determinismo **non** deve irrigidire la vita musicale (feel, swing, timing reale, interazione umana). Vedi D15/D16.
4. **Bounded everything.** Nessun contenitore a crescita illimitata nel path realtime: `StaticVector<T,N>`, array+count, ring buffer a capacità fissa. I limiti sono `constexpr` e verificati con `static_assert`.
5. **Realtime path senza sorprese.** No exceptions, no RTTI, no `iostream`, no `std::string`, no `new`/`malloc`, no lock che bloccano nel path audio-rate (usare SPSC lock-free ring buffer tra ISR e loop).
6. **Fixed-point per default.** Tempo in tick interi; BPM come `bpm_x100` (uint). Float ammesso solo dove serve davvero (es. curve di scoring nei tool laptop), mai nel path realtime STM32.
7. **Separazione dati/logica/IO.** Tre strati: *model* (POD serializzabile), *engine* (trasformazioni pure sul model + eventi), *platform* (adapter). I tool laptop (YAML/JSON, SMF, editor) vivono fuori dal core e non vi entrano mai.
8. **Un solo formato binario versionato** per il progetto/style su device, con `magic + version + CRC`. YAML/JSON solo lato tool.
9. **Testabilità headless prima di tutto.** Ogni engine è testabile con input MIDI registrato → output MIDI confrontato con golden file, senza hardware e senza tempo reale.
10. **Budget espliciti.** RAM/flash/CPU/latenza hanno numeri di target dichiarati e un validatore che rifiuta un progetto che li sfora prima dell'export su device.

---

## 3. Moduli software candidati (tabella)

Priorità: **P0** = MVP indispensabile · **P1** = MVP2 · **P2** = future/architetturale.
STM32 = gira anche su firmware. Tool = esiste solo lato laptop.

| Modulo | Descrizione | Perché serve | Prio | STM32 | Tool | Rischio |
|---|---|---|---|---|---|---|
| **Transport/Clock** | Tick master, PPQN, tempo, play/stop/continue, song position, tick→beat/bar | Base di ogni timing | P0 | Sì | No | Medio |
| **MIDI In Processor** | Parsing byte-stream, running status, NoteOn v0→Off, merge input, timestamp | Ingresso corretto e compliant | P0 | Sì | No | Medio |
| **MIDI Out Scheduler** | Coda eventi timestampati, ordinamento, throttling/bandwidth DIN, flush per porta | Timing stabile in uscita | P0 | Sì | No | Alto |
| **MIDI Router** | Instradamento in→out per porta/canale, thru/soft-thru, filtri, remap canale | Interoperabilità/flessibilità | P0 | Sì | No | Medio |
| **Note/Voice Tracker** | Traccia note attive per canale/porta, sustain, per panic/all-notes-off, anti-stuck | Evita note bloccate | P0 | Sì | No | Medio |
| **Sequencer** | Pattern step + live record, lunghezza per traccia, mute/solo, velocity/gate/prob | Cuore ritmico | P0 | Sì | No | Alto |
| **Chord Engine** | Riconoscimento accordo da note live, modi fingering, chord hold/memory | Fa "seguire gli accordi" | P0 | Sì | No | Alto |
| **Arranger/Style Engine** | Sezioni (Intro/Var/Fill/Break/End), pattern per ruolo, trasformazione su accordo | Feature identitaria | P0/P1 | Sì | No | Alto |
| **MIDI Looper** | Record/overdub/replace/erase/undo, quantize-after, loop len, sync | Feature identitaria | P1 | Sì | No | Alto |
| **Scale/Key Engine** | Chiave/scala corrente, scale-lock, correzione note fuori scala | Coerenza musicale | P1 | Sì | No | Medio |
| **Quantizer** | Quantizza input registrato (grid, swing, strength) | Qualità registrazione | P1 | Sì | No | Medio |
| **Groove/Swing Engine** | Micro-timing/swing/humanize deterministico (PRNG seedato) | Feel non meccanico | P1 | Sì | No | Medio |
| **Arpeggiator** | up/down/updown/random/as-played, octave, latch, sync, pattern arp | Espressività | P1 | Sì | No | Medio |
| **Phrase/Pad Engine** | Pad MIDI: one-shot/loop/hold/toggle, sync, chord/drum/CC pad, fill/scene trigger | Performance live | P1 | Sì | No | Medio |
| **Chord Sequencer** | Registra/riproduce progressione accordi per pilotare l'arranger a mani libere | Libera le mani | P1 | Sì | No | Medio |
| **CC Automation Engine** | Lane di automazione CC/pitchbend per traccia, con interpolazione step | Movimento timbrico esterno | P1 | Sì | No | Medio |
| **Performance/Scene Manager** | Stato live richiamabile (style, var, mute, split, routing, transpose, pad map) | Richiamo istantaneo | P1 | Sì | No | Medio |
| **Song/Scene/Chain Manager** | Concatena pattern/scene/sezioni in una song | Struttura brani | P1 | Sì | No | Medio |
| **Project/Preset Manager** | Load/save progetto binario, slot, defaults | Persistenza | P1 | Sì (I/O via HAL) | No | Medio |
| **Storage Model + Serializer** | Layout binario versionato + CRC + migrazione | Persistenza sicura | P1 | Sì | Parz. | Alto |
| **UI State Machine (ui_model)** | Stato UI headless: pagine, cursori, encoder, LED intent — separato dal render | UI piccola/portabile | P1 | Sì | No | Medio |
| **Diagnostics/MIDI Monitor** | Log eventi, contatori, jitter meter, ultimo accordo/sezione | Debug/interop | P1 | Parz. | Sì | Basso |
| **Panic/Reset Controller** | All Notes Off, All Sound Off, Reset All Controllers, per porta/canale | Sicurezza live | P0 | Sì | No | Basso |
| **MIDI Compliance Layer** | Politiche: running status out, v0 vs Off, active sensing, channel mode | Interop garantita | P0 | Sì | No | Medio |
| **MIDI Learn / Controller Map** | Mappa CC/note fisici → funzioni interne | Personalizzazione | P2 | Sì | Parz. | Medio |
| **Program/Bank Manager** | Emissione PC + Bank MSB/LSB + CC init per device esterni | Setup suoni esterni | P1 | Sì | No | Basso |
| **Device Profile Registry** | Modello di synth esterni (vedi §18) | Interop mirata | P2 | Sì (dati) | Sì (editor) | Medio |
| **SysEx Engine** | Ricezione/invio SysEx per backup/device profile (bounded, chunked) | Backup/interop | P2 | Sì (opz.) | Sì | Alto |
| **Clock Sync Manager** | Master/slave, drift/jitter handling, SPP, start/continue | Sync con mondo esterno | P0/P1 | Sì | No | Alto |
| **Style Compiler** | YAML/JSON style → binario validato per device | Content pipeline | P2 | No | Sì | Medio |
| **Pattern/Style Editor** | Editor grafico pattern/style lato laptop | Autoring | P2 | No | Sì | Medio |
| **SMF Import/Export** | Standard MIDI File ↔ pattern/song | Interop file | P2 | No (o opz.) | Sì | Medio |
| **Regression Runner** | Esegue golden tests deterministici | Qualità | P0 (dev) | No | Sì | Basso |
| **Fault Injector** | Inietta jitter/byte corrotti/clock drift nel simulatore | Robustezza | P2 | No | Sì | Basso |

### Moduli/concetti aggiuntivi che consiglio di prevedere (non nella tua lista)

- **Latch/Hold Manager** trasversale (chord hold, arp latch, pad hold) — meglio un concetto unico riusato.
- **Transpose/Octave Engine** globale + per-track (master transpose, key transpose, octave shift), con regola "chi segue l'accordo e chi no".
- **Velocity Curve / Note Range Mapper** per-destinazione (parte del device profile ma usato nel path realtime).
- **Tap Tempo / Tempo Nudge** (input) e **Metronome/Click out** (come eventi MIDI o su canale dedicato).
- **Fill-on-change / Auto-fill** logica (fill automatico quando cambi variation) — concetto arranger classico.
- **Ending/Intro count-in scheduler** (le sezioni "one-shot" che poi passano a una variation).
- **Mute/Solo Group + "Track enable mask"** come oggetto di prima classe (richiamabile da Performance/Scene).
- **Chord→Bass inversion / "on-bass" (slash chord) resolver** come sotto-modulo del Chord/Arranger.
- **Voicing/Voice-leading resolver** (close/open/drop-2, smoothing tra accordi) — decide *come* le note seguono l'accordo.
- **Event Priority/Collision resolver** nello scheduler (NoteOff prima di NoteOn sullo stesso tick, ordine deterministico).
- **Time Signature Engine** (metro, per polymeter/section length) — spesso dimenticato ma serve presto.
- **Song Position/Locate** (spostarsi a bar N con ricostruzione stato) — non banale con arranger.
- **"MIDI thru mute during record"** e **input channel filter** — dettagli di routing che evitano doppie note.
- **Snapshot/Undo ring** generico (per looper e per edit) come struttura bounded riusata.

---

## 4. Moduli indispensabili (MVP credibile)

Un MVP che dimostra l'identità del prodotto richiede **timing impeccabile + sequencing + accompagnamento accordo-consapevole minimo**:

1. **Transport/Clock** (PPQN interno, play/stop, tempo, tick→bar/beat).
2. **MIDI In Processor** (parsing compliant, v0→Off, merge, timestamp).
3. **MIDI Out Scheduler** (coda timestampata, ordine deterministico, throttling DIN).
4. **MIDI Router** (in→out per canale/porta, thru configurabile, filtri base).
5. **Note/Voice Tracker + Panic** (no stuck notes, All Notes Off).
6. **Sequencer** (almeno pattern step multi-traccia + live record quantizzato).
7. **Chord Engine** (fingering base: Fingered + Single Finger; chord hold).
8. **Arranger core** (una Style con Variation A/B + Fill + Intro/Ending, drums fissi + bass/chord che seguono l'accordo).
9. **Clock Sync (slave minimo)** (segui clock esterno + Start/Stop) — o almeno master out pulito.
10. **Compliance Layer + Regression Runner** (perché "molto MIDI-compliant" è un requisito primario, non un extra).

Senza (1)-(5) non hai un dispositivo MIDI serio; senza (6)-(8) non hai *questo* dispositivo.

---

## 5. Moduli interessanti ma non MVP (prevedere, non implementare subito)

- **MIDI Looper completo** (overdub/undo/replace) — architettura sì, feature dopo il sequencer.
- **Arpeggiator, Phrase Pads, Chord Sequencer** — grande valore live, ma MVP2.
- **Groove/Humanize, Quantizer avanzato, Scale-lock**.
- **Performance/Scene/Song Manager** completo (nel MVP basta "load 1 project").
- **CC Automation lanes**, **Device Profile Registry**, **Program/Bank Manager** ricco.
- **SysEx**, **SMF import/export**, **MIDI Learn**, **Style Compiler/Editor** grafico.
- **Fault injection**, **jitter meter** avanzato.

Regola: *prevedi i punti di estensione (interfacce, campi riservati nel formato, hook nello scheduler) ma non il codice.*

---

## 6. Moduli da evitare (per non esplodere)

- Qualsiasi **audio/DSP/synthesis/sampling/FX** — escluso per mandato.
- **Motore di notazione/partitura**, editor score.
- **UI desktop complessa** dentro al core (solo `ui_model` headless + un render sottile).
- **Scripting embedded / VM** (Lua ecc.) nel firmware realtime.
- **Parser JSON/XML/YAML nel core** — solo nei tool.
- **Networking/OSC/Wi-Fi/Bluetooth** in MVP (eventuale P2 come adapter, mai nel core realtime).
- **MIDI 2.0/MIDI-CI full** ora — troppo ampio; lasciare un'astrazione, non implementare.
- **Allocatori/GC custom sofisticati**, **plugin dinamici**, **reflection**.
- **Undo illimitato / history persistente** — solo ring bounded.
- **Multi-utente / cloud / account** — fuori scopo.

---

## 7. Tassonomia dati (entità del model)

Gerarchia consigliata (dal contenitore live al mattone):

```
Project
 ├─ MIDISetup            (porte, sync mode, global transpose, master channel)
 ├─ DeviceProfile[]      (descrizione synth esterni)
 ├─ Program/ExternalSound[]   (riferimento suono esterno: port+ch+bank+PC+CC init)
 ├─ Style[]
 │   └─ Section[]        (Intro1/2, VarA..D, FillA..D, Break, End1/2)
 │       └─ TrackRef[]   (ruolo → Pattern + policy accordo/transpose/voicing)
 ├─ Pattern[]            (griglia relativa a gradi + timing; riferita da Section/Sequencer)
 ├─ Song[]               (catena di Scene/Section + ChordSequence + tempo map)
 │   └─ Scene[]          (snapshot: variation, mute mask, routing, transpose…)
 ├─ ChordSequence[]      (progressione accordi timestampata)
 ├─ Performance[]        (stato live richiamabile — la "registration")
 ├─ Pad[]                (assegnazioni pad → phrase/chord/CC/fill/scene)
 ├─ RoutingProfile[]     (matrice in→out, filtri, thru)
 └─ ControllerMap[]      (MIDI learn: controllo fisico → funzione)
```

Differenze chiave (per evitare confusione concettuale):

| Entità | Cos'è | Cosa NON è |
|---|---|---|
| **Project** | Il documento salvabile che contiene *tutto* (styles, songs, setup, profili). Root della serializzazione. | Non è lo stato live volatile. |
| **MIDISetup** | Config globale I/O: porte, sync master/slave, transpose master, canale master keyboard. | Non contiene pattern musicali. |
| **Style** | Un accompagnamento completo: insieme di **Section**, ognuna con pattern per **ruolo** traccia + policy. | Non è una song; non ha progressione fissa. |
| **Section** | Una parte dell'arrangiamento (VariationA, FillB, Intro1…) con lunghezza in bar e i pattern dei ruoli. | Non è un accordo; è "neutra" e si trasforma sull'accordo. |
| **Variation / Fill / Intro / Break / Ending** | Sottotipi di **Section** con semantica di transizione (loop vs one-shot). | — |
| **TrackRole / TrackRef** | Il ruolo (Drums, Bass, Chord1…) e la policy: segue accordo? transpose? voicing? range? destinazione MIDI. | Non è il pattern stesso: è il "come suonarlo". |
| **Pattern** | Il contenuto ritmico/melodico *relativo* (gradi rispetto alla root, step, gate, velocity), risoluzione-libero. | Non contiene note MIDI assolute finché non è risolto sull'accordo/chiave. |
| **Event** | Un singolo evento risolto/registrato: tick, tipo, canale, dati. Mattone di looper/registrazione/output. | Non è un pattern astratto; è concreto. |
| **Song** | Struttura di brano: catena ordinata di Scene/Section nel tempo + ChordSequence + tempo map. | Non è uno Style; usa Style/Pattern. |
| **Scene** | Snapshot di stato di performance a un punto della song (variation, mute mask, routing, transpose). | Non è una Section; è "quale configurazione". |
| **ChordSequence** | Progressione di accordi timestampata (per pilotare l'arranger senza mani). | Non genera note da sola; guida l'arranger. |
| **Performance** | Stato live richiamabile con un tasto (la "Registration/Combi"): style selezionato, variation, split, mute, routing, transpose, pad map, arp state. | Non è persistenza dell'intero project; è un *preset di stato*. |
| **Program / ExternalSound** | Riferimento a un suono *esterno*: port+channel+bankMSB/LSB+PC+CC init+range+velocity curve+transpose. | Non è un timbro interno (non esistono). |
| **DeviceProfile** | Descrizione di un device esterno (mappa drum, CC supportati, quirks, init/panic). Un Program lo referenzia. | Non è un singolo suono; è il "manuale" del device. |
| **Pad** | Trigger performativo: phrase/chord/drum/CC/fill/scene, con modo (one-shot/loop/hold/toggle) e sync. | Non è una traccia dell'arranger. |
| **RoutingProfile** | Matrice in→out + filtri + thru + remap. Referenziata da Performance/Scene. | Non è musica; è instradamento. |
| **ControllerMap** | Mapping controllo fisico (CC/nota/encoder) → funzione interna (MIDI learn). | Non è routing MIDI→MIDI. |

Rappresentazione in memoria (embedded-friendly): tutte queste entità sono **POD** (struct semplici, `enum class`, `StaticVector<T,N>` o array+count). Nessun puntatore owning; i riferimenti sono **indici/ID** (`u16`) dentro tabelle del Project, non pointer — così la serializzazione è memcpy-friendly e relocatabile.

---

## 8. Analogia con Korg / Yamaha / Roland / Casio (con fonti)

L'obiettivo è tradurre concetti *generali e pubblici* (non feature proprietarie) di arranger/workstation/groovebox in un progetto **MIDI-only**. Il punto centrale è che nei grandi strumenti questi concetti mescolano "controllo performance" e "generazione suono"; da noi la parte "suono" diventa sempre **riferimento MIDI esterno**.

### 8.1 Korg (Pa series)
Una **Style** Korg contiene tipicamente 8 style track, con **3 Intro, 4 Variation, 4 Fill, Break, 3 Ending**, più **4 STS (Single Touch Settings)** e **4 Pad** e una **Style Performance** per style. **STS** richiama i suoni per le real-time track (Upper/Lower). Il **SongBook** è un database musicale utente che memorizza *tutti* i setting per suonare un brano (style/MIDI file/tempo/volumi/suoni/mute/FX/STS/transpose) e si organizza in **Set List**. ([Korg — Songbook & Set List](https://support.korg.co.uk/en-US/songbook-and-set-list-setup-for-pa-keyboards-351393), [Korg Pa300 features](https://www.korg.com/us/products/synthesizers/pa300/page_1.php), [Korg Pa300 User Manual PDF](https://www.bhphotovideo.com/lit_files/252801.pdf))

**Traduzione arrangrr:** Style→`Style`; le sue sezioni→`Section[]`; **STS/Style Performance**→`Performance` (ma i "suoni" sono `Program` esterni: bank/PC/CC init); **Pad**→`Pad[]`; **SongBook/Set List**→`Song[]` + una `SetList` (lista ordinata di Performance/Song con setup MIDI). Le 8 style track → i nostri **TrackRole** (drums/perc/bass/chord/pad/…).

### 8.2 Yamaha (Genos/Tyros/PSR/Montage)
Concetti: **Registration Memory** (1–10) richiama pannello completo di setting; **One Touch Setting (OTS)** richiama i setting più appropriati (Keyboard Parts, Harmony/Arp, Multi Pad) per lo Style; **Multi Pad** in **Bank** da 4 frasi ritmico/melodiche; sezioni Style con **Intro/Main(Variation)/Fill/Break/Ending**. ([Yamaha Genos Owner's Manual](https://usa.yamaha.com/files/download/other_assets/7/1130977/genos_en_om_h0.pdf), [Genos Reference Manual](https://usa.yamaha.com/files/download/other_assets/7/1131007/genos_en_rm_h0.pdf), [Yamaha: Single Finger vs Fingered](https://faq.yamaha.com/usa/s/article/U0002033)) Internamente gli Style Yamaha usano tabelle di trasposizione (regole tipo NTR/NTT) per far *seguire l'accordo* ai pattern — concetto pubblico che ispira il nostro **voicing/transpose resolver**.

**Traduzione:** Registration Memory→`Performance`; OTS→un sottoinsieme "suggerito" di `Program`/mute per Style (opzionale); Multi Pad Bank→`Pad[]` in banchi da 4; la logica NTR/NTT→**note-transposition policy** per-ruolo nel Chord/Arranger.

### 8.3 Roland (Fantom/BK/E/MC)
Fantom struttura i suoni in **Tone → Zone → Scene**: un Tone è un suono, sta in una Zone (fino a 16 tone/zone, con key range/volume/pan/controller reception), e Zone+setting si salvano in una **Scene**. ([Roland: What is a Zone](https://support.roland.com/hc/en-us/articles/12869134186523-FANTOM-6-FANTOM-7-FANTOM-8-What-Is-a-Zone), [Sweetwater: Tones, Zones, Scenes](https://www.sweetwater.com/sweetcare/articles/roland-fantom-tones-zones-and-scenes/)) Gli arranger BK/E-series usano Rhythm/Style + registrazioni.

**Traduzione:** **Zone**→concetto potentissimo per noi: una **Zone** = porzione di tastiera (key range) + canale/porta di destinazione + transpose + velocity curve = esattamente come vogliamo instradare la master keyboard verso device esterni (split/layer). Adotto **Zone** come entità di routing di ingresso→destinazione. **Scene**→`Scene`/`Performance`. **Tone**→`Program` (esterno).

### 8.4 Casio (arranger CTK/LK/WK)
Modi di fingering: **Casio Chord** (accordi a un dito con regole semplici), **Fingered 1/2/3** (fino a 15 tipi di accordo), **Full Range Chord**. **Registration Memory** per richiamare setup. ([Casio CTK-6200 — chord fingering modes](https://www.manualslib.com/manual/595359/Casio-Ctk-6200.html?page=28), [Casio Auto Accompaniment PDF](https://support.casio.com/pdf/008/lk50_e_08.pdf), [Casio Memory Function PDF](https://support.casio.com/pdf/008/lk50_e_11.pdf))

**Traduzione:** i modi di fingering confermano il set che voglio nel **Chord Engine**: `SingleFinger`, `Fingered`, `FullKeyboard`, (+ `FingeredOnBass`, `AIFingered` opzionali). Registration→`Performance`.

### 8.5 Groovebox / MPC / Elektron
MPC: struttura **Sequence → Track → Program**; una Sequence può essere una sezione o un intero brano; nessun concetto di "clip" separata, note dentro le track. ([MPC Standalone OS User Guide](https://cdn.inmusicbrands.com/akai/MPC3-NI/MPC%20Standalone%20OS%20-%20User%20Guide%20-%20v3.4.pdf), [Sound on Sound: Akai MPC Basics](https://www.soundonsound.com/techniques/akai-mpc-basics)) Elektron: **parameter locks** per-step e pattern/chain, il sequencer come "modulatore". ([Elektronauts discussion](https://www.elektronauts.com/t/parameter-locks-on-the-mpc-live-nope-but-heres-a-workaround/39832))

**Traduzione:** MPC Sequence/Track→il nostro **Pattern/Song**; MPC "Program" (in MPC è un kit di suoni)→da noi **Program esterno** (mappa MIDI). **Parameter locks** Elektron→**per-step CC/velocity/prob lanes** nel Sequencer (concetto generale: "lock" = valore per-step). **Pattern chain**→`Song`/scene chain.

### 8.6 Sintesi della tassonomia ispirata

| Concetto industriale | Fonte | Entità arrangrr | Note MIDI-only |
|---|---|---|---|
| Style + sezioni | Korg/Yamaha | `Style`+`Section[]` | pattern relativi ai gradi |
| Performance / Registration / STS / Scene | tutti | `Performance` | richiama *riferimenti MIDI*, non suoni |
| Sound / Program / Tone | tutti | `Program`/`ExternalSound` | port+ch+bank+PC+CC init |
| Zone (key range→dest) | Roland | `Zone` (dentro RoutingProfile) | split/layer verso device |
| SongBook / Set List | Korg | `Song[]` + `SetList` | catena performance/song |
| Multi Pad / Pad | Yamaha/Korg | `Pad[]` (banchi da 4) | phrase/chord/CC/fill trigger |
| Sequence/Track/Pattern chain | MPC/Elektron | `Pattern`/`Song`/scene chain | — |
| Parameter locks | Elektron | per-step lanes | CC/vel/prob per step |
| Chord fingering modes | Yamaha/Casio | `ChordMode` enum | riconoscimento accordo |
| Note transposition rules | Yamaha | voicing/transpose policy | chi segue l'accordo |

---

## 9. MIDI Compliance Checklist

Distinguo cinque livelli (come richiesto):

- **A. Protocol compliance** — i byte sono corretti secondo lo standard MIDI 1.0.
- **B. Musical correctness** — le note giuste al momento giusto (accordi/scala/voicing).
- **C. Timing quality** — jitter/latenza/stabilità del clock.
- **D. Device interoperability** — funziona con synth/DAW reali e i loro quirk.
- **E. Routing flexibility** — instradamento/merge/thru/filtri configurabili.

### A. Protocol compliance
- [ ] Parsing **running status** in ingresso; opzione di *usarlo o meno* in uscita (config per bandwidth/compat).
- [ ] **Note On velocity 0 == Note Off** (accettato in input; in output policy configurabile: emettere Note Off reale di default).
- [ ] **Note Off reale** con release velocity (default 64) supportato.
- [ ] Gestione corretta di **status byte vs data byte** (bit alto), messaggi real-time interleavabili dentro altri messaggi.
- [ ] **System Real-Time** (Clock F8, Start FA, Continue FB, Stop FC, Active Sensing FE, Reset FF) gestiti fuori dallo stream principale.
- [ ] **SysEx** (F0…F7) parsing bounded/chunked, con timeout e reset su interruzione; passthrough sicuro.
- [ ] **Channel Voice**: Note On/Off, Poly Pressure, CC, Program Change, Channel Pressure, Pitch Bend — tutti round-trip.
- [ ] **Channel Mode messages** (CC 120 All Sound Off, 121 Reset All Controllers, 122 Local, 123 All Notes Off, 124–127 Omni/Mono/Poly).
- [ ] **Bank Select** CC0 (MSB) + CC32 (LSB) seguiti da Program Change, nell'ordine corretto.
- [ ] **RPN/NRPN**: CC 101/100 (RPN MSB/LSB) o 99/98 (NRPN), poi CC6 (Data MSB)/CC38 (Data LSB), con **RPN NULL (127/127)** per chiudere; increment/decrement CC96/97.
- [ ] **Pitch Bend** 14-bit (LSB+MSB) corretto; center 0x2000.
- [ ] **14-bit CC** (coppie MSB/LSB 0–31 / 32–63) — almeno gestione/pass-through corretta.
- [ ] **System Common**: Song Position Pointer (F2, 14-bit in MIDI beat=6 clock), Song Select (F3), Tune Request (F6).
- [ ] **Active Sensing** (FE): opzionale in output; se ricevuto, timeout ~300 ms → panic se il flusso si interrompe (configurabile, off di default per non essere invadente).
- [ ] Robustezza: byte corrotti, status incompleto, data byte orfani → scartati senza crash.

### B. Musical correctness
- [ ] Voicing accordo corretto per ruolo (bass = root/inversione, chord = triade/estensioni, pad = smooth).
- [ ] Trasposizione che rispetta chi "segue l'accordo" e chi no (drums/perc mai trasposti).
- [ ] Scale-lock/quantize-to-scale opzionale coerente con Key Engine.
- [ ] Note fuori range del device clampate/ripiegate secondo policy (non mute silenzioso involontario).
- [ ] Nessuna nota doppia (stesso pitch+channel) senza Note Off intermedio; gestione re-trigger corretta.

### C. Timing quality
- [ ] **MIDI Clock 24 PPQN** in output preciso; interno a PPQN maggiore (96/192) sotto-diviso a 24 per il clock.
- [ ] **Jitter budget** dichiarato (es. < 1 ms su DIN, < 0.5 ms preferibile) e misurato dal jitter meter.
- [ ] **Latency budget** input→output dichiarato (es. < 3 ms interno).
- [ ] Scheduler **look-ahead** con timestamp; NoteOff prima di NoteOn sullo stesso tick; ordine deterministico dei simultanei.
- [ ] **Bandwidth DIN** (numeri corretti): 31250 baud / 10 bit-per-byte = **3125 byte/s → ~320 µs/byte**; un messaggio da 3 byte ≈ **960 µs**; un accordo di 4 note (12 byte) ≈ **3.8 ms** su un singolo DIN ⇒ la "simultaneità" sub-ms sul filo DIN è fisicamente impossibile: il budget jitter va definito per **inizio-messaggio**, non per-nota. Prioritizzazione (clock/realtime > note > CC).
- [ ] **Scheduler & banda PER-PORTA** (Francesco #10): coda e modello di banda **separati per ogni porta** — USB (veloce) e DIN (lento, 3125 B/s) non condividono un budget globale.
- [ ] **USB-MIDI packetization** (Francesco M6): USB-MIDI 1.0 class = **pacchetti da 4 byte** (Code Index Number + cable number), niente running status sul filo, flow-control diverso dal DIN ⇒ path di throttling distinto; mappare cable→porta.
- [ ] Nessun blocco/GC/alloc nel path di scheduling.

### D. Device interoperability
- [ ] Init sequence per device (Bank/PC/CC init, GM Reset opzionale) all'attivazione di un Program.
- [ ] Panic robusto: All Notes Off **+** All Sound Off **+** Reset All Controllers su tutti i canali/porte usati, con anti-stuck (invia Note Off espliciti per ogni nota tracciata).
- [ ] Sustain pedal (CC64) gestito: note trattenute finché pedale su; panic rilascia anche il sustain.
- [ ] GM/GM2/GS/XG drum map: profilo device configurabile (canale drum 10, mappa nota).
- [ ] Test con destinatari reali: DAW (Reaper/Bitwig), Volca (canale/nota fisse), synth DIN, interfaccia USB-MIDI.

### E. Routing flexibility
- [ ] Multi-port, multi-channel; matrice in→out.
- [ ] **Thru** hard e **soft-thru** (rigenerato dallo scheduler, con re-timing/filtri).
- [ ] Merge di più input con timestamp e risoluzione collisioni.
- [ ] Filtri per tipo messaggio/canale/range nota per rotta.
- [ ] Mute del thru durante record per evitare doppie note.

---

## 10. Timing model

- **PPQN interno di scheduling: 960** (D27). 960 = 40 × 24 ⇒ MIDI clock F8 ogni **40 tick** (intero, sync pulito). A 120 BPM, 1 tick = **0.52 ms**: abbastanza fine per swing/humanize/micro-timing (offset interi in tick, niente sub-tick/float). **96 PPQN resta la "griglia musicale"** con cui si ragiona/quantizza (1/16 = 24 tick a 96 = 240 tick a 960; terzine e 1/32 tutte intere a 960). `Event.tick` = `i32` a 960. 480 come fallback (480/24=20). A ~300 BPM, 960 PPQN = ~4800 tick/s: banale per un Cortex-M.
- **BPM**: `bpm_x100` (`uint16`/`uint32`), es. 12000 = 120.00 BPM. Tempo→durata tick calcolata in fixed-point; niente float nel path realtime.
- **Sorgente del tick**: un timer hardware (STM32) o un thread ad alta priorità (Linux) chiama `core.onTick()` a risoluzione PPQN interna, oppure il core deriva i tick da un timer più veloce con accumulatore fixed-point (preferibile: timer a µs → accumulo → emette tick a soglia, così il tempo cambia senza riprogrammare il timer).
- **Mapping MIDI clock**: in **master** emetti F8 ogni 40 tick (a 960 PPQN). In **slave** ricevi F8 e ricostruisci il tempo con PLL/filtro (media mobile degli intervalli) per assorbire il jitter; il tick interno viene interpolato tra i clock ricevuti.
- **Start/Stop/Continue + SPP**: Start=riparti da 0; Continue=riparti da SPP; SPP in "MIDI beat" (1 beat = 6 MIDI clock = 16th). Locate ricostruisce lo stato dell'arranger a quel bar.
- **Scheduling**: coda di eventi ordinata per tick (min-heap bounded o timing wheel/bucket per tick nel look-ahead). Un evento = `{tick_assoluto, tipo, payload}`. Il drain avviene "just-in-time" a ogni tick con look-ahead di N tick per assorbire il costo di calcolo.
- **Quantizzazione (record)**: input registrato con timestamp raw → quantizza opzionale a grid con **strength** (0–100%) e **swing**; conserva l'off-grid se strength<100.
- **Swing**: ritardo dei sotto-battiti pari (es. 16th dispari) come % del passo; deterministico. Applicato come offset di scheduling, non modificando il pattern.
- **Jitter handling**: look-ahead + timestamp assoluti; il drain confronta il tick corrente e non "recupera" emettendo raffiche disordinate — se in ritardo, emette in ordine mantenendo la sequenza.
- **Overdub**: registra su un layer di eventi che si fonde con il loop esistente senza cancellare (vedi §13).
- **Section switching**: il cambio di variation è *quantizzato al confine di bar/beat* (configurabile: end-of-bar, end-of-pattern, immediate). Fino al confine, la sezione corrente continua; poi swap atomico dei pattern per ruolo.
- **Fill scheduling**: premendo Fill, lo scheduler inserisce il pattern di fill fino al prossimo confine di bar, poi passa alla variation target (auto-fill on-change opzionale).

---

## 11. Arranger design

**Sezioni** (sottotipi di `Section`, con semantica di loop/one-shot):

| Sezione | Comportamento | Transizione |
|---|---|---|
| Intro 1/2 | one-shot, poi → Variation corrente | non-loopante; conta i bar poi passa |
| Variation A/B/C/D | loop continuo | swap quantizzato al confine |
| Fill A/B/C/D | one-shot breve (di solito 1 bar) | poi torna alla variation (o va alla target) |
| Break | riduzione/silenzio parziale, loop breve | manuale |
| Ending 1/2 | one-shot, poi **stop** transport | conclude il brano |

**Chord following.** Il Chord Engine produce un `ChordState {root_pc, tipo, bass_pc, note_set}`. Ogni `TrackRole` ha una **policy**:

| Ruolo | Segue accordo? | Transpose | Voicing | Note |
|---|---|---|---|---|
| Drums | No | No | — | pattern fisso, canale drum |
| Percussion | No | No | — | fisso |
| Bass | Sì | root/bass | on-bass/inversione | rispetta slash chord |
| Chord 1 | Sì | sì | triade/close | voicing primario |
| Chord 2 | Sì | sì | voicing alternato (open/drop) | evita unisono con Chord1 |
| Pad | Sì | sì | smooth/voice-leading | minimo movimento tra accordi |
| Arp | Sì | note tenute/accordo | — | vedi §14 |
| Phrase | Config: fisso o trasposto | opz. | opz. | scelta per pad |
| Lead | Opz. scale-lock | opz. | — | quantize-to-scale |
| CC | — | — | — | solo automazione |

**Bass inversion / on-bass.** Se l'accordo ha una bass note diversa dalla root (slash chord, es. C/E), il Bass usa `bass_pc`; la policy sceglie tra: root-only, root+fifth, walking (da pattern), o inversione automatica per minimizzare il salto.

**Voicing / voice-leading resolver.** Trasforma i *gradi relativi* del Pattern in note MIDI concrete date `ChordState` + Key. Modalità: `Close`, `Open`, `Drop2`, `RootPosition`, `SmoothVoiceLeading` (minimizza movimento rispetto all'accordo precedente, entro un range). Clampa nel note-range del device.

**Chord modes (fingering).** Enum `ChordMode`: `SingleFinger`, `Fingered`, `FingeredOnBass`, `FullKeyboard`, `AIFingered` (opz.). 
- Split keyboard: sotto lo `split_point` → riconoscimento accordo; sopra → parti realtime instradate alle Zone.
- Full keyboard mode: riconosce accordi da ≥3 note ovunque, senza split.
- Single finger: 1–2 tasti → accordo maggiore/minore/7 secondo regole.
- Fingered: suoni tutte le note dell'accordo (fino a ~15 tipi).

**Chord hold / memory.** `Chord Hold` mantiene l'ultimo accordo riconosciuto anche a mani alzate (l'arranger continua). `Chord Memory` (opz. P2) associa a un pad/tasto un accordo memorizzato.

**Split & layer.** `Zone[]` (ispirato Roland): ogni Zone = key range + destinazione (port+channel) + transpose + velocity curve + on/off. Consente split multipli e layer verso device diversi. La zona "chord recognition" è una Zone speciale che alimenta il Chord Engine invece di suonare.

**Track enable / mute mask** come oggetto richiamabile (Performance/Scene).

---

## 12. Sequencer design

- **Modello**: `Pattern` per traccia con `length_steps` indipendente ⇒ **polymeter** naturale (traccia A 16 step, traccia B 12 step). Ogni traccia ha risoluzione (steps/beat) e destinazione MIDI.
- **Step data (per step)**: `on/off`, `note(s)`, `velocity`, `gate/length`, `tie`, `rest`, `probability`, `ratchet` (ripetizioni nello step), `micro-timing offset`, `condition` (es. "1/2", "fill only"). Questo copre parameter-lock-style (Elektron) per CC/velocity per-step.
- **Live recording**: input timestampato → quantize-after opzionale (grid+strength+swing); replace vs overdub; count-in; record loop.
- **Mute/Solo**: maschera per-traccia, con solo esclusivo; richiamabile da Scene.
- **CC lanes**: automazione per-traccia (CC/pitchbend) con punti step, interpolazione step/lineare (lineare solo nei tool → in realtime pre-computata a step per evitare float).
- **Song mode / Scene**: catena di pattern/scene con ripetizioni; `Scene` = snapshot di quali pattern/mute/routing sono attivi.
- **Probability/ratchet/tie/rest/gate**: tutti deterministici (probability via PRNG seedato per riproducibilità nei test).
- **Interazione con Arranger**: sequencer e arranger condividono Transport e Scheduler; una traccia sequencer può essere instradata come "traccia extra" accanto ai ruoli dell'arranger.

---

## 13. MIDI Looper design

- **Buffer**: per-traccia, `StaticVector<Event, N>` bounded (capacità dichiarata, es. 4k eventi/loop). Se pieno → warning, non crash.
- **Stati**: `Empty → Recording → Playing → Overdub → (Replace) → Stopped`. FSM esplicita.
- **Record**: cattura eventi MIDI in ingresso con tick assoluti (relativi all'inizio loop).
- **Overdub**: aggiunge eventi al buffer esistente (merge), il loop continua.
- **Replace**: sostituisce eventi in una finestra (cancella nel range e registra nuovi).
- **Erase/Undo**: `Undo ring` bounded (snapshot leggeri o journal di operazioni) — es. ultimi 1–4 stati.
- **Quantize after record**: applica grid+strength opzionale post-registrazione (non distruttivo se conservi il raw).
- **Loop length**: fissa (impostata prima), o auto (primo ciclo definisce la lunghezza), o quantizzata a bar. Per-track loop length (polymeter) o global loop.
- **Sync**: allineato al Transport dell'arranger (start del loop a confine bar) e al clock esterno se slave. `Capture` (retrospettivo): un ring buffer sempre-attivo permette "cattura le ultime N battute già suonate".
- **Note safety**: a stop/erase, invia Note Off per note attive del looper (anti-stuck).
- **Re-harmonize/transpose live via Note tracker (Francesco #5, vincolo non nota a piè di pagina)**: ogni ri-voicing di materiale suonante (cambio accordo/tonalità mentre un loop gira) deve passare dal **Note tracker** che possiede tutte le note attive ed emette i **NoteOff prima dei nuovi NoteOn**. Altrimenti il cambio-accordo-in-loop è una fabbrica di note bloccate.

---

## 14. Arpeggiator design

**Conviene includerlo?** Sì ma in **MVP2** — alto valore live, complessità media, si aggancia bene a Chord Engine + Transport.

- **Modi**: `Up, Down, UpDown (incl/escl estremi), DownUp, AsPlayed, Random, ChordRepeat (accordo intero ritmato), Gated`.
- **Octave spread**: 1–4 ottave, direzione.
- **Rate**: sotto-divisione del tick (1/8, 1/16, terzine…), gate length, swing (condiviso col Groove Engine).
- **Latch/Hold**: tiene le note anche a mani alzate; aggiunge/rimuove note al set latchato.
- **Sync**: al Transport (start su beat/bar) e al clock esterno.
- **Pattern arp / Rhythm arp**: sequenza di step (on/rest/velocity/accent) applicata all'ordine di note ⇒ "pattern" ritmico-melodico (concetto tipo pattern-arp Roland/Korg).
- **Interazione Chord Engine**: l'arp può prendere (a) le note fisicamente tenute, o (b) l'accordo riconosciuto (`ChordState`) espanso secondo voicing. Config per-arp.
- **Interazione Style Engine**: il ruolo `Arp` dell'arranger *è* un'istanza dell'arpeggiator alimentata dall'accordo corrente; l'arp "performativo" (su una Zone) è un'altra istanza. ⇒ Un **unico ArpEngine riusabile**, istanziato N volte con sorgenti diverse.

---

## 15. Phrase Pads / Pad Engine

- **Tipi di pad**: `Phrase` (sequenza MIDI registrata/preset), `Chord` (accordo one-shot/held), `Drum` (nota/e su canale drum), `CC` (invia CC/valore), `FillTrigger` (lancia un Fill dell'arranger), `SceneTrigger`/`VariationTrigger` (cambia stato), `NoteRepeat`.
- **Modi di trigger**: `OneShot`, `Loop`, `Hold` (suona finché premuto), `Toggle` (on/off).
- **Sync**: `Immediate`, `ToBeat`, `ToBar`, `ToPattern` (quantizza il lancio al confine).
- **Pitch behavior**: `FixedPitch` o `TransposeWithChord` (segue `ChordState`) — per phrase/chord pad.
- **Struttura**: `Pad {tipo, modo, sync, sorgente(patternId/nota/cc), destinazione(port+ch), pitchPolicy}`. Banchi da 4 (ispirazione Yamaha Multi Pad) → `PadBank[]`.
- **Riuso**: i pad "phrase" riproducono un `Pattern`/`Event[]` attraverso lo stesso scheduler; nessun motore separato.

---

## 16. Chord Sequencer

- **Modello (D28, funzionale)**: `ChordSequence = StaticVector<ChordStep, N>` con `key_ref` per-sequenza; `ChordStep {tick_start, dur_ticks, degree, quality_ovr?, alt[], inversion, abs_override?}`. Il grado è relativo a `key_ref`; `abs_override` (root_pc+qualità) è usato solo per cromatici/prestati (`mod sec/borrow`). La risoluzione grado→accordo concreto→voicing avviene al playback via **NTT** (D24). `transpose to <key>`/`±semi` e cambio-modo ri-derivano i gradi (non trasposizione cieca).
- **Registrazione**: da tastiera live (con quantizzazione a bar/beat) o step-input.
- **Playback**: emette `ChordState` verso l'Arranger al posto delle mani ⇒ liberi le mani per suonare il lead sopra.
- **Editing**: inserisci/cancella/trasponi step; loop; transpose globale.
- **Uso con arranger**: quando attivo, è la *sorgente d'accordo* (priorità sul riconoscimento live, o merge configurabile).
- **Uso live**: loop di 4–8 battute di progressione mentre suoni sopra; combinabile con Song mode.

---

## 17. Performance / Registration / Preset model

`Performance` = stato live richiamabile con un tasto (la "Registration/Combi/STS"). Salva **riferimenti + stato**, non contenuti pesanti (che stanno nel Project):

```
Performance {
  name
  style_id, current_variation
  tempo_x100, master_transpose, key
  split_point, chord_mode, chord_hold
  track_enable_mask, mute/solo state
  routing_profile_id            // matrice in→out + Zone
  zones[]                       // split/layer verso device
  per_track_program_id[]        // riferimenti a Program esterni (bank/PC/CC init)
  pad_bank_id                   // assegnazioni pad
  arp_state[]                   // modo/rate/latch per istanza arp
  chord_sequence_id (opz.)
  scene_refs[] (opz.)           // per song mode
  controller_map_id             // MIDI learn
}
```

- **Richiamo**: applica atomicamente lo stato; le emissioni "init" (PC/Bank/CC) verso i device avvengono in modo ordinato e throttlato all'attivazione.
- **SetList**: `SetList = lista ordinata di Performance/Song` (ispirazione Korg Set List / Yamaha Registration Sequence) per scaletta live.
- **Snapshot vs preset**: la Performance è un *preset di stato*; una `Scene` (dentro Song) è uno snapshot temporizzato. Condividono i campi ma Scene è ancorata al tempo.

---

## 18. Device Profile / External Program model

Poiché non ci sono suoni interni, un synth esterno è descritto da un **DeviceProfile**, e un suono specifico da un **Program/ExternalSound**:

```
DeviceProfile {
  name                          // "Volca Keys", "MODX ch1", "Reaper track 3"
  default_port
  drum_map (opz.)               // nome→nota (GM/GS/XG/custom)
  supported_cc[]                // CC noti + nome (per UI/learn)
  note_range_min/max
  velocity_curve_id
  bank_select_mode              // MSB-only / MSB+LSB / none
  init_messages[]               // bounded: sequenza CC/PC/SysEx d'init
  panic_messages[]              // override panic (device con quirk)
  quirks_flags                  // es. "no running status", "needs GM reset", "ignores CC64"
}

Program / ExternalSound {
  name (locale, per UI)
  device_profile_id
  port, channel
  bank_msb, bank_lsb, program_change
  cc_init[]                     // bounded: coppie (cc, value) all'attivazione
  transpose, octave
  note_range_min/max (override)
  velocity_curve (override)
}
```

- **Init messages**: emessi (ordinati, throttlati) quando il Program viene attivato in una Performance/Scene.
- **Panic per-device**: default globale, override per device con quirk.
- **Velocity curve**: LUT a 128 entry (fixed) per-curve; nessun float in realtime.
- **Editor** dei profili: solo tool laptop (YAML/JSON), poi compilati nel binario del Project.

---

## 19. PC simulator (design)

Tre eseguibili/target sopra lo **stesso core**, distinti solo dagli adapter HAL:

1. **Headless deterministic runner** (`sim_headless`):
   - Input: file di eventi MIDI timestampati + script di comandi utente (JSON/testo, *fuori* dal core).
   - Clock **simulato** (tick avanzati dal runner, tempo virtuale) ⇒ zero dipendenza dal wall-clock.
   - Output: stream di eventi MIDI (dump testuale canonico) confrontato con **golden file**.
   - Uso: unit/regression test, CI, TDD degli engine.

2. **Live virtual MIDI** (`sim_live`):
   - Adapter HAL su **ALSA seq / JACK / PipeWire** virtual MIDI (crea porte virtuali in/out).
   - Clock reale ad alta priorità; il core gira come su device.
   - Uso: suonare con tastiera reale → engine → synth/DAW reali.

3. **Integrazione DAW/hardware**: stesso `sim_live`, con routing verso porte esterne; modalità master/slave clock verso DAW/hardware.

4. **Debug UI** (`sim_ui`, sottile, *non nel core*):
   - Mostra: BPM, posizione (bar:beat:tick), accordo riconosciuto, sezione/variation, tracce+mute, routing/Zone, stream MIDI in/out, jitter meter.
   - Render dello **stesso `ui_model` headless** che girerà su display piccolo ⇒ la UI finale è già validata.

**Componenti del simulatore** (tutti tool-side):
- **MIDI Monitor** (dump leggibile + filtro).
- **Replay** (rigioca una sessione registrata → determinismo).
- **Golden test harness** (`regression_runner`): esegue N scenari, diff con golden, report.
- **Fault injection**: jitter sul clock in ingresso, byte corrotti, running status aggressivo, clock drift, buffer starvation ⇒ verifica robustezza.
- **Clock simulation**: virtuale (headless) e reale (live); **external clock sim** genera F8/Start/Stop/SPP con drift/jitter controllati per testare lo slave.

---

## 20. STM32 portability (regole concrete)

- **Zero allocazioni dinamiche nel core, ovunque (D32)**: nessun `new`/`malloc`/`std::vector`/`std::string`/`std::map`/`std::function` — né nel hot path né a init/load. Il load riempie **pool statici pre-dimensionati** (`std::array`/`StaticVector`), non alloca. L'heap (se mai) esiste solo nei tool host.
- **Compile-time-first (D32)**: `constexpr`/`consteval`/`constinit`, tabelle dati `constexpr` (accordi/scale/NTT/velocity/GM), dispatch enum→handler risolto a compile-time, `concepts` per contratti, `static_assert` su ogni dimensione/limite. Polimorfismo compile-time (template/CRTP) nel hot path; `virtual` solo ai confini HAL.
- **Bounded containers**: `StaticVector<T,N>`, array+count, ring buffer SPSC; capacità `constexpr` + `static_assert`.
- **No exceptions / no RTTI / no `iostream` / no `std::string`** nel firmware path. Errori via codici/`enum`/**`Result<T,E>` proprio** (Francesco M9: **NON** `std::expected` — non garantito freestanding su arm-none-eabi).
- **Standard C++: C++26 come target, con realismo.** Host = GCC 16 (già presente, ottimo supporto `-std=c++26`). Firmware = `arm-none-eabi-gcc` recente (GCC 14+/15+) compilato **freestanding** con `-std=c++26 -fno-exceptions -fno-rtti -fno-threadsafe-statics`. Il **core condiviso** usa un *subset* che compila su *entrambi*: si sfruttano feature moderne "a costo zero" (`constexpr`/`consteval`, `enum class`, `std::array`, `std::span` dove disponibile, `[[nodiscard]]`, `concepts`, designated initializers, `std::bit_cast`), ma si **evita tutto ciò che richiede heap/RTTI/eccezioni/`<iostream>`/`<string>`/`<expected>`** e si forniscono **`Span<T>`, `StaticVector<T,N>`, `RingBuffer`, `Result<T,E>` propri** per non dipendere dalla completezza della libc++/libstdc++ embedded. **Realismo C++26 (Francesco M8/M9):** reflection statica e contracts **NON sono in nessun GCC arm shipping** e non lo saranno a breve → **non progettare nulla che *dipenda* da C++26**; tratta `-std=c++26` come "zuccheri C++20/23 reali + qualche extra dove compila". **Pinna la versione minima** di `arm-none-eabi-gcc` (≥14, meglio 15) nel toolchain file. Regola operativa: *una feature entra nel core solo se il CI la compila su entrambi i target* (la lista è **positiva/verificata dal CI**, non aspirazionale).
- **No filesystem / no OS calls nel core**: storage/tempo/MIDI via **HAL** (`IClock`, `IMidiIn`, `IMidiOut`, `IStorage`, `IInput`, `IDisplay`).
- **Ring buffers** tra ISR (UART/USB MIDI) e loop principale (lock-free SPSC). ISR fa il minimo: copia byte nel ring; il parsing avviene nel loop.
- **Static asserts** su dimensioni struct, allineamenti, budget (`static_assert(sizeof(Project) <= BUDGET)`).
- **Budget espliciti (target, da confermare al scelta HW)**: es. core RAM di lavoro < ~64–128 KB, project in RAM < ~256 KB, tick jitter < 1 ms, latenza in→out < 3 ms. Un **validator** (tool) rifiuta project che sforano i limiti prima dell'export.
- **Serializzazione binaria**: layout fisso, endianness dichiarata (little-endian), `magic + version`, campi riservati per estensione, **CRC32** su tutto il blob. Versioning con migrazione *solo lato tool* (il device legge la sua versione o rifiuta con messaggio).
- **Graceful degradation**: se un buffer è pieno → droppa CC prima di note, mai clock; se storage assente → modalità volatile; watchdog + crash-recovery (all-notes-off al boot).
- **Determinismo**: nessun `Date.now`/random non seedato; PRNG esplicito.

**HAL interfaces (bozza):**
```cpp
struct IClock   { virtual uint64_t nowMicros() = 0; /* o tick source */ };
struct IMidiIn  { virtual size_t read(Span<uint8_t> buf) = 0; };      // non bloccante
struct IMidiOut { virtual size_t write(Span<const uint8_t>) = 0; };   // per porta
struct IStorage { virtual bool read(uint32_t off, Span<uint8_t>) = 0;
                  virtual bool write(uint32_t off, Span<const uint8_t>) = 0; };
struct IInput   { virtual InputEvent poll() = 0; };  // bottoni/encoder debounced
struct IDisplay { virtual void present(const UiFrame&) = 0; };
```
Il core espone: `onTick()`, `pushMidiIn(port, bytes, ts)`, `pushControl(cmd)`, `drainMidiOut(sink)`.

---

## 21. File formats

Due mondi separati (regola d'oro: **testo lato tool, binario lato device**):

- **Sorgente autoring (tool, laptop)**: **YAML/JSON** per Style/Pattern/DeviceProfile/Performance leggibili e diffabili in git. Schema con `schema_version`. Validazione con JSON Schema.
- **Binario compilato (device)**: prodotto dallo **Style Compiler**; layout fisso, `magic("ARGR") + format_version + payload + CRC32`. Sezioni: header, tabelle (styles, patterns, programs, device profiles, performances, songs), pool di eventi. Indici/ID `u16`, niente pointer.
- **Style Compiler pipeline (tool)**: YAML/JSON → validazione → **limit validation** (contro i budget STM32) → binario. Rifiuta con report se sfora.
- **SMF (Standard MIDI File) import/export (tool, P2)**: import → pattern/song (con quantize opzionale); export → SMF Type 1 per condividere/backuppare. Solo lato laptop; il core non parserizza SMF.
- **Schema versioning & compatibility**: `format_version` nel binario; il device accetta versioni ≤ della sua e migra *a monte* (nel tool), non a runtime. Campi riservati per forward-compat minima.
- **Backup/restore device**: via SysEx (P2) o via storage HAL (SD/flash) — dump del blob binario + CRC.

---

## 22. Tre MVP possibili

### MVP-α "molto piccolo" — *MIDI Brain + Step Sequencer*
- **Incluse**: Transport/Clock (96 PPQN, master+slave base), MIDI In/Out/Router, Note tracker+Panic, Compliance base, Sequencer multi-traccia con live record quantizzato, load 1 project binario, headless runner + golden tests, `sim_live` con ALSA virtual MIDI.
- **Escluse**: arranger, chord engine, looper, arp, pad, performance ricca, storage save.
- **Rischi**: bassi; il rischio è "non dimostra l'identità arranger".
- **Complessità**: piccola (fondamenta).
- **Dimostra**: timing/compliance/determinismo solidi + un groove sequencer usabile con hardware reale. È la **base non negoziabile**.

### MVP-β "realistico" — *Arranger accordo-consapevole* (CONSIGLIATO)
- **Incluse**: tutto α + Chord Engine (SingleFinger+Fingered, chord hold, split), Arranger con 1 Style (Intro/VarA/VarB/Fill/Ending), ruoli Drums(fisso)/Bass(segue)/Chord/Pad(seguono) con voicing resolver base, section switching quantizzato, Program/Bank/PC init verso device esterni, Performance minima (1 slot), sync slave completo (Start/Stop/Continue/SPP).
- **Escluse**: looper, arp, pad, chord sequencer, CC lanes, SysEx, SMF, device profile editor, song mode.
- **Rischi**: medi (chord→voicing→timing è il cuore difficile).
- **Complessità**: media.
- **Dimostra**: *questo prodotto* — suoni accordi, l'accompagnamento segue, cambi variation/fill, piloti synth esterni, sincronizzi con DAW. È il target realistico del primo traguardo credibile.

### MVP-γ "ambizioso" — *Performance live completa*
- **Incluse**: tutto β + MIDI Looper (record/overdub/undo), Arpeggiator, Phrase Pads, Chord Sequencer, Groove/Humanize, Scene/Song mode, Performance/SetList, Device Profile Registry, save project, Debug UI completa + fault injection.
- **Escluse**: SysEx full, SMF, MIDI 2.0, style editor grafico (restano tool P2).
- **Rischi**: alti (superficie ampia, integrazione).
- **Complessità**: grande.
- **Dimostra**: una "one-man-band" MIDI completa, quasi feature-complete pre-STM32.

---

## 23. Roadmap (incrementale)

Ordine consigliato (leggermente rivisto rispetto al tuo: consolido timing+MIDI+test prima di tutto, e anticipo il "vertical slice" end-to-end):

- **Fase 0 — Fondamenta, build dual-target, HAL & test harness.** Repo scaffold + **CMake con due toolchain (host GCC16 + arm-none-eabi cortex-m stub) verdi dal primo giorno** (il core compila come `.a` per entrambi; un firmware stub bare-metal linka e "gira" in QEMU o almeno linka pulito). HAL interfaces, `StaticVector`/`Span`/`RingBuffer`/`fixed`, Transport/Clock, MIDI In/Out/Router, Panic, headless runner + golden test format, `sim_live` (host MIDI backend). CI che compila **entrambi i target** ad ogni commit. *Vertical slice: nota in → routing → nota out con timing, testata headless e live; stesso core cross-compilato per arm.*
- **Fase 1 — Sequencer.** Pattern multi-traccia, live record + quantize, mute/solo, CC lanes base, song/scene minimale.
- **Fase 2 — Chord Engine.** Fingering modes, chord hold, split, `ChordState`, Key/Scale engine.
- **Fase 3 — Arranger/Style.** Sezioni, voicing/transpose resolver, section switching, fill, Program/Bank init. *(= MVP-β)*
- **Fase 4 — MIDI Looper.** Record/overdub/replace/undo, quantize-after, sync, capture.
- **Fase 5 — Arpeggiator & Pads & Chord Sequencer.** ArpEngine riusabile, Pad Engine, Chord Sequencer.
- **Fase 6 — Performance/Project/Storage.** Performance/Scene/Song, SetList, serializer binario + CRC, save/load, Device Profile Registry, Groove/Humanize.
- **Fase 7 — Tool pipeline.** Style Compiler (YAML→binario), limit validator, SMF import/export, device profile editor, fault injection.
- **Fase 8 — STM32 port.** HAL STM32 (USB MIDI device class + UART DIN + timer + storage), budget validation reale, watchdog/crash-recovery, display/encoder/LED, ottimizzazione.

*Perché prima le fondamenta+test:* con un core non-testabile deterministicamente, ogni fase successiva accumula debito; il golden-test harness è ciò che rende sicuro rifattorizzare per l'embedded.

---

## 24. Domande aperte (decisioni da prendere presto)

Con la mia raccomandazione tra parentesi:

| Decisione | Raccomandazione |
|---|---|
| **PPQN interno** | 960 scheduling (D27); 96 = griglia musicale |
| **BPM repr** | `bpm_x100` fixed-point |
| **Max tracks** | ~16 arranger-role + ~16 sequencer (da confermare vs RAM) |
| **Max events (looper/pattern)** | ~4k eventi/loop, ~64k eventi/project (budget, da validare) |
| **Max sections/style** | ~16 (Intro×2, Var×4, Fill×4, Break, End×2 + margine) |
| **Max projects / storage** | dipende da SD/flash; definire dopo scelta HW |
| **Max patterns** | ~256–512 per project (indici u16) |
| **Max MIDI ports** | 2–4 (1 USB + 1–2 DIN) come target iniziale |
| **Chord modes MVP** | SingleFinger + Fingered (FullKeyboard P1) |
| **Button/encoder count** | definisce `ui_model`; proporre ~8 bottoni + 2–4 encoder (da HW) |
| **Display assumptions** | piccolo mono/OLED (es. 128×64) o char LCD; `ui_model` astratto |
| **Storage assumptions** | flash interna per config + SD opzionale per project (HAL) |
| **MIDI 1.0 vs 2.0** | MIDI 1.0 ora; astrazione pronta, 2.0 non implementato |
| **SysEx** | Sì solo per backup/device profile, P2, bounded/chunked |
| **SMF import/export** | Sì, solo tool laptop, P2 |
| **External device profiles** | Sì, dati nel project (P1), editor tool (P2) |
| **Arpeggiator MVP** | No (MVP2), ma ArpEngine previsto architetturalmente |
| **Section switch quantize default** | end-of-bar |
| **Note On v0 in output** | Note Off reale di default (configurabile) |
| **Running status in output** | off di default (max compat), attivabile per bandwidth |

Le prime da bloccare *ora* (impattano il formato dati e il core): **PPQN, BPM repr, C++26+subset embedded (verificato dual-build), indici u16, limiti bounded principali (tracks/events/patterns/sections)**.

---

## 25. Raccomandazione finale

**Includere sicuramente (ora).** Il core deterministico con **Transport/Clock, MIDI In/Out/Router, Note tracker+Panic, Compliance layer, Sequencer** e il **golden-test harness** headless + `sim_live` su virtual MIDI. Bloccare subito: PPQN=96, `bpm_x100`, C++26 con subset embedded dual-build verde, `StaticVector`/`Span` propri, ID `u16`, HAL a 6 interfacce, formato binario `magic+version+CRC`. Puntare a **MVP-β** come primo traguardo di prodotto (arranger accordo-consapevole).

**Prevedere architetturalmente ma non implementare subito.** Looper, Arpeggiator (ArpEngine riusabile), Phrase Pads, Chord Sequencer, Performance/Scene/Song/SetList, Device Profile Registry, CC automation, SysEx, SMF, MIDI Learn, Style Compiler/Editor. Lasciare gli hook: campi riservati nel formato, `phase`/priority nello scheduler, sorgente d'accordo pluggabile, istanze multiple di ArpEngine, `Zone[]` nel routing.

**Evitare.** Qualsiasi audio/DSP/synth/sampler/FX; notazione; UI desktop nel core; scripting/VM embedded; parser JSON/XML/YAML nel core; networking in MVP; MIDI 2.0 full; allocazioni dinamiche/undo illimitato nel realtime.

**Primo prototipo su laptop.** Il **vertical slice della Fase 0**: core con Transport + MIDI In → Router → Out, guidato dal `sim_headless` (clock virtuale, input MIDI da file, output diffato con golden), e poi lo stesso core in `sim_live` che crea porte ALSA/PipeWire virtuali.

**Primo test end-to-end con virtual MIDI.** *Clock + echo + panic:* master keyboard (o porta virtuale) → `arrangrr` in `sim_live` → una traccia in Reaper/Bitwig. Verificare: (a) note passano con timing corretto e senza stuck; (b) `arrangrr` master invia MIDI Clock 24 PPQN + Start/Stop e la DAW segue il tempo; (c) `arrangrr` slave segue il clock della DAW; (d) Panic spegne tutte le note su tutti i canali. Questo test convalida i cinque assi di compliance (protocol/musical/timing/interop/routing) sul percorso minimo, prima di costruire arranger e sequencer sopra.

---

### Verifica (come validare l'esecuzione di questo piano)
1. **Scaffold + dual-build**: creare `app/{core,platform/{host,stm32},tools,firmware}` con CMake e **due toolchain file** (host, arm-none-eabi cortex-m). Compilare il core sia con GCC16 host sia con arm-none-eabi (`-std=c++26 -fno-exceptions -fno-rtti`, freestanding) — la CI verde su entrambi *è* il gate del subset. **Dipendenza da installare: `arm-none-eabi-gcc`** (ARM GNU toolchain).
2. **Headless**: `sim_headless` esegue uno scenario "note-in→note-out" e produce output identico al golden file (determinismo).
3. **Live**: `sim_live` crea porte virtuali; `aconnect`/`aseqdump` mostrano il flusso; una DAW riceve note e clock.
4. **Compliance smoke**: eseguire la checklist §9 sul percorso minimo (v0→Off, running status parse, panic, clock master/slave).

### Nota sulla struttura repo proposta
La tua struttura è **corretta e ben pensata**. Aggiustamenti minori consigliati:
- Aggiungere `app/core/hal/` (interfacce) e `app/core/common/` (`StaticVector`, `Span`, `RingBuffer`, `fixed`, `crc`).
- Aggiungere `app/core/routing/` (Router + Zone) e `app/core/scheduler/` (Out Scheduler) come moduli distinti da `midi/`.
- `app/core/tests/` bene; aggiungere `app/tests/golden/` per i golden file e `app/tools/regression_runner/`.
- `platform/host/` (generico, non "linux") conterrà gli adapter desktop; i backend MIDI Linux (ALSA/JACK/PipeWire) sono sotto-moduli intercambiabili (`platform/host/midi_alsa`, ecc.) così un futuro backend macOS/Windows si aggancia senza toccare il core. `platform/stm32/` gli adapter HW. Il `firmware/stm32h7/` (o generico `firmware/<board>/`) resta il progetto di build firmware che linka `core` + `platform/stm32`.
- **Dipendenze da discutere:** `arm-none-eabi-gcc` (necessaria per la build STM32 — non installata). Framework di test: valutare **roll-our-own minimale** vs una lib header-only leggera (es. doctest) — decisione "poche dipendenze". Backend MIDI host: RtMidi vs API native ALSA/JACK dirette (una dipendenza sottile, da discutere).

---

## 26. Dream Feature List (il "sogno" — aspirazionale, senza vincoli di priorità)

Questa è la *wishlist massima*: non tutto verrà costruito, ma serve a fissare l'orizzonte. Organizzata secondo l'architettura **Timeline Vivente** (`Timeline × Track × Transform`): una spina dorsale invariante, un layer Transform armonico (il cuore), la primitiva timeline/track, i tre gesti come strati, poi espressione, struttura, interop e tool. Tag: **[spina]** invariante · **[transform]** cuore armonico · **[gesto]** modo di riempire una track · **[sistema]** qualità/robustezza · **[interop]** verso il mondo esterno · **[tool]** solo laptop · **[hw]** solo device fisico.

### 26.1 Spina dorsale MIDI realtime — [spina]
- Transport a tick interi (PPQN 96, pronto a 192), `bpm_x100`, play/stop/continue, locate/song-position.
- Engine MIDI multi-porta: **DIN in/out + USB in/out**, ognuna indirizzabile; parsing compliant (running status, v0→Off, real-time interleaved).
- Out Scheduler timestampato con look-ahead, ordine deterministico (NoteOff prima di NoteOn), throttling bandwidth DIN, prioritizzazione (clock > note > CC).
- Router/matrice in→out, thru/soft-thru, filtri per canale/tipo/range, merge input, mute-thru-in-record.
- Note/Voice tracker + **Panic** (All Notes Off / All Sound Off / Reset Controllers) per porta/canale, anti-stuck.
- Clock master/slave con PLL/filtro anti-jitter, Start/Stop/Continue/SPP, drift handling.
- Compliance completa (checklist §9) come feature di prima classe.

### 26.2 Transform armonico — il cuore — [transform]
- **Chord intelligence da input scarno** → accordo pieno, modi selezionabili: **B diatonico key-aware**, **A single-finger assoluto**, **C shell/parziale→completamento**, (D ibrido intelligente come sogno lontano).
- Key/Scale engine (tonalità, scala, modo), scale-lock opzionale.
- Voicing / voice-leading resolver (close/open/drop-2, root-position, smoothing tra accordi), clamp a range device.
- Bass/inversion resolver (root, on-bass/slash chord, walking).
- Re-harmonize: applicare una nuova progressione a materiale esistente (pattern/loop) che "segue".
- Transpose/octave engine globale + per-track, con regola "chi segue l'accordo e chi no".
- Chord-hold / chord-memory; suggerimenti/estensioni (sogno).

### 26.3 Primitiva Timeline / Track — [spina/transform]
- **Timeline** di eventi bounded, deterministica; **Track** = ruolo + destinazione (port+channel) + policy transform (follow-chord? transpose? voicing?).
- Ogni track riempibile con **3 gesti** (scrivi / genera-da-accordo / cattura), intercambiabili.
- Mute/solo, track enable mask richiamabile; per-track length (polymeter).
- Snapshot/Undo ring bounded riusabile.

### 26.4 Gesto "Genera-da-accordo" — Arranger — [gesto]
- **ChordSequence** registrabile/editabile/loopabile/trasponibile come sorgente d'armonia (dal chord engine).
- Style = Section[] (Intro 1/2, Variation A–D, Fill A–D, Break, Ending 1/2), pattern relativi ai gradi per ruolo.
- Ruoli: Drums/Perc (fissi), Bass/Chord1/Chord2/Pad/Arp/Phrase/Lead/CC con policy follow-chord individuale.
- Section switching quantizzato (bar/beat), auto-fill on-change, intro/ending one-shot.
- Split/Full-keyboard, Zone (key-range→destinazione), single-finger/fingered/full-keyboard recognition.

### 26.5 Gesto "Scrivi" — Sequencer profondo — [gesto]
- Step per-track con per-step **locks**: velocity, gate/length, tie, rest, probability, ratchet, micro-timing, **conditional trig** ("1 su N", "solo nel fill").
- Polymeter, CC-lanes (automazione), pattern chain, song mode.

### 26.6 Gesto "Cattura" — MIDI Looper — [gesto]
- Record/overdub/replace/erase/undo, quantize-after (non distruttivo), loop length (fissa/auto/quantizzata), per-track o global.
- **Capture retroattivo** (ring sempre-attivo: "prendi le ultime N battute").
- Sync ad arranger e a clock esterno; catture che possono **follow-chord** (re-harmonize).

### 26.7 Espressione — [gesto/transform]
- **Arpeggiator** riusabile (up/down/updown/random/as-played/chord-repeat/gated, octave, latch, sync, pattern/rhythm arp), alimentato da note tenute *o* dal ChordState.
- **Phrase/Pad engine**: pad phrase/chord/drum/CC/fill/scene, modi one-shot/loop/hold/toggle, sync to beat/bar, fixed vs transpose-with-chord; banchi da 4.
- **Groove/Humanize** deterministico (swing, micro-timing, velocity, PRNG seedato).
- Metronome/click, tap-tempo, tempo-nudge.

### 26.8 Struttura & richiamo — [sistema]
- **Performance/Registration** richiamabile (style, variation, split, mute mask, routing, transpose, program refs, pad map, arp state, chord seq).
- **Song/Scene**: scene snapshot temporizzate, chaining, tempo map, time-signature engine.
- **SetList** (scaletta di performance/song).
- Project/Preset manager (slot, default).

### 26.9 Interop esterna — [interop]
- **DeviceProfile** (mappa drum, CC noti, range, velocity curve, init/panic, quirks) + **Program/ExternalSound** (port+ch+bank MSB/LSB+PC+CC init+transpose+range).
- Program/Bank/PC emission ordinata all'attivazione; RPN/NRPN, 14-bit CC, pitch-bend, aftertouch/poly-pressure, sustain.
- **MIDI Learn** / ControllerMap (controllo fisico → funzione).
- **SysEx** (backup/device profile, bounded/chunked) — [interop, opz.].

### 26.10 Sistema / qualità — [sistema]
- Determinismo totale, golden-test harness, replay, fault injection.
- Diagnostics/MIDI monitor, jitter/latency meter.
- Storage binario versionato + CRC + graceful degradation; watchdog/crash-recovery (all-notes-off al boot) — [hw].
- Firmware update — [hw, valutare].

### 26.11 Tool laptop — [tool]
- Style compiler (YAML/JSON → binario, con **limit validator** anti-sforo STM32).
- Pattern/style editor, device-profile editor.
- **SMF import/export** (Type 1), regression runner, fault injector, debug UI (che riusa il futuro `ui_model`).

### 26.12 Sogni "oltre l'orizzonte" (da valutare, non impegnativi)
- Re-harmonization intelligente e variation/mutation generativa dei pattern.
- Scale-lock lead assistito, chord-scale suggestions.
- Multi-progetto/backup via SysEx o storage esterno; profili device condivisibili.
- MIDI 2.0 / MIDI-CI (solo astrazione ora).
- UI fisica ricca (pad/encoder/display) quando in scope — [hw].

---

## 27. Roadmap di milestone (ordinata — supersede l'ordine indicativo di §23)

Principi: ogni milestone ha **exit criteria dimostrabili via CLI + virtual MIDI + golden test**, il **dual-build (host + arm-none-eabi) resta verde da M0**, l'UI fisica è **out of scope** finché non deciso (interazione via CLI). "Primo WOW" = **M3**, "secondo WOW" = **M5**.

| Milestone | Obiettivo (cosa entra) | Gesto/feature sbloccato | Exit criteria (demo) |
|---|---|---|---|
| **M0 — Fondamenta & dual-build & harness** | Repo scaffold, **CMake 2 toolchain** (host GCC16 + arm-none-eabi cortex-m stub), `StaticVector`/`Span`/`RingBuffer`/`fixed`/`crc`, Transport/Clock (96 PPQN, `bpm_x100`), MIDI In parse, Out scheduler, **multi-porta HAL (DIN+USB astratti)**, Router+thru, Note tracker+**Panic**, headless golden runner, **CLI shell**, host virtual-MIDI backend. | [spina] | nota-in → router → nota-out con timing corretto su virtual MIDI (aseqdump/Bitwig); golden test verde; **il core cross-compila per arm** in CI. |
| **M1 — Timeline & Track** | La primitiva `Timeline × Track × Transform` con gesto "scrivi" minimo (step base) e playback; Track = ruolo+destinazione; mute/solo; per-track length. | [gesto scrivi minimo] | definisci track via CLI, riproduci pattern verso DAW, timing/golden ok. |
| **M2 — Chord Engine diatonico (B) + harmonizer live** | Key/Scale engine; **input nota → accordo diatonico key-aware**; output live verso synth; chord-hold. | [transform: modo B] | `key=C; play D → Dm7` esce su virtual MIDI; golden. |
| **M3 — Chord Sequencer 🌟 PRIMO WOW** | Registri la progressione da input scarno, **edit/loop/transpose**, playback che emette accordi; output doppio (harmonizer live + ChordSequence). | [primo prodotto] | costruisci `\| Dm7 \| G7 \| Cmaj7 \| Am7 \|` via CLI/MIDI, loop, trasponi, suona verso gear; golden. |
| **M4 — Chord modes A & C** | Aggiungi **A single-finger assoluto** e **C shell/parziale→completamento**, commutabili (`chord_mode`). | [transform: modi A/C] | stessi test nei tre modi; golden per modo. |
| **M5 — Arranger 🌟 SECONDO WOW** | Style + Section[] (Intro/VarA-B/Fill/Ending), ruoli con policy follow-chord, **voicing/voice-leading resolver**, section switching quantizzato, guidato da ChordSequence o input live; Program/Bank init base. | [gesto genera-da-accordo] | ChordSequence → band multi-track verso gear, cambi Variation/Fill; golden. |
| **M6 — Sequencer profondo** | Per-step locks (velocity/prob/ratchet/tie/rest/micro-timing/**conditional trig**), polymeter, CC-lanes, pattern chain. | [gesto scrivi ricco] | pattern chirurgici deterministici; golden. |
| **M7 — MIDI Looper** | Record/overdub/replace/undo/erase, quantize-after, **capture retroattivo**, per-track/global loop, sync; catture **follow-chord** (re-harmonize). | [gesto cattura] | loop live + re-harmonize su cambio ChordSequence; golden. |
| **M8 — Espressione** | Arpeggiator riusabile (da note o ChordState), Phrase/Pad engine (banchi da 4), Groove/Humanize deterministico, metronome/tap-tempo. | [espressione] | arp segue ChordSequence; pad triggera phrase/chord/fill; golden. |
| **M9 — Struttura & richiamo** | Song/Scene (snapshot+chaining+tempo/time-sig), **Performance/Registration**, SetList. | [struttura] | richiami performance, concateni scene, riproduci una song. |
| **M10 — Interop esterna** | DeviceProfile + Program/ExternalSound, Program/Bank/PC init ordinato, velocity curve, RPN/NRPN/pitch-bend/aftertouch/sustain, MIDI Learn/ControllerMap. | [interop] | attivi program → init verso device reale; mappi un controller. |
| **M11 — Persistenza & robustezza** | Storage binario versionato + **CRC**, save/load project, graceful degradation; Diagnostics/MIDI monitor; **fault injection** suite. | [sistema] | save/load round-trip con CRC; regression ampia + fault injection verdi. |
| **M12 — Tool laptop** | Style compiler (YAML→bin) + **limit validator** anti-sforo, pattern/device editor, **SMF import/export**. | [tool] | compili uno style da YAML validando i budget; esporti/importi SMF. |
| **M13 — STM32 port reale** | HAL STM32 (USB-MIDI device + UART DIN + timer + storage), budget validation reale, watchdog/crash-recovery; UI fisica (pad/encoder/display) **se/quando in scope**. | [hw] | gira su hardware con DIN+USB; panic e sync verificati sul device. |

**Note di percorso:** M0–M2 sono la *spina condivisa* (identiche per qualunque identità). M3 è il primo traguardo "prodotto" mostrabile. M5 chiude il gesto-eroe (arranger). Da M6 in poi l'ordine è più flessibile e si può riprioritizzare in base a ciò che senti mancare "con l'oggetto in mano". La UI fisica e la scelta HW specifica restano deliberatamente rimandate (D7), senza mai bloccare il core.
- Prevedere `app/core/device/` (DeviceProfile/Program) e `app/core/performance/` già presente.

---

## 28. CLI API Design (definitivo — architettura a 3 livelli, D23)

**Filosofia (D22 + D17 + D23).** La CLI `arrangrr` è un **thin client** su un core headless, progettata in **3 livelli sovrapposti** — così una sola API onora insieme ergonomia musicale, apertura e determinismo:

| Livello | Nome | Cos'è | A chi serve |
|---|---|---|---|
| **L2** | **Superficie — "Musician REPL"** | Sugar terso verb-first (`key C major`, `play D`, `loop on`, `start`). Alias comodi, poca punteggiatura, pensato per **suonare a mano**. | Uso live, minimal-deep. |
| **L1** | **Modello — path indirizzabili** | Ogni comando è un'operazione su uno **spazio parametri** (`get`/`set`/`do <path>`). Ogni stato è leggibile/settabile/**mappabile (MIDI-learn)**/automatizzabile in modo uniforme (D17b). | Apertura/hackability, tooling. |
| **L0** | **Filo — protocollo JSONL** | Wire **machine-first**: `{"cmd":…}` → / `{"ev":…}` ←, versionato. Deterministico, replay, **daemon+client, GUI futura** (D17a, D16). | Integrazione, golden, GUI. |

**Regola di espansione:** il sugar L2 si **espande** in operazioni L1 (`play D` → `do chord.play D`), che **serializzano** in messaggi L0 (`{"cmd":"chord.play","note":"D"}`). Nessuna magia nascosta: `--echo-expand` mostra l'espansione L2→L1→L0. La CLI può parlare a **qualsiasi livello** (`--format human|jsonl`, `--sugar on|off`). **Attenzione (D26, fix Francesco #1/#2):** L0-JSONL con path-stringa è l'**encoding host** (per CLI/GUI/tool); il **core NON vede JSONL né stringhe** — riceve `Command` **binari tipizzati** (`{op, coll:u16, idx:u16, param_id:u16, value}`) su ring buffer ed emette `Event` POD. La traduzione *stringa/JSONL → binario* avviene in `platform/host`. Così niente parser JSON né `std::string` nel path realtime STM32; le collezioni sono array a capacità fissa e i nomi utente restano host-side (device standalone: slot numerici).

### 28.1 Invocazione
```
arrangrr [--backend alsa|jack|pipewire|null]   # sink/source MIDI host (default: auto)
         [--in <port|alias>]... [--out <port|alias>]...
         [--clock real|virtual]                # virtual = deterministico (batch)
         [--ppqn 96] [--tempo 120]
         [--script FILE | -]                   # esegue comandi (o stdin); poi esce se non --repl
         [--replay FILE]                        # file con timestamp @tick, clock virtuale, event-log canonico
         [--events all|none|<filter>]           # verbosità stream eventi
         [--load STATEFILE] [--seed N]
         [--format human|jsonl]                 # forma di ack/eventi
```
- **Nessun arg** → REPL, backend auto, clock reale, event stream sui tipi principali.
- **`--script s.acmd`** (o `arrangrr < s.acmd`) → esegue e (senza `--repl`) esce. Con `--clock virtual` è deterministico.
- **`--replay r.acmd`** → clock virtuale guidato dai timestamp, stampa **event-log canonico** su stdout per il **golden diff**.

### 28.2 Sintassi
- Una riga = un comando: `namespace verb [args…]`. Comandi frequentissimi hanno alias brevi.
- **Path parametri** con punto: `set transport.tempo 120`, `get track.bass.dest.channel`.
- **Commenti** `# …`. **Timestamp** opzionale in testa (solo batch/virtual): `@<tick>` o `@<bar:beat:tick>`; assente ⇒ "ora / prossimo tick".
- Note: nome (`C`, `F#3`, `Bb`) o numero MIDI (`60`). Tempo BPM: `120` o `120.00` (interno `bpm_x100`).
- **Risposte:** `ok [valore]` / `err <codice> <msg>`. In `--format jsonl`, ack ed eventi sono oggetti JSON per-riga.

### 28.3 Namespaces & comandi (v0)

**Transport / clock**
```
transport start | stop | continue | toggle
transport tempo <bpm> | transport ppqn <n>
transport locate <bar:beat:tick> | transport sync internal|external
advance <ticks|Nbars>          # SOLO clock virtual: avanza il tempo in modo deterministico
```

**Porte / routing** (multi-porta DIN+USB → su host sono porte virtuali)
```
port list | port open in|out <name> [as <alias>]
route <in>[:ch] -> <out>[:ch] [drop cc|note|clock … | only …]
thru <in> -> <out> [soft|hard|off]
panic [<port>|all]
```

**Tonalità / scala**
```
key <root> <mode>              # key C major | key A minor
scale <name>                   # override scala corrente
```

**Chord intelligence** (D19 smart per grado, D20 diatonico+modificatori)
```
chord mode diatonic|single|shell
chord play <note> [mod …]      # mod: maj7 min7 dom7 dim7 sus2 sus4 add9 9 11 13
                               #      sec (dominante secondaria) borrow (prestito) inv<n>
chord hold on|off | chord stop
```
Default: ricchezza *smart* per grado; i `mod` la sovrascrivono. Esempi:
```
key C major
chord play D            # -> Dm7  (ii, smart)
chord play G            # -> G7   (V, smart)
chord play G mod sec    # -> D7   (V/V, dominante secondaria)
chord play A mod borrow # -> Ab   (bVI prestito) [esplicito]
```

**Chord sequence** (D14 — durate libere; live-rec quantize-after o step-edit)
```
seq new <name> | seq use <name>
seq rec [quantize <grid>] | seq stop
seq add <chord> [len <beats|bars>]     # step entry a durata libera
seq edit <i> [chord <c>] [len <d>] | seq del <i>
seq loop on|off | seq transpose <±semi | to <key>>
seq play [<name>] | seq show [<name>] | seq list
```

**Track** (la primitiva; 3 gesti = 3 modi di riempire)
```
track new <name> role <role> dest <out>:<ch>
track fill <name> write|generate|capture
track follow <name> on|off             # segue l'accordo (transform)
track voicing <name> close|open|drop2|smooth
track mute|solo|unmute <name>
```

**Arranger / style** (sintassi prevista per M5)
```
style load <name> | style section intro1|varA|varB|fill|break|ending1
```

**Modello parametri indirizzabile** (D17b)
```
get <path> | set <path> <value> | ls <path>
# es: transport.tempo · chord.mode · track.bass.dest.channel · seq.verse.loop
```

**Stato / persistenza** (D21)
```
state dump [file] | state load <file> | state inspect [path]
project save <name> | project load <name>     # binario versionato + CRC
```

**MIDI raw / monitor / learn** (D17a/c/d)
```
midi send <port> <bytes…>
monitor on|off [filter: midi|chord|clock|section|<ev-type>]
learn <param-path>            # poi muovi un controllo -> mappato
map list | map del <path>
```

**Meta**
```
help [topic] | echo <text> | wait <ms|Nt> | seed <N> | quit
```

### 28.4 Stream eventi (←)
Ogni evento è una riga `ev <type> <campi…>` (o JSON con `--format jsonl`), filtrabile con `monitor`:
```
ev clock    tick 96 bar 1 beat 1
ev chord    in C -> Cmaj7 deg I
ev midi-out port synth ch 1 noteon 60 vel 100 @0
ev section  varA
ev warn     buffer-full dropped cc
```
In **replay/batch** l'event-log ordinato è l'**output canonico** confrontato col golden file.

### 28.5 Determinismo & replay (bridge M0)
- Con `--clock virtual` il tempo avanza **solo** con `advance`/`@tick` ⇒ un file sessione con timestamp produce **output identico** (golden). `seed N` fissa il PRNG (humanize/probability) quando serve; senza, il live può variare (D16).
- Esempio sessione deterministica (`hello_chord.acmd`):
```
# key + un accordo diatonico smart, verso una porta virtuale
key C major
port open out virt as synth
chord mode diatonic
@0   chord play D          # ev: Dm7 -> noteon…
@96  chord stop
advance 192
```
  `arrangrr --clock virtual --replay hello_chord.acmd` → event-log deterministico → `diff` con golden.

### 28.6 Esempio REPL live (primo WOW, M3)
```
arrangrr> key C major
arrangrr> port open out virt as synth
arrangrr> seq new verse
arrangrr> seq rec quantize 1/1
arrangrr> chord play D        # Dm7
arrangrr> chord play G        # G7
arrangrr> chord play C        # Cmaj7
arrangrr> chord play A        # Am7
arrangrr> seq stop
arrangrr> seq loop on
arrangrr> transport start     # la progressione gira e suona verso 'synth'
arrangrr> seq transpose +2    # tutta la progressione sale di un tono, live
```

### 28.7 Convenzioni risolte (D23) & mapping dei livelli
- **Sintassi:** L2 usa `verb` / `namespace verb` **con lo spazio** (musicista: `play D`, `seq verse`); L1 usa **path puntati** (`do chord.play D`, `set seq.verse.loop on`, `get transport.tempo`). Non sono "due stili in conflitto": sono **due livelli** — il primo si espande nel secondo. `--sugar off` obbliga la forma L1.
- **Espansione tracciabile:** `--echo-expand` stampa per ogni riga la catena `L2 → L1 → L0`, così la superficie non è mai magica (coerente con "aperto/ispezionabile").
- **Tempo in batch:** entrambi i meccanismi — prefisso `@<tick>`/`@<bar:beat:tick>` per **schedulare** una riga, e `advance <ticks|Nbars>` per **far avanzare** il clock virtuale in modo deterministico.
- **Formato:** `--format human` (default, L2/L1 leggibile) · `--format jsonl` (L0 canonico, usato per replay/golden e per client/GUI). L'event-log JSONL ordinato è l'artefatto **golden**.
- **Mapping esempio (tutti e 3 i livelli, stessa azione):**
```
L2 (digiti):   play D
L1 (modello):  do chord.play note=D
L0 (filo):     {"cmd":"chord.play","note":"D"}
   evento ←:   {"ev":"chord","in":"D","out":"Dm7","deg":"ii"}
               {"ev":"midi-out","port":"synth","ch":1,"noteon":62,"vel":100,"@":0}
```

### 28.8 Task aperto: schema dello spazio-parametri (L1)
Lo **schema completo dei path indirizzabili** (`transport.*`, `chord.*`, `seq.<name>.*`, `track.<name>.*`, `port.*`, `style.*`, `state.*`, `map.*`) è **esso stesso una mini-spec** e va definito in **M0/M1** insieme al protocollo L0 (versionato). È il contratto che regge CLI, MIDI-learn, automazione, replay e (domani) GUI: va disegnato una volta e con cura. Principi: nomi stabili, tipi espliciti, unità dichiarate (tick, `bpm_x100`, semitoni), enum chiusi, ogni path `get`-abile e (dove sensato) `set`/`learn`-abile. **La spec v0 è §29.**

---

## 29. L1 Param-Space & L0 Protocol — Spec v0 (contratto)

Questa sezione è il **contratto**: lo spazio parametri indirizzabile (**L1**) e il protocollo wire (**L0**). La superficie musicista (**L2**) è puro sugar che si espande in operazioni L1. Copre ciò che serve fino a **M5** (arranger); i namespace successivi (looper, arp, pad, performance, device) si aggiungono con lo stesso schema.

### 29.1 Tipi & convenzioni
- **Tipi:** `bool` · `int` · `int(a..b)` (range) · `fixed(bpm_x100)` (intero ×100) · `enum{…}` · `string` · `note` (nome `C`/`F#3`/`Bb` **o** 0..127) · `pos` (`bar:beat:tick`) · `ticks` · `semitones` · `array<T>` · `id` (string alias).
- **Accesso:** `r` (get) · `rw` (get+set) · `do` (azione). **`learn`** = path bindabile via MIDI-learn.
- **Unità sempre dichiarate.** Nessun float sul filo per valori realtime: il tempo è `bpm_x100`, le durate in `ticks`/`bars`.
- **Collezioni** con path parametrico: `seq.<name>.*`, `track.<name>.*`, `port.<alias>.*`. Il nome è un `id` stabile scelto dall'utente.
- **Enum chiusi e versionati:** aggiungere un valore = bump di `proto` minor.

### 29.2 Catalogo L1 (v0)

**transport.**
| path | tipo | accesso | note |
|---|---|---|---|
| `transport.tempo` | fixed(bpm_x100) | rw · learn | default 12000 (=120.00) |
| `transport.ppqn` | int | rw | default 96; settabile solo a stop |
| `transport.state` | enum{stopped,playing,paused} | r | |
| `transport.position` | pos | r | bar:beat:tick |
| `transport.sync` | enum{internal,external} | rw | slave a clock esterno |
| `transport.start` / `.stop` / `.continue` / `.toggle` | — | do · learn | |
| `transport.locate` | do(pos) | do | ricostruisce stato arranger |
| `transport.advance` | do(ticks\|bars) | do | **solo clock virtual**: avanza deterministicamente |

**key. / scale.**
| path | tipo | accesso |
|---|---|---|
| `key.root` | enum{C,C#,D,…,B} | rw · learn |
| `key.mode` | enum{major,minor,dorian,phrygian,lydian,mixolydian,locrian} | rw · learn |
| `scale.name` | string | rw | override esplicito |

**chord.** (D19 smart, D20 diatonico+modificatori)
| path | tipo | accesso | note |
|---|---|---|---|
| `chord.mode` | enum{diatonic,single,shell} | rw · learn | default diatonic (B) |
| `chord.richness` | enum{smart,triad,seventh,extended} | rw | default smart |
| `chord.hold` | bool | rw · learn | |
| `chord.current` | string | r | ultimo accordo riconosciuto (es. "Dm7") |
| `chord.play` | do(note, mod:array<enum>?) | do · learn | mod: `maj7 min7 dom7 dim7 halfdim sus2 sus4 add9 9 11 13 sec borrow inv1 inv2 inv3` |
| `chord.stop` | — | do | |

**seq.** (collezione di ChordSequence; D14 durate libere)
| path | tipo | accesso | note |
|---|---|---|---|
| `seq.<n>.loop` | bool | rw · learn | |
| `seq.<n>.length` | ticks | r | |
| `seq.<n>.transpose` | semitones | rw · learn | |
| `seq.<n>.playing` | bool | r | |
| `seq.<n>.steps` | array<{i,chord,len_ticks}> | r | struttura ispezionabile |
| `seq.new` | do(name) | do | |
| `seq.use` | do(name) | do | seq "corrente" per i comandi brevi |
| `seq.<n>.rec` | do(quantize?) | do · learn | live-rec, quantize-after |
| `seq.<n>.stop` | — | do | |
| `seq.<n>.add` | do(chord, len?) | do | step-entry, durata libera |
| `seq.<n>.edit` | do(i, chord?, len?) | do | |
| `seq.<n>.del` | do(i) | do | |
| `seq.<n>.play` / `.show` | — | do | |
| `seq.<n>.transpose!` | do(by\|to) | do · learn | azione (oltre al leaf rw) |
| `seq.list` / `seq.count` | do / r | | meta collezione |

**track.** (la primitiva; 3 gesti)
| path | tipo | accesso | note |
|---|---|---|---|
| `track.<n>.role` | enum{drums,perc,bass,chord1,chord2,pad,arp,phrase,lead,cc} | rw | |
| `track.<n>.dest.port` | id | rw | alias porta out |
| `track.<n>.dest.channel` | int(1..16) | rw · learn | |
| `track.<n>.fill` | enum{write,generate,capture} | rw | quale gesto riempie la track |
| `track.<n>.follow` | bool | rw · learn | segue l'accordo (transform) |
| `track.<n>.voicing` | enum{close,open,drop2,smooth,root} | rw | |
| `track.<n>.transpose` | semitones | rw · learn | |
| `track.<n>.octave` | int | rw · learn | |
| `track.<n>.mute` / `.solo` | bool | rw · learn | |
| `track.<n>.length` | int(steps) | rw | polymeter |
| `track.new` | do(name, role, dest) | do | |
| `track.<n>.remove` | — | do | |

**style.** (arranger, M5 — contratto già ora)
| path | tipo | accesso |
|---|---|---|
| `style.current` | string | r |
| `style.load` | do(name) | do |
| `style.section` | enum{intro1,intro2,varA,varB,varC,varD,fillA,fillB,fillC,fillD,break,ending1,ending2} | rw · learn |
| `style.section.trigger` | do(name) | do · learn | (quantizzato al confine) |

**port. / route. / thru.** (multi-porta DIN+USB; su host = virtuali)
| path | tipo | accesso |
|---|---|---|
| `port.list` | do → array | do |
| `port.open` | do(dir:enum{in,out}, name, as?) | do |
| `port.close` | do(alias) | do |
| `port.<alias>.dir` | enum{in,out} | r |
| `route.add` | do(from:"alias[:ch]", to:"alias[:ch]", filter?) | do |
| `route.list` / `route.del` | do / do(id) | do |
| `thru.set` | do(from, to, mode:enum{soft,hard,off}) | do |
| `panic` | do(port?) | do · learn |

**map.** (MIDI-learn — D17c)
| path | tipo | accesso |
|---|---|---|
| `map.learn` | do(param-path) | do | arma il learn: la prossima sorgente MIDI → bind |
| `map.add` | do(param-path, source:"port:ch:cc\|note") | do |
| `map.list` / `map.del` | do / do(param-path) | do |

**state. / project.** (D21)
| path | tipo | accesso |
|---|---|---|
| `state.dump` | do(file?) | do | snapshot testo/JSON ispezionabile |
| `state.load` | do(file) | do |
| `state.inspect` | do(path?) | do |
| `state.seed` | int | rw | seed PRNG (humanize/probability) |
| `project.save` / `project.load` | do(name) | do | binario versionato + CRC |

**monitor. / meta.**
| path | tipo | accesso |
|---|---|---|
| `monitor.set` | do(filter:enum{all,none,midi,chord,clock,section,warn}, on:bool) | do |
| `meta.hello` | do(proto) | do | handshake versione |
| `meta.version` | string | r |
| `meta.help` | do(topic?) | do |
| `meta.echo` / `meta.wait` / `meta.quit` | do | | `wait` in ms (real) o ticks (virtual) |

### 29.3 Protocollo L0 (JSONL, versionato)
Un oggetto JSON per riga, UTF-8. **client→core = comandi**, **core→client = risposte + eventi**.

**Envelope comando** (una qualsiasi delle 4 op — `get`/`set`/`do`/`ls` — su un path L1):
```json
{"op":"do","path":"chord.play","args":{"note":"D"},"@":0,"id":5}
{"op":"set","path":"transport.tempo","value":12000}
{"op":"get","path":"seq.verse.loop","id":6}
{"op":"ls","path":"chord"}
```
- `@` (opzionale): tick assoluto di esecuzione (clock virtual). Assente ⇒ "adesso/prossimo tick".
- `id` (opzionale): correlazione; la risposta la riecheggia in `re`.

**Risposte** (core→client):
```json
{"re":6,"ok":true,"value":true}
{"re":5,"ok":true}
{"re":7,"ok":false,"err":"bad_note","msg":"unknown note 'H'"}
```

**Eventi** (core→client, non sollecitati; filtrati da `monitor`):
```json
{"ev":"chord","in":"D","out":"Dm7","deg":"ii","@":0}
{"ev":"midi-out","port":"synth","ch":1,"msg":"noteon","note":62,"vel":100,"@":0}
{"ev":"midi-in","port":"kbd","ch":1,"msg":"noteon","note":62,"vel":100,"@":0}
{"ev":"clock","tick":96,"bar":1,"beat":2,"@":96}
{"ev":"section","name":"varA","@":384}
{"ev":"transport","state":"playing","@":0}
{"ev":"warn","code":"buffer_full","detail":"dropped cc","@":123}
```

**Handshake / versione** (prima riga):
```json
-> {"op":"do","path":"meta.hello","args":{"proto":1}}
<- {"ev":"hello","proto":1,"core":"arrangrr 0.1.0","ppqn":96,"format":"jsonl"}
```
Codici errore (enum stabile): `bad_path` · `bad_arg` · `bad_note` · `not_found` · `read_only` · `busy` · `unsupported` · `overflow`.

### 29.4 Espansione L2 → L1 → L0 (esempi)
```
L2:  play D
L1:  do chord.play note=D
L0:  {"op":"do","path":"chord.play","args":{"note":"D"}}

L2:  loop on
L1:  set seq.<current>.loop on
L0:  {"op":"set","path":"seq.verse.loop","value":true}

L2:  xpose +2
L1:  set seq.<current>.transpose +2
L0:  {"op":"set","path":"seq.verse.transpose","value":2}

L2:  play G mod sec
L1:  do chord.play note=G mod=[sec]
L0:  {"op":"do","path":"chord.play","args":{"note":"G","mod":["sec"]}}
```

### 29.5 Sessioni golden (deterministiche, clock virtual)
Formato `.acmd` (L2 sugar con `@tick`); l'esecuzione con `--clock virtual --format jsonl` produce l'event-log canonico confrontato col `.golden`. (Note MIDI e tick illustrativi; i valori esatti si fissano quando il core esiste — questi file *sono* la spec del comportamento atteso.)

> **Golden robusti (Francesco #4):** separa i golden di **protocollo/timing** (stabili: ordine eventi, tick, porte, `@`) da quelli **musicali**. Per l'armonia **asserisci su identità-accordo + grado** (`{"ev":"chord","out":"Dm7","deg":"ii"}`), **non** sulle note MIDI esatte del voicing — altrimenti ogni ritocco al voicing/NTT riscrive tutti i golden e i test diventano rumore. Le note esatte si asseriscono solo in golden di voicing dedicati.

**G1 — `hello_chord.acmd`** (un accordo diatonico smart → note out)
```
meta.hello proto=1
key C major
port open out virt as synth
chord mode diatonic
@0    play D            # ii -> Dm7 (smart)
@96   chord stop
advance 192
```
Atteso (estratto):
```json
{"ev":"chord","in":"D","out":"Dm7","deg":"ii","@":0}
{"ev":"midi-out","port":"synth","ch":1,"msg":"noteon","note":62,"vel":100,"@":0}
{"ev":"midi-out","port":"synth","ch":1,"msg":"noteon","note":65,"vel":100,"@":0}
{"ev":"midi-out","port":"synth","ch":1,"msg":"noteon","note":69,"vel":100,"@":0}
{"ev":"midi-out","port":"synth","ch":1,"msg":"noteon","note":72,"vel":100,"@":0}
{"ev":"midi-out","port":"synth","ch":1,"msg":"noteoff","note":62,"@":96}
... (altre noteoff @96)
```

**G2 — `ii_V_I_vi.acmd`** (progressione a durate libere, loop, transpose live)
```
key C major
port open out virt as synth
seq new verse
seq.verse rec quantize 1/1
@0    play D            # Dm7
@384  play G            # G7   (1 bar dopo, a 96ppqn*4)
@768  play C            # Cmaj7
@1152 play A            # Am7
@1536 seq.verse stop
seq.verse loop on
transport start
advance 1536            # un giro di loop
seq.verse transpose +2  # tutta la progressione +2 semitoni, live
advance 1536
```
Atteso: eventi `chord` per Dm7/G7/Cmaj7/Am7 al primo giro; dopo `transpose +2`, gli stessi gradi con root alzata (Em7/A7/Dmaj7/Bm7) al secondo giro. L'ordine e i tick sono deterministici.

**G3 — `routing_panic.acmd`** (thru + iniezione nota + panic)
```
port open in virt as kbd
port open out virt as synth
thru set kbd synth soft
@0    midi send kbd 90 3C 64     # noteon C4 su kbd
@10   panic all
advance 48
```
Atteso: `midi-in noteon 60`, `midi-out noteon 60` (soft-thru), poi al panic `midi-out` con All-Notes-Off/All-Sound-Off/Reset e `noteoff 60` esplicito (anti-stuck) su ogni canale attivo.

**G4 — `secondary_dominant.acmd`** (modificatore esplicito, D20)
```
key C major
port open out virt as synth
@0    play G mod sec     # V/V -> D7 (dominante secondaria)
@96   chord stop
advance 192
```
Atteso: `{"ev":"chord","in":"G","out":"D7","deg":"V/V","@":0}` + note di D7.

### 29.6 Cosa resta da fissare quando esiste il core
- Voicing/ottava esatti di default per grado (le note precise negli event-log golden).
- Politica note-off del loop al cambio accordo (legato vs re-trigger).
- Nomi finali di alcuni enum (es. sezioni oltre M5).
- Se `@tick` implica auto-`advance` fino a quel tick in modalità script "densa" (proposto: no — `advance` è esplicito, `@` solo scheduling).

### 29.7 Nit di review foldati (Francesco minor/major)
- **Modificatori accordo ad assi ORTOGONALI (M11):** `chord.play` non prende un array piatto ma **assi separati** — `quality` (maj/min/dom/dim/halfdim/…), `extensions[]` (add9/9/11/13/…), `function` (diatonic/sec/borrow), `inversion` (0..3). Validazione conflitti per-asse; niente `[min7,maj7]` ambigui.
- **Durate in UNITÀ MUSICALI (M12):** step/pattern memorizzano durate in **beat/frazioni** (i tick a 960 sono *derivati*), così cambiare risoluzione non reinterpreta i contenuti. `transport.ppqn` comunque immutabile **dopo** il caricamento di contenuti.
- **`state.dump` NON tocca il filesystem nel core (M13):** l'op core è `serialize → Span<byte>`; la scrittura file vive in `platform/host`. `state.dump [file]` è zucchero host (coerente D26).
- **`scale.name` = enum/registry chiuso** (non stringa libera), come `key.mode` → validabile/deterministico.
- **`deg` strutturato negli eventi** (`{"degree":2,"quality":"min7","alt":[]}`), non stringa libera, per consumo macchina.
- **Spelling enarmonico key-aware** (Ab vs G#): `chord.current`/display usano un algoritmo di spelling in funzione della tonalità — da definire in M2.
- **Sorgente velocity harmonizer:** dinamica dell'accordo generato = (a) velocity dell'input o (b) valore fisso per-track; **default = input**.
- **Compliance §9 completata:** oltre a CC64 (sustain) includere **CC66 (sostenuto)** e **CC67 (soft)**; nota parser: i byte real-time (F8/FA/…) **non resettano** il running status.
- **Chord-detection window + isteresi (M8):** i modi live usano una **finestra** di aggregazione note + **isteresi** (non cambiare accordo finché stabile per N ms) per evitare chord-flicker → param `chord.window_ms`, `chord.hysteresis_ms`.
- **QEMU (M0):** il gate ARM è **compile+link freestanding**, non "run con periferiche"; un run QEMU è bonus, non criterio.
- **Polymeter × section-switch:** con track di lunghezze diverse, il "confine" di switch è definito sul **bar globale del transport** (non sulla lunghezza della singola track); da dettagliare in M5/M6.

---

## 30. Valutazione feature "auto-accompaniment" (le 17 classiche)

Mappatura delle 17 feature "la tastiera suona da sola" sul progetto. **Verdetto: 12 già coperte, 5 additive/da elevare.** Conferma che il modello Timeline Vivente + chord-intelligence-first le contiene tutte. La catena che l'utente sintetizza — *input umano minimale → interpretazione armonica → generazione MIDI intelligente* — **è** esattamente il north-star (D11/D15) e il primo/secondo WOW.

| # | Feature | In arrangrr? | Dove | Milestone | Nota |
|---|---|---|---|---|---|
| 1 | **Arranger** | ✅ core | §11, §26.4 (gesto genera-da-accordo) | M5 | è il 2° WOW |
| 2 | **Chord recognition** (single/fingered/**multi**/full/**bass-inv**) | ✅ (add multi-finger) | §11, D12 | M2/M4 | multi-finger = variante da aggiungere; bass-inversion/slash già in §11 |
| 3 | **Style engine** | ✅ | §26.4, §8 | M5 | frasi MIDI adattate, non audio |
| 4 | **NTT / Note Transposition Table** | 🔼 **elevato** | D24, §8.2 | M5 (concetto da ora) | era annegato nel voicing resolver → ora modulo core |
| 5 | **Arpeggiator** | ✅ | §14, §26.7 | M8 | ArpEngine riusabile |
| 6 | **Chord memory** (1 tasto → accordo) | 🔼 **elevato** | era §11 "opz P2" | M4/M8 | mappa tasto→accordo memorizzato; diventa un insert MIDI-FX (D25) |
| 7 | **Chord sequencer / looper** | ✅ **1° WOW** | D11, §16 | M3 | libera la mano sinistra |
| 8 | **Auto-accompaniment** (l'intera catena) | ✅ | §11+§14+§16 integrati | M5 | = arranger integrato |
| 9 | **OTS / Keyboard Set** | ✅ | §17, §8 | M9 | MIDI-only: auto-emette program/bank/CC + split/tempo/routing allo Style |
| 10 | **Registration / Performance / Scene** | ✅ | §17, §26.8 | M9 | snapshot richiamabile |
| 11 | **Pads / Multi / Phrase Pads** | ✅ | §15, §26.7 | M8 | seguono accordo o fissi; banchi da 4 |
| 12 | **Quantization** | ✅ | §12, §10 | M6 (+quantize-after M7) | grid/swing/groove template |
| 13 | **Scale assist / quantizer** | ✅ (elevare input-quantize) | §26.2 scale-lock | M6/M8 | forzare input in scala → insert MIDI-FX (D25) |
| 14 | **Harmonizer** (melodia → armonia) | 🔼 **additivo** | nuovo | M8 | distinto da chord-intelligence: 1 linea → voci armonizzate su accordo/scala; insert MIDI-FX |
| 15 | **MIDI effects** (echo/strum/ratchet/prob/humanize/vel/scale-filter/delay/repeat/random/transpose) | 🔼 **elevato a catena** | D25 | M6/M8 | nuovo concetto unificante "MIDI FX insert chain" |
| 16 | **Pattern sequencer / clip launcher** | ✅ | §12, §26.5 | M6 | gesto "scrivi" + scene launch |
| 17 | **Song mode / Scene chain** | ✅ | §12, §26.8 | M9 | Intro→Verse→Chorus→…→Ending |

### 30.1 I 5 elementi additivi / elevati (cosa cambia nel piano)
- **#4 NTT — Style-Follow Resolver → modulo core di prima classe (D24).** È *la* ragione per cui una frase in Cmaj suona giusta anche su Am/D7/Fsus4/G-B senza note sbagliate: non trasposizione meccanica, ma regole musicali (grado, source-chord→target-chord tables, evitare le note "sbagliate"). È il fattore-qualità n.1 dell'arranger: senza NTT decente, "genera-da-accordo" fa schifo. Va progettato in M5, ma il posto nell'architettura (layer Transform, tra ChordState e output della track) va riservato da ora.
- **#15 MIDI FX insert chain → concetto unificante di prima classe (D25).** Invece di trattare arp, humanize, scale-lock, transpose come moduli sparsi, diventano **insert** di una catena componibile e bounded per-track/zone. Fortissimo per il north-star "aperto/hackable" (ogni insert ha parametri indirizzabili L1, mappabili/automatizzabili). Molti insert sono win facili (transpose, velocity, scale-filter, note-repeat, echo/delay, strum, ratchet, probability, randomize, humanize).
- **#14 Harmonizer → insert additivo.** Una singola linea melodica → voci aggiunte seguendo accordo/scala. Diverso dalla chord-intelligence (che parte da input scarno *come accordo*): qui l'input è *melodia* e l'output *armonizzato*. Vive come insert della catena MIDI-FX (D25). M8.
- **#6 Chord memory → insert/feature esplicita.** Mappa "1 tasto → accordo memorizzato". Realizzabile come insert `chord-memory-expand` (D25) o come mode del Chord Engine. Elevato da nota P2 a feature nominata. M4/M8.
- **#2 multi-finger → variante di `chord.mode`.** Aggiungere `multi` all'enum `chord.mode` (§29.2) accanto a diatonic/single/shell/full. M4.

### 30.2 Impatto su moduli/roadmap (nessun cambio d'ordine sostanziale)
- §3 (module table) e §26 (dream list): aggiungere **NTT Resolver** e **MIDI-FX Chain** come moduli di prima classe; **Harmonizer** e **Chord-memory** come insert; **multi-finger** come mode.
- §29 (contratto L1): prevedere il namespace **`fx.<track>.<slot>.*`** per la catena MIDI-FX (tipo insert, parametri, on/off, ordine) e `chord.mode += multi`. NTT ha i suoi parametri sotto `style.*`/`track.*` (regola di follow, tabella).
- Roadmap: NTT dentro M5 (parte non negoziabile dell'arranger); MIDI-FX chain avviata a M6 (transpose/velocity/scale-filter/note-repeat) ed estesa a M8 (arp/harmonize/strum/echo come insert). Nessuno slittamento dei WOW (M3/M5).
