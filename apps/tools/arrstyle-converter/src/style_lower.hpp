#pragma once

#include <string>

#include "midisrc/diagnostics.hpp"
#include "model.hpp"

// Style-lowering pass (Phase-5 Item #8, first slice): reads a canonical
// StyleModel (§4, model.hpp) and EMITS C++ SOURCE TEXT in the device's
// constexpr Style/StyleSection/StylePattern/StyleEvent format
// (components/core/arrangrr/include/arrangrr/arranger/style.hpp,
// arrangrr/arranger/styles/bossa.hpp is the reference shape). This is a
// text-generating codegen pass, NOT a linker of arrangrr: the tool never
// includes an arrangrr header and never constructs an arrangrr::Style object;
// it only knows the vocabulary/text well enough to print a source file that
// the REAL arrangrr headers will later compile against.
//
// This is a STANDALONE, privately-built artifact (ship-gate, see the task's
// licensing note): the generated header is never wired into
// arrangrr/arranger/style.hpp's kBuiltins list, any default build, or ci.sh.
// Callers own writing the generated text to disk and compiling it ad hoc.
//
// Only fields the StyleModel can prove MECHANICALLY are lowered; anything
// that would require inventing a musical decision is dropped with a
// diagnostic and reported, never guessed:
//
//  - SectionKind+SectionVariation -> SectionType. The device has exactly 13
//    fixed slots (2 intro, 4 var, 4 fill, 1 break, 2 ending). A
//    (kind, variation) that addresses none of them (e.g. a 3rd intro/ending
//    variant), or a second section that lands on an already-filled slot, is
//    DROPPED with a diagnostic — the tool never guesses which one "wins".
//  - Role -> TrackRole is a 1:1 vocabulary rename (model.hpp's Role mirrors
//    the runtime TrackRole on purpose, DESIGN.md §4). Role::kUnassigned has
//    no device destination and its lane is dropped with a diagnostic.
//  - TranspositionPolicy -> RolePolicy is a direct rename.
//  - A kFixed lane's PhraseEvent.note copies straight to StyleEvent.tone (a
//    literal MIDI note both sides never transpose).
//  - A kChordTone lane's PhraseEvent.note is reduced against the LANE's own
//    source_root_pc/source_quality into (chord-tone index, octave), using the
//    same reference-chord-shape idea the runtime NTT kernel resolves against
//    (arrangrr::arranger::resolve() / chorddet::theory::shape_of) and the
//    same per-role register anchor the arranger uses
//    (arrangrr::Arranger::kRoleAnchor, private to that class — mirrored here
//    as a local table; a comment at the mirror site names the source so the
//    two never drift silently). The reduction is EXACT: a note must land
//    precisely on one of the reference chord's root/3rd/5th/(7th) pitch
//    classes and octave-fold onto an integer register; anything else (a
//    passing tone, a 9th/13th, an undecoded source quality) cannot be
//    mechanically reduced and is dropped with a diagnostic — this is the
//    "chord-tone reduction beyond the 4 positions" gap flagged for this slice.
//  - PhraseEvent.tick/gate_ticks are rescaled from source_ppqn onto the
//    device's fixed 960-PPQN / 16th-grid step. An event whose tick does not
//    fall exactly on a device step (a swung/micro-timed source) is snapped to
//    the nearest step but COUNTED and reported — never silently rounded away.
//  - PhraseLane.note_low/note_high (CASM register clamp) and .retrigger have
//    no destination field in the current device Style format (retrigger is
//    explicitly a FUTURE NTT input, model.hpp). Both are dropped with a
//    single informational diagnostic (not per-lane spam).
//  - StylePattern.gm_program/.voicing and Style.groove have NO source data in
//    StyleModel at all (the importer never captures Program Change / groove
//    events). They are left at their historical neutral defaults (-1 /
//    kAsWritten / {}) and reported once: guessing a GM voice or a groove feel
//    would be a musical judgment this pass must not make.

namespace arrstyle {

struct StyleLowerOptions {
  // Must already be a valid, lowercase C++ identifier (used verbatim as both
  // the generated namespace and the Style::name string). The caller validates
  // this before calling in — lower_style() does not sanitize it, so a bad
  // name fails loudly (a compile error in the generated file) rather than
  // silently mutating into something else.
  std::string style_name;
};

// Lowers `model` onto the device Style text form. Returns false only when
// NOTHING could be lowered (no section addressed a device slot) — the
// caller should treat that as a failed compile, not a partial one. On
// success `out_text` holds a complete, self-contained #pragma-once header.
bool lower_style(const StyleModel& model, const StyleLowerOptions& opts, std::string& out_text,
                 Diagnostics& diag);

}  // namespace arrstyle
