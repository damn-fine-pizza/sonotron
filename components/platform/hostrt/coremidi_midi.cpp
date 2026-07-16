#include "coremidi_midi.hpp"

#include <utility>

namespace arrangrr::host {

CoreMidiMidi::~CoreMidiMidi() {
  for (const InPort& p : m_in_ports) {
    MIDIEndpointDispose(p.destination);
  }
  for (const OutPort& p : m_out_ports) {
    MIDIEndpointDispose(p.source);
  }
  if (m_client != 0) {
    MIDIClientDispose(m_client);
  }
}

bool CoreMidiMidi::open(const char* client_name, std::string& error) {
  CFStringRef name =
      CFStringCreateWithCString(kCFAllocatorDefault, client_name, kCFStringEncodingUTF8);
  const OSStatus status = MIDIClientCreate(name, nullptr, nullptr, &m_client);
  if (name != nullptr) {
    CFRelease(name);
  }
  if (status != noErr) {
    error = "cannot create CoreMIDI client";
    return false;
  }
  return true;
}

bool CoreMidiMidi::create_port(const PortDef& def, std::string& error) {
  CFStringRef name =
      CFStringCreateWithCString(kCFAllocatorDefault, def.name.c_str(), kCFStringEncodingUTF8);
  bool ok = false;

  if (def.is_input) {
    auto context =
        std::make_unique<DestContext>(DestContext{.self = this, .core_index = def.index});
    MIDIEndpointRef destination = 0;
    const OSStatus status = MIDIDestinationCreate(m_client, name, &CoreMidiMidi::midi_read_proc,
                                                  context.get(), &destination);
    if (status == noErr && destination != 0) {
      m_in_ports.push_back(InPort{.destination = destination, .context = std::move(context)});
      ok = true;
    }
  } else {
    MIDIEndpointRef source = 0;
    const OSStatus status = MIDISourceCreate(m_client, name, &source);
    if (status == noErr && source != 0) {
      m_out_ports.push_back(OutPort{.source = source, .core_index = def.index});
      ok = true;
    }
  }

  if (name != nullptr) {
    CFRelease(name);
  }
  if (!ok) {
    error = "cannot create CoreMIDI port '" + def.name + "'";
  }
  return ok;
}

void CoreMidiMidi::send(std::uint8_t core_port, const MidiMessage& msg) {
  MIDIEndpointRef source = 0;
  for (const OutPort& p : m_out_ports) {
    if (p.core_index == core_port) {
      source = p.source;
      break;
    }
  }
  if (source == 0) {
    return;
  }

  const int len = msg.wire_length();
  if (len <= 0) {
    return;
  }
  std::uint8_t bytes[3] = {msg.status, msg.d1, msg.d2};

  Byte buffer[64];
  MIDIPacketList* packet_list = reinterpret_cast<MIDIPacketList*>(buffer);
  MIDIPacket* packet = MIDIPacketListInit(packet_list);
  packet =
      MIDIPacketListAdd(packet_list, sizeof(buffer), packet, 0, static_cast<ByteCount>(len), bytes);
  if (packet == nullptr) {
    return;
  }
  MIDIReceived(source, packet_list);
}

void CoreMidiMidi::midi_read_proc(const MIDIPacketList* packet_list, void* read_proc_ref_con,
                                  void* src_conn_ref_con) {
  (void)src_conn_ref_con;
  const DestContext* context = reinterpret_cast<const DestContext*>(read_proc_ref_con);
  if (context == nullptr || context->self == nullptr) {
    return;
  }
  context->self->handle_packet_list(context->core_index, packet_list);
}

void CoreMidiMidi::handle_packet_list(std::uint8_t core_index, const MIDIPacketList* packet_list) {
  std::vector<QueuedInput> queued;
  const MIDIPacket* packet = &packet_list->packet[0];
  for (UInt32 i = 0; i < packet_list->numPackets; ++i) {
    std::size_t offset = 0;
    while (offset < packet->length) {
      const std::uint8_t status = packet->data[offset];
      if ((status & 0x80u) == 0) {
        break;  // not a status byte (running status/malformed) -- out of scope, stop here
      }
      const int extra = arrangrr::midi::data_length(status);
      if (extra < 0) {
        break;  // SysEx -- out of scope for every IMidiHal backend (message.hpp)
      }
      const std::size_t total = static_cast<std::size_t>(1 + extra);
      if (offset + total > packet->length) {
        break;  // truncated across a packet boundary -- ignore the remainder
      }
      QueuedInput ev{};
      ev.core_port = core_index;
      ev.len = static_cast<std::uint8_t>(total);
      ev.bytes[0] = status;
      if (extra >= 1) {
        ev.bytes[1] = packet->data[offset + 1];
      }
      if (extra >= 2) {
        ev.bytes[2] = packet->data[offset + 2];
      }
      queued.push_back(ev);
      offset += total;
    }
    packet = MIDIPacketNext(packet);
  }

  if (!queued.empty()) {
    std::lock_guard<std::mutex> lock(m_input_mutex);
    for (const QueuedInput& ev : queued) {
      m_input_queue.push_back(ev);
    }
  }
}

void CoreMidiMidi::drain_input(const InputSink& on_bytes) {
  std::vector<QueuedInput> drained;
  {
    std::lock_guard<std::mutex> lock(m_input_mutex);
    drained.swap(m_input_queue);
  }
  for (const QueuedInput& ev : drained) {
    on_bytes(ev.core_port, ev.bytes, ev.len);
  }
}

int CoreMidiMidi::poll_fd_count() const {
  // CoreMIDI's I/O runs on its own internal thread, never a pollable file
  // descriptor -- see this header's own doc comment.
  return 0;
}

int CoreMidiMidi::fill_poll_fds(MidiPollFd* fds, int space) const {
  (void)fds;
  (void)space;
  return 0;
}

}  // namespace arrangrr::host
