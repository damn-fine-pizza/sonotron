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
