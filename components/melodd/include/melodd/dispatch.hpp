#pragma once

#include "common/midi/message.hpp"

namespace melodd {

class Synth;

// The ONE authoritative MidiMessage -> Synth decode (Phase-6 Theme 2 design
// review, docs/phase6-design-reviews.md "Audio in the standalone GUI",
// Decision 3/5): both apps/tools/melodd's ALSA-driven main.cpp and
// gui_sonotron_audio::AudioEngine's ring-driven realization call this one
// entry point instead of each maintaining its own hand-written switch over
// MIDI status bytes -- the same "shared label helpers, not two drifting
// tables" discipline components/hostrt/event_labels.hpp already applies
// elsewhere in this codebase.
//
// Ignores anything that is not a channel-voice message
// (arrangrr::midi::is_channel_voice) -- system/realtime bytes never reach a
// Synth. Not internally thread-safe (mirrors Synth's own header note): the
// caller must hold whatever lock/ring discipline already protects `synth`.
void dispatch_midi_message(Synth& synth, const arrangrr::MidiMessage& msg);

}  // namespace melodd
