# Golden tests: deterministic .acmd scripts through the CLI's virtual clock,
# canonical JSONL output compared byte-for-byte with the .golden file.

function(arrangrr_golden name)
  add_test(NAME golden_${name}
    COMMAND ${CMAKE_COMMAND}
      -DCLI=$<TARGET_FILE:arrangrr_cli>
      -DSCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/${name}.acmd
      -DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/${name}.golden
      -DOUT=${CMAKE_CURRENT_BINARY_DIR}/${name}.out
      -P ${CMAKE_CURRENT_SOURCE_DIR}/run_golden.cmake)
endfunction()

arrangrr_golden(routing_panic)
arrangrr_golden(clock_master)
arrangrr_golden(running_status)
arrangrr_golden(polymeter)
arrangrr_golden(mute_solo)
arrangrr_golden(hello_chord)
arrangrr_golden(smart_richness)
arrangrr_golden(minor_v)
arrangrr_golden(seq_progression)
arrangrr_golden(seq_record)
