#pragma once

#include <cstdint>

namespace arrangrr {

// MIDI 1.0 status bytes (channel voice: high nibble; system: full byte).
namespace midi {
inline constexpr std::uint8_t kNoteOff = 0x80;
inline constexpr std::uint8_t kNoteOn = 0x90;
inline constexpr std::uint8_t kPolyPressure = 0xA0;
inline constexpr std::uint8_t kControlChange = 0xB0;
inline constexpr std::uint8_t kProgramChange = 0xC0;
inline constexpr std::uint8_t kChannelPressure = 0xD0;
inline constexpr std::uint8_t kPitchBend = 0xE0;

inline constexpr std::uint8_t kSysExStart = 0xF0;
inline constexpr std::uint8_t kMtcQuarterFrame = 0xF1;
inline constexpr std::uint8_t kSongPosition = 0xF2;
inline constexpr std::uint8_t kSongSelect = 0xF3;
inline constexpr std::uint8_t kTuneRequest = 0xF6;
inline constexpr std::uint8_t kSysExEnd = 0xF7;
inline constexpr std::uint8_t kClock = 0xF8;
inline constexpr std::uint8_t kStart = 0xFA;
inline constexpr std::uint8_t kContinue = 0xFB;
inline constexpr std::uint8_t kStop = 0xFC;
inline constexpr std::uint8_t kActiveSensing = 0xFE;
inline constexpr std::uint8_t kSystemReset = 0xFF;

// Channel-mode / notable controllers.
inline constexpr std::uint8_t kCcSustain = 64;
inline constexpr std::uint8_t kCcSostenuto = 66;
inline constexpr std::uint8_t kCcSoft = 67;
inline constexpr std::uint8_t kCcAllSoundOff = 120;
inline constexpr std::uint8_t kCcResetAllControllers = 121;
inline constexpr std::uint8_t kCcAllNotesOff = 123;

constexpr bool is_status(std::uint8_t b) noexcept { return (b & 0x80u) != 0; }
constexpr bool is_realtime(std::uint8_t b) noexcept { return b >= 0xF8u; }
constexpr bool is_system(std::uint8_t b) noexcept { return (b & 0xF0u) == 0xF0u; }
constexpr bool is_channel_voice(std::uint8_t b) noexcept { return is_status(b) && !is_system(b); }

// Number of data bytes for a status byte; -1 for variable (SysEx).
constexpr int data_length(std::uint8_t status) noexcept {
  if (is_realtime(status)) {
    return 0;
  }
  switch (status & 0xF0u) {
    case kNoteOff:
    case kNoteOn:
    case kPolyPressure:
    case kControlChange:
    case kPitchBend:
      return 2;
    case kProgramChange:
    case kChannelPressure:
      return 1;
    default:
      break;
  }
  switch (status) {
    case kSongPosition:
      return 2;
    case kMtcQuarterFrame:
    case kSongSelect:
      return 1;
    case kTuneRequest:
      return 0;
    case kSysExStart:
      return -1;
    default:
      return 0;
  }
}
}  // namespace midi

// One normalized wire message (channel voice, system common, or realtime).
// SysEx payloads are not represented here (M0 skips them safely; P2 feature).
struct MidiMessage {
  std::uint8_t status = 0;  // full status byte, channel included
  std::uint8_t d1 = 0;
  std::uint8_t d2 = 0;

  constexpr std::uint8_t type() const noexcept {
    return midi::is_system(status) ? status : static_cast<std::uint8_t>(status & 0xF0u);
  }
  constexpr std::uint8_t channel() const noexcept {
    return static_cast<std::uint8_t>(status & 0x0Fu);
  }
  constexpr int wire_length() const noexcept { return 1 + midi::data_length(status); }

  static constexpr MidiMessage note_on(std::uint8_t ch, std::uint8_t note,
                                       std::uint8_t vel) noexcept {
    return {.status=static_cast<std::uint8_t>(midi::kNoteOn | (ch & 0x0Fu)), .d1=note, .d2=vel};
  }
  static constexpr MidiMessage note_off(std::uint8_t ch, std::uint8_t note,
                                        std::uint8_t vel = 64) noexcept {
    return {.status=static_cast<std::uint8_t>(midi::kNoteOff | (ch & 0x0Fu)), .d1=note, .d2=vel};
  }
  static constexpr MidiMessage cc(std::uint8_t ch, std::uint8_t controller,
                                  std::uint8_t value) noexcept {
    return {.status=static_cast<std::uint8_t>(midi::kControlChange | (ch & 0x0Fu)), .d1=controller, .d2=value};
  }
  // Program Change: one data byte (GM program 0..127); d2 is unused.
  static constexpr MidiMessage program(std::uint8_t ch, std::uint8_t num) noexcept {
    return {.status=static_cast<std::uint8_t>(midi::kProgramChange | (ch & 0x0Fu)), .d1=num, .d2=0};
  }
  static constexpr MidiMessage realtime(std::uint8_t status) noexcept { return {.status=status, .d1=0, .d2=0}; }
};
static_assert(sizeof(MidiMessage) == 3);

}  // namespace arrangrr
