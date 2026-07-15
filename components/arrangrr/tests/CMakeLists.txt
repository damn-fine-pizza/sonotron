# Three-metric coverage labels (see scripts/coverage.sh and project memory
# `coverage-metrics`). Every test carries exactly ONE CTest label, chosen by the
# SUBJECT-UNDER-TEST (not by whether a driver/harness is used):
#   unit       — the subject is a SINGLE production module. Direct class
#                instances or pure functions qualify; so does a producer that
#                can only be observed by running it under an Engine HARNESS
#                (e.g. Arp, Arranger) — there the Engine is scaffolding, the
#                module under test is still one unit.
#   functional — the subject is the INTEGRATION itself: the Engine's own
#                cross-producer coordination (dispatch, fire order), the
#                chord-follow selector arbitrating three producers, or the
#                PanelManager/Shell navigation loop.
#   regression — written to pin a specific FIXED bug (red-before / green-after).
# A file may cover lines both ways; the label reflects its PRIMARY intent (CTest
# labels are per-binary, not per-CHECK). test_arp / test_arranger are the pivot
# calls (unit here) and are discussed in the QA report so the owner can flip
# them — doing so drops metric-1 branch coverage below the 80% gate.
function(arrangrr_test name category)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE arrangrr)
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS ${category})
endfunction()

arrangrr_test(test_common unit)
arrangrr_test(test_midi unit)
# test_abi_frozen: the frozen-ABI guard (node 11720). Pins every command/event
# id and the exact wire-struct sizes via static_assert so a break in the
# additive-only v1 freeze fails the BUILD. Subject is the ABI surface itself -> unit.
arrangrr_test(test_abi_frozen unit)
arrangrr_test(test_engine functional)
arrangrr_test(test_timeline unit)
# test_step_locks pins the fixed sustain-chain lone-tie bug (a lone tie was
# extended by exactly one step): red-before / green-after — hence `regression`.
# It also carries Timeline parameter-lock coverage; regression is the primary
# intent per the three-metric plan.
arrangrr_test(test_step_locks regression)
arrangrr_test(test_chord unit)
arrangrr_test(test_chord_seq unit)
arrangrr_test(test_chord_detect unit)
arrangrr_test(test_chord_follow functional)
# test_chord_followed_event: the additive kChordFollowed OutEvent (node 0700).
# Drives all four emit sites (manual/sequencer/detect/bar-promote) through the
# ABI and asserts the decoded cur/next chord, the honest producer src, and the
# delta policy (one event per genuine change, none on unchanged ticks) -> functional.
arrangrr_test(test_chord_followed_event functional)
# test_followed_context: the FollowedContext owner (explicit latch, D47 gate,
# stage/commit_now/commit_bar, establish_default no-op-if-explicit) plus the
# release-phantom regression and the immediate-default/shift-quantize input
# model. Supersedes the old test_chord_quantize (which pinned the removed D53
# quantized-default primitives). Torquato re-ratifies any further D53->immediate
# expectations here.
arrangrr_test(test_followed_context unit)
# test_chord_context_diagnosis: Torquato QA diagnosis of two user-reported
# chord-context bugs (idle-playback drift with zero input; a section/style
# switch resetting a steered chord). Subject is the Engine's own cross-tick
# and cross-command coordination of the followed chord context -> functional.
# Currently RED where it pins an open, unfixed defect (see the printed
# timeline and the CHECK comments in the file); do not silence it.
arrangrr_test(test_chord_context_diagnosis functional)
# test_chord_seq_vs_live_steer: the LIVE-PRIORITY arbitration (engine
# default) between fire_chord_seq and a live steer through the FollowedContext
# owner. Locks: a held live chord beats a running sequencer (band follows live,
# the seq comps ON the live chord), release resumes the sequencer, no-seq
# latches (Strada 1), the `chord follow sequencer` override, and kAuto's kept
# legacy last-writer clobber. Subject is the Engine's cross-tick arbitration ->
# functional.
arrangrr_test(test_chord_seq_vs_live_steer functional)
# test_input_zone: the Engine's per-port InputZone route-vs-suppress split and
# the detector's scale-aware single-finger steering, driven through the ABI ->
# the subject is the integration, so `functional`.
arrangrr_test(test_input_zone functional)
arrangrr_test(test_harmony_steer functional)
# test_arp / test_arranger: subject is ONE producer (Arp / Arranger); the Engine
# is only the harness that lets it emit over ticks -> unit (see header note).
arrangrr_test(test_arp unit)
# test_arp_functional drives the FULL Engine (push_command/push_midi_in/advance)
# for no-stuck-note invariants across transport stop and retrigger -> the subject
# is the arp's integration with transport/scheduler, so `functional`.
arrangrr_test(test_arp_functional functional)
arrangrr_test(test_arranger unit)
arrangrr_test(test_gesture unit)
arrangrr_test(test_voicing unit)
# test_restyle: the RestyleStage (roadmap 9320) -- the inverse NTT classifier
# (restyle::classify, the one genuinely new piece), the rhythmic snap
# (restyle::snap_to_grid), the register anchor, and the stage's own inert-by-
# default / chord-tone-anchoring / harmony-preservation behavior. Subject is
# ONE new stage -> unit (same precedent as test_arranger/test_voicing).
arrangrr_test(test_restyle unit)
# test_motif: the generative motif engine (roadmap 9210) -- motif::generate,
# motif::apply_transform, motif::apply_repeat (call-and-response), plus their
# wiring into Arranger::on_tick's gather phase. Subject is ONE new producer ->
# unit (same precedent as test_restyle/test_gesture/test_voicing).
arrangrr_test(test_motif unit)
# test_clip: the clip launch primitive (Phase-5 Item #2, docs/design/
# clip-primitive-design.md). ClipMatrix is pure POD bookkeeping, but the
# musically-observable behavior only exists through Engine's cmd_clip/
# apply_clip_content/fire_clips cross-producer wiring (Arranger/
# ChordSequencer/Timeline) -> functional, same precedent as test_input_zone.
arrangrr_test(test_clip functional)
# Phase-5 Item #9 (docs/phase5-design-reviews.md "Pad/Scene live ->
# Performance"). test_boundary_latch: the ONE shared arm-at-boundary
# primitive (Corelli fix #2) is a pure freestanding POD -> unit, same
# precedent as test_common.cpp's own common/ primitive coverage.
arrangrr_test(test_boundary_latch unit)
# test_pad_bank: PadEngine's own pure bookkeeping (assign/get/runtime), no
# dispatch logic -> unit, same precedent as test_common.cpp/ClipMatrix's own
# scope tripwire (dispatch lives in Engine, covered separately below).
arrangrr_test(test_pad_bank unit)
# test_pad: the pad-bank WRAPPER dispatch (fire_pad routing each PadType to
# the verb it wraps, the 4 PadModes, and quantize/BoundaryLatch reuse) only
# exists through Engine's cmd_pad/fire_pad cross-producer wiring -> functional,
# same precedent as test_clip.
arrangrr_test(test_pad functional)
# test_performance: Performance capture/recall (Engine's cmd_perf/
# capture_performance/validate_performance/apply_performance), including the
# Corelli-flagged recall atomicity and bar-gate-ordering properties -- only
# observable through the Engine's cross-producer wiring -> functional.
arrangrr_test(test_performance functional)
# test_performance_wire: the on-disk serialize()/deserialize() byte format +
# PerformanceStore's own pool bookkeeping -- pure, no Engine -> unit, same
# precedent as test_common.cpp.
arrangrr_test(test_performance_wire unit)
# test_performance_validate: QA gate-restoration follow-up (owner-approved
# "extract pure logic + unit-test it") -- arrangrr::perf::validate is now a
# PURE free function (performance.hpp/performance.cpp), exhaustively branch-
# covered here WITHOUT an Engine instance -> unit, and now counts toward the
# enforced metric-1 gate (previously only reachable through
# test_performance.cpp's functional atomicity coverage).
arrangrr_test(test_performance_validate unit)
# test_performance_style_id_regression: Torquato QA pin for an open defect in
# Engine::apply_performance (a recalled CONCRETE style_id degrades to 0xFFFF
# "unknown", poisoning the metadata AND any later capture) -- written to pin
# a not-yet-fixed bug, red-before/green-after once Nazzareno lands the fix.
# Currently RED; do not silence it. Kept in its OWN file/CTest entry (mirrors
# test_chord_context_diagnosis's own precedent) so its failure never masks
# the surrounding GREEN test_performance.cpp coverage.
arrangrr_test(test_performance_style_id_regression regression)
# Phase-5 Item #10 (docs/phase5-design-reviews.md "MIDI-FX insert chain",
# node 5100/5200). test_insert_chain: Insert::process's four stateless
# per-type functions (ScaleLock/VelocityProc/Echo/NoteRepeat) and
# InsertChain::apply's own single-input-note bookkeeping -- pure,
# freestanding, no Engine -> unit, same precedent as test_pad_bank.
arrangrr_test(test_insert_chain unit)
# test_insert_chain_fan_overflow_regression: Torquato QA lock for
# InsertChain::apply's overflow guard -- reached whenever a chain stage's
# COMBINED fan-out across MULTIPLE pending input notes (an earlier stage's
# own fan-out) exceeds kMaxChainFan. Originally pinned an open crash defect
# (the guard's assert condition was unconditionally false, trapping via
# ARR_ASSERT/__builtin_trap() instead of truncating); Nazzareno fixed the
# guard to break-and-truncate. Now a normal CHECK-based green lock proving
# the bounded/no-OOB/no-crash contract. Kept in its own minimal target
# (mirrors test_performance_style_id_regression's isolation precedent) so a
# future re-break of this specific guard is caught in isolation.
arrangrr_test(test_insert_chain_fan_overflow_regression regression)
# test_fx: cmd_fx's (kFxSet/kFxParam/kFxEnable/kFxClear) dispatch/validation
# and the Corelli must-fix (a chain-produced fan-out note's groove/schedule
# recomputed at ITS OWN grid position) only exist through Engine's
# Arranger::on_tick D40-pipeline cross-producer wiring -> functional, same
# precedent as test_pad/test_clip.
arrangrr_test(test_fx functional)
# Phase 6 Theme 1b (Torquato QA, coverage-gate restoration). test_clip_matrix:
# ClipMatrix's own pure bookkeeping (add/get/arm/force/on_bar promotion) --
# no dispatch logic lives here (Engine::apply_clip_content/fire_clips, covered
# by test_clip.cpp, own that) -> unit, same precedent as test_pad_bank.
arrangrr_test(test_clip_matrix unit)
# test_function_ref: FunctionRef's own call/rebind/reference-semantics
# contract -- pure, freestanding, no Engine -> unit, same precedent as
# test_common.cpp's own common/ primitive coverage.
arrangrr_test(test_function_ref unit)
# Phase-6 Theme 3 Item #1 (docs/reflections/phase6-theme3-master-transpose-
# scope.md). test_master_transpose: the kMasterTranspose ABI verb's
# dispatch/validation and its propagation to BOTH note-emitting paths
# (Arranger's band + ChordEngine's pressed/pad chord) plus the untouched
# detected-chord root and Timeline step-track literal content -- only
# observable through Engine's cross-producer wiring -> functional, same
# precedent as test_fx/test_clip. resolve()'s own single-producer unit
# coverage lives in test_arranger.cpp (test_master_transpose_resolve).
arrangrr_test(test_master_transpose functional)
# test_master_transpose_voicing_regression: Torquato QA pin for an open
# defect where VoicingState's kLead re-octaving (voicing.hpp) silently folds
# a LIVE master_transpose change by a whole octave whenever the delta from
# the previous voicing exceeds a tritone -- Arranger::set_master_transpose
# never resets/invalidates the per-role voicing memory the way
# load_style()/on_transport_start() do. Kept in its own file/CTest entry
# (mirrors test_performance_style_id_regression's own isolation precedent)
# so its failure never masks the surrounding GREEN test_master_transpose.cpp/
# test_arranger.cpp/test_voicing.cpp coverage. Currently RED; do not silence
# it.
arrangrr_test(test_master_transpose_voicing_regression regression)
# test_arp_insert_style_switch_stale_regression: Torquato QA heavy-pass pin
# (Phase-6 Theme 4, target 6) -- Arranger::request_style() (the LIVE
# style-switch path) never calls reset_role_arps(), unlike load_style();
# a role's arp-insert keeps arpeggiating a chord resolved under the OLD
# style after a live switch. Kept in its own file (mirrors
# test_master_transpose_voicing_regression.cpp's isolation precedent) so its
# failure never masks test_arranger.cpp's surrounding GREEN coverage.
# Currently RED; do not silence it.
arrangrr_test(test_arp_insert_style_switch_stale_regression regression)
# test_dual_arp_collision: Torquato QA heavy-pass pin (Phase-6 Theme 4, Fork 6)
# -- see the file's own header comment for the exact collision scenario. RED:
# a role's kArp insert and the live-keyboard arp sharing one output port/
# channel/pitch corrupt each other's note-off via the shared scheduler's
# single-producer retrigger-care logic. GREEN (kept in the same file/target,
# same collision setup): Panic still drops both arps' held state cleanly.
arrangrr_test(test_dual_arp_collision regression)
# test_clip_matrix_live_meter_change_regression: Torquato QA pin (Phase 7,
# node T0 hardening) -- ClipMatrix::on_bar recomputes its quantize window
# FRESH from whatever live ticks_per_bar Engine threads in on each call,
# unlike the sibling BoundaryLatch primitive (Corelli fix #2), which
# snapshots its window ONCE at arm() time (see test_boundary_latch.cpp's own
# frozen-window test for the contrast). A clip armed for "the next N bars"
# while the meter is stable silently retunes to a DIFFERENT fire tick the
# moment the live time signature changes mid-count-down. See the file's own
# header comment for the exact minimized reproducer. Kept in its own file/
# CTest entry (mirrors test_performance_style_id_regression's own isolation
# precedent) so its failure never masks test_clip_matrix.cpp's surrounding
# GREEN coverage. Currently RED; do not silence it.
arrangrr_test(test_clip_matrix_live_meter_change_regression regression)
# Phase 7 (node 6000, the Looper -- docs/reflections/phase7-scope-6000-8100-
# clip-timeline-seam.md, SLICE 1). test_loop_event: LoopEvent's decompose_
# note()/resolve_note() chord-relative round-trip (Fork A) and LoopClip's own
# record/quantize/length-mode/remove surface -- pure, no Engine -> unit, same
# precedent as test_chord_seq.cpp's ChordSequence-only coverage.
arrangrr_test(test_loop_event unit)
# test_loop: the musically-observable record/overdub/erase/undo/launch
# behavior only exists through Engine's cmd_loop/apply_clip_content/
# fire_loop cross-producer wiring -> functional, same precedent as test_clip.
arrangrr_test(test_loop functional)
# test_loop_buffer_sounding_overflow_regression: Torquato QA regression guard
# (Phase 7, node 6000 SLICE 1 hardening) -- LoopBuffer::on_tick's PLAYBACK-side
# sounding-note table (kMaxLoopHeldNotes, loop_buffer.hpp) is bounded at 16
# concurrently-sounding voices per slot; Nazzareno's fix gates the note-ON
# itself on store_sounding() claiming a tracking slot, so an excess voice
# past that bound is silently CAPPED (never sounds) instead of sounding and
# then losing its release path. Originally pinned the pre-fix orphaned-note
# defect (RED); now GREEN, asserting the locked contract (bounded polyphony,
# zero orphans -- every voice that sounds gets released). Subject is
# LoopBuffer alone, no Engine needed -> unit-scoped, kept `regression` per
# this file's own isolation precedent (mirrors test_performance_style_id_
# regression.cpp) so a future re-break of this specific guard is caught in
# isolation, without masking the surrounding GREEN test_loop/test_loop_event
# coverage.
arrangrr_test(test_loop_buffer_sounding_overflow_regression regression)
# test_loop_reharmonize: Torquato QA hardening pass -- THE key property of
# node 6000's whole storage shape (Fork A, loop_event.hpp) proven end to end
# through Engine's real ABI (record -> stop -> context change -> launch),
# covering all three LoopNoteSource branches. test_loop.cpp's own round-trip
# test never changes the followed chord/key between record and launch, so
# this is genuinely new coverage, not a duplicate -> functional (the subject
# is the cross-tick/cross-command Engine integration, same precedent as
# test_clip/test_fx).
arrangrr_test(test_loop_reharmonize functional)
# test_loop_wrap_and_idempotency_regression: Torquato QA guards for the two
# ALREADY-FIXED bugs loop_buffer.hpp's own header comments document in
# detail (note-off-at-wrap, tick-0 idempotency across the synchronous-
# promote + ordinary-fire_loop dual call site) -- GREEN today, kept in its
# own file/CTest entry so a future re-break of either specific guard is
# caught in isolation, same precedent as test_dual_arp_collision.cpp.
arrangrr_test(test_loop_wrap_and_idempotency_regression regression)
# test_loop_ops: Torquato QA hardening pass -- closes the remaining mandate
# gaps test_loop.cpp/test_loop_event.cpp leave open: overdub's own wrap
# alignment across a cycle boundary, undo's shared shadow moving ACROSS
# different slots, two loop slots playing simultaneously/independently, live-
# meter-respecting playback (T0), and per-slot event-pool capacity
# (kMaxLoopEvents) degrading gracefully -> functional, same precedent as
# test_clip/test_fx (the subject is Engine's own cross-producer wiring).
arrangrr_test(test_loop_ops functional)
# test_scene (Phase 7, node 8100, Scenes/song mode -- docs/reflections/
# phase7-scope-6000-8100-clip-timeline-seam.md, Fork B RESOLVED = own
# transport): the MINIMAL happy-path pass -- a 2-scene chain advancing at the
# right bar and applying each transition's Performance, a non-4/4 scene step
# actually moving Transport::ticks_per_bar() AND the re-anchored next-bar-
# boundary regression guard, plus inert/default-path byte-identity checks.
# Subject is Engine's own cross-producer wiring (cmd_scene/fire_scene/
# apply_scene_transition) -> functional, same precedent as test_clip/
# test_loop. Thorough functional/regression QA is Torquato's follow-up.
arrangrr_test(test_scene functional)
