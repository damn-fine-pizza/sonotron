# components/core/audio_engine: the ISoundEngine contract (docs/proposals/
# isoundengine-contract.md, Corelli) -- a pure-virtual, POD-only-signature
# interface (dispatch/all_notes_off/render/name). Header-only/INTERFACE,
# core-capable NOW even though every concrete implementation today
# (components/platform/audio's SoundfontEngine) is host-only: no heap, no
# exceptions, no owned allocation in the contract itself. Depends only on
# components/core/common for arrangrr::MidiMessage -- the same unconditional
# dependency arrangrr/runtime/chorddet already carry, which is why this
# target lives in the top-level CMakeLists.txt's unconditional block
# alongside them rather than the host-only else() branch: nothing in the
# contract itself needs a hosted OS, and tests/arm-smoke/
# null_sound_engine_check.cpp compiles a trivial ISoundEngine implementation
# on the arm-none-eabi toolchain so "dual-target-ready" is a CI-verified
# fact, not an aspirational claim about a header nobody tried to compile
# freestanding.
add_library(audio_engine_core INTERFACE)
target_include_directories(audio_engine_core INTERFACE include)
target_link_libraries(audio_engine_core INTERFACE common)
