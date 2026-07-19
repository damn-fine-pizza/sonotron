# Sonotron Desktop — Expansion of priority features

Status: PROPOSED (2026-07-19). Translated in full from the original Italian
source (`desktop-feature-expansion-proposal.md`), preserving meaning and
every code identifier verbatim. Filed in `docs/roadmap.md` under the new
`13000` band — see that file's `13000` section and its 2026-07-19
decision-log entry for the filing. Not yet decided, prioritized, or
scheduled: this document proposes twelve desktop feature evolutions; the
roadmap records them as PROPOSED, nothing stronger.

This document describes twelve strategic evolutions of Sonotron Desktop. The
sections define product value, expected behavior, the relationship to
existing code, architectural boundaries, and the criteria needed to consider
each feature genuinely complete.

The features must not be interpreted as independent panels. Taken together
they form a coherent system:

* the **Musical Resource Graph** describes the musical material;
* the **Semantic Resource Browser** lets you find it;
* the **Universal Resource Runtime** lets you execute it;
* the **Repeat Zone** lets you combine it;
* the **Live Musical Transaction Engine** applies changes in a musically
  correct way;
* **Visualized Musical Causality** makes what the system is doing
  understandable;
* Sequence Edit, Variation Lab, Smart Form Builder, and Arrangement Graph
  turn material into compositions;
* Lead Sheet and Performance Transformation Surface respectively let you
  control harmony and performance.

---

## 1. Looper in the Repeat Zone

### Product objective

The Repeat Zone must allow a MIDI loop to be recorded directly inside a cell
without stopping playback, changing mode, or opening a separate window.

The primary gesture must be extremely immediate:

1. the user selects or points at an empty cell;
2. holds it down to begin recording;
3. plays from a MIDI keyboard, a virtual surface, or another enabled source;
4. releases, or waits for the configured boundary;
5. the cell immediately becomes a playable, editable, and reusable loop.

The Looper must turn the Repeat Zone into a musical construction tool, not
merely a launcher for content prepared elsewhere.

### Starting state in the code

The core already contains a substantial part of the infrastructure:

* `LoopBuffer` manages slots, recording, playback, overdub, replace, erase,
  undo, quantization, and length;
* `RetroCaptureRing` can capture a recent window of live MIDI input;
* `ContentKind::kLoopBuffer` lets `ClipMatrix` treat a loop as a normal
  launchable resource;
* `Engine::apply_clip_content()` already distinguishes loops from other
  content types;
* quantized launch and the `kArmed`, `kPlaying`, `kQueuedStop` states are
  already part of the Repeat Zone's model.

The main remaining work is in host and GUI integration:

* real hardware MIDI acquisition in the desktop application;
* connecting virtual surfaces to the same MIDI pipeline;
* the graphical lifecycle of recording;
* stable association between cell, clip, and `LoopBuffer` slot;
* persistence of recorded content;
* full handling of post-recording operations.

### Expected desktop behavior

An empty cell must clearly offer at least three actions:

* dragging in an existing resource;
* creating an empty sequence;
* recording a new loop.

Recording must support:

* immediate start;
* start on the next beat;
* start on the next bar;
* configurable count-in;
* fixed length in bars;
* automatic close at the end of the length;
* free length, determined by the user;
* overdub synchronized onto an already-playing loop;
* full or interval-limited replace;
* selective erase;
* undo of the last pass;
* destructive or graduated quantization;
* optional preservation of human timing.

The source must be explicit but simple:

* selected hardware MIDI input;
* virtual keyboard;
* virtual drum pads;
* mapped controller;
* future output of an internal generator.

The first version must focus on recording live input. Recording output
generated internally by the arranger, arpeggiators, or Strudel must be
treated as a separate extension, because it requires a clear definition of
which point in the pipeline to capture.

### Cell visual states

The cell must make immediately distinguishable:

* empty;
* ready to record;
* count-in;
* recording active;
* recording waiting for the next boundary;
* overdub;
* playback;
* playback with overdub armed;
* stop queued;
* loop modified but not yet applied;
* error or MIDI source unavailable.

Recording duration and position must be visible directly in the cell via a
ring, a sweep bar, or another animation synchronized to the transport.

The cell must not rely on color alone: state, icon, motion, and text must
all contribute to communicating it.

### Functional model

The cell must continue to hold a reference, not the MIDI content itself.
`ClipMatrix` must retain its current responsibility:

* reference to the content;
* role;
* scene;
* launch state;
* pending temporal boundary.

Recorded content continues to live in `LoopBuffer` or a future persistent
resource derived from it.

The following must be distinguished:

* **runtime slot**, needed for playback and recording;
* **persistent resource**, savable, renamable, duplicable, and indexable by
  the Browser;
* **instance in the Repeat Zone**, which can refer to the resource and have
  local settings.

This separation is necessary to allow the same loop to be reused across
multiple scenes without implicitly duplicating the content.

### Persistence and editing

A recorded loop must be able to be:

* renamed;
* saved to the local library;
* duplicated;
* edited in Sequence Edit;
* turned into a persistent MIDI clip;
* exported as a Standard MIDI File;
* dragged into another cell;
* inserted into the Variation Lab;
* associated with tags and metadata;
* unlinked from the original resource to create an independent copy.

The project must save at least:

* MIDI events;
* musical length;
* resolution;
* original source;
* applied quantization;
* musical role;
* any harmonic relationship;
* seed and applied transformations;
* associations with cells and scenes.

### Roadmap phases

#### Phase 1 — Minimal end-to-end recording

* real hardware MIDI acquisition;
* recording into an empty cell;
* fixed length;
* immediate playback;
* erase and undo;
* opening in Sequence Edit.

#### Phase 2 — Performative recording

* synchronized overdub;
* replace;
* count-in;
* free length;
* retroactive capture from the UI;
* complete visual feedback.

#### Phase 3 — Persistent resource

* saving to the library;
* drag-and-drop;
* linked/unlinked duplication;
* metadata;
* export;
* integration with the Resource Graph and the Semantic Browser.

### Completion criteria

The feature is considered complete when the user can:

* connect a MIDI keyboard;
* record without stopping the transport;
* hear the loop at the expected boundary;
* overdub and undo;
* edit the result in Sequence Edit;
* reuse it in another scene;
* close and reopen the project without losing content or associations.

---

## 2. Performance Transformation Surface

### Product objective

The Performance Transformation Surface must allow a performance to be
transformed in real time with high-level musical gestures.

It must not be simply an audio effects panel, and must not be limited to
controlling filters, delay, or reverb. It must directly manipulate
structure, rhythm, orchestration, harmony, and density.

The user must be able to perform gestures such as:

* creating a breakdown;
* preparing a build;
* reducing density;
* increasing syncopation;
* introducing a fill;
* freezing a fragment;
* shifting the orchestration;
* increasing harmonic tension;
* making the pattern more minimal or more aggressive;
* gradually transforming the current scene into the next one.

### Relationship to existing code

Sonotron already has reusable elements:

* `InsertChain` for per-role MIDI transformations;
* savable inserts inside `Performance`;
* groove, swing, humanize, accent, and quantize;
* arpeggiators and other MIDI FX;
* role control and mute/solo;
* quantized launch and changes;
* information on current and next chord;
* Repeat Zone and SceneChain;
* local Intention state in the GUI.

The new surface must orchestrate these systems. It must not duplicate them
nor introduce a second implementation of the same effects.

### Interaction model

The main surface can be a combination of:

* XY pad;
* vertical or circular macros;
* momentary gestures;
* momentary buttons;
* quantized triggers;
* morph between states;
* transformation presets.

A transformation must be able to be:

* momentary, active only while the control is held;
* latched, held until release;
* committed into the scene;
* recorded as automation;
* applied at the next beat, bar, or phrase boundary;
* cancelled before commit;
* made relative to the current state.

The surface must offer different domains, not dozens of simultaneous
controls.

#### Rhythmic domain

* density;
* stutter;
* ratchet;
* syncopation;
* perceived half-time/double-time;
* probability;
* fill amount;
* swing and humanization;
* progressive subtraction of onsets.

#### Harmonic domain

* tension;
* consonance;
* voicing openness;
* inversions;
* extensions;
* guide tones;
* pedal point;
* simplification or enrichment;
* transition toward the next chord or key.

#### Orchestral domain

* progressive entry or exit of roles;
* transition from acoustic to electronic;
* substitution of a part;
* register shift;
* transferring the pattern between instruments;
* call-and-response;
* foreground/background.

#### Performative and sonic domain

* filter sweep;
* spatial spread;
* freeze;
* delay throw;
* reverb lift;
* accentuation;
* velocity compression/expansion;
* rhythmic mute;
* controlled glitch.

### Musical transformations, not opaque presets

Every transformation must declare:

* which subsystems it modifies;
* which roles it affects;
* whether it is destructive or temporary;
* the temporal boundary of application;
* the starting value;
* the target value;
* the interpolation curve;
* the possibility of rollback;
* the relationship to the seed.

A "Breakdown" macro might, for example:

* reduce drums and percussion;
* keep pad and bass;
* lower density;
* open up the reverb;
* simplify the voicing;
* prepare a rise over four bars.

This macro must remain inspectable and editable.

### Recording and automation

Every gesture must be capturable as:

* a point event;
* a curve;
* a scene state;
* a transition between scenes;
* a recorded performance;
* a reusable macro.

The surface must integrate with the future automation system, but can start
by storing high-level musical events, avoiding immediately producing dozens
of low-level lanes.

### Integration with the Transaction Engine

Any gesture that modifies multiple subsystems must become an atomic
transaction.

A build that modifies groove, mute, tension, and instruments cannot apply
its components at different moments. All changes must share:

* boundary;
* duration;
* origin;
* pending state;
* cancellation possibility;
* transaction identifier.

### Roadmap phases

#### Phase 1 — Gesture core

* four fundamental transformations: breakdown, build, fill, density;
* quantized application;
* momentary and latch modes;
* current/target visualization.

#### Phase 2 — Programmable surface

* composed macros;
* per-role control;
* recording;
* user presets;
* morph between two states.

#### Phase 3 — Structural transformations

* harmonic tension;
* orchestration shift;
* scene morph;
* integration with Flow and Intention;
* system-generated transformations.

### Completion criteria

The feature must allow a full performance to be executed without opening
the mixer, piano roll, or automation windows, while keeping every
transformation:

* synchronized;
* repeatable;
* recordable;
* editable;
* undoable;
* visually understandable.

---

## 3. Complete, live Sequence Edit

### Product objective

Sequence Edit must become the universal environment for quickly correcting,
adapting, and developing MIDI content while the session keeps playing.

It must serve both immediate editing of a loop and deep editing. The user
must not be forced to choose between simplicity and precision: the same
resource must offer progressive levels of control.

The fundamental experience is:

1. selecting a cell or resource;
2. editing during playback;
3. hearing the current version while the edit is being prepared;
4. automatic application of the new version at the configured musical
   boundary.

### Current state

The project already has:

* `SeqEditModel`;
* piano roll and step views;
* `StepPatternStore`;
* role selection;
* lane visibility;
* a pitch window with zoom and content-fit;
* initial editing operations;
* `Timeline` and step primitives;
* sending edits to the core via `BrainSession`;
* automation tests for the piano roll, lanes, and cell opening.

The current model is mostly centered on `StepTrack`. The roadmap must
extend the editor without creating a second, incompatible system for every
resource type.

### Supported content types

Sequence Edit must be able to open:

* StepTrack;
* LoopBuffer converted or represented as a sequence;
* imported MIDI clip;
* part of a Style Section;
* ChordSequence;
* generated pattern;
* future Strudel resource frozen into MIDI.

Not all content must be directly editable in the same way.

For every opened item the editor must distinguish:

* editable source;
* editable instance;
* derived content;
* read-only content;
* content that requires a local copy before editing.

Editing part of a shared Style must not implicitly alter every other scene
that uses that Style. The user must choose between:

* editing the original resource;
* a local override;
* an independent copy;
* a variation linked to the source.

### Current/draft model

Playback must continue to use a stable version while the user edits a
draft.

Sequence Edit must maintain:

* **current version**, currently playing;
* **draft version**, visible and editable;
* **pending version**, already validated and waiting for the boundary;
* an optional history of recent versions.

Commit can happen:

* immediately;
* at the next step;
* at the next beat;
* at the next bar;
* at the end of the loop;
* at the end of the phrase.

The default behavior for live editing should be the next loop boundary or
the next bar, depending on the nature of the edit.

### Note editing

The piano roll must support:

* insertion;
* deletion;
* single and multiple selection;
* move;
* copy;
* duplicate;
* resize;
* split;
* merge;
* transpose;
* velocity;
* gate;
* tie;
* probability;
* ratchet;
* microtiming;
* note mute;
* note condition;
* quantization;
* humanization;
* legato;
* distribution and alignment.

It must support multi-bar loops and different meters.

The step view must remain available as a compact, performative
representation of the same content, not as an alternative data format.

### Lanes and expression

The editor must be able to show:

* velocity;
* probability;
* gate;
* ratchet;
* microtiming;
* CC;
* pitch bend;
* channel pressure;
* polyphonic aftertouch;
* future per-note automation.

Lanes must be added gradually. The architecture must, however, avoid
assuming that a sequence contains only note on/off events.

### Harmonically aware editing

Notes must be viewable as:

* absolute MIDI notes;
* scale notes;
* chord degrees;
* relative intervals;
* Sonotron functional notes.

The user must be able to lock editing to:

* key;
* scale;
* current chord;
* per-step chord;
* musical role.

Automatic corrections must be previewable and reversible.

### Quick Edit

Alongside the piano roll there must be a quick-transformation mode:

* simplify;
* make denser;
* create a fill;
* vary the last bar;
* shift accents;
* add ghost notes;
* create call-and-response;
* adapt to the new chord;
* change register;
* generate a variation while keeping the start.

Quick Edit must operate on the same model as manual editing and produce an
inspectable draft.

### Undo and versions

Undo must work at the level of a musical operation, not a single mouse
movement.

Examples:

* "Quantize 50%" is one operation;
* "Generate fill" is one operation;
* dragging ten notes is one operation;
* applying a variation is one operation.

It must be possible to compare current and draft, A/B listen to them, and
revert to a previous version.

### Roadmap phases

#### Phase 1 — Reliable piano roll

* complete note editing;
* multi-selection;
* velocity and gate;
* zoom and navigation;
* undo/redo;
* commit at the next boundary.

#### Phase 2 — Deep parameters

* probability;
* ratchet;
* microtiming;
* lanes;
* multi-bar loop;
* full support for different meters.

#### Phase 3 — Universal editing

* LoopBuffer;
* imported clips;
* Style Section with overrides;
* ChordSequence;
* harmonic transformations;
* generalized current/draft/pending.

### Completion criteria

Sequence Edit is complete when any MIDI loop used in the Repeat Zone can be
opened, corrected during playback, applied without a glitch, saved, and
reused without resorting to an external DAW.

---

## 4. Smart Form Builder

### Product objective

Smart Form Builder must solve the problem of moving from a collection of
loops to a song with a recognizable form.

The user must not be forced to manually build Intro, A, B, Fill, Break, and
Ending. They must be able to supply:

* a few scenes or resources;
* an approximate duration;
* an energy direction;
* any constraints;
* a desired structure type.

Sonotron must propose a complete form, always explicit and editable.

### Starting state

The code already has:

* `SceneChain`;
* `SceneStep`;
* `PerformanceStore`;
* per-scene duration;
* repeat count;
* style, groove, key, and meter overrides in `GridModel`;
* auto-song;
* predefined harmonic progressions;
* information on section types;
* Intro, Variation, Fill, Break, and Ending;
* quantized launch and transitions.

The current structure is mostly linear and built from the grid's columns.
Smart Form Builder must work on top of these elements, not replace them.

### Builder input

The builder must accept minimal input:

* short, medium, long duration, or an approximate number of bars;
* energy curve;
* mandatory material;
* excluded material;
* presence or absence of an intro;
* presence or absence of an ending;
* amount of repetition;
* level of surprise;
* degree of similarity between sections.

In advanced mode it must accept:

* number of main sections;
* position of the climax;
* number of breakdowns;
* return rules;
* harmonic variations;
* meter;
* probability of deviating from the form;
* per-role density;
* minimum and maximum scene duration.

### Output

The result must be a structure composed of normal Sonotron objects:

* scenes;
* performance;
* scene step;
* chord sequence;
* transitions;
* overrides;
* any generated variations.

There must be no opaque "generated song" separate from the rest of the
project.

The user must be able to:

* replace a scene;
* change the duration;
* change a chord;
* move the climax;
* regenerate only a region;
* lock already-approved sections;
* request an alternative form;
* capture the form in the Repeat Zone;
* turn it into an Arrangement Graph.

### Initial formal models

The system must start from a limited but meaningful set of archetypes:

* Intro → A → B → A2 → B2 → Ending;
* Intro → Verse → Pre-Chorus → Chorus → Verse → Chorus → Bridge → Chorus;
* Build → Drop → Break → Build → Drop;
* A → A2 → B → A3;
* an evolving ambient form;
* a groovebox form with no fixed ending;
* a live form with manually held sections;
* a classic arranger form with automatic fills.

The archetypes must be described through rules and formal roles, not
through rigid genre names.

### Energy curve

The energy curve must influence:

* scene selection;
* density;
* orchestration;
* variation tier;
* fill presence;
* harmonic tension;
* register;
* groove;
* mutation probability.

The curve must not be interpreted as simple volume.

### Quality rules

The builder must avoid:

* excessive identical repetition;
* premature climaxes;
* endings that feel like interruptions;
* fills with no function;
* incompatible scenes;
* random harmonic transitions;
* unprepared meter changes;
* excessive simultaneous novelty.

It must be able to briefly explain why it chose a structure.

### Roadmap phases

#### Phase 1 — Explicit form templates

* deterministic archetypes;
* existing scenes;
* duration and repeat;
* intro/fill/ending;
* output into `SceneChain`.

#### Phase 2 — Energy-aware form

* energy curves;
* automatic scene selection;
* generation of variations;
* adaptive harmony;
* section locking.

#### Phase 3 — Form exploration

* alternatives;
* branching;
* A/B comparison;
* local regeneration;
* handoff to the Nonlinear Arrangement Graph.

### Completion criteria

The builder is complete when it can take a session made of a few loops and
produce, in under a minute, a musical structure that is:

* playable;
* understandable;
* editable;
* non-destructive;
* exportable;
* free of manifestly incompatible passages.

---

## 5. Seeded Variation Lab

### Product objective

Seeded Variation Lab must allow systematic exploration of musical variants
without losing the identity of the original material and without depending
on irreproducible random results.

The seed must not remain a technical detail. It must become part of a
variation's identity.

Every result must be:

* reproducible;
* comparable;
* derivable from a source;
* editable;
* savable;
* regenerable.

### Starting state

Sonotron already uses determinism and seeds in several systems:

* groove;
* humanization;
* probability;
* motif generation;
* some transformations;
* golden and replay tests.

This infrastructure must be elevated to a product concept.

### Variation model

A variation must retain:

* source identifier;
* source version;
* seed;
* transformation type;
* parameters;
* locked elements;
* modified elements;
* any prior transformations;
* algorithm version.

The algorithm version is essential: the same seed must produce the same
result only within the same version of the generator.

### Editable dimensions

The user must be able to choose which dimensions can vary:

* rhythm;
* onsets;
* velocity;
* gate;
* probability;
* ratchet;
* microtiming;
* pitch;
* intervals;
* harmonization;
* voicing;
* register;
* density;
* orchestration;
* groove;
* last bar;
* fill;
* call-and-response;
* complexity.

It must be able to lock:

* specific notes;
* first or last bars;
* rhythmic pattern;
* melodic profile;
* bass;
* drums;
* chords;
* accents;
* one or more tracks;
* structural points.

### User experience

The Lab must show the source and a limited set of candidate variations.

Each candidate must be able to be:

* heard at the next boundary;
* compared A/B;
* promoted to a new version;
* saved as a resource;
* dragged into a scene;
* varied further;
* combined with another candidate;
* discarded.

The visualization must highlight what changed:

* added notes;
* removed notes;
* modified timing;
* density;
* harmony;
* instruments;
* preserved regions.

### Genealogy

Variations must form a genealogy, not a flat list.

The user must be able to see:

* source;
* children;
* branches;
* seed;
* transformations;
* active version;
* divergence points.

There is no need to start with a complex graphical visualization. A tree
structure or a branching timeline is sufficient, as long as the data model
supports the genealogy.

### Combining variations

The system must be able to combine aspects of two variants:

* the rhythm of variant A;
* the pitch of variant B;
* the last bar of variant C;
* the groove of the source.

The combination must be explicit and deterministic.

### Relationship to Sequence Edit

Variation Lab generates proposals; Sequence Edit lets you refine them.

An accepted variation must become a normal, editable draft. Subsequent
manual edits must be recorded in the provenance without pretending the
content is still produced exclusively by the seed.

### Roadmap phases

#### Phase 1 — StepTrack variations

* rhythmic variations;
* density;
* velocity;
* fill;
* visible seed;
* A/B;
* save as new resource.

#### Phase 2 — Harmonic and melodic variations

* pitch;
* voicing;
* register;
* chord adaptation;
* selective locks;
* genealogy.

#### Phase 3 — Multi-resource variations

* scenes;
* orchestration;
* combination between variants;
* use by Smart Form Builder and Flow Director.

### Completion criteria

The Lab is complete when the user can generate numerous alternatives
without losing:

* source;
* control;
* reproducibility;
* the ability to compare;
* the ability to edit;
* the ability to go back.

---

## 6. Visualized Musical Causality

### Product objective

Visualized Musical Causality must make visible not only what is happening,
but also:

* what is about to happen;
* when it will happen;
* why it will happen;
* which command or rule caused it;
* which parts of the system will be affected.

This feature is necessary because Sonotron wants to offer deep, generative,
and quantized systems without forcing the user to understand their internal
architecture.

### Starting state

Various fragments of state already exist:

* clip `LaunchState`;
* armed clips and queued stops;
* current and next chord;
* current scenes;
* `BrainEvent`;
* `AppState`;
* playhead;
* boundary;
* local Intention state;
* information on style, variation, and performance.

The problem is that this information does not yet form a shared causal
model.

### Current, pending, and predicted model

The UI must clearly distinguish:

* **Current**: the state that is genuinely active;
* **Pending**: a change already requested and awaiting commit;
* **Predicted**: an expected consequence not yet definitively scheduled;
* **Candidate**: a proposal not yet accepted;
* **Historical**: an event that just happened, useful for understanding the
  transition.

Example:

* scene A is current;
* scene B is pending for the next bar;
* the system predicts the bass will switch to the new voicing;
* a fill is candidate because it depends on an auto-fill rule;
* after the boundary the entire group becomes historical for a few seconds.

### Visual language

Causality must use a coherent grammar:

* ghosting for future content;
* a pulsing outline for pending state;
* a sweep or countdown for the boundary;
* lines or arcs to show origin and destination;
* propagation along the tracks involved;
* a visual difference between certain and probabilistic consequences;
* intensity proportional to the magnitude of the change;
* an icon or short label for the type of cause.

Animations must be musically synchronized. They must not be decorative or
independent of the transport.

### Cause categories

The system must distinguish at least:

* direct user action;
* scene launch;
* form-builder rule;
* SceneChain transition;
* harmonic response;
* generative variation;
* Transformation Surface gesture;
* automation;
* Sequence Edit change;
* Flow decision;
* resource compatibility or adaptation.

### Progressive explanation

The first level must be immediate:

> "Var B at the next bar."

A second level, available via hover or an inspector, can show:

> "Requested by the user; launch quantized to 1 bar; will apply the style
> section, groove override, and key override."

A third, diagnostic level can show identifiers, the wire event, transaction
id, and the subsystems involved.

The normal UI must not show technical details by default.

### Causal Event Model

A common host-side model is needed that collects:

* cause identifier;
* type;
* source;
* target;
* time of request;
* boundary;
* state;
* predicted consequences;
* applied consequences;
* any error;
* any parent transaction.

`BrainEvent` can be extended or paired with a richer model. Not all
information must cross the core binary ABI: the desktop host can derive
part of the causality from its own actions, as long as the final state is
confirmed by the core.

### Relationship to debugging and testing

Visual causality must be backed by verifiable causality.

Every important transition must be reproducible in tests:

* request;
* pending;
* boundary;
* commit;
* resulting events.

This system can also become a diagnostic tool for UI tests, trace, and
replay.

### Roadmap phases

#### Phase 1 — Coherent Current/Next

* clip;
* scene;
* chord;
* style;
* variation;
* boundary countdown.

#### Phase 2 — Transaction visualization

* atomic groups;
* origin;
* target;
* cancellation;
* error;
* commit.

#### Phase 3 — Musical causal graph

* compatibility;
* transformations;
* Flow;
* Smart Form Builder;
* progressive explanations.

### Completion criteria

The feature is complete when a user can understand every important change
without wondering:

* why the music hasn't changed yet;
* which scene will come in;
* which components will be modified;
* whether an action was registered;
* whether a proposal is only a preview;
* whether the system is operating autonomously.

---

## 7. Live Musical Transaction Engine

### Product objective

The Live Musical Transaction Engine must provide a single model for applying
complex changes during playback in a way that is:

* atomic;
* deterministic;
* quantized;
* cancellable;
* observable;
* testable.

Currently different subsystems independently manage the concept of "apply
at the next boundary." This is sufficient for isolated operations, but
becomes fragile when a single action simultaneously modifies multiple
components.

### Starting state

Sonotron already has:

* `BoundaryLatch`;
* `ClipMatrix::arm()`;
* `due_bar_index`;
* quantized launch;
* chord staging;
* SceneChain transitions;
* Performance recall;
* Style changes;
* pending states in different subsystems.

The new engine must not immediately eliminate these primitives. It must
first coordinate them and later reduce duplication.

### Definition of a musical transaction

A transaction represents one complete, intentional change.

Examples:

* switching to a new scene;
* replacing a clip;
* applying an edit from Sequence Edit;
* a Breakdown macro;
* a simultaneous change of style, groove, and key;
* loading a new Performance;
* morphing between two states;
* inserting a fill followed by a variation.

Every transaction must contain:

* identifier;
* origin;
* set of operations;
* required initial state;
* boundary;
* duration or curve;
* dependencies;
* validation policy;
* error policy;
* cancellability;
* any rollback;
* musical timestamp;
* current state.

### States

The minimum lifecycle must include:

* created;
* validating;
* ready;
* scheduled;
* pending;
* committing;
* committed;
* cancelled;
* rejected;
* failed;
* rolled back.

Not all states must be shown in the normal UI, but they must exist in the
model.

### Supported boundaries

The model must be able to express:

* immediate;
* next step;
* next beat;
* next bar;
* next N bars;
* loop end;
* phrase end;
* scene end;
* explicit musical position.

The first implementation can support a subset, but the format must not
assume that every operation happens only at the next bar.

### Atomicity

A transaction that modifies:

* style;
* variation;
* groove;
* key;
* mute;
* instruments;

must produce a coherent state at a single musical point.

It must not be possible to hear, for a fraction of a bar, the new groove
with the old style, or the new chord with the old voicing, unless this is
expressly part of a transition.

### Validation

Before scheduling, the system must verify:

* resource existence;
* role compatibility;
* runtime capacity;
* slot availability;
* ID correctness;
* whether the boundary can be applied;
* any conflicts with already-pending transactions;
* replacement policy.

### Conflicts

The system must define what happens when a new request touches a state
that is already pending:

* it replaces the previous request;
* it queues;
* it merges;
* it is rejected;
* it cancels the previous group;
* it modifies only part of the transaction.

The policy must depend on the type of operation, not on an arbitrary global
rule.

### Rollback

Full realtime rollback may be impossible for some transformations already
emitted via MIDI. The model must distinguish:

* cancellation before commit;
* logical rollback of state;
* compensation via a new transaction;
* a non-reversible operation.

The interface must not promise reversibility when the system can only send
a subsequent correction.

### Architecture

The core must receive a bounded, realtime-safe representation of the
necessary operations. The desktop host can keep a richer description.

A host transaction can be compiled into:

* one or more `Command`;
* a bounded bundle;
* a prepared Performance;
* references to already-registered resources;
* a commit plan.

The core must not receive strings, dynamic structures, or arbitrary graphs
on the realtime path.

### Roadmap phases

#### Phase 1 — Host-side transaction coordinator

* identifiers;
* pending state;
* clip launch;
* scene launch;
* Sequence Edit commit;
* cancellation.

#### Phase 2 — Atomic bundles

* Performance;
* style/groove/key;
* Transformation Surface;
* validation;
* conflict policy.

#### Phase 3 — Phrase and graph transactions

* scene end;
* morph;
* Nonlinear Arrangement Graph;
* Flow Director;
* rollback and compensation.

### Completion criteria

The Transaction Engine is complete when all major live operations:

* share the same lifecycle;
* are visualizable;
* can be tested;
* do not produce incoherent intermediate states;
* can be cancelled before commit;
* apply their result at the declared boundary.

---

## 8. Semantic Resource Browser

### Product objective

The Browser must evolve from a list of categories and names into a
contextual musical search system.

The user must be able to search not only for "rock" or "bass", but for
requests like:

* a dark eight-bar intro;
* a drum pattern compatible with the current scene;
* a less dense bass;
* an energetic ending;
* a loop that works after the selected scene;
* a warm instrument for the pad role;
* a similar but not identical variation;
* material suited to 120–130 BPM.

### Starting state

`BrowserModel` already has:

* Styles, Variations, Voices, Kits, and Clips categories;
* built-in names;
* search;
* `StyleFamily` taxonomy;
* drag-and-drop support;
* separation between model and panel;
* initial classification criteria.

This base must be generalized. The Browser must not keep depending on lists
manually copied from the core as the number of resources grows.

### Concept of a resource

The Browser must show every object identified by the Musical Resource
Graph:

* Style;
* Style Section;
* per-role pattern;
* MIDI clip;
* recorded loop;
* ChordSequence;
* instrument;
* kit;
* preset;
* groove;
* macro;
* Performance;
* scene;
* form template;
* future Strudel resource;
* automation;
* transformation preset.

The UI can group and filter these resources, but the underlying model must
be uniform.

### Taxonomy and labels

Labels must cover at least:

* type;
* role;
* formal function;
* musical family;
* BPM;
* meter;
* key;
* mode;
* energy;
* density;
* tension;
* complexity;
* groove;
* duration;
* instrumentation;
* origin;
* license;
* quality;
* compatibility;
* user state.

Labels must be able to be:

* declared by the author;
* derived by the importer;
* computed;
* edited by the user;
* temporary and contextual.

### Search and ranking

The result must not be merely filtered, but ranked.

Ranking can consider:

* textual match;
* harmonic compatibility;
* rhythmic compatibility;
* formal compatibility;
* distance from the BPM;
* similarity;
* usage history;
* favorites;
* selected role;
* current scene;
* transformations needed.

The UI must distinguish:

* direct compatibility;
* compatibility via transformation;
* uncertain compatibility;
* known incompatibility.

### Contextual preview

Preview must happen in the context of the session:

* current BPM;
* current key;
* current chord;
* current instrument;
* target role;
* target scene;
* quantized boundary.

The user must be able to hear the resource:

* alone;
* with the current scene;
* temporarily replacing a part;
* in the next scene;
* with automatic adaptation.

Preview must not permanently modify the project.

### Drag-and-drop

The drag must communicate:

* the resource's type;
* valid destinations;
* adaptations that will be applied;
* copy or reference;
* possible creation of a new scene;
* possible replacement.

The drop must be able to produce a pending transaction, not an immediate,
uncontrolled change.

### Role of Styles

Styles must remain visible, but must not dominate the entire taxonomy.

In the Browser they can appear as:

* curated collections;
* starting points;
* filters;
* coherent packages;
* musical recipes;
* sources of sections and patterns.

The Browser must allow a Style to be opened and its components navigated,
without forcing the user to treat it as an indivisible block.

### Local library

The Browser must index:

* included resources;
* local folders;
* recent projects;
* recorded loops;
* imports;
* installed packages;
* user resources;
* generated results.

The index must be persistent and incrementally updated.

### Roadmap phases

#### Phase 1 — Unified browser

* common resource model;
* labels;
* search;
* filters;
* coherent drag-and-drop;
* basic preview.

#### Phase 2 — Compatibility ranking

* BPM;
* role;
* harmony;
* duration;
* scene context;
* explanation of adaptations.

#### Phase 3 — Similarity and recommendation

* similarity;
* genealogy;
* "works well after";
* history;
* suggestions;
* composed musical queries.

### Completion criteria

The Browser is complete when a user can build a session without knowing:

* file names;
* folder structure;
* technical provenance;
* the internal format of resources.

They must be able to find material based on the desired musical function.

---

## 9. Musical Resource Graph

### Product objective

Musical Resource Graph must become the shared semantic model of all
Sonotron musical resources.

The Graph must describe:

* what a resource is;
* what it can do;
* what it derives from;
* what it is compatible with;
* what transformations it supports;
* how it can be executed;
* how it can be modified;
* where it is used.

It is not primarily a graphical visualization. It is a data and domain
infrastructure.

### Current problem

The project has several separate models:

* Style and Style Section;
* Clip and `ContentKind`;
* StepTrack;
* LoopBuffer;
* ChordSequence;
* Performance;
* SceneStep;
* instruments and kits;
* host data manually copied into `BrowserModel`.

These models work within their own subsystems, but there is no shared
identity usable by:

* the Browser;
* persistence;
* drag-and-drop;
* the editor;
* variation;
* compatibility;
* the project graph;
* future resource types.

### Resource ID

Every persistent resource must have a stable identifier, independent of:

* position in the folder;
* runtime index;
* cell;
* load order;
* visible name.

The identity can include:

* host-side UUID;
* type;
* content hash;
* version;
* source package;
* revision.

The core's bounded indices must remain separate from host-side persistent
identifiers.

### Resource descriptor

Every resource must declare:

* type;
* name;
* author;
* version;
* provenance;
* license;
* duration;
* meter;
* ideal BPM and allowed range;
* key and mode;
* role;
* formal function;
* energy;
* density;
* tension;
* harmonic capabilities;
* time-adaptation capabilities;
* editing capabilities;
* preview capabilities;
* required backend;
* dependencies;
* checksum;
* capability flags.

Not every field applies to every type. The format must distinguish "not
applicable", "unknown", and "declared value."

### Relationships

The Graph must represent relationships such as:

* contains;
* derived from;
* variation of;
* compatible with;
* requires;
* uses instrument;
* uses groove;
* belongs to style;
* works after;
* works before;
* replaces;
* generated from;
* frozen from;
* overridden by;
* used in scene;
* used in project.

Relationships can be:

* declared;
* computed;
* learned from usage;
* temporary;
* associated with a score.

### Style decomposition

A Style must become a node composed of:

* sections;
* patterns;
* roles;
* groove;
* suggested instruments;
* harmonic vocabulary;
* energy curves;
* transitions;
* metadata.

This lets a Style be used as a coherent package without preventing its
components from being combined with external resources.

### Capability model

The Graph must allow a resource to be queried:

* can it be launched?
* can it be looped?
* can it follow chords?
* can it be transposed?
* can it be time-stretched?
* can it be edited in Sequence Edit?
* can it produce MIDI?
* can it produce audio?
* can it be frozen?
* can it be varied?
* can it run on the embedded core?
* does it require the desktop host?

This avoids distributed switches based on the concrete type.

### Host/core boundary

The full Graph is mostly host-side.

The realtime core must receive only:

* bounded handles;
* reduced descriptors;
* necessary capabilities;
* already-compiled content;
* realtime-safe parameters.

The core must not navigate a dynamic graph, read JSON, or resolve paths.

### Persistence

The Graph must distinguish:

* the global library;
* project resources;
* embedded resources;
* linked external resources;
* cache;
* missing resources;
* migrated resources.

A project must be openable even when a resource is missing, clearly showing
the unresolved reference and allowing substitution.

### Query API

The system must support queries such as:

* all resources for the bass role;
* all sections compatible with the current scene;
* all variants derived from this loop;
* all projects that use this resource;
* all instruments compatible with this preset;
* all resources executable on the selected runtime.

### Roadmap phases

#### Phase 1 — Registry and descriptor

* Resource ID;
* type;
* metadata;
* mapping to existing objects;
* persistence;
* basic query.

#### Phase 2 — Relationships and compatibility

* derivation;
* composition;
* Style decomposition;
* capability;
* compatibility edges.

#### Phase 3 — Full provenance

* genealogy;
* missing-resource recovery;
* package;
* cache;
* updates;
* cross-project usage.

### Completion criteria

The Resource Graph is complete when Browser, Repeat Zone, Sequence Edit,
Variation Lab, and persistence can all refer to the same resource without:

* duplicating taxonomies;
* copying lists from the core;
* using paths as identity;
* introducing a new isolated model for every new content type.

---

## 10. Lead Sheet and intelligent Chord Map

### Product objective

Lead Sheet and Chord Map must offer a compact musical view of the session's
harmonic and formal structure.

This view must allow the user to understand and edit:

* chords;
* key;
* sections;
* duration;
* cadences;
* tension;
* modulations;
* the relationship between harmony and scenes.

It must not become a traditional DAW timeline nor a full notation editor.

### Starting state

The core already has:

* `ChordSequence`;
* chord-follow;
* live priority;
* the detector;
* current and next chord;
* key and mode;
* default progressions;
* cadences;
* SceneChain;
* per-scene harmonic overrides;
* ChordPro import in the converter.

This information must be exposed through a coherent visual model.

### Lead Sheet

The Lead Sheet must show a readable sequence of:

* sections;
* bars;
* chords;
* markers;
* any text notes;
* repeats;
* endings;
* key changes;
* meter changes.

It must be able to represent:

* one chord per bar;
* multiple chords in the same bar;
* chords held across several bars;
* anticipations;
* slash chords;
* non-diatonic chords;
* regions with no explicit harmony.

### Chord Map

The Chord Map must offer a more conceptual representation:

* harmonic function;
* tonicization;
* tension;
* resolution;
* distance from the key;
* relationships between chords;
* possible destinations.

It must not force the beginner to know Roman numerals. It must offer
levels:

#### Perceptual level

* stable;
* open;
* tense;
* dark;
* bright;
* suspended;
* resolving.

#### Intermediate level

* chord name;
* key;
* degree;
* quality;
* tension.

#### Advanced level

* function;
* inversion;
* voicing;
* guide tones;
* substitutions;
* modulation;
* cadence.

### Editing

The user must be able to:

* insert a chord;
* edit root and quality;
* drag the duration;
* duplicate a region;
* transpose;
* change key;
* apply a cadence;
* make a region more tense;
* simplify;
* generate a transition;
* lock a progression;
* convert absolute chords into relative degrees and vice versa.

Edits must be applicable live via the Transaction Engine.

### Relationship to scenes

The view must show the alignment between:

* chords;
* scenes;
* form;
* variation;
* energy;
* meter.

A scene can:

* own a progression;
* inherit a global progression;
* override a region;
* use live harmony;
* use a chord sequence;
* use a hybrid mode.

The active harmonic source must always be visible.

### Compatibility with chord-follow

When a resource follows the chords, the Lead Sheet must show the
progression driving the transformation.

When live input takes priority, the UI must distinguish:

* the programmed progression;
* the temporary live chord;
* the point where the progression will resume;
* any staged chord.

### Import and export

The roadmap must plan for:

* ChordPro import;
* ChordPro export;
* import from MIDI with chord markers, when available;
* chord map export;
* future DAW interoperability.

The existing importer in the converter can provide a base, but integration
into the app requires a shared persistent model.

### Roadmap phases

#### Phase 1 — Linear Lead Sheet

* ChordSequence;
* scenes;
* key;
* meter;
* basic editing;
* playback cursor.

#### Phase 2 — Harmony intelligence

* degrees;
* tension;
* cadences;
* suggestions;
* live-priority visualization;
* resource adaptation.

#### Phase 3 — Form and graph integration

* Smart Form Builder;
* Nonlinear Arrangement Graph;
* harmonic alternatives;
* modulations;
* Intention.

### Completion criteria

The feature is complete when a beginner can change the harmonic direction
without knowing theory, and an experienced musician can explicitly control
chords, functions, and cadences without using external tools.

---

## 11. Nonlinear Arrangement Graph

### Product objective

Nonlinear Arrangement Graph must allow the composition of musical forms
that are not merely a linear sequence of scenes.

Scenes become nodes. Transitions become explicit edges.

The Graph must support:

* live performance;
* conditional structures;
* variations;
* alternative paths;
* adaptive forms;
* continuous generation;
* songs that change between performances;
* capturing a path as a linear arrangement.

### Starting state

`SceneChain` implements a linear chain:

* every `SceneStep` points to a Performance;
* it has duration and repeat;
* it can change meter;
* the last step stays active;
* `SceneTransitionKind` currently supports only `kCut`.

This primitive remains useful. The Graph can initially live host-side and
compile portions of the path into SceneChain.

### Node model

A node must be able to represent:

* a scene;
* a Performance;
* a group of scenes;
* a form section;
* a generator;
* a Flow state;
* a wait node;
* an ending;
* a decision point.

Every node can declare:

* minimum duration;
* maximum duration;
* repeat;
* entry behavior;
* exit behavior;
* resources;
* harmony;
* Intention;
* meter;
* completion conditions.

### Transition model

A transition must be triggerable by:

* a user command;
* repeat count;
* end of scene;
* probability;
* energy;
* tension;
* chord;
* MIDI input;
* gesture;
* automation;
* a Flow condition;
* an external event;
* a musical timeout.

The transition can have:

* priority;
* weight;
* quantization;
* fill;
* duration;
* morph;
* fallback destination;
* cooldown;
* seed.

### Determinism

Probabilistic transitions must be deterministic when the seed and input are
identical.

The project must be able to save:

* the Graph;
* the seed;
* the path executed;
* decisions;
* the input that influenced the path.

This enables replay and testing.

### UI experience

The Graph must not immediately present itself as a technical node editor.

The experience can have three levels:

#### Simple level

* connected scenes;
* arrows;
* start point;
* ending;
* repeat.

#### Intermediate level

* alternative paths;
* probability;
* conditions;
* fill;
* duration.

#### Advanced level

* expressions;
* input mapping;
* Intention;
* rules;
* generative nodes;
* diagnostics.

The Repeat Zone must remain the main performance point. The Graph describes
how scenes can evolve; it does not replace the launcher.

### Performance recording

During a jam the system must record:

* nodes traversed;
* transitions;
* manually launched scenes;
* real duration;
* gestures;
* deviations.

The recorded path must be able to be:

* played back;
* converted into a SceneChain;
* edited;
* turned into a Smart Form;
* exported.

### Runtime

The first version can keep the Graph in the desktop host:

1. the host evaluates non-realtime conditions;
2. prepares the next decision;
3. compiles the transition;
4. sends a bounded transaction to the core;
5. the core applies the result at the boundary.

Strictly realtime conditions must be reduced to bounded, precompiled
primitives.

### Roadmap phases

#### Phase 1 — Manual branches

* scene nodes;
* manual transitions;
* repeat;
* ending;
* conversion into SceneChain.

#### Phase 2 — Conditions and probability

* musical conditions;
* seed;
* weights;
* fallback;
* path trace.

#### Phase 3 — Adaptive composition

* Intention;
* Flow;
* performance input;
* morph;
* generator nodes;
* endless structures.

### Completion criteria

The Graph is complete when a session can:

* follow multiple paths;
* react to the performance;
* remain deterministic when required;
* be visually understood;
* be captured as an arrangement;
* keep using SceneChain and Performance without duplicating their
  responsibilities.

---

## 12. Universal Resource Runtime

### Product objective

Universal Resource Runtime must allow any musical resource supported by
Sonotron to participate coherently in the main workflows:

* Browser;
* preview;
* drag-and-drop;
* Repeat Zone;
* launch;
* stop;
* Sequence Edit;
* persistence;
* capture;
* variation;
* export.

The system must make it possible to add future resource types without
modifying numerous independent switches and without creating a separate
workflow for each type.

### Current state

`ClipMatrix` supports four `ContentKind` values:

* `kStyleSection`;
* `kChordSequence`;
* `kStepTrack`;
* `kLoopBuffer`.

Content is identified by kind and index. `Engine::apply_clip_content()`
performs a switch and routes to the correct subsystem.

This solution is simple, bounded, and suited to the embedded core. It must
not be replaced with dynamic polymorphism in the realtime core.

The limit shows up on the desktop, where the expected types grow:

* persistent MIDI clips;
* audio clips;
* instruments;
* generators;
* macros;
* automations;
* Strudel resources;
* composite scenes;
* frozen resources.

### Architectural principle

The universal runtime must have two levels.

#### Host-side Resource Runtime

It can use:

* a dynamic registry;
* adapters;
* rich descriptors;
* Resource ID;
* plugins;
* filesystem;
* cache;
* preview;
* conversions;
* non-realtime asynchronous jobs.

#### Core-side Compiled Runtime

It must use:

* bounded handles;
* explicit types;
* fixed pools;
* POD;
* commands;
* events;
* predictable dispatch;
* no realtime allocation.

The universal runtime does not mean making the core dynamic. It means
providing an explicit compilation phase between a desktop resource and its
executable form.

### Functional contract

Every resource adapter must declare its supported operations.

Possible capabilities:

* inspect;
* preview;
* prepare;
* compile;
* register;
* launch;
* stop;
* pause;
* seek;
* loop;
* record;
* overdub;
* edit;
* transpose;
* reharmonize;
* time-adapt;
* vary;
* freeze;
* render;
* export;
* serialize.

A resource must not implement operations that have no meaning for it. The
Browser and the UI must query the capabilities instead of assuming behavior
from the concrete type name.

### Lifecycle

The general lifecycle must include:

1. discovery;
2. metadata loading;
3. validation;
4. dependency resolution;
5. preparation;
6. compilation;
7. runtime registration;
8. preview or launch;
9. state observation;
10. persistence;
11. release.

Preparation can fail before launch, letting the UI show an error without
destabilizing the session.

### Resource instance

The resource must be distinguished from the instance.

The resource describes the shared content.

The instance describes:

* scene;
* role;
* start offset;
* duration;
* loop;
* transpose;
* gain or velocity;
* overrides;
* launch quantization;
* local automation;
* link to the resource.

This is needed to reuse a resource in multiple places with different
behaviors.

### Preview

Every previewable resource must be listenable:

* standalone;
* in context;
* with temporary adaptation;
* without altering the session;
* with immediate cancel;
* using the correct backend.

The runtime must distinguish preview from permanent launch.

### Core registration

For already-existing types, the runtime must map the host-side Resource ID
to:

* `ContentKind`;
* index;
* pool;
* registration command;
* core lifecycle.

For new desktop-only types it can:

* render or compile into an existing type;
* use a separate, synchronized host engine;
* introduce a new core type only when a genuine shared realtime need
  exists.

A Strudel resource, for example, could initially produce MIDI events on the
host and freeze into a StepTrack or LoopBuffer, without immediately
requiring a JavaScript runtime in the core.

### ClipMatrix migration

`ClipMatrix` must continue to be launch bookkeeping.

It must not:

* own resources;
* load files;
* know about the Browser;
* interpret metadata;
* perform conversions;
* manage recording.

The universal runtime must sit between the Resource Graph and `ClipMatrix`,
preparing valid references and translating the launch to the correct
subsystem.

### Error handling

The system must handle:

* missing resource;
* unsupported format;
* missing dependency;
* backend unavailable;
* compilation failed;
* core slots exhausted;
* incompatible resource version;
* preview unavailable;
* incompatible runtime.

Errors must be associated with the resource and the instance, not merely
written to the log.

### Roadmap phases

#### Phase 1 — Adapters for the four existing types

* StyleSection;
* ChordSequence;
* StepTrack;
* LoopBuffer;
* Resource ID;
* capability;
* host/core mapping.

#### Phase 2 — Persistent resources and conversions

* MIDI clips;
* generators;
* instruments;
* freeze;
* compile-to-existing-kind;
* uniform preview.

#### Phase 3 — Advanced desktop types

* audio;
* Strudel;
* automations;
* composite scenes;
* plugin-backed resources;
* multiple backends.

### Completion criteria

The runtime is complete when adding a new resource type mainly requires:

* defining the descriptor;
* an adapter;
* capabilities;
* a compiler or backend;
* a serializer;

and does not require separately rewriting Browser, Repeat Zone, preview,
persistence, Sequence Edit, and drag-and-drop.

---

# Dependencies and recommended architectural order

The twelve features have strong dependencies. The order must not be read as
a fully serial sequence, but as an order for stabilizing the concepts.

## Block A — Immediate experience on the current code

1. Looper in the Repeat Zone
2. Complete, live Sequence Edit
3. Performance Transformation Surface
4. Visualized Musical Causality

These features directly exploit already-present primitives and produce
immediate desktop value.

## Block B — Coherence of live operations

5. Live Musical Transaction Engine

The Transaction Engine must be introduced before Sequence Edit,
Transformation Surface, and scenes start producing multi-subsystem changes
that are too complex.

## Block C — Resource model

6. Musical Resource Graph
7. Universal Resource Runtime
8. Semantic Resource Browser

The Resource Graph describes the resources. The Universal Runtime executes
them. The Browser presents and finds them. The three initiatives must be
designed together, even if implemented incrementally.

## Block D — Intelligent composition

9. Lead Sheet and Chord Map
10. Seeded Variation Lab
11. Smart Form Builder
12. Nonlinear Arrangement Graph

Lead Sheet provides the visible harmonic model. Variation Lab produces
material. Smart Form Builder builds structures. Arrangement Graph makes it
possible to go beyond the linear form.

# Cross-cutting principles

All features must respect the following principles:

* the transport must not be stopped for normal creative operations;
* live changes must be quantizable;
* the result must be deterministic when the seed and input are the same;
* every generative proposal must be inspectable and editable;
* the UI must distinguish current, draft, pending, and preview;
* persistent resources must not be identified by runtime index;
* the realtime core must remain bounded and free of dynamic allocation;
* the desktop host can keep richer, more dynamic models;
* every new feature must produce observable events and state;
* every important behavior must be reproducible via tests and trace;
* initial simplicity must not eliminate progressive access to depth;
* animations must communicate state, time, and causality, not be
  standalone decoration.
