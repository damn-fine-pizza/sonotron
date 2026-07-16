#include "melodd/dispatch.hpp"

#include "melodd/synth.hpp"

namespace melodd {

void dispatch_midi_message(Synth& synth, const arrangrr::MidiMessage& msg) {
  if (!arrangrr::midi::is_channel_voice(msg.status)) {
    return;
  }
  const int channel = msg.channel();
  switch (msg.type()) {
    case arrangrr::midi::kNoteOn:
      synth.note_on(channel, msg.d1, msg.d2);
      break;
    case arrangrr::midi::kNoteOff:
      synth.note_off(channel, msg.d1);
      break;
    case arrangrr::midi::kProgramChange:
      synth.program_change(channel, msg.d1);
      break;
    case arrangrr::midi::kPitchBend: {
      const int value14 = (static_cast<int>(msg.d2) << 7) | msg.d1;
      synth.pitch_bend(channel, value14);
      break;
    }
    case arrangrr::midi::kControlChange:
      // Not required by the first-slice spec, but TinySoundFont routes it
      // for free (sustain, bank-select, all-notes-off...).
      synth.control_change(channel, msg.d1, msg.d2);
      break;
    default:
      break;
  }
}

}  // namespace melodd
