#pragma once

#include "arrangrr/abi.hpp"
#include "brain_event.hpp"

// The in-process OutEvent -> BrainEvent decode (Phase 2b, docs/design/
// sonotron-server-phase2-brief.md "Data on the ring", Corelli §15.3/§15.4
// correction #4). This is the direction the ring replaces JSONL text with a
// raw POD: the GUI-thread decode of an OutEvent drained off the engine->GUI
// ring, populating the SAME sonotron::BrainEvent shape parse_brain_event()
// (brain_event.cpp) produces from the socket's JSONL text.
//
// Both this function and components/hostrt/jsonl.cpp's to_jsonl()/to_human()
// call the SAME shared label helpers (components/hostrt/event_labels.hpp) --
// one enum/code -> label computation, two thin front ends (a text serializer
// and this struct populator). That is what keeps the ring's in-process decode
// and the socket's JSONL wire from drifting into two independently
// hand-maintained label tables (the risk Corelli's review flagged).
//
// D38 is retired for Phase 2 (owner-decided, sonotron-server-phase2-brief.md
// "Corelli §15 review -- resolution", item 1): this file, unlike the rest of
// apps/gui-sonotron/src, DOES include core headers and link hostrt/arrangrr --
// it is the GUI binary's engine-linking half, not the pure-client half.
// `brain_event.hpp` (the OTHER half of this signature) stays exactly as
// core-free as it always was.

namespace sonotron {

// `prefer_flats` mirrors the second parameter to_jsonl()/to_human() already
// take (derived from arrangrr::host::Shell::prefer_flats(), itself driven by
// the current key) -- it is host-side rendering state, not part of the ABI,
// so it cannot be recovered from the OutEvent alone and must be threaded in
// by the caller (see docs/design/orchestrator-pipeline-extraction.md §15.3).
BrainEvent brain_event_from_outevent(const arrangrr::OutEvent& ev, bool prefer_flats);

}  // namespace sonotron
