# components/chorddet: the dual-target, freestanding, no-heap chord-detect
# peer (ChordDetector + FollowedContext + the theory vocabulary they need,
# moved byte-for-byte out of components/arrangrr, plus the NEW ChorddetStage
# Seam-C adapter -- docs/design/orchestrator-pipeline-extraction.md
# §16.1/§16.2c/§16.4/§16.8, Phase 4b). Depends ONLY on components/common and
# components/runtime -- NEVER on components/arrangrr (D43): chorddet must
# stay name-blind of arrangrr so a future firmware [chorddet]->[arrangrr]
# two-stage deployment, or a standalone chorddet peer, never pulls in
# arrangrr's own routing/timeline/arranger machinery. arrangrr depends on
# chorddet instead, one direction only.
#
# Header-only/INTERFACE, same shape as components/runtime: every type here is
# a template or a constexpr class, no forcing reason for a ceremony .cpp.
add_library(chorddet INTERFACE)
target_include_directories(chorddet INTERFACE include)
target_link_libraries(chorddet INTERFACE common runtime)
target_compile_options(chorddet INTERFACE
  -fno-exceptions
  -fno-rtti
  -fno-threadsafe-statics
)
