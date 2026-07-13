# Golden tests: deterministic .acmd scripts through the headless server's
# virtual clock, canonical JSONL output compared byte-for-byte with the
# .golden file. Phase 2a re-points this at `sonotron_server` (the thin
# headless binary, docs/design/orchestrator-pipeline-extraction.md §15): same
# Shell/Runtime/arrangrr code as `cli_arrangrr`'s own --script, different
# binary -- byte output is unchanged.

function(arrangrr_golden name)
  add_test(NAME golden_${name}
    COMMAND ${CMAKE_COMMAND}
      -DCLI=$<TARGET_FILE:sonotron_server>
      -DSCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/${name}.acmd
      -DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/${name}.golden
      -DOUT=${CMAKE_CURRENT_BINARY_DIR}/${name}.out
      -P ${CMAKE_CURRENT_SOURCE_DIR}/run_golden.cmake)
  # Golden scripts drive the CLI binary end-to-end (clock -> engine -> JSONL):
  # cross-module behaviour, hence `functional` in the three-metric split.
  set_tests_properties(golden_${name} PROPERTIES LABELS functional)
endfunction()

arrangrr_golden(routing_panic)
arrangrr_golden(clock_master)
arrangrr_golden(running_status)
arrangrr_golden(polymeter)
arrangrr_golden(mute_solo)
arrangrr_golden(hello_chord)
arrangrr_golden(chord_followed)
arrangrr_golden(smart_richness)
arrangrr_golden(minor_v)
arrangrr_golden(seq_progression)
arrangrr_golden(seq_record)
arrangrr_golden(chord_modes)
arrangrr_golden(arranger_band)
arrangrr_golden(arranger_gesture)
arrangrr_golden(arranger_voicing)
arrangrr_golden(feel_swing)
arrangrr_golden(feel_shuffle)
arrangrr_golden(feel_blues)

# Accompany (roadmap 9310), docs/design/orchestrator-pipeline-extraction.md
# §16.7, phase4-execution-plan.md 4d: the NEW golden category proving the
# 3-stage `[MIDI-source, chorddet, arrangrr]` pipeline end-to-end -- a
# `midi-source load <path>` L1 line feeds a plain SMF melody thru, merged in
# D29 total order with the band the SAME script drives via the existing
# chord/style verbs. Same harness (run_golden.cmake), same CLI binary
# (sonotron_server) as every other golden -- WORKDIR pins the process CWD to
# the repo root so the script's relative fixture path is independent of
# wherever ctest itself was invoked from. Reuses the ALREADY-committed shared
# fixture `components/midisrc/tests/fixtures/tiny.mid` (also used by
# midisrc's and arrstyle-converter's own tests) rather than adding a new
# binary asset under tests/golden/ (Corelli's §16.7 "new asset class"
# placement flag, sidestepped by pointing at an existing one).
function(accompany_golden name)
  add_test(NAME golden_${name}
    COMMAND ${CMAKE_COMMAND}
      -DCLI=$<TARGET_FILE:sonotron_server>
      -DSCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/${name}.acmd
      -DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/${name}.golden
      -DOUT=${CMAKE_CURRENT_BINARY_DIR}/${name}.out
      -DWORKDIR=${CMAKE_SOURCE_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/run_golden.cmake)
  set_tests_properties(golden_${name} PROPERTIES LABELS functional)
endfunction()

accompany_golden(accompany_basic)

# Phase 4e (docs/design/orchestrator-pipeline-extraction.md §3.6's "emits raw
# MIDI thru events AND feeds notes into" chorddet, runtime/pipeline.hpp's
# forward-flow): the node-9310 feature proof itself -- the band under chords
# DETECTED FROM the imported melody, no scripted `chord`/`seq` steer anywhere
# in the script (see accompany_melody_detect.acmd's own header comment).
accompany_golden(accompany_melody_detect)

# Restyle (roadmap 9320, Phase 5 Item A): the node-9320 feature proof --
# `restyle <style>` transforms the imported melody's OWN notes into the
# target style's idiom (rhythm snap + groove, register-anchored chord tones)
# instead of Accompany's verbatim melody-thru (see accompany_restyle.acmd's
# own header comment for the exact transform this pins).
accompany_golden(accompany_restyle)
