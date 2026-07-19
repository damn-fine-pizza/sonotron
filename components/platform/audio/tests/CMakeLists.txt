# SPSC ring primitive (Phase 2b, docs/design/sonotron-server-phase2-brief.md,
# promoted out of apps/gui-sonotron per docs/proposals/
# components-restructure-move-plan.md §2/§3 Step 5): header-only,
# dependency-free beyond STL -- links only the INTERFACE ring target +
# Threads for its threaded producer/consumer stress case.
add_executable(test_spsc_ring test_spsc_ring.cpp)
target_link_libraries(test_spsc_ring PRIVATE audio_ring Threads::Threads)
target_compile_options(test_spsc_ring PRIVATE -Wall -Wextra -Werror)
add_test(NAME test_spsc_ring COMMAND test_spsc_ring)
set_tests_properties(test_spsc_ring PROPERTIES LABELS unit)

# Device-independent AudioBackend/SoundfontEngine smoke test (Torquato QA
# pass, promoted alongside the ISoundEngine seam): links `audio` directly --
# construction/destruction, note_ring(), and SoundfontEngine::load()'s
# success/failure paths, none of which require a real audio device to open.
# See this file's own header comment for what is and is not reachable this
# way (the private data_callback()/render() path needs a live device).
add_executable(test_audio_engine_smoke test_audio_engine_smoke.cpp)
target_link_libraries(test_audio_engine_smoke PRIVATE audio)
target_compile_options(test_audio_engine_smoke PRIVATE -Wall -Wextra -Werror)
add_test(NAME test_audio_engine_smoke COMMAND test_audio_engine_smoke)
# NOT a hard SKIP_RETURN_CODE convention: only the LAST block (load()
# success path) needs a system SoundFont -- construction/ring/failure-path
# checks always run for real, so a missing SoundFont degrades this to a
# partial (still meaningful) pass rather than a full skip.
set_tests_properties(test_audio_engine_smoke PROPERTIES LABELS functional)

# Anti-NO-OP proof for ReverbSoundEngine (Phase-1 sound task #7, deliverable
# A): dispatch()/all_notes_off() must really forward to the inner engine,
# and render() must really change the signal (wet != dry), not silently
# regress to a passthrough. Same device-independent, SKIP-partial-if-no-
# system-SoundFont convention as test_audio_engine_smoke.cpp above.
add_executable(test_reverb_sound_engine test_reverb_sound_engine.cpp)
target_link_libraries(test_reverb_sound_engine PRIVATE audio)
target_compile_options(test_reverb_sound_engine PRIVATE -Wall -Wextra -Werror)
add_test(NAME test_reverb_sound_engine COMMAND test_reverb_sound_engine)
set_tests_properties(test_reverb_sound_engine PROPERTIES LABELS functional)
